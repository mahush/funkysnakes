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
    // Stage 1: Parse char stream into logical keys (context-free)
    auto [maybe_key, new_state] = tryParseKey(*ch, key_parser_state_);
    key_parser_state_ = new_state;

    // Stage 2: If we got a key, interpret it in game context and apply effects
    if (maybe_key) {
      auto quit = tryConvertKeyToQuit(*maybe_key);
      auto pause = tryConvertKeyToPauseToggle(*maybe_key);
      auto direction = tryConvertKeyToDirectionMsg(*maybe_key);
      applyEffects(direction, pause, quit);
    }
  }
}

// ============================================================================
// Stage 1: Context-Free Parsing (Char Stream → Keys)
// ============================================================================

std::pair<std::optional<Key>, InputActor::KeyParseState> InputActor::tryParseKey(char ch, KeyParseState state) const {
  // Stage 1: Context-free parsing of char stream into logical keys
  // Arrow keys come as 3-char escape sequences: ESC [ A/B/C/D

  switch (state) {
    case KeyParseState::NORMAL:
      if (ch == 27) {  // ESC
        return {std::nullopt, KeyParseState::SAW_ESC};
      }
      // Normal character - pass through as-is
      return {Key{ch}, KeyParseState::NORMAL};

    case KeyParseState::SAW_ESC:
      if (ch == '[') {
        return {std::nullopt, KeyParseState::SAW_BRACKET};
      }
      // Not an arrow key - treat as normal char, ESC is lost
      return {Key{ch}, KeyParseState::NORMAL};

    case KeyParseState::SAW_BRACKET:
      // Arrow key code - return special key
      switch (ch) {
        case 'A':
          return {Key{SpecialKey::ArrowUp}, KeyParseState::NORMAL};
        case 'B':
          return {Key{SpecialKey::ArrowDown}, KeyParseState::NORMAL};
        case 'C':
          return {Key{SpecialKey::ArrowRight}, KeyParseState::NORMAL};
        case 'D':
          return {Key{SpecialKey::ArrowLeft}, KeyParseState::NORMAL};
        default:
          // Unknown escape sequence - ignore, reset state
          return {std::nullopt, KeyParseState::NORMAL};
      }
  }

  // Unreachable, but satisfy compiler
  return {std::nullopt, KeyParseState::NORMAL};
}

// ============================================================================
// Imperative Shell - Apply Effects
// ============================================================================

void InputActor::applyEffects(std::optional<DirectionMsg> direction, std::optional<PauseToggleMsg> pause,
                              std::optional<QuitMsg> quit) {
  if (quit) {
    quit_pub_->publish(*quit);
    quit_requested_ = true;
  }

  if (pause) {
    pause_pub_->publish(*pause);
  }

  if (direction) {
    direction_pub_->publish(*direction);
  }
}

// ============================================================================
// Stage 2: Key Interpretation - Game-Specific Mapping
// ============================================================================

std::optional<DirectionMsg> InputActor::tryConvertKeyToDirectionMsg(const Key& key) const {
  // Stage 2: Map logical keys to game actions
  // Both alphanumeric keys AND arrow keys are interpreted

  PlayerId player_id;
  Direction direction{Direction::UP};  // Initialize to silence warning
  bool matched = false;

  // Handle both char and SpecialKey variants
  std::visit(
      [&](auto&& k) {
        using T = std::decay_t<decltype(k)>;
        if constexpr (std::is_same_v<T, char>) {
          // Player A: w/a/s/d - Player B: i/j/k/l
          switch (k) {
            // Player A keys
            case 'w':
            case 'W':
              player_id = PLAYER_A;
              direction = Direction::UP;
              matched = true;
              break;
            case 's':
            case 'S':
              player_id = PLAYER_A;
              direction = Direction::DOWN;
              matched = true;
              break;
            case 'a':
            case 'A':
              player_id = PLAYER_A;
              direction = Direction::LEFT;
              matched = true;
              break;
            case 'd':
            case 'D':
              player_id = PLAYER_A;
              direction = Direction::RIGHT;
              matched = true;
              break;

            // Player B keys
            case 'i':
            case 'I':
              player_id = PLAYER_B;
              direction = Direction::UP;
              matched = true;
              break;
            case 'k':
            case 'K':
              player_id = PLAYER_B;
              direction = Direction::DOWN;
              matched = true;
              break;
            case 'j':
            case 'J':
              player_id = PLAYER_B;
              direction = Direction::LEFT;
              matched = true;
              break;
            case 'l':
            case 'L':
              player_id = PLAYER_B;
              direction = Direction::RIGHT;
              matched = true;
              break;
          }
        } else if constexpr (std::is_same_v<T, SpecialKey>) {
          // Arrow keys map to Player B
          player_id = PLAYER_B;
          matched = true;
          switch (k) {
            case SpecialKey::ArrowUp:
              direction = Direction::UP;
              break;
            case SpecialKey::ArrowDown:
              direction = Direction::DOWN;
              break;
            case SpecialKey::ArrowLeft:
              direction = Direction::LEFT;
              break;
            case SpecialKey::ArrowRight:
              direction = Direction::RIGHT;
              break;
          }
        }
      },
      key);

  if (!matched) {
    return std::nullopt;
  }

  // Build and return DirectionMsg
  DirectionMsg msg;
  msg.game_id = game_id_;
  msg.player_id = player_id;
  msg.new_direction = direction;
  return msg;
}

std::optional<PauseToggleMsg> InputActor::tryConvertKeyToPauseToggle(const Key& key) const {
  // Only char 'p'/'P' toggles pause
  return std::visit(
      [&](auto&& k) -> std::optional<PauseToggleMsg> {
        using T = std::decay_t<decltype(k)>;
        if constexpr (std::is_same_v<T, char>) {
          if (k == 'p' || k == 'P') {
            PauseToggleMsg msg;
            msg.game_id = game_id_;
            return msg;
          }
        }
        return std::nullopt;
      },
      key);
}

std::optional<QuitMsg> InputActor::tryConvertKeyToQuit(const Key& key) const {
  // Only char 'q'/'Q' quits
  return std::visit(
      [&](auto&& k) -> std::optional<QuitMsg> {
        using T = std::decay_t<decltype(k)>;
        if constexpr (std::is_same_v<T, char>) {
          if (k == 'q' || k == 'Q') {
            return QuitMsg{};
          }
        }
        return std::nullopt;
      },
      key);
}

}  // namespace snake
