#include "snake/orbital_frame.hpp"

#include <algorithm>
#include <chrono>
#include <map>

#include "common/snake_model.hpp"

namespace snake {
namespace orbital_frame {

namespace {

using orbital::StepId;
using orbital::game_boundary::Views;
using orbital::order_authority::OrderRecord;
using orbital::order_authority::Status;
using std::chrono::milliseconds;

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
CellGrid buildCells(const Views& views, const std::map<PlayerId, Point>& cursors) {
  const Board& board = views.arena.board;
  CellGrid cells(board.height, std::vector<Cell>(board.width));

  for (const auto& [player, route] : views.selected_routes) {
    putRoute(cells, snake_model::head(views.arena.snakes.at(player)), route, board, CellRole::SELECTED_ROUTE, player);
  }
  for (const auto& [player, route] : views.transmitted_routes) {
    putRoute(cells, snake_model::head(views.arena.snakes.at(player)), route, board, CellRole::SENT_ROUTE, player);
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
  for (const auto& [player, orders] : views.orders.players) {
    for (const auto& order : {orders.selected, orders.transmitted}) {
      if (order) {
        put(cells, order->order.target, Cell{CellRole::TARGET, player});
      }
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
    panel.banked = score->second.banked;
    panel.held = score->second.held;
    panel.chain = score->second.chain;
    panel.next_handover = score->second.next_handover;
  }
  panel.alive = snake_model::alive(views.arena.snakes.at(player));
  if (auto orders = views.orders.players.find(player); orders != views.orders.players.end()) {
    if (const std::optional<OrderRecord>& executing = orders->second.executing) {
      panel.executing_target = executing->order.target;
    }
    if (const std::optional<OrderRecord>& transmitted = orders->second.transmitted) {
      panel.transmitted_target = transmitted->order.target;
    }
    if (const std::optional<OrderRecord>& selected = orders->second.selected) {
      panel.selected_target = selected->order.target;
    }
  }
  if (auto cursor = cursors.find(player); cursor != cursors.end()) {
    panel.cursor = cursor->second;
  }
  return panel;
}

// Time of the latest step that ran: the clock never stands on a step that has not run
milliseconds lastStep(const Views& views) { return views.now / views.step_interval * views.step_interval; }

// Time each player's executing order is expected to arrive, one step per remaining move
std::map<PlayerId, milliseconds> expectedArrivals(const Views& views) {
  std::map<PlayerId, milliseconds> arrivals;
  for (const auto& [player, active] : views.arena.active) {
    if (!active.remaining_route.empty()) {
      arrivals[player] = lastStep(views) + views.step_interval * static_cast<int>(active.remaining_route.size());
    }
  }
  return arrivals;
}

std::vector<OverviewRow> buildOverview(const Views& views, int rows) {
  // The remaining time counts from the last step that ran, not from the current time between steps
  const milliseconds duration = lastStep(views) + views.step_interval * views.game.remaining_steps;
  std::vector<OverviewRow> overview(rows);
  for (int row = 0; row < rows; ++row) {
    const milliseconds from = duration * row / rows;
    const milliseconds until = duration * (row + 1) / rows;
    OverviewRow& stretch = overview[row];
    stretch.steps = static_cast<int>(until / views.step_interval - from / views.step_interval);
    stretch.opportunities = static_cast<int>(std::count_if(views.transmission_schedule.begin(),
                                                           views.transmission_schedule.end(),
                                                           [&views, from, until](StepId opportunity) {
                                                             const milliseconds at =
                                                                 views.step_interval * opportunity.value;
                                                             return from < at && at <= until;
                                                           }));
    stretch.elapsed = until <= views.now;
    stretch.now = from <= views.now && (views.now < until || row == rows - 1);
  }
  if (views.now >= duration && rows > 0) {
    overview.back().elapsed = false;  // At the end the last stretch shows where time stopped
  }
  return overview;
}

std::vector<LaneSlot> buildLane(const Views& views, int slots) {
  const std::map<PlayerId, milliseconds> arrivals = expectedArrivals(views);
  std::vector<LaneSlot> lane(slots);
  for (int slot = 0; slot < slots; ++slot) {
    const milliseconds at = lastStep(views) + views.step_interval * (slot + 1);
    lane[slot].opportunity = std::binary_search(views.transmission_schedule.begin(),
                                                views.transmission_schedule.end(),
                                                StepId{static_cast<int>(at / views.step_interval)});
    for (const auto& [player, arrival] : arrivals) {
      if (at <= arrival) {
        lane[slot].executing.insert(player);
      }
    }
  }
  return lane;
}

UplinkTimeline buildUplink(const Views& views) {
  const int rows = std::max(views.arena.board.height - UPLINK_LABEL_ROWS, 2);
  UplinkTimeline uplink;
  uplink.lane_span = views.step_interval * (2 * rows);
  uplink.overview = buildOverview(views, rows);
  uplink.lane = buildLane(views, 2 * rows);
  return uplink;
}

}  // namespace

Frame build(const Views& views,
            const std::map<PlayerId, Point>& cursors,
            const std::map<PlayerId, int>& handover_flashes) {
  Frame frame;
  frame.cells = buildCells(views, cursors);
  frame.uplink = buildUplink(views);
  frame.remaining = views.step_interval * views.game.remaining_steps;
  if (views.game.result) {
    frame.game_over = GameOver{views.game.result->winner};
  }
  for (const auto& [player, snake] : views.arena.snakes) {
    frame.players[player] = buildPanel(views, player, cursors);
    if (auto prospect = views.handover_prospects.find(player); prospect != views.handover_prospects.end()) {
      frame.players[player].handover_hint = prospect->second;
    }
    if (auto flash = handover_flashes.find(player); flash != handover_flashes.end()) {
      frame.players[player].handover_flash = flash->second;
    }
  }
  return frame;
}

}  // namespace orbital_frame
}  // namespace snake
