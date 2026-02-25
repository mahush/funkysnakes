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

void InputActor::scheduleRead(InputState state) {
  if (!is_reading_) {
    return;
  }

  // Async read one character - runs on strand via bind_executor
  asio::async_read(stdin_, asio::buffer(read_buffer_),
                   asio::bind_executor(strand_, [weak_self = weak_from_this(), state](std::error_code ec, std::size_t) {
                     auto self = weak_self.lock();
                     if (!self || ec) {
                       if (self) {
                         self->is_reading_ = false;
                       }
                       return;
                     }

                     char ch = self->read_buffer_[0];

                     // Process character with continuation callback
                     self->processChar(ch, state, [weak_self](InputState next_state) {
                       if (auto self = weak_self.lock()) {
                         self->scheduleRead(next_state);
                       }
                     });
                   }));
}

// ============================================================================
// Raw Input Acquisition Layer (Purely Async State Machine)
// ============================================================================

void InputActor::processChar(char ch, InputState state, std::function<void(InputState)> continue_reading) {
  // Pure business logic - fully testable without async I/O
  // Parse character with current state
  auto [maybe_key, next_state] = parseChar(ch, state);

  // If we got a complete key, process it
  if (maybe_key) {
    onKeyPress(*maybe_key);
  }

  // Continue reading with next state
  continue_reading(next_state);
}

std::pair<std::optional<char>, InputActor::InputState> InputActor::parseChar(char ch, InputState state) const {
  // Pure function: takes (char, state) → returns (optional_key, next_state)
  // State machine for parsing escape sequences across async reads
  // Arrow keys: ESC [ A/B/C/D (three separate async reads)

  switch (state) {
    case InputState::NORMAL:
      if (ch == 27) {                                // ESC
        return {std::nullopt, InputState::SAW_ESC};  // Wait for '['
      }
      // Normal character - pass through
      return {ch, InputState::NORMAL};

    case InputState::SAW_ESC:
      if (ch == '[') {
        return {std::nullopt, InputState::SAW_BRACKET};  // Wait for arrow key code
      }
      // Not an arrow key - treat as normal char, ESC is lost
      return {ch, InputState::NORMAL};

    case InputState::SAW_BRACKET:
      // Arrow key code - normalize to Player B key equivalents
      switch (ch) {
        case 'A':
          return {'i', InputState::NORMAL};  // Up arrow → 'i'
        case 'B':
          return {'k', InputState::NORMAL};  // Down arrow → 'k'
        case 'C':
          return {'l', InputState::NORMAL};  // Right arrow → 'l'
        case 'D':
          return {'j', InputState::NORMAL};  // Left arrow → 'j'
        default:
          // Unknown escape sequence - ignore, reset state
          return {std::nullopt, InputState::NORMAL};
      }
  }

  // Unreachable, but satisfy compiler
  return {std::nullopt, InputState::NORMAL};
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
