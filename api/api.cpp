#include <console.hpp>

extern "C" {

void clear() {
    console_clear();
}

void print(char const* str) {
    console_print(str);
}

}
