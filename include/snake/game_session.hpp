#pragma once

#include <asio.hpp>
#include <memory>

#include "snake/game_messages.hpp"
#include "snake/game_session_sink.hpp"

namespace snake {

// Forward declarations
class RendererSink;
class ClockSink;
class GameManagerSink;

/**
 * @brief Core game engine - manages game state and logic
 *
 * GameSession holds all game state: board, snakes, food, scores, level.
 * Processes ticks, direction changes, and game logic.
 */
class GameSession : public GameSessionSink, public std::enable_shared_from_this<GameSession> {
 public:
  /**
   * @brief Construct a new Game Session
   * @param io The io_context for async operations
   * @param renderer The renderer actor
   * @param clock The clock actor
   * @param manager The game manager actor
   */
  GameSession(asio::io_context& io, std::shared_ptr<RendererSink> renderer, std::shared_ptr<ClockSink> clock,
              std::shared_ptr<GameManagerSink> manager);

  void post(Tick msg) override;
  void post(DirectionChange msg) override;
  void post(PauseGame msg) override;
  void post(ResumeGame msg) override;

 private:
  void onTick(const Tick& msg);
  void onDirectionChange(const DirectionChange& msg);
  void onPauseGame(const PauseGame& msg);
  void onResumeGame(const ResumeGame& msg);

  asio::strand<asio::io_context::executor_type> strand_;
  std::shared_ptr<RendererSink> renderer_;
  std::shared_ptr<ClockSink> clock_;
  std::shared_ptr<GameManagerSink> manager_;

  GameState state_;
};

}  // namespace snake
