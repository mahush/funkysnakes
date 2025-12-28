#pragma once

#include <map>
#include <optional>

#include "snake/game_messages.hpp"
#include "snake/snake_model.hpp"

namespace snake {

// Type alias for semantic clarity - DirectionChange represents a command
using DirectionCommand = DirectionChange;

/**
 * @brief Result of consuming next direction from all players
 */
struct ConsumeResult {
  std::map<PlayerId, DirectionCommandFilterState> filters;
  std::map<PlayerId, Direction> consumed_directions;
};

namespace direction_command_filter {

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
 * @param filters Map of player filter states
 * @param snakes Map of player snakes (for current direction lookup)
 * @param cmd Direction command from player
 * @return Updated filters map
 */
std::map<PlayerId, DirectionCommandFilterState> try_add(std::map<PlayerId, DirectionCommandFilterState> filters,
                                                         const std::map<PlayerId, Snake>& snakes,
                                                         const DirectionCommand& cmd);

/**
 * @brief Consume next direction from each player's queue (one per player)
 *
 * @param filters Map of player filter states
 * @return Updated filters and map of consumed directions per player
 */
ConsumeResult try_consume_next(std::map<PlayerId, DirectionCommandFilterState> filters);

}  // namespace direction_command_filter

}  // namespace snake
