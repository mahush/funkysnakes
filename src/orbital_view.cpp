#include "snake/orbital_view.hpp"

#include <algorithm>
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
using Lines = std::vector<std::string>;

constexpr int DELAY_GAUGE_WIDTH = 10;  // Width of the wedge at its slow end; even, so it has a middle column

// Symbol of a player's snake head and cursor
char playerSymbol(const PlayerId& player) { return player == PLAYER_A ? 'A' : 'B'; }

char tailSymbol(const PlayerId& player) { return player == PLAYER_A ? 'a' : 'b'; }

char cellSymbol(const Cell& cell) {
  switch (cell.role) {
    case CellRole::EMPTY:
      return ' ';
    case CellRole::PENDING_ROUTE:
      return '.';
    case CellRole::ACTIVE_ROUTE:
      return '+';
    case CellRole::FOOD:
      return '*';
    case CellRole::TARGET:
      return '?';
    case CellRole::SNAKE_BODY:
      return tailSymbol(*cell.player);
    case CellRole::SNAKE_HEAD:
    case CellRole::CURSOR:
      return playerSymbol(*cell.player);
    case CellRole::DEAD_HEAD:
      return 'x';
  }
  return ' ';
}

std::string seconds(std::chrono::milliseconds duration) {
  std::ostringstream text;
  text.setf(std::ios::fixed);
  text.precision(1);
  text << static_cast<double>(duration.count()) / 1000.0 << "s";
  return text.str();
}

Lines boardLines(const Frame& frame) {
  const int width = frame.cells.empty() ? 0 : static_cast<int>(frame.cells.front().size());
  const std::string horizontal = "+" + std::string(width, '-') + "+";
  Lines lines{horizontal};
  for (const auto& row : frame.cells) {
    std::string line = "|";
    for (const Cell& cell : row) {
      line += cellSymbol(cell);
    }
    lines.push_back(line + "|");
  }
  lines.push_back(horizontal);
  return lines;
}

// Link delay of a submission sent now: a wedge as tall as the board, wide and slow at the top and
// narrow and fast at the bottom, filled from the bottom up to the current delay, which is marked
Lines delayLines(const DelayGauge& delay, int height) {
  const int last = std::max(height - 1, 1);
  const int now_from_bottom = static_cast<int>(
      ((delay.now - delay.min).count() * last + (delay.max - delay.min).count() / 2) / (delay.max - delay.min).count());
  const int middle = DELAY_GAUGE_WIDTH / 2;

  Lines gauge;
  for (int row = 0; row < height; ++row) {
    const int from_bottom = last - row;
    const int half_width = (from_bottom * middle + last / 2) / last;
    std::string line(DELAY_GAUGE_WIDTH + 1, ' ');
    for (int column = middle - half_width; column <= middle + half_width; ++column) {
      line[column] = from_bottom <= now_from_bottom ? '#' : '.';
    }
    line += from_bottom == now_from_bottom ? " < " + seconds(delay.now) : std::string(7, ' ');
    if (row == 0) {
      line += "   " + seconds(delay.max) + " slow";
    } else if (row == last) {
      line += "   " + seconds(delay.min) + " fast";
    }
    gauge.push_back(line);
  }
  return gauge;
}

std::string targetText(Point target) { return "(" + std::to_string(target.x) + "," + std::to_string(target.y) + ")"; }

std::string playerLine(const PlayerId& player, const PlayerPanel& panel) {
  std::string line = player + " [" + playerSymbol(player) + "]:";
  line += " points " + std::to_string(panel.credited) + " (+" + std::to_string(panel.unbanked) + " unbanked)";
  if (!panel.alive) {
    line += "  dead";
  }
  if (panel.executing_target) {
    line += "  executing " + targetText(*panel.executing_target);
  }
  if (panel.pending_target) {
    line += "  pending " + targetText(*panel.pending_target) + " in " + seconds(panel.pending_remaining);
  }
  if (panel.cursor) {
    line += "  cursor " + targetText(*panel.cursor);
  }
  return line;
}

std::string resultLine(const Frame& frame) {
  if (!frame.game_over) {
    return "Time left " + seconds(frame.remaining);
  }
  if (!frame.game_over->winner) {
    return "Game over: draw";
  }
  return "Game over: " + *frame.game_over->winner + " wins";
}

}  // namespace

ftxui::Element draw(const Frame& frame) {
  Lines lines = boardLines(frame);
  const Lines gauge = delayLines(frame.delay, static_cast<int>(frame.cells.size()));
  lines.front() += "  Link delay";
  for (size_t row = 0; row < gauge.size(); ++row) {
    lines[row + 1] += "  " + gauge[row];  // Right of the board row, after the top border
  }
  lines.push_back(resultLine(frame));
  for (const auto& [player, panel] : frame.players) {
    lines.push_back(playerLine(player, panel));
  }

  std::vector<ftxui::Element> elements{ftxui::text("funkysnakes: Orbital Orders")};
  for (const std::string& line : lines) {
    elements.push_back(ftxui::text(line));
  }
  elements.push_back(ftxui::text("A: w/a/s/d cursor, e order   B: arrows cursor, Enter order   n new game   q quit"));
  return ftxui::vbox(std::move(elements));
}

}  // namespace orbital_view
}  // namespace snake
