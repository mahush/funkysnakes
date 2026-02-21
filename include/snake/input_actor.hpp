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

  // No subscriptions - InputActor only publishes
  void processInputs() override {}

  /**
   * @brief Construct a new Input Actor
   * @param ctx Actor execution context
   * @param direction_topic Topic to publish direction changes
   * @param pause_topic Topic to publish pause toggle requests
   * @param game_id The current game ID
   */
  InputActor(Actor<InputActor>::ActorContext ctx, TopicPtr<DirectionMsg> direction_topic,
             TopicPtr<PauseToggle> pause_topic, GameId game_id);

 private:
  void scheduleRead();
  void handleChar(char ch);
  void handleEscapeSequence();
  void publishDirectionMsg(PlayerId player_id, Direction dir);
  void publishPauseToggle();
  Direction charToDirection(char key) const;
  PlayerId keyToPlayer(char key) const;
  void enableRawMode();
  void disableRawMode();

  PublisherPtr<DirectionMsg> direction_pub_;
  PublisherPtr<PauseToggle> pause_pub_;
  GameId game_id_;

  asio::posix::stream_descriptor stdin_;
  std::vector<char> read_buffer_;
  bool is_reading_{false};
  termios orig_termios_{};
  bool raw_mode_enabled_{false};
};

}  // namespace snake
