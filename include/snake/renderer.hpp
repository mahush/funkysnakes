#pragma once

#include <asio.hpp>
#include <memory>

#include "actor-core/actor.hpp"
#include "actor-core/timer/timer.hpp"
#include "actor-core/topic.hpp"
#include "actor-core/topic_subscription.hpp"
#include "snake/game_messages.hpp"

namespace snake {

// Import actor_core types into snake namespace for convenience
using actor_core::Actor;
using actor_core::SubscriptionPtr;
using actor_core::Timer;
using actor_core::TimerCommand;
using actor_core::TimerElapsedEvent;
using actor_core::TimerFactoryPtr;
using actor_core::TopicPtr;

// Timer type definitions for Renderer
struct FlashTimerTag {};
using FlashTimerElapsedEvent = TimerElapsedEvent<FlashTimerTag>;
using FlashTimerCommand = TimerCommand<FlashTimerTag>;
using FlashTimer = Timer<FlashTimerElapsedEvent, FlashTimerCommand>;
using FlashTimerPtr = std::shared_ptr<FlashTimer>;

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
   * @param state_topic Topic for renderable state
   * @param gameover_topic Topic for game over notifications
   * @param level_topic Topic for level changes
   * @param timer_factory Factory for creating timers
   */
  Renderer(Actor<Renderer>::ActorContext ctx, TopicPtr<RenderableState> state_topic,
           TopicPtr<GameOver> gameover_topic, TopicPtr<LevelChange> level_topic, TimerFactoryPtr timer_factory);

  // Process messages from subscribed topics
  void processMessages() override;

 private:
  void onRenderableState(const RenderableState& state);
  void onGameOver(const GameOver& msg);
  void onLevelChange(const LevelChange& msg);
  void onFlashTimer();
  void renderBoard(const RenderableState& state, bool show_game_over = false);

  // Subscriptions for pulling messages
  SubscriptionPtr<RenderableState> state_sub_;
  SubscriptionPtr<GameOver> gameover_sub_;
  SubscriptionPtr<LevelChange> level_sub_;

  // Timer for flashing game over overlay
  FlashTimerPtr flash_timer_;

  // Last rendered state (for game over overlay)
  RenderableState last_state_;

  // Game over state tracking
  bool game_over_active_{false};
  bool flash_visible_{true};
};

}  // namespace snake
