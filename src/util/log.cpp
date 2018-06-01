#include <iostream>
#include <string>
//
#include "log.hpp"

namespace util
{

void log(std::string const &prefix, Level level, std::string const &msg)
{
    if(level > logger_report_level)
    {
        return;
    }
    std::string output = prefix == "" ? "" : "[" + prefix + "] ";
    switch(level)
    {
    case OFF:
        return;

    case CRITICAL:
        output += "CRIT:  ";
        break;

    case ERROR:
        output += "ERROR: ";
        break;

    case WARN:
        output += "WARN:  ";
        break;

    case INFO:
        output += "INFO:  ";
        break;

    case DEBUG:
        output += "DEBUG: ";
        break;

    case TRACE:
        output += "TRACE: ";
        break;
    }
    output += msg;
    std::cout << output << std::endl;
}

Level logger_report_level(ALL);

}
