#include "snake/input_actor.hpp"

#include <iostream>

#include "snake/game_session_sink.hpp"

namespace snake {

InputActor::InputActor(asio::io_context& io, std::shared_ptr<GameSessionSink> session, GameId game_id)
    : strand_(asio::make_strand(io)), session_(std::move(session)), game_id_(std::move(game_id)) {}

void InputActor::post(UserInputEvent msg) {
  asio::post(strand_, [self = shared_from_this(), msg] { self->onUserInputEvent(msg); });
}

void InputActor::onUserInputEvent(const UserInputEvent& msg) {
  std::cout << "[InputActor] Player '" << msg.player_id << "' pressed key '" << msg.key << "'\n";

  // Translate key to direction
  Direction dir = charToDirection(msg.key);

  // Send direction change to game session
  DirectionChange change;
  change.game_id = game_id_;
  change.player_id = msg.player_id;
  change.new_direction = dir;

  session_->post(change);
}

Direction InputActor::charToDirection(char key) const {
  // Simple mapping: w=UP, s=DOWN, a=LEFT, d=RIGHT
  // Could be extended for multiple players with different key bindings
  switch (key) {
    case 'w':
    case 'W':
      return Direction::UP;
    case 's':
    case 'S':
      return Direction::DOWN;
    case 'a':
    case 'A':
      return Direction::LEFT;
    case 'd':
    case 'D':
      return Direction::RIGHT;
    default:
      return Direction::UP;  // Default
  }
}

}  // namespace snake
