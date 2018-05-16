#include "tick_clock.hpp"

namespace util
{

TickClock::TickClock(Duration rate) :
    cycle(0),
    rate(rate),
    before(clock.now())
{

}

void TickClock::reset()
{
    cycle = Duration(0);
}

void TickClock::start()
{
    before = clock.now();
}

void TickClock::stop()
{
    cycle += Duration(clock.now() - before);
}

void TickClock::set_rate(TickClock::Duration value)
{
    rate = value;
}

typename TickClock::Duration TickClock::synchronize()
{
    Duration delta = rate - cycle;
    if(cycle < rate)
    {
        std::this_thread::sleep_for(delta);
        reset();
    }
    cycle = Duration::zero();
    return delta;
}

}
