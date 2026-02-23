#include "snake/input_actor.hpp"

#include <unistd.h>

#include <iostream>

#include "snake/control_messages.hpp"

namespace snake {

InputActor::InputActor(ActorContext ctx, TopicPtr<DirectionMsg> direction_topic, TopicPtr<PauseToggleMsg> pause_topic,
                       TopicPtr<QuitMsg> quit_topic, GameId game_id)
    : Actor{ctx},
      direction_pub_{create_pub(std::move(direction_topic))},
      pause_pub_{create_pub(std::move(pause_topic))},
      quit_pub_{create_pub(std::move(quit_topic))},
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
                     self->handleRawChar(ch);

                     // Re-schedule next read
                     self->scheduleRead();
                   }));
}

// ============================================================================
// Raw Input Acquisition Layer
// ============================================================================

void InputActor::handleRawChar(char ch) {
  // Special case: escape sequences need multi-char read
  if (ch == 27) {  // ESC
    if (auto key = readEscapeSequenceAsKey()) {
      onKeyPress(*key);
    }
    return;
  }

  // Normal key: pass to semantic key handler
  onKeyPress(ch);
}

std::optional<char> InputActor::readEscapeSequenceAsKey() {
  // Read the next two characters synchronously for escape sequence
  // (Arrow keys are: ESC [ A/B/C/D)
  char seq1 = 0;
  char seq2 = 0;
  if (read(STDIN_FILENO, &seq1, 1) == 1 && read(STDIN_FILENO, &seq2, 1) == 1) {
    if (seq1 == '[') {
      // Arrow key detected - normalize to Player B key equivalents
      switch (seq2) {
        case 'A':
          return 'i';  // Up arrow → 'i'
        case 'B':
          return 'k';  // Down arrow → 'k'
        case 'C':
          return 'l';  // Right arrow → 'l'
        case 'D':
          return 'j';  // Left arrow → 'j'
        default:
          return std::nullopt;  // Unknown escape sequence
      }
    }
  }
  return std::nullopt;  // Failed to read or not an arrow key
}

// ============================================================================
// Key Processing Layer (Semantic Key Handling)
// ============================================================================

void InputActor::onKeyPress(char key) {
  // Check for quit request
  if (auto quit_msg = tryConvertKeyToQuit(key)) {
    quit_pub_->publish(*quit_msg);
    quit_requested_ = true;  // Signal to orchestrator
    return;
  }

  // Check for pause toggle
  if (auto pause_msg = tryConvertKeyToPauseToggle(key)) {
    pause_pub_->publish(*pause_msg);
    return;
  }

  // Check for direction command
  if (auto direction_msg = tryConvertKeyToDirectionMsg(key)) {
    direction_pub_->publish(*direction_msg);
  }
}

// ============================================================================
// Key Conversion Functions - Clean, Scalable Pattern
// ============================================================================

std::optional<DirectionMsg> InputActor::tryConvertKeyToDirectionMsg(char key) const {
  // Determine player and direction from key
  PlayerId player_id;
  Direction direction;

  // Player A: w/a/s/d - Player B: i/j/k/l
  switch (key) {
    // Player A keys
    case 'w':
    case 'W':
      player_id = PLAYER_A;
      direction = Direction::UP;
      break;
    case 's':
    case 'S':
      player_id = PLAYER_A;
      direction = Direction::DOWN;
      break;
    case 'a':
    case 'A':
      player_id = PLAYER_A;
      direction = Direction::LEFT;
      break;
    case 'd':
    case 'D':
      player_id = PLAYER_A;
      direction = Direction::RIGHT;
      break;

    // Player B keys
    case 'i':
    case 'I':
      player_id = PLAYER_B;
      direction = Direction::UP;
      break;
    case 'k':
    case 'K':
      player_id = PLAYER_B;
      direction = Direction::DOWN;
      break;
    case 'j':
    case 'J':
      player_id = PLAYER_B;
      direction = Direction::LEFT;
      break;
    case 'l':
    case 'L':
      player_id = PLAYER_B;
      direction = Direction::RIGHT;
      break;

    default:
      return std::nullopt;  // Unknown key
  }

  // Build and return DirectionMsg
  DirectionMsg msg;
  msg.game_id = game_id_;
  msg.player_id = player_id;
  msg.new_direction = direction;
  return msg;
}

std::optional<PauseToggleMsg> InputActor::tryConvertKeyToPauseToggle(char key) const {
  if (key == 'p' || key == 'P') {
    PauseToggleMsg msg;
    msg.game_id = game_id_;
    return msg;
  }
  return std::nullopt;
}

std::optional<QuitMsg> InputActor::tryConvertKeyToQuit(char key) const {
  if (key == 'q' || key == 'Q') {
    return QuitMsg{};
  }
  return std::nullopt;
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
