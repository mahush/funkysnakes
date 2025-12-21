#pragma once

#include <vector>

#include "snake/control_messages.hpp"
#include "snake/game_messages.hpp"
#include "snake/message_sink.hpp"

namespace snake {

/**
 * @brief Mock subscriber for Tick messages
 */
class MockTickSubscriber : public MessageSink<Tick> {
 public:
  void post(Tick msg) override { ticks.push_back(msg); }
  std::vector<Tick> ticks;
};

/**
 * @brief Mock subscriber for DirectionChange messages
 */
class MockDirectionChangeSubscriber : public MessageSink<DirectionChange> {
 public:
  void post(DirectionChange msg) override { direction_changes.push_back(msg); }
  std::vector<DirectionChange> direction_changes;
};

/**
 * @brief Mock subscriber for StateUpdate messages
 */
class MockStateUpdateSubscriber : public MessageSink<StateUpdate> {
 public:
  void post(StateUpdate msg) override { state_updates.push_back(msg); }
  std::vector<StateUpdate> state_updates;
};

/**
 * @brief Mock subscriber for GameOver messages
 */
class MockGameOverSubscriber : public MessageSink<GameOver> {
 public:
  void post(GameOver msg) override { game_overs.push_back(msg); }
  std::vector<GameOver> game_overs;
};

/**
 * @brief Mock subscriber for StartClock messages
 */
class MockStartClockSubscriber : public MessageSink<StartClock> {
 public:
  void post(StartClock msg) override { start_clocks.push_back(msg); }
  std::vector<StartClock> start_clocks;
};

/**
 * @brief Mock subscriber for StopClock messages
 */
class MockStopClockSubscriber : public MessageSink<StopClock> {
 public:
  void post(StopClock msg) override { stop_clocks.push_back(msg); }
  std::vector<StopClock> stop_clocks;
};

}  // namespace snake
