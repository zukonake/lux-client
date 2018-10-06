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

void wfon() {
#ifdef LUX_GLES_2_0
    print("unsupported in OpenGL ES 2.0");
#else
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#endif
}

void wfoff() {
#ifdef LUX_GLES_2_0
    print("unsupported in OpenGL ES 2.0");
#else
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif
}

}
