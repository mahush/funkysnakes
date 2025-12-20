#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "mock_result_sink.hpp"
#include "snake/calculator_actor.hpp"
#include "snake/result_actor.hpp"

namespace snake {

TEST(ActorTest, MultipleMessages_SingleThreaded) {
  asio::io_context io;

  auto result_sink = std::make_shared<MockResultSink>();
  auto calculator = std::make_shared<CalculatorActor>(io, result_sink);

  // Post multiple calculations
  calculator->post(MsgAdd{2, 5});
  calculator->post(MsgAdd{10, 3});
  calculator->post(MsgAdd{100, 42});

  // Run all pending work synchronously
  io.run();

  // Assert all results
  ASSERT_EQ(result_sink->getResults().size(), 3);
  EXPECT_EQ(result_sink->getResults()[0], 7);
  EXPECT_EQ(result_sink->getResults()[1], 13);
  EXPECT_EQ(result_sink->getResults()[2], 142);
}

TEST(ActorTest, MultipleMessages_MultiThreaded) {
  asio::io_context io;

  auto result_actor = std::make_shared<ResultActor>(io);
  auto calculator = std::make_shared<CalculatorActor>(io, result_actor);

  std::thread runner([&io] { io.run(); });

  // Post multiple calculations
  for (int i = 0; i < 10; ++i) {
    calculator->post(MsgAdd{i, i});
  }

  // Give time for processing
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  io.stop();
  runner.join();

  SUCCEED();
}

}  // namespace snake
