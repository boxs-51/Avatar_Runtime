#include "owned_bundle.hpp"
#include <array>
#include <utf8proc.h>
namespace ars::assets {
const Member &OwnedBundle::find(std::string_view path) const {
    for (const auto &m : members)
        if (!m.directory && m.name == path)
            return m;
    fail();
}
std::pmr::string canonical_path(std::string_view name, std::pmr::memory_resource *resource,
                                const Limits &limits) {
    require(!name.empty() && name.size() <= limits.path,
            name.size() > limits.path ? ARS_E_OUT_OF_BUDGET : ARS_E_BAD_ASSET);
    require(name.front() != '/' && name.front() != '\\');
    for (unsigned char c : name)
        require(c >= 32 && c != 127 && c != '\\' && c != ':' && c != '*' && c != '?' && c != '"' &&
                c != '<' && c != '>' && c != '|');
    std::array<utf8proc_int32_t, 8192> buffer{};
    const auto options = UTF8PROC_STABLE | UTF8PROC_COMPOSE | UTF8PROC_CASEFOLD;
    const auto n = utf8proc_decompose(reinterpret_cast<const utf8proc_uint8_t *>(name.data()),
                                      static_cast<utf8proc_ssize_t>(name.size()), buffer.data(),
                                      static_cast<utf8proc_ssize_t>(buffer.size()), options);
    require(n >= 0);
    require(static_cast<std::size_t>(n) <= buffer.size(), ARS_E_OUT_OF_BUDGET);
    const auto size = utf8proc_reencode(buffer.data(), n, options);
    require(size > 0);
    require(size <= 2048, ARS_E_OUT_OF_BUDGET);
    std::pmr::string out(reinterpret_cast<const char *>(buffer.data()), static_cast<std::size_t>(size),
                         resource);
    if (out.back() == '/')
        out.pop_back();
    require(!out.empty());
    std::size_t at = 0;
    while (at < out.size()) {
        auto end = out.find('/', at);
        if (end == std::string::npos)
            end = out.size();
        const auto part = std::string_view(out).substr(at, end - at);
        require(!part.empty() && part != "." && part != ".." && part.back() != '.' && part.back() != ' ');
        const auto base = part.substr(0, part.find('.'));
        require(base != "con" && base != "prn" && base != "aux" && base != "nul");
        require(base != "conin$" && base != "conout$" && base != "clock$");
        if (base.starts_with("com") || base.starts_with("lpt")) {
            const auto digit = base.substr(3);
            require(digit != "\xc2\xb9" && digit != "\xc2\xb2" && digit != "\xc2\xb3");
        }
        require(!(base.size() == 4 && (base.substr(0, 3) == "com" || base.substr(0, 3) == "lpt") &&
                  base[3] >= '1' && base[3] <= '9'));
        at = end + 1;
    }
    for (const auto suffix : {".zip", ".arschar", ".exe", ".dll", ".so", ".dylib", ".hlsl", ".glsl", ".spv",
                              ".py", ".js", ".lua"})
        require(!out.ends_with(suffix));
    return out;
}
} // namespace ars::assets
