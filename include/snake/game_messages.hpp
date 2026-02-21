#pragma once

#include "snake/control_messages.hpp"
#include "snake/direction_command_filter.hpp"
#include "snake/game_types.hpp"

namespace snake {

/**
 * @brief Complete game state snapshot
 *
 * Contains internal game state for GameEngineActor.
 * Does not include level as that is managed by GameManager.
 */
struct GameState {
  GameId game_id;
  PerPlayerSnakes snakes;                                          // Snakes for each player
  PerPlayerScores scores;                                          // Scores for each player
  FoodItems food_items;                                            // Food items on the board
  direction_command_filter::State direction_command_filter_state;  // Direction command filter module state
  Board board;                                                     // Board dimensions
  CollisionMode collision_mode{CollisionMode::BITE_DROP_FOOD};     // Collision handling mode
  int interval_ms{200};                                            // TickMsg interval in milliseconds
  bool should_reposition_food{false};                              // Flag: reposition food this tick
  PerPlayerAliveStates previous_alive_states;                      // Previous alive states for change detection
};

/**
 * @brief Game tick event - drives the game loop
 */
struct TickMsg {
  GameId game_id;
};

/**
 * @brief Player direction change command
 */
struct DirectionMsg {
  GameId game_id;
  PlayerId player_id;
  Direction new_direction;
};

/**
 * @brief Renderable game state - visual projection for rendering
 *
 * Contains only the visual data needed for rendering game elements.
 * Game metadata (level, pause state, game_id) is provided separately
 * by GameManager via GameStateMetadataMsg.
 */
struct RenderableStateMsg {
  Board board;
  FoodItems food_items;
  PerPlayerSnakes snakes;
  PerPlayerScores scores;
};

/**
 * @brief Game state metadata - game lifecycle information
 *
 * Sent by GameManager to communicate game metadata that changes during gameplay.
 * Includes level, pause state, and game identifier.
 * This is separate from RenderableStateMsg which contains only visual game elements.
 */
struct GameStateMetadataMsg {
  GameId game_id;
  int level;
  bool paused;
};

/**
 * @brief Game over notification
 */
struct GameOverMsg {
  GameSummaryMsg summary;
};

/**
 * @brief TickMsg rate change command
 */
struct TickRateChangeMsg {
  GameId game_id;
  int interval_ms;  // New tick interval in milliseconds
};

/**
 * @brief Food reposition trigger
 *
 * Signals that food items should be repositioned this tick.
 * Sent by GameManager based on its scheduling logic.
 */
struct FoodRepositionTriggerMsg {
  GameId game_id;
};

/**
 * @brief Start clock command
 */
struct StartClockMsg {
  GameId game_id;
  int interval_ms;
};

/**
 * @brief Stop clock command
 */
struct StopClockMsg {
  GameId game_id;
};

/**
 * @brief User input event (from keyboard/controller)
 */
struct UserInputEventMsg {
  PlayerId player_id;
  char key;  // For now, simple char input
};

/**
 * @brief Log message effect - represents a message to be logged
 *
 * Used in effect handler pattern to keep logging logic pure.
 * Handler functions return this effect instead of calling std::cout directly.
 */
struct LogMsg {
  std::string message;
};

/**
 * @brief Player alive states - published when any player's alive state changes
 *
 * This message is sent only when at least one player's alive state changes.
 * Used by GameManager to detect game over conditions.
 */
struct PlayerAliveStatesMsg {
  GameId game_id;
  PerPlayerAliveStates alive_states;
};

/**
 * @brief Request current game state summary
 *
 * Sent by GameManager when it needs complete game state (e.g., on game over).
 */
struct GameStateSummaryRequestMsg {
  GameId game_id;
};

/**
 * @brief Response with current game state summary
 *
 * Sent by GameEngineActor in response to GameStateSummaryRequestMsg.
 * Contains game state data that GameEngineActor manages (scores, alive states).
 * Does not include level or game_id as those are managed by GameManager.
 */
struct GameStateSummaryResponseMsg {
  PerPlayerScores scores;
  PerPlayerAliveStates alive_states;
};

}  // namespace snake
