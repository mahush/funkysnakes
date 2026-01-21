#include "snake/direction_command_filter.hpp"

#include "snake/game_messages.hpp"

namespace snake {
namespace direction_command_filter {

bool is_opposite(Direction a, Direction b) {
  return (a == Direction::UP && b == Direction::DOWN) || (a == Direction::DOWN && b == Direction::UP) ||
         (a == Direction::LEFT && b == Direction::RIGHT) || (a == Direction::RIGHT && b == Direction::LEFT);
}

State try_add(State state, const PerPlayerSnakes& snakes, const DirectionCommand& cmd) {
  // Look up player's snake
  auto snake_it = snakes.find(cmd.player_id);
  if (snake_it == snakes.end()) {
    return state;  // Player not found, no change
  }

  const Snake& snake = snake_it->second;

  // Lazy initialization: create queue if player doesn't have one yet
  auto [queue_it, inserted] = state.queues.try_emplace(cmd.player_id, std::deque<Direction>{});
  std::deque<Direction>& player_queue = queue_it->second;
  Direction new_dir = cmd.new_direction;

  // Rule 1: Undo rule - if opposite of first queued direction, clear queue
  if (!player_queue.empty()) {
    Direction first_queued = player_queue.front();
    if (is_opposite(new_dir, first_queued)) {
      player_queue.clear();
      // Continue to add the new direction below
    }
  }

  // Determine effective direction (last queued, or snake's current if queue empty)
  Direction effective_dir = player_queue.empty() ? snake.current_direction : player_queue.back();

  // Rule 2: Reject 180° turn from effective direction
  if (is_opposite(new_dir, effective_dir)) {
    return state;  // Reject, return unchanged
  }

  // Rule 3: Reject 0° turn (same as effective)
  if (new_dir == effective_dir) {
    return state;  // Reject, return unchanged
  }

  // Rule 4: Reject if queue is full
  if (player_queue.size() >= MAX_QUEUE_SIZE) {
    return state;  // Reject, return unchanged
  }

  // Rule 5: Accept and add to queue
  player_queue.push_back(new_dir);

  return state;
}

std::tuple<State, PerPlayerDirection> try_consume_next(State state) {
  PerPlayerDirection next_directions;

  // Consume one direction from each player's queue
  for (auto& [player_id, player_queue] : state.queues) {
    if (!player_queue.empty()) {
      Direction next = player_queue.front();
      player_queue.pop_front();
      next_directions[player_id] = next;
    }
  }

  return std::make_tuple(std::move(state), std::move(next_directions));
}

}  // namespace direction_command_filter
}  // namespace snake
