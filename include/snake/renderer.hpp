#pragma once

#include <asio.hpp>
#include <memory>

#include "snake/actor.hpp"
#include "snake/game_messages.hpp"
#include "snake/topic.hpp"
#include "snake/topic_subscription.hpp"

namespace snake {

/**
 * @brief Displays game state
 *
 * Renderer receives state updates and renders the current board,
 * snakes, food, scores, and level information.
 * For now, uses std::cout for simple text output.
 */
class Renderer : public Actor<Renderer> {
 public:
  // Process messages from subscribed topics
  void processMessages() override;

 protected:
  friend class Actor<Renderer>;

  /**
   * @brief Construct a new Renderer
   * @param io The io_context for async operations
   * @param state_topic Topic for state updates
   * @param gameover_topic Topic for game over notifications
   * @param level_topic Topic for level changes
   */
  Renderer(asio::io_context& io,
           TopicPtr<StateUpdate> state_topic,
           TopicPtr<GameOver> gameover_topic,
           TopicPtr<LevelChange> level_topic);

 private:
  void onStateUpdate(const StateUpdate& msg);
  void onGameOver(const GameOver& msg);
  void onLevelChange(const LevelChange& msg);

  // Subscriptions for pulling messages
  SubscriptionPtr<StateUpdate> state_sub_;
  SubscriptionPtr<GameOver> gameover_sub_;
  SubscriptionPtr<LevelChange> level_sub_;
};

}  // namespace snake
