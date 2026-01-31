#include "snake/logger.hpp"

#include <fstream>
#include <mutex>

namespace snake {

namespace {
std::ofstream log_file;
std::mutex log_mutex;
}  // namespace

void Logger::initialize(const std::string& log_file_path) {
  std::lock_guard<std::mutex> lock(log_mutex);
  if (log_file.is_open()) {
    log_file.close();
  }
  log_file.open(log_file_path, std::ios::out | std::ios::app);
}

void Logger::shutdown() {
  std::lock_guard<std::mutex> lock(log_mutex);
  if (log_file.is_open()) {
    log_file.close();
  }
}

void Logger::log(const std::string& message) {
  std::lock_guard<std::mutex> lock(log_mutex);
  if (log_file.is_open()) {
    log_file << message;
    log_file.flush();
  }
}

}  // namespace snake
