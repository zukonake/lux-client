#include <map.hpp>
#include <console.hpp>

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
    print("unimplemented");
}

}
