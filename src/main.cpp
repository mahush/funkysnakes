#include <chrono>
#include <iostream>
#include <thread>

#include "actor-core/timer/timer_factory.hpp"
#include "actor-core/topic.hpp"
#include "actor-core/topic_publisher.hpp"
#include "snake/game_engine.hpp"
#include "snake/game_manager.hpp"
#include "snake/input_actor.hpp"
#include "snake/renderer.hpp"

using actor_core::Publisher;
using actor_core::TimerFactory;
using actor_core::Topic;

int main() {
  std::cout << "=== Snake Game - Interactive Demo ===\n\n";

  asio::io_context io;
  auto work_guard = asio::make_work_guard(io);  // Keep io_context alive

  // Create timer factory
  auto timer_factory = std::make_shared<TimerFactory>(io);

  // Create all topics first
  auto direction_topic = std::make_shared<Topic<snake::DirectionChange>>();
  auto state_topic = std::make_shared<Topic<snake::RenderableState>>();
  auto gameover_topic = std::make_shared<Topic<snake::GameOver>>();
  auto level_topic = std::make_shared<Topic<snake::LevelChange>>();
  auto clock_topic = std::make_shared<Topic<snake::GameClockCommand>>();
  auto tickrate_topic = std::make_shared<Topic<snake::TickRateChange>>();
  auto reposition_topic = std::make_shared<Topic<snake::FoodRepositionTrigger>>();
  auto joinrequest_topic = std::make_shared<Topic<snake::JoinRequest>>();
  auto leaverequest_topic = std::make_shared<Topic<snake::LeaveRequest>>();
  auto startgame_topic = std::make_shared<Topic<snake::StartGame>>();

  // Create actors using factory methods - clean single-stage construction!
  auto renderer = snake::Renderer::create(io, state_topic, gameover_topic, level_topic);

  auto engine = snake::GameEngine::create(io, direction_topic, state_topic, clock_topic, tickrate_topic, level_topic,
                                          reposition_topic, timer_factory);

  auto manager = snake::GameManager::create(io, gameover_topic, clock_topic, joinrequest_topic, leaverequest_topic,
                                            startgame_topic, reposition_topic, level_topic, tickrate_topic,
                                            timer_factory);

  auto input_actor = snake::InputActor::create(io, direction_topic, "game_001");

  // Create publishers for main thread to send commands
  Publisher<snake::JoinRequest> joinrequest_pub{joinrequest_topic};
  Publisher<snake::StartGame> startgame_pub{startgame_topic};

  // Run io_context in background thread
  std::thread runner([&io] {
    std::cout << "[Main] io_context thread started\n";
    io.run();
    std::cout << "[Main] io_context thread finished\n";
  });

  std::cout << "Joining players...\n";
  joinrequest_pub.publish(snake::JoinRequest{"player1"});
  joinrequest_pub.publish(snake::JoinRequest{"player2"});
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  std::cout << "\nStarting game...\n";
  snake::StartGame start;
  start.starting_level = 1;
  start.players = {"player1", "player2"};
  startgame_pub.publish(start);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  std::cout << "\n--- Game Running ---\n";
  std::cout << "Type keys to control the snakes (press Enter after each key):\n\n";

  // Start reading keyboard input
  input_actor->startReading();

  // Wait for input thread to finish (when user presses 'q' or EOF)
  while (input_actor->isReading()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::cout << "\nShutting down...\n";
  input_actor->stopReading();
  io.stop();
  runner.join();

  std::cout << "Demo complete!\n";
  return 0;
}
