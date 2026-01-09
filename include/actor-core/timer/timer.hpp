#pragma once

#include <chrono>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "actor-core/timer/timer_core.hpp"

namespace actor_core {

// Forward declaration
class ProcessorInterface;

/**
 * @brief Empty context type for when no context is needed
 */
struct NoContext {};

/**
 * @brief Timer command structure for controlling timer behavior
 */
template <typename TTag, typename TContext = NoContext>
struct TimerCommand {
  enum class Command {
    NONE,                // No operation
    START_SINGLE_SHOT,   // Start single-shot timer with specified duration
    START_PERIODIC,      // Start periodic timer with specified interval
    CANCEL               // Cancel/reset any scheduled timer
  };

  Command command{Command::NONE};
  std::chrono::milliseconds duration{0};
  TContext context{};
};

/**
 * @brief Default timer elapsed event structure
 */
template <typename TTag, typename TContext = NoContext>
struct TimerElapsedEvent {
  TContext context{};
};

/**
 * @brief Templated timer class with custom event and command handling
 *
 * This class provides the take_all_elapsed_events() method for consuming elapsed events
 * and execute_command() for timer control. By using templates, different timer event types
 * and command types can be used without code duplication.
 */
template <typename TTimerElapsedEvent, typename TTimerCommand>
class Timer {
 public:
  using ElapsedEventType = TTimerElapsedEvent;
  using CommandType = TTimerCommand;

  /**
   * @brief Construct a new Timer with provided timer core
   * @param timer_core Timer core implementation
   */
  explicit Timer(TimerCorePtr timer_core) : timer_core_{std::move(timer_core)} {}

  /**
   * @brief Execute a timer command
   * @param command Timer command to execute
   */
  void execute_command(const TTimerCommand& command) {
    current_context_ = command.context;

    switch (command.command) {
      case TTimerCommand::Command::START_SINGLE_SHOT:
        timer_core_->schedule_single_shot(command.duration);
        break;
      case TTimerCommand::Command::START_PERIODIC:
        timer_core_->schedule_periodic(command.duration);
        break;
      case TTimerCommand::Command::CANCEL:
        timer_core_->cancel_scheduled();
        break;
      case TTimerCommand::Command::NONE:
        // No operation
        break;
    }
  }

  /**
   * @brief Take all pending timer elapsed events
   * @return Vector of timer elapsed events that occurred since last take
   */
  std::vector<TTimerElapsedEvent> take_all_elapsed_events() {
    const auto count = timer_core_->take_all_elapsed_events();

    std::vector<TTimerElapsedEvent> events;
    events.reserve(count);

    for (size_t i = 0; i < count; ++i) {
      TTimerElapsedEvent event{};
      event.context = current_context_;
      events.push_back(event);
    }

    return events;
  }

  /**
   * @brief Check if timer is currently scheduled
   * @return true if timer is scheduled, false otherwise
   */
  bool is_scheduled() const { return timer_core_->is_scheduled(); }

  /**
   * @brief Subscribe a processor to receive timer event notifications
   * @param processor The processor (forwards to timer core)
   */
  void subscribe(std::weak_ptr<ProcessorInterface> processor) {
    timer_core_->subscribe(std::move(processor));
  }

 private:
  // Helper to extract context type from command
  template <typename TTag, typename TContext>
  static TContext extract_context_type(const TimerCommand<TTag, TContext>&);
  using ContextType = decltype(extract_context_type(std::declval<TTimerCommand>()));

  ContextType current_context_{};
  TimerCorePtr timer_core_;
};

// Template alias for convenience
template <typename TTimerElapsedEvent, typename TTimerCommand>
using TimerPtr = std::shared_ptr<Timer<TTimerElapsedEvent, TTimerCommand>>;

// Factory functions for creating timer commands
template <typename TTag, typename TContext = NoContext>
TimerCommand<TTag, TContext> make_single_shot_command(std::chrono::milliseconds duration,
                                                       const TContext& context = {}) {
  return {TimerCommand<TTag, TContext>::Command::START_SINGLE_SHOT, duration, context};
}

template <typename TTag, typename TContext = NoContext>
TimerCommand<TTag, TContext> make_periodic_command(std::chrono::milliseconds interval, const TContext& context = {}) {
  return {TimerCommand<TTag, TContext>::Command::START_PERIODIC, interval, context};
}

template <typename TTag, typename TContext = NoContext>
TimerCommand<TTag, TContext> make_cancel_command(const TContext& context = {}) {
  return {TimerCommand<TTag, TContext>::Command::CANCEL, std::chrono::milliseconds{0}, context};
}

template <typename TTag, typename TContext = NoContext>
TimerCommand<TTag, TContext> make_no_op_command(const TContext& context = {}) {
  return {TimerCommand<TTag, TContext>::Command::NONE, std::chrono::milliseconds{0}, context};
}

// Utility function for converting command to string
template <typename TTag, typename TContext>
std::string to_string(const TimerCommand<TTag, TContext>& command) {
  std::ostringstream ss;
  ss << "TimerCommand{";
  ss << "command: ";
  switch (command.command) {
    case TimerCommand<TTag, TContext>::Command::NONE:
      ss << "NONE";
      break;
    case TimerCommand<TTag, TContext>::Command::START_SINGLE_SHOT:
      ss << "START_SINGLE_SHOT";
      break;
    case TimerCommand<TTag, TContext>::Command::START_PERIODIC:
      ss << "START_PERIODIC";
      break;
    case TimerCommand<TTag, TContext>::Command::CANCEL:
      ss << "CANCEL";
      break;
  }
  ss << ", duration: " << command.duration.count() << "ms}";
  return ss.str();
}

}  // namespace actor_core
