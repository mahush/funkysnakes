#include "snake/game_manager_actor.hpp"

#include "snake/logger.hpp"
#include "snake/process_helpers.hpp"

namespace snake {

GameManagerActor::GameManagerActor(
    ActorContext ctx, TopicPtr<GameClockCommandMsg> clock_topic, TopicPtr<StartGameMsg> startgame_topic,
    TopicPtr<FoodRepositionTriggerMsg> reposition_topic, TopicPtr<GameStateMetadataMsg> metadata_topic,
    TopicPtr<TickRateChangeMsg> tickrate_topic, TopicPtr<PlayerAliveStatesMsg> alivests_topic,
    TopicPtr<GameStateSummaryRequestMsg> summary_req_topic, TopicPtr<GameStateSummaryResponseMsg> summary_resp_topic,
    TopicPtr<GameOverMsg> gameover_topic, TopicPtr<PauseToggleMsg> pause_topic, TimerFactoryPtr timer_factory)
    : Actor(ctx),
      clock_pub_(create_pub(clock_topic)),
      reposition_pub_(create_pub(reposition_topic)),
      metadata_pub_(create_pub(metadata_topic)),
      tickrate_pub_(create_pub(tickrate_topic)),
      summary_req_pub_(create_pub(summary_req_topic)),
      gameover_pub_(create_pub(gameover_topic)),
      startgame_sub_(create_sub(startgame_topic)),
      alive_states_sub_(create_sub(alivests_topic)),
      summary_resp_sub_(create_sub(summary_resp_topic)),
      pause_sub_(create_sub(pause_topic)),
      reposition_timer_(create_timer<RepositionTimer>(timer_factory)),
      level_timer_(create_timer<LevelTimer>(timer_factory)) {}

void GameManagerActor::processInputs() {
  // Timer events
  process_event(reposition_timer_, [&](const RepositionTimerElapsedEvent&) { onRepositionTimer(); });
  process_event(level_timer_, [&](const LevelTimerElapsedEvent&) { onLevelTimer(); });

  // Game lifecycle
  while (auto msg = startgame_sub_->tryTakeMessage()) {
    onStartGame(*msg);
  }

  // Game state monitoring
  while (auto msg = alive_states_sub_->tryTakeMessage()) {
    onPlayerAliveStates(*msg);
  }

  // Summary responses
  while (auto msg = summary_resp_sub_->tryTakeMessage()) {
    onSummaryResponse(*msg);
  }

  // Pause toggle
  while (auto msg = pause_sub_->tryTakeMessage()) {
    onPauseToggle(*msg);
  }
}

void GameManagerActor::publishMetadata() {
  GameStateMetadataMsg metadata;
  metadata.game_id = current_game_id_;
  metadata.level = current_level_;
  metadata.paused = paused_;
  metadata_pub_->publish(metadata);
}

void GameManagerActor::onStartGame(const StartGameMsg& msg) {
  Logger::log("[GameManagerActor] Starting game with level " + std::to_string(msg.starting_level) + " and " +
              std::to_string(msg.players.size()) + " players\n");

  current_game_id_ = "game_001";
  current_level_ = msg.starting_level;
  game_over_detected_ = false;
  paused_ = false;

  // Send START command to GameEngineActor (will use default 200ms interval)
  GameClockCommandMsg cmd;
  cmd.game_id = current_game_id_;
  cmd.state = GameClockState::START;
  clock_pub_->publish(cmd);

  // Publish initial metadata
  publishMetadata();

  // Start food reposition timer (every 5 seconds)
  reposition_timer_->execute_command(make_periodic_command<RepositionTimerTag>(std::chrono::seconds(5)));

  // Start level timer (every 60 seconds)
  level_timer_->execute_command(make_periodic_command<LevelTimerTag>(std::chrono::seconds(60)));
}

void GameManagerActor::onPlayerAliveStates(const PlayerAliveStatesMsg& msg) {
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

  // Game over condition: 0 players alive (last snake standing wins)
  if (alive_count == 0) {
    Logger::log("[GameManagerActor] Game over condition detected: all snakes dead\n");
    game_over_detected_ = true;

    // Request game state summary to build GameSummaryMsg
    GameStateSummaryRequestMsg request;
    request.game_id = current_game_id_;
    summary_req_pub_->publish(request);
  }
}

void GameManagerActor::onSummaryResponse(const GameStateSummaryResponseMsg& response) {
  // Ignore if not expecting response
  if (!game_over_detected_) {
    return;
  }

  Logger::log("[GameManagerActor] Received game summary, publishing GameOverMsg\n");

  // Build GameSummaryMsg using GameManagerActor's own state for level/game_id
  GameSummaryMsg summary;
  summary.game_id = current_game_id_;
  summary.final_level = current_level_;
  for (const auto& [player_id, score] : response.scores) {
    summary.final_scores.push_back({player_id, score});
  }

  // Publish GameOverMsg
  GameOverMsg gameover;
  gameover.summary = summary;
  gameover_pub_->publish(gameover);

  // Stop timers
  reposition_timer_->execute_command(make_cancel_command<RepositionTimerTag>());
  level_timer_->execute_command(make_cancel_command<LevelTimerTag>());

  // Send STOP command to GameEngineActor
  GameClockCommandMsg cmd;
  cmd.game_id = current_game_id_;
  cmd.state = GameClockState::STOP;
  clock_pub_->publish(cmd);

  Logger::log("[GameManagerActor] Game '" + summary.game_id + "' ended at level " +
              std::to_string(summary.final_level) + "\n");
  Logger::log("[GameManagerActor] Final scores:\n");
  for (const auto& [player_id, score] : summary.final_scores) {
    Logger::log("[GameManagerActor]   " + player_id + ": " + std::to_string(score) + "\n");
  }
}

void GameManagerActor::onRepositionTimer() {
  // Don't reposition food while paused
  if (paused_) {
    return;
  }

  FoodRepositionTriggerMsg trigger{current_game_id_};
  reposition_pub_->publish(trigger);
}

void GameManagerActor::onLevelTimer() {
  // Don't level up while paused
  if (paused_) {
    return;
  }

  // Increment level
  current_level_++;

  Logger::log("[GameManagerActor] Level up! Now at level " + std::to_string(current_level_) + "\n");

  // Calculate new tick rate: faster with each level
  // Base interval: 200ms, reduce by 15ms per level, minimum 50ms
  constexpr int BASE_INTERVAL_MS = 200;
  constexpr int REDUCTION_PER_LEVEL_MS = 15;
  constexpr int MIN_INTERVAL_MS = 50;

  int new_interval_ms = std::max(MIN_INTERVAL_MS, BASE_INTERVAL_MS - ((current_level_ - 1) * REDUCTION_PER_LEVEL_MS));

  Logger::log("[GameManagerActor] New tick interval: " + std::to_string(new_interval_ms) + "ms\n");

  // Publish updated metadata (level changed)
  publishMetadata();

  // Publish tick rate change to speed up the game
  TickRateChangeMsg tickrate_change;
  tickrate_change.game_id = current_game_id_;
  tickrate_change.interval_ms = new_interval_ms;
  tickrate_pub_->publish(tickrate_change);
}

void GameManagerActor::onPauseToggle(const PauseToggleMsg& msg) {
  // Ignore if message is for wrong game
  if (msg.game_id != current_game_id_) {
    return;
  }

  // Toggle pause state
  paused_ = !paused_;

  Logger::log("[GameManagerActor] Game " + std::string(paused_ ? "PAUSED" : "RESUMED") + "\n");

  // Send clock command to GameEngineActor
  GameClockCommandMsg cmd;
  cmd.game_id = current_game_id_;
  cmd.state = paused_ ? GameClockState::PAUSE : GameClockState::RESUME;
  clock_pub_->publish(cmd);

  // Publish updated metadata (pause state changed)
  publishMetadata();
}

}  // namespace snake
