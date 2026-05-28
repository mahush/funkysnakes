#ifndef SNAKE_GAME_TYPES_HPP
#define SNAKE_GAME_TYPES_HPP

#include <map>
#include <vector>

#include "snake/control_messages.hpp"
#include "snake/game_primitives.hpp"
#include "snake/snake_model.hpp"

namespace snake {

/**
 * @brief Collision handling mode
 *
 * Determines what happens when snakes collide.
 */
enum class CollisionMode {
  BITE_REMOVE_TAIL,  // Cut tail is simply removed
  BITE_DROP_FOOD     // Cut tail segments become food items
};

// Domain type alias for snake_model::Snake
using Snake = snake_model::Snake;

// Type aliases for per-player game data
using PerPlayerSnakes = std::map<PlayerId, Snake>;
using PerPlayerScores = std::map<PlayerId, int>;
using PerPlayerDirection = std::map<PlayerId, Direction>;
using PerPlayerAliveStates = std::map<PlayerId, bool>;

// Type alias for food items
using FoodItems = std::vector<Point>;

}  // namespace snake

#endif  // SNAKE_GAME_TYPES_HPP
