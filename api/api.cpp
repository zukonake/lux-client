#include <map.hpp>
#include <console.hpp>
#include <client.hpp>

extern "C" {

void clear() {
    console_clear();
}

void print(char const* str) {
    console_print(str);
}

void reload_program() {
    map_reload_program();
}

void quit() {
    client_quit();
}

}
