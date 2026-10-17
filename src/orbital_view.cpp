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
using orbital_frame::DelayGauge;
using orbital_frame::Frame;
using orbital_frame::PlayerPanel;

constexpr int DELAY_GAUGE_WIDTH = 10;  // Width of the wedge at its slow end; even, so it has a middle column

// Symbol of a player's snake head and cursor
char playerSymbol(const PlayerId& player) { return player == PLAYER_A ? 'A' : 'B'; }

char tailSymbol(const PlayerId& player) { return player == PLAYER_A ? 'a' : 'b'; }

// A board cell is two terminal columns wide, so that a square board looks square
std::string cellSymbol(const Cell& cell) {
  switch (cell.role) {
    case CellRole::EMPTY:
      return "  ";
    case CellRole::PENDING_ROUTE:
      return ". ";
    case CellRole::ACTIVE_ROUTE:
      return "+ ";
    case CellRole::FOOD:
      return "* ";
    case CellRole::TARGET:
      return "? ";
    case CellRole::SNAKE_BODY:
      return {tailSymbol(*cell.player), ' '};
    case CellRole::SNAKE_HEAD:
    case CellRole::CURSOR:
      return {playerSymbol(*cell.player), ' '};
    case CellRole::DEAD_HEAD:
      return "x ";
  }
  return "  ";
}

// Seconds with one decimal, e.g. "0.6s"
std::string seconds(std::chrono::milliseconds duration) {
  std::ostringstream text;
  text.setf(std::ios::fixed);
  text.precision(1);
  text << static_cast<double>(duration.count()) / 1000.0 << "s";
  return text.str();
}

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
    std::string line;
    for (const Cell& cell : row) {
      line += cellSymbol(cell);
    }
    rows.push_back(ftxui::text(line));
  }
  return ftxui::window(ftxui::text(" Orbital Orders · " + statusText(frame) + " "),
                       padded(ftxui::vbox(std::move(rows))));
}

// Link delay of a submission sent now: a wedge as tall as the board, wide and slow at the top and
// narrow and fast at the bottom, filled from the bottom up to the current delay, which is marked
ftxui::Element delayGauge(const DelayGauge& delay, int height) {
  const int wedge_height = std::max(height - 2, 2);  // Leaves a row each for the slow and fast labels
  const int last = wedge_height - 1;
  const int now_from_bottom = static_cast<int>(
      ((delay.now - delay.min).count() * last + (delay.max - delay.min).count() / 2) / (delay.max - delay.min).count());
  const int middle = DELAY_GAUGE_WIDTH / 2;

  std::vector<ftxui::Element> rows{ftxui::text("slow " + seconds(delay.max))};
  for (int row = 0; row < wedge_height; ++row) {
    const int from_bottom = last - row;
    const int half_width = (from_bottom * middle + last / 2) / last;
    std::string line(DELAY_GAUGE_WIDTH + 1, ' ');
    for (int column = middle - half_width; column <= middle + half_width; ++column) {
      line[column] = from_bottom <= now_from_bottom ? '#' : '.';
    }
    line += from_bottom == now_from_bottom ? " < " + seconds(delay.now) : "";
    rows.push_back(ftxui::text(line));
  }
  rows.push_back(ftxui::text("fast " + seconds(delay.min)));
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

ftxui::Element playerPanel(const PlayerId& player, const PlayerPanel& panel) {
  std::vector<ftxui::Element> lines{
      ftxui::text(std::to_string(panel.credited) + " pts   +" + std::to_string(panel.unbanked) + " unbanked"),
      ftxui::text(orderText(panel)),
  };
  if (panel.pending_target && panel.executing_target) {
    lines.push_back(ftxui::text("executing " + targetText(*panel.executing_target)));
  }
  return ftxui::window(ftxui::text(" " + std::string{playerSymbol(player)} + " " + player + " "),
                       padded(ftxui::vbox(std::move(lines)))) |
         ftxui::flex;
}

ftxui::Element keyHelp() { return ftxui::text(" A  w a s d · e     B  ← ↑ → ↓ · Enter     n new   q quit"); }

}  // namespace

ftxui::Element draw(const Frame& frame) {
  std::vector<ftxui::Element> panels;
  for (const auto& [player, panel] : frame.players) {
    panels.push_back(playerPanel(player, panel));
  }
  // The vbox is as wide as the board and link delay; the panels share that width
  return ftxui::hbox({
      ftxui::vbox({
          ftxui::hbox({board(frame), delayGauge(frame.delay, static_cast<int>(frame.cells.size()))}),
          ftxui::hbox(std::move(panels)),
          keyHelp(),
      }),
      ftxui::filler(),
  });
}

}  // namespace orbital_view
}  // namespace snake
