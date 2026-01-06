#pragma once

#include <map>
#include <optional>

#include "snake/game_messages.hpp"
#include "snake/snake_model.hpp"

namespace snake {

// Type alias for semantic clarity - DirectionChange represents a command
using DirectionCommand = DirectionChange;

namespace direction_command_filter {

// Maximum number of pending directions per player
constexpr size_t MAX_QUEUE_SIZE = 2;

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
 * @param pending_directions Pending direction queues per player
 * @param snakes Snakes per player (for current direction lookup)
 * @param cmd Direction command from player
 * @return Updated pending directions
 */
PerPlayerDirectionQueue try_add(PerPlayerDirectionQueue pending_directions,
                                const PerPlayerSnakes& snakes,
                                const DirectionCommand& cmd);

/**
 * @brief Consume next direction from each player's queue (one per player)
 *
 * @param pending_directions Pending direction queues per player
 * @return Tuple of (updated pending directions, consumed directions per player)
 */
std::tuple<PerPlayerDirectionQueue, PerPlayerDirection>
try_consume_next(PerPlayerDirectionQueue pending_directions);

}  // namespace direction_command_filter

}  // namespace snake
