// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#include <spdlog/details/tag.h>

#include <string>

namespace spdlog {
class scoped_tag
{
public:
    explicit scoped_tag(std::string new_tag)
        : old_tag_(std::move(details::get_tag_()))
    {
        details::get_tag_() = std::move(new_tag);
    }

    ~scoped_tag()
    {
        details::get_tag_() = std::move(old_tag_);
    }

private:
    std::string old_tag_;
};
} // namespace spdlog
