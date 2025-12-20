#pragma once

#include "snake/game_messages.hpp"

namespace snake {

/**
 * @brief Interface for components that can control game clock
 *
 * Clock implements this interface.
 * Allows for mocking in tests.
 */
class ClockSink {
 public:
  virtual ~ClockSink() = default;

  /**
   * @brief Start the clock
   */
  virtual void post(StartClock msg) = 0;

  /**
   * @brief Stop the clock
   */
  virtual void post(StopClock msg) = 0;

  /**
   * @brief Change the tick rate
   */
  virtual void post(TickRateChange msg) = 0;
};

}  // namespace snake
