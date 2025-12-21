#pragma once

#include <asio.hpp>
#include <memory>

#include "snake/actor.hpp"
#include "snake/control_messages.hpp"
#include "snake/game_messages.hpp"
#include "snake/message_sink.hpp"
#include "snake/topic.hpp"

namespace snake {

/**
 * @brief Core game engine - manages game state and logic
 *
 * GameSession holds all game state: board, snakes, food, scores, level.
 * Processes ticks, direction changes, and game logic.
 * Sends state updates and clock control messages via topics.
 */
class GameSession : public Actor<GameSession>,
                    public MessageSink<Tick>,
                    public MessageSink<DirectionChange>,
                    public MessageSink<PauseGame>,
                    public MessageSink<ResumeGame> {
 public:
  // MessageSink interface implementations
  void onEvent(Tick msg) override;
  void onEvent(DirectionChange msg) override;
  void onEvent(PauseGame msg) override;
  void onEvent(ResumeGame msg) override;

 protected:
  friend class Actor<GameSession>;

  /**
   * @brief Construct a new Game Session
   * @param io The io_context for async operations
   * @param tick_topic Topic to subscribe for game ticks
   * @param direction_topic Topic to subscribe for direction changes
   * @param state_topic Topic to publish state updates
   * @param startclock_topic Topic to publish clock start commands
   * @param stopclock_topic Topic to publish clock stop commands
   */
  GameSession(asio::io_context& io,
              std::shared_ptr<Topic<Tick>> tick_topic,
              std::shared_ptr<Topic<DirectionChange>> direction_topic,
              std::shared_ptr<Topic<StateUpdate>> state_topic,
              std::shared_ptr<Topic<StartClock>> startclock_topic,
              std::shared_ptr<Topic<StopClock>> stopclock_topic);

  auto subscribeToTopics() {
    return std::make_tuple(tick_topic_, direction_topic_);
  }

 private:
  // Topics for subscribing
  std::shared_ptr<Topic<Tick>> tick_topic_;
  std::shared_ptr<Topic<DirectionChange>> direction_topic_;

  // Topics for publishing
  std::shared_ptr<Topic<StateUpdate>> state_topic_;
  std::shared_ptr<Topic<StartClock>> startclock_topic_;
  std::shared_ptr<Topic<StopClock>> stopclock_topic_;

  GameState state_;
};

}  // namespace snake
