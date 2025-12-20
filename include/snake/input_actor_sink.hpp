#pragma once

#include "snake/game_messages.hpp"

namespace snake {

/**
 * @brief Interface for components that can receive user input
 *
 * InputActor implements this interface.
 * Allows for mocking in tests.
 */
class InputActorSink {
 public:
  virtual ~InputActorSink() = default;

  /**
   * @brief Handle a user input event
   */
  virtual void post(UserInputEvent msg) = 0;
};

}  // namespace snake
