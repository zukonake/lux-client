#include <viewport.hpp>

Viewport screen_viewport = {{0.f, 0.f}, {1.f, 1.f}};
Viewport world_viewport  = {{0.f, 0.f}, {0.04f, -0.04f}};
Viewport ui_viewport     = {{0.f, 0.f}, {0.1f, -0.1f}};

Vec2F transform_point(Vec2F const& point, Viewport const& viewport) {
    return (point + viewport.pos) * viewport.scale;
}
