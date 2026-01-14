#pragma once

#include <asio.hpp>
#include <atomic>
#include <map>
#include <memory>
#include <thread>
#include <termios.h>

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
 * and translates them into DirectionChange commands.
 * Supports multiple players (shared controller/keyboard).
 * Reads from stdin in a background thread.
 */
class InputActor : public Actor<InputActor> {
 public:
  /**
   * @brief Destructor - stops the input reading thread
   */
  ~InputActor();

  /**
   * @brief Start reading from stdin in background thread
   */
  void startReading();

  /**
   * @brief Stop reading from stdin
   */
  void stopReading();

  /**
   * @brief Check if the input thread is still running
   */
  bool isReading() const { return input_thread_.joinable() && !should_stop_; }

  // No subscriptions - InputActor only publishes
  void processMessages() override {}

  /**
   * @brief Construct a new Input Actor
   * @param ctx Actor execution context
   * @param direction_topic Topic to publish direction changes
   * @param game_id The current game ID
   */
  InputActor(Actor<InputActor>::ActorContext ctx, TopicPtr<DirectionChange> direction_topic, GameId game_id);

  // Helper for background thread and tests to post events to this actor's strand
  void post(UserInputEvent msg);

 private:
  void onUserInput(UserInputEvent msg);
  Direction charToDirection(char key) const;
  void readInputLoop();
  PlayerId keyToPlayer(char key) const;
  void enableRawMode();
  void disableRawMode();

  PublisherPtr<DirectionChange> direction_pub_;
  GameId game_id_;

  std::atomic<bool> should_stop_{false};
  std::thread input_thread_;
  termios orig_termios_{};
  bool raw_mode_enabled_{false};
};

}  // namespace snake
