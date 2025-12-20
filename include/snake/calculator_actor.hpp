#pragma once

#include <asio.hpp>
#include <memory>

#include "snake/result_sink.hpp"
#include "snake/messages.hpp"

namespace snake {

/**
 * @brief Actor that performs addition operations
 *
 * CalculatorActor receives MsgAdd messages, performs addition,
 * and sends results to any result sink. All message handling is
 * serialized through its strand for thread-safety.
 */
class CalculatorActor : public std::enable_shared_from_this<CalculatorActor> {
 public:
  /**
   * @brief Construct a new Calculator Actor
   * @param io The io_context for async operations
   * @param result_sink Any sink that can receive calculation results
   */
  CalculatorActor(asio::io_context& io, std::shared_ptr<ResultSink> result_sink);

  /**
   * @brief Post an addition message to this actor
   * @param msg The addition message to process
   */
  void post(MsgAdd msg);

 private:
  void onAdd(const MsgAdd& msg);

  asio::strand<asio::io_context::executor_type> strand_;
  std::shared_ptr<ResultSink> result_sink_;
};

}  // namespace snake
