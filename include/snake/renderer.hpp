#pragma once

#include <asio.hpp>
#include <memory>

#include "snake/game_messages.hpp"
#include "snake/renderer_sink.hpp"

namespace snake {

/**
 * @brief Displays game state
 *
 * Renderer receives state updates and renders the current board,
 * snakes, food, scores, and level information.
 * For now, uses std::cout for simple text output.
 */
class Renderer : public RendererSink, public std::enable_shared_from_this<Renderer> {
 public:
  /**
   * @brief Construct a new Renderer
   * @param io The io_context for async operations
   */
  explicit Renderer(asio::io_context& io);

  void post(StateUpdate msg) override;
  void post(GameOver msg) override;
  void post(LevelChange msg) override;

 private:
  void onStateUpdate(const StateUpdate& msg);
  void onGameOver(const GameOver& msg);
  void onLevelChange(const LevelChange& msg);

  asio::strand<asio::io_context::executor_type> strand_;
};

}  // namespace snake
