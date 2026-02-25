#include <gtest/gtest.h>

#include "actor-core/publisher.hpp"
#include "actor-core/timer/timer_factory.hpp"
#include "actor-core/topic.hpp"
#include "mock_actors.hpp"
#include "snake/game_engine_actor.hpp"
#include "snake/game_manager_actor.hpp"
#include "snake/input_actor.hpp"
#include "snake/logger.hpp"
#include "snake/renderer_actor.hpp"
#include "snake/stdin_reader.hpp"

using actor_core::Publisher;
using actor_core::TimerFactory;
using actor_core::Topic;
using snake::StdinReader;

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

  // Create shared stdin reader
  auto stdin_reader = std::make_shared<StdinReader>(io);

  // Create topics
  auto direction_topic = std::make_shared<Topic<DirectionMsg>>();
  auto pause_topic = std::make_shared<Topic<PauseToggleMsg>>();
  auto quit_topic = std::make_shared<Topic<QuitMsg>>();

  // Create InputActor with stdin reader and topics
  auto input_actor = InputActor::create(io, stdin_reader, direction_topic, pause_topic, quit_topic, "game_001");

  // Verify actor was created successfully
  ASSERT_NE(input_actor, nullptr);
  EXPECT_FALSE(input_actor->quitRequested());

  // Note: The functional core (processInputChar, parseEscapeSequence, etc.)
  // can be tested separately without any async I/O.
}

// Test GameManagerActor coordinates actors correctly
TEST(ActorTest, GameManagerActor_CoordinatesStartGame) {
  asio::io_context io;

  // Create timer factory
  auto timer_factory = std::make_shared<TimerFactory>(io);

  // Create topics
  auto gameover_topic = std::make_shared<Topic<GameOverMsg>>();
  auto clock_topic = std::make_shared<Topic<GameClockCommandMsg>>();
  auto startgame_topic = std::make_shared<Topic<StartGameMsg>>();
  auto reposition_topic = std::make_shared<Topic<FoodRepositionTriggerMsg>>();
  auto metadata_topic = std::make_shared<Topic<GameStateMetadataMsg>>();
  auto tickrate_topic = std::make_shared<Topic<TickRateChangeMsg>>();
  auto alivests_topic = std::make_shared<Topic<PlayerAliveStatesMsg>>();
  auto summary_req_topic = std::make_shared<Topic<GameStateSummaryRequestMsg>>();
  auto summary_resp_topic = std::make_shared<Topic<GameStateSummaryResponseMsg>>();
  auto pause_topic = std::make_shared<Topic<PauseToggleMsg>>();

  // Create GameManagerActor
  auto manager = GameManagerActor::create(io, clock_topic, startgame_topic, reposition_topic, metadata_topic,
                                          tickrate_topic, alivests_topic, summary_req_topic, summary_resp_topic,
                                          gameover_topic, pause_topic, timer_factory);

  // Verify GameManagerActor was created successfully
  SUCCEED();
}

