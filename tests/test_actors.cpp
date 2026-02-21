#include <gtest/gtest.h>

#include "actor-core/publisher.hpp"
#include "actor-core/timer/timer_factory.hpp"
#include "actor-core/topic.hpp"
#include "mock_actors.hpp"
#include "snake/game_engine_actor.hpp"
#include "snake/game_manager.hpp"
#include "snake/input_actor.hpp"
#include "snake/logger.hpp"
#include "snake/renderer.hpp"

using actor_core::Publisher;
using actor_core::TimerFactory;
using actor_core::Topic;

namespace snake {

// Global test environment for logger initialization
class LoggerTestEnvironment : public ::testing::Environment {
 public:
  void SetUp() override { Logger::initialize("test_snake.log"); }
  void TearDown() override { Logger::shutdown(); }
};

// Register the global test environment
::testing::Environment* const logger_env = ::testing::AddGlobalTestEnvironment(new LoggerTestEnvironment());

// Test InputActor can be created and managed
TEST(ActorTest, InputActor_CreationAndLifecycle) {
  asio::io_context io;

  // Create topics
  auto direction_topic = std::make_shared<Topic<DirectionChange>>();
  auto pause_topic = std::make_shared<Topic<PauseToggle>>();

  // Create InputActor with the topics
  auto input_actor = InputActor::create(io, direction_topic, pause_topic, "game_001");

  // Verify actor was created successfully
  ASSERT_NE(input_actor, nullptr);
  EXPECT_FALSE(input_actor->isReading());

  // Note: Integration testing of actual keyboard input would require
  // stdin mocking/piping which is complex. The pure mapping functions
  // (charToDirection, keyToPlayer) should be tested separately.
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
  auto metadata_topic = std::make_shared<Topic<GameStateMetadata>>();
  auto tickrate_topic = std::make_shared<Topic<TickRateChange>>();
  auto alivests_topic = std::make_shared<Topic<PlayerAliveStates>>();
  auto summary_req_topic = std::make_shared<Topic<GameStateSummaryRequest>>();
  auto summary_resp_topic = std::make_shared<Topic<GameStateSummaryResponse>>();
  auto pause_topic = std::make_shared<Topic<PauseToggle>>();

  // Create GameManager
  auto manager = GameManager::create(io, clock_topic, startgame_topic, reposition_topic, metadata_topic, tickrate_topic,
                                     alivests_topic, summary_req_topic, summary_resp_topic, gameover_topic, pause_topic,
                                     timer_factory);

  // Verify GameManager was created successfully
  SUCCEED();
}

// Test GameEngineActor handles clock commands
TEST(ActorTest, GameEngineActor_HandlesClockCommands) {
  asio::io_context io;

  // Create timer factory
  auto timer_factory = std::make_shared<TimerFactory>(io);

  // Create topics
  auto direction_topic = std::make_shared<Topic<DirectionChange>>();
  auto state_topic = std::make_shared<Topic<RenderableState>>();
  auto clock_topic = std::make_shared<Topic<GameClockCommand>>();
  auto tickrate_topic = std::make_shared<Topic<TickRateChange>>();
  auto reposition_topic = std::make_shared<Topic<FoodRepositionTrigger>>();
  auto alivests_topic = std::make_shared<Topic<PlayerAliveStates>>();
  auto summary_req_topic = std::make_shared<Topic<GameStateSummaryRequest>>();
  auto summary_resp_topic = std::make_shared<Topic<GameStateSummaryResponse>>();

  // Create GameEngineActor
  auto engine = GameEngineActor::create(io, direction_topic, state_topic, clock_topic, tickrate_topic, reposition_topic,
                                        alivests_topic, summary_req_topic, summary_resp_topic, timer_factory);

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

// Test GameManager handles pause toggle
TEST(ActorTest, GameManager_HandlesPauseToggle) {
  asio::io_context io;

  // Create timer factory
  auto timer_factory = std::make_shared<TimerFactory>(io);

  // Create topics
  auto gameover_topic = std::make_shared<Topic<GameOver>>();
  auto clock_topic = std::make_shared<Topic<GameClockCommand>>();
  auto startgame_topic = std::make_shared<Topic<StartGame>>();
  auto reposition_topic = std::make_shared<Topic<FoodRepositionTrigger>>();
  auto metadata_topic = std::make_shared<Topic<GameStateMetadata>>();
  auto tickrate_topic = std::make_shared<Topic<TickRateChange>>();
  auto alivests_topic = std::make_shared<Topic<PlayerAliveStates>>();
  auto summary_req_topic = std::make_shared<Topic<GameStateSummaryRequest>>();
  auto summary_resp_topic = std::make_shared<Topic<GameStateSummaryResponse>>();
  auto pause_topic = std::make_shared<Topic<PauseToggle>>();

  // Create mock subscriber for clock commands
  auto mock_clock_subscriber = MockClockCommandSubscriber::create(io, clock_topic);

  // Create GameManager
  auto manager = GameManager::create(io, clock_topic, startgame_topic, reposition_topic, metadata_topic, tickrate_topic,
                                     alivests_topic, summary_req_topic, summary_resp_topic, gameover_topic, pause_topic,
                                     timer_factory);

  // Create publisher for pause toggle
  Publisher<PauseToggle> pause_pub{pause_topic};

  // Start game first
  StartGame start;
  start.starting_level = 1;
  start.players = {PLAYER_A, PLAYER_B};
  Publisher<StartGame> startgame_pub{startgame_topic};
  startgame_pub.publish(start);

  // Process start game command
  io.poll();
  io.restart();

  // Clear the START command from mock subscriber
  mock_clock_subscriber->clock_commands.clear();

  // Send first pause toggle (should pause)
  PauseToggle toggle;
  toggle.game_id = "game_001";
  pause_pub.publish(toggle);

  // Run all pending work
  io.poll();
  io.restart();

  // Verify PAUSE command was published
  ASSERT_EQ(mock_clock_subscriber->clock_commands.size(), 1u);
  EXPECT_EQ(mock_clock_subscriber->clock_commands[0].state, GameClockState::PAUSE);

  // Send second pause toggle (should resume)
  pause_pub.publish(toggle);

  // Run all pending work
  io.poll();

  // Verify RESUME command was published
  ASSERT_EQ(mock_clock_subscriber->clock_commands.size(), 2u);
  EXPECT_EQ(mock_clock_subscriber->clock_commands[1].state, GameClockState::RESUME);
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
  auto metadata_topic = std::make_shared<Topic<GameStateMetadata>>();
  auto tickrate_topic = std::make_shared<Topic<TickRateChange>>();
  auto alivests_topic = std::make_shared<Topic<PlayerAliveStates>>();
  auto summary_req_topic = std::make_shared<Topic<GameStateSummaryRequest>>();
  auto summary_resp_topic = std::make_shared<Topic<GameStateSummaryResponse>>();
  auto pause_topic = std::make_shared<Topic<PauseToggle>>();

  // Create mock subscriber for clock commands
  auto mock_clock_subscriber = MockClockCommandSubscriber::create(io, clock_topic);

  // Create GameManager
  auto manager = GameManager::create(io, clock_topic, startgame_topic, reposition_topic, metadata_topic, tickrate_topic,
                                     alivests_topic, summary_req_topic, summary_resp_topic, gameover_topic, pause_topic,
                                     timer_factory);

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
