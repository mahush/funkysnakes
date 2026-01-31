#pragma once

#include "snake/control_messages.hpp"
#include "snake/direction_command_filter.hpp"
#include "snake/game_types.hpp"

namespace snake {

/**
 * @brief Complete game state snapshot
 */
struct GameState {
  GameId game_id;
  PerPlayerSnakes snakes;                                          // Snakes for each player
  PerPlayerScores scores;                                          // Scores for each player
  FoodItems food_items;                                            // Food items on the board
  direction_command_filter::State direction_command_filter_state;  // Direction command filter module state
  Board board;                                                     // Board dimensions
  CollisionMode collision_mode{CollisionMode::BITE_DROP_FOOD};    // Collision handling mode
  int level{1};
  int interval_ms{200};                                            // Tick interval in milliseconds
  bool should_reposition_food{false};                              // Flag: reposition food this tick
  PerPlayerAliveStates previous_alive_states;                      // Previous alive states for change detection
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
 * @brief Renderable game state - visual projection for rendering
 *
 * Contains only the data needed for rendering, avoiding transfer
 * of internal engine state like direction filters or timers.
 */
struct RenderableState {
  GameId game_id;
  int level;
  Board board;
  FoodItems food_items;
  PerPlayerSnakes snakes;
  PerPlayerScores scores;
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
 * @brief Food reposition trigger
 *
 * Signals that food items should be repositioned this tick.
 * Sent by GameManager based on its scheduling logic.
 */
struct FoodRepositionTrigger {
  GameId game_id;
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

/**
 * @brief Log message effect - represents a message to be logged
 *
 * Used in effect handler pattern to keep logging logic pure.
 * Handler functions return this effect instead of calling std::cout directly.
 */
struct LogMessage {
  std::string message;
};

/**
 * @brief Player alive states - published when any player's alive state changes
 *
 * This message is sent only when at least one player's alive state changes.
 * Used by GameManager to detect game over conditions.
 */
struct PlayerAliveStates {
  GameId game_id;
  PerPlayerAliveStates alive_states;
};

/**
 * @brief Request current game state summary
 *
 * Sent by GameManager when it needs complete game state (e.g., on game over).
 */
struct GameStateSummaryRequest {
  GameId game_id;
};

/**
 * @brief Response with current game state summary
 *
 * Sent by GameEngine in response to GameStateSummaryRequest.
 * Contains complete current state for building GameSummary.
 */
struct GameStateSummaryResponse {
  GameId game_id;
  int level;
  PerPlayerScores scores;
  PerPlayerAliveStates alive_states;
};

}  // namespace snake