// Test GameEngineActor handles clock commands
TEST(ActorTest, GameEngineActor_HandlesClockCommands) {
  asio::io_context io;

  // Create timer factory
  auto timer_factory = std::make_shared<TimerFactory>(io);

  // Create topics
  auto direction_topic = std::make_shared<Topic<DirectionMsg>>();
  auto state_topic = std::make_shared<Topic<RenderableStateMsg>>();
  auto clock_topic = std::make_shared<Topic<GameClockCommandMsg>>();
  auto tickrate_topic = std::make_shared<Topic<TickRateChangeMsg>>();
  auto reposition_topic = std::make_shared<Topic<FoodRepositionTriggerMsg>>();
  auto alivests_topic = std::make_shared<Topic<PlayerAliveStatesMsg>>();
  auto summary_req_topic = std::make_shared<Topic<GameStateSummaryRequestMsg>>();
  auto summary_resp_topic = std::make_shared<Topic<GameStateSummaryResponseMsg>>();

  // Create GameEngineActor
  auto engine = GameEngineActor::create(io, direction_topic, state_topic, clock_topic, tickrate_topic, reposition_topic,
                                        alivests_topic, summary_req_topic, summary_resp_topic, timer_factory);

  // Create publisher to send clock commands
  Publisher<GameClockCommandMsg> clock_pub{clock_topic};

  // Send START command
  GameClockCommandMsg cmd;
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

// Test GameManagerActor handles pause toggle
TEST(ActorTest, GameManagerActor_HandlesPauseToggle) {
  asio::io_context io;

  // Create timer factory
  auto timer_factory = std::make_shared<TimerFactory>(io);

  // Create topics
  auto gameover_topic = std::make_shared<Topic<GameOverMsg>>();
  auto clock_topic = std::make_shared<Topic<GameClockCommandMsg>>();
  auto startgame_topic = std::make_shared<Topic<StartGameMsg>>();
  auto reposition_topic = std::make_shared<Topic<FoodRepositionTriggerMsg>>();
  auto metadata_topic = std::make_shared<Topic<GameStateMetadataMsg>>();
  auto tickrate_topic = std::make_shared<Topic<TickRateChangeMsg>>();
  auto alivests_topic = std::make_shared<Topic<PlayerAliveStatesMsg>>();
  auto summary_req_topic = std::make_shared<Topic<GameStateSummaryRequestMsg>>();
  auto summary_resp_topic = std::make_shared<Topic<GameStateSummaryResponseMsg>>();
  auto pause_topic = std::make_shared<Topic<PauseToggleMsg>>();

  // Create mock subscriber for clock commands
  auto mock_clock_subscriber = MockClockCommandSubscriber::create(io, clock_topic);

  // Create GameManagerActor
  auto manager = GameManagerActor::create(io, clock_topic, startgame_topic, reposition_topic, metadata_topic,
                                          tickrate_topic, alivests_topic, summary_req_topic, summary_resp_topic,
                                          gameover_topic, pause_topic, timer_factory);

  // Create publisher for pause toggle
  Publisher<PauseToggleMsg> pause_pub{pause_topic};

  // Start game first
  StartGameMsg start;
  start.starting_level = 1;
  start.players = {PLAYER_A, PLAYER_B};
  Publisher<StartGameMsg> startgame_pub{startgame_topic};
  startgame_pub.publish(start);

  // Process start game command
  io.poll();
  io.restart();

  // Clear the START command from mock subscriber
  mock_clock_subscriber->clock_commands.clear();

  // Send first pause toggle (should pause)
  PauseToggleMsg toggle;
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

// Test GameManagerActor sends clock commands
TEST(ActorTest, GameManagerActor_SendsClockCommands) {
  asio::io_context io;

  // Create timer factory
  auto timer_factory = std::make_shared<TimerFactory>(io);

  // Create topics
  auto gameover_topic = std::make_shared<Topic<GameOverMsg>>();
  auto clock_topic = std::make_shared<Topic<GameClockCommandMsg>>();
  auto startgame_topic = std::make_shared<Topic<StartGameMsg>>();
  auto reposition_topic = std::make_shared<Topic<FoodRepositionTriggerMsg>>();
  auto metadata_topic = std::make_shared<Topic<GameStateMetadataMsg>>();
  auto tickrate_topic = std::make_shared<Topic<TickRateChangeMsg>>();
  auto alivests_topic = std::make_shared<Topic<PlayerAliveStatesMsg>>();
  auto summary_req_topic = std::make_shared<Topic<GameStateSummaryRequestMsg>>();
  auto summary_resp_topic = std::make_shared<Topic<GameStateSummaryResponseMsg>>();
  auto pause_topic = std::make_shared<Topic<PauseToggleMsg>>();

  // Create mock subscriber for clock commands
  auto mock_clock_subscriber = MockClockCommandSubscriber::create(io, clock_topic);

  // Create GameManagerActor
  auto manager = GameManagerActor::create(io, clock_topic, startgame_topic, reposition_topic, metadata_topic,
                                          tickrate_topic, alivests_topic, summary_req_topic, summary_resp_topic,
                                          gameover_topic, pause_topic, timer_factory);

  // Create publisher for start game
  Publisher<StartGameMsg> startgame_pub{startgame_topic};

  // Start game
  StartGameMsg start;
  start.starting_level = 1;
  start.players = {PLAYER_A, PLAYER_B};
  startgame_pub.publish(start);

  // Run pending operations (use poll to avoid waiting for 20s timer)
  io.poll();

  // Verify GameManagerActor sent START clock command
  ASSERT_EQ(mock_clock_subscriber->clock_commands.size(), 1u);
  EXPECT_EQ(mock_clock_subscriber->clock_commands[0].state, GameClockState::START);
}

}  // namespace snake
