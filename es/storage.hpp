//---------------------------------------------------------------------------
/// \file   es/storage.hpp
/// \brief  The entity/component data store
//
// Copyright 2013, nocte@hippie.nu            Released under the MIT License.
//---------------------------------------------------------------------------
#pragma once

#include <algorithm>
#include <bitset>
#include <cassert>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
#include <unordered_map>

#include "component.hpp"
#include "entity.hpp"
#include "traits.hpp"


#include <iostream>

namespace es {

/** A storage ties entities and components together.
 * Storage associates two other bits of data with every entity:
 * - A 64-bit mask that keeps track of which components are defined
 * - A vector of bytes, holding the actual data
 *
 * The vector of bytes tries to pack the component data as tightly as
 * possible.  It is really fast for plain old datatypes, but it also
 * handles nontrivial types safely.  It packs a virtual table and a
 * pointer to some heap space in the vector, and calls the constructor
 * and destructor as needed.
 */
class storage
{
    // It is assumed the first few components will be accessed the
    // most often.  We keep a cache of the first 12.
    static const uint32_t cache_size = 12;
    static const uint32_t cache_mask = (1 << cache_size) - 1;

    /** This data gets associated with every entity. */
    struct elem
    {
        /** Bitmask to keep track of which components are held in \a data. */
        std::bitset<64>     components;
        /** Set to true if any data has changed. */
        bool                dirty;
        /** Component data for this entity. */
        std::vector<char>   data;

        elem() : dirty(true) {}
    };

    typedef component::placeholder placeholder;

    /** Data types that do not have a flat memory layout are kept in the
     ** elem::data buffer in a placeholder object. */
    template <typename t>
    class holder : public placeholder
    {
    public:
        holder () { }
        holder (t&& init) : held_(std::move(init)) { }
        holder (const t& init) : held_(init) { }

        const t& held() const { return held_; }
        t&       held()       { return held_; }

        placeholder* clone() const { return new holder<t>(held_); }

        void serialize(std::vector<char>& buffer) const
            { es::serialize(held(), buffer); }

        buffer_t::const_iterator deserialize(buffer_t::const_iterator first, buffer_t::const_iterator last)
            { return es::deserialize(held(), first, last); }

        void move_to (buffer_t::iterator pos)
            {
                auto ptr (reinterpret_cast<holder<t>*>(&*pos));
                new (ptr) holder<t>(std::move(held_));
            }

    private:
        t held_;
    };

    typedef std::unordered_map<uint32_t, elem> stor_impl;

public:
    typedef uint8_t     component_id;

    typedef stor_impl::iterator         iterator;
    typedef stor_impl::const_iterator   const_iterator;


public:
    std::function<void(iterator)>       on_new_entity;
    std::function<void(iterator)>       on_deleted_entity;

public:
    storage();
    ~storage();

    template <typename type>
    component_id register_component (std::string name)
        {
            size_t size;

            if (is_flat<type>::value)
            {
                size = sizeof(type);
                components_.emplace_back(std::move(name), size, nullptr);
            }
            else
            {
                flat_mask_.set(components_.size());
                size = sizeof(holder<type>);
                components_.emplace_back(std::move(name), size,
                                         std::unique_ptr<placeholder>(new holder<type>()));
            }

            if (components_.size() < cache_size)
            {
                size_t i (component_offsets_.size());
                component_offsets_.resize(i * 2);
                for (size_t j (i); j != i * 2; ++j)
                    component_offsets_[j] = component_offsets_[j-i] + size;
            }

            return components_.size() - 1;
        }

    component_id find_component (const std::string& name) const;

    const component& operator[] (component_id id) const
        { return components_[id]; }

    const std::vector<component>& components() const
        { return components_; }

public:
    entity new_entity();

    /** Get an entity with a given ID, or create it if it didn't exist yet. */
    iterator make (uint32_t id);

    /** Create a whole bunch of empty entities in one go.
     * @param count     The number of entities to create
     * @return The range of entities created */
    std::pair<entity, entity> new_entities (size_t count);

    entity clone_entity (iterator f);

    iterator find (entity en);

    const_iterator find (entity en) const;

    size_t size() const;

    bool delete_entity (entity en);

    void delete_entity (iterator f);

    void remove_component_from_entity (iterator en, component_id c);

    bool exists (entity en) const
        { return entities_.count(en) != 0; }

    bool entity_has_component (iterator en, component_id c) const;

    template <typename type>
    void set (entity en, component_id c_id, type val)
        { return set<type>(find(en), c_id, std::move(val)); }

