//---------------------------------------------------------------------------
/// \file   hexa/ecs/entity.hpp
/// \brief  An entity
//
// Copyright 2013, nocte@hippie.nu            Released under the MIT License.
//---------------------------------------------------------------------------
#pragma once

#include <cstdint>

namespace ecs {

/** An entity is nothing more than a 32-bit integer.
 *  See \a storage for a description of how to combine entities and
 *  components. */
class entity
{
public:
    entity(uint32_t id) : id_(id) { }

    uint32_t id() const { return id_; }

private:
    uint32_t            id_;
};

} // namespace ecs
