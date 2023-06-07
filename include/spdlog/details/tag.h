// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#include <string>

namespace spdlog {
namespace details {
inline std::string &get_tag_() {
    thread_local std::string tag;
    return tag;
}
} // namespace details
} // namespace spdlog
