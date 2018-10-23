#pragma once

#include <config.hpp>
//
#include <lux_shared/common.hpp>
#include <lux_shared/entity.hpp>
#include <lux_shared/net/data.hpp>

extern DynArr<EntityHandle> entities;
void entity_init();
void set_net_entity_comps(NetSsTick::EntityComps const& net_comps);
