#pragma once

#include <asio.hpp>
#include <memory>

#include "snake/result_sink.hpp"
#include "snake/messages.hpp"

namespace snake {

/**
 * @brief Actor that receives and displays calculation results
 *
 * ResultActor processes MsgResult messages and outputs them.
 * All message handling is serialized through its strand.
 */
class ResultActor : public ResultSink, public std::enable_shared_from_this<ResultActor> {
 public:
  /**
   * @brief Construct a new Result Actor
   * @param io The io_context for async operations
   */
  explicit ResultActor(asio::io_context& io);

  /**
   * @brief Post a result message to this actor
   * @param msg The result message to process
   */
  void post(MsgResult msg) override;

 private:
  void onResult(const MsgResult& msg);

  asio::strand<asio::io_context::executor_type> strand_;
};

}  // namespace snake
