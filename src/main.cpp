#include <chrono>
#include <iostream>
#include <thread>

#include "actor-core/publisher.hpp"
#include "actor-core/timer/timer_factory.hpp"
#include "actor-core/topic.hpp"
#include "snake/game_engine_actor.hpp"
#include "snake/game_manager_actor.hpp"
#include "snake/input_actor.hpp"
#include "snake/logger.hpp"
#include "snake/renderer_actor.hpp"

using actor_core::Publisher;
using actor_core::TimerFactory;
using actor_core::Topic;

int main() {
  std::cout << "=== Snake Game - Interactive Demo ===\n\n";

  // Initialize logger
  snake::Logger::initialize("snake_game.log");

  asio::io_context io;
  auto work_guard = asio::make_work_guard(io);  // Keep io_context alive

  // Create timer factory
  auto timer_factory = std::make_shared<TimerFactory>(io);

  // Create all topics first
  auto direction_topic = std::make_shared<Topic<snake::DirectionChange>>();
  auto state_topic = std::make_shared<Topic<snake::RenderableState>>();
  auto gameover_topic = std::make_shared<Topic<snake::GameOver>>();
  auto metadata_topic = std::make_shared<Topic<snake::GameStateMetadata>>();
  auto clock_topic = std::make_shared<Topic<snake::GameClockCommand>>();
  auto tickrate_topic = std::make_shared<Topic<snake::TickRateChange>>();
  auto reposition_topic = std::make_shared<Topic<snake::FoodRepositionTrigger>>();
  auto startgame_topic = std::make_shared<Topic<snake::StartGame>>();
  auto alivests_topic = std::make_shared<Topic<snake::PlayerAliveStates>>();
  auto summary_req_topic = std::make_shared<Topic<snake::GameStateSummaryRequest>>();
  auto summary_resp_topic = std::make_shared<Topic<snake::GameStateSummaryResponse>>();
  auto pause_topic = std::make_shared<Topic<snake::PauseToggle>>();

  // Create actors using factory methods - clean single-stage construction!
  auto renderer = snake::RendererActor::create(io, state_topic, gameover_topic, metadata_topic, timer_factory);

  auto engine =
      snake::GameEngineActor::create(io, direction_topic, state_topic, clock_topic, tickrate_topic, reposition_topic,
                                     alivests_topic, summary_req_topic, summary_resp_topic, timer_factory);

  auto manager = snake::GameManagerActor::create(io, clock_topic, startgame_topic, reposition_topic, metadata_topic,
                                                 tickrate_topic, alivests_topic, summary_req_topic, summary_resp_topic,
                                                 gameover_topic, pause_topic, timer_factory);

  auto input_actor = snake::InputActor::create(io, direction_topic, pause_topic, "game_001");

  // Create publisher for main thread to send commands
  Publisher<snake::StartGame> startgame_pub{startgame_topic};

  // Run io_context in background thread
  std::thread runner([&io] {
    std::cout << "[Main] io_context thread started\n";
    io.run();
    std::cout << "[Main] io_context thread finished\n";
  });

  std::cout << "Starting game...\n";
  snake::StartGame start;
  start.starting_level = 1;
  start.players = {snake::PLAYER_A, snake::PLAYER_B};
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

  // Shutdown logger
  snake::Logger::shutdown();

  std::cout << "Demo complete!\n";
  return 0;
}
