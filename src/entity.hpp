#pragma once

#include <config.hpp>
//
#include <lux_shared/common.hpp>
#include <lux_shared/entity.hpp>
#include <lux_shared/net/data.hpp>
//
#include <ui.hpp>

struct EntityComps {
    typedef EntityVec Pos;
    typedef DynArr<char> Name;
    struct Visible {
        U32   visible_id;
        Vec2F quad_sz;
    };
    struct Orientation {
        F32 angle; ///in radians
    };
    struct Container {
        DynArr<EntityHandle> items;
    };
    struct Text {
        TextHandle text;
    };

    HashTable<EntityHandle, Pos>         pos;
    HashTable<EntityHandle, Name>        name;
    HashTable<EntityHandle, Visible>     visible;
    HashTable<EntityHandle, Container>   container;
    HashTable<EntityHandle, Orientation> orientation;
    HashTable<EntityHandle, Text>        text;
};

extern EntityComps& entity_comps;
extern DynArr<EntityHandle> entities;
void entity_init();
void set_net_entity_comps(NetSsTick::EntityComps const& net_comps);
