#include "snake/input_actor.hpp"

#include <iostream>
#include <unistd.h>

#include "snake/control_messages.hpp"

namespace snake {

InputActor::InputActor(Actor<InputActor>::ActorContext ctx, TopicPtr<DirectionChange> direction_topic, GameId game_id)
    : Actor(ctx), direction_pub_(create_pub(direction_topic)), game_id_(std::move(game_id)) {}

InputActor::~InputActor() { stopReading(); }

void InputActor::startReading() {
  if (input_thread_.joinable()) {
    return;  // Already running
  }

  enableRawMode();

  should_stop_ = false;
  // Note: Background thread keeps shared_ptr to keep actor alive while reading
  input_thread_ = std::thread([self = shared_from_this()] { self->readInputLoop(); });

  std::cout << "[InputActor] Started reading from stdin (raw mode)\n";
  std::cout << "[InputActor] Player 1 (Snake A): w=UP, a=LEFT, s=DOWN, d=RIGHT\n";
  std::cout << "[InputActor] Player 2 (Snake B): Arrow keys (↑ ↓ ← →)\n";
  std::cout << "[InputActor] Press 'q' to quit\n";
}

void InputActor::stopReading() {
  should_stop_ = true;
  if (input_thread_.joinable()) {
    input_thread_.join();
  }
  disableRawMode();
}

void InputActor::onUserInput(UserInputEvent msg) {
  // Translate key to direction
  Direction dir = charToDirection(msg.key);

  // Publish direction change to topic
  DirectionChange change;
  change.game_id = game_id_;
  change.player_id = msg.player_id;
  change.new_direction = dir;

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
      // Check for quit command
      if (ch == 'q' || ch == 'Q') {
        std::cout << "\n[InputActor] Quit requested\n";
        should_stop_ = true;
        break;
      }

      // Check for escape sequences (arrow keys)
      if (ch == 27) {  // ESC
        char seq1, seq2;
        if (std::cin.get(seq1) && std::cin.get(seq2)) {
          if (seq1 == '[') {
            // Arrow key detected
            char arrow_key = 0;
            switch (seq2) {
              case 'A': arrow_key = 'i'; break;  // Up arrow -> 'i'
              case 'B': arrow_key = 'k'; break;  // Down arrow -> 'k'
              case 'C': arrow_key = 'l'; break;  // Right arrow -> 'l'
              case 'D': arrow_key = 'j'; break;  // Left arrow -> 'j'
            }

            if (arrow_key) {
              UserInputEvent event;
              event.player_id = PLAYER_B;  // Arrow keys control Player B
              event.key = arrow_key;
              post(event);
            }
          }
        }
        continue;
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

  std::cout << "\n[InputActor] Stopped reading from stdin\n";
}

PlayerId InputActor::keyToPlayer(char key) const {
  // Hardcoded mapping:
  // Player A: w/a/s/d
  // Player B: i/j/k/l
  switch (key) {
    case 'w':
    case 'W':
    case 'a':
    case 'A':
    case 's':
    case 'S':
    case 'd':
    case 'D':
      return PLAYER_A;

    case 'i':
    case 'I':
    case 'j':
    case 'J':
    case 'k':
    case 'K':
    case 'l':
    case 'L':
      return PLAYER_B;

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

void InputActor::enableRawMode() {
  // Get current terminal attributes
  if (tcgetattr(STDIN_FILENO, &orig_termios_) == -1) {
    std::cerr << "[InputActor] Failed to get terminal attributes\n";
    return;
  }

  termios raw = orig_termios_;

  // Disable canonical mode (line buffering) and echo
  raw.c_lflag &= ~(ICANON | ECHO);

  // Set minimum characters to read and timeout
  raw.c_cc[VMIN] = 1;   // Read one character at a time
  raw.c_cc[VTIME] = 0;  // No timeout

  // Apply new settings
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
    std::cerr << "[InputActor] Failed to set raw mode\n";
    return;
  }

  raw_mode_enabled_ = true;
}

void InputActor::disableRawMode() {
  if (raw_mode_enabled_) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios_);
    raw_mode_enabled_ = false;
  }
}

}  // namespace snake
