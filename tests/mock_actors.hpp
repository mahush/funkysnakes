#pragma once

#include <memory>
#include <vector>

#include "snake/control_messages.hpp"
#include "snake/game_messages.hpp"
#include "snake/message_processor_interface.hpp"
#include "snake/topic.hpp"
#include "snake/topic_subscription.hpp"

namespace snake {

/**
 * @brief Mock subscriber for Tick messages
 */
class MockTickSubscriber : public MessageProcessorInterface,
                           public std::enable_shared_from_this<MockTickSubscriber> {
 public:
  void setup(std::shared_ptr<Topic<Tick>> topic, asio::strand<asio::io_context::executor_type> strand) {
    subscription = std::make_shared<TopicSubscription<Tick>>();
    topic->subscribe(shared_from_this(), subscription.get(), strand);
  }

  void processMessages() override {
    while (auto msg = subscription->tryReceive()) {
      ticks.push_back(*msg);
    }
  }

  std::shared_ptr<TopicSubscription<Tick>> subscription;
  std::vector<Tick> ticks;
};

/**
 * @brief Mock subscriber for DirectionChange messages
 */
class MockDirectionChangeSubscriber : public MessageProcessorInterface,
                                      public std::enable_shared_from_this<MockDirectionChangeSubscriber> {
 public:
  void setup(std::shared_ptr<Topic<DirectionChange>> topic, asio::strand<asio::io_context::executor_type> strand) {
    subscription = std::make_shared<TopicSubscription<DirectionChange>>();
    topic->subscribe(shared_from_this(), subscription.get(), strand);
  }

  void processMessages() override {
    while (auto msg = subscription->tryReceive()) {
      direction_changes.push_back(*msg);
    }
  }

  std::shared_ptr<TopicSubscription<DirectionChange>> subscription;
  std::vector<DirectionChange> direction_changes;
};

/**
 * @brief Mock subscriber for StateUpdate messages
 */
class MockStateUpdateSubscriber : public MessageProcessorInterface,
                                  public std::enable_shared_from_this<MockStateUpdateSubscriber> {
 public:
  void setup(std::shared_ptr<Topic<StateUpdate>> topic, asio::strand<asio::io_context::executor_type> strand) {
    subscription = std::make_shared<TopicSubscription<StateUpdate>>();
    topic->subscribe(shared_from_this(), subscription.get(), strand);
  }

  void processMessages() override {
    while (auto msg = subscription->tryReceive()) {
      state_updates.push_back(*msg);
    }
  }

  std::shared_ptr<TopicSubscription<StateUpdate>> subscription;
  std::vector<StateUpdate> state_updates;
};

/**
 * @brief Mock subscriber for GameOver messages
 */
class MockGameOverSubscriber : public MessageProcessorInterface,
                               public std::enable_shared_from_this<MockGameOverSubscriber> {
 public:
  void setup(std::shared_ptr<Topic<GameOver>> topic, asio::strand<asio::io_context::executor_type> strand) {
    subscription = std::make_shared<TopicSubscription<GameOver>>();
    topic->subscribe(shared_from_this(), subscription.get(), strand);
  }

  void processMessages() override {
    while (auto msg = subscription->tryReceive()) {
      game_overs.push_back(*msg);
    }
  }

  std::shared_ptr<TopicSubscription<GameOver>> subscription;
  std::vector<GameOver> game_overs;
};

/**
 * @brief Mock subscriber for StartClock messages
 */
class MockStartClockSubscriber : public MessageProcessorInterface,
                                 public std::enable_shared_from_this<MockStartClockSubscriber> {
 public:
  void setup(std::shared_ptr<Topic<StartClock>> topic, asio::strand<asio::io_context::executor_type> strand) {
    subscription = std::make_shared<TopicSubscription<StartClock>>();
    topic->subscribe(shared_from_this(), subscription.get(), strand);
  }

  void processMessages() override {
    while (auto msg = subscription->tryReceive()) {
      start_clocks.push_back(*msg);
    }
  }

  std::shared_ptr<TopicSubscription<StartClock>> subscription;
  std::vector<StartClock> start_clocks;
};

/**
 * @brief Mock subscriber for StopClock messages
 */
class MockStopClockSubscriber : public MessageProcessorInterface,
                                public std::enable_shared_from_this<MockStopClockSubscriber> {
 public:
  void setup(std::shared_ptr<Topic<StopClock>> topic, asio::strand<asio::io_context::executor_type> strand) {
    subscription = std::make_shared<TopicSubscription<StopClock>>();
    topic->subscribe(shared_from_this(), subscription.get(), strand);
  }

  void processMessages() override {
    while (auto msg = subscription->tryReceive()) {
      stop_clocks.push_back(*msg);
    }
  }

  std::shared_ptr<TopicSubscription<StopClock>> subscription;
  std::vector<StopClock> stop_clocks;
};

}  // namespace snake
