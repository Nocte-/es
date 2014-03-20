//---------------------------------------------------------------------------
/// \file   es/traits.hpp
/// \brief  Determine memory layout and serialization for certain types
//
// Copyright 2013, nocte@hippie.nu            Released under the MIT License.
//---------------------------------------------------------------------------
#pragma once

#include <type_traits>

namespace es {

/** Determine if a given type has a flat memory layout.
 *  By default, it uses std::is_trivial, which is always safe but not always
 *  optimal.  You can specialize is_flat for your own types if neccesary.
 *
 *  Note: other queries than is_trivial could make an even better default,
 *  but they're not consistently supported across compilers.
 */
template <typename t>
struct is_flat
{
    static const bool value = std::is_trivial<t>::value;
};

} // namespace es

