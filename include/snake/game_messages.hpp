#pragma once

#include "classic/classic_arena_authority.hpp"
#include "classic/difficulty_policy.hpp"
#include "classic/game_boundary.hpp"
#include "snake/control_messages.hpp"
#include "snake/game_types.hpp"

namespace snake {

/**
 * @brief GameEngineActor state
 *
 * Holds the arena (owned by the Classic Arena Authority) together with the
 * actor's own realization state: game routing, tick interval and the last
 * published alive states. Does not include level as that is managed by GameManager.
 */
struct GameState {
  GameId game_id;
  classic_arena_authority::State arena;                   // Arena state owned by the Classic Arena Authority
  int interval_ms{difficulty_policy::stepIntervalMs(1)};  // Step interval in milliseconds, initially for level 1
  PerPlayerAliveStates previous_alive_states;             // Previous alive states for change detection
};

/**
 * @brief Request to start a new game
 */
struct StartGameMsg {
  game_boundary::Start start;  // Boundary interaction
};

/**
 * @brief Request to toggle pause state
 */
struct PauseToggleMsg {
  GameId game_id;                     // Routing
  game_boundary::TogglePause toggle;  // Boundary interaction
};

/**
 * @brief Player direction change command
 */
struct DirectionMsg {
  GameId game_id;              // Routing
  game_boundary::Steer steer;  // Boundary interaction
};

/**
 * @brief Renderable game state - visual projection for rendering
 *
 * Contains only the visual data needed for rendering game elements.
 * Game metadata (level, pause state, game_id) is provided separately
 * by GameManager via GameStateMetadataMsg.
 */
struct RenderableStateMsg {
  game_boundary::ArenaView view;  // Boundary observation
};

/**
 * @brief Game state metadata - game lifecycle information
 *
 * Sent by GameManager to communicate game metadata that changes during gameplay.
 * Includes level, pause state, and game identifier.
 * This is separate from RenderableStateMsg which contains only visual game elements.
 */
struct GameStateMetadataMsg {
  GameId game_id;                // Routing
  game_boundary::Status status;  // Boundary observation
};

/**
 * @brief Game over notification
 */
struct GameOverMsg {
  GameId game_id;                     // Routing
  game_boundary::GameOver game_over;  // Boundary observation
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

}  // namespace snake
