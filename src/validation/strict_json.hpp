#pragma once
#include "../assets/asset_limits.hpp"
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>
namespace ars::validation {
enum class Kind { null_value, boolean, number, string, array, object };
struct Node {
    Kind kind = Kind::null_value;
    double number = 0;
    bool boolean = false;
    std::pmr::string text, key;
    std::pmr::vector<std::size_t> children;
    explicit Node(std::pmr::memory_resource *r) : text(r), key(r), children(r) {}
    Node(Node &&) = default;
    Node &operator=(Node &&) = default;
    Node(const Node &) = delete;
};
class Value;
struct Document {
    std::pmr::vector<Node> nodes;
    explicit Document(std::pmr::memory_resource *r) : nodes(r) {}
    Document(const Document &) = delete;
    Value root() const;
};
class Value {
    const Document *doc_;
    std::size_t index_;

  public:
    Value(const Document &doc, std::size_t index) : doc_(&doc), index_(index) {}
    const Node &node() const { return doc_->nodes.at(index_); }
    Kind kind() const { return node().kind; }
    bool has(std::string_view key) const;
    Value at(std::string_view key) const;
    Value at(std::size_t i) const;
    std::size_t size() const { return node().children.size(); }
    std::string_view string() const {
        assets::require(kind() == Kind::string);
        return node().text;
    }
    double number() const {
        assets::require(kind() == Kind::number);
        return node().number;
    }
    std::uint32_t u32() const;
    bool boolean() const {
        assets::require(kind() == Kind::boolean);
        return node().boolean;
    }
};
void parse_json(std::string_view, Document &, const assets::Limits &, assets::Work &);
} // namespace ars::validation
