#pragma once

#include "snake/messages.hpp"

namespace snake {

/**
 * @brief Interface for actors/components that can receive calculation results
 *
 * This interface allows CalculatorActor to send results to any sink
 * without depending on specific implementations (e.g., for testing).
 */
class ResultSink {
 public:
  virtual ~ResultSink() = default;

  /**
   * @brief Post a result message to this sink
   * @param msg The result message
   */
  virtual void post(MsgResult msg) = 0;
};

}  // namespace snake
