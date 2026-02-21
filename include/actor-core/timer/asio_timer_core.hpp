#pragma once

#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>

#include "actor-core/processor_interface.hpp"
#include "actor-core/timer/timer_core.hpp"

namespace actor_core {

/**
 * @brief Asio-based implementation of ITimerCore
 *
 * Uses asio::steady_timer for timer functionality.
 * Thread-safe for use across multiple strands.
 * Notifies a processor when timer events occur.
 */
class AsioTimerCore : public ITimerCore, public std::enable_shared_from_this<AsioTimerCore> {
 public:
  /**
   * @brief Construct a new Asio Timer Core
   * @param io The io_context for async operations
   * @param strand The strand to execute timer callbacks on
   */
  AsioTimerCore(asio::io_context& io, asio::strand<asio::io_context::executor_type>& strand);

  void subscribe(std::weak_ptr<ProcessorInterface> processor) override;
  void schedule_single_shot(std::chrono::milliseconds duration_from_now_until_elapsed) override;
  void schedule_periodic(std::chrono::milliseconds interval) override;
  void cancel_scheduled() override;
  bool is_scheduled() const override;
  bool tryTakeElapsedEvent() override;

 private:
  void schedule_timer(std::chrono::milliseconds duration);
  void on_timer_expired(const asio::error_code& ec);

  asio::steady_timer timer_;
  asio::strand<asio::io_context::executor_type>& strand_;
  std::weak_ptr<ProcessorInterface> processor_;

  mutable std::mutex mutex_;
  bool is_periodic_{false};
  std::chrono::milliseconds interval_{0};
  bool is_scheduled_{false};
  size_t elapsed_count_{0};
};

}  // namespace actor_core
