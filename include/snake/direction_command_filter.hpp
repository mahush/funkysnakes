#pragma once

#include <deque>
#include <map>
#include <optional>

#include "snake/game_types.hpp"

namespace snake {

// Forward declaration
struct DirectionChange;

// Type alias for semantic clarity - DirectionChange represents a command
using DirectionCommand = DirectionChange;

namespace direction_command_filter {

// Maximum number of pending directions per player
constexpr size_t MAX_QUEUE_SIZE = 2;

// Internal type for per-player direction queues
using PerPlayerDirectionQueue = std::map<PlayerId, std::deque<Direction>>;

/**
 * @brief Module state for direction command filtering
 *
 * Encapsulates the internal state of the direction command filter module.
 * Domain code should treat this as an opaque type.
 */
struct State {
  PerPlayerDirectionQueue queues;
};

/**
 * @brief Check if two directions are opposite (180° turn)
 */
bool is_opposite(Direction a, Direction b);

/**
 * @brief Add incoming direction command with filtering logic
 *
 * Filtering rules:
 * 1. If opposite of first queued direction → clear queue (player changed mind)
 * 2. If 180° turn from effective direction → reject
 * 3. If 0° turn (same as effective) → reject
 * 4. If queue full → reject
 * 5. Otherwise → accept and add to queue
 *
 * @param state Module state containing direction queues
 * @param snakes Snakes per player (for current direction lookup)
 * @param cmd Direction command from player
 * @return Updated state
 */
State try_add(State state, const PerPlayerSnakes& snakes, const DirectionCommand& cmd);

/**
 * @brief Consume next direction from each player's queue (one per player)
 *
 * @param state Module state containing direction queues
 * @return Tuple of (updated state, consumed directions per player)
 */
std::tuple<State, PerPlayerDirection> try_consume_next(State state);

}  // namespace direction_command_filter

}  // namespace snake
