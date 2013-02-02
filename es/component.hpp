//---------------------------------------------------------------------------
/// \file   es/component.hpp
/// \brief  A component is a data type that can be assigned to entities
//
// Copyright 2013, nocte@hippie.nu            Released under the MIT License.
//---------------------------------------------------------------------------
#pragma once

#include <memory>
#include <string>
#include <typeinfo>

namespace es {

class storage;

/** A component is a data type that can be assigned to entities.
 * For example, an entity could have a position and a velocity.  The position
 * would be a component, and the data type would be a 2- or 3-dimensional
 * coordinate.  The velocity would be yet another component, and the data
 * type would be a vector. */
class component
{
    friend class storage;

protected:
    /** Placeholder for complex data types.
     *  Some data types don't have a fixed, flat memory layout.  This
     *  class defines an interface that can be used as a placeholder in
     *  the entity buffer, while the actual object resides on the heap. */
    class placeholder
    {
    public:
        virtual ~placeholder() { }

        /** Return a copy of the underlying object. */
        virtual placeholder* clone() const = 0;
    };

public:
    /** @param name     Descriptive name
     *  @param size     Size of an instance of this component in bytes.  For
     *                  components that need a placeholder, this should be
     *                  sizeof(placeholder).
     *  @param ph       Simple types should pass a nullptr here.  Complex types
     *                  should pass a pointer to a placeholder instance of
     *                  the correct type. */
    component(std::string name, size_t size, std::unique_ptr<placeholder> ph)
        : name_(std::move(name))
        , size_(size)
        , ph_(std::move(ph))
    { }

    const std::string&    name() const      { return name_; }
    size_t                size() const      { return size_; }
    bool                  is_flat() const   { return ph_ == nullptr; }

    bool operator== (const std::string& compare) const
        { return name_ == compare; }

    bool operator!= (const std::string& compare) const
        { return name_ != compare; }

private:
    std::string             name_;
    size_t                  size_;
    std::unique_ptr<placeholder> ph_;
};

} // namespace es

