#include "snake/input_actor.hpp"

#include "snake/control_messages.hpp"

namespace snake {

InputActor::InputActor(ActorContext ctx, std::shared_ptr<StdinReader> stdin_reader,
                       TopicPtr<DirectionMsg> direction_topic, TopicPtr<PauseToggleMsg> pause_topic,
                       TopicPtr<QuitMsg> quit_topic, GameId game_id)
    : Actor{ctx},
      stdin_reader_{std::move(stdin_reader)},
      direction_pub_{create_pub(std::move(direction_topic))},
      pause_pub_{create_pub(std::move(pause_topic))},
      quit_pub_{create_pub(std::move(quit_topic))},
      game_id_{std::move(game_id)} {
  // Subscribe to stdin reader (deferred like topic subscriptions)
  // Must happen after shared_ptr is fully constructed
  asio::post(strand_, [this] {
    // Now we can safely call shared_from_this()
    stdin_reader_->subscribe(shared_from_this(), strand_);
  });
}

// ============================================================================
// Imperative Shell - Process Inputs (Symmetric to GameEngineActor!)
// ============================================================================

void InputActor::processInputs() {
  // Pull-based processing - exactly like GameEngineActor::processInputs()!
  while (auto ch = stdin_reader_->tryTakeChar()) {
    // Functional core: pure state transformation
    auto [new_state, effects] = processInputChar(*ch, escape_state_);

    // Update actor state
    escape_state_ = new_state;

    // Imperative shell: apply effects
    applyEffects(effects);
  }
}

// ============================================================================
// Functional Core - Pure State Transformations
// ============================================================================

std::pair<InputActor::EscapeSequenceState, InputActor::InputEffects> InputActor::processInputChar(
    char ch, EscapeSequenceState state) const {
  // Parse escape sequence (state machine)
  auto [maybe_key, new_state] = parseEscapeSequence(ch, state);

  InputEffects effects;

  // If we got a complete key, convert to messages
  if (maybe_key) {
    effects.quit = tryConvertKeyToQuit(*maybe_key);
    effects.pause = tryConvertKeyToPauseToggle(*maybe_key);
    effects.direction = tryConvertKeyToDirectionMsg(*maybe_key);
  }

  return {new_state, effects};
}

std::pair<std::optional<char>, InputActor::EscapeSequenceState> InputActor::parseEscapeSequence(
    char ch, EscapeSequenceState state) const {
  // Pure function: parse escape sequences across multiple chars
  // Arrow keys: ESC [ A/B/C/D

  switch (state) {
    case EscapeSequenceState::NORMAL:
      if (ch == 27) {  // ESC
        return {std::nullopt, EscapeSequenceState::SAW_ESC};
      }
      // Normal character - pass through
      return {ch, EscapeSequenceState::NORMAL};

    case EscapeSequenceState::SAW_ESC:
      if (ch == '[') {
        return {std::nullopt, EscapeSequenceState::SAW_BRACKET};
      }
      // Not an arrow key - treat as normal char, ESC is lost
      return {ch, EscapeSequenceState::NORMAL};

    case EscapeSequenceState::SAW_BRACKET:
      // Arrow key code - normalize to Player B key equivalents
      switch (ch) {
        case 'A':
          return {'i', EscapeSequenceState::NORMAL};  // Up arrow → 'i'
        case 'B':
          return {'k', EscapeSequenceState::NORMAL};  // Down arrow → 'k'
        case 'C':
          return {'l', EscapeSequenceState::NORMAL};  // Right arrow → 'l'
        case 'D':
          return {'j', EscapeSequenceState::NORMAL};  // Left arrow → 'j'
        default:
          // Unknown escape sequence - ignore, reset state
          return {std::nullopt, EscapeSequenceState::NORMAL};
      }
  }

  // Unreachable, but satisfy compiler
  return {std::nullopt, EscapeSequenceState::NORMAL};
}

// ============================================================================
// Imperative Shell - Apply Effects
// ============================================================================

void InputActor::applyEffects(const InputEffects& effects) {
  if (effects.quit) {
    quit_pub_->publish(*effects.quit);
    quit_requested_ = true;
  }

  if (effects.pause) {
    pause_pub_->publish(*effects.pause);
  }

  if (effects.direction) {
    direction_pub_->publish(*effects.direction);
  }
}

// ============================================================================
// Key Conversion Functions - Pure, Testable
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

}  // namespace snake
