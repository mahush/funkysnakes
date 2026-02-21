#include "actor-core/timer/asio_timer_core.hpp"

namespace actor_core {

AsioTimerCore::AsioTimerCore(asio::io_context& io, asio::strand<asio::io_context::executor_type>& strand)
    : timer_(io), strand_(strand) {}

void AsioTimerCore::subscribe(std::weak_ptr<ProcessorInterface> processor) {
  std::lock_guard<std::mutex> lock(mutex_);
  processor_ = std::move(processor);
}

void AsioTimerCore::schedule_single_shot(std::chrono::milliseconds duration_from_now_until_elapsed) {
  std::lock_guard<std::mutex> lock(mutex_);

  is_periodic_ = false;
  interval_ = duration_from_now_until_elapsed;
  is_scheduled_ = true;

  schedule_timer(duration_from_now_until_elapsed);
}

void AsioTimerCore::schedule_periodic(std::chrono::milliseconds interval) {
  std::lock_guard<std::mutex> lock(mutex_);

  is_periodic_ = true;
  interval_ = interval;
  is_scheduled_ = true;

  schedule_timer(interval);
}

void AsioTimerCore::cancel_scheduled() {
  std::lock_guard<std::mutex> lock(mutex_);

  is_scheduled_ = false;
  timer_.cancel();
}

bool AsioTimerCore::is_scheduled() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return is_scheduled_;
}

bool AsioTimerCore::tryTakeElapsedEvent() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (elapsed_count_ > 0) {
    --elapsed_count_;
    return true;
  }
  return false;
}

void AsioTimerCore::schedule_timer(std::chrono::milliseconds duration) {
  timer_.expires_after(duration);
  timer_.async_wait(asio::bind_executor(strand_, [weak_self = weak_from_this()](const asio::error_code& ec) {
                      if (auto self = weak_self.lock()) {
                        self->on_timer_expired(ec);
                      }
                    }));
}

void AsioTimerCore::on_timer_expired(const asio::error_code& ec) {
  if (ec == asio::error::operation_aborted) {
    // Timer was cancelled
    return;
  }

  if (ec) {
    // Error occurred
    return;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);

    // Increment elapsed event counter
    ++elapsed_count_;

    // If periodic, reschedule
    if (is_periodic_ && is_scheduled_) {
      schedule_timer(interval_);
    } else {
      is_scheduled_ = false;
    }
  }

  // Notify processor that timer event occurred (outside lock)
  if (auto processor = processor_.lock()) {
    asio::post(strand_, [processor]() { processor->processInputs(); });
  }
}

}  // namespace actor_core
