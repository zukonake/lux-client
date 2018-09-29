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

void server_command(char const* str) {
    if(send_command(str) != LUX_OK) {
        print("failed to send command");
    }
}

void reload_program() {
    map_reload_program();
}

void quit() {
    client_quit();
}

}
