#include "snake/game_manager_actor.hpp"

#include "snake/logger.hpp"
#include "snake/process_helpers.hpp"

namespace snake {

GameManagerActor::GameManagerActor(ActorContext ctx,
                                   TopicPtr<GameClockCommandMsg> clock_topic,
                                   TopicPtr<StartGameMsg> startgame_topic,
                                   TopicPtr<FoodRepositionTriggerMsg> reposition_topic,
                                   TopicPtr<GameStateMetadataMsg> metadata_topic,
                                   TopicPtr<TickRateChangeMsg> tickrate_topic,
                                   TopicPtr<PlayerAliveStatesMsg> alivests_topic,
                                   TopicPtr<GameStateSummaryRequestMsg> summary_req_topic,
                                   TopicPtr<GameStateSummaryResponseMsg> summary_resp_topic,
                                   TopicPtr<GameOverMsg> gameover_topic,
                                   TopicPtr<PauseToggleMsg> pause_topic,
                                   TimerFactoryPtr timer_factory)
    : Actor{ctx},
      clock_pub_{create_pub(clock_topic)},
      reposition_pub_{create_pub(reposition_topic)},
      metadata_pub_{create_pub(metadata_topic)},
      tickrate_pub_{create_pub(tickrate_topic)},
      summary_req_pub_{create_pub(summary_req_topic)},
      gameover_pub_{create_pub(gameover_topic)},
      startgame_sub_{create_sub(startgame_topic)},
      alive_states_sub_{create_sub(alivests_topic)},
      summary_resp_sub_{create_sub(summary_resp_topic)},
      pause_sub_{create_sub(pause_topic)},
      reposition_timer_{create_timer<RepositionTimer>(timer_factory)},
      level_timer_{create_timer<LevelTimer>(timer_factory)} {}

void GameManagerActor::processInputs() {
  // Timer events
  processEvent(reposition_timer_, [&](const RepositionTimerElapsedEvent&) { onRepositionTimer(); });
  processEvent(level_timer_, [&](const LevelTimerElapsedEvent&) { onLevelTimer(); });

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

namespace lifecycle = classic_game_lifecycle_authority;

void GameManagerActor::publishMetadata() {
  GameStateMetadataMsg metadata;
  metadata.game_id = lifecycle_.game_id;
  metadata.level = lifecycle_.level;
  metadata.paused = lifecycle_.paused;
  metadata_pub_->publish(metadata);
}

void GameManagerActor::executeClockIntent(lifecycle::ClockIntent clock) {
  GameClockCommandMsg cmd;
  cmd.game_id = lifecycle_.game_id;
  cmd.state = clock;
  clock_pub_->publish(cmd);
}

void GameManagerActor::executeCadenceIntent(lifecycle::CadenceIntent cadence) {
  if (cadence == lifecycle::CadenceIntent::START) {
    reposition_timer_->execute_command(make_periodic_command<RepositionTimerTag>(lifecycle::REPOSITION_PERIOD));
    level_timer_->execute_command(make_periodic_command<LevelTimerTag>(lifecycle::LEVEL_PERIOD));
  } else {
    reposition_timer_->execute_command(make_cancel_command<RepositionTimerTag>());
    level_timer_->execute_command(make_cancel_command<LevelTimerTag>());
  }
}

void GameManagerActor::onStartGame(const StartGameMsg& msg) {
  Logger::log("[GameManagerActor] Starting game with level " + std::to_string(msg.starting_level) + " and " +
              std::to_string(msg.players.size()) + " players\n");

  auto [state, clock, cadence] = lifecycle::start("game_001", msg.starting_level, lifecycle_);
  lifecycle_ = state;

  executeClockIntent(clock);
  publishMetadata();
  executeCadenceIntent(cadence);
}

void GameManagerActor::onPlayerAliveStates(const PlayerAliveStatesMsg& msg) {
  // Ignore messages for another game
  if (msg.game_id != lifecycle_.game_id) {
    return;
  }

  auto [state, conclude] = lifecycle::observeAliveStates(lifecycle_, msg.alive_states);
  lifecycle_ = state;

  if (conclude) {
    Logger::log("[GameManagerActor] Game over condition detected: all snakes dead\n");

    // Request game state summary to build GameSummaryMsg
    GameStateSummaryRequestMsg request;
    request.game_id = lifecycle_.game_id;
    summary_req_pub_->publish(request);
  }
}

void GameManagerActor::onSummaryResponse(const GameStateSummaryResponseMsg& response) {
  // Ignore if not expecting response
  if (!lifecycle_.over) {
    return;
  }

  Logger::log("[GameManagerActor] Received game summary, publishing GameOverMsg\n");

  GameSummaryMsg summary;
  summary.game_id = lifecycle_.game_id;
  summary.final_level = lifecycle_.level;
  for (const auto& [player_id, score] : response.scores) {
    summary.final_scores.push_back({player_id, score});
  }

  GameOverMsg gameover;
  gameover.summary = summary;
  gameover_pub_->publish(gameover);

  auto [clock, cadence] = lifecycle::concluded(lifecycle_);
  executeCadenceIntent(cadence);
  executeClockIntent(clock);

  Logger::log("[GameManagerActor] Game '" + summary.game_id + "' ended at level " +
              std::to_string(summary.final_level) + "\n");
  Logger::log("[GameManagerActor] Final scores:\n");
  for (const auto& [player_id, score] : summary.final_scores) {
    Logger::log("[GameManagerActor]   " + player_id + ": " + std::to_string(score) + "\n");
  }
}

void GameManagerActor::onRepositionTimer() {
  if (lifecycle::repositionPeriodElapsed(lifecycle_)) {
    FoodRepositionTriggerMsg trigger{lifecycle_.game_id};
    reposition_pub_->publish(trigger);
  }
}

void GameManagerActor::onLevelTimer() {
  auto [state, interval] = lifecycle::levelPeriodElapsed(lifecycle_);
  lifecycle_ = state;

  if (!interval) {
    return;
  }

  Logger::log("[GameManagerActor] Level up! Now at level " + std::to_string(lifecycle_.level) + "\n");
  Logger::log("[GameManagerActor] New tick interval: " + std::to_string(interval->interval_ms) + "ms\n");

  publishMetadata();

  TickRateChangeMsg tickrate_change;
  tickrate_change.game_id = lifecycle_.game_id;
  tickrate_change.interval_ms = interval->interval_ms;
  tickrate_pub_->publish(tickrate_change);
}

void GameManagerActor::onPauseToggle(const PauseToggleMsg& msg) {
  // Ignore messages for another game
  if (msg.game_id != lifecycle_.game_id) {
    return;
  }

  auto [state, clock] = lifecycle::togglePause(lifecycle_);
  lifecycle_ = state;

  Logger::log("[GameManagerActor] Game " + std::string(lifecycle_.paused ? "PAUSED" : "RESUMED") + "\n");

  executeClockIntent(clock);
  publishMetadata();
}

}  // namespace snake
