#pragma once

#include <asio.hpp>
#include <memory>

#include "snake/actor.hpp"
#include "snake/game_messages.hpp"
#include "snake/message_sink.hpp"
#include "snake/topic.hpp"

namespace snake {

/**
 * @brief Displays game state
 *
 * Renderer receives state updates and renders the current board,
 * snakes, food, scores, and level information.
 * For now, uses std::cout for simple text output.
 */
class Renderer : public Actor<Renderer>,
                 public MessageSink<StateUpdate>,
                 public MessageSink<GameOver>,
                 public MessageSink<LevelChange> {
 public:
  // MessageSink interface implementations
  void post(StateUpdate msg) override;
  void post(GameOver msg) override;
  void post(LevelChange msg) override;

 protected:
  friend class Actor<Renderer>;  // Allow Actor to call protected constructor

  /**
   * @brief Construct a new Renderer
   * @param io The io_context for async operations
   * @param state_topic Topic for state updates
   * @param gameover_topic Topic for game over notifications
   * @param level_topic Topic for level changes
   */
  Renderer(asio::io_context& io,
           std::shared_ptr<Topic<StateUpdate>> state_topic,
           std::shared_ptr<Topic<GameOver>> gameover_topic,
           std::shared_ptr<Topic<LevelChange>> level_topic);

  void subscribeToTopics() override;

 private:
  void onStateUpdate(const StateUpdate& msg);
  void onGameOver(const GameOver& msg);
  void onLevelChange(const LevelChange& msg);

  asio::strand<asio::io_context::executor_type> strand_;
  std::shared_ptr<Topic<StateUpdate>> state_topic_;
  std::shared_ptr<Topic<GameOver>> gameover_topic_;
  std::shared_ptr<Topic<LevelChange>> level_topic_;
};

}  // namespace snake
