#pragma once

#include <string>

namespace util
{

enum Level
{
    OFF,
    CRITICAL,
    ERROR,
    WARN,
    INFO,
    DEBUG,
    TRACE,
    ALL = TRACE
};

extern Level logger_report_level;

void log(std::string const &prefix, Level level, std::string const &msg);

}
