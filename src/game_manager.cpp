#include "snake/game_manager.hpp"

#include <algorithm>
#include <iostream>

#include "snake/process_helpers.hpp"

namespace snake {

GameManager::GameManager(Actor<GameManager>::ActorContext ctx, TopicPtr<GameOver> gameover_topic,
                         TopicPtr<GameClockCommand> clock_topic, TopicPtr<JoinRequest> joinrequest_topic,
                         TopicPtr<LeaveRequest> leaverequest_topic, TopicPtr<StartGame> startgame_topic,
                         TopicPtr<FoodRepositionTrigger> reposition_topic, TopicPtr<LevelChange> level_topic,
                         TopicPtr<TickRateChange> tickrate_topic, TimerFactoryPtr timer_factory)
    : Actor(ctx),
      clock_pub_(create_pub(clock_topic)),
      reposition_pub_(create_pub(reposition_topic)),
      level_pub_(create_pub(level_topic)),
      tickrate_pub_(create_pub(tickrate_topic)),
      gameover_sub_(create_sub(gameover_topic)),
      joinrequest_sub_(create_sub(joinrequest_topic)),
      leaverequest_sub_(create_sub(leaverequest_topic)),
      startgame_sub_(create_sub(startgame_topic)),
      reposition_timer_(create_timer<RepositionTimer>(timer_factory)),
      level_timer_(create_timer<LevelTimer>(timer_factory)) {}

void GameManager::processMessages() {
  // Timer events
  process_event(reposition_timer_, [&](const RepositionTimerElapsedEvent&) { onRepositionTimer(); });
  process_event(level_timer_, [&](const LevelTimerElapsedEvent&) { onLevelTimer(); });

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
  current_level_ = msg.starting_level;

  // Send START command to GameEngine (will use default 200ms interval)
  GameClockCommand cmd;
  cmd.game_id = current_game_id_;
  cmd.state = GameClockState::START;
  clock_pub_->publish(cmd);

  // Start food reposition timer (every 5 seconds)
  reposition_timer_->execute_command(make_periodic_command<RepositionTimerTag>(std::chrono::seconds(5)));

  // Start level timer (every 60 seconds)
  level_timer_->execute_command(make_periodic_command<LevelTimerTag>(std::chrono::seconds(60)));
}

void GameManager::onGameOver(const GameOver& msg) {
  std::cout << "[GameManager] Game '" << msg.summary.game_id << "' ended at level " << msg.summary.final_level << "\n";
  std::cout << "[GameManager] Final scores:\n";
  for (const auto& [player_id, score] : msg.summary.final_scores) {
    std::cout << "[GameManager]   " << player_id << ": " << score << "\n";
  }

  // Stop timers
  reposition_timer_->execute_command(make_cancel_command<RepositionTimerTag>());
  level_timer_->execute_command(make_cancel_command<LevelTimerTag>());

  // Send STOP command to GameEngine
  GameClockCommand cmd;
  cmd.game_id = msg.summary.game_id;
  cmd.state = GameClockState::STOP;
  clock_pub_->publish(cmd);
}

void GameManager::onRepositionTimer() {
  FoodRepositionTrigger trigger{current_game_id_};
  reposition_pub_->publish(trigger);
}

void GameManager::onLevelTimer() {
  // Increment level
  current_level_++;

  std::cout << "[GameManager] Level up! Now at level " << current_level_ << "\n";

  // Calculate new tick rate: faster with each level
  // Base interval: 200ms, reduce by 15ms per level, minimum 50ms
  constexpr int BASE_INTERVAL_MS = 200;
  constexpr int REDUCTION_PER_LEVEL_MS = 15;
  constexpr int MIN_INTERVAL_MS = 50;

  int new_interval_ms = std::max(MIN_INTERVAL_MS, BASE_INTERVAL_MS - ((current_level_ - 1) * REDUCTION_PER_LEVEL_MS));

  std::cout << "[GameManager] New tick interval: " << new_interval_ms << "ms\n";

  // Publish level change for display (renderer, etc.)
  LevelChange level_change;
  level_change.game_id = current_game_id_;
  level_change.new_level = current_level_;
  level_pub_->publish(level_change);

  // Publish tick rate change to speed up the game
  TickRateChange tickrate_change;
  tickrate_change.game_id = current_game_id_;
  tickrate_change.interval_ms = new_interval_ms;
  tickrate_pub_->publish(tickrate_change);
}

}  // namespace snake
