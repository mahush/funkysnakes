#include "snake/input_actor.hpp"

#include <iostream>

namespace snake {

InputActor::InputActor(asio::io_context& io, std::shared_ptr<Topic<DirectionChange>> direction_topic, GameId game_id)
    : Actor(io), direction_pub_(create_pub(direction_topic)), game_id_(std::move(game_id)) {}

InputActor::~InputActor() { stopReading(); }

void InputActor::startReading() {
  if (input_thread_.joinable()) {
    return;  // Already running
  }

  should_stop_ = false;
  // Note: Background thread keeps shared_ptr to keep actor alive while reading
  input_thread_ = std::thread([self = shared_from_this()] { self->readInputLoop(); });

  std::cout << "[InputActor] Started reading from stdin\n";
  std::cout << "[InputActor] Player 1: w=UP, a=LEFT, s=DOWN, d=RIGHT\n";
  std::cout << "[InputActor] Player 2: i=UP, j=LEFT, k=DOWN, l=RIGHT\n";
  std::cout << "[InputActor] Press 'q' to quit\n";
}

void InputActor::stopReading() {
  should_stop_ = true;
  if (input_thread_.joinable()) {
    input_thread_.join();
  }
}

void InputActor::onUserInput(UserInputEvent msg) {
  std::cout << "[InputActor] Player '" << msg.player_id << "' pressed key '" << msg.key << "'\n";

  // Translate key to direction
  Direction dir = charToDirection(msg.key);

  // Publish direction change to topic
  DirectionChange change;
  change.game_id = game_id_;
  change.player_id = msg.player_id;
  change.new_direction = dir;

  std::cout << "[InputActor] Publishing DirectionChange (dir=" << static_cast<int>(dir) << ")\n";
  direction_pub_->publish(change);
}

// Helper method for background thread to post events to this actor's strand
void InputActor::post(UserInputEvent msg) {
  asio::post(strand_, [weak_self = weak_from_this(), msg] {
    if (auto self = weak_self.lock()) {
      self->onUserInput(msg);
    }
  });
}

void InputActor::readInputLoop() {
  while (!should_stop_) {
    char ch;
    if (std::cin.get(ch)) {
      // Skip newlines and whitespace for cleaner input
      if (ch == '\n' || ch == ' ') {
        continue;
      }

      // Check for quit command
      if (ch == 'q' || ch == 'Q') {
        std::cout << "[InputActor] Quit requested\n";
        should_stop_ = true;
        break;
      }

      // Determine which player this key belongs to
      PlayerId player = keyToPlayer(ch);
      if (!player.empty()) {
        UserInputEvent event;
        event.player_id = player;
        event.key = ch;

        // Post to our own strand for thread-safe processing
        post(event);
      }
    } else {
      // EOF or error
      break;
    }
  }

  std::cout << "[InputActor] Stopped reading from stdin\n";
}

PlayerId InputActor::keyToPlayer(char key) const {
  // Hardcoded mapping:
  // Player 1: w/a/s/d
  // Player 2: i/j/k/l
  switch (key) {
    case 'w':
    case 'W':
    case 'a':
    case 'A':
    case 's':
    case 'S':
    case 'd':
    case 'D':
      return "player1";

    case 'i':
    case 'I':
    case 'j':
    case 'J':
    case 'k':
    case 'K':
    case 'l':
    case 'L':
      return "player2";

    default:
      return "";  // Unknown key
  }
}

Direction InputActor::charToDirection(char key) const {
  // Player 1: w=UP, a=LEFT, s=DOWN, d=RIGHT
  // Player 2: i=UP, j=LEFT, k=DOWN, l=RIGHT
  switch (key) {
    case 'w':
    case 'W':
    case 'i':
    case 'I':
      return Direction::UP;

    case 's':
    case 'S':
    case 'k':
    case 'K':
      return Direction::DOWN;

    case 'a':
    case 'A':
    case 'j':
    case 'J':
      return Direction::LEFT;

    case 'd':
    case 'D':
    case 'l':
    case 'L':
      return Direction::RIGHT;

    default:
      return Direction::UP;  // Default fallback
  }
}

}  // namespace snake
