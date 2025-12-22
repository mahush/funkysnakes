#pragma once

#include <memory>
#include <vector>

#include "snake/actor.hpp"
#include "snake/control_messages.hpp"
#include "snake/game_messages.hpp"
#include "snake/topic.hpp"
#include "snake/topic_subscription.hpp"

namespace snake {

/**
 * @brief Mock subscriber for Tick messages
 */
class MockTickSubscriber : public Actor<MockTickSubscriber> {
 public:
  void processMessages() override {
    while (auto msg = tick_sub_->tryReceive()) {
      ticks.push_back(*msg);
    }
  }

  std::vector<Tick> ticks;

 protected:
  friend class Actor<MockTickSubscriber>;
  MockTickSubscriber(asio::io_context& io, std::shared_ptr<Topic<Tick>> topic)
      : Actor(io), tick_sub_(create_sub(topic)) {}

 private:
  std::shared_ptr<TopicSubscription<Tick>> tick_sub_;
};

/**
 * @brief Mock subscriber for DirectionChange messages
 */
class MockDirectionChangeSubscriber : public Actor<MockDirectionChangeSubscriber> {
 public:
  void processMessages() override {
    while (auto msg = direction_sub_->tryReceive()) {
      direction_changes.push_back(*msg);
    }
  }

  std::vector<DirectionChange> direction_changes;

 protected:
  friend class Actor<MockDirectionChangeSubscriber>;
  MockDirectionChangeSubscriber(asio::io_context& io, std::shared_ptr<Topic<DirectionChange>> topic)
      : Actor(io), direction_sub_(create_sub(topic)) {}

 private:
  std::shared_ptr<TopicSubscription<DirectionChange>> direction_sub_;
};

/**
 * @brief Mock subscriber for StateUpdate messages
 */
class MockStateUpdateSubscriber : public Actor<MockStateUpdateSubscriber> {
 public:
  void processMessages() override {
    while (auto msg = state_sub_->tryReceive()) {
      state_updates.push_back(*msg);
    }
  }

  std::vector<StateUpdate> state_updates;

 protected:
  friend class Actor<MockStateUpdateSubscriber>;
  MockStateUpdateSubscriber(asio::io_context& io, std::shared_ptr<Topic<StateUpdate>> topic)
      : Actor(io), state_sub_(create_sub(topic)) {}

 private:
  std::shared_ptr<TopicSubscription<StateUpdate>> state_sub_;
};

/**
 * @brief Mock subscriber for GameOver messages
 */
class MockGameOverSubscriber : public Actor<MockGameOverSubscriber> {
 public:
  void processMessages() override {
    while (auto msg = gameover_sub_->tryReceive()) {
      game_overs.push_back(*msg);
    }
  }

  std::vector<GameOver> game_overs;

 protected:
  friend class Actor<MockGameOverSubscriber>;
  MockGameOverSubscriber(asio::io_context& io, std::shared_ptr<Topic<GameOver>> topic)
      : Actor(io), gameover_sub_(create_sub(topic)) {}

 private:
  std::shared_ptr<TopicSubscription<GameOver>> gameover_sub_;
};

/**
 * @brief Mock subscriber for StartClock messages
 */
class MockStartClockSubscriber : public Actor<MockStartClockSubscriber> {
 public:
  void processMessages() override {
    while (auto msg = startclock_sub_->tryReceive()) {
      start_clocks.push_back(*msg);
    }
  }

  std::vector<StartClock> start_clocks;

 protected:
  friend class Actor<MockStartClockSubscriber>;
  MockStartClockSubscriber(asio::io_context& io, std::shared_ptr<Topic<StartClock>> topic)
      : Actor(io), startclock_sub_(create_sub(topic)) {}

 private:
  std::shared_ptr<TopicSubscription<StartClock>> startclock_sub_;
};

/**
 * @brief Mock subscriber for StopClock messages
 */
class MockStopClockSubscriber : public Actor<MockStopClockSubscriber> {
 public:
  void processMessages() override {
    while (auto msg = stopclock_sub_->tryReceive()) {
      stop_clocks.push_back(*msg);
    }
  }

  std::vector<StopClock> stop_clocks;

 protected:
  friend class Actor<MockStopClockSubscriber>;
  MockStopClockSubscriber(asio::io_context& io, std::shared_ptr<Topic<StopClock>> topic)
      : Actor(io), stopclock_sub_(create_sub(topic)) {}

 private:
  std::shared_ptr<TopicSubscription<StopClock>> stopclock_sub_;
};

}  // namespace snake
