#pragma once

#include <optional>

#include "common/game_types.hpp"
#include "snake/control_messages.hpp"

namespace snake {

// ============================================================================
// Messages between GameManagerActor and GameEngineActor
// ============================================================================
// These are realization details of the two-actor topology, not part of the Domain System
// Boundary (see game_boundary.hpp). They carry the lifecycle Authority's intents to the
// engine and the arena's views back to the manager.

/**
 * @brief Game clock state for controlling timer
 */
enum class GameClockState {
  START,  // Start running the clock
  STOP,   // Stop and reset
  PAUSE,  // Pause (can resume)
  RESUME  // Resume from pause
};

/**
 * @brief Unified game clock control command
 *
 * START carries the initial step interval; later changes use TickRateChangeMsg.
 */
struct GameClockCommandMsg {
  GameId game_id;
  GameClockState state;
  std::optional<int> interval_ms;  // Step interval to start with (START only)
};

/**
 * @brief Step interval change command
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
