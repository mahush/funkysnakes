#pragma once

#include "snake/game_messages.hpp"

namespace snake {

/**
 * @brief Interface for components that can render game state
 *
 * Renderer implements this interface.
 * Allows for mocking in tests.
 */
class RendererSink {
 public:
  virtual ~RendererSink() = default;

  /**
   * @brief Handle a state update
   */
  virtual void post(StateUpdate msg) = 0;

  /**
   * @brief Handle a game over notification
   */
  virtual void post(GameOver msg) = 0;

  /**
   * @brief Handle a level change notification
   */
  virtual void post(LevelChange msg) = 0;
};

}  // namespace snake
