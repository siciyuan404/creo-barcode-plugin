#include "logger.h"
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <mutex>

namespace creo_barcode {

Logger& Logger::getInstance() {
    // Use a pointer to avoid static initialization order issues in DLL
    static Logger* instance = nullptr;
    static std::once_flag flag;
    std::call_once(flag, []() {
        instance = new Logger();
    });
    return *instance;
}

Logger::Logger() : consoleOutput_(false) {
    // Don't open log file in constructor - defer until first write
    // This avoids potential issues during DLL load
#ifdef _WIN32
    const char* temp = std::getenv("TEMP");
    if (temp) {
        logFilePath_ = std::string(temp) + "\\creo_barcode.log";
    } else {
        logFilePath_ = "creo_barcode.log";
    }
#else
    logFilePath_ = "/tmp/creo_barcode.log";
#endif
}

Logger::~Logger() {
    if (logFile_.is_open()) {
        logFile_.close();
    }
}

void Logger::setLogFilePath(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (logFile_.is_open()) {
        logFile_.close();
    }
    logFilePath_ = path;
}

std::string Logger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &time);
#else
    localtime_r(&time, &tm_buf);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void Logger::writeToFile(const std::string& level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string timestamp = getCurrentTimestamp();
    std::string logLine = "[" + timestamp + "] [" + level + "] " + message;
    
    if (consoleOutput_) {
        std::cout << logLine << std::endl;
    }
    
    if (!logFile_.is_open()) {
        logFile_.open(logFilePath_, std::ios::app);
    }
    
    if (logFile_.is_open()) {
        logFile_ << logLine << std::endl;
        logFile_.flush();
    }
}

void Logger::info(const std::string& message) {
    writeToFile("INFO", message);
}

void Logger::warning(const std::string& message) {
    writeToFile("WARNING", message);
}

void Logger::error(const std::string& message) {
    writeToFile("ERROR", message);
}

void Logger::error(const std::string& message, const ErrorInfo& err) {
    std::string fullMessage = message + " - Code: " + errorCodeToString(err.code);
    if (!err.message.empty()) {
        fullMessage += " - " + err.message;
    }
    if (!err.details.empty()) {
        fullMessage += " (" + err.details + ")";
    }
    writeToFile("ERROR", fullMessage);
}

} // namespace creo_barcode
