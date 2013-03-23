//---------------------------------------------------------------------------
/// \file   es/storage.hpp
/// \brief  The entity/component data store
//
// Copyright 2013, nocte@hippie.nu            Released under the MIT License.
//---------------------------------------------------------------------------
#include "storage.hpp"

namespace es {

storage::storage()
    : next_id_(0)
{
    component_offsets_.push_back(0);
}

storage::~storage()
{
    for (auto i (entities_.begin()); i != entities_.end(); ++i)
        call_destructors(i);
}

storage::component_id
storage::find_component (const std::string& name) const
{
    auto found (std::find(components_.begin(), components_.end(), name));
    if (found == components_.end())
        throw std::logic_error("component does not exist");

    return std::distance(components_.begin(), found);
}

entity
storage::new_entity()
{
    entities_[next_id_++];
    return next_id_ - 1;
}

storage::iterator
storage::make (uint32_t id)
{
    if (next_id_ <= id)
        next_id_ = id + 1;

    return entities_.insert(entities_.end(), std::make_pair(id, elem()));
}

std::pair<entity, entity>
storage::new_entities (size_t count)
{
    auto range_begin (next_id_);
    for (; count > 0; --count)
        entities_[next_id_++];

    return std::make_pair(range_begin, next_id_);
}

entity
storage::clone_entity (iterator f)
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

storage::iterator
storage::find (entity en)
{
    auto found (entities_.find(en));
    if (found == entities_.end())
        throw std::logic_error("unknown entity");

    return found;
}

storage::const_iterator
storage::find (entity en) const
{
    auto found (entities_.find(en));
    if (found == entities_.end())
        throw std::logic_error("unknown entity");

    return found;
}

size_t
storage::size() const
{
    return entities_.size();
}

bool
storage::delete_entity (entity en)
{
    auto found (find(en));
    if (found != entities_.end())
    {
        delete_entity(found);
        return true;
    }
    return false;
}

void
storage::delete_entity (iterator f)
{
    call_destructors(f);
    entities_.erase(f);
}

void
storage::remove_component_from_entity (iterator en, component_id c)
{
    auto& e (en->second);
    if (!e.components[c])
        return;

    size_t off (offset(e, c));
    auto& comp_info (components_[c]);
    if (!comp_info.is_flat())
    {
        auto ptr (reinterpret_cast<placeholder*>(&*e.data.begin() + off));
        if (ptr)
            ptr->~placeholder();
    }
    auto o (e.data.begin() + off);
    e.data.erase(o, o + comp_info.size());
    e.components.reset(c);
}

bool
storage::entity_has_component (iterator en, component_id c) const
{
    return c < components_.size() && en->second.components.test(c);
}

size_t
storage::offset (const elem& e, component_id c) const
{
    assert(c < components_.size());
    assert((c & cache_mask) < component_offsets_.size());

    size_t result (component_offsets_[((1 << c) - 1) & e.components.to_ulong() & cache_mask]);

    for (component_id search (cache_size); search < c; ++search)
    {
        if (e.components[search])
            result += components_[search].size();
    }

    return result;
}

bool
storage::check_dirty (iterator en)
{
    return en->second.dirty.any();
}

bool
storage::check_dirty_and_clear (iterator en)
{
    bool result (check_dirty(en));
    en->second.dirty.reset();
    return result;
}

bool
storage::check_dirty_flag (iterator en, component_id c_id)
{
    return en->second.dirty[c_id];
}

bool
storage::check_dirty_flag_and_clear (iterator en, component_id c_id)
{
    bool result (check_dirty_flag(en, c_id));
    en->second.dirty.reset(c_id);
    return result;
}

void
storage::call_destructors(iterator i) const
{
    elem& e (i->second);

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
}

} // namespace es
