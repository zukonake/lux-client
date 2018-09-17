#include "lux_opengl.hpp"

void check_opengl_error()
{
    GLenum error = glGetError();
    if(error != GL_NO_ERROR)
    {
        char const* str;
        switch(error)
        {
            case GL_INVALID_ENUM: str = "invalid enum"; break;
            case GL_INVALID_VALUE: str = "invalid value"; break;
            case GL_INVALID_OPERATION: str = "invalid operation"; break;
            case GL_OUT_OF_MEMORY: str = "out of memory"; break;
            default: str = "unknown"; break;
        }
        LUX_FATAL("OpenGL error: %d - %s", error, str);
    }
}

GLuint load_shader(GLenum type, char const* path) {
    GLuint id = glCreateShader(type);

    U8* str;
    {   std::ifstream file(path);
        file.seekg(0, file.end);
        long len = file.tellg();
        file.seekg(0, file.beg);

        str = lux_alloc<U8>((SizeT)len + 1);
        file.read(str, len);
        file.close();
    }
    str[(SizeT)len] = '\0';
    glShaderSource(id, 1, &str, nullptr);
    glCompileShader(id);
    lux_free(str);

    {   int success;
        glGetShaderiv(id, GL_COMPILE_STATUS, &success);
        if(!success) {
            static constexpr SizeT OPENGL_LOG_SIZE = 512;
            char log[OPENGL_LOG_SIZE];
            glGetShaderInfoLog(id, OPENGL_LOG_SIZE, nullptr, log);
            LUX_FATAL("shader compilation error: \n%s", log);
        }
    }
    return id;
}

GLuint load_program(char const* vert_path, char const* frag_path) {
    GLuint vert_id = load_shader(GL_VERTEX_SHADER  , vert_path);
    GLuint frag_id = load_shader(GL_FRAGMENT_SHADER, frag_path);

    GLuint id = glCreateProgram();
    glAttachShader(id, vert_id);
    glAttachShader(id, frag_id);
    glLinkProgram(id);
    glDeleteShader(vert_id);
    glDeleteShader(frag_id);

    {   int success;
        glGetProgramiv(id, GL_LINK_STATUS, &success);
        if(!success) {
            static constexpr SizeT OPENGL_LOG_SIZE = 512;
            char log[OPENGL_LOG_SIZE];
            glGetProgramInfoLog(id, OPENGL_LOG_SIZE, nullptr, log);
            LUX_FATAL("program linking error: \n%s", log);
        }
    }
    return id;
}

GLuint load_texture(char const* path, Vec2UI& size_out) {
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    DynArr<U8> img;
    {   auto err = lodepng::decode(img, size_out.x, size_out.y, path);
        if(err) LUX_FATAL("couldn't load texture: %s", path);
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size_out.x, size_out.y,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, img.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void generate_mipmaps(GLuint texture_id, U32 max_lvl) {
    LUX_LOG("unimplemented");
}
