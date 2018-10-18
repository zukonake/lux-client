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

void bind_key(char key, char const* input) {
    if(console_bind_key(key, input) != LUX_OK) {
        print("failed to bind key");
    }
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
