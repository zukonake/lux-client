#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>
#undef GLM_ENABLE_EXPERIMENTAL
//
#include <lux/common.hpp>
#include <lux/world/map.hpp>
#include <lux/net/server/packet.hpp>
#include <lux/net/client/packet.hpp>
//
#include <config.h>
#include <data/config.hpp>
#include <map/geometry_vert.hpp>
#include <map/lightning_vert.hpp>
#include "renderer.hpp"

Renderer::Renderer(GLFWwindow *win, data::Config const &conf) :
    IoNode(win),
    map(*conf.db),
    view_range(conf.view_range),
    z_far(0.f),
    fov(conf.fov),
    wireframe(false),
    face_culling(true),
    frustrum_culling(true),
    distance_sorting(true),
    sky_color(conf.sky_color),
    screen_scale(conf.window_scale),
    world_mat({1.0, 0.0, 0.0, 0.0}, /* swapped z with y */
              {0.0, 0.0, 1.0, 0.0},
              {0.0, 1.0, 0.0, 0.0},
              {0.0, 0.0, 0.0, 1.0}),
    view_mat(1.f),
    projection_mat(1.f)
{
    Vec2I current_screen_size;
    glfwGetWindowSize(win, &current_screen_size.x, &current_screen_size.y);
    screen_size = (Vec2UI)current_screen_size;
    last_mouse_pos = screen_size / 2u;

    program.init(conf.vert_shader_path, conf.frag_shader_path);
    program.use();
    program.set_uniform("world", glUniformMatrix4fv,
        1, GL_FALSE, glm::value_ptr(world_mat));
    update_view_range();

    Vec2F tileset_size = tileset.load(conf.tileset_path);
    Vec2F tile_scale = {(F32)conf.tile_size.x / (F32)tileset_size.x,
                            (F32)conf.tile_size.y / (F32)tileset_size.y};
    tileset.generate_mipmaps(tileset_size.x / conf.tile_size.x);

    program.set_uniform("tex_size", glUniform2f, tile_scale.x, tile_scale.y);

    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glGenFramebuffers(1, &fb_id);
    glGenRenderbuffers(1, &color_rb_id);
    glGenRenderbuffers(1, &depth_rb_id);
    glBindFramebuffer(GL_FRAMEBUFFER, fb_id);
    glBindRenderbuffer(GL_RENDERBUFFER, color_rb_id);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_RENDERBUFFER, color_rb_id);
    glBindRenderbuffer(GL_RENDERBUFFER, depth_rb_id);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
        GL_RENDERBUFFER, depth_rb_id);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

Renderer::~Renderer()
{
    glDeleteFramebuffers(1, &fb_id);
    glDeleteRenderbuffers(1, &color_rb_id);
    glDeleteRenderbuffers(1, &depth_rb_id);
}

void Renderer::toggle_wireframe()
{
    wireframe = !wireframe;
}

void Renderer::toggle_face_culling()
{
    face_culling = !face_culling;
}

void Renderer::toggle_frustrum_culling()
{
    frustrum_culling = !frustrum_culling;
}

void Renderer::toggle_distance_sorting()
{
    distance_sorting = !distance_sorting;
}

void Renderer::increase_view_range()
{
    view_range += 1.f;
    update_view_range();
}

void Renderer::decrease_view_range()
{
    if(view_range >= 1.f)
    {
        view_range -= 1.f;
        update_view_range();
    }
}

F32 Renderer::get_view_range()
{
    return view_range;
}

void Renderer::take_st(net::server::Tick const &st)
{
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb_id);
    glViewport(0, 0, screen_size.x, screen_size.y);
    glEnable(GL_DEPTH_TEST);
    if(face_culling) glEnable(GL_CULL_FACE);
    if(wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    program.use();
    tileset.use();
    glClearColor(sky_color.r, sky_color.g, sky_color.b, sky_color.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    render_world(st.player_pos);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb_id);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, screen_size.x, screen_size.y,
                      0, 0, screen_size.x * screen_scale.x,
                            screen_size.y * screen_scale.y,
                      GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    if(face_culling) glDisable(GL_CULL_FACE);
    if(wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_DEPTH_TEST);
    glViewport(0, 0, screen_size.x * screen_scale.x,
                     screen_size.y * screen_scale.y);
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
    Vec2F rotation = camera.get_rotation();
    ct.yaw = rotation.x;
    ct.pitch = rotation.y;
}

void Renderer::take_resize(Vec2UI const &size)
{
    screen_size = size / screen_scale;
    glBindRenderbuffer(GL_RENDERBUFFER, color_rb_id);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA,
        screen_size.x, screen_size.y);
    glBindRenderbuffer(GL_RENDERBUFFER, depth_rb_id);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24,
        screen_size.x, screen_size.y);
    program.use();
    update_projection((F32)screen_size.x/(F32)screen_size.y);
}

void Renderer::render_world(EntityPos const &player_pos)
{
    update_view(player_pos);

    ChkPos center = to_chk_pos(glm::round(player_pos));

    render_queue.clear(); //TODO only when moving?
    get_render_queue(render_queue, center);
    sort_render_queue(render_queue, center);

    for(auto const &chunk : render_queue)
    {
        render_chunk(chunk);
    }
}

