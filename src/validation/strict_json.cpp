#include "strict_json.hpp"
#include <charconv>
#include <cmath>
#include <utf8proc.h>
namespace ars::validation {
using assets::fail;
using assets::require;
namespace {
class Parser {
    std::string_view s_;
    std::size_t p_ = 0;
    Document &d_;
    const assets::Limits &limits_;
    assets::Work &work_;
    void ws() {
        while (p_ < s_.size() && (s_[p_] == ' ' || s_[p_] == '\r' || s_[p_] == '\n' || s_[p_] == '\t')) {
            ++p_;
            work_.take();
        }
    }
    char take() {
        require(p_ < s_.size());
        work_.take();
        return s_[p_++];
    }
    unsigned hex() {
        unsigned v = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = take();
            v <<= 4;
            if (c >= '0' && c <= '9')
                v += c - '0';
            else if (c >= 'a' && c <= 'f')
                v += c - 'a' + 10;
            else if (c >= 'A' && c <= 'F')
                v += c - 'A' + 10;
            else
                fail();
        }
        return v;
    }
    std::pmr::string string() {
        require(take() == '"');
        std::pmr::string out(d_.nodes.get_allocator().resource());
        while (true) {
            const auto start = p_;
            const unsigned char c = static_cast<unsigned char>(take());
            if (c == '"')
                break;
            require(c >= 0x20);
            if (c == '\\') {
                const char e = take();
                switch (e) {
                case '"':
                case '\\':
                case '/':
                    out += e;
                    break;
                case 'b':
                    out += '\b';
                    break;
                case 'f':
                    out += '\f';
                    break;
                case 'n':
                    out += '\n';
                    break;
                case 'r':
                    out += '\r';
                    break;
                case 't':
                    out += '\t';
                    break;
                case 'u': {
                    auto cp = hex();
                    if (cp >= 0xd800 && cp <= 0xdbff) {
                        require(take() == '\\' && take() == 'u');
                        const auto low = hex();
                        require(low >= 0xdc00 && low <= 0xdfff);
                        cp = 0x10000 + (cp - 0xd800) * 1024 + low - 0xdc00;
                    } else
                        require(cp < 0xdc00 || cp > 0xdfff);
                    utf8proc_uint8_t bytes[4]{};
                    const auto n = utf8proc_encode_char(static_cast<utf8proc_int32_t>(cp), bytes);
                    require(n > 0);
                    out.append(reinterpret_cast<char *>(bytes), static_cast<std::size_t>(n));
                    break;
                }
                default:
                    fail();
                }
            } else if (c < 0x80)
                out += static_cast<char>(c);
            else {
                utf8proc_int32_t cp = 0;
                const auto n = utf8proc_iterate(reinterpret_cast<const utf8proc_uint8_t *>(s_.data() + start),
                                                static_cast<utf8proc_ssize_t>(s_.size() - start), &cp);
                require(n > 0);
                work_.take(static_cast<std::size_t>(n - 1));
                p_ = start + static_cast<std::size_t>(n);
                out.append(s_.substr(start, static_cast<std::size_t>(n)));
            }
        }
        return out;
    }
    std::size_t value(std::size_t depth) {
        require(depth <= limits_.depth, ARS_E_OUT_OF_BUDGET);
        require(d_.nodes.size() < limits_.nodes, ARS_E_OUT_OF_BUDGET);
        ws();
        require(p_ < s_.size());
        const auto index = d_.nodes.size();
        d_.nodes.emplace_back(d_.nodes.get_allocator().resource());
        const char c = s_[p_];
        if (c == '{' || c == '[') {
            ++p_;
            work_.take();
            const bool object = c == '{';
            d_.nodes[index].kind = object ? Kind::object : Kind::array;
            ws();
            const char end = object ? '}' : ']';
            if (p_ < s_.size() && s_[p_] == end) {
                ++p_;
                return index;
            }
            while (true) {
                std::pmr::string key(d_.nodes.get_allocator().resource());
                if (object) {
                    ws();
                    key = string();
                    require(key.size() <= 1024, ARS_E_OUT_OF_BUDGET);
                    for (auto child : d_.nodes[index].children) {
                        work_.take();
                        require(d_.nodes[child].key != key);
                    }
                    ws();
                    require(take() == ':');
                }
                const auto child = value(depth + 1);
                d_.nodes[child].key = std::move(key);
                d_.nodes[index].children.push_back(child);
                ws();
                const char sep = take();
                if (sep == end)
                    break;
                require(sep == ',');
            }
        } else if (c == '"') {
            d_.nodes[index].kind = Kind::string;
            d_.nodes[index].text = string();
        } else if (c == 't' || c == 'f' || c == 'n') {
            const auto token = c == 't'   ? std::string_view("true")
                               : c == 'f' ? std::string_view("false")
                                          : std::string_view("null");
            require(s_.substr(p_, token.size()) == token);
            p_ += token.size();
            work_.take(token.size());
            d_.nodes[index].kind = c == 'n' ? Kind::null_value : Kind::boolean;
            d_.nodes[index].boolean = c == 't';
        } else {
            const auto begin = p_;
            if (s_[p_] == '-')
                ++p_;
            require(p_ < s_.size());
            if (s_[p_] == '0')
                ++p_;
            else {
                require(s_[p_] >= '1' && s_[p_] <= '9');
                while (p_ < s_.size() && s_[p_] >= '0' && s_[p_] <= '9')
                    ++p_;
            }
            if (p_ < s_.size() && s_[p_] == '.') {
                ++p_;
                const auto digits = p_;
                while (p_ < s_.size() && s_[p_] >= '0' && s_[p_] <= '9')
                    ++p_;
                require(p_ > digits);
            }
            if (p_ < s_.size() && (s_[p_] == 'e' || s_[p_] == 'E')) {
                ++p_;
                if (p_ < s_.size() && (s_[p_] == '+' || s_[p_] == '-'))
                    ++p_;
                const auto digits = p_;
                while (p_ < s_.size() && s_[p_] >= '0' && s_[p_] <= '9')
                    ++p_;
                require(p_ > digits);
            }
            work_.take(p_ - begin);
            double number = 0;
            const auto parsed = std::from_chars(s_.data() + begin, s_.data() + p_, number);
            require(parsed.ec == std::errc{} && parsed.ptr == s_.data() + p_ && std::isfinite(number));
            d_.nodes[index].kind = Kind::number;
            d_.nodes[index].number = number;
        }
        return index;
    }

  public:
    Parser(std::string_view s, Document &d, const assets::Limits &l, assets::Work &w)
        : s_(s), d_(d), limits_(l), work_(w) {}
    void run() {
        require(s_.size() <= limits_.json, ARS_E_OUT_OF_BUDGET);
        d_.nodes.clear();
        value(1);
        ws();
        require(p_ == s_.size());
    }
};
} // namespace
void parse_json(std::string_view s, Document &d, const assets::Limits &l, assets::Work &w) {
    Parser(s, d, l, w).run();
}
} // namespace ars::validation
