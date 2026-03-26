#pragma once

#include <bitset>
#include <cstddef>
#include <mutex>
#include <sstream>
#include "log_callback.h"

#if defined(ANDROID)
#include <android/log.h>
#else
#include <ctime>
#if defined(WINDOWS)
#include <iostream> // std::cerr on Windows (no fork concern)
#else
#include <unistd.h> // for write() — fork-safe output
#endif
#endif

#if !defined(WINDOWS)
// Remove path and extract only filename.
#define FILENAME \
    (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)
#else
#define FILENAME __FILE__
#endif

#define call_user_callback(...) call_user_callback_located(FILENAME, __LINE__, __VA_ARGS__)

#define LogDebug() LogDebugDetailed(FILENAME, __LINE__)
#define LogInfo() LogInfoDetailed(FILENAME, __LINE__)
#define LogWarn() LogWarnDetailed(FILENAME, __LINE__)
#define LogErr() LogErrDetailed(FILENAME, __LINE__)

namespace mavsdk {

// Mutex moved to log.cpp to avoid inlining issues
std::mutex& get_log_mutex();

std::ostream& operator<<(std::ostream& os, std::byte b);

enum class Color { Red, Green, Yellow, Blue, Gray, Reset };

void set_color(Color color);

class LogDetailed {
public:
    LogDetailed(const char* filename, int filenumber) :
        _lock_guard(get_log_mutex()),
        _s(),
        _caller_filename(filename),
        _caller_filenumber(filenumber)
    {}

    template<typename T> LogDetailed& operator<<(const T& x)
    {
        _s << x;
        return *this;
    }

    virtual ~LogDetailed()
    {
        if (log::get_callback() &&
            log::get_callback()(_log_level, _s.str(), _caller_filename, _caller_filenumber)) {
            return;
        }

#if ANDROID
        switch (_log_level) {
            case log::Level::Debug:
                __android_log_print(ANDROID_LOG_DEBUG, "Mavsdk", "%s", _s.str().c_str());
                break;
            case log::Level::Info:
                __android_log_print(ANDROID_LOG_INFO, "Mavsdk", "%s", _s.str().c_str());
                break;
            case log::Level::Warn:
                __android_log_print(ANDROID_LOG_WARN, "Mavsdk", "%s", _s.str().c_str());
                break;
            case log::Level::Err:
                __android_log_print(ANDROID_LOG_ERROR, "Mavsdk", "%s", _s.str().c_str());
                break;
        }
        // Unused:
        (void)_caller_filename;
        (void)_caller_filenumber;
#else
        // Build the complete log line in a stringstream, then output with a
        // single write() call.  This avoids std::cout whose locale/codecvt
        // internals are not safe after fork() (e.g. uvicorn --reload).
        std::ostringstream line;

        switch (_log_level) {
            case log::Level::Debug:
                line << "\x1b[32m"; // green
                break;
            case log::Level::Info:
                line << "\x1b[34m"; // blue
                break;
            case log::Level::Warn:
                line << "\x1b[33m"; // yellow
                break;
            case log::Level::Err:
                line << "\x1b[31m"; // red
                break;
        }

        // Time output taken from:
        // https://stackoverflow.com/questions/16357999#answer-16358264
        time_t rawtime;
        time(&rawtime);
        struct tm* timeinfo = localtime(&rawtime);
        char time_buffer[10]{}; // We need 8 characters + \0
        strftime(time_buffer, sizeof(time_buffer), "%I:%M:%S", timeinfo);
        line << "[" << time_buffer;

        switch (_log_level) {
            case log::Level::Debug:
                line << "|Debug] ";
                break;
            case log::Level::Info:
                line << "|Info ] ";
                break;
            case log::Level::Warn:
                line << "|Warn ] ";
                break;
            case log::Level::Err:
                line << "|Error] ";
                break;
        }

        line << "\x1b[0m"; // reset

        line << _s.str();
        line << " (" << _caller_filename << ":" << std::dec << _caller_filenumber << ")\n";

#if defined(WINDOWS)
        // On Windows, fall back to std::cerr (no fork concern)
        std::cerr << line.str();
#else
        // POSIX write() is fork-safe (unlike std::cout whose locale internals are not)
        std::string out = line.str();
        auto written [[maybe_unused]] = ::write(STDOUT_FILENO, out.c_str(), out.size());
#endif
#endif
    }

    LogDetailed(const mavsdk::LogDetailed&) = delete;
    void operator=(const mavsdk::LogDetailed&) = delete;

protected:
    log::Level _log_level = log::Level::Debug;

private:
    std::lock_guard<std::mutex> _lock_guard;

    std::stringstream _s;
    const char* _caller_filename;
    int _caller_filenumber;
};

class LogDebugDetailed : public LogDetailed {
public:
    LogDebugDetailed(const char* filename, int filenumber) : LogDetailed(filename, filenumber)
    {
        _log_level = log::Level::Debug;
    }
};

class LogInfoDetailed : public LogDetailed {
public:
    LogInfoDetailed(const char* filename, int filenumber) : LogDetailed(filename, filenumber)
    {
        _log_level = log::Level::Info;
    }
};

class LogWarnDetailed : public LogDetailed {
public:
    LogWarnDetailed(const char* filename, int filenumber) : LogDetailed(filename, filenumber)
    {
        _log_level = log::Level::Warn;
    }
};

class LogErrDetailed : public LogDetailed {
public:
    LogErrDetailed(const char* filename, int filenumber) : LogDetailed(filename, filenumber)
    {
        _log_level = log::Level::Err;
    }
};

} // namespace mavsdk
