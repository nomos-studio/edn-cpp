// SPDX-License-Identifier: BSL-1.0
#include <edn/value.hpp>

#include <mutex>
#include <unordered_set>

namespace edn::intern {

namespace {
    std::unordered_set<std::string> table;
    std::mutex                      mu;
} // namespace

std::string_view get(std::string_view s) {
    std::lock_guard<std::mutex> lock(mu);
    auto [it, inserted] = table.emplace(s);
    (void)inserted;
    return *it;
}

} // namespace edn::intern
