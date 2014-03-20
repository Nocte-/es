//---------------------------------------------------------------------------
/// \file   es/component.hpp
/// \brief  A component is a data type that can be assigned to entities
//
// Copyright 2013-2014, nocte@hippie.nu       Released under the MIT License.
//---------------------------------------------------------------------------
#pragma once

#include <functional>
#include <memory>
#include <stdexcept>
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
        typedef std::vector<char>   buffer_t;

        virtual ~placeholder() { }

        /** Return a copy of the underlying object. */
        virtual placeholder* clone() const = 0;

        /** Serialize the object to a buffer. */
        virtual void serialize(buffer_t& buffer) const = 0;

        /** Deserialize from a buffer.
         * The function is passed a range in a buffer.  It should return
         * the end point up until which it has parsed. */
        virtual buffer_t::const_iterator
                     deserialize(buffer_t::const_iterator first,
                                 buffer_t::const_iterator last) = 0;

        virtual void move_to (buffer_t::iterator pos) = 0;
    };

public:
    /**
     * @param name     Descriptive name
     * @param size     Size of an instance of this component in bytes.  For
     *                 components that need a placeholder, this should be
     *                 sizeof(placeholder).
     * @param ph       Simple types should pass a nullptr here.  Complex types
     *                 should pass a pointer to a placeholder instance of
     *                 the correct type. */
    component(std::string name, size_t size, std::unique_ptr<placeholder> ph)
        : name_ (std::move(name))
        , size_ (size)
        , ph_   (std::move(ph))
    { }

	component(component&& m)
		: name_(std::move(m.name_))
		, size_(m.size_)
		, ph_(std::move(m.ph_))
	{
		m.size_ = 0;
	}

	component& operator=(component&& m)
	{
		if (&m != this)
		{
			name_ = std::move(m.name_);
			size_ = m.size_;
			ph_ = std::move(m.ph_);
			m.size_ = 0;
		}
		return *this;
	}

    const std::string&  name() const    { return name_; }
    size_t              size() const    { return size_; }
    bool                is_flat() const { return ph_ == nullptr; }

    bool operator== (const std::string& compare) const
        { return name_ == compare; }

    bool operator!= (const std::string& compare) const
        { return name_ != compare; }

protected:
    placeholder*          clone() const   { return ph_->clone(); }

private:
    std::string name_;
    size_t      size_;
    std::unique_ptr<placeholder> ph_;
};

//---------------------------------------------------------------------------

// Implement these two functions for any custom data types you want to
// (de)serialize.  You can find an example in unit_tests.cpp.

template<typename t>
void
serialize (const t&, std::vector<char>&)
{
    throw std::runtime_error(std::string("es::serialize not implemented for ") + typeid(t).name());
}

template<typename t>
std::vector<char>::const_iterator
deserialize (t&, std::vector<char>::const_iterator, std::vector<char>::const_iterator)
{
    throw std::runtime_error(std::string("es::deserialize not implemented for ") + typeid(t).name());
}

} // namespace es

