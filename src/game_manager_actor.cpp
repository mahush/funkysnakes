#include "snake/game_manager_actor.hpp"

#include <algorithm>

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

namespace {

// Maps the lifecycle Authority's clock intent to the engine's clock command
GameClockState toGameClockState(lifecycle::ClockIntent clock) {
  switch (clock) {
    case lifecycle::ClockIntent::START:
      return GameClockState::START;
    case lifecycle::ClockIntent::STOP:
      return GameClockState::STOP;
    case lifecycle::ClockIntent::PAUSE:
      return GameClockState::PAUSE;
    case lifecycle::ClockIntent::RESUME:
      return GameClockState::RESUME;
  }
  return GameClockState::STOP;
}

}  // namespace

void GameManagerActor::publishMetadata() {
  GameStateMetadataMsg metadata;
  metadata.game_id = lifecycle_.game_id;
  metadata.status = game_boundary::Status{lifecycle_.level, lifecycle_.paused};
  metadata_pub_->publish(metadata);
}

void GameManagerActor::executeClockIntent(lifecycle::ClockIntent clock,
                                          std::optional<lifecycle::StepIntervalIntent> interval) {
  GameClockCommandMsg cmd;
  cmd.game_id = lifecycle_.game_id;
  cmd.state = toGameClockState(clock);
  if (interval) {
    cmd.interval_ms = interval->interval_ms;
  }
  clock_pub_->publish(cmd);
}

void GameManagerActor::executeCadenceIntent(lifecycle::CadenceIntent cadence) {
  cadences_running_ = cadence == lifecycle::CadenceIntent::START;
  if (cadences_running_) {
    reposition_timer_->execute_command(make_periodic_command<RepositionTimerTag>(lifecycle::REPOSITION_PERIOD));
    level_timer_->execute_command(make_periodic_command<LevelTimerTag>(lifecycle::LEVEL_PERIOD));
    level_period_start_ = reposition_period_start_ = Clock::now();
  } else {
    reposition_timer_->execute_command(make_cancel_command<RepositionTimerTag>());
    level_timer_->execute_command(make_cancel_command<LevelTimerTag>());
  }
}

void GameManagerActor::freezeCadences() {
  if (!cadences_running_) {
    return;
  }
  auto remaining = [now = Clock::now()](Clock::time_point start, std::chrono::milliseconds period) {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
    return std::max(std::chrono::milliseconds{0}, period - elapsed);
  };
  level_period_remaining_ = remaining(level_period_start_, lifecycle::LEVEL_PERIOD);
  reposition_period_remaining_ = remaining(reposition_period_start_, lifecycle::REPOSITION_PERIOD);
  reposition_timer_->execute_command(make_cancel_command<RepositionTimerTag>());
  level_timer_->execute_command(make_cancel_command<LevelTimerTag>());
}

void GameManagerActor::resumeCadences() {
  if (!cadences_running_) {
    return;
  }
  // Finish the interrupted periods first; the timer handlers switch back to periodic afterwards
  auto now = Clock::now();
  level_period_start_ = now - (lifecycle::LEVEL_PERIOD - level_period_remaining_);
  reposition_period_start_ = now - (lifecycle::REPOSITION_PERIOD - reposition_period_remaining_);
  level_timer_->execute_command(make_single_shot_command<LevelTimerTag>(level_period_remaining_));
  reposition_timer_->execute_command(make_single_shot_command<RepositionTimerTag>(reposition_period_remaining_));
}

void GameManagerActor::onStartGame(const StartGameMsg& msg) {
  Logger::log("[GameManagerActor] Starting game with level " + std::to_string(msg.start.starting_level) + "\n");

  auto [state, clock, interval, cadence] = lifecycle::start("game_001", msg.start.starting_level, lifecycle_);
  lifecycle_ = state;

  executeClockIntent(clock, interval);
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

  GameOverMsg gameover{lifecycle_.game_id, game_boundary::GameOver{response.scores, lifecycle_.level}};
  gameover_pub_->publish(gameover);

  auto [clock, cadence] = lifecycle::concluded(lifecycle_);
  executeCadenceIntent(cadence);
  executeClockIntent(clock);

  Logger::log("[GameManagerActor] Game '" + gameover.game_id + "' ended at level " +
              std::to_string(gameover.game_over.final_level) + "\n");
  Logger::log("[GameManagerActor] Final scores:\n");
  for (const auto& [player_id, score] : gameover.game_over.final_scores) {
    Logger::log("[GameManagerActor]   " + player_id + ": " + std::to_string(score) + "\n");
  }
}

void GameManagerActor::onRepositionTimer() {
  // Restart the period (also switches back to periodic after a resumed, shortened period)
  reposition_period_start_ = Clock::now();
  reposition_timer_->execute_command(make_periodic_command<RepositionTimerTag>(lifecycle::REPOSITION_PERIOD));

  if (lifecycle::repositionPeriodElapsed(lifecycle_)) {
    FoodRepositionTriggerMsg trigger{lifecycle_.game_id};
    reposition_pub_->publish(trigger);
  }
}

void GameManagerActor::onLevelTimer() {
  // Restart the period (also switches back to periodic after a resumed, shortened period)
  level_period_start_ = Clock::now();
  level_timer_->execute_command(make_periodic_command<LevelTimerTag>(lifecycle::LEVEL_PERIOD));

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
  if (lifecycle_.paused) {
    freezeCadences();
  } else {
    resumeCadences();
  }
  publishMetadata();
}

}  // namespace snake
