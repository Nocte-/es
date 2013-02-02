//---------------------------------------------------------------------------
/// \file   es/entity.hpp
/// \brief  An entity
//
// Copyright 2013, nocte@hippie.nu            Released under the MIT License.
//---------------------------------------------------------------------------
#pragma once

#include <cstdint>

namespace es {

/** An entity.
 * On its own, an entity is nothing more than a 32-bit integer that
 * corresponds to a "thing" in your game.  So on its own, it's not very
 * useful.  By assigning components to entities, you can describe the
 * different aspects of the "thing": it could have a position, a speed,
 * health, a name, enchantments.  This coupling is done in a \a storage
 * object. */ 
class entity
{
public:
    entity(uint32_t id) : id_(id) { }

    uint32_t id() const { return id_; }

private:
    uint32_t            id_;
};

} // namespace es

