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

/**
 * @brief Request to join a game
 */
struct JoinRequest {
  PlayerId player_id;
};

/**
 * @brief Request to leave a game
 */
struct LeaveRequest {
  PlayerId player_id;
};

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
  int board_width{30};
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

}  // namespace snake
