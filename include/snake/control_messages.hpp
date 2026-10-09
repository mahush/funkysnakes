#pragma once

#include <optional>
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
 * @brief Request to quit the application
 */
struct QuitMsg {
  // Empty - just a signal to initiate shutdown
};

}  // namespace snake
