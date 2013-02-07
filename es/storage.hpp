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
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
#include <unordered_map>

#include "component.hpp"
#include "entity.hpp"
#include "traits.hpp"

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
    static constexpr uint32_t cache_size = 12;
    static constexpr uint32_t cache_mask = (1 << cache_size) - 1;

    /** This data gets associated with every entity. */
    struct elem
    {
        /** Bitmask to keep track of which components are held in \a data. */
        std::bitset<64>     components;
        /** Component data for this entity. */
        std::vector<char>   data;
    };

    typedef component::placeholder placeholder;

    /** Data types that do not have a flat memory layout are kept in the 
     ** elem::data buffer in a placeholder object. */
    template <typename t>
    class holder : public placeholder
    {
    public:
        holder () { }
        holder (t init) : held_(std::move(init)) { }

        const t& held() const { return held_; }
        t&       held()       { return held_; }

        placeholder* clone() const { return new holder<t>(held_); }

    private:
        t held_;
    };

    typedef std::unordered_map<uint32_t, elem> stor_impl;

public:
    typedef uint8_t     component_id;

    typedef stor_impl::iterator         iterator;
    typedef stor_impl::const_iterator   const_iterator;

public:
    /** Variable references are used by systems to access an entity's data.
     *  It keeps track of the elem::data buffer and the offset inside that
     *  buffer. */
    template <typename type>
    class var_ref
    {
        friend class storage;

        type& get()
        {
            if (is_flat<type>::value)
                return *reinterpret_cast<type*>(&*e_.data.begin() + offset_);

            auto ptr (reinterpret_cast<holder<type>*>(&*e_.data.begin() + offset_));
            return ptr->held();
        }

    public:
        operator const type& () const
        {
            if (is_flat<type>::value)
                return *reinterpret_cast<const type*>(&*e_.data.begin() + offset_);

            auto ptr (reinterpret_cast<const holder<type>*>(&*e_.data.begin() + offset_));
            return ptr->held();
        }

        var_ref& operator= (type assign)
        {
            if (is_flat<type>::value)
            {
                new (&*e_.data.begin() + offset_) type(std::move(assign));
            }
            else
            {
                auto ptr (reinterpret_cast<holder<type>*>(&*e_.data.begin() + offset_));
                new (ptr) holder<type>(std::move(assign));
            }
            return *this;
        }

        template <typename s>
        var_ref& operator+= (s val)
            { get() += val; return *this; }

        template <typename s>
        var_ref& operator-= (s val)
            { get() -= val; return *this; }

        template <typename s>
        var_ref& operator*= (s val)
            { get() *= val; return *this; }

        template <typename s>
        var_ref& operator/= (s val)
            { get() /= val; return *this; }

    protected:
        var_ref (size_t offset, elem& e)
            : offset_(offset), e_(e)
            { }

    private:
        size_t offset_;
        elem&  e_;
    };

