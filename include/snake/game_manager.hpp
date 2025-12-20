#pragma once

#include <asio.hpp>
#include <memory>
#include <vector>

#include "snake/control_messages.hpp"
#include "snake/game_manager_sink.hpp"
#include "snake/game_messages.hpp"

namespace snake {

// Forward declarations
class GameSessionSink;
class ClockSink;
class RendererSink;

/**
 * @brief Coordinates game lifecycle, sessions, and player registry
 *
 * GameManager supervises all game sessions and handles high-level
 * game control (start, stop, player join/leave).
 */
class GameManager : public GameManagerSink, public std::enable_shared_from_this<GameManager> {
 public:
  /**
   * @brief Construct a new Game Manager
   * @param io The io_context for async operations
   * @param session The game session actor
   * @param clock The clock actor
   * @param renderer The renderer actor
   */
  GameManager(asio::io_context& io, std::shared_ptr<GameSessionSink> session, std::shared_ptr<ClockSink> clock,
              std::shared_ptr<RendererSink> renderer);

  void post(JoinRequest msg) override;
  void post(LeaveRequest msg) override;
  void post(StartGame msg) override;
  void post(GameOver msg) override;

 private:
  void onJoinRequest(const JoinRequest& msg);
  void onLeaveRequest(const LeaveRequest& msg);
  void onStartGame(const StartGame& msg);
  void onGameOver(const GameOver& msg);

  asio::strand<asio::io_context::executor_type> strand_;
  std::shared_ptr<GameSessionSink> session_;
  std::shared_ptr<ClockSink> clock_;
  std::shared_ptr<RendererSink> renderer_;
  std::vector<PlayerId> registered_players_;
};

}  // namespace snake
