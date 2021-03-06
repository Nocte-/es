//---------------------------------------------------------------------------
/// \file   es/storage.hpp
/// \brief  The entity/component data store
//
// Copyright 2014, nocte@hippie.nu            Released under the MIT License.
//---------------------------------------------------------------------------
#pragma once

#include <algorithm>
#include <bitset>
#include <cassert>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <type_traits>
#include <vector>
#include <unordered_map>

#include "component.hpp"
#include "entity.hpp"
#include "traits.hpp"

namespace es
{
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
    /** This data gets associated with every entity. */
    struct elem
    {
        /** Bitmask to keep track of which components are held in \a data. */
        std::bitset<64> components;
        /** Track what aspects of an entity have changed. */
        std::bitset<64> dirty;
        /** Component data for this entity. */
        std::vector<char> data;

        elem()
            : dirty(true)
        {
        }
    };

    typedef component::placeholder placeholder;

    /** Data types that do not have a flat memory layout are kept in the
    * * elem::data buffer in a placeholder object. */
    template <typename T>
    class holder : public placeholder
    {
    public:
        holder() {}

        holder(T&& init)
            : held_(std::move(init))
        {
        }

        holder(const T& init)
            : held_(init)
        {
        }

        const T& held() const { return held_; }

        T& held() { return held_; }

        placeholder* clone() const { return new holder<T>(held_); }

        void serialize(std::vector<char>& buffer) const
        {
            es::serialize(held(), buffer);
        }

        buffer_t::const_iterator deserialize(buffer_t::const_iterator first,
                                             buffer_t::const_iterator last)
        {
            return es::deserialize(held(), first, last);
        }

        void move_to(buffer_t::iterator pos)
        {
            auto ptr = reinterpret_cast<holder<T>*>(&*pos);
            auto tmp = new (ptr) holder<T>(std::move(held_));
            assert(tmp == ptr);
            (void)tmp;
        }

    private:
        T held_;
    };

    typedef std::unordered_map<uint32_t, elem> stor_impl;

public:
    typedef uint8_t component_id;

    typedef stor_impl::iterator iterator;
    typedef stor_impl::const_iterator const_iterator;

public:
    std::function<void(iterator)> on_new_entity;
    std::function<void(iterator)> on_deleted_entity;

public:
    storage();
    ~storage();

    template <typename type>
    component_id register_component(std::string&& name)
    {
        size_t size;

        if (is_flat<type>::value) {
            size = sizeof(type);
            components_.emplace_back(std::move(name), size, typeid(type),
                                     nullptr);
        } else {
            flat_mask_.set(components_.size());
            size = sizeof(holder<type>);
            components_.emplace_back(
                std::move(name), size, typeid(type),
                std::unique_ptr<placeholder>(new holder<type>()));
        }

        component_id index = components_.size() - 1;
        size_t block = (index & 0x38) << 5;
        size_t i = 1 << (index & 0x07);

        for (size_t j = block + i; j != block + i * 2; ++j)
            component_offsets_[j] = component_offsets_[j - i] + size;

        return index;
    }

    component_id find_component(const std::string& name) const;

    const component& operator[](component_id id) const
    {
        return components_[id];
    }

    const std::vector<component>& components() const { return components_; }

public:
    entity new_entity();

    /** Get an entity with a given ID, or create it if it didn't exist yet. */
    iterator make(uint32_t id);

    /** Create a whole bunch of empty entities in one go.
     * @param count     The number of entities to create
     * @return The range of entities created */
    std::pair<entity, entity> new_entities(size_t count);

    entity clone_entity(iterator f);

    iterator find(entity en);

    const_iterator find(entity en) const;

    size_t size() const;

    bool delete_entity(entity en);

    void delete_entity(iterator f);

    void remove_component_from_entity(iterator en, component_id c);

    bool exists(entity en) const { return entities_.count(en) != 0; }

    bool entity_has_component(iterator en, component_id c) const;

    template <typename T>
    void set(entity en, component_id c_id, T val)
    {
        set<T>(find(en), c_id, val);
    }

    template <typename T>
    void set(iterator en, component_id c_id, T val)
    {
        assert(c_id < components_.size());
        const component& c = components_[c_id];
        assert(c.is_of_type<T>());
        elem& e = en->second;
        size_t off = offset(e, c_id);

        if (!e.components[c_id]) {
            if (e.data.size() < off)
                e.data.resize(off + c.size());
            else
                e.data.insert(e.data.begin() + off, c.size(), 0);
        }

        if (is_flat<T>::value) {
            assert(e.data.size() >= off + sizeof(T));
            new (&*e.data.begin() + off) T(val);
        } else {
            assert(e.data.size() >= off + sizeof(holder<T>));

            auto ptr = reinterpret_cast<holder<T>*>(&*e.data.begin() + off);
            if (e.components[c_id])
                ptr->~holder();

            auto tmp = new (ptr) holder<T>(std::move(val));
            assert(tmp == ptr);
            (void)tmp;
        }
        e.components.set(c_id);
        e.dirty.set(c_id);
    }

