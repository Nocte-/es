//---------------------------------------------------------------------------
/// \file   ecs/component.hpp
/// \brief  A component
//
// Copyright 2013, nocte@hippie.nu            Released under the MIT License.
//---------------------------------------------------------------------------
#pragma once

#include <memory>
#include <string>
#include <typeinfo>

namespace ecs {

class storage;

class component
{
    friend class storage;

protected:
    class placeholder
    {
    public:
        virtual ~placeholder() { }
    };

public:
    component(std::string name, size_t size, std::unique_ptr<placeholder> ph)
        : name_(name)
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

} // namespace ecs

