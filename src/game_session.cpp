#include "snake/game_session.hpp"

#include <iostream>

namespace snake {

GameSession::GameSession(asio::io_context& io,
                         std::shared_ptr<Topic<Tick>> tick_topic,
                         std::shared_ptr<Topic<DirectionChange>> direction_topic,
                         std::shared_ptr<Topic<StateUpdate>> state_topic,
                         std::shared_ptr<Topic<StartClock>> startclock_topic,
                         std::shared_ptr<Topic<StopClock>> stopclock_topic)
    : Actor(io),
      tick_topic_(tick_topic),
      direction_topic_(direction_topic),
      state_topic_(state_topic),
      startclock_topic_(startclock_topic),
      stopclock_topic_(stopclock_topic) {
  state_.game_id = "game_001";
  state_.running = false;
}

void GameSession::onEvent(Tick msg) {
  if (!state_.running) {
    return;
  }

  std::cout << "[GameSession] Tick for game '" << msg.game_id << "'\n";

  // TODO: Actual game logic (move snakes, check collisions, etc.)
  // For now, just send state update
  StateUpdate update;
  update.state = state_;
  state_topic_->publish(update);
}

void GameSession::onEvent(DirectionChange msg) {
  std::cout << "[GameSession] Player '" << msg.player_id << "' changed direction to "
            << static_cast<int>(msg.new_direction) << "\n";

  // TODO: Update snake direction in state
  // For now, just acknowledge
}

void GameSession::onEvent(PauseGame msg) {
  std::cout << "[GameSession] Game '" << msg.game_id << "' paused\n";
  state_.running = false;

  StopClock stop;
  stop.game_id = msg.game_id;
  stopclock_topic_->publish(stop);
}

void GameSession::onEvent(ResumeGame msg) {
  std::cout << "[GameSession] Game '" << msg.game_id << "' resumed\n";
  state_.running = true;

  StartClock start;
  start.game_id = msg.game_id;
  start.interval_ms = 500;  // TODO: Use actual tick rate
  startclock_topic_->publish(start);
}

}  // namespace snake
