#include "snake/orbital_view.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "ftxui/screen/string.hpp"

namespace snake {
namespace orbital_view {

namespace {

using orbital_frame::Cell;
using orbital_frame::CellRole;
using orbital_frame::DelayGauge;
using orbital_frame::Frame;
using orbital_frame::PlayerPanel;

// Width of the link delay wedge at its slow and fast ends; both odd, so that every row is centred
constexpr int DELAY_GAUGE_WIDTH = 11;
constexpr int DELAY_GAUGE_FAST_WIDTH = 3;  // Exactly the current delay, e.g. "0.2", inside the bar
// Content width of the link delay box: the wedge with a column of space on each side, so that the
// box title "Link delay" has border line on both sides
constexpr int DELAY_BOX_WIDTH = DELAY_GAUGE_WIDTH + 2;

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
ftxui::Color delayFilledColor() { return gruvbox::fg4(); }
ftxui::Color delayEmptyColor() { return gruvbox::bg2(); }

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
    case CellRole::PENDING_ROUTE:
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
  if (cell.role == CellRole::PENDING_ROUTE) {
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

// Text centred in a fixed width: the same number of spaces left and right, one more right if odd
ftxui::Element centred(ftxui::Element content, int content_width, int width) {
  const int left = (width - content_width) / 2;
  return ftxui::hbox({ftxui::text(std::string(left, ' ')),
                      std::move(content),
                      ftxui::text(std::string(width - content_width - left, ' '))});
}

ftxui::Element centredText(const std::string& text, int width) {
  return centred(ftxui::text(text), ftxui::string_width(text), width);
}

// Link delay of a submission sent now: a wedge as tall as the board, wide and slow at the top and
// narrow and fast at the bottom, filled from the bottom up to the current delay, whose value is
// written into its row. The narrow end fits that value, so the gauge always has the same width.
ftxui::Element delayGauge(const DelayGauge& delay, int height) {
  const int wedge_height = std::max(height - 2, 2);  // Leaves a row each for the slow and fast labels
  const int last = wedge_height - 1;
  // The delay's value sits on the row of its level, from the second row (fastest) to the top row
  // (slowest), so the bottom row is filled even at the fastest delay
  const int value_rows = last - 1;
  const int now_from_bottom =
      1 + static_cast<int>(((delay.now - delay.min).count() * value_rows + (delay.max - delay.min).count() / 2) /
                           (delay.max - delay.min).count());

  std::vector<ftxui::Element> rows{centredText("slow " + seconds(delay.max), DELAY_BOX_WIDTH) |
                                   ftxui::color(labelColor())};
  for (int row = 0; row < wedge_height; ++row) {
    const int from_bottom = last - row;
    // Widths grow in steps of two from the fast to the slow end, so every row is centred exactly
    const int steps = (DELAY_GAUGE_WIDTH - DELAY_GAUGE_FAST_WIDTH) / 2;
    const int width = DELAY_GAUGE_FAST_WIDTH + 2 * ((from_bottom * steps + last / 2) / last);
    const bool filled = from_bottom <= now_from_bottom;
    const std::string fill = filled ? "▓" : "░";
    const ftxui::Color fill_color = filled ? delayFilledColor() : delayEmptyColor();

    ftxui::Element bar;
    if (from_bottom == now_from_bottom) {
      const std::string value = decimalSeconds(delay.now);
      const int value_width = static_cast<int>(value.size());
      const int left = (width - value_width) / 2;
      std::string left_fill;
      std::string right_fill;
      for (int column = 0; column < left; ++column) {
        left_fill += fill;
      }
      for (int column = 0; column < width - value_width - left; ++column) {
        right_fill += fill;
      }
      bar = ftxui::hbox({ftxui::text(left_fill) | ftxui::color(fill_color),
                         ftxui::text(value) | ftxui::color(textColor()) | ftxui::bold,
                         ftxui::text(right_fill) | ftxui::color(fill_color)});
    } else {
      std::string line;
      for (int column = 0; column < width; ++column) {
        line += fill;
      }
      bar = ftxui::text(line) | ftxui::color(fill_color);
    }
    rows.push_back(centred(std::move(bar), width, DELAY_BOX_WIDTH));
  }
  rows.push_back(centredText("fast " + seconds(delay.min), DELAY_BOX_WIDTH) | ftxui::color(labelColor()));
  return box(ftxui::text("Link delay"), ftxui::vbox(std::move(rows)));
}

std::string orderText(const PlayerPanel& panel) {
  if (!panel.alive) {
    return "dead";
  }
  if (panel.pending_target) {
    return "pending " + targetText(*panel.pending_target) + " in " + seconds(panel.pending_remaining);
  }
  if (panel.executing_target) {
    return "executing " + targetText(*panel.executing_target);
  }
  return "no order";
}

ftxui::Element playerPanel(const PlayerId& player, const PlayerPanel& panel, int width) {
  std::vector<ftxui::Element> lines{
      ftxui::text(std::to_string(panel.credited) + " pts   +" + std::to_string(panel.unbanked) + " unbanked"),
      ftxui::text(orderText(panel)),
  };
  if (panel.pending_target && panel.executing_target) {
    lines.push_back(ftxui::text("executing " + targetText(*panel.executing_target)));
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
  ftxui::Element top = ftxui::hbox({board(frame), delayGauge(frame.delay, static_cast<int>(frame.cells.size()))});
  top->ComputeRequirement();

  // The player panels share the width of the board and link delay; the last one takes the
  // remainder, so that they end exactly below the link delay
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
