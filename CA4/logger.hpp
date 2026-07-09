#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>

inline void log_msg(const std::string& msg) {
    // Get current time
    auto now = std::chrono::system_clock::now();
    auto timer = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    // Print with [HH:MM:SS.mmm] format
    std::cout << "[" << std::put_time(std::localtime(&timer), "%T") << "." 
              << std::setfill('0') << std::setw(3) << ms.count() << "] " << msg << std::endl;
}

#endif // LOGGER_HPP