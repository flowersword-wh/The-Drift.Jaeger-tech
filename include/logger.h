#pragma once

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>

#define LOG_COLOR_RED "\033[1;31m"
#define LOG_COLOR_GREEN "\033[1;32m"
#define LOG_COLOR_YELLOW "\033[1;33m"
#define LOG_COLOR_BLUE "\033[1;34m"
#define LOG_COLOR_RESET "\033[0m"
#define LOG_TS_COLOR "\033[90m"

class Logger {
public:
  Logger() : startTime_(std::chrono::steady_clock::now()) {}

  void info(std::string_view message) {
    write(LOG_COLOR_GREEN, "INFO", message);
  }

  void warning(std::string_view message) {
    write(LOG_COLOR_YELLOW, "WARNING", message);
  }

  void debug(std::string_view message) {
    write(LOG_COLOR_BLUE, "DEBUG", message);
  }

  void error(std::string_view message) {
    write(LOG_COLOR_RED, "ERROR", message);
  }

private:
  std::chrono::steady_clock::time_point startTime_;
  double get_timer() const {
    const auto now = std::chrono::steady_clock::now();
    const auto duration = now - startTime_;
    return std::chrono::duration<double, std::milli>(duration).count();
  }

  void write(const char *color, const char *level,
             std::string_view message) const {
    std::cout << std::fixed << std::setprecision(3)
              << LOG_TS_COLOR << "[ " << get_timer() << " ms] "
              << LOG_COLOR_RESET << color << "[" << level << "] "
              << LOG_COLOR_RESET << message << std::endl;
  }
};
