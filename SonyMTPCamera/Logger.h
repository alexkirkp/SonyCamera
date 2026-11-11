#pragma once
#include "pch.h"

#include <atomic>

// The level is checked before the arguments are evaluated, so a filtered-out call does no work.
// (if/else form is safe inside unbraced if/else statements.)
#define LOG_AT(lvl, format, ...) if (!logger.IsEnabled(lvl)) {} else logger.Log(lvl, format, __VA_ARGS__)
#define LOGTRACE(format, ...) LOG_AT(Logger::LogLevel::Trace, format, __VA_ARGS__)
#define LOGDEBUG(format, ...) LOG_AT(Logger::LogLevel::Debug, format, __VA_ARGS__)
#define LOGINFO(format, ...) LOG_AT(Logger::LogLevel::Info, format, __VA_ARGS__)
#define LOGWARN(format, ...) LOG_AT(Logger::LogLevel::Warn, format, __VA_ARGS__)
#define LOGERROR(format, ...) LOG_AT(Logger::LogLevel::Error, format, __VA_ARGS__)

class Logger
{
public:
    enum class LogLevel
    {
        Error = 5, Warn = 4, Info = 3, Debug = 2, Trace = 1
    };

    Logger();
    ~Logger();

    static void SetLogFilename(std::wstring filename);

    void SetLogLevel(LogLevel level);
    bool IsEnabled(LogLevel level) const { return level >= m_level.load(std::memory_order_relaxed); }
    void Log(LogLevel level, const WCHAR* format, ...);

    static std::wstring s_logfileName;
    static HANDLE s_logFileHandle;

protected:

private:
    HANDLE m_serializationMutex = INVALID_HANDLE_VALUE;
    std::atomic<LogLevel> m_level{ LogLevel::Error };
};

extern Logger logger;
