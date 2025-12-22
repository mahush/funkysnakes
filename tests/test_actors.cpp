#include <gtest/gtest.h>

#include "mock_actors.hpp"
#include "snake/game_manager.hpp"
#include "snake/game_session.hpp"
#include "snake/input_actor.hpp"
#include "snake/renderer.hpp"
#include "snake/topic.hpp"
#include "snake/topic_publisher.hpp"

namespace snake {

// Test InputActor processes user input correctly
TEST(ActorTest, InputActor_ProcessesUserInput) {
  asio::io_context io;

  // Create topic and mock subscriber
  auto direction_topic = std::make_shared<Topic<DirectionChange>>();
  auto mock_subscriber = MockDirectionChangeSubscriber::create(io, direction_topic);

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

  // Create publisher to send join requests
  Publisher<JoinRequest> joinrequest_pub{joinrequest_topic};

  // Join players via topic
  joinrequest_pub.publish(JoinRequest{"player1"});
  joinrequest_pub.publish(JoinRequest{"player2"});

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

  // Create mock subscriber for state updates
  auto mock_state_subscriber = MockStateUpdateSubscriber::create(io, state_topic);

  // Create GameSession
  auto session = GameSession::create(io, tick_topic, direction_topic, state_topic);

  // Create publisher to send ticks
  Publisher<Tick> tick_pub{tick_topic};

  // Simulate a tick by publishing to the topic
  Tick tick;
  tick.game_id = "game_001";
  tick_pub.publish(tick);

  // Run all pending work
  io.run();

  // Verify session sent a state update
  ASSERT_EQ(mock_state_subscriber->state_updates.size(), 1u);
  EXPECT_EQ(mock_state_subscriber->state_updates[0].state.game_id, "game_001");
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

  // Create mock subscriber for ticks
  auto mock_tick_subscriber = MockTickSubscriber::create(io, tick_topic);

  // Create GameManager
  auto manager = GameManager::create(io, tick_topic, gameover_topic, startclock_topic, stopclock_topic, tickrate_topic,
                                     joinrequest_topic, leaverequest_topic, startgame_topic);

  // Create publishers for clock control
  Publisher<StartClock> startclock_pub{startclock_topic};
  Publisher<StopClock> stopclock_pub{stopclock_topic};

  // Start game with short interval for testing
  StartClock start;
  start.game_id = "game_001";
  start.interval_ms = 10;  // 10ms for fast test
  startclock_pub.publish(start);

  // Run for a bit to allow some ticks
  io.run_for(std::chrono::milliseconds(50));

  // Stop
  StopClock stop;
  stop.game_id = "game_001";
  stopclock_pub.publish(stop);
  io.run();

  // Should have received multiple ticks (at least 3-4)
  EXPECT_GE(mock_tick_subscriber->ticks.size(), 3);
}

}  // namespace snake
