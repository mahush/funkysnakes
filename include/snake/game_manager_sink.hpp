#pragma once

#include "snake/control_messages.hpp"
#include "snake/game_messages.hpp"

namespace snake {

/**
 * @brief Interface for components that can manage game lifecycle
 *
 * GameManager implements this interface.
 * Allows for mocking in tests.
 */
class GameManagerSink {
 public:
  virtual ~GameManagerSink() = default;

  /**
   * @brief Handle a join request
   */
  virtual void post(JoinRequest msg) = 0;

  /**
   * @brief Handle a leave request
   */
  virtual void post(LeaveRequest msg) = 0;

  /**
   * @brief Handle a start game request
   */
  virtual void post(StartGame msg) = 0;

  /**
   * @brief Handle a game over notification
   */
  virtual void post(GameOver msg) = 0;
};

}  // namespace snake
