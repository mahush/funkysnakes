#pragma once

#include <asio.hpp>
#include <memory>

#include "actor-core/actor.hpp"
#include "actor-core/subscription.hpp"
#include "actor-core/timer/timer.hpp"
#include "actor-core/topic.hpp"
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

// Timer type definitions for RendererActor
struct FlashTimerTag {};
using FlashTimerElapsedEvent = TimerElapsedEvent<FlashTimerTag>;
using FlashTimerCommand = TimerCommand<FlashTimerTag>;
using FlashTimer = Timer<FlashTimerElapsedEvent, FlashTimerCommand>;
using FlashTimerPtr = std::shared_ptr<FlashTimer>;

/**
 * @brief Displays game state
 *
 * RendererActor receives state updates and renders the current board,
 * snakes, food, scores, and level information.
 * For now, uses std::cout for simple text output.
 */
class RendererActor : public Actor<RendererActor> {
 public:
  /**
   * @brief Construct a new RendererActor
   * @param ctx Actor execution context
   * @param state_topic Topic for renderable state
   * @param gameover_topic Topic for game over notifications
   * @param metadata_topic Topic for game state metadata (level, pause state)
   * @param timer_factory Factory for creating timers
   */
  RendererActor(Actor<RendererActor>::ActorContext ctx, TopicPtr<RenderableState> state_topic,
                TopicPtr<GameOver> gameover_topic, TopicPtr<GameStateMetadata> metadata_topic,
                TimerFactoryPtr timer_factory);

  // Process messages from subscribed topics
  void processInputs() override;

 private:
  void onRenderableState(const RenderableState& state);
  void onGameOver(const GameOver& msg);
  void onGameStateMetadata(const GameStateMetadata& msg);
  void onFlashTimer();
  void renderBoard(const RenderableState& state, bool show_game_over = false, bool show_paused = false);

  // Subscriptions for pulling messages
  SubscriptionPtr<RenderableState> state_sub_;
  SubscriptionPtr<GameOver> gameover_sub_;
  SubscriptionPtr<GameStateMetadata> metadata_sub_;

  // Timer for flashing game over overlay
  FlashTimerPtr flash_timer_;

  // Last rendered state (for game over overlay)
  RenderableState last_state_;

  // Game metadata (from GameManager)
  GameStateMetadata metadata_;

  // Game over state tracking
  bool game_over_active_{false};
  bool flash_visible_{true};
};

}  // namespace snake
