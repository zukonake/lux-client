#pragma once

#include <config.hpp>
//
#include <lux_shared/common.hpp>
#include <lux_shared/entity.hpp>

void entity_init();
void entity_render(EntityVec const& player_pos, EntityComps const& comps);
