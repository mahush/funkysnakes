#include "snake/input_actor.hpp"

#include <unistd.h>

#include <iostream>

#include "snake/control_messages.hpp"

namespace snake {

InputActor::InputActor(Actor<InputActor>::ActorContext ctx, TopicPtr<DirectionMsg> direction_topic,
                       TopicPtr<PauseToggleMsg> pause_topic, GameId game_id)
    : Actor{ctx},
      direction_pub_{create_pub(std::move(direction_topic))},
      pause_pub_{create_pub(std::move(pause_topic))},
      game_id_{std::move(game_id)},
      stdin_{ctx.io_context(), STDIN_FILENO},
      read_buffer_{1} {}

InputActor::~InputActor() { stopReading(); }

void InputActor::startReading() {
  if (is_reading_) {
    return;  // Already reading
  }

  enableRawMode();
  is_reading_ = true;

  std::cout << "[InputActor] Started reading from stdin (async mode)\n";
  std::cout << "[InputActor] Player 1 (Snake A): w=UP, a=LEFT, s=DOWN, d=RIGHT\n";
  std::cout << "[InputActor] Player 2 (Snake B): Arrow keys (↑ ↓ ← →)\n";
  std::cout << "[InputActor] Press 'p' to pause/resume, 'q' to quit\n";

  scheduleRead();
}

void InputActor::stopReading() {
  if (!is_reading_) {
    return;
  }

  is_reading_ = false;
  stdin_.cancel();
  disableRawMode();

  std::cout << "\n[InputActor] Stopped reading from stdin\n";
}

void InputActor::scheduleRead() {
  if (!is_reading_) {
    return;
  }

  // Async read one character - runs on strand via bind_executor
  asio::async_read(stdin_, asio::buffer(read_buffer_),
                   asio::bind_executor(strand_, [weak_self = weak_from_this()](std::error_code ec, std::size_t) {
                     auto self = weak_self.lock();
                     if (!self || ec) {
                       if (self) {
                         self->is_reading_ = false;
                       }
                       return;
                     }

                     char ch = self->read_buffer_[0];
                     self->handleChar(ch);

                     // Re-schedule next read
                     self->scheduleRead();
                   }));
}

void InputActor::handleChar(char ch) {
  // Check for quit command
  if (ch == 'q' || ch == 'Q') {
    std::cout << "\n[InputActor] Quit requested\n";
    stopReading();
    return;
  }

  // Check for pause toggle
  if (ch == 'p' || ch == 'P') {
    publishPauseToggle();
    return;
  }

  // Check for escape sequences (arrow keys)
  if (ch == 27) {  // ESC
    handleEscapeSequence();
    return;
  }

  // Determine which player this key belongs to
  PlayerId player = keyToPlayer(ch);
  if (!player.empty()) {
    Direction dir = charToDirection(ch);
    publishDirectionMsg(player, dir);
  }
}

void InputActor::handleEscapeSequence() {
  // Read the next two characters synchronously for escape sequence
  // (Arrow keys are: ESC [ A/B/C/D)
  char seq1 = 0;
  char seq2 = 0;
  if (read(STDIN_FILENO, &seq1, 1) == 1 && read(STDIN_FILENO, &seq2, 1) == 1) {
    if (seq1 == '[') {
      // Arrow key detected - map to Player B controls
      char mapped_key = 0;
      switch (seq2) {
        case 'A':
          mapped_key = 'i';
          break;  // Up arrow
        case 'B':
          mapped_key = 'k';
          break;  // Down arrow
        case 'C':
          mapped_key = 'l';
          break;  // Right arrow
        case 'D':
          mapped_key = 'j';
          break;  // Left arrow
      }

      if (mapped_key) {
        Direction dir = charToDirection(mapped_key);
        publishDirectionMsg(PLAYER_B, dir);
      }
    }
  }
}

void InputActor::publishDirectionMsg(PlayerId player_id, Direction dir) {
  DirectionMsg change;
  change.game_id = game_id_;
  change.player_id = player_id;
  change.new_direction = dir;
  direction_pub_->publish(change);
}

void InputActor::publishPauseToggle() {
  PauseToggleMsg toggle;
  toggle.game_id = game_id_;
  pause_pub_->publish(toggle);
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
