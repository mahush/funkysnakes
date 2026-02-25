#pragma once

#include <termios.h>

#include <asio.hpp>
#include <vector>

#include "actor-core/actor.hpp"
#include "actor-core/topic.hpp"
#include "snake/game_messages.hpp"

namespace snake {

// Import actor_core types into snake namespace for convenience
using actor_core::Actor;
using actor_core::PublisherPtr;
using actor_core::TopicPtr;

/**
 * @brief Captures and processes user input
 *
 * InputActor receives input events, tags them with player ID,
 * and translates them into DirectionMsg commands.
 * Supports multiple players (shared controller/keyboard).
 * Uses asio async IO to read from stdin without blocking.
 */
class InputActor : public Actor<InputActor> {
 public:
  /**
   * @brief Destructor - stops input reading
   */
  ~InputActor();

  /**
   * @brief Start async reading from stdin
   */
  void startReading();

  /**
   * @brief Stop async reading from stdin
   */
  void stopReading();

  /**
   * @brief Check if still reading input
   */
  bool isReading() const { return is_reading_; }

  /**
   * @brief Check if quit was requested
   */
  bool quitRequested() const { return quit_requested_; }

  // No subscriptions - InputActor only publishes
  void processInputs() override {}

  /**
   * @brief Construct a new Input Actor
   * @param ctx Actor execution context
   * @param direction_topic Topic to publish direction changes
   * @param pause_topic Topic to publish pause toggle requests
   * @param quit_topic Topic to publish quit requests
   * @param game_id The current game ID
   */
  InputActor(ActorContext ctx, TopicPtr<DirectionMsg> direction_topic, TopicPtr<PauseToggleMsg> pause_topic,
             TopicPtr<QuitMsg> quit_topic, GameId game_id);

 private:
  // Escape sequence parsing state (local to read chain, not actor-global)
  enum class InputState {
    NORMAL,      // Waiting for any key
    SAW_ESC,     // Saw ESC (27), waiting for '['
    SAW_BRACKET  // Saw ESC + '[', waiting for arrow key code
  };

  // Raw input acquisition layer (stdin, escape sequences)
  void scheduleRead(InputState state = InputState::NORMAL);
  void processChar(char ch, InputState state, std::function<void(InputState)> continue_reading);
  std::pair<std::optional<char>, InputState> parseChar(char ch, InputState state) const;
  void enableRawMode();
  void disableRawMode();

  // Key processing layer (semantic key handling)
  void onKeyPress(char key);

  // Key-to-message converters (clean, scalable pattern)
  std::optional<DirectionMsg> tryConvertKeyToDirectionMsg(char key) const;
  std::optional<PauseToggleMsg> tryConvertKeyToPauseToggle(char key) const;
  std::optional<QuitMsg> tryConvertKeyToQuit(char key) const;

  PublisherPtr<DirectionMsg> direction_pub_;
  PublisherPtr<PauseToggleMsg> pause_pub_;
  PublisherPtr<QuitMsg> quit_pub_;
  GameId game_id_;

  asio::posix::stream_descriptor stdin_;
  std::vector<char> read_buffer_;
  bool is_reading_{false};
  bool quit_requested_{false};
  termios orig_termios_{};
  bool raw_mode_enabled_{false};
};

}  // namespace snake
