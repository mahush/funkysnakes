#pragma once

#include <asio.hpp>
#include <funkyactors/actor.hpp>
#include <funkyactors/subscription.hpp>
#include <funkyactors/timer/timer.hpp>
#include <funkyactors/topic.hpp>
#include <memory>

#include "snake/game_messages.hpp"

namespace snake {

// Import actor_core types into snake namespace for convenience
using funkyactors::Actor;
using funkyactors::SubscriptionPtr;
using funkyactors::Timer;
using funkyactors::TimerCommand;
using funkyactors::TimerElapsedEvent;
using funkyactors::TimerFactoryPtr;
using funkyactors::TopicPtr;

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
  RendererActor(ActorContext ctx,
                TopicPtr<RenderableStateMsg> state_topic,
                TopicPtr<GameOverMsg> gameover_topic,
                TopicPtr<GameStateMetadataMsg> metadata_topic,
                TimerFactoryPtr timer_factory);

  // Process messages from subscribed topics
  void processInputs() override;

 private:
  void onRenderableState(const RenderableStateMsg& state);
  void onGameOver(const GameOverMsg& msg);
  void onGameStateMetadata(const GameStateMetadataMsg& msg);
  void onFlashTimer();
  void renderBoard(const RenderableStateMsg& state, bool show_game_over = false, bool show_paused = false);

  // Subscriptions for pulling messages
  SubscriptionPtr<RenderableStateMsg> state_sub_;
  SubscriptionPtr<GameOverMsg> gameover_sub_;
  SubscriptionPtr<GameStateMetadataMsg> metadata_sub_;

  // Timer for flashing game over overlay
  FlashTimerPtr flash_timer_;

  // Last rendered state (for game over overlay)
  RenderableStateMsg last_state_;

  // Game metadata (from GameManager)
  GameStateMetadataMsg metadata_;

  // Game over state tracking
  bool game_over_active_{false};
  bool flash_visible_{true};
};

}  // namespace snake
