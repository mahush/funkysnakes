#include "snake/game_manager.hpp"

#include "snake/logger.hpp"
#include "snake/process_helpers.hpp"

namespace snake {

GameManager::GameManager(Actor<GameManager>::ActorContext ctx, TopicPtr<GameClockCommand> clock_topic,
                         TopicPtr<StartGame> startgame_topic, TopicPtr<FoodRepositionTrigger> reposition_topic,
                         TopicPtr<LevelChange> level_topic, TopicPtr<TickRateChange> tickrate_topic,
                         TopicPtr<PlayerAliveStates> alivests_topic, TopicPtr<GameStateSummaryRequest> summary_req_topic,
                         TopicPtr<GameStateSummaryResponse> summary_resp_topic, TopicPtr<GameOver> gameover_topic,
                         TimerFactoryPtr timer_factory)
    : Actor(ctx),
      clock_pub_(create_pub(clock_topic)),
      reposition_pub_(create_pub(reposition_topic)),
      level_pub_(create_pub(level_topic)),
      tickrate_pub_(create_pub(tickrate_topic)),
      summary_req_pub_(create_pub(summary_req_topic)),
      gameover_pub_(create_pub(gameover_topic)),
      startgame_sub_(create_sub(startgame_topic)),
      alive_states_sub_(create_sub(alivests_topic)),
      summary_resp_sub_(create_sub(summary_resp_topic)),
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

  // Game state monitoring
  while (auto msg = alive_states_sub_->tryReceive()) {
    onPlayerAliveStates(*msg);
  }

  // Summary responses
  while (auto msg = summary_resp_sub_->tryReceive()) {
    onSummaryResponse(*msg);
  }
}

void GameManager::onStartGame(const StartGame& msg) {
  Logger::log("[GameManager] Starting game with level " + std::to_string(msg.starting_level) + " and " +
              std::to_string(msg.players.size()) + " players\n");

  current_game_id_ = "game_001";
  current_level_ = msg.starting_level;
  game_over_detected_ = false;

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

void GameManager::onPlayerAliveStates(const PlayerAliveStates& msg) {
  // Ignore if game already over or message is for wrong game
  if (game_over_detected_ || msg.game_id != current_game_id_) {
    return;
  }

  // Count alive players
  int alive_count = 0;
  for (const auto& [player_id, alive] : msg.alive_states) {
    if (alive) {
      alive_count++;
    }
  }

  // Game over condition: <= 1 player alive (death match mode)
  if (alive_count <= 1) {
    Logger::log("[GameManager] Game over condition detected: " + std::to_string(alive_count) + " player(s) alive\n");
    game_over_detected_ = true;

    // Request game state summary to build GameSummary
    GameStateSummaryRequest request;
    request.game_id = current_game_id_;
    summary_req_pub_->publish(request);
  }
}

void GameManager::onSummaryResponse(const GameStateSummaryResponse& response) {
  // Ignore if not expecting response or wrong game
  if (!game_over_detected_ || response.game_id != current_game_id_) {
    return;
  }

  Logger::log("[GameManager] Received game summary, publishing GameOver\n");

  // Build GameSummary
  GameSummary summary;
  summary.game_id = response.game_id;
  summary.final_level = response.level;
  for (const auto& [player_id, score] : response.scores) {
    summary.final_scores.push_back({player_id, score});
  }

  // Publish GameOver
  GameOver gameover;
  gameover.summary = summary;
  gameover_pub_->publish(gameover);

  // Stop timers
  reposition_timer_->execute_command(make_cancel_command<RepositionTimerTag>());
  level_timer_->execute_command(make_cancel_command<LevelTimerTag>());

  // Send STOP command to GameEngine
  GameClockCommand cmd;
  cmd.game_id = current_game_id_;
  cmd.state = GameClockState::STOP;
  clock_pub_->publish(cmd);

  Logger::log("[GameManager] Game '" + summary.game_id + "' ended at level " + std::to_string(summary.final_level) +
              "\n");
  Logger::log("[GameManager] Final scores:\n");
  for (const auto& [player_id, score] : summary.final_scores) {
    Logger::log("[GameManager]   " + player_id + ": " + std::to_string(score) + "\n");
  }
}

void GameManager::onRepositionTimer() {
  FoodRepositionTrigger trigger{current_game_id_};
  reposition_pub_->publish(trigger);
}

void GameManager::onLevelTimer() {
  // Increment level
  current_level_++;

  Logger::log("[GameManager] Level up! Now at level " + std::to_string(current_level_) + "\n");

  // Calculate new tick rate: faster with each level
  // Base interval: 200ms, reduce by 15ms per level, minimum 50ms
  constexpr int BASE_INTERVAL_MS = 200;
  constexpr int REDUCTION_PER_LEVEL_MS = 15;
  constexpr int MIN_INTERVAL_MS = 50;

  int new_interval_ms = std::max(MIN_INTERVAL_MS, BASE_INTERVAL_MS - ((current_level_ - 1) * REDUCTION_PER_LEVEL_MS));

  Logger::log("[GameManager] New tick interval: " + std::to_string(new_interval_ms) + "ms\n");

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
