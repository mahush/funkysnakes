#include "snake/orbital_frame.hpp"

#include <algorithm>
#include <cstdlib>
#include <sstream>

#include "common/snake_model.hpp"
#include "orbital/latency_policy.hpp"

namespace snake {
namespace orbital_frame {

namespace {

using orbital::game_boundary::Views;
using orbital::order_authority::OrderRecord;
using Grid = std::vector<std::string>;

constexpr int DELAY_GAUGE_WIDTH = 10;  // Width of the wedge at its slow end; even, so it has a middle column

// Symbol of a player's snake head and cursor
char playerSymbol(const PlayerId& player) { return player == PLAYER_A ? 'A' : 'B'; }

char tailSymbol(const PlayerId& player) { return player == PLAYER_A ? 'a' : 'b'; }

void put(Grid& grid, Point p, char symbol) {
  if (p.y >= 0 && p.y < static_cast<int>(grid.size()) && p.x >= 0 && p.x < static_cast<int>(grid[p.y].size())) {
    grid[p.y][p.x] = symbol;
  }
}

// Position after one move on a wrapping board
Point stepOn(Point p, Direction move, const Board& board) {
  switch (move) {
    case Direction::UP:
      return Point{p.x, (p.y - 1 + board.height) % board.height};
    case Direction::DOWN:
      return Point{p.x, (p.y + 1) % board.height};
    case Direction::LEFT:
      return Point{(p.x - 1 + board.width) % board.width, p.y};
    case Direction::RIGHT:
      return Point{(p.x + 1) % board.width, p.y};
  }
  return p;
}

void drawRoute(Grid& grid, Point head, const std::vector<Direction>& route, const Board& board, char symbol) {
  Point p = head;
  for (Direction move : route) {
    p = stepOn(p, move, board);
    put(grid, p, symbol);
  }
}

Grid drawBoard(const Views& views, const std::map<PlayerId, Point>& cursors) {
  const Board& board = views.arena.board;
  Grid grid(board.height, std::string(board.width, ' '));

  for (const auto& [player, route] : views.pending_routes) {
    drawRoute(grid, snake_model::head(views.arena.snakes.at(player)), route, board, '.');
  }
  for (const auto& [player, active] : views.arena.active) {
    drawRoute(grid, snake_model::head(views.arena.snakes.at(player)), active.remaining_route, board, '+');
  }
  for (const Point& food : views.arena.food) {
    put(grid, food, '*');
  }
  for (const auto& [player, orders] : views.orders.players) {
    if (orders.pending) {
      put(grid, orders.pending->order.target, '?');
    }
  }
  for (const auto& [player, active] : views.arena.active) {
    put(grid, active.target, '@');
  }
  for (const auto& [player, snake] : views.arena.snakes) {
    for (const Point& segment : snake_model::tail(snake)) {
      put(grid, segment, tailSymbol(player));
    }
    put(grid, snake_model::head(snake), snake_model::alive(snake) ? playerSymbol(player) : 'x');
  }
  for (const auto& [player, cursor] : cursors) {
    put(grid, cursor, playerSymbol(player));
  }

  Grid framed;
  const std::string horizontal = "+" + std::string(board.width, '-') + "+";
  framed.push_back(horizontal);
  for (const std::string& row : grid) {
    framed.push_back("|" + row + "|");
  }
  framed.push_back(horizontal);
  return framed;
}

std::string seconds(std::chrono::milliseconds duration) {
  std::ostringstream text;
  text.setf(std::ios::fixed);
  text.precision(1);
  text << static_cast<double>(duration.count()) / 1000.0 << "s";
  return text.str();
}

// Link delay of a submission sent now: a wedge as tall as the board, wide and slow at the top and
// narrow and fast at the bottom, filled from the bottom up to the current delay, which is marked
Grid delayGauge(std::chrono::milliseconds delay, int height) {
  using orbital::latency_policy::MAX_DELAY;
  using orbital::latency_policy::MIN_DELAY;
  const int last = std::max(height - 1, 1);
  const int now_from_bottom = static_cast<int>(
      ((delay - MIN_DELAY).count() * last + (MAX_DELAY - MIN_DELAY).count() / 2) / (MAX_DELAY - MIN_DELAY).count());
  const int middle = DELAY_GAUGE_WIDTH / 2;

  Grid gauge;
  for (int row = 0; row < height; ++row) {
    const int from_bottom = last - row;
    const int half_width = (from_bottom * middle + last / 2) / last;
    std::string line(DELAY_GAUGE_WIDTH + 1, ' ');
    for (int column = middle - half_width; column <= middle + half_width; ++column) {
      line[column] = from_bottom <= now_from_bottom ? '#' : '.';
    }
    line += from_bottom == now_from_bottom ? " < " + seconds(delay) : std::string(7, ' ');
    if (row == 0) {
      line += "   " + seconds(MAX_DELAY) + " slow";
    } else if (row == last) {
      line += "   " + seconds(MIN_DELAY) + " fast";
    }
    gauge.push_back(line);
  }
  return gauge;
}

std::string targetText(Point target) { return "(" + std::to_string(target.x) + "," + std::to_string(target.y) + ")"; }

std::string playerLine(const Views& views, const PlayerId& player, const std::map<PlayerId, Point>& cursors) {
  std::string line = player + " [" + playerSymbol(player) + "]:";
  if (auto score = views.game.players.find(player); score != views.game.players.end()) {
    line += " points " + std::to_string(score->second.credited) + " (+" + std::to_string(score->second.unbanked) +
            " unbanked)";
  }
  if (auto snake = views.arena.snakes.find(player);
      snake != views.arena.snakes.end() && !snake_model::alive(snake->second)) {
    line += "  dead";
  }
  if (auto orders = views.orders.players.find(player); orders != views.orders.players.end()) {
    if (const std::optional<OrderRecord>& executing = orders->second.executing) {
      line += "  executing " + targetText(executing->order.target);
    }
    if (const std::optional<OrderRecord>& pending = orders->second.pending) {
      line += "  pending " + targetText(pending->order.target) + " in " +
              seconds(std::max(pending->deadline - views.now, std::chrono::milliseconds{0}));
    }
  }
  if (auto cursor = cursors.find(player); cursor != cursors.end()) {
    line += "  cursor " + targetText(cursor->second);
  }
  return line;
}

std::string resultLine(const Views& views) {
  if (!views.game.result) {
    return "Time left " + seconds(views.game.remaining);
  }
  if (!views.game.result->winner) {
    return "Game over: draw";
  }
  return "Game over: " + *views.game.result->winner + " wins";
}

}  // namespace

std::vector<std::string> render(const Views& views, const std::map<PlayerId, Point>& cursors) {
  Grid lines = drawBoard(views, cursors);
  const Grid gauge = delayGauge(views.submission_delay, views.arena.board.height);
  lines.front() += "  Link delay";
  for (size_t row = 0; row < gauge.size(); ++row) {
    lines[row + 1] += "  " + gauge[row];  // Right of the board row, after the top border
  }
  lines.push_back(resultLine(views));
  for (const auto& [player, snake] : views.arena.snakes) {
    lines.push_back(playerLine(views, player, cursors));
  }
  return lines;
}

}  // namespace orbital_frame
}  // namespace snake
