#pragma once

#include <lux_shared/common.hpp>
#include <lux_shared/entity.hpp>
#include <lux_shared/net/data.hpp>
//
#include <ui.hpp>

struct EntityComps {
    typedef EntityVec Pos;
    typedef DynArr<char> Name;
    struct Model {
        U32   id;
    };
    struct Text {
        UiTextId text;
    };

    IdMap<EntityId, Pos>         pos;
    IdMap<EntityId, Name>        name;
    IdMap<EntityId, Model>       model;
    IdMap<EntityId, Text>        text;
};

extern EntityComps& entity_comps;
extern DynArr<EntityId> entities;
void entity_init();
void set_net_entity_comps(NetSsTick::EntityComps const& net_comps);
