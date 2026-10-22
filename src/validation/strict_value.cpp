#include "strict_json.hpp"
#include <cmath>
namespace ars::validation {
using assets::fail;
using assets::require;
Value Document::root() const {
    require(!nodes.empty());
    return {*this, 0};
}
bool Value::has(std::string_view key) const {
    require(kind() == Kind::object);
    for (auto i : node().children)
        if (doc_->nodes[i].key == key)
            return true;
    return false;
}
Value Value::at(std::string_view key) const {
    require(kind() == Kind::object);
    for (auto i : node().children)
        if (doc_->nodes[i].key == key)
            return {*doc_, i};
    fail();
}
Value Value::at(std::size_t i) const {
    require(kind() == Kind::array && i < size());
    return {*doc_, node().children[i]};
}
std::uint32_t Value::u32() const {
    const auto n = number();
    require(n >= 0 && n <= UINT32_MAX && std::floor(n) == n);
    return static_cast<std::uint32_t>(n);
}
}
