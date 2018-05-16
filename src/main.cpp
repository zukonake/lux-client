#include <ncurses.h>
//
#include <client.hpp>

int main()
{
    initscr();
    Client client("localhost", 30337, 128.0);
    endwin();
}
