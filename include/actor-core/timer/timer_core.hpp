#pragma once

#include <chrono>
#include <memory>

namespace actor_core {

// Forward declaration
class ProcessorInterface;

/**
 * @brief Interface for timer core functionality
 *
 * This interface abstracts the underlying timer implementation to support
 * both real asio timer and mock implementations for testing.
 */
class ITimerCore {
 public:
  virtual ~ITimerCore() = default;

  /**
   * @brief Subscribe a processor to receive timer event notifications
   * @param processor The processor (weak_ptr for automatic cleanup)
   */
  virtual void subscribe(std::weak_ptr<ProcessorInterface> processor) = 0;

  /**
   * @brief Schedule a single-shot timer
   * @param duration_from_now_until_elapsed Duration from now until the timer elapses
   */
  virtual void schedule_single_shot(std::chrono::milliseconds duration_from_now_until_elapsed) = 0;

  /**
   * @brief Schedule a periodic timer
   * @param interval Interval between timer events
   */
  virtual void schedule_periodic(std::chrono::milliseconds interval) = 0;

  /**
   * @brief Cancel any scheduled timer
   */
  virtual void cancel_scheduled() = 0;

  /**
   * @brief Check if timer is currently scheduled
   * @return true if timer is scheduled, false otherwise
   */
  virtual bool is_scheduled() const = 0;

  /**
   * @brief Try to take the next elapsed event (non-blocking)
   * @return true if an event was taken, false if no events are pending
   */
  virtual bool tryTakeElapsedEvent() = 0;
};

using TimerCorePtr = std::shared_ptr<ITimerCore>;

}  // namespace actor_core
