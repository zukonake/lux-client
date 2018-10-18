#pragma once

#include <lux_shared/vec.hpp>

struct Viewport {
    Vec2F pos;
    Vec2F scale;
};

extern Viewport screen_viewport;
extern Viewport world_viewport;

Vec2F transform_point(Vec2F const& point, Viewport const& viewport);
