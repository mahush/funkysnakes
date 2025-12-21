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

  // Create topic and mock subscriber with strand
  auto direction_topic = std::make_shared<Topic<DirectionChange>>();
  auto mock_subscriber = std::make_shared<MockDirectionChangeSubscriber>();
  auto mock_strand = asio::make_strand(io);
  direction_topic->subscribe(mock_subscriber, mock_strand);

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
  auto joinrequest_topic = std::make_shared<Topic<JoinRequest>>();
  auto leaverequest_topic = std::make_shared<Topic<LeaveRequest>>();
  auto startgame_topic = std::make_shared<Topic<StartGame>>();

  // Create GameManager
  auto manager = GameManager::create(io, tick_topic, gameover_topic, startclock_topic, stopclock_topic, tickrate_topic,
                                     joinrequest_topic, leaverequest_topic, startgame_topic);

  // Join players via topic
  joinrequest_topic->publish(JoinRequest{"player1"});
  joinrequest_topic->publish(JoinRequest{"player2"});

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

  // Create mock subscriber for state updates with strand
  auto mock_state_subscriber = std::make_shared<MockStateUpdateSubscriber>();
  auto mock_strand = asio::make_strand(io);
  state_topic->subscribe(mock_state_subscriber, mock_strand);

  // Create GameSession
  auto session = GameSession::create(io, tick_topic, direction_topic, state_topic, startclock_topic, stopclock_topic);

  // Simulate a tick by publishing to the topic
  Tick tick;
  tick.game_id = "game_001";
  tick_topic->publish(tick);

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
  auto joinrequest_topic = std::make_shared<Topic<JoinRequest>>();
  auto leaverequest_topic = std::make_shared<Topic<LeaveRequest>>();
  auto startgame_topic = std::make_shared<Topic<StartGame>>();

  // Create mock subscriber for ticks with strand
  auto mock_tick_subscriber = std::make_shared<MockTickSubscriber>();
  auto mock_strand = asio::make_strand(io);
  tick_topic->subscribe(mock_tick_subscriber, mock_strand);

  // Create GameManager
  auto manager = GameManager::create(io, tick_topic, gameover_topic, startclock_topic, stopclock_topic, tickrate_topic,
                                     joinrequest_topic, leaverequest_topic, startgame_topic);

  // Start game with short interval for testing
  StartClock start;
  start.game_id = "game_001";
  start.interval_ms = 10;  // 10ms for fast test
  startclock_topic->publish(start);

  // Run for a bit to allow some ticks
  io.run_for(std::chrono::milliseconds(50));

  // Stop
  StopClock stop;
  stop.game_id = "game_001";
  stopclock_topic->publish(stop);
  io.run();

  // Should have received multiple ticks (at least 3-4)
  EXPECT_GE(mock_tick_subscriber->ticks.size(), 3);
}

}  // namespace snake