public:
    storage()
        : next_id_(0)
        {
            component_offsets_.push_back(0);
        }

    template <typename type>
    component_id register_component (std::string name)
        {
            size_t size;

            if (is_flat<type>::value)
            {
                size = sizeof(type);
                components_.emplace_back(name, size, nullptr);
            }
            else
            {
                flat_mask_.set(components_.size());
                size = sizeof(holder<type>);
                components_.emplace_back(name, size,
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

    component_id find_component (const std::string& name) const
        {
            auto found (std::find(components_.begin(), components_.end(), name));
            if (found == components_.end())
                throw std::logic_error("component does not exist");

            return std::distance(components_.begin(), found);
        }

    const component& operator[] (component_id id) const
        { return components_[id]; }

    const std::vector<component>& components() const
        { return components_; }

public:
    entity new_entity()
        {
            entities_[next_id_++];
            return next_id_ - 1;
        }

    /** Create a whole bunch of empty entities in one go.
     * @param count     The number of entities to create
     * @return The range of entities created */
    std::pair<entity, entity> new_entities (size_t count)
        {
            auto range_begin (next_id_);
            for (; count > 0; --count)
                entities_[next_id_++];

            return std::make_pair(range_begin, next_id_);
        }

    entity clone_entity (iterator f)
        {
            elem& e (f->second);
            entities_[next_id_++] = e;

            // Quick check if we need to make deep copies
            if ((e.components & flat_mask_).any())
            {
                size_t off (0);
                for (int c_id (0); c_id < 64 && off < e.data.size(); ++c_id)
                {
                    if (e.components[c_id])
                    {
                        if (!components_[c_id].is_flat())
                        {
                            auto ptr (reinterpret_cast<placeholder*>(&*e.data.begin() + off));
                            ptr = ptr->clone();
                        }
                        off += components_[c_id].size();
                    }
                }
            }
            return next_id_ - 1;
        }

    iterator find (entity en)
        {
            auto found (entities_.find(en.id()));
            if (found == entities_.end())
                throw std::logic_error("unknown entity");

            return found;
        }

    const_iterator find (entity en) const
        {
            auto found (entities_.find(en.id()));
            if (found == entities_.end())
                throw std::logic_error("unknown entity");

            return found;
        }

    void delete_entity (entity en)
        { delete_entity(find(en)); }

    void delete_entity (iterator f)
        {
            elem& e (f->second);

            // Quick check if we'll have to call any destructors.
            if ((e.components & flat_mask_).any())
            {
                size_t off (0);
                for (int search (0); search < 64 && off < e.data.size(); ++search)
                {
                    if (e.components[search])
                    {
                        if (!components_[search].is_flat())
                        {
                            auto ptr (reinterpret_cast<placeholder*>(&*e.data.begin() + off));
                            ptr->~placeholder();
                        }
                        off += components_[search].size();
                    }
                }
            }

            entities_.erase(f);
        }

    void remove_component_from_entity (iterator en, component_id c)
        {
            auto& e (en->second);
            if (!e.components[c])
                return;

            size_t off (offset(e, c));
            auto& comp_info (components_[c]);
            if (!comp_info.is_flat())
            {
                auto ptr (reinterpret_cast<placeholder*>(&*e.data.begin() + off));
                ptr->~placeholder();
            }
            auto o (e.data.begin() + off);
            e.data.erase(o, o + comp_info.size());
            e.components.reset(c);
        }

    /** Call a function for every entity that has a given component.
     *  The callee can then query and change the value of the component through
     *  a var_ref object, or remove the entity.
     * @param c     The component to look for.
     * @param func  The function to call.  This function will be passed an 
     *              iterator to the current entity, and a var_ref corresponding
     *              to the component value in this entity. */
    template <typename t>
    void for_each (component_id c, std::function<void(iterator, var_ref<t>)> func)
        {
            std::bitset<64> mask;
            mask.set(c);
            for (auto i (entities_.begin()); i != entities_.end(); )
            {
                auto next (std::next(i));
                elem& e (i->second);
                if ((e.components & mask) == mask)
                    func(i, var_ref<t>(offset(e, c), e));

                i = next;
            }
        }

    template <typename t1, typename t2>
    void for_each (component_id c1, component_id c2,
                   std::function<void(iterator, var_ref<t1>, var_ref<t2>)> func)
        {
            std::bitset<64> mask;
            mask.set(c1);
            mask.set(c2);
            for (auto i (entities_.begin()); i != entities_.end(); )
            {
                auto next (std::next(i));
                elem& e (i->second);
                if ((e.components & mask) == mask)
                {
                    func(i, var_ref<t1>(offset(e, c1), e),
                            var_ref<t2>(offset(e, c2), e));
                }
                i = next;
            }
        }

    template <typename t1, typename t2, typename t3>
    void for_each (component_id c1, component_id c2, component_id c3,
                   std::function<void(iterator, var_ref<t1>, var_ref<t2>, var_ref<t3>)> func)
        {
            std::bitset<64> mask;
            mask.set(c1);
            mask.set(c2);
            mask.set(c3);
            for (auto i (entities_.begin()); i != entities_.end(); )
            {
                auto next (std::next(i));
                elem& e (i->second);
                if ((e.components & mask) == mask)
                {
                    func(i, var_ref<t1>(offset(e, c1), e),
                            var_ref<t2>(offset(e, c2), e),
                            var_ref<t3>(offset(e, c3), e));
                }
                i = next;
            }
        }

    bool exists (entity en) const
        { return entities_.count(en.id()); }

    template <typename type>
    void set (entity en, component_id c_id, type val)
        { return set<type>(find(en), c_id, std::move(val)); }

    template <typename type>
    void set (iterator en, component_id c_id, type val)
        {
            const component& c (components_[c_id]);
            elem&  e   (en->second);
            size_t off (offset(e, c_id));

            if (!e.components[c_id])
            {
                if (e.data.size() < off)
                    e.data.resize(off + c.size());
                else
                    e.data.insert(e.data.begin() + off, c.size(), 0);

                e.components.set(c_id);
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
                new (ptr) holder<type>(std::move(val));
            }
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

private:
    template <typename type>
    const type& get (const elem& e, component_id c_id) const
        {
            size_t off (offset(e, c_id));
            if (is_flat<type>::value)
                return *reinterpret_cast<const type*>(&*e.data.begin() + off);

            auto ptr (reinterpret_cast<const holder<type>*>(&*e.data.begin() + off));

            return ptr->held();
        }

    size_t offset (const elem& e, component_id c) const
        {
            assert(c < components_.size());
            assert((c & cache_mask) < component_offsets_.size());
            size_t result (component_offsets_[c & cache_mask]);

            for (component_id search (cache_size); search < c; ++search)
            {
                if (e.components[search])
                    result += components_[search].size();
            }
            return result;
        }

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
