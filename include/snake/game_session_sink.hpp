#pragma once

#include "snake/game_messages.hpp"

namespace snake {

/**
 * @brief Interface for components that can receive game session commands
 *
 * GameSession implements this interface.
 * Allows for mocking in tests.
 */
class GameSessionSink {
 public:
  virtual ~GameSessionSink() = default;

  /**
   * @brief Handle a game tick
   */
  virtual void post(Tick msg) = 0;

  /**
   * @brief Handle a direction change
   */
  virtual void post(DirectionChange msg) = 0;

  /**
   * @brief Handle a pause request
   */
  virtual void post(PauseGame msg) = 0;

  /**
   * @brief Handle a resume request
   */
  virtual void post(ResumeGame msg) = 0;
};

}  // namespace snake