void Renderer::render_chunk(ChkPos const &pos)
{
    Chunk const *chunk = map.get_chunk(pos);
    if(chunk != nullptr)
    {
        if(chunk->mesh.is_generated == false)
        {
            map.try_mesh(pos);
        }
        else
        {
            if(chunk->mesh.indices.size() > 0)
            {
                render_mesh(chunk->mesh);
            }
        }
    }
}

bool Renderer::is_chunk_visible(ChkPos const &pos)
{
    MapPos base_pos = to_map_pos(pos, 0);
    for(U32 i = 0; i <= 0b111; ++i)
    {
        MapPos map_pos = base_pos + MapPos(CHK_SIZE) *
            MapPos(i & 0b001, (i & 0b010) >> 1, (i & 0b100) >> 2);
        Vec4F v_pos = wvp_mat * Vec4F(map_pos, 1.f);
        if(v_pos.z >= -v_pos.w && v_pos.z <= v_pos.w)
        {
            return true;
        }
    }
    return false;
}

void Renderer::get_render_queue(Vector<ChkPos> &render_queue, ChkPos const &center)
{
    ChkPos iter;
    if(frustrum_culling)
    {
        for(iter.z  = center.z - view_range;
            iter.z <= center.z + view_range;
            ++iter.z)
        {
            for(iter.y  = center.y - view_range;
                iter.y <= center.y + view_range;
                ++iter.y)
            {
                for(iter.x  = center.x - view_range;
                    iter.x <= center.x + view_range;
                    ++iter.x)
                {
                    if(is_chunk_visible(iter)) render_queue.push_back(iter);
                }
            }
        }
    }
    else
    {
        for(iter.z  = center.z - view_range;
            iter.z <= center.z + view_range;
            ++iter.z)
        {
            for(iter.y  = center.y - view_range;
                iter.y <= center.y + view_range;
                ++iter.y)
            {
                for(iter.x  = center.x - view_range;
                    iter.x <= center.x + view_range;
                    ++iter.x)
                {
                    render_queue.push_back(iter);
                }
            }
        }
    }
}

void Renderer::sort_render_queue(Vector<ChkPos> &render_queue, ChkPos const &center)
{
    if(distance_sorting)
    {
        auto f_point = [&] (Vec3F const &p) -> Vec3F
        {
            return p + Vec3F(0.5, 0.5, 0.5);
        };
        Vec3F f_center = f_point(center);
        auto distance_sort = [&] (Vec3F const &a, Vec3F const &b) -> bool
        {
            return glm::distance(f_point(a), f_center) <
                   glm::distance(f_point(b), f_center);
        };
        std::sort(render_queue.begin(), render_queue.end(), distance_sort);
    }
}

void Renderer::render_mesh(Chunk::Mesh const &mesh)
{
    glBindBuffer(GL_ARRAY_BUFFER, mesh.geometry_vbo);
    glVertexAttribPointer(0, 3, GL_INT, GL_FALSE,
        sizeof(Chunk::Mesh::GeometryVert),
        (void*)offsetof(Chunk::Mesh::GeometryVert, pos));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
        sizeof(Chunk::Mesh::GeometryVert),
        (void*)offsetof(Chunk::Mesh::GeometryVert, tex_pos));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.lightning_vbo);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE,
        sizeof(Chunk::Mesh::LightningVert),
        (void*)offsetof(Chunk::Mesh::LightningVert, col));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glDrawElements(GL_TRIANGLES, mesh.indices.size(),
                   Chunk::Mesh::INDEX_GL_TYPE, 0);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
}

void Renderer::update_view(EntityPos const &player_pos)
{
    //TODO move to entity controller?
    Vec2D mouse_pos;
    glfwGetCursorPos(IoNode::win, &mouse_pos.x, &mouse_pos.y);
    camera.rotate({(F32)(mouse_pos.x - last_mouse_pos.x)
                      / (screen_size.x * screen_scale.x),
                   (F32)(last_mouse_pos.y - mouse_pos.y)
                      / (screen_size.y * screen_scale.y)});
    last_mouse_pos = mouse_pos;
    camera.teleport(Vec3F(world_mat *
        Vec4F(player_pos + Vec3F(0.0, 0.0, 0.8), 1.0)));
    view_mat = camera.get_view();
    program.set_uniform("view", glUniformMatrix4fv,
        1, GL_FALSE, glm::value_ptr(view_mat));
    update_wvp();
}

void Renderer::update_projection(F32 width_to_height)
{
    projection_mat =
        glm::perspective(glm::radians(fov), width_to_height, Z_NEAR, z_far);
    program.set_uniform("projection", glUniformMatrix4fv,
        1, GL_FALSE, glm::value_ptr(projection_mat));
    update_wvp();
}

void Renderer::update_wvp()
{
    wvp_mat = projection_mat * view_mat * world_mat;
    program.set_uniform("wvp", glUniformMatrix4fv,
        1, GL_FALSE, glm::value_ptr(wvp_mat));
}

void Renderer::update_view_range()
{
    z_far = glm::compMax(CHK_SIZE) * (view_range + 1);

    program.use();
    update_projection((F32)screen_size.x/(F32)screen_size.y);
}
