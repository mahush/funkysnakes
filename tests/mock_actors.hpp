#pragma once

#include <vector>

#include "snake/clock_sink.hpp"
#include "snake/game_manager_sink.hpp"
#include "snake/game_session_sink.hpp"
#include "snake/input_actor_sink.hpp"
#include "snake/renderer_sink.hpp"

namespace snake {

/**
 * @brief Mock GameSessionSink for testing
 */
class MockGameSession : public GameSessionSink {
 public:
  void post(Tick msg) override { ticks.push_back(msg); }
  void post(DirectionChange msg) override { direction_changes.push_back(msg); }
  void post(PauseGame msg) override { pauses.push_back(msg); }
  void post(ResumeGame msg) override { resumes.push_back(msg); }

  std::vector<Tick> ticks;
  std::vector<DirectionChange> direction_changes;
  std::vector<PauseGame> pauses;
  std::vector<ResumeGame> resumes;
};

/**
 * @brief Mock RendererSink for testing
 */
class MockRenderer : public RendererSink {
 public:
  void post(StateUpdate msg) override { state_updates.push_back(msg); }
  void post(GameOver msg) override { game_overs.push_back(msg); }
  void post(LevelChange msg) override { level_changes.push_back(msg); }

  std::vector<StateUpdate> state_updates;
  std::vector<GameOver> game_overs;
  std::vector<LevelChange> level_changes;
};

/**
 * @brief Mock ClockSink for testing
 */
class MockClock : public ClockSink {
 public:
  void post(StartClock msg) override { start_clocks.push_back(msg); }
  void post(StopClock msg) override { stop_clocks.push_back(msg); }
  void post(TickRateChange msg) override { tick_rate_changes.push_back(msg); }

  std::vector<StartClock> start_clocks;
  std::vector<StopClock> stop_clocks;
  std::vector<TickRateChange> tick_rate_changes;
};

/**
 * @brief Mock GameManagerSink for testing
 */
class MockGameManager : public GameManagerSink {
 public:
  void post(JoinRequest msg) override { join_requests.push_back(msg); }
  void post(LeaveRequest msg) override { leave_requests.push_back(msg); }
  void post(StartGame msg) override { start_games.push_back(msg); }
  void post(GameOver msg) override { game_overs.push_back(msg); }

  std::vector<JoinRequest> join_requests;
  std::vector<LeaveRequest> leave_requests;
  std::vector<StartGame> start_games;
  std::vector<GameOver> game_overs;
};

/**
 * @brief Mock InputActorSink for testing
 */
class MockInputActor : public InputActorSink {
 public:
  void post(UserInputEvent msg) override { user_inputs.push_back(msg); }

  std::vector<UserInputEvent> user_inputs;
};

}  // namespace snake
