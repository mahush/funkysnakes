#include <chrono>
#include <funkyactors/publisher.hpp>
#include <funkyactors/timer/timer_factory.hpp>
#include <funkyactors/topic.hpp>
#include <iostream>
#include <thread>

#include "snake/game_engine_actor.hpp"
#include "snake/game_manager_actor.hpp"
#include "snake/input_actor.hpp"
#include "snake/logger.hpp"
#include "snake/renderer_actor.hpp"
#include "snake/stdin_reader.hpp"

using funkyactors::Publisher;
using funkyactors::TimerFactory;
using funkyactors::Topic;

int main() {
  std::cout << "=== Snake Game - Interactive Demo ===\n\n";

  // Initialize logger
  snake::Logger::initialize("snake_game.log");

  asio::io_context io;
  auto work_guard = asio::make_work_guard(io);  // Keep io_context alive

  // Create timer factory
  auto timer_factory = std::make_shared<TimerFactory>(io);

  // Create all topics first
  auto direction_topic = std::make_shared<Topic<snake::DirectionMsg>>();
  auto state_topic = std::make_shared<Topic<snake::RenderableStateMsg>>();
  auto gameover_topic = std::make_shared<Topic<snake::GameOverMsg>>();
  auto metadata_topic = std::make_shared<Topic<snake::GameStateMetadataMsg>>();
  auto clock_topic = std::make_shared<Topic<snake::GameClockCommandMsg>>();
  auto tickrate_topic = std::make_shared<Topic<snake::TickRateChangeMsg>>();
  auto reposition_topic = std::make_shared<Topic<snake::FoodRepositionTriggerMsg>>();
  auto startgame_topic = std::make_shared<Topic<snake::StartGameMsg>>();
  auto alivests_topic = std::make_shared<Topic<snake::PlayerAliveStatesMsg>>();
  auto summary_req_topic = std::make_shared<Topic<snake::GameStateSummaryRequestMsg>>();
  auto summary_resp_topic = std::make_shared<Topic<snake::GameStateSummaryResponseMsg>>();
  auto pause_topic = std::make_shared<Topic<snake::PauseToggleMsg>>();
  auto quit_topic = std::make_shared<Topic<snake::QuitMsg>>();

  // Create actors using factory methods - clean single-stage construction!
  auto renderer = snake::RendererActor::create(io, state_topic, gameover_topic, metadata_topic, timer_factory);

  auto engine = snake::GameEngineActor::create(io,
                                               direction_topic,
                                               state_topic,
                                               clock_topic,
                                               tickrate_topic,
                                               reposition_topic,
                                               alivests_topic,
                                               summary_req_topic,
                                               summary_resp_topic,
                                               timer_factory);

  auto manager = snake::GameManagerActor::create(io,
                                                 clock_topic,
                                                 startgame_topic,
                                                 reposition_topic,
                                                 metadata_topic,
                                                 tickrate_topic,
                                                 alivests_topic,
                                                 summary_req_topic,
                                                 summary_resp_topic,
                                                 gameover_topic,
                                                 pause_topic,
                                                 timer_factory);

  // Create shared stdin reader (like a topic)
  auto stdin_reader = std::make_shared<snake::StdinReader>(io);

  auto input_actor = snake::InputActor::create(io, stdin_reader, direction_topic, pause_topic, quit_topic, "game_001");

  // Create publisher for main thread to send commands
  Publisher<snake::StartGameMsg> startgame_pub{startgame_topic};

  // Run io_context in background thread
  std::thread runner([&io] {
    std::cout << "[Main] io_context thread started\n";
    io.run();
    std::cout << "[Main] io_context thread finished\n";
  });

  std::cout << "Starting game...\n";
  snake::StartGameMsg start;
  start.starting_level = 1;
  start.players = {snake::PLAYER_A, snake::PLAYER_B};
  startgame_pub.publish(start);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  std::cout << "\n--- Game Running ---\n";
  std::cout << "Type keys to control the snakes:\n\n";

  // Start reading keyboard input (like starting to publish to a topic)
  stdin_reader->startReading();

  // Wait for quit signal (orchestration layer)
  std::cout << "[Main] Waiting for quit signal (press 'q' to quit)...\n";
  while (!input_actor->quitRequested()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // Orchestrated shutdown sequence
  std::cout << "\n[Main] Quit signal received, initiating shutdown...\n";
  stdin_reader->stopReading();  // Stop the stdin reader
  work_guard.reset();           // Allow io_context to exit
  io.stop();
  runner.join();

  // Shutdown logger
  snake::Logger::shutdown();

  std::cout << "Demo complete!\n";
  return 0;
}
