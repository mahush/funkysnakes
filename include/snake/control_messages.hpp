#pragma once

#include <string>
#include <vector>

namespace snake {

/**
 * @brief Player identifier type
 */
using PlayerId = std::string;

/**
 * @brief Game identifier type
 */
using GameId = std::string;

// ============================================================================
// Single source of truth for player identities
// ============================================================================
// For the simplified 2-player game, we hardcode player IDs here.
// All other code references these constants to avoid duplication.
inline constexpr const char* PLAYER_A = "Player A";
inline constexpr const char* PLAYER_B = "Player B";

/**
 * @brief Request to start a new game
 */
struct StartGame {
  int starting_level{1};
  std::vector<PlayerId> players;
};

/**
 * @brief Request to start a game session
 */
struct StartSession {
  GameId game_id;
  int starting_level;
  std::vector<PlayerId> players;
  int board_width{60};
  int board_height{20};
};

/**
 * @brief Summary of a completed game
 */
struct GameSummary {
  GameId game_id;
  std::vector<std::pair<PlayerId, int>> final_scores;  // player_id, score
  int final_level;
};

/**
 * @brief Request to end a game session
 */
struct EndSession {
  GameId game_id;
  GameSummary summary;
};

/**
 * @brief Request to pause the game
 */
struct PauseGame {
  GameId game_id;
};

/**
 * @brief Request to resume the game
 */
struct ResumeGame {
  GameId game_id;
};

/**
 * @brief Game clock state for controlling timer
 */
enum class GameClockState {
  START,   // Start running the clock
  STOP,    // Stop and reset
  PAUSE,   // Pause (can resume)
  RESUME   // Resume from pause
};

/**
 * @brief Unified game clock control command
 *
 * Replaces StartClock, StopClock, PauseGame, ResumeGame with single message.
 * Tick interval is controlled separately via TickRateChange message.
 */
struct GameClockCommand {
  GameId game_id;
  GameClockState state;
};

}  // namespace snake
