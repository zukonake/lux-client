#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
//
#include <lux/common/chunk.hpp>
#include <lux/net/server/packet.hpp>
#include <lux/net/client/packet.hpp>
//
#include <data/config.hpp>
#include "renderer.hpp"

Renderer::Renderer(GLFWwindow *win, data::Config const &conf) :
    IoNode(win),
    map(*conf.db),
    view_range(conf.view_range),
    world_mat({1.0, 0.0, 0.0, 0.0}, /* swapped z with y */
              {0.0, 0.0, 1.0, 0.0},
              {0.0, 1.0, 0.0, 0.0},
              {0.0, 0.0, 0.0, 1.0})
{
    program.init(conf.vert_shader_path, conf.frag_shader_path);
    program.set_uniform("world", glUniformMatrix4fv,
        1, GL_FALSE, glm::value_ptr(world_mat));

    glm::vec2 tileset_size = tileset.load(conf.tileset_path);
    glm::vec2 tile_scale = {(F32)conf.tile_size.x / (F32)tileset_size.x,
                            (F32)conf.tile_size.y / (F32)tileset_size.y};

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    program.set_uniform("tex_size", glUniform2f, tile_scale.x, tile_scale.y);
    program.set_uniform("world", glUniformMatrix4fv,
        1, GL_FALSE, glm::value_ptr(world_mat));

    glfwGetWindowSize(IoNode::win, &last_mouse_pos.x, &last_mouse_pos.y);
    last_mouse_pos /= 2.f;
}

void Renderer::take_st(net::server::Tick const &st)
{
    render_world(st.player_pos);
}

void Renderer::take_ss(net::server::Packet const &sp)
{
    if(sp.type == net::server::Packet::MAP)
    {
        for(auto const &chunk : sp.map.chunks)
        {
            map.add_chunk(chunk);
        }
    }
}

void Renderer::give_ct(net::client::Tick &ct)
{
    glm::vec2 rotation = camera.get_rotation();
    ct.yaw = rotation.x;
    ct.pitch = rotation.y;
}

void Renderer::take_resize(Vec2<U32> const &size)
{
    update_projection((F32)size.x/(F32)size.y);
}

void Renderer::render_world(entity::Pos const &player_pos)
{
    update_view(player_pos);

    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    chunk::Pos iter;
    chunk::Pos center = chunk::to_pos(player_pos); //TODO entity::to_pos
    for(iter.z = center.z - view_range.z;  //TODO render all chunks instead
        iter.z <= center.z + view_range.z; // server will handle the loading
        ++iter.z)
    {
        for(iter.y = center.y - view_range.y;
            iter.y <= center.y + view_range.y;
            ++iter.y)
        {
            for(iter.x = center.x - view_range.x;
                iter.x <= center.x + view_range.x;
                ++iter.x)
            {
                render_chunk(iter);
            }
        }
    }
}

void Renderer::render_chunk(chunk::Pos const &pos)
{
    map::Chunk const *chunk = map[pos];
    if(chunk != nullptr)
    {
        glBindBuffer(GL_ARRAY_BUFFER, chunk->vbo_id);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, chunk->ebo_id);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
            sizeof(render::Vertex), (void*)offsetof(render::Vertex, pos));
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE,
            sizeof(render::Vertex), (void*)offsetof(render::Vertex, col));
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
            sizeof(render::Vertex), (void*)offsetof(render::Vertex, tex_pos));
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        glDrawElements(GL_TRIANGLES, chunk->indices.size(), render::INDEX_TYPE, 0);
        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
    }

}

void Renderer::update_view(entity::Pos const &player_pos)
{
    Vec2<F64> mouse_pos;
    Vec2<I32> screen_size;
    glfwGetCursorPos(IoNode::win, &mouse_pos.x, &mouse_pos.y);
    glfwGetWindowSize(IoNode::win, &screen_size.x, &screen_size.y);
    camera.rotate({(F32)(mouse_pos.x - last_mouse_pos.x) / screen_size.x,
                   (F32)(last_mouse_pos.y - mouse_pos.y) / screen_size.y});
    last_mouse_pos = mouse_pos;
    camera.teleport(glm::vec3(world_mat *
        glm::vec4(player_pos + glm::vec3(0.0, 0.0, 0.8), 1.0)));
    program.set_uniform("view", glUniformMatrix4fv,
        1, GL_FALSE, glm::value_ptr(camera.get_view()));
}

void Renderer::update_projection(F32 width_to_height)
{
    glm::mat4 projection =
        glm::perspective(glm::radians(FOV), width_to_height, Z_NEAR, Z_FAR);
    program.set_uniform("projection", glUniformMatrix4fv,
        1, GL_FALSE, glm::value_ptr(projection));
}
