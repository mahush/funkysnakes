#include "snake/stdin_reader.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <iostream>
#include <system_error>

namespace snake {

namespace {

/**
 * @brief Open a descriptor reading the same input as stdin, with its own open file description
 *
 * Asio switches every descriptor it waits on to O_NONBLOCK, and that flag belongs to the open file
 * description. A terminal's stdin and stdout share one description, so reading STDIN_FILENO through
 * Asio would make stdout non-blocking as well: a write to a full terminal then fails with EAGAIN and
 * std::cout drops all further output. Opening the terminal anew keeps the flag away from stdout.
 *
 * @return A descriptor owned by the caller: the terminal opened anew if stdin is one, otherwise a duplicate of stdin
 * @throws std::system_error if stdin is a terminal that cannot be opened anew; a duplicate would share stdout's
 *         description again
 */
int openInputDescriptor() {
  if (!isatty(STDIN_FILENO)) {
    // A pipe or file never shares its description with stdout
    return dup(STDIN_FILENO);
  }
  const char* terminal_path = ttyname(STDIN_FILENO);
  int terminal_fd = terminal_path ? open(terminal_path, O_RDONLY | O_NOCTTY | O_CLOEXEC) : -1;
  if (terminal_fd == -1) {
    throw std::system_error{errno, std::generic_category(), "cannot open the stdin terminal anew for key input"};
  }
  return terminal_fd;
}

}  // namespace

StdinReader::StdinReader(asio::io_context& io)
    : stdin_{io, openInputDescriptor()}, read_buffer_{1}, strand_{asio::make_strand(io)} {}

StdinReader::~StdinReader() { stopReading(); }

void StdinReader::subscribe(std::weak_ptr<funkyactors::ProcessorInterface> processor,
                            asio::strand<asio::io_context::executor_type> strand) {
  processor_ = processor;
  strand_ = strand;
}

void StdinReader::startReading() {
  if (is_reading_) {
    return;  // Already reading
  }

  enableRawMode();
  is_reading_ = true;

  std::cout << "[StdinReader] Started reading from stdin (async mode)\n";
  std::cout << "[StdinReader] Player 1 (Snake A): w=UP, a=LEFT, s=DOWN, d=RIGHT\n";
  std::cout << "[StdinReader] Player 2 (Snake B): Arrow keys (↑ ↓ ← →)\n";
  std::cout << "[StdinReader] Press 'p' to pause/resume, 'q' to quit\n";

  scheduleRead();
}

void StdinReader::stopReading() {
  if (!is_reading_) {
    return;
  }

  is_reading_ = false;
  stdin_.cancel();
  disableRawMode();

  std::cout << "\n[StdinReader] Stopped reading from stdin\n";
}

std::optional<char> StdinReader::tryTakeChar() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (queue_.empty()) {
    return std::nullopt;
  }
  char ch = queue_.front();
  queue_.pop_front();
  return ch;
}

bool StdinReader::hasChars() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return !queue_.empty();
}

void StdinReader::scheduleRead() {
  if (!is_reading_) {
    return;
  }

  // Async read one character
  asio::async_read(stdin_, asio::buffer(read_buffer_), [this](std::error_code ec, std::size_t) {
    if (ec) {
      is_reading_ = false;
      return;
    }

    char ch = read_buffer_[0];
    onCharRead(ch);

    // Schedule next read
    scheduleRead();
  });
}

void StdinReader::onCharRead(char ch) {
  // Enqueue character (like Subscription::enqueue)
  bool was_empty;
  {
    std::lock_guard<std::mutex> lock(mutex_);

    // If queue is full, drop oldest char
    if (queue_.size() >= MAX_QUEUE_SIZE) {
      queue_.pop_front();
    }

    was_empty = queue_.empty();
    queue_.push_back(ch);
  }

  // Notify processor only if queue was empty (like Topic::publish)
  if (was_empty) {
    if (auto proc = processor_.lock()) {
      asio::post(strand_, [proc] { proc->processInputs(); });
    }
  }
}

void StdinReader::enableRawMode() {
  // Get current terminal attributes
  if (tcgetattr(STDIN_FILENO, &orig_termios_) == -1) {
    std::cerr << "[StdinReader] Failed to get terminal attributes\n";
    return;
  }

  termios raw = orig_termios_;

  // Disable canonical mode (line buffering) and echo
  raw.c_lflag &= ~(ICANON | ECHO);

  // Set minimum characters to read and timeout
  raw.c_cc[VMIN] = 1;   // Read one character at a time
  raw.c_cc[VTIME] = 0;  // No timeout

  // Apply new settings
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
    std::cerr << "[StdinReader] Failed to set raw mode\n";
    return;
  }

  raw_mode_enabled_ = true;
}

void StdinReader::disableRawMode() {
  if (raw_mode_enabled_) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios_);
    raw_mode_enabled_ = false;
  }
}

}  // namespace snake