    template <typename T>
    const T& get(entity en, component_id c_id) const
    {
        return get<T>(find(en), c_id);
    }

    template <typename T>
    const T& get(const_iterator en, component_id c_id) const
    {
        auto& e = en->second;
        if (!e.components[c_id])
            throw std::logic_error("entity does not have component");

        return get<T>(e, c_id);
    }

    template <typename T>
    T& get(entity en, component_id c_id)
    {
        return get<T>(find(en), c_id);
    }

    template <typename T>
    T& get(iterator en, component_id c_id)
    {
        auto& e = en->second;
        if (!e.components[c_id])
            throw std::logic_error("entity does not have component");

        return get<T>(e, c_id);
    }

    /** Call a function for every entity that has a given component.
     *  The callee can then query and change the value of the component through
     *  a var_ref object, or remove the entity.
     * @param c     The component to look for.
     * @param func  The function to call.  This function will be passed an
     *              iterator to the current entity, and a var_ref corresponding
     *              to the component value in this entity. */
    template <typename T>
    void for_each(component_id c, std::function<uint64_t(iterator, T&)> func)
    {
        std::bitset<64> mask;
        mask.set(c);
        for (auto i(begin()); i != end();) {
            auto next = std::next(i);
            elem& e(i->second);
            if ((e.components & mask) == mask) {
                e.dirty |= (func(i, get<T>(e, c)) & mask.to_ullong());
            }
            i = next;
        }
    }

    template <typename T1, typename T2>
    void for_each(component_id c1, component_id c2,
                  std::function<uint64_t(iterator, T1&, T2&)> func)
    {
        std::bitset<64> mask;
        mask.set(c1);
        mask.set(c2);
        for (auto i(begin()); i != end();) {
            auto next = std::next(i);
            elem& e = i->second;
            if ((e.components & mask) == mask)
                e.dirty |= (func(i, get<T1>(e, c1), get<T2>(e, c2))
                            & mask.to_ullong());

            i = next;
        }
    }

    template <typename T1, typename T2, typename T3>
    void for_each(component_id c1, component_id c2, component_id c3,
                  std::function<uint64_t(iterator, T1&, T2&, T3&)> func)
    {
        std::bitset<64> mask;
        mask.set(c1);
        mask.set(c2);
        mask.set(c3);
        for (auto i(begin()); i != end();) {
            auto next = std::next(i);
            elem& e = i->second;
            if ((e.components & mask) == mask) {
                e.dirty |= (func(i, get<T1>(e, c1), get<T2>(e, c2),
                                 get<T3>(e, c3)) & mask.to_ullong());
            }
            i = next;
        }
    }

    bool check_dirty(iterator en);
    bool check_dirty_and_clear(iterator en);

    bool check_dirty(iterator en, component_id c);
    bool check_dirty_and_clear(iterator en, component_id c);

    void serialize(const_iterator en, std::vector<char>& buffer) const;
    void deserialize(iterator en, const std::vector<char>& buffer);

    iterator begin() { return entities_.begin(); }

    iterator end() { return entities_.end(); }

    const_iterator begin() const { return entities_.begin(); }

    const_iterator end() const { return entities_.end(); }

    const_iterator cbegin() const { return entities_.cbegin(); }

    const_iterator cend() const { return entities_.cend(); }

private:
    template <typename T>
    const T& get(const elem& e, component_id c_id) const
    {
        assert(components_[c_id].is_of_type<T>());
        auto data_ptr(&*e.data.begin() + offset(e, c_id));
        if (is_flat<T>::value)
            return *reinterpret_cast<const T*>(data_ptr);

        auto obj_ptr(reinterpret_cast<const holder<T>*>(data_ptr));
        return obj_ptr->held();
    }

    template <typename T>
    T& get(elem& e, component_id c_id)
    {
        assert(components_[c_id].is_of_type<T>());
        auto data_ptr(&*e.data.begin() + offset(e, c_id));
        if (is_flat<T>::value)
            return *reinterpret_cast<T*>(data_ptr);

        auto obj_ptr(reinterpret_cast<holder<T>*>(data_ptr));
        return obj_ptr->held();
    }

    size_t offset(const elem& e, component_id c) const;

    void call_destructors(iterator i) const;

private:
    /** Keeps track of entity IDs to give out. */
    uint32_t next_id_;

    /** The list of registered components. */
    std::vector<component> components_;

    /** Mapping entity IDs to their data. */
    std::unordered_map<uint32_t, elem> entities_;

    /** A lookup table for the data offsets of components. */
    std::vector<size_t> component_offsets_;

    /** A bitmask to quickly determine whether a certain combination of
    * * components has a flat memory layout or not. */
    std::bitset<64> flat_mask_;
};

} // namespace es
