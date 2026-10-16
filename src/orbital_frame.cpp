#include "snake/orbital_frame.hpp"

#include <algorithm>

#include "common/snake_model.hpp"
#include "orbital/latency_policy.hpp"

namespace snake {
namespace orbital_frame {

namespace {

using orbital::game_boundary::Views;
using orbital::order_authority::OrderRecord;

void put(CellGrid& cells, Point p, Cell cell) {
  if (p.y >= 0 && p.y < static_cast<int>(cells.size()) && p.x >= 0 && p.x < static_cast<int>(cells[p.y].size())) {
    cells[p.y][p.x] = std::move(cell);
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

void putRoute(CellGrid& cells,
              Point head,
              const std::vector<Direction>& route,
              const Board& board,
              CellRole role,
              const PlayerId& player) {
  Point p = head;
  for (Direction move : route) {
    p = stepOn(p, move, board);
    put(cells, p, Cell{role, player});
  }
}

// Board cells, drawn lowest precedence first so that later cells cover earlier ones
CellGrid buildCells(const Views& views,
                    const std::map<PlayerId, Point>& cursors,
                    const orbital_food_labels::FoodLabels& food_labels) {
  const Board& board = views.arena.board;
  CellGrid cells(board.height, std::vector<Cell>(board.width));

  for (const auto& [player, route] : views.pending_routes) {
    putRoute(cells, snake_model::head(views.arena.snakes.at(player)), route, board, CellRole::PENDING_ROUTE, player);
  }
  for (const auto& [player, active] : views.arena.active) {
    putRoute(cells,
             snake_model::head(views.arena.snakes.at(player)),
             active.remaining_route,
             board,
             CellRole::ACTIVE_ROUTE,
             player);
  }
  for (const Point& food : views.arena.food) {
    put(cells, food, Cell{CellRole::FOOD, std::nullopt});
  }
  for (const auto& [label, food] : food_labels) {
    put(cells, food, Cell{CellRole::FOOD, std::nullopt, label});
  }
  for (const auto& [player, orders] : views.orders.players) {
    if (orders.pending) {
      put(cells, orders.pending->order.target, Cell{CellRole::TARGET, player});
    }
  }
  for (const auto& [player, active] : views.arena.active) {
    put(cells, active.target, Cell{CellRole::TARGET, player});
  }
  for (const auto& [player, snake] : views.arena.snakes) {
    for (const Point& segment : snake_model::tail(snake)) {
      put(cells, segment, Cell{CellRole::SNAKE_BODY, player});
    }
    put(cells,
        snake_model::head(snake),
        Cell{snake_model::alive(snake) ? CellRole::SNAKE_HEAD : CellRole::DEAD_HEAD, player});
  }
  for (const auto& [player, cursor] : cursors) {
    put(cells, cursor, Cell{CellRole::CURSOR, player});
  }
  return cells;
}

PlayerPanel buildPanel(const Views& views, const PlayerId& player, const std::map<PlayerId, Point>& cursors) {
  PlayerPanel panel;
  if (auto score = views.game.players.find(player); score != views.game.players.end()) {
    panel.credited = score->second.credited;
    panel.unbanked = score->second.unbanked;
    panel.chain = score->second.chain;
    panel.next_handover = score->second.next_handover;
  }
  panel.alive = snake_model::alive(views.arena.snakes.at(player));
  if (auto orders = views.orders.players.find(player); orders != views.orders.players.end()) {
    if (const std::optional<OrderRecord>& executing = orders->second.executing) {
      panel.executing_target = executing->order.target;
    }
    if (const std::optional<OrderRecord>& pending = orders->second.pending) {
      panel.pending_target = pending->order.target;
      panel.pending_remaining = std::max(pending->deadline - views.now, std::chrono::milliseconds{0});
    }
  }
  if (auto cursor = cursors.find(player); cursor != cursors.end()) {
    panel.cursor = cursor->second;
  }
  return panel;
}

}  // namespace

Frame build(const Views& views,
            const std::map<PlayerId, Point>& cursors,
            const orbital_food_labels::FoodLabels& food_labels) {
  Frame frame;
  frame.cells = buildCells(views, cursors, food_labels);
  frame.delay =
      DelayGauge{views.submission_delay, orbital::latency_policy::MIN_DELAY, orbital::latency_policy::MAX_DELAY};
  frame.remaining = views.game.remaining;
  if (views.game.result) {
    frame.game_over = GameOver{views.game.result->winner};
  }
  for (const auto& [player, snake] : views.arena.snakes) {
    frame.players[player] = buildPanel(views, player, cursors);
  }
  return frame;
}

}  // namespace orbital_frame
}  // namespace snake
