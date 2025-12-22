#pragma once

#include <asio.hpp>
#include <memory>

#include "snake/actor.hpp"
#include "snake/control_messages.hpp"
#include "snake/game_messages.hpp"
#include "snake/topic.hpp"
#include "snake/topic_subscription.hpp"

namespace snake {

/**
 * @brief Core game engine - manages game state and logic
 *
 * GameSession holds all game state: board, snakes, food, scores, level.
 * Processes ticks, direction changes, and game logic.
 * Sends state updates and clock control messages via topics.
 */
class GameSession : public Actor<GameSession> {
 public:
  // Process messages from subscribed topics
  void processMessages() override;

 protected:
  friend class Actor<GameSession>;

  /**
   * @brief Construct a new Game Session
   * @param io The io_context for async operations
   * @param tick_topic Topic to subscribe for game ticks
   * @param direction_topic Topic to subscribe for direction changes
   * @param state_topic Topic to publish state updates
   * @param startclock_topic Topic to subscribe for clock start commands
   * @param stopclock_topic Topic to subscribe for clock stop commands
   */
  GameSession(asio::io_context& io,
              std::shared_ptr<Topic<Tick>> tick_topic,
              std::shared_ptr<Topic<DirectionChange>> direction_topic,
              std::shared_ptr<Topic<StateUpdate>> state_topic,
              std::shared_ptr<Topic<StartClock>> startclock_topic,
              std::shared_ptr<Topic<StopClock>> stopclock_topic);

 private:
  void onTick(const Tick& msg);
  void onDirectionChange(const DirectionChange& msg);
  void onStartClock(const StartClock& msg);
  void onStopClock(const StopClock& msg);

  // Game logic helpers
  void initializeSnake(const PlayerId& player_id);
  void moveSnake(SnakeState& snake);
  Position getNextHeadPosition(const SnakeState& snake) const;

  // Publishers for sending messages
  std::shared_ptr<TopicPublisher<StateUpdate>> state_pub_;

  // Subscriptions for pulling messages
  std::shared_ptr<TopicSubscription<Tick>> tick_sub_;
  std::shared_ptr<TopicSubscription<DirectionChange>> direction_sub_;
  std::shared_ptr<TopicSubscription<StartClock>> startclock_sub_;
  std::shared_ptr<TopicSubscription<StopClock>> stopclock_sub_;

  GameState state_;
};

}  // namespace snake
