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
        DynArr<EntityId> items;
    };
    struct Text {
        TextId text;
    };

    HashTable<EntityId, Pos>         pos;
    HashTable<EntityId, Name>        name;
    HashTable<EntityId, Visible>     visible;
    HashTable<EntityId, Container>   container;
    HashTable<EntityId, Orientation> orientation;
    HashTable<EntityId, Text>        text;
};

extern EntityComps& entity_comps;
extern DynArr<EntityId> entities;
void entity_init();
void set_net_entity_comps(NetSsTick::EntityComps const& net_comps);
