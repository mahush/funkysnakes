#pragma once

#include <asio.hpp>
#include <atomic>
#include <map>
#include <memory>
#include <thread>

#include "snake/actor.hpp"
#include "snake/game_messages.hpp"
#include "snake/topic.hpp"

namespace snake {

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

  // Helper for background thread and tests to post events to this actor's strand
  void post(UserInputEvent msg);

 protected:
  friend class Actor<InputActor>;

  /**
   * @brief Construct a new Input Actor
   * @param io The io_context for async operations
   * @param direction_topic Topic to publish direction changes
   * @param game_id The current game ID
   */
  InputActor(asio::io_context& io, std::shared_ptr<Topic<DirectionChange>> direction_topic, GameId game_id);

 private:
  void onUserInput(UserInputEvent msg);
  Direction charToDirection(char key) const;
  void readInputLoop();
  PlayerId keyToPlayer(char key) const;

  std::shared_ptr<TopicPublisher<DirectionChange>> direction_pub_;
  GameId game_id_;

  std::atomic<bool> should_stop_{false};
  std::thread input_thread_;
};

}  // namespace snake
