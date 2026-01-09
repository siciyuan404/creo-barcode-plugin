#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include "error_codes.h"

namespace creo_barcode {

class Logger {
public:
    static Logger& getInstance();
    
    // Logging methods
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    void error(const std::string& message, const ErrorInfo& err);
    
    // Configuration
    void setLogFilePath(const std::string& path);
    void setConsoleOutput(bool enabled) { consoleOutput_ = enabled; }
    
private:
    Logger();
    ~Logger();
    
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    void writeToFile(const std::string& level, const std::string& message);
    std::string getCurrentTimestamp();
    
    std::string logFilePath_;
    std::ofstream logFile_;
    std::mutex mutex_;
    bool consoleOutput_ = true;
};

// Convenience macros - DISABLED to avoid static initialization issues in DLL
// #define LOG_INFO(msg) creo_barcode::Logger::getInstance().info(msg)
// #define LOG_WARNING(msg) creo_barcode::Logger::getInstance().warning(msg)
// #define LOG_ERROR(msg) creo_barcode::Logger::getInstance().error(msg)

// No-op logging macros for DLL safety
#define LOG_INFO(msg) ((void)0)
#define LOG_WARNING(msg) ((void)0)
#define LOG_ERROR(msg) ((void)0)

} // namespace creo_barcode

#endif // LOGGER_H
