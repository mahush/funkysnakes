#include "snake/game_manager.hpp"

#include <iostream>

namespace snake {

GameManager::GameManager(asio::io_context& io,
                         TopicPtr<GameOver> gameover_topic,
                         TopicPtr<GameClockCommand> clock_topic,
                         TopicPtr<JoinRequest> joinrequest_topic,
                         TopicPtr<LeaveRequest> leaverequest_topic,
                         TopicPtr<StartGame> startgame_topic)
    : Actor(io),
      clock_pub_(create_pub(clock_topic)),
      gameover_sub_(create_sub(gameover_topic)),
      joinrequest_sub_(create_sub(joinrequest_topic)),
      leaverequest_sub_(create_sub(leaverequest_topic)),
      startgame_sub_(create_sub(startgame_topic)) {
}

void GameManager::processMessages() {
  // Game lifecycle
  while (auto msg = startgame_sub_->tryReceive()) {
    onStartGame(*msg);
  }
  while (auto msg = gameover_sub_->tryReceive()) {
    onGameOver(*msg);
  }

  // Player management
  while (auto msg = joinrequest_sub_->tryReceive()) {
    onJoinRequest(*msg);
  }
  while (auto msg = leaverequest_sub_->tryReceive()) {
    onLeaveRequest(*msg);
  }
}

void GameManager::onJoinRequest(const JoinRequest& msg) {
  std::cout << "[GameManager] Player '" << msg.player_id << "' joined\n";
  registered_players_.push_back(msg.player_id);
}

void GameManager::onLeaveRequest(const LeaveRequest& msg) {
  std::cout << "[GameManager] Player '" << msg.player_id << "' left\n";
  // Remove from registered players (simple implementation)
  auto it = std::find(registered_players_.begin(), registered_players_.end(), msg.player_id);
  if (it != registered_players_.end()) {
    registered_players_.erase(it);
  }
}

void GameManager::onStartGame(const StartGame& msg) {
  std::cout << "[GameManager] Starting game with level " << msg.starting_level << " and " << msg.players.size()
            << " players\n";

  current_game_id_ = "game_001";

  // Send START command to GameSession (will use default 200ms interval)
  GameClockCommand cmd;
  cmd.game_id = current_game_id_;
  cmd.state = GameClockState::START;
  clock_pub_->publish(cmd);
}

void GameManager::onGameOver(const GameOver& msg) {
  std::cout << "[GameManager] Game '" << msg.summary.game_id << "' ended at level " << msg.summary.final_level << "\n";
  std::cout << "[GameManager] Final scores:\n";
  for (const auto& [player_id, score] : msg.summary.final_scores) {
    std::cout << "[GameManager]   " << player_id << ": " << score << "\n";
  }

  // Send STOP command to GameSession
  GameClockCommand cmd;
  cmd.game_id = msg.summary.game_id;
  cmd.state = GameClockState::STOP;
  clock_pub_->publish(cmd);
}

}  // namespace snake
