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
    auto result (entities_.emplace(next_id_, elem()).first);
    if (on_new_entity)
        on_new_entity(result);

    ++next_id_;
    return next_id_ - 1;
}

storage::iterator
storage::make (uint32_t id)
{
    if (next_id_ <= id)
        next_id_ = id + 1;

    auto result (entities_.emplace(id, elem()));
    if (result.second && on_new_entity)
        on_new_entity(result.first);

    return result.first;
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
    auto cloned (entities_.emplace(next_id_, f->second).first);
    elem& e (cloned->second);

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
                    ptr->clone()->move_to(e.data.begin() + off);
                }
                off += components_[c_id].size();
            }
        }
    }
    if (on_new_entity)
        on_new_entity(cloned);

    ++next_id_;
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
    if (on_deleted_entity)
        on_deleted_entity(f);

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
    e.dirty = true;
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
    return en->second.dirty;
}

bool
storage::check_dirty_and_clear (iterator en)
{
    bool result (check_dirty(en));
    en->second.dirty = false;
    return result;
}

void
storage::serialize (const_iterator en, std::vector<char>& buffer) const
{
    auto& e (en->second);
    buffer.reserve(8 + e.data.size());
    buffer.resize(8);

    *(reinterpret_cast<uint64_t*>(&buffer[0])) = e.components.to_ullong();

    auto first (e.data.begin());
    auto last  (first);

    for (size_t i (0); i < components_.size(); ++i)
    {
        if (!e.components[i])
            continue;

        auto& c (components_[i]);
        if (c.is_flat())
        {
            // As long as we have a flat memory layout, just move the
            // end of the range.
            std::advance(last, c.size());
        }
        else
        {
            // If not, write the current range to the buffer.
            buffer.insert(buffer.end(), first, last);
            // Then serialize the object using the function the caller
            // provided.
            auto ptr (reinterpret_cast<const component::placeholder*>(&*last));
            ptr->serialize(buffer);
            // Reset the range.
            std::advance(last, c.size());
            first = last;
        }

        if (last >= e.data.end())
            break;
    }
    // Write the last bit after we're done.
    assert(last == e.data.end());
    buffer.insert(buffer.end(), first, e.data.end());
}

void
storage::deserialize (iterator en, const std::vector<char>& buffer)
{
    auto first (buffer.begin());
    auto& e (en->second);

    call_destructors(en);
    e.data.clear();
    e.components = *(reinterpret_cast<const uint64_t*>(&*first));

    std::advance(first, 8);
    auto last (first);

    for (size_t i (0); i < components_.size(); ++i)
    {
        if (!e.components[i])
            continue;

        auto& c (components_[i]);
        if (c.is_flat())
        {
            // As long as we have a flat memory layout, just move the
            // end of the range.
            std::advance(last, c.size());
        }
        else
        {
            // If not, write the current range to the entity data.
            e.data.insert(e.data.end(), first, last);
            // Create a new object for the component and deserialize the
            // data using the function the caller provided.
            auto ptr (c.clone());
            last = ptr->deserialize(last, buffer.end());
            first = last;
            // Move the object to the buffer.
            auto offset (e.data.size());
            e.data.resize(offset + c.size());
            ptr->move_to(e.data.begin() + offset);
        }

        if (last >= buffer.end())
            break;
    }
    // Write the last bit after we're done.
    assert(last == buffer.end());
    e.data.insert(e.data.end(), first, buffer.end());
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
