#include <chrono>
#include <iostream>
#include <thread>

#include "snake/calculator_actor.hpp"
#include "snake/result_actor.hpp"

int main() {
  asio::io_context io;

  // Create actors
  auto result_actor = std::make_shared<snake::ResultActor>(io);
  auto calculator = std::make_shared<snake::CalculatorActor>(io, result_actor);

  // Run io_context in background thread
  std::thread runner([&io] { io.run(); });

  // Post some work
  std::cout << "Posting calculations...\n";
  calculator->post(snake::MsgAdd{2, 5});
  calculator->post(snake::MsgAdd{10, 3});
  calculator->post(snake::MsgAdd{100, 42});

  // Give time for messages to process
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Graceful shutdown
  std::cout << "Shutting down...\n";
  io.stop();
  runner.join();

  std::cout << "Done.\n";
  return 0;
}
