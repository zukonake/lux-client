#include <stdexcept>
#include <fstream>
//
#include "program.hpp"

namespace render
{

Program::~Program()
{
    glDeleteProgram(id);
}

void Program::init(String const &vert_path, String const &frag_path)
{
    vert_id = load_shader(GL_VERTEX_SHADER  , vert_path);
    frag_id = load_shader(GL_FRAGMENT_SHADER, frag_path);

    id = glCreateProgram();
    glAttachShader(id, vert_id);
    glAttachShader(id, frag_id);
    glLinkProgram(id);
    {
        int success;
        glGetProgramiv(id, GL_LINK_STATUS, &success);
        if(!success)
        {
            char log[OPENGL_LOG_SIZE];
            glGetProgramInfoLog(id, OPENGL_LOG_SIZE, NULL, log);
            throw std::runtime_error("program linking error: \n" + std::string(log));
        }
    }
    glUseProgram(id);
    glDeleteShader(vert_id);
    glDeleteShader(frag_id);
}

GLuint Program::get_id() const
{
    return id;
}

GLuint Program::get_vert_id() const
{
    return vert_id;
}

GLuint Program::get_frag_id() const
{
    return frag_id;
}

GLuint Program::load_shader(GLenum type, String const &path)
{
    GLuint shader_id = glCreateShader(type);

    std::ifstream file(path);
    file.seekg (0, file.end);
    long len = file.tellg();
    file.seekg (0, file.beg);

    char *str = new char[(SizeT)len + 1];
    file.read(str, len);
    str[(SizeT)len] = '\0';
    glShaderSource(shader_id, 1, &str, NULL);
    glCompileShader(shader_id);
    file.close();
    delete[] str;

    int success;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);
    if(!success)
    {
        char log[OPENGL_LOG_SIZE];
        glGetShaderInfoLog(shader_id, OPENGL_LOG_SIZE, NULL, log);
        throw std::runtime_error("shader compile error: \n" + std::string(log));
    }
    return shader_id;
}

}
