#include "snake/direction_command_filter.hpp"

namespace snake {
namespace direction_command_filter {

bool is_opposite(Direction a, Direction b) {
  return (a == Direction::UP && b == Direction::DOWN) || (a == Direction::DOWN && b == Direction::UP) ||
         (a == Direction::LEFT && b == Direction::RIGHT) || (a == Direction::RIGHT && b == Direction::LEFT);
}

PerPlayerDirectionQueue try_add(PerPlayerDirectionQueue pending_directions, const PerPlayerSnakes& snakes,
                                const DirectionCommand& cmd) {
  // Look up player's snake and direction queue
  auto snake_it = snakes.find(cmd.player_id);
  auto queue_it = pending_directions.find(cmd.player_id);

  if (snake_it == snakes.end() || queue_it == pending_directions.end()) {
    return pending_directions;  // Player not found, no change
  }

  const Snake& snake = snake_it->second;
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
    return pending_directions;  // Reject, return unchanged
  }

  // Rule 3: Reject 0° turn (same as effective)
  if (new_dir == effective_dir) {
    return pending_directions;  // Reject, return unchanged
  }

  // Rule 4: Reject if queue is full
  if (player_queue.size() >= MAX_QUEUE_SIZE) {
    return pending_directions;  // Reject, return unchanged
  }

  // Rule 5: Accept and add to queue
  player_queue.push_back(new_dir);

  return pending_directions;
}

std::tuple<PerPlayerDirectionQueue, PerPlayerDirection> try_consume_next(PerPlayerDirectionQueue pending_directions) {
  PerPlayerDirection next_directions;

  // Consume one direction from each player's queue
  for (auto& [player_id, player_queue] : pending_directions) {
    if (!player_queue.empty()) {
      Direction next = player_queue.front();
      player_queue.pop_front();
      next_directions[player_id] = next;
    }
  }

  return std::make_tuple(std::move(pending_directions), std::move(next_directions));
}

}  // namespace direction_command_filter
}  // namespace snake
