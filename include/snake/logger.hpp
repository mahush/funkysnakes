#pragma once

#include <string>

namespace snake {

/**
 * @brief Simple logging utility that writes to a file
 *
 * Provides a static log function that writes messages to a log file.
 * Thread-safe for basic use cases.
 */
class Logger {
 public:
  /**
   * @brief Log a message to the log file
   *
   * @param message Message to log
   */
  static void log(const std::string& message);

  /**
   * @brief Initialize the logger with a log file path
   *
   * @param log_file_path Path to the log file
   */
  static void initialize(const std::string& log_file_path);

  /**
   * @brief Close the log file
   */
  static void shutdown();

 private:
  Logger() = default;
};

}  // namespace snake
