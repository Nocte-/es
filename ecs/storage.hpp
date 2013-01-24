//---------------------------------------------------------------------------
/// \file   ecs/storage.hpp
/// \brief  The entity/component data store
//
// Copyright 2013, nocte@hippie.nu            Released under the MIT License.
//---------------------------------------------------------------------------
#pragma once

#include <algorithm>
#include <bitset>
#include <cassert>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
#include <unordered_map>

#include "component.hpp"
#include "entity.hpp"

namespace ecs {

/** A storage ties together entities and components.
 *  This storage associates two other bits of data with every entity:
 *  - A 64-bit mask that keeps track of which components are defined
 *  - A vector of bytes, holding the actual data
 *
 *  The vector of bytes tries to pack the component data as tightly as
 *  possible.  It is really fast for plain old datatypes, but it also
 *  handles nontrivial types safely.  It packs a virtual table and a
 *  pointer to some heap space in the vector, and calls the constructor
 *  and destructor as needed.
 *
 *
 */
class storage
{
private:
    struct elem
    {
        std::bitset<64>     components;
        std::vector<char>   data;
    };

    typedef component::placeholder placeholder;

    template <typename t>
    class holder : public placeholder
    {
    public:
        holder () { }
        holder (t init) : held_(std::move(init)) { }
        const t& held() const { return held_; }

    private:
        t held_;
    };

    typedef std::unordered_map<uint32_t, elem> stor_impl;

public:
    typedef uint8_t     component_id;

public:
    storage()
    {
        component_offsets_.push_back(0);
    }

    template <typename type>
    component_id register_component (std::string name)
        {
            size_t size;

            if (std::is_pod<type>().value)
            {
                size = sizeof(type);
                components_.emplace_back(name, typeid(type), size, nullptr);
            }
            else
            {
                pod_mask_.set(components_.size());
                size = sizeof(holder<type>);
                components_.emplace_back(name, typeid(type), size,
                                         std::unique_ptr<placeholder>(new holder<type>()));
            }

            if (components_.size() < 10)
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
            entities_.emplace(next_id_++, elem());
            return next_id_ - 1;
        }

    void delete_entity (entity en)
        {
            auto f (entities_.find(en.id()));
            if (f == entities_.end())
                throw std::logic_error("unknown entity");

            elem& e (f->second);

            // Quick check if we'll have to call any destructors.
            if ((e.components & pod_mask_).any())
            {
                size_t off {0};
                for (int search {0}; search < 64 && off < e.data.size(); ++search)
                {
                    if (e.components[search])
                    {
                        if (!components_[search].is_pod())
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

    bool exists (entity en) const
        { return entities_.count(en.id()); }

    template <typename type>
    void set (entity en, component_id c_id, type val)
        {
            const component& c (components_[c_id]);
            if (c.type() != typeid(type))
                throw std::logic_error("component type does not match");

            auto& e (find_elem(en));
            size_t off (offset(e, c_id));
            if (!e.components[c_id])
            {
                e.data.insert(e.data.begin() + off, c.size(), 0);
                e.components.set(c_id);
            }

            if (std::is_pod<type>().value)
            {
                new (&*e.data.begin() + off) type(std::move(val));
            }
            else
            {
                auto ptr (reinterpret_cast<holder<type>*>(&*e.data.begin() + off));
                new (ptr) holder<type>(std::move(val));
            }
        }

    template <typename type>
    const type& get (entity en, component_id c_id) const
        {
            const component& c (components_[c_id]);
            if (c.type() != typeid(type))
                throw std::logic_error("component type does not match");

            auto& e (find_elem(en));
            if (!e.components[c_id])
                throw std::logic_error("entity does not have component");

            size_t off (offset(e, c_id));
            if (std::is_pod<type>().value)
                return *reinterpret_cast<const type*>(&*e.data.begin() + off);

            auto ptr (reinterpret_cast<const holder<type>*>(&*e.data.begin() + off));

            return ptr->held();
        }

private:
    elem& find_elem (entity e)
        {
            auto f (entities_.find(e.id()));
            if (f == entities_.end())
                throw std::logic_error("unknown entity");

            return f->second;
        }

    const elem& find_elem (entity e) const
        {
            auto f (entities_.find(e.id()));
            if (f == entities_.end())
                throw std::logic_error("unknown entity");

            return f->second;
        }

    size_t offset (const elem& e, component_id c) const
        {
            size_t result {0};
            for (component_id search {0}; search != c; ++search)
            {
                if (e.components[search])
                    result += components_[search].size();
            }
            assert(e.data.size() >= result);
            return result;
        }

private:
    /** Keeps track of entity IDs to give out. */
    uint32_t                            next_id_ { 0 };
    /** The list of registered components. */
    std::vector<component>              components_;
    /** Mapping entity IDs to their data. */
    std::unordered_map<uint32_t, elem>  entities_;
    /** A lookup table for the data offsets of components.
     *  The index is the bitmask as used in elem::components. */
    std::vector<size_t>                 component_offsets_;
    /** A bitmask to quickly determine whether a certain combination of
     ** components has any non-POD data types. */
    std::bitset<64>                     pod_mask_;
};

} // namespace ecs