    template <typename type>
    void set (iterator en, component_id c_id, type val)
        {
            assert(c_id < components_.size());
            const component& c (components_[c_id]);
            elem&  e   (en->second);
            size_t off (offset(e, c_id));

            if (!e.components[c_id])
            {
                if (e.data.size() < off)
                    e.data.resize(off + c.size());
                else
                    e.data.insert(e.data.begin() + off, c.size(), 0);
            }

            if (is_flat<type>::value)
            {
                assert(e.data.size() >= off + sizeof(type));
                new (&*e.data.begin() + off) type(std::move(val));
            }
            else
            {
                assert(e.data.size() >= off + sizeof(holder<type>));
                auto ptr (reinterpret_cast<holder<type>*>(&*e.data.begin() + off));                
                if (e.components[c_id])
                    ptr->~placeholder();

                new (ptr) holder<type>(std::move(val));
            }
            e.components.set(c_id);
            e.dirty = true;
        }

    template <typename type>
    const type& get (entity en, component_id c_id) const
        { return get<type>(find(en), c_id); }

    template <typename type>
    const type& get (const_iterator en, component_id c_id) const
        {
            auto& e (en->second);
            if (!e.components[c_id])
                throw std::logic_error("entity does not have component");

            return get<type>(e, c_id);
        }

    template <typename type>
    type& get (entity en, component_id c_id)
        { return get<type>(find(en), c_id); }

    template <typename type>
    type& get (iterator en, component_id c_id)
        {
            auto& e (en->second);
            if (!e.components[c_id])
                throw std::logic_error("entity does not have component");

            return get<type>(e, c_id);
        }


    /** Call a function for every entity that has a given component.
     *  The callee can then query and change the value of the component through
     *  a var_ref object, or remove the entity.
     * @param c     The component to look for.
     * @param func  The function to call.  This function will be passed an
     *              iterator to the current entity, and a var_ref corresponding
     *              to the component value in this entity. */
    template <typename t>
    void for_each (component_id c, std::function<bool(iterator, t&)> func)
        {
            std::bitset<64> mask;
            mask.set(c);
            for (auto i (begin()); i != end(); )
            {
                auto next (std::next(i));
                elem& e (i->second);
                if ((e.components & mask) == mask)
                    e.dirty |= func(i, get<t>(e, c));

                i = next;
            }
        }

    template <typename t1, typename t2>
    void for_each (component_id c1, component_id c2,
                   std::function<bool(iterator, t1&, t2&)> func)
        {
            std::bitset<64> mask;
            mask.set(c1);
            mask.set(c2);
            for (auto i (begin()); i != end(); )
            {
                auto next (std::next(i));
                elem& e (i->second);
                if ((e.components & mask) == mask)
                    e.dirty |= func(i, get<t1>(e, c1), get<t2>(e, c2));

                i = next;
            }
        }

    template <typename t1, typename t2, typename t3>
    void for_each (component_id c1, component_id c2, component_id c3,
                   std::function<bool(iterator, t1&, t2&, t3&)> func)
        {
            std::bitset<64> mask;
            mask.set(c1);
            mask.set(c2);
            mask.set(c3);
            for (auto i (begin()); i != end(); )
            {
                auto next (std::next(i));
                elem& e (i->second);
                if ((e.components & mask) == mask)
                {
                    e.dirty |= func(i, get<t1>(e, c1),
                                       get<t2>(e, c2),
                                       get<t3>(e, c3));
                }
                i = next;
            }
        }

    bool check_dirty (iterator en);
    bool check_dirty_and_clear (iterator en);

    void serialize (const_iterator en, std::vector<char>& buffer) const;
    void deserialize (iterator en, const std::vector<char>& buffer);

    iterator begin()                { return entities_.begin();  }
    iterator end()                  { return entities_.end();    }
    const_iterator begin() const    { return entities_.begin();  }
    const_iterator end() const      { return entities_.end();    }
    const_iterator cbegin() const   { return entities_.cbegin(); }
    const_iterator cend() const     { return entities_.cend();   }

private:
    template <typename type>
    const type& get (const elem& e, component_id c_id) const
        {
            auto data_ptr (&*e.data.begin() + offset(e, c_id));
            if (is_flat<type>::value)
                return *reinterpret_cast<const type*>(data_ptr);

            auto obj_ptr (reinterpret_cast<const holder<type>*>(data_ptr));
            return obj_ptr->held();
        }

    template <typename type>
    type& get (elem& e, component_id c_id)
        {
            auto data_ptr (&*e.data.begin() + offset(e, c_id));
            if (is_flat<type>::value)
                return *reinterpret_cast<type*>(data_ptr);

            auto obj_ptr (reinterpret_cast<holder<type>*>(data_ptr));
            return obj_ptr->held();
        }

    size_t offset (const elem& e, component_id c) const;

    void call_destructors (iterator i) const;

private:
    /** Keeps track of entity IDs to give out. */
    uint32_t                            next_id_;

    /** The list of registered components. */
    std::vector<component>              components_;

    /** Mapping entity IDs to their data. */
    std::unordered_map<uint32_t, elem>  entities_;

    /** A lookup table for the data offsets of components.
     *  The index is the bitmask as used in elem::components. */
    std::vector<size_t>                 component_offsets_;

    /** A bitmask to quickly determine whether a certain combination of
     ** components has a flat memory layout or not. */
    std::bitset<64>                     flat_mask_;
};

} // namespace es
