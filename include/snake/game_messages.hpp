#pragma once

#include "snake/control_messages.hpp"
#include "snake/game_types.hpp"

namespace snake {

/**
 * @brief Complete game state snapshot
 */
struct GameState {
  GameId game_id;
  PerPlayerSnakes snakes;                                       // Snakes for each player
  PerPlayerScores scores;                                       // Scores for each player
  FoodItems food_items;                                         // Food items on the board
  PerPlayerDirectionQueue pending_directions;                   // Pending direction commands per player
  Board board;                                                  // Board dimensions
  CollisionMode collision_mode{CollisionMode::BITE_DROP_FOOD};  // Collision handling mode
  int level{1};
};

/**
 * @brief Game tick event - drives the game loop
 */
struct Tick {
  GameId game_id;
};

/**
 * @brief Player direction change command
 */
struct DirectionChange {
  GameId game_id;
  PlayerId player_id;
  Direction new_direction;
};

/**
 * @brief Game state update notification
 */
struct StateUpdate {
  GameState state;
};

/**
 * @brief Level change notification
 */
struct LevelChange {
  GameId game_id;
  int new_level;
};

/**
 * @brief Game over notification
 */
struct GameOver {
  GameSummary summary;
};

/**
 * @brief Tick rate change command
 */
struct TickRateChange {
  GameId game_id;
  int interval_ms;  // New tick interval in milliseconds
};

/**
 * @brief Start clock command
 */
struct StartClock {
  GameId game_id;
  int interval_ms;
};

/**
 * @brief Stop clock command
 */
struct StopClock {
  GameId game_id;
};

/**
 * @brief User input event (from keyboard/controller)
 */
struct UserInputEvent {
  PlayerId player_id;
  char key;  // For now, simple char input
};

}  // namespace snake
