#pragma once

#include <memory>
#include <optional>

#include "actor-core/actor.hpp"
#include "actor-core/topic.hpp"
#include "snake/game_messages.hpp"
#include "snake/stdin_reader.hpp"

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
 * Follows the imperative shell / functional core pattern:
 * - Imperative shell: processInputs() pulls chars and applies effects
 * - Functional core: pure state transformations
 */
class InputActor : public Actor<InputActor> {
 public:
  /**
   * @brief Construct a new Input Actor
   * @param ctx Actor execution context
   * @param stdin_reader Shared stdin reader (like a topic)
   * @param direction_topic Topic to publish direction changes
   * @param pause_topic Topic to publish pause toggle requests
   * @param quit_topic Topic to publish quit requests
   * @param game_id The current game ID
   */
  InputActor(ActorContext ctx, std::shared_ptr<StdinReader> stdin_reader, TopicPtr<DirectionMsg> direction_topic,
             TopicPtr<PauseToggleMsg> pause_topic, TopicPtr<QuitMsg> quit_topic, GameId game_id);

  /**
   * @brief Check if quit was requested
   */
  bool quitRequested() const { return quit_requested_; }

  /**
   * @brief Process pending input characters (imperative shell)
   *
   * Symmetric to GameEngineActor::processInputs()!
   */
  void processInputs() override;

 private:
  // Escape sequence parsing state
  enum class EscapeSequenceState {
    NORMAL,      // Waiting for any key
    SAW_ESC,     // Saw ESC (27), waiting for '['
    SAW_BRACKET  // Saw ESC + '[', waiting for arrow key code
  };

  // Effects from processing input (functional core output)
  struct InputEffects {
    std::optional<DirectionMsg> direction;
    std::optional<PauseToggleMsg> pause;
    std::optional<QuitMsg> quit;
  };

  // Functional core: pure state transformation
  std::pair<EscapeSequenceState, InputEffects> processInputChar(char ch, EscapeSequenceState state) const;

  // Imperative shell: apply effects
  void applyEffects(const InputEffects& effects);

  // Key-to-message converters (pure functions)
  std::optional<DirectionMsg> tryConvertKeyToDirectionMsg(char key) const;
  std::optional<PauseToggleMsg> tryConvertKeyToPauseToggle(char key) const;
  std::optional<QuitMsg> tryConvertKeyToQuit(char key) const;

  // Parse escape sequence (pure function)
  std::pair<std::optional<char>, EscapeSequenceState> parseEscapeSequence(char ch, EscapeSequenceState state) const;

  std::shared_ptr<StdinReader> stdin_reader_;  // Like a subscription
  PublisherPtr<DirectionMsg> direction_pub_;
  PublisherPtr<PauseToggleMsg> pause_pub_;
  PublisherPtr<QuitMsg> quit_pub_;
  GameId game_id_;

  // Actor state (like game engine state)
  EscapeSequenceState escape_state_{EscapeSequenceState::NORMAL};
  bool quit_requested_{false};
};

}  // namespace snake
