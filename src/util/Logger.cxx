/*
 * Copyright (c) 2014, Astron Contributors. All rights reserved.
 * Copyright (c) 2023, Max Rodriguez. All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license. You should have received a copy of this license along
 * with this source code in a file named "COPYING".
 *
 * @file Logger.cxx
 * @author Astron Contributors
 * @date 2023-04-30
 */

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#include <time.h>
#include <iostream>
#include <fstream>
#include "Logger.hxx"

NullStream null_stream; // used to print nothing by compiling out the unwanted messages
NullBuffer null_buffer; // used to print nothing by ignoring the unwanted messages

Logger::Logger(const std::string &log_file, LogSeverity sev, bool console_output) :
    m_buf(&m_buffer, log_file, console_output), m_severity(sev), m_output(&m_buf)
{
}

#ifdef ASTRON_DEBUG_MESSAGES
Logger::Logger() : m_buf(&m_buffer), m_severity(LSEVERITY_DEBUG), m_output(&m_buf), m_color_enabled(true) { }
#else
Logger::Logger() : m_buf(&m_buffer), m_severity(LSEVERITY_INFO), m_output(&m_buf), m_color_enabled(true) { }
#endif // ASTRON_DEBUG_MESSAGES

/* Reset code */
static const char* ANSI_RESET = "\x1b[0m";

/* Normal Colors */
static const char* ANSI_RED = "\x1b[31m";
static const char* ANSI_GREEN = "\x1b[32m";
static const char* ANSI_ORANGE = "\x1b[33m";
static const char* ANSI_YELLOW = "\x1b[33;2m";
//static const char* ANSI_BLUE = "\x1b[34m";
//static const char* ANSI_CYAN = "\x1b[36m";
static const char* ANSI_GREY = "\x1b[37m";

/* Bold Colors */
//static const char* ANSI_BOLD_RED = "\x1b[31;1m";
//static const char* ANSI_BOLD_GREEN = "\x1b[32:1m";
//static const char* ANSI_BOLD_YELLOW = "\x1b[33:1m";
//static const char* ANSI_BOLD_WHITE = "\x1b[37;1m";

/* Dark Colors */
//static const char* ANSI_DARK_RED = "\x1b[31;2m";
//static const char* ANSI_DARK_GREEN = "\x1b[32;2m";
static const char* ANSI_DARK_CYAN = "\x1b[36;2m";
static const char* ANSI_DARK_GREY = "\x1b[37;2m";

const char* Logger::get_severity_color(LogSeverity sev)
{
    switch(sev) {
    case LSEVERITY_FATAL:
    case LSEVERITY_ERROR:
        return ANSI_RED;
    case LSEVERITY_SECURITY:
        return ANSI_ORANGE;
    case LSEVERITY_WARNING:
        return ANSI_YELLOW;
    case LSEVERITY_DEBUG:
    case LSEVERITY_PACKET:
    case LSEVERITY_TRACE:
        return ANSI_DARK_CYAN;
    case LSEVERITY_INFO:
        return ANSI_GREEN;
    default:
        return ANSI_GREY;
    }
}

// log returns an output stream for C++ style stream operations.
LockedLogOutput Logger::log(LogSeverity sev)
{
    const char *severity_label;

    if(sev < m_severity) {
        LockedLogOutput null_out(nullptr, nullptr);
        return null_out;
    }

    switch(sev) {
    case LSEVERITY_PACKET:
        severity_label = "PACKET";
        break;
    case LSEVERITY_TRACE:
        severity_label = "TRACE";
        break;
    case LSEVERITY_DEBUG:
        severity_label = "DEBUG";
        break;
    case LSEVERITY_INFO:
        severity_label = "INFO";
        break;
    case LSEVERITY_WARNING:
        severity_label = "WARNING";
        break;
    case LSEVERITY_SECURITY:
        severity_label = "SECURITY";
        break;
    case LSEVERITY_ERROR:
        severity_label = "ERROR";
        break;
    case LSEVERITY_FATAL:
        severity_label = "FATAL";
        break;
    default:
        severity_label = "UNKNOWN";
        break;
    }

    time_t rawtime;
    time(&rawtime);
    char timetext[1024];
    strftime(timetext, 1024, "%Y-%m-%d %H:%M:%S", localtime(&rawtime));

    LockedLogOutput out(&m_output, &m_lock);

    if(m_color_enabled) {
        out << ANSI_DARK_GREY
            << "[" << timetext << "] "
            << get_severity_color(sev)
            << severity_label
            << ": "
            << ANSI_RESET;
    } else {
        out << "[" << timetext << "] "
            << severity_label
            << ": ";
    }

    return out;
}

