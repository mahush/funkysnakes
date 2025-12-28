#include "snake/direction_command_filter.hpp"

namespace snake {
namespace direction_command_filter {

bool is_opposite(Direction a, Direction b) {
  return (a == Direction::UP && b == Direction::DOWN) || (a == Direction::DOWN && b == Direction::UP) ||
         (a == Direction::LEFT && b == Direction::RIGHT) || (a == Direction::RIGHT && b == Direction::LEFT);
}

std::map<PlayerId, DirectionCommandFilterState> try_add(std::map<PlayerId, DirectionCommandFilterState> state,
                                                        const std::map<PlayerId, Snake>& snakes,
                                                        const DirectionCommand& cmd) {
  // Look up player's snake and filter
  auto snake_it = snakes.find(cmd.player_id);
  auto filter_it = state.find(cmd.player_id);

  if (snake_it == snakes.end() || filter_it == state.end()) {
    return state;  // Player not found, no change
  }

  const Snake& snake = snake_it->second;
  DirectionCommandFilterState filter_state = filter_it->second;
  Direction new_dir = cmd.new_direction;

  // Rule 1: Undo rule - if opposite of first queued direction, clear queue
  if (!filter_state.queue.empty()) {
    Direction first_queued = filter_state.queue.front();
    if (is_opposite(new_dir, first_queued)) {
      filter_state.queue.clear();
      // Continue to add the new direction below
    }
  }

  // Determine effective direction (last queued, or snake's current if queue empty)
  Direction effective_dir = filter_state.queue.empty() ? snake.current_direction : filter_state.queue.back();

  // Rule 2: Reject 180° turn from effective direction
  if (is_opposite(new_dir, effective_dir)) {
    return state;  // Reject, return unchanged
  }

  // Rule 3: Reject 0° turn (same as effective)
  if (new_dir == effective_dir) {
    return state;  // Reject, return unchanged
  }

  // Rule 4: Reject if queue is full
  if (filter_state.queue.size() >= DirectionCommandFilterState::MAX_QUEUE_SIZE) {
    return state;  // Reject, return unchanged
  }

  // Rule 5: Accept and add to queue
  filter_state.queue.push_back(new_dir);
  state[cmd.player_id] = filter_state;

  return state;
}

ConsumeResult try_consume_next(std::map<PlayerId, DirectionCommandFilterState> filters) {
  ConsumeResult result;
  result.filters = filters;

  // Consume one direction from each player's queue
  for (auto& [player_id, filter_state] : result.filters) {
    if (!filter_state.queue.empty()) {
      Direction consumed = filter_state.queue.front();
      filter_state.queue.pop_front();
      result.consumed_directions[player_id] = consumed;
    }
  }

  return result;
}

}  // namespace direction_command_filter
}  // namespace snake
