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
constexpr int DELAY_GAUGE_FAST_WIDTH = 3;         // Exactly the current delay, e.g. "0.2", inside the bar
constexpr float PENDING_ROUTE_BRIGHTNESS = 0.3F;  // Share of the player colour left in a pending route

ftxui::Color foodColor() { return ftxui::Color::RGB(240, 200, 60); }

ftxui::Color playerColor(const PlayerId& player) {
  return player == PLAYER_A ? ftxui::Color::RGB(95, 215, 95) : ftxui::Color::RGB(215, 95, 215);
}

// Player colour blended towards black, for marks that should recede
ftxui::Color dimmed(const PlayerId& player) {
  return ftxui::Color::Interpolate(PENDING_ROUTE_BRIGHTNESS, ftxui::Color::RGB(0, 0, 0), playerColor(player));
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
  return ftxui::window(ftxui::text(" Orbital Orders · " + statusText(frame) + " "),
                       padded(ftxui::vbox(std::move(rows))));
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
  const int now_from_bottom = static_cast<int>(
      ((delay.now - delay.min).count() * last + (delay.max - delay.min).count() / 2) / (delay.max - delay.min).count());

  std::vector<ftxui::Element> rows{centredText("slow " + seconds(delay.max), DELAY_GAUGE_WIDTH) | ftxui::dim};
  for (int row = 0; row < wedge_height; ++row) {
    const int from_bottom = last - row;
    // Widths grow in steps of two from the fast to the slow end, so every row is centred exactly
    const int steps = (DELAY_GAUGE_WIDTH - DELAY_GAUGE_FAST_WIDTH) / 2;
    const int width = DELAY_GAUGE_FAST_WIDTH + 2 * ((from_bottom * steps + last / 2) / last);
    const bool filled = from_bottom <= now_from_bottom;
    const std::string fill = filled ? "▓" : "░";

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
      bar = ftxui::hbox({ftxui::text(left_fill), ftxui::text(value) | ftxui::bold, ftxui::text(right_fill)});
    } else {
      std::string line;
      for (int column = 0; column < width; ++column) {
        line += fill;
      }
      bar = filled ? ftxui::text(line) : ftxui::text(line) | ftxui::dim;
    }
    rows.push_back(centred(std::move(bar), width, DELAY_GAUGE_WIDTH));
  }
  rows.push_back(centredText("fast " + seconds(delay.min), DELAY_GAUGE_WIDTH) | ftxui::dim);
  return ftxui::window(ftxui::text(" Link delay "), padded(ftxui::vbox(std::move(rows))));
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
      ftxui::text(" "),
      ftxui::text("●") | ftxui::color(playerColor(player)),
      ftxui::text(" " + player + " "),
  });
  return ftxui::window(std::move(title), padded(ftxui::vbox(std::move(lines)))) |
         ftxui::size(ftxui::WIDTH, ftxui::EQUAL, width);
}

ftxui::Element keyHelp() {
  return ftxui::hbox({
             ftxui::text(" "),
             ftxui::text("●") | ftxui::color(playerColor(PLAYER_A)),
             ftxui::text(" w a s d · e     "),
             ftxui::text("●") | ftxui::color(playerColor(PLAYER_B)),
             ftxui::text(" ← ↑ → ↓ · Enter     n new   q quit"),
         }) |
         ftxui::dim;
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
