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
 * @brief Collision handling mode
 *
 * Determines what happens when snakes collide.
 */
enum class CollisionMode {
  BITE_REMOVE_TAIL,  // Cut tail is simply removed
  BITE_DROP_FOOD     // Cut tail segments become food items
};

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
   */
  GameSession(asio::io_context& io,
              TopicPtr<Tick> tick_topic,
              TopicPtr<DirectionChange> direction_topic,
              TopicPtr<StateUpdate> state_topic);

 private:
  void onTick(const Tick& msg);
  void onDirectionChange(const DirectionChange& msg);

  // Game initialization helpers
  void initializeSnake(const PlayerId& player_id);

  // Publishers for sending messages
  PublisherPtr<StateUpdate> state_pub_;

  // Subscriptions for pulling messages
  SubscriptionPtr<Tick> tick_sub_;
  SubscriptionPtr<DirectionChange> direction_sub_;

  GameState state_;
  CollisionMode collision_mode_{CollisionMode::BITE_REMOVE_TAIL};
};

}  // namespace snake
