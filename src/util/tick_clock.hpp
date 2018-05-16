#pragma once

#include <thread>
#include <chrono>

namespace util
{

class TickClock
{
    public:
    typedef std::chrono::duration<double> Duration;
    typedef std::chrono::time_point<std::chrono::steady_clock> TimePoint;

    TickClock(Duration rate);

    void reset();
    void start();
    void stop();
    void set_rate(Duration value);
    Duration get();
    Duration synchronize();
    private:
    std::chrono::steady_clock clock;
    Duration  cycle;
    Duration  rate;
    TimePoint before;
};

}
