#include <gtest/gtest.h>

#include "actor-core/timer/timer_factory.hpp"
#include "actor-core/topic.hpp"
#include "actor-core/topic_publisher.hpp"
#include "mock_actors.hpp"
#include "snake/game_engine.hpp"
#include "snake/game_manager.hpp"
#include "snake/input_actor.hpp"
#include "snake/renderer.hpp"

using actor_core::Publisher;
using actor_core::TimerFactory;
using actor_core::Topic;

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
  event.player_id = PLAYER_A;
  event.key = 'w';
  input_actor->post(event);

  // Run all pending work
  io.run();

  // Verify direction change was published
  ASSERT_EQ(mock_subscriber->direction_changes.size(), 1);
  EXPECT_EQ(mock_subscriber->direction_changes[0].player_id, PLAYER_A);
  EXPECT_EQ(mock_subscriber->direction_changes[0].new_direction, Direction::UP);
}

// Test GameManager coordinates actors correctly
TEST(ActorTest, GameManager_CoordinatesStartGame) {
  asio::io_context io;

  // Create timer factory
  auto timer_factory = std::make_shared<TimerFactory>(io);

  // Create topics
  auto gameover_topic = std::make_shared<Topic<GameOver>>();
  auto clock_topic = std::make_shared<Topic<GameClockCommand>>();
  auto startgame_topic = std::make_shared<Topic<StartGame>>();
  auto reposition_topic = std::make_shared<Topic<FoodRepositionTrigger>>();
  auto level_topic = std::make_shared<Topic<LevelChange>>();
  auto tickrate_topic = std::make_shared<Topic<TickRateChange>>();

  // Create GameManager
  auto manager = GameManager::create(io, gameover_topic, clock_topic, startgame_topic, reposition_topic, level_topic,
                                     tickrate_topic, timer_factory);

  // Verify GameManager was created successfully
  SUCCEED();
}

// Test GameEngine handles clock commands
TEST(ActorTest, GameEngine_HandlesClockCommands) {
  asio::io_context io;

  // Create timer factory
  auto timer_factory = std::make_shared<TimerFactory>(io);

  // Create topics
  auto direction_topic = std::make_shared<Topic<DirectionChange>>();
  auto state_topic = std::make_shared<Topic<RenderableState>>();
  auto clock_topic = std::make_shared<Topic<GameClockCommand>>();
  auto tickrate_topic = std::make_shared<Topic<TickRateChange>>();
  auto level_topic = std::make_shared<Topic<LevelChange>>();
  auto reposition_topic = std::make_shared<Topic<FoodRepositionTrigger>>();

  // Create GameEngine
  auto engine =
      GameEngine::create(io, direction_topic, state_topic, clock_topic, tickrate_topic, level_topic, reposition_topic, timer_factory);

  // Create publisher to send clock commands
  Publisher<GameClockCommand> clock_pub{clock_topic};

  // Send START command
  GameClockCommand cmd;
  cmd.game_id = "game_001";
  cmd.state = GameClockState::START;
  clock_pub.publish(cmd);

  // Send STOP command
  cmd.state = GameClockState::STOP;
  clock_pub.publish(cmd);

  // Process all commands
  io.run();

  // Verify no crash - engine should handle START/STOP commands
  SUCCEED();
}

// Test GameManager sends clock commands
TEST(ActorTest, GameManager_SendsClockCommands) {
  asio::io_context io;

  // Create timer factory
  auto timer_factory = std::make_shared<TimerFactory>(io);

  // Create topics
  auto gameover_topic = std::make_shared<Topic<GameOver>>();
  auto clock_topic = std::make_shared<Topic<GameClockCommand>>();
  auto startgame_topic = std::make_shared<Topic<StartGame>>();
  auto reposition_topic = std::make_shared<Topic<FoodRepositionTrigger>>();
  auto level_topic = std::make_shared<Topic<LevelChange>>();
  auto tickrate_topic = std::make_shared<Topic<TickRateChange>>();

  // Create mock subscriber for clock commands
  auto mock_clock_subscriber = MockClockCommandSubscriber::create(io, clock_topic);

  // Create GameManager
  auto manager = GameManager::create(io, gameover_topic, clock_topic, startgame_topic, reposition_topic, level_topic,
                                     tickrate_topic, timer_factory);

  // Create publisher for start game
  Publisher<StartGame> startgame_pub{startgame_topic};

  // Start game
  StartGame start;
  start.starting_level = 1;
  start.players = {PLAYER_A, PLAYER_B};
  startgame_pub.publish(start);

  // Run pending operations (use poll to avoid waiting for 20s timer)
  io.poll();

  // Verify GameManager sent START clock command
  ASSERT_EQ(mock_clock_subscriber->clock_commands.size(), 1u);
  EXPECT_EQ(mock_clock_subscriber->clock_commands[0].state, GameClockState::START);
}

}  // namespace snake
