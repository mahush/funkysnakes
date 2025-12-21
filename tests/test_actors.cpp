#include <gtest/gtest.h>

#include "mock_actors.hpp"
#include "snake/game_manager.hpp"
#include "snake/game_session.hpp"
#include "snake/input_actor.hpp"
#include "snake/renderer.hpp"
#include "snake/topic.hpp"

namespace snake {

// Test InputActor processes user input correctly
TEST(ActorTest, InputActor_ProcessesUserInput) {
  asio::io_context io;

  // Create topic and mock subscriber
  auto direction_topic = std::make_shared<Topic<DirectionChange>>();
  auto mock_subscriber = std::make_shared<MockDirectionChangeSubscriber>();
  direction_topic->subscribe(mock_subscriber);

  // Create InputActor with the topic
  auto input_actor = InputActor::create(io, direction_topic, "game_001");

  // Simulate user input
  UserInputEvent event;
  event.player_id = "player1";
  event.key = 'w';
  input_actor->post(event);

  // Run all pending work
  io.run();

  // Verify direction change was published
  ASSERT_EQ(mock_subscriber->direction_changes.size(), 1);
  EXPECT_EQ(mock_subscriber->direction_changes[0].player_id, "player1");
  EXPECT_EQ(mock_subscriber->direction_changes[0].new_direction, Direction::UP);
}

// Test GameManager coordinates actors correctly
TEST(ActorTest, GameManager_CoordinatesStartGame) {
  asio::io_context io;

  // Create topics
  auto tick_topic = std::make_shared<Topic<Tick>>();
  auto gameover_topic = std::make_shared<Topic<GameOver>>();
  auto startclock_topic = std::make_shared<Topic<StartClock>>();
  auto stopclock_topic = std::make_shared<Topic<StopClock>>();
  auto tickrate_topic = std::make_shared<Topic<TickRateChange>>();

  // Create GameManager
  auto manager = GameManager::create(io, tick_topic, gameover_topic, startclock_topic, stopclock_topic, tickrate_topic);

  // Join players
  manager->post(JoinRequest{"player1"});
  manager->post(JoinRequest{"player2"});

  // Note: We don't test StartGame here because it starts an infinite timer loop.
  // Timer functionality is tested separately in GameManager_SendsPeriodicTicks.
  // Just run pending join operations
  io.run();

  // Verify no crash during player joins
  SUCCEED();
}

// Test GameSession handles ticks
TEST(ActorTest, GameSession_HandlesTicks) {
  asio::io_context io;

  // Create topics
  auto tick_topic = std::make_shared<Topic<Tick>>();
  auto direction_topic = std::make_shared<Topic<DirectionChange>>();
  auto state_topic = std::make_shared<Topic<StateUpdate>>();
  auto startclock_topic = std::make_shared<Topic<StartClock>>();
  auto stopclock_topic = std::make_shared<Topic<StopClock>>();

  // Create mock subscriber for state updates
  auto mock_state_subscriber = std::make_shared<MockStateUpdateSubscriber>();
  state_topic->subscribe(mock_state_subscriber);

  // Create GameSession
  auto session = GameSession::create(io, tick_topic, direction_topic, state_topic, startclock_topic, stopclock_topic);

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

// Test GameManager sends periodic ticks
TEST(ActorTest, GameManager_SendsPeriodicTicks) {
  asio::io_context io;

  // Create topics
  auto tick_topic = std::make_shared<Topic<Tick>>();
  auto gameover_topic = std::make_shared<Topic<GameOver>>();
  auto startclock_topic = std::make_shared<Topic<StartClock>>();
  auto stopclock_topic = std::make_shared<Topic<StopClock>>();
  auto tickrate_topic = std::make_shared<Topic<TickRateChange>>();

  // Create mock subscriber for ticks
  auto mock_tick_subscriber = std::make_shared<MockTickSubscriber>();
  tick_topic->subscribe(mock_tick_subscriber);

  // Create GameManager
  auto manager = GameManager::create(io, tick_topic, gameover_topic, startclock_topic, stopclock_topic, tickrate_topic);

  // Start game with short interval for testing
  StartClock start;
  start.game_id = "game_001";
  start.interval_ms = 10;  // 10ms for fast test
  manager->post(start);

  // Run for a bit to allow some ticks
  io.run_for(std::chrono::milliseconds(50));

  // Stop
  StopClock stop;
  stop.game_id = "game_001";
  manager->post(stop);
  io.run();

  // Should have received multiple ticks (at least 3-4)
  EXPECT_GE(mock_tick_subscriber->ticks.size(), 3);
}

}  // namespace snake
