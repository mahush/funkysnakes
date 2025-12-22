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
      state_topic_(state_topic),
      startclock_topic_(startclock_topic),
      stopclock_topic_(stopclock_topic),
      tick_sub_(create_sub(tick_topic)),
      direction_sub_(create_sub(direction_topic)) {
  state_.game_id = "game_001";
  state_.running = false;
}

void GameSession::processMessages() {
  // Process all pending ticks first
  while (auto tick = tick_sub_->tryReceive()) {
    onTick(*tick);
  }

  // Then process direction changes
  while (auto dir = direction_sub_->tryReceive()) {
    onDirectionChange(*dir);
  }
}

void GameSession::onTick(const Tick& msg) {
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

void GameSession::onDirectionChange(const DirectionChange& msg) {
  std::cout << "[GameSession] Player '" << msg.player_id << "' changed direction to "
            << static_cast<int>(msg.new_direction) << "\n";

  // TODO: Update snake direction in state
  // For now, just acknowledge
}

}  // namespace snake