// set_color_enabled turns ANSI colorized output on or off.
void Logger::set_color_enabled(bool enabled)
{
    m_color_enabled = enabled;
}

// set_min_serverity sets the lowest severity that will be output to the log.
// Messages with lower severity levels will be discarded.
void Logger::set_min_severity(LogSeverity sev)
{
    m_severity = sev;
}

// get_min_severity returns the current minimum severity that will be logged by the logger.
LogSeverity Logger::get_min_severity()
{
    return m_severity;
}

LoggerBuf::LoggerBuf(std::string *buffer) :
    std::streambuf(), m_buffer(buffer), m_has_file(false), m_output_to_console(true)
{
#ifndef __EMSCRIPTEN__
    std::cout << std::unitbuf;
#endif // __EMSCRIPTEN__
}

LoggerBuf::LoggerBuf(std::string *buffer, const std::string &file_name, bool output_to_console) :
    m_buffer(buffer), m_file(file_name), m_has_file(true), m_output_to_console(output_to_console)
{
#ifndef __EMSCRIPTEN__
    if(m_output_to_console) {
        std::cout << std::unitbuf;
    }
#endif // __EMSCRIPTEN__

    if(!m_file.is_open()) {
        m_has_file = false;
    }
}

int LoggerBuf::overflow(int c)
{
    if(m_output_to_console) {
#ifdef __EMSCRIPTEN__
        m_buffer->append(std::to_string(c).c_str());
#else
        std::cout.put(c);
#endif // __EMSCRIPTEN__
    }
    if(m_has_file) {
        m_file.put(c);
    }
    return c;
}

// overrides std::basic_streambuf::xsputn method
std::streamsize LoggerBuf::xsputn(const char* s, std::streamsize n)
{
    if(m_output_to_console) {
#ifdef __EMSCRIPTEN__
        m_buffer->append(s);
#else
        std::cout.write(s, n);
#endif // __EMSCRIPTEN__
    }
    if(m_has_file) {
        m_file.write(s, n);
    }
    return n;
}

#ifdef __EMSCRIPTEN__
/*
 * We have to explicitly call the `js_flush()` method to make the `emscripten_log()` call.
 * The original Astron logger makes continuous writes to the `std::cout` stream as it
 * forms the log output. Targeting Web Assembly with Emscripten, `std::cout` isn't printed
 * to the Javascript console unless we call `emscripten_log()` via the Emscripten API ("emscripten.h").
 *
 * The astron.libwasm implementation uses a modified Logger module that, instead of writing directly
 * to the `std::cout` stream, it writes into a `std::string` buffer. Then, once `js_flush()` is
 * called, `emscripten_log()` is called (passing the string buffer) and clears the string buffer.
 */
void Logger::js_flush()
{
    if(m_buffer.empty()) return;  // make sure m_buffer isn't empty
    emscripten_log(EM_LOG_CONSOLE, m_buffer.c_str());
    m_buffer.clear();
}
#endif // __EMSCRIPTEN__

// In the Astron daemon source, this is defined in `src/global.cpp`.
std::unique_ptr<Logger> g_logger(new Logger());