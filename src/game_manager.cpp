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
      tick_pub_(create_pub(tick_topic)),
      startclock_pub_(create_pub(startclock_topic)),
      gameover_sub_(create_sub(gameover_topic)),
      startclock_sub_(create_sub(startclock_topic)),
      stopclock_sub_(create_sub(stopclock_topic)),
      tickrate_sub_(create_sub(tickrate_topic)),
      joinrequest_sub_(create_sub(joinrequest_topic)),
      leaverequest_sub_(create_sub(leaverequest_topic)),
      startgame_sub_(create_sub(startgame_topic)),
      timer_(io) {
}

void GameManager::processMessages() {
  // Process messages in priority order
  // High priority: Clock control
  while (auto msg = startclock_sub_->tryReceive()) {
    onStartClock(*msg);
  }
  while (auto msg = stopclock_sub_->tryReceive()) {
    onStopClock(*msg);
  }
  while (auto msg = tickrate_sub_->tryReceive()) {
    onTickRateChange(*msg);
  }

  // Medium priority: Game lifecycle
  while (auto msg = startgame_sub_->tryReceive()) {
    onStartGame(*msg);
  }
  while (auto msg = gameover_sub_->tryReceive()) {
    onGameOver(*msg);
  }

  // Low priority: Player management
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
  interval_ms_ = 200;  // 200ms per tick for responsive gameplay

  // Send StartClock to GameSession to begin the game
  StartClock start_clock;
  start_clock.game_id = current_game_id_;
  start_clock.interval_ms = interval_ms_;
  startclock_pub_->publish(start_clock);

  // Start the timer for ticks
  running_ = true;
  scheduleTick();
}

void GameManager::onGameOver(const GameOver& msg) {
  std::cout << "[GameManager] Game '" << msg.summary.game_id << "' ended at level " << msg.summary.final_level << "\n";
  std::cout << "[GameManager] Final scores:\n";
  for (const auto& [player_id, score] : msg.summary.final_scores) {
    std::cout << "[GameManager]   " << player_id << ": " << score << "\n";
  }

  // Stop the timer
  running_ = false;
  timer_.cancel();
}

void GameManager::onStartClock(const StartClock& msg) {
  std::cout << "[GameManager] Starting game timer with interval " << msg.interval_ms << "ms\n";
  current_game_id_ = msg.game_id;
  interval_ms_ = msg.interval_ms;
  running_ = true;
  scheduleTick();
}

void GameManager::onStopClock(const StopClock& msg) {
  std::cout << "[GameManager] Stopping game timer for '" << msg.game_id << "'\n";
  running_ = false;
  timer_.cancel();
}

void GameManager::onTickRateChange(const TickRateChange& msg) {
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
  tick_pub_->publish(tick);

  // Schedule next tick
  scheduleTick();
}

}  // namespace snake
