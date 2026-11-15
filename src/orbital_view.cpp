#include "snake/orbital_view.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace snake {
namespace orbital_view {

namespace {

using orbital_frame::Cell;
using orbital_frame::CellRole;
using orbital_frame::Frame;
using orbital_frame::HandoverHint;
using orbital_frame::LaneSlot;
using orbital_frame::OverviewRow;
using orbital_frame::PlayerPanel;
using orbital_frame::UplinkTimeline;

// Gruvbox dark palette
namespace gruvbox {
ftxui::Color bg2() { return ftxui::Color::RGB(0x50, 0x49, 0x45); }
ftxui::Color gray() { return ftxui::Color::RGB(0x92, 0x83, 0x74); }
ftxui::Color fg4() { return ftxui::Color::RGB(0xa8, 0x99, 0x84); }
ftxui::Color fg() { return ftxui::Color::RGB(0xeb, 0xdb, 0xb2); }
ftxui::Color green() { return ftxui::Color::RGB(0xb8, 0xbb, 0x26); }
ftxui::Color yellow() { return ftxui::Color::RGB(0xfa, 0xbd, 0x2f); }
ftxui::Color purple() { return ftxui::Color::RGB(0xd3, 0x86, 0x9b); }
ftxui::Color blue() { return ftxui::Color::RGB(0x83, 0xa5, 0x98); }
ftxui::Color fadedPurple() { return ftxui::Color::RGB(0x8f, 0x3f, 0x71); }
ftxui::Color fadedYellow() { return ftxui::Color::RGB(0xb5, 0x76, 0x14); }
}  // namespace gruvbox

ftxui::Color borderColor() { return gruvbox::blue(); }
ftxui::Color textColor() { return gruvbox::fg(); }
ftxui::Color labelColor() { return gruvbox::gray(); }
ftxui::Color foodColor() { return gruvbox::green(); }
ftxui::Color uplinkColor() { return gruvbox::fg4(); }
ftxui::Color elapsedColor() { return gruvbox::bg2(); }

ftxui::Color playerColor(const PlayerId& player) { return player == PLAYER_A ? gruvbox::purple() : gruvbox::yellow(); }

// Faded variant of the player colour, for marks that should recede; a blend towards the background
// would turn grey in a 256 colour terminal
ftxui::Color dimmed(const PlayerId& player) {
  return player == PLAYER_A ? gruvbox::fadedPurple() : gruvbox::fadedYellow();
}

// Symbol of a board cell; a cell is two terminal columns wide, so that a square board looks square
std::string cellSymbol(CellRole role) {
  switch (role) {
    case CellRole::EMPTY:
      return "  ";
    case CellRole::SELECTED_ROUTE:
    case CellRole::SENT_ROUTE:
      return "· ";
    case CellRole::ACTIVE_ROUTE:
      return "• ";
    case CellRole::FOOD:
      return "◆ ";
    case CellRole::TARGET:
      return "◎ ";
    case CellRole::SNAKE_BODY:
      return "● ";
    case CellRole::SNAKE_HEAD:
      return "⬤ ";
    case CellRole::DEAD_HEAD:
      return "✖ ";
    case CellRole::CURSOR:
      return "✚ ";
  }
  return "  ";
}

ftxui::Element cellElement(const Cell& cell) {
  ftxui::Element symbol = ftxui::text(cellSymbol(cell.role));
  if (cell.role == CellRole::FOOD) {
    return symbol | ftxui::color(foodColor());
  }
  if (!cell.player) {
    return symbol;
  }
  if (cell.role == CellRole::SELECTED_ROUTE) {
    return symbol | ftxui::color(dimmed(*cell.player));
  }
  if (cell.role == CellRole::SNAKE_HEAD || cell.role == CellRole::CURSOR) {
    return symbol | ftxui::color(playerColor(*cell.player)) | ftxui::bold;
  }
  return symbol | ftxui::color(playerColor(*cell.player));
}

// Seconds with one decimal and no unit, e.g. "0.6"
std::string decimalSeconds(std::chrono::milliseconds duration) {
  std::ostringstream text;
  text.setf(std::ios::fixed);
  text.precision(1);
  text << static_cast<double>(duration.count()) / 1000.0;
  return text.str();
}

// Seconds with one decimal, e.g. "0.6s"
std::string seconds(std::chrono::milliseconds duration) { return decimalSeconds(duration) + "s"; }

// Minutes and seconds, e.g. "1:45"
std::string clock(std::chrono::milliseconds duration) {
  const auto total = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
  std::ostringstream text;
  text << total / 60 << ":" << std::setw(2) << std::setfill('0') << total % 60;
  return text.str();
}

// One column of space between a box's border and its content
ftxui::Element padded(ftxui::Element content) {
  return ftxui::hbox({ftxui::text(" "), std::move(content), ftxui::text(" ")});
}

// Rounded box whose title starts after one line character of the top border, e.g. "╭─ Title ───╮"
// The border is drawn in a muted colour; title and content are drawn over it in the text colour,
// where their own colours, such as the snakes', take precedence
ftxui::Element box(ftxui::Element title, ftxui::Element content) {
  ftxui::Element framed_title =
      ftxui::hbox({ftxui::text("─ "), std::move(title) | ftxui::color(textColor()), ftxui::text(" ")});
  return ftxui::window(std::move(framed_title), padded(std::move(content) | ftxui::color(textColor()))) |
         ftxui::color(borderColor());
}

std::string targetText(Point target) { return "(" + std::to_string(target.x) + "," + std::to_string(target.y) + ")"; }

std::string statusText(const Frame& frame) {
  if (!frame.game_over) {
    return clock(frame.remaining) + " left";
  }
  if (!frame.game_over->winner) {
    return "Game over: draw";
  }
  return "Game over: " + *frame.game_over->winner + " wins";
}

ftxui::Element board(const Frame& frame) {
  std::vector<ftxui::Element> rows;
  for (const auto& row : frame.cells) {
    std::vector<ftxui::Element> cells;
    for (const Cell& cell : row) {
      cells.push_back(cellElement(cell));
    }
    rows.push_back(ftxui::hbox(std::move(cells)));
  }
  return box(ftxui::text("Orbital Orders · " + statusText(frame)), ftxui::vbox(std::move(rows)));
}

// Shade of an overview stretch by the share of its steps that are opportunities, e.g. "▓▓"
std::string densityShade(const OverviewRow& row) {
  if (row.opportunities == 0 || row.steps == 0) {
    return "  ";
  }
  const int share = 100 * row.opportunities / row.steps;
  if (share >= 75) {
    return "██";
  }
  if (share >= 40) {
    return "▓▓";
  }
  if (share >= 20) {
    return "▒▒";
  }
  return "░░";
}

// Half block for two steps sharing a cell: the earlier step in the lower half, the later in the upper
std::string halfBlock(bool lower, bool upper) {
  if (lower && upper) {
    return "█";
  }
  if (lower) {
    return "▄";
  }
  if (upper) {
    return "▀";
  }
  return " ";
}

// Bar for steps an executing order still runs, two steps sharing a cell as in halfBlock
std::string barLine(bool lower, bool upper) {
  if (lower && upper) {
    return "┃";
  }
  if (lower) {
    return "╻";
  }
  if (upper) {
    return "╹";
  }
  return " ";
}

// A player's lane cell: the remaining execution of the player's order as a bar up to its expected arrival
ftxui::Element playerLaneCell(const PlayerId& player, const LaneSlot& lower, const LaneSlot& upper) {
  return ftxui::text(barLine(lower.executing.count(player) > 0, upper.executing.count(player) > 0)) |
         ftxui::color(playerColor(player));
}

// One timeline row: the overview stretch with the now marker, the separator, and the lane cell with
// the opportunities and each player's expected arrival, e.g. "▶▓▓ ┆ █ ▄  "
ftxui::Element timelineRow(const OverviewRow& stretch,
                           const LaneSlot& lower,
                           const LaneSlot& upper,
                           const std::vector<PlayerId>& players) {
  std::vector<ftxui::Element> parts{
      ftxui::text(stretch.now ? "▶" : " ") | ftxui::color(textColor()) | ftxui::bold,
      ftxui::text(densityShade(stretch)) | ftxui::color(stretch.elapsed ? elapsedColor() : uplinkColor()),
      ftxui::text(" ┆ ") | ftxui::color(labelColor()),
      ftxui::text(halfBlock(lower.opportunity, upper.opportunity) + halfBlock(lower.opportunity, upper.opportunity)) |
          ftxui::color(textColor()),
  };
  for (const PlayerId& player : players) {
    parts.push_back(ftxui::text(" "));
    parts.push_back(playerLaneCell(player, lower, upper));
  }
  return ftxui::hbox(std::move(parts));
}

// The uplink: the whole game from bottom to top beside the coming steps from bottom to top, a step per half row
ftxui::Element uplink(const UplinkTimeline& timeline, const std::vector<PlayerId>& players) {
  std::vector<ftxui::Element> rows{
      ftxui::text("end ┆ +" + seconds(timeline.lane_span)) | ftxui::color(labelColor()),
  };
  const int timeline_rows = static_cast<int>(timeline.overview.size());
  for (int row = timeline_rows - 1; row >= 0; --row) {
    rows.push_back(timelineRow(timeline.overview.at(row),
                               timeline.lane.at(2 * static_cast<std::size_t>(row)),
                               timeline.lane.at(2 * static_cast<std::size_t>(row) + 1),
                               players));
  }
  rows.push_back(ftxui::text("0:00┆ now") | ftxui::color(labelColor()));
  return box(ftxui::text("Transmission"), ftxui::vbox(std::move(rows)));
}

// The player's orders, latest stage first: the one executing, the one sent, the one waiting to be sent
std::vector<ftxui::Element> orderLines(const PlayerPanel& panel) {
  if (!panel.alive) {
    return {ftxui::text("dead")};
  }
  std::vector<ftxui::Element> lines{
      ftxui::text(panel.executing_target ? "active   " + targetText(*panel.executing_target) : "no active order")};
  if (panel.transmitted_target) {
    lines.push_back(ftxui::text("sent     " + targetText(*panel.transmitted_target)));
  }
  if (panel.selected_target) {
    lines.push_back(ftxui::text("selected " + targetText(*panel.selected_target)) | ftxui::color(labelColor()));
  }
  if (panel.handover_hint == HandoverHint::CHOOSE_NOW) {
    lines.push_back(ftxui::text("handover window: choose now") | ftxui::bold);
  } else if (panel.handover_hint == HandoverHint::SUCCESSOR_READY) {
    lines.push_back(ftxui::text("successor ready") | ftxui::bold);
  }
  return lines;
}

ftxui::Element playerPanel(const PlayerId& player, const PlayerPanel& panel, int width) {
  std::vector<ftxui::Element> lines{
      ftxui::text(std::to_string(panel.banked) + " pts   +" + std::to_string(panel.held) + " held"),
      ftxui::text("chain " + std::to_string(panel.chain) + "   next handover +" + std::to_string(panel.next_handover)),
  };
  for (ftxui::Element& line : orderLines(panel)) {
    lines.push_back(std::move(line));
  }
  ftxui::Element title = ftxui::hbox({
      ftxui::text("●") | ftxui::color(playerColor(player)),
      ftxui::text(" " + player),
  });
  return box(std::move(title), ftxui::vbox(std::move(lines))) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, width);
}

ftxui::Element keyHelp() {
  return ftxui::hbox({
             ftxui::text(" "),
             ftxui::text("●") | ftxui::color(playerColor(PLAYER_A)),
             ftxui::text(" w a s d · e     "),
             ftxui::text("●") | ftxui::color(playerColor(PLAYER_B)),
             ftxui::text(" ← ↑ → ↓ · Enter     n new   q quit"),
         }) |
         ftxui::color(labelColor());
}

}  // namespace

ftxui::Element draw(const Frame& frame) {
  std::vector<PlayerId> players;
  for (const auto& [player, panel] : frame.players) {
    players.push_back(player);
  }
  ftxui::Element top = ftxui::hbox({board(frame), uplink(frame.uplink, players)});
  top->ComputeRequirement();

  // The player panels share the width of the board and uplink; the last one takes the
  // remainder, so that they end exactly below the uplink
  const int total_width = top->requirement().min_x;
  const int panel_count = std::max(static_cast<int>(frame.players.size()), 1);
  const int panel_width = total_width / panel_count;
  std::vector<ftxui::Element> panels;
  int index = 0;
  for (const auto& [player, panel] : frame.players) {
    const bool is_last = ++index == panel_count;
    panels.push_back(playerPanel(player, panel, is_last ? total_width - panel_width * (panel_count - 1) : panel_width));
  }
  return ftxui::vbox({
      std::move(top),
      ftxui::hbox(std::move(panels)),
      keyHelp(),
  });
}

}  // namespace orbital_view
}  // namespace snake
