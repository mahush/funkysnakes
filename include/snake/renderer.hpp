#pragma once

#include <asio.hpp>
#include <memory>

#include "actor-core/actor.hpp"
#include "actor-core/topic.hpp"
#include "actor-core/topic_subscription.hpp"
#include "snake/game_messages.hpp"

namespace snake {

// Import actor_core types into snake namespace for convenience
using actor_core::Actor;
using actor_core::SubscriptionPtr;
using actor_core::TopicPtr;

/**
 * @brief Displays game state
 *
 * Renderer receives state updates and renders the current board,
 * snakes, food, scores, and level information.
 * For now, uses std::cout for simple text output.
 */
class Renderer : public Actor<Renderer> {
 public:
  /**
   * @brief Construct a new Renderer
   * @param ctx Actor execution context
   * @param state_topic Topic for state updates
   * @param gameover_topic Topic for game over notifications
   * @param level_topic Topic for level changes
   */
  Renderer(Actor<Renderer>::ActorContext ctx, TopicPtr<StateUpdate> state_topic,
           TopicPtr<GameOver> gameover_topic, TopicPtr<LevelChange> level_topic);

  // Process messages from subscribed topics
  void processMessages() override;

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
