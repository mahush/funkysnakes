#include <chrono>
#include <iostream>
#include <thread>

#include "snake/clock.hpp"
#include "snake/game_manager.hpp"
#include "snake/game_session.hpp"
#include "snake/input_actor.hpp"
#include "snake/renderer.hpp"

int main() {
  std::cout << "=== Snake Game Actor Demo ===\n\n";

  asio::io_context io;

  // Create all actors
  auto renderer = std::make_shared<snake::Renderer>(io);
  auto session = std::make_shared<snake::GameSession>(io, renderer, nullptr, nullptr);
  auto clock = std::make_shared<snake::Clock>(io, session);
  auto input_actor = std::make_shared<snake::InputActor>(io, session, "game_001");

  // Update session and create manager with proper dependencies
  session = std::make_shared<snake::GameSession>(io, renderer, clock, nullptr);
  auto manager = std::make_shared<snake::GameManager>(io, session, clock, renderer);

  // Update session with manager
  session = std::make_shared<snake::GameSession>(io, renderer, clock, manager);

  // Update input actor with correct session
  input_actor = std::make_shared<snake::InputActor>(io, session, "game_001");

  // Run io_context in background thread
  std::thread runner([&io] { io.run(); });

  std::cout << "1. Joining players...\n";
  manager->post(snake::JoinRequest{"player1"});
  manager->post(snake::JoinRequest{"player2"});
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  std::cout << "\n2. Starting game...\n";
  snake::StartGame start;
  start.starting_level = 1;
  start.players = {"player1", "player2"};
  manager->post(start);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  std::cout << "\n3. Simulating user inputs...\n";
  // Player 1 presses 'w' (UP)
  input_actor->post(snake::UserInputEvent{"player1", 'w'});
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Player 2 presses 'd' (RIGHT)
  input_actor->post(snake::UserInputEvent{"player2", 'd'});
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Player 1 presses 'a' (LEFT)
  input_actor->post(snake::UserInputEvent{"player1", 'a'});
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  std::cout << "\n4. Letting game tick for a few seconds...\n";
  std::this_thread::sleep_for(std::chrono::seconds(2));

  std::cout << "\n5. Simulating game over...\n";
  snake::GameSummary summary;
  summary.game_id = "game_001";
  summary.final_level = 3;
  summary.final_scores = {{"player1", 15}, {"player2", 12}};

  manager->post(snake::GameOver{summary});
  renderer->post(snake::GameOver{summary});
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  std::cout << "\n6. Shutting down...\n";
  io.stop();
  runner.join();

  std::cout << "\nDemo complete!\n";
  return 0;
}
