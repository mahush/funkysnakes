#pragma once

#include <termios.h>

#include <asio.hpp>
#include <deque>
#include <funkyactors/input_source.hpp>
#include <funkyactors/processor_interface.hpp>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace snake {

/**
 * @brief Async stdin reader that follows the actor-core pattern
 *
 * Mirrors the Topic/Subscription design:
 * - Async reads from stdin (like Topic publishes messages)
 * - Buffers characters in a queue (like Subscription)
 * - Notifies processor when chars are available (like Topic notifies subscribers)
 * - Pull-based interface via tryTakeChar() (like tryTakeMessage())
 *
 * Implements InputSource<char> for unified processing.
 */
class StdinReader : public funkyactors::InputSource<char> {
 public:
  explicit StdinReader(asio::io_context& io);
  ~StdinReader();

  // Non-copyable, non-movable
  StdinReader(const StdinReader&) = delete;
  StdinReader& operator=(const StdinReader&) = delete;
  StdinReader(StdinReader&&) = delete;
  StdinReader& operator=(StdinReader&&) = delete;

  /**
   * @brief Subscribe a processor to receive notifications when chars are available
   * @param processor The processor to notify (weak_ptr for automatic cleanup)
   * @param strand The strand to post notifications on
   */
  void subscribe(std::weak_ptr<funkyactors::ProcessorInterface> processor,
                 asio::strand<asio::io_context::executor_type> strand);

  /**
   * @brief Start reading from stdin
   */
  void startReading();

  /**
   * @brief Stop reading from stdin
   */
  void stopReading();

  /**
   * @brief Check if currently reading
   */
  bool isReading() const { return is_reading_; }

  // InputSource<char> interface implementation
  std::optional<char> tryTake() override { return tryTakeChar(); }

  bool hasInputItems() const override { return hasChars(); }

  /**
   * @brief Try to take the next character from queue (non-blocking)
   * @return The next char if available, std::nullopt otherwise
   *
   * This is the pull-based interface, symmetric to Subscription::tryTakeMessage()
   */
  std::optional<char> tryTakeChar();

  /**
   * @brief Check if there are pending characters
   */
  bool hasChars() const;

 private:
  void scheduleRead();
  void onCharRead(char ch);
  void enableRawMode();
  void disableRawMode();

  asio::posix::stream_descriptor stdin_;
  std::vector<char> read_buffer_;

  // Queue and synchronization (like Subscription)
  mutable std::mutex mutex_;
  std::deque<char> queue_;
  static constexpr size_t MAX_QUEUE_SIZE = 100;

  // Processor notification (like Topic)
  std::weak_ptr<funkyactors::ProcessorInterface> processor_;
  asio::strand<asio::io_context::executor_type> strand_;

  // State
  bool is_reading_{false};
  termios orig_termios_{};
  bool raw_mode_enabled_{false};
};

}  // namespace snake
