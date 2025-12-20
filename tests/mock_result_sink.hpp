#pragma once

#include <vector>

#include "snake/result_sink.hpp"

namespace snake {

/**
 * @brief Mock result sink for testing - no asio dependencies
 *
 * This simple mock captures results in a vector for easy assertions.
 * Unlike ResultActor, it doesn't use strands or async operations.
 */
class MockResultSink : public ResultSink {
 public:
  void post(MsgResult msg) override { results_.push_back(msg.value); }

  const std::vector<int>& getResults() const { return results_; }

  void clear() { results_.clear(); }

 private:
  std::vector<int> results_;
};

}  // namespace snake
