#pragma once

#include <chrono>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <optional>
#include <mutex>
#include <thread>
#include <iostream>

using namespace std::literals;

#define LOG(...) Logger::GetInstance().Log(__VA_ARGS__)

class Logger {
private:
    std::ofstream log_file_;
    std::string current_log_file_name_;
    std::mutex mutex_;
    std::optional<std::chrono::system_clock::time_point> manual_ts_;

    Logger() {
        std::lock_guard<std::mutex> lock(mutex_);
        OpenLogFile();
    }

    Logger(const Logger&) = delete;

    auto GetTime() const {
        if (manual_ts_) {
            return *manual_ts_;
        }
        return std::chrono::system_clock::now();
    }

    auto GetTimeStamp() const {
        const auto now = GetTime();
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        return std::put_time(std::gmtime(&t_c), "%F %T");
    }

    std::string GetFileTimeStamp() const {
        const auto now = GetTime();
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&t_c), "%Y_%m_%d");
        return ss.str();
    }

    void OpenLogFile() {
        const std::string log_file_name = "/var/log/sample_log_" + GetFileTimeStamp() + ".log";
        if (log_file_name != current_log_file_name_) {
            if (log_file_.is_open()) {
                log_file_.close();
            }
            current_log_file_name_ = log_file_name;
            log_file_.open(current_log_file_name_, std::ios::app);
        }
    }

public:
    static Logger& GetInstance() {
        static Logger instance;
        return instance;
    }

    template<class... Ts>
    void Log(const Ts&... args) {
        std::lock_guard<std::mutex> lock(mutex_);
        OpenLogFile();
        log_file_ << GetTimeStamp() << ": "sv ;
        (log_file_ << ... << args);
        log_file_ << std::endl;
    }

    void SetTimestamp(std::chrono::system_clock::time_point ts) {
        std::lock_guard<std::mutex> lock(mutex_);
        manual_ts_ = ts;
    }
};
