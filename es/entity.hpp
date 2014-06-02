//---------------------------------------------------------------------------
/// \file   es/entity.hpp
/// \brief  An entity
//
// Copyright 2014, nocte@hippie.nu            Released under the MIT License.
//---------------------------------------------------------------------------
#pragma once

#include <cstdint>

namespace es
{
/** An entity.
 * An entity is nothing more than a 32-bit integer that uniquely identifies a
 * "thing" in your game.  So on its own, it's not very useful.  By assigning
 * components to entities, you can describe the different aspects of the
 * "thing": it could have a position, a speed, health, a name, enchantments.
 * This coupling is done in a \a storage object. */
typedef uint32_t entity;

} // namespace es
