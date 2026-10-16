#include "glb_validator.hpp"
#include "manifest_validator.hpp"
#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstring>
#include <limits>
namespace ars::validation {
using assets::glb_none;
using assets::require;
namespace {
constexpr double float_max = 3.4028234663852886e38;
std::size_t width(std::uint32_t component) {
    switch (component) {
    case 5120:
    case 5121:
        return 1;
    case 5122:
    case 5123:
        return 2;
    case 5125:
    case 5126:
        return 4;
    default:
        assets::fail();
    }
}
double normalized(const assets::GlbAccessor &a, double value) {
    if (!a.normalized)
        return value;
    switch (a.component) {
    case 5120:
        return std::max(value / 127., -1.);
    case 5121:
        return value / 255.;
    case 5122:
        return std::max(value / 32767., -1.);
    case 5123:
        return value / 65535.;
    default:
        assets::fail();
    }
}
struct Context {
    const Document &document;
    Value root;
    std::span<const std::uint8_t> bin;
    assets::GlbData &data;
    assets::Resource &resource;
    const assets::Limits &limits;
    assets::Work &work;
    bool disabled = false;
    std::pmr::vector<std::uint32_t> parents;
    std::pmr::vector<unsigned char> view_use, vertex_accessors;
    std::pmr::vector<std::uint32_t> vertex_view_counts;
    Context(const Document &doc, std::span<const std::uint8_t> b, assets::GlbData &d, assets::Resource &r,
            const assets::Limits &l, assets::Work &w)
        : document(doc), root(doc.root()), bin(b), data(d), resource(r), limits(l), work(w), parents(&r),
          view_use(&r), vertex_accessors(&r), vertex_view_counts(&r) {}
    Value child(std::size_t index) const { return Value(document, index); }
    void object(Value v) const { require(v.kind() == Kind::object); }
    std::size_t array(Value v, bool empty = false) {
        require(v.kind() == Kind::array && (empty || v.size() > 0));
        require(v.size() <= 65536, ARS_E_OUT_OF_BUDGET);
        work.take(v.size());
        return v.size();
    }
    std::size_t count(Value v, std::string_view key) { return v.has(key) ? array(v.at(key)) : 0; }
    std::uint32_t number(Value v, std::string_view key, std::uint32_t fallback = 0) const {
        return v.has(key) ? v.at(key).u32() : fallback;
    }
    std::uint32_t reference(Value v, std::string_view key, std::size_t count) const {
        if (!v.has(key))
            return glb_none;
        const auto index = v.at(key).u32();
        require(index < count);
        return index;
    }
    double scalar(Value v) const {
        const auto n = v.number();
        require(std::abs(n) <= float_max);
        return n;
    }
    void name(Value v, std::pmr::string &out) const {
        if (v.has("name")) {
            auto s = v.at("name").string();
            require(s.size() <= 1024 && s.find('\0') == s.npos);
            out = s;
        }
    }
    template <std::size_t N> void vector(Value v, std::array<double, N> &out) {
        require(array(v) == N);
        for (std::size_t i = 0; i < N; ++i)
            out[i] = scalar(v.at(i));
    }
    static void quaternion(const double *v) {
        double norm = 0;
        for (std::size_t i = 0; i < 4; ++i)
            norm += v[i] * v[i];
        require(std::abs(norm - 1) <= 0.001);
    }
    void range(std::size_t offset, std::size_t count, std::size_t stride, std::size_t extent,
               std::size_t length) const {
        require(count > 0 && offset <= length && extent <= length - offset);
        require(count - 1 <= (length - offset - extent) / stride);
    }
    std::uint32_t bits(std::size_t at, std::size_t n) const {
        require(at <= bin.size() && n <= bin.size() - at);
        std::uint32_t v = 0;
        for (std::size_t i = 0; i < n; ++i)
            v |= std::uint32_t(bin[at + i]) << (8 * i);
        return v;
    }
    double component(std::size_t at, std::uint32_t kind) const {
        auto value = bits(at, width(kind));
        if (kind == 5126) {
            const float f = std::bit_cast<float>(value);
            require(std::isfinite(f));
            return f;
        }
        if (kind == 5120)
            return value < 128 ? double(value) : double(value) - 256.;
        if (kind == 5122)
            return value < 32768 ? double(value) : double(value) - 65536.;
        return value;
    }
    const assets::GlbView &view(std::uint32_t index) const {
        require(index < data.views.size());
        return data.views[index];
    }
    const assets::GlbAccessor &accessor(std::uint32_t index) const {
        require(index < data.accessors.size());
        return data.accessors[index];
    }
    void use(std::uint32_t index, unsigned char kind) {
        const auto &a = accessor(index);
        if (a.view == glb_none)
            return;
        auto &mask = view_use[a.view];
        require(mask == 0 || mask == kind);
        mask = kind;
        const auto &v = view(a.view);
        if (kind == 1) {
            const auto bytes = width(a.component) * a.components;
            require(a.offset % 4 == 0 && (v.offset + a.offset) % 4 == 0 &&
                    (v.stride ? v.stride : bytes) % 4 == 0);
            require(v.target == 0 || v.target == 34962);
            if (!vertex_accessors[index]) {
                vertex_accessors[index] = 1;
                if (++vertex_view_counts[a.view] > 1)
                    require(v.stride != 0);
            }
        } else {
            require(v.stride == 0);
            require(v.target == 0 || (kind == 2 && v.target == 34963));
        }
    }
    void extensions() {
        object(root);
        object(root.at("asset"));
        require(root.at("asset").at("version").string() == "2.0");
        for (auto key : {"copyright", "generator"})
            if (root.at("asset").has(key))
                (void)root.at("asset").at(key).string();
        for (auto key : {"buffers", "bufferViews", "accessors", "meshes", "nodes", "skins", "animations",
                         "scenes", "images", "textures", "samplers", "materials", "cameras"})
            if (root.has(key)) {
                auto objects = root.at(key);
                array(objects);
                for (std::size_t i = 0; i < objects.size(); ++i) {
                    auto v = objects.at(i);
                    object(v);
                    if (v.has("name")) {
                        auto s = v.at("name").string();
                        require(s.size() <= 1024 && s.find('\0') == s.npos);
                    }
                }
            }
        if (root.at("asset").has("minVersion"))
            require(root.at("asset").at("minVersion").string() == "2.0");
        std::pmr::vector<std::size_t> pending(&resource);
        pending.push_back(0);
        while (!pending.empty()) {
            const auto current = pending.back();
            pending.pop_back();
            const auto &node = document.nodes[current];
            work.take();
            if (node.key == "extras")
                continue;
            for (auto index : node.children) {
                if (node.kind == Kind::object && document.nodes[index].key == "extensions") {
                    const auto ext = child(index);
                    object(ext);
                    for (auto entry : ext.node().children) {
                        work.take();
                        require(!document.nodes[entry].key.empty());
                        object(child(entry));
                    }
                } else
                    pending.push_back(index);
            }
        }
        for (auto key : {"extensionsUsed", "extensionsRequired"})
            if (root.has(key)) {
                auto v = root.at(key);
                array(v);
                for (std::size_t i = 0; i < v.size(); ++i) {
                    auto s = v.at(i).string();
                    require(!s.empty() && s.size() <= 256 && s.find('\0') == s.npos);
                    for (std::size_t j = 0; j < i; ++j) {
                        work.take();
                        require(v.at(j).string() != s);
                    }
                }
            }
        if (root.has("extensionsRequired"))
            for (std::size_t i = 0; i < root.at("extensionsRequired").size(); ++i) {
                require(root.has("extensionsUsed"));
                bool found = false;
                for (std::size_t j = 0; j < root.at("extensionsUsed").size(); ++j) {
                    work.take();
                    found |= root.at("extensionsRequired").at(i).string() ==
                             root.at("extensionsUsed").at(j).string();
                }
                require(found);
                disabled = true;
            }
    }
    void buffers() {
        const auto n = count(root, "buffers");
        require(n <= 1);
        if (n) {
            auto b = root.at("buffers").at(0);
            object(b);
            require(!b.has("uri"));
            const auto size = b.at("byteLength").u32();
            require(size > 0 && size <= bin.size() && bin.size() - size <= 3);
            for (std::size_t i = size; i < bin.size(); ++i)
                require(bin[i] == 0);
            bin = bin.first(size);
        } else
            require(bin.empty());
        const auto views = count(root, "bufferViews");
        data.views.reserve(views);
        for (std::size_t i = 0; i < views; ++i) {
            auto v = root.at("bufferViews").at(i);
            object(v);
            require(n == 1 && v.at("buffer").u32() == 0);
            assets::GlbView out;
            out.offset = number(v, "byteOffset");
            out.length = v.at("byteLength").u32();
            out.stride = number(v, "byteStride");
            out.target = number(v, "target");
            require(out.length > 0 && out.offset <= bin.size() && out.length <= bin.size() - out.offset);
            if (v.has("byteStride"))
                require(out.stride >= 4 && out.stride <= 252 && out.stride % 4 == 0);
            require(!v.has("target") || out.target == 34962 || out.target == 34963);
            data.views.push_back(out);
        }
        view_use.resize(views);
        vertex_view_counts.resize(views);
    }
    void accessors() {
        const auto n = count(root, "accessors");
        data.accessors.reserve(n);
        vertex_accessors.resize(n);
        for (std::size_t i = 0; i < n; ++i) {
            auto v = root.at("accessors").at(i);
            object(v);
            data.accessors.emplace_back(&resource);
            auto &a = data.accessors.back();
            a.count = v.at("count").u32();
            require(a.count > 0);
            a.component = v.at("componentType").u32();
            const auto size = width(a.component);
            auto type = v.at("type").string();
            a.rows = type == "SCALAR"                   ? 1
                     : type == "VEC2" || type == "MAT2" ? 2
                     : type == "VEC3" || type == "MAT3" ? 3
                     : type == "VEC4" || type == "MAT4" ? 4
                                                        : 0;
            require(a.rows > 0);
            a.columns = type.starts_with("MAT") ? a.rows : 1;
            a.components = a.rows * a.columns;
            a.normalized = v.has("normalized") ? v.at("normalized").boolean() : false;
            require(!a.normalized || a.component == 5120 || a.component == 5121 || a.component == 5122 ||
                    a.component == 5123);
            a.offset = number(v, "byteOffset");
            const auto column = a.columns > 1 ? (a.rows * size + 3) / 4 * 4 : a.rows * size;
            const auto extent = (a.columns - 1) * column + a.rows * size, element = column * a.columns;
            require(a.count <= work.remaining / a.components, ARS_E_OUT_OF_BUDGET);
            work.take(std::size_t(a.count) * a.components);
            a.values.resize(std::size_t(a.count) * a.components);
            if (v.has("bufferView")) {
                a.view = v.at("bufferView").u32();
                const auto &b = view(a.view);
                const auto stride = b.stride ? b.stride : element;
                require(stride >= element && stride % size == 0 && a.offset % size == 0 &&
                        (b.offset + a.offset) % size == 0);
                if (a.columns > 1)
                    require((b.offset + a.offset) % 4 == 0);
                range(a.offset, a.count, stride, extent, b.length);
                for (std::size_t j = 0; j < a.count; ++j)
                    for (std::size_t k = 0; k < a.components; ++k)
                        a.values[j * a.components + k] = component(
                            b.offset + a.offset + j * stride + (k / a.rows) * column + (k % a.rows) * size,
                            a.component);
            } else
                require(a.offset == 0);
            if (v.has("sparse")) {
                auto sparse = v.at("sparse");
                object(sparse);
                const auto count = sparse.at("count").u32();
                require(count > 0 && count <= a.count);
                auto indices = sparse.at("indices"), values = sparse.at("values");
                object(indices);
                object(values);
                const auto kind = indices.at("componentType").u32();
                require(kind == 5121 || kind == 5123 || kind == 5125);
                const auto isize = width(kind);
                const auto ii = indices.at("bufferView").u32(), vi = values.at("bufferView").u32();
                const auto &ib = view(ii), &vb = view(vi);
                require(ib.stride == 0 && vb.stride == 0 && ib.target == 0 && vb.target == 0);
                const auto io = number(indices, "byteOffset"), vo = number(values, "byteOffset");
                require(io % isize == 0 && (ib.offset + io) % isize == 0 && vo % size == 0 &&
                        (vb.offset + vo) % size == 0);
                if (a.columns > 1)
                    require((vb.offset + vo) % 4 == 0);
                range(io, count, isize, isize, ib.length);
                range(vo, count, element, extent, vb.length);
                std::uint32_t previous = 0;
                for (std::size_t j = 0; j < count; ++j) {
                    work.take(a.components + 1);
                    const auto index = bits(ib.offset + io + j * isize, isize);
                    require(index < a.count && (j == 0 || index > previous));
                    previous = index;
                    for (std::size_t k = 0; k < a.components; ++k)
                        a.values[std::size_t(index) * a.components + k] = component(
                            vb.offset + vo + j * element + (k / a.rows) * column + (k % a.rows) * size,
                            a.component);
                }
            }
            for (auto key : {"min", "max"})
                if (v.has(key)) {
                    auto bounds = v.at(key);
                    require(array(bounds) == a.components);
                    for (std::size_t k = 0; k < a.components; ++k) {
                        auto actual = a.values[k];
                        for (std::size_t j = 1; j < a.count; ++j) {
                            work.take();
                            actual = key == std::string_view("min")
                                         ? std::min(actual, a.values[j * a.components + k])
                                         : std::max(actual, a.values[j * a.components + k]);
                        }
                        const auto declared = scalar(bounds.at(k));
                        require(a.component == 5126 ? double(static_cast<float>(declared)) == actual
                                                    : declared == actual);
                    }
                }
        }
    }
    void meshes();
    void nodes();
    void skins();
    void animation();
    void images_and_materials();
};
void Context::meshes() {
    const auto n = count(root, "meshes");
    data.meshes.reserve(n);
    std::size_t total = 0;
    const auto materials = count(root, "materials");
    for (std::size_t i = 0; i < n; ++i) {
        auto source = root.at("meshes").at(i);
        object(source);
        data.meshes.emplace_back(&resource);
        auto &mesh = data.meshes.back();
        name(source, mesh.name);
        auto primitives = source.at("primitives");
        array(primitives);
        mesh.primitives.reserve(primitives.size());
        std::size_t targets = 0;
        for (std::size_t p = 0; p < primitives.size(); ++p) {
            auto v = primitives.at(p);
            object(v);
            auto attrs = v.at("attributes");
            object(attrs);
            require(attrs.has("POSITION"));
            mesh.primitives.emplace_back(&resource);
            auto &out = mesh.primitives.back();
            out.position = attrs.at("POSITION").u32();
            const auto &position = accessor(out.position);
            require(position.component == 5126 && position.components == 3 && !position.normalized);
            auto declared = root.at("accessors").at(out.position);
            require(declared.has("min") && declared.has("max"));
            require(position.count <= limits.vertices - total, ARS_E_OUT_OF_BUDGET);
            total += position.count;
            std::pmr::vector<double> weights_sum(&resource);
            for (auto entry : attrs.node().children) {
                work.take();
                const auto key = std::string_view(document.nodes[entry].key);
                const auto index = child(entry).u32();
                const auto &a = accessor(index);
                require(a.count == position.count && a.columns == 1);
                use(index, 1);
                for (auto prefix : {"TEXCOORD_", "COLOR_", "JOINTS_", "WEIGHTS_"})
                    if (key.starts_with(prefix)) {
                        const auto suffix = key.substr(std::string_view(prefix).size());
                        require(!suffix.empty() && (suffix.size() == 1 || suffix.front() != '0'));
                        for (char c : suffix)
                            require(c >= '0' && c <= '9');
                        std::uint32_t set = 0;
                        const auto parsed =
                            std::from_chars(suffix.data(), suffix.data() + suffix.size(), set);
                        require(parsed.ec == std::errc{} && set < attrs.size());
                        if (set > 0) {
                            char previous[11]{};
                            const auto end = std::to_chars(previous, previous + sizeof(previous), set - 1);
                            require(end.ec == std::errc{});
                            std::pmr::string prior(prefix, &resource);
                            prior.append(previous, end.ptr);
                            require(attrs.has(prior));
                        }
                    }
                const bool floating = a.component == 5126 && !a.normalized;
                const bool packed = (a.component == 5121 || a.component == 5123) && a.normalized;
                if (key == "POSITION")
                    continue;
                if (key == "NORMAL") {
                    require(floating && a.components == 3);
                    out.normal = index;
                    for (std::size_t j = 0; j < a.count; ++j) {
                        work.take();
                        double norm = 0;
                        for (std::size_t k = 0; k < 3; ++k)
                            norm += a.values[j * 3 + k] * a.values[j * 3 + k];
                        require(std::abs(norm - 1) <= 0.002);
                    }
                } else if (key == "TANGENT") {
                    require(floating && a.components == 4);
                    out.tangent = index;
                    for (std::size_t j = 0; j < a.count; ++j) {
                        work.take();
                        const auto *x = a.values.data() + j * 4;
                        require(std::abs(x[0] * x[0] + x[1] * x[1] + x[2] * x[2] - 1) <= 0.002 &&
                                (x[3] == 1 || x[3] == -1));
                    }
                } else if (key.starts_with("TEXCOORD_")) {
                    require((floating || packed) && a.components == 2);
                    if (key == "TEXCOORD_0")
                        out.uv = index;
                } else if (key.starts_with("COLOR_")) {
                    require((floating || packed) && (a.components == 3 || a.components == 4));
                    for (auto value : a.values) {
                        work.take();
                        const auto c = normalized(a, value);
                        require(c >= 0 && c <= 1);
                    }
                    if (key == "COLOR_0")
                        out.color = index;
                } else if (key.starts_with("JOINTS_")) {
                    require((a.component == 5121 || a.component == 5123) && !a.normalized &&
                            a.components == 4);
                    std::pmr::string pair("WEIGHTS_", &resource);
                    pair.append(key.substr(7));
                    require(attrs.has(pair));
                    if (key == "JOINTS_0")
                        out.joints = index;
                    else
                        disabled = true;
                } else if (key.starts_with("WEIGHTS_")) {
                    require((floating || packed) && a.components == 4);
                    std::pmr::string pair("JOINTS_", &resource);
                    pair.append(key.substr(8));
                    require(attrs.has(pair));
                    if (weights_sum.empty())
                        weights_sum.resize(a.count);
                    for (std::size_t j = 0; j < a.count; ++j)
                        for (std::size_t k = 0; k < 4; ++k) {
                            work.take();
                            const auto c = normalized(a, a.values[j * 4 + k]);
                            require(c >= 0 && c <= 1);
                            weights_sum[j] += c;
                        }
                    if (key == "WEIGHTS_0")
                        out.weights = index;
                    else
                        disabled = true;
                } else
                    require(key.starts_with("_") && a.component != 5125);
            }
            require((out.joints == glb_none) == (out.weights == glb_none));
            for (auto sum : weights_sum) {
                work.take();
                require(std::abs(sum - 1) <= 0.002);
            }
            if (!weights_sum.empty())
                require(out.weights != glb_none && out.joints != glb_none);
            out.material = reference(v, "material", materials);
            out.mode = number(v, "mode", 4);
            require(out.mode <= 6);
            disabled |= out.mode != 4;
            if (v.has("indices")) {
                const auto index = v.at("indices").u32();
                const auto &a = accessor(index);
                use(index, 2);
                require((a.component == 5121 || a.component == 5123 || a.component == 5125) &&
                        a.components == 1 && !a.normalized);
                out.indices.reserve(a.count);
                const double restart = a.component == 5121   ? 255.
                                       : a.component == 5123 ? 65535.
                                                             : 4294967295.;
                for (auto value : a.values) {
                    work.take();
                    require(value < position.count && value != restart);
                    out.indices.push_back(static_cast<std::uint32_t>(value));
                }
            } else {
                work.take(position.count);
                out.indices.resize(position.count);
                for (std::uint32_t j = 0; j < position.count; ++j)
                    out.indices[j] = j;
            }
            const auto indices = out.indices.size();
            require(indices > 0);
            if (out.mode == 4)
                require(indices >= 3 && indices % 3 == 0);
            else if (out.mode == 1)
                require(indices >= 2 && indices % 2 == 0);
            else if (out.mode >= 5)
                require(indices >= 3);
            else if (out.mode == 2 || out.mode == 3)
                require(indices >= 2);
            const auto morphs = count(v, "targets");
            if (p == 0)
                targets = morphs;
            else
                require(targets == morphs);
            require(morphs <= 64, ARS_E_OUT_OF_BUDGET);
            for (std::size_t m = 0; m < morphs; ++m) {
                auto morph = v.at("targets").at(m);
                object(morph);
                require(!morph.node().children.empty());
                assets::GlbMorph target;
                for (auto entry : morph.node().children) {
                    work.take();
                    const auto key = std::string_view(document.nodes[entry].key);
                    const auto index = child(entry).u32();
                    const auto &a = accessor(index);
                    require(a.component == 5126 && !a.normalized && a.components == 3 &&
                            a.count == position.count);
                    use(index, 1);
                    if (key == "POSITION") {
                        const auto morph_bounds = root.at("accessors").at(index);
                        require(morph_bounds.has("min") && morph_bounds.has("max"));
                        target.position = index;
                    } else if (key == "NORMAL") {
                        require(out.normal != glb_none);
                        target.normal = index;
                    } else if (key == "TANGENT") {
                        require(out.tangent != glb_none);
                        target.tangent = index;
                    } else
                        assets::fail();
                }
                out.morphs.push_back(target);
            }
        }
        mesh.weights.resize(targets);
        if (source.has("weights")) {
            auto w = source.at("weights");
            require(array(w) == targets);
            for (std::size_t j = 0; j < targets; ++j)
                mesh.weights[j] = static_cast<float>(scalar(w.at(j)));
        }
        if (source.has("extras") && source.at("extras").kind() == Kind::object &&
            source.at("extras").has("targetNames")) {
            auto names = source.at("extras").at("targetNames");
            require(array(names) == targets);
            for (std::size_t j = 0; j < targets; ++j) {
                auto s = names.at(j).string();
                require(!s.empty() && s.size() <= 128 && s.find('\0') == s.npos);
                for (const auto &old : mesh.target_names) {
                    work.take();
                    require(old != s);
                }
                mesh.target_names.emplace_back(s);
            }
        }
    }
}
void Context::nodes() {
    const auto n = count(root, "nodes");
    data.nodes.reserve(n);
    parents.assign(n, glb_none);
    const auto cameras = count(root, "cameras");
    const auto skins = count(root, "skins");
    for (std::size_t i = 0; i < n; ++i) {
        auto v = root.at("nodes").at(i);
        object(v);
        data.nodes.emplace_back(&resource);
        auto &node = data.nodes.back();
        name(v, node.name);
        node.mesh = reference(v, "mesh", data.meshes.size());
        node.skin = reference(v, "skin", skins);
        if (v.has("camera"))
            require(v.at("camera").u32() < cameras);
        node.has_matrix = v.has("matrix");
        if (node.has_matrix) {
            require(!v.has("translation") && !v.has("rotation") && !v.has("scale"));
            vector(v.at("matrix"), node.matrix);
            require(node.matrix[3] == 0 && node.matrix[7] == 0 && node.matrix[11] == 0 &&
                    node.matrix[15] == 1);
            for (std::size_t a = 0; a < 3; ++a)
                for (std::size_t b = a + 1; b < 3; ++b) {
                    double dot = 0, na = 0, nb = 0;
                    for (std::size_t k = 0; k < 3; ++k) {
                        const auto x = node.matrix[a * 4 + k], y = node.matrix[b * 4 + k];
                        dot += x * y;
                        na += x * x;
                        nb += y * y;
                    }
                    require(std::abs(dot) <= 0.0001 * std::max(1., std::sqrt(na * nb)));
                }
        } else {
            if (v.has("translation"))
                vector(v.at("translation"), node.translation);
            if (v.has("scale"))
                vector(v.at("scale"), node.scale);
            if (v.has("rotation")) {
                vector(v.at("rotation"), node.rotation);
                quaternion(node.rotation.data());
            }
        }
        const auto children = count(v, "children");
        for (std::size_t j = 0; j < children; ++j) {
            const auto index = v.at("children").at(j).u32();
            require(index < n && index != i && parents[index] == glb_none);
            parents[index] = static_cast<std::uint32_t>(i);
            node.children.push_back(index);
        }
        if (v.has("weights")) {
            require(node.mesh != glb_none);
            auto weights = v.at("weights");
            require(array(weights) == data.meshes[node.mesh].weights.size());
            for (std::size_t j = 0; j < weights.size(); ++j)
                node.weights.push_back(static_cast<float>(scalar(weights.at(j))));
        }
    }
    std::pmr::vector<std::uint32_t> ready(&resource);
    ready.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        if (parents[i] == glb_none)
            ready.push_back(static_cast<std::uint32_t>(i));
    for (std::size_t i = 0; i < ready.size(); ++i)
        for (auto next : data.nodes[ready[i]].children) {
            work.take();
            ready.push_back(next);
        }
    require(ready.size() == n);
    const auto scenes = count(root, "scenes");
    data.scenes.reserve(scenes);
    for (std::size_t i = 0; i < scenes; ++i) {
        auto v = root.at("scenes").at(i);
        object(v);
        data.scenes.emplace_back(&resource);
        auto &scene = data.scenes.back();
        const auto roots = count(v, "nodes");
        for (std::size_t j = 0; j < roots; ++j) {
            const auto index = v.at("nodes").at(j).u32();
            require(index < n && parents[index] == glb_none);
            for (auto previous : scene.roots) {
                work.take();
                require(previous != index);
            }
            scene.roots.push_back(index);
        }
    }
    data.scene = reference(root, "scene", scenes);
}
void Context::skins() {
    const auto n = count(root, "skins");
    data.skins.reserve(n);
    std::size_t joints = 0;
    auto ancestor = [&](std::uint32_t node, std::uint32_t target) {
        while (node != glb_none) {
            work.take();
            if (node == target)
                return true;
            node = parents[node];
        }
        return false;
    };
    auto root_of = [&](std::uint32_t node) {
        while (parents[node] != glb_none) {
            work.take();
            node = parents[node];
        }
        return node;
    };
    for (std::size_t i = 0; i < n; ++i) {
        auto v = root.at("skins").at(i);
        object(v);
        data.skins.emplace_back(&resource);
        auto &skin = data.skins.back();
        name(v, skin.name);
        const auto ids = v.at("joints");
        array(ids);
        require(ids.size() <= 256 - joints, ARS_E_OUT_OF_BUDGET);
        joints += ids.size();
        for (std::size_t j = 0; j < ids.size(); ++j) {
            auto index = ids.at(j).u32();
            require(index < data.nodes.size());
            for (auto old : skin.joints) {
                work.take();
                require(index != old);
            }
            skin.joints.push_back(index);
        }
        skin.skeleton = reference(v, "skeleton", data.nodes.size());
        const auto common = root_of(skin.joints.front());
        for (auto index : skin.joints) {
            require(root_of(index) == common);
            if (skin.skeleton != glb_none)
                require(ancestor(index, skin.skeleton));
        }
        skin.inverse_bind = reference(v, "inverseBindMatrices", data.accessors.size());
        if (skin.inverse_bind != glb_none) {
            const auto &a = accessor(skin.inverse_bind);
            use(skin.inverse_bind, 8);
            require(a.component == 5126 && !a.normalized && a.rows == 4 && a.columns == 4 &&
                    a.count >= ids.size());
            for (std::size_t j = 0; j < a.count; ++j) {
                work.take();
                const auto *m = a.values.data() + j * 16;
                require(m[3] == 0 && m[7] == 0 && m[11] == 0 && m[15] == 1);
            }
        }
    }
    for (const auto &node : data.nodes)
        if (node.skin != glb_none) {
            require(node.skin < data.skins.size() && node.mesh != glb_none);
            const auto &skin = data.skins[node.skin];
            for (std::size_t p = 0; p < data.meshes[node.mesh].primitives.size(); ++p) {
                const auto &primitive = data.meshes[node.mesh].primitives[p];
                require(primitive.joints != glb_none && primitive.weights != glb_none);
                const auto attrs = root.at("meshes").at(node.mesh).at("primitives").at(p).at("attributes");
                for (auto entry : attrs.node().children)
                    if (std::string_view(document.nodes[entry].key).starts_with("JOINTS_")) {
                        const auto &a = accessor(child(entry).u32());
                        for (auto index : a.values) {
                            work.take();
                            require(index < skin.joints.size());
                        }
                    }
            }
        }
}
void Context::animation() {
    const auto n = count(root, "animations");
    data.animations.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        auto source = root.at("animations").at(i);
        object(source);
        data.animations.emplace_back(&resource);
        auto &animation = data.animations.back();
        name(source, animation.name);
        const auto samplers = source.at("samplers");
        array(samplers);
        for (std::size_t s = 0; s < samplers.size(); ++s) {
            auto v = samplers.at(s);
            object(v);
            assets::GlbAnimationSampler sampler;
            sampler.input = v.at("input").u32();
            sampler.output = v.at("output").u32();
            const auto &input = accessor(sampler.input), &output = accessor(sampler.output);
            use(sampler.input, 4);
            use(sampler.output, 4);
            require(input.component == 5126 && !input.normalized && input.components == 1);
            require(output.component == 5126 && !output.normalized && output.columns == 1);
            auto declaration = root.at("accessors").at(sampler.input);
            require(declaration.has("min") && declaration.has("max"));
            for (std::size_t t = 0; t < input.count; ++t) {
                work.take();
                require(input.values[t] >= 0 && (t == 0 || input.values[t] > input.values[t - 1]));
            }
            auto interpolation =
                v.has("interpolation") ? v.at("interpolation").string() : std::string_view("LINEAR");
            sampler.interpolation = interpolation == "LINEAR"        ? 0
                                    : interpolation == "STEP"        ? 1
                                    : interpolation == "CUBICSPLINE" ? 2
                                                                     : 3;
            require(sampler.interpolation < 3);
            if (sampler.interpolation == 2)
                require(input.count >= 2);
            animation.samplers.push_back(sampler);
        }
        auto channels = source.at("channels");
        array(channels);
        for (std::size_t ch = 0; ch < channels.size(); ++ch) {
            auto v = channels.at(ch);
            object(v);
            auto target = v.at("target");
            object(target);
            assets::GlbAnimationChannel channel;
            channel.sampler = v.at("sampler").u32();
            require(channel.sampler < animation.samplers.size());
            const auto path = target.at("path").string();
            channel.path = path == "translation" ? 0
                           : path == "rotation"  ? 1
                           : path == "scale"     ? 2
                           : path == "weights"   ? 3
                                                 : 4;
            require(channel.path < 4);
            // Core glTF explicitly ignores channels without a target node.
            // The owned JSON retains their metadata; no active channel is fabricated.
            if (!target.has("node"))
                continue;
            channel.node = target.at("node").u32();
            require(channel.node < data.nodes.size());
            for (const auto &old : animation.channels) {
                work.take();
                require(old.node != channel.node || old.path != channel.path);
            }
            const auto &sampler = animation.samplers[channel.sampler];
            const auto &input = accessor(sampler.input), &output = accessor(sampler.output);
            const auto &node = data.nodes[channel.node];
            const auto multiplier = sampler.interpolation == 2 ? 3u : 1u;
            std::size_t dimensions = 1;
            if (channel.path == 3) {
                require(node.mesh != glb_none);
                dimensions = data.meshes[node.mesh].weights.size();
                require(dimensions > 0 && output.components == 1);
            } else {
                require(!node.has_matrix);
                require(output.components == (channel.path == 1 ? 4u : 3u));
            }
            require(std::uint64_t(input.count) * multiplier * dimensions == output.count);
            if (channel.path == 1)
                for (std::size_t t = 0; t < input.count; ++t) {
                    work.take();
                    const auto key = std::size_t(t) * multiplier + (multiplier == 3 ? 1 : 0);
                    quaternion(output.values.data() + key * 4);
                }
            animation.channels.push_back(channel);
        }
    }
}
// JPEG is a recognized disabled codec. Validate its bounded marker/frame/scan
// envelope only; this is deliberately not a claim of JPEG entropy decoding.
void jpeg_envelope(std::span<const std::uint8_t> bytes, const assets::Limits &limits, assets::Work &work) {
    require(bytes.size() >= 4 && bytes[0] == 0xff && bytes[1] == 0xd8);
    std::size_t at = 2;
    bool frame = false, scan = false, entropy = false;
    while (at < bytes.size()) {
        work.take();
        if (entropy && bytes[at] != 0xff) {
            ++at;
            continue;
        }
        require(bytes[at++] == 0xff);
        while (at < bytes.size() && bytes[at] == 0xff) {
            work.take();
            ++at;
        }
        require(at < bytes.size());
        const auto marker = bytes[at++];
        if (entropy && (marker == 0 || (marker >= 0xd0 && marker <= 0xd7)))
            continue;
        entropy = false;
        if (marker == 0xd9) {
            require(frame && scan && at == bytes.size());
            return;
        }
        require(marker != 0 && marker != 0xd8 && !(marker >= 0xd0 && marker <= 0xd7) &&
                bytes.size() - at >= 2);
        const auto size = std::size_t(bytes[at]) * 256 + bytes[at + 1];
        require(size >= 2 && size <= bytes.size() - at);
        if (marker == 0xc0 || marker == 0xc1 || marker == 0xc2) {
            require(!frame && size >= 8 && bytes[at + 2] == 8);
            const auto height = std::uint32_t(bytes[at + 3]) * 256 + bytes[at + 4],
                       width = std::uint32_t(bytes[at + 5]) * 256 + bytes[at + 6];
            const auto components = bytes[at + 7];
            require(height && width && components >= 1 && components <= 4 &&
                    size == 8 + std::size_t(components) * 3);
            require(width <= limits.edge && height <= limits.edge &&
                        std::uint64_t(width) * height * 4 <= limits.pixels,
                    ARS_E_OUT_OF_BUDGET);
            frame = true;
        }
        if (marker == 0xda) {
            require(frame && size >= 6 && size == 6 + std::size_t(bytes[at + 2]) * 2);
            scan = true;
            entropy = true;
        }
        at += size;
    }
    assets::fail();
}
void Context::images_and_materials() {
    const auto images = count(root, "images");
    data.images.reserve(images);
    std::size_t pixels = 0;
    for (std::size_t i = 0; i < images; ++i) {
        auto image = root.at("images").at(i);
        object(image);
        require(!image.has("uri"));
        const auto vi = image.at("bufferView").u32();
        const auto &v = view(vi);
        require(v.target == 0 && v.stride == 0 && (view_use[vi] == 0 || view_use[vi] == 32));
        view_use[vi] = 32;
        const auto bytes = bin.subspan(v.offset, v.length);
        const auto mime = image.at("mimeType").string();
        auto remaining = limits;
        remaining.pixels -= pixels;
        data.images.emplace_back(&resource);
        if (mime == "image/png") {
            assets::decode_png(bytes, data.images.back(), resource, remaining, work);
            pixels += data.images.back().rgba.size();
        } else if (mime == "image/jpeg") {
            jpeg_envelope(bytes, remaining, work);
            disabled = true;
        } else
            assets::fail();
    }
    const auto samplers = count(root, "samplers");
    for (std::size_t i = 0; i < samplers; ++i) {
        auto v = root.at("samplers").at(i);
        object(v);
        if (v.has("magFilter")) {
            const auto n = v.at("magFilter").u32();
            require(n == 9728 || n == 9729);
        }
        if (v.has("minFilter")) {
            const auto n = v.at("minFilter").u32();
            require(n == 9728 || n == 9729 || (n >= 9984 && n <= 9987));
        }
        for (auto key : {"wrapS", "wrapT"})
            if (v.has(key)) {
                const auto n = v.at(key).u32();
                require(n == 10497 || n == 33071 || n == 33648);
            }
    }
    const auto textures = count(root, "textures");
    for (std::size_t i = 0; i < textures; ++i) {
        auto v = root.at("textures").at(i);
        object(v);
        if (v.has("sampler"))
            require(v.at("sampler").u32() < samplers);
        if (v.has("source"))
            require(v.at("source").u32() < images);
    }
    const auto materials = count(root, "materials");
    for (std::size_t i = 0; i < materials; ++i) {
        auto material = root.at("materials").at(i);
        object(material);
        std::pmr::vector<std::uint32_t> uvsets(&resource);
        auto texture = [&](Value owner, std::string_view key) {
            if (!owner.has(key))
                return;
            auto info = owner.at(key);
            object(info);
            const auto index = info.at("index").u32();
            require(index < textures);
            const auto source = root.at("textures").at(index);
            require(source.has("source") || source.has("extensions"));
            uvsets.push_back(number(info, "texCoord"));
            if (info.has("scale"))
                (void)scalar(info.at("scale"));
            if (info.has("strength")) {
                const auto s = scalar(info.at("strength"));
                require(s >= 0 && s <= 1);
            }
        };
        auto unit_vector = [&]<std::size_t N>(Value owner, std::string_view key,
                                              std::array<double, N> initial) {
            if (owner.has(key)) {
                vector(owner.at(key), initial);
                for (auto n : initial)
                    require(n >= 0 && n <= 1);
            }
        };
        if (material.has("pbrMetallicRoughness")) {
            auto pbr = material.at("pbrMetallicRoughness");
            object(pbr);
            unit_vector(pbr, "baseColorFactor", std::array<double, 4>{1, 1, 1, 1});
            for (auto key : {"metallicFactor", "roughnessFactor"})
                if (pbr.has(key)) {
                    const auto n = scalar(pbr.at(key));
                    require(n >= 0 && n <= 1);
                }
            texture(pbr, "baseColorTexture");
            texture(pbr, "metallicRoughnessTexture");
        }
        unit_vector(material, "emissiveFactor", std::array<double, 3>{});
        texture(material, "normalTexture");
        texture(material, "occlusionTexture");
        texture(material, "emissiveTexture");
        if (material.has("doubleSided"))
            (void)material.at("doubleSided").boolean();
        if (material.has("alphaMode")) {
            auto mode = material.at("alphaMode").string();
            require(mode == "OPAQUE" || mode == "MASK" || mode == "BLEND");
        }
        if (material.has("alphaCutoff"))
            require(scalar(material.at("alphaCutoff")) >= 0);
        for (std::size_t m = 0; m < data.meshes.size(); ++m)
            for (std::size_t p = 0; p < data.meshes[m].primitives.size(); ++p) {
                work.take();
                if (data.meshes[m].primitives[p].material != i)
                    continue;
                const auto attrs = root.at("meshes").at(m).at("primitives").at(p).at("attributes");
                for (auto uv : uvsets) {
                    work.take();
                    char number[11]{};
                    const auto converted = std::to_chars(number, number + sizeof(number), uv);
                    require(converted.ec == std::errc{});
                    std::pmr::string name("TEXCOORD_", &resource);
                    name.append(number, converted.ptr);
                    require(attrs.has(name));
                }
            }
    }
    const auto cameras = count(root, "cameras");
    for (std::size_t i = 0; i < cameras; ++i) {
        auto camera = root.at("cameras").at(i);
        object(camera);
        const auto type = camera.at("type").string();
        require(type == "perspective" || type == "orthographic");
        auto projection = camera.at(type);
        object(projection);
        if (type == "perspective") {
            require(!camera.has("orthographic"));
            const auto near = scalar(projection.at("znear")), fov = scalar(projection.at("yfov"));
            require(near > 0 && fov > 0 && fov < 3.141592653589793);
            if (projection.has("zfar"))
                require(scalar(projection.at("zfar")) > near);
            if (projection.has("aspectRatio"))
                require(scalar(projection.at("aspectRatio")) > 0);
        } else {
            require(!camera.has("perspective"));
            const auto near = scalar(projection.at("znear")), far = scalar(projection.at("zfar"));
            require(near >= 0 && far > near && scalar(projection.at("xmag")) > 0 &&
                    scalar(projection.at("ymag")) > 0);
        }
    }
}
} // namespace
bool validate_glb(const Document &doc, std::span<const std::uint8_t> bin, assets::GlbData &out,
                  assets::Resource &r, const assets::Limits &limits, assets::Work &work) {
    Context c(doc, bin, out, r, limits, work);
    c.extensions();
    c.buffers();
    c.accessors();
    c.meshes();
    c.nodes();
    c.skins();
    c.animation();
    c.images_and_materials();
    return c.disabled;
}
void validate_glb_mappings(Value manifest, const assets::GlbModel &model, assets::Work &work) {
    const auto mappings = manifest.at("representations").at("3d").at("mappings");
    const auto semantics = manifest.at("rig_semantics");
    const auto &data = model.data();
    for (std::size_t i = 0; i < mappings.size(); ++i) {
        work.take();
        const auto mapping = mappings.at(i),
                   semantic = semantics.at(id_index(semantics, mapping.at("semantic").string(), work));
        const auto target = mapping.at("target").string(), type = semantic.at("type").string();
        const auto units = semantic.has("units") ? semantic.at("units").string() : std::string_view{};
        if (target.starts_with("bone:")) {
            const auto dot = target.rfind('.');
            require(dot != target.npos && dot > 5 && dot + 1 < target.size());
            const auto name = target.substr(5, dot - 5), property = target.substr(dot + 1);
            std::size_t matches = 0;
            for (const auto &node : data.nodes) {
                work.take();
                matches += node.name == name ? 1 : 0;
            }
            require(matches == 1);
            if (property == "yaw" || property == "pitch" || property == "roll")
                require(type == "float" && (units.empty() || units == "radians"));
            else if (property == "rotation")
                require(type == "quat" && (units.empty() || units == "unitless"));
            else if (property == "translation")
                require(type == "vec3" && (units.empty() || units == "meters"));
            else if (property == "scale")
                require(type == "vec3" && (units.empty() || units == "unitless"));
            else
                assets::fail();
        } else if (target.starts_with("morph:")) {
            require(target.size() > 6 && type == "float" &&
                    (units.empty() || units == "unitless" || units == "normalized"));
            std::size_t matches = 0;
            for (std::size_t m = 0; m < data.meshes.size(); ++m)
                for (std::size_t t = 0; t < data.meshes[m].target_names.size(); ++t) {
                    work.take();
                    if (data.meshes[m].target_names[t] != target.substr(6))
                        continue;
                    ++matches;
                    const auto initial = static_cast<float>(semantic.at("default").number());
                    require(initial == data.meshes[m].weights[t]);
                    for (const auto &node : data.nodes) {
                        work.take();
                        if (node.mesh == m && !node.weights.empty())
                            require(node.weights[t] == initial);
                    }
                }
            require(matches == 1);
        } else
            assets::fail();
    }
}
} // namespace ars::validation
