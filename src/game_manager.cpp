#include "snake/game_manager.hpp"

#include <iostream>

namespace snake {

GameManager::GameManager(asio::io_context& io,
                         std::shared_ptr<Topic<Tick>> tick_topic,
                         std::shared_ptr<Topic<GameOver>> gameover_topic,
                         std::shared_ptr<Topic<StartClock>> startclock_topic,
                         std::shared_ptr<Topic<StopClock>> stopclock_topic,
                         std::shared_ptr<Topic<TickRateChange>> tickrate_topic,
                         std::shared_ptr<Topic<JoinRequest>> joinrequest_topic,
                         std::shared_ptr<Topic<LeaveRequest>> leaverequest_topic,
                         std::shared_ptr<Topic<StartGame>> startgame_topic)
    : Actor(io),
      tick_topic_(tick_topic),
      gameover_topic_(gameover_topic),
      startclock_topic_(startclock_topic),
      stopclock_topic_(stopclock_topic),
      tickrate_topic_(tickrate_topic),
      joinrequest_topic_(joinrequest_topic),
      leaverequest_topic_(leaverequest_topic),
      startgame_topic_(startgame_topic),
      timer_(io) {}

void GameManager::onEvent(JoinRequest msg) {
  std::cout << "[GameManager] Player '" << msg.player_id << "' joined\n";
  registered_players_.push_back(msg.player_id);
}

void GameManager::onEvent(LeaveRequest msg) {
  std::cout << "[GameManager] Player '" << msg.player_id << "' left\n";
  // Remove from registered players (simple implementation)
  auto it = std::find(registered_players_.begin(), registered_players_.end(), msg.player_id);
  if (it != registered_players_.end()) {
    registered_players_.erase(it);
  }
}

void GameManager::onEvent(StartGame msg) {
  std::cout << "[GameManager] Starting game with level " << msg.starting_level << " and " << msg.players.size()
            << " players\n";

  // Create StartSession message for GameSession
  StartSession session_msg;
  session_msg.game_id = "game_001";  // Simple ID for now
  session_msg.starting_level = msg.starting_level;
  session_msg.players = msg.players;

  // TODO: Send StartSession to GameSession (not yet implemented in interface)

  // Start the game timer
  current_game_id_ = session_msg.game_id;
  interval_ms_ = 500;  // 500ms per tick initially
  running_ = true;
  scheduleTick();
}

void GameManager::onEvent(GameOver msg) {
  std::cout << "[GameManager] Game '" << msg.summary.game_id << "' ended at level " << msg.summary.final_level << "\n";
  std::cout << "[GameManager] Final scores:\n";
  for (const auto& [player_id, score] : msg.summary.final_scores) {
    std::cout << "[GameManager]   " << player_id << ": " << score << "\n";
  }

  // Stop the timer
  running_ = false;
  timer_.cancel();
}

void GameManager::onEvent(StartClock msg) {
  std::cout << "[GameManager] Starting game timer with interval " << msg.interval_ms << "ms\n";
  current_game_id_ = msg.game_id;
  interval_ms_ = msg.interval_ms;
  running_ = true;
  scheduleTick();
}

void GameManager::onEvent(StopClock msg) {
  std::cout << "[GameManager] Stopping game timer for '" << msg.game_id << "'\n";
  running_ = false;
  timer_.cancel();
}

void GameManager::onEvent(TickRateChange msg) {
  std::cout << "[GameManager] Changing tick rate to " << msg.interval_ms << "ms\n";
  interval_ms_ = msg.interval_ms;
  // New interval will take effect on next tick
}

void GameManager::scheduleTick() {
  if (!running_) {
    return;
  }

  timer_.expires_after(std::chrono::milliseconds(interval_ms_));
  timer_.async_wait(asio::bind_executor(strand_, [weak_self = weak_from_this()](const asio::error_code& ec) {
    if (auto self = weak_self.lock()) {
      self->onTimerExpired(ec);
    }
  }));
}

void GameManager::onTimerExpired(const asio::error_code& ec) {
  if (ec == asio::error::operation_aborted) {
    // Timer was stopped
    return;
  }

  if (ec) {
    std::cerr << "[GameManager] Timer error: " << ec.message() << "\n";
    return;
  }

  // Publish tick to topic
  Tick tick;
  tick.game_id = current_game_id_;
  tick_topic_->publish(tick);

  // Schedule next tick
  scheduleTick();
}

}  // namespace snake
