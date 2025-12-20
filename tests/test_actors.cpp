#pragma once

#include <gtest/gtest.h>

#include "mock_actors.hpp"
#include "snake/clock.hpp"
#include "snake/game_manager.hpp"
#include "snake/game_session.hpp"
#include "snake/input_actor.hpp"
#include "snake/renderer.hpp"

namespace snake {

// Test InputActor processes user input correctly
TEST(ActorTest, InputActor_ProcessesUserInput) {
  asio::io_context io;

  auto mock_session = std::make_shared<MockGameSession>();
  auto input_actor = std::make_shared<InputActor>(io, mock_session, "game_001");

  // Simulate user input
  UserInputEvent event;
  event.player_id = "player1";
  event.key = 'w';
  input_actor->post(event);

  // Run all pending work
  io.run();

  // Verify direction change was sent
  ASSERT_EQ(mock_session->direction_changes.size(), 1);
  EXPECT_EQ(mock_session->direction_changes[0].player_id, "player1");
  EXPECT_EQ(mock_session->direction_changes[0].new_direction, Direction::UP);
}

// Test GameManager coordinates actors correctly
TEST(ActorTest, GameManager_CoordinatesStartGame) {
  asio::io_context io;

  auto mock_session = std::make_shared<MockGameSession>();
  auto mock_clock = std::make_shared<MockClock>();
  auto mock_renderer = std::make_shared<MockRenderer>();

  auto manager = std::make_shared<GameManager>(io, mock_session, mock_clock, mock_renderer);

  // Join players
  manager->post(JoinRequest{"player1"});
  manager->post(JoinRequest{"player2"});

  // Start game
  StartGame start;
  start.starting_level = 2;
  start.players = {"player1", "player2"};
  manager->post(start);

  // Run all pending work
  io.run();

  // Verify clock was started
  ASSERT_EQ(mock_clock->start_clocks.size(), 1);
  EXPECT_EQ(mock_clock->start_clocks[0].game_id, "game_001");
  EXPECT_EQ(mock_clock->start_clocks[0].interval_ms, 500);
}

// Test GameSession handles ticks
TEST(ActorTest, GameSession_HandlesTicks) {
  asio::io_context io;

  auto mock_renderer = std::make_shared<MockRenderer>();
  auto mock_clock = std::make_shared<MockClock>();
  auto mock_manager = std::make_shared<MockGameManager>();

  auto session = std::make_shared<GameSession>(io, mock_renderer, mock_clock, mock_manager);

  // Simulate a tick
  Tick tick;
  tick.game_id = "game_001";
  session->post(tick);

  // Run all pending work
  io.run();

  // For now, session doesn't send state updates when not running
  // This test just verifies no crash
  SUCCEED();
}

// Test Clock sends periodic ticks
TEST(ActorTest, Clock_SendsPeriodicTicks) {
  asio::io_context io;

  auto mock_session = std::make_shared<MockGameSession>();
  auto clock = std::make_shared<Clock>(io, mock_session);

  // Start clock with short interval
  StartClock start;
  start.game_id = "game_001";
  start.interval_ms = 10;  // 10ms for fast test
  clock->post(start);

  // Run for a bit to allow some ticks
  io.run_for(std::chrono::milliseconds(50));

  // Stop clock
  StopClock stop;
  stop.game_id = "game_001";
  clock->post(stop);
  io.run();

  // Should have received multiple ticks (at least 3-4)
  EXPECT_GE(mock_session->ticks.size(), 3);
}

}  // namespace snake
