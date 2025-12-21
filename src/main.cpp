#include <chrono>
#include <iostream>
#include <thread>

#include "snake/game_manager.hpp"
#include "snake/game_session.hpp"
#include "snake/input_actor.hpp"
#include "snake/renderer.hpp"
#include "snake/topic.hpp"

int main() {
  std::cout << "=== Snake Game - Interactive Demo ===\n\n";

  asio::io_context io;

  // Create all topics first
  auto tick_topic = std::make_shared<snake::Topic<snake::Tick>>();
  auto direction_topic = std::make_shared<snake::Topic<snake::DirectionChange>>();
  auto state_topic = std::make_shared<snake::Topic<snake::StateUpdate>>();
  auto gameover_topic = std::make_shared<snake::Topic<snake::GameOver>>();
  auto level_topic = std::make_shared<snake::Topic<snake::LevelChange>>();
  auto startclock_topic = std::make_shared<snake::Topic<snake::StartClock>>();
  auto stopclock_topic = std::make_shared<snake::Topic<snake::StopClock>>();
  auto tickrate_topic = std::make_shared<snake::Topic<snake::TickRateChange>>();
  auto joinrequest_topic = std::make_shared<snake::Topic<snake::JoinRequest>>();
  auto leaverequest_topic = std::make_shared<snake::Topic<snake::LeaveRequest>>();
  auto startgame_topic = std::make_shared<snake::Topic<snake::StartGame>>();

  // Create actors using factory methods - clean single-stage construction!
  auto renderer = snake::Renderer::create(io, state_topic, gameover_topic, level_topic);

  auto session = snake::GameSession::create(io, tick_topic, direction_topic, state_topic, startclock_topic,
                                            stopclock_topic);

  auto manager = snake::GameManager::create(io, tick_topic, gameover_topic, startclock_topic, stopclock_topic,
                                            tickrate_topic, joinrequest_topic, leaverequest_topic, startgame_topic);

  auto input_actor = snake::InputActor::create(io, direction_topic, "game_001");

  // Run io_context in background thread
  std::thread runner([&io] { io.run(); });

  std::cout << "Joining players...\n";
  joinrequest_topic->publish(snake::JoinRequest{"player1"});
  joinrequest_topic->publish(snake::JoinRequest{"player2"});
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  std::cout << "\nStarting game...\n";
  snake::StartGame start;
  start.starting_level = 1;
  start.players = {"player1", "player2"};
  startgame_topic->publish(start);
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
