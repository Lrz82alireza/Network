#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <iomanip>

class Logger {
private:
    std::ofstream event_file;
    bool write_to_file;

public:
    Logger(const std::string& filepath = "") {
        if (!filepath.empty()) {
            event_file.open(filepath, std::ios::out | std::ios::trunc);
            write_to_file = event_file.is_open();
        } else {
            write_to_file = false;
        }
    }

    ~Logger() {
        if (write_to_file) {
            event_file.close();
        }
    }

    void log(const std::string& msg) {
        auto now = std::chrono::system_clock::now();
        auto timer = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        // Format: [HH:MM:SS.mmm]
        char buffer[30];
        std::strftime(buffer, sizeof(buffer), "%T", std::localtime(&timer));
        
        std::string time_str = std::string(buffer) + "." + 
            (ms.count() < 100 ? (ms.count() < 10 ? "00" : "0") : "") + std::to_string(ms.count());

        std::string full_msg = "[" + time_str + "] " + msg;
        
        // Print to terminal
        std::cout << full_msg << std::endl;
        
        // Write to events.log if enabled
        if (write_to_file) {
            event_file << full_msg << "\n";
            event_file.flush();
        }
    }
};

#endif // LOGGER_HPP