#include "scene3d.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ars::rendering {
namespace {
using Mat = std::array<double, 16>;
using Vec = std::array<double, 3>;
constexpr Mat identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
Mat multiply(const Mat &a, const Mat &b) {
    Mat r{};
    for (int c = 0; c < 4; ++c)
        for (int row = 0; row < 4; ++row)
            for (int k = 0; k < 4; ++k)
                r[c * 4 + row] += a[k * 4 + row] * b[c * 4 + k];
    return r;
}
Mat local(const assets::GlbNode &n) {
    if (n.has_matrix) return n.matrix;
    const auto &[x, y, z, w] = n.rotation;
    Mat m{
        (1 - 2 * (y*y + z*z))*n.scale[0], (2 * (x*y + z*w))*n.scale[0], (2 * (x*z - y*w))*n.scale[0], 0,
        (2 * (x*y - z*w))*n.scale[1], (1 - 2 * (x*x + z*z))*n.scale[1], (2 * (y*z + x*w))*n.scale[1], 0,
        (2 * (x*z + y*w))*n.scale[2], (2 * (y*z - x*w))*n.scale[2], (1 - 2 * (x*x + y*y))*n.scale[2], 0,
        n.translation[0], n.translation[1], n.translation[2], 1};
    return m;
}
bool finite(const Mat &m) {
    return std::all_of(m.begin(), m.end(), [](double x) { return std::isfinite(x); });
}
Vec point(const Mat &m, Vec v) {
    return {m[0]*v[0]+m[4]*v[1]+m[8]*v[2]+m[12],
            m[1]*v[0]+m[5]*v[1]+m[9]*v[2]+m[13],
            m[2]*v[0]+m[6]*v[1]+m[10]*v[2]+m[14]};
}
Vec vector(const Mat &m,Vec v) {
    return {m[0]*v[0]+m[4]*v[1]+m[8]*v[2],
            m[1]*v[0]+m[5]*v[1]+m[9]*v[2],
            m[2]*v[0]+m[6]*v[1]+m[10]*v[2]};
}
Vec direction(const Mat &m, Vec v) {
    // Inverse transpose of the upper 3x3. Singular transforms cannot supply
    // an unambiguous normal and are rejected before publishing the snapshot.
    const Vec a{m[0],m[1],m[2]}, b{m[4],m[5],m[6]}, c{m[8],m[9],m[10]};
    const Vec bc{b[1]*c[2]-b[2]*c[1],b[2]*c[0]-b[0]*c[2],b[0]*c[1]-b[1]*c[0]};
    const Vec ca{c[1]*a[2]-c[2]*a[1],c[2]*a[0]-c[0]*a[2],c[0]*a[1]-c[1]*a[0]};
    const Vec ab{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
    const double det=a[0]*bc[0]+a[1]*bc[1]+a[2]*bc[2];
    if (!std::isfinite(det) || std::abs(det)<1e-12) assets::fail(ARS_E_UNSUPPORTED);
    return {(bc[0]*v[0]+ca[0]*v[1]+ab[0]*v[2])/det,
            (bc[1]*v[0]+ca[1]*v[1]+ab[1]*v[2])/det,
            (bc[2]*v[0]+ca[2]*v[1]+ab[2]*v[2])/det};
}
Vec unit(Vec v) {
    const double length=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    if (!std::isfinite(length) || length<1e-12) assets::fail(ARS_E_UNSUPPORTED);
    for (auto &x:v) x/=length;
    return v;
}
float checked(double n) {
    if (!std::isfinite(n) || std::abs(n)>std::numeric_limits<float>::max())
        assets::fail(ARS_E_UNSUPPORTED);
    return static_cast<float>(n);
}
Vec read3(const assets::GlbAccessor &a, std::size_t i) {
    assets::require(a.components>=3 && i<a.count, ARS_E_BAD_ASSET);
    return {a.values[i*a.components],a.values[i*a.components+1],a.values[i*a.components+2]};
}
double normalized_component(const assets::GlbAccessor &a, std::size_t index) {
    const auto value=a.values.at(index);
    if (!a.normalized) return value;
    // Validated WEIGHTS/TEXCOORD packed profiles use only these unsigned types.
    if (a.component==5121) return value/255.;
    if (a.component==5123) return value/65535.;
    assets::fail(ARS_E_UNSUPPORTED);
}
Mat read_matrix(const assets::GlbAccessor &a, std::size_t i) {
    assets::require(a.components==16 && i<a.count, ARS_E_BAD_ASSET);
    Mat m{};
    std::copy_n(a.values.begin()+i*16,16,m.begin());
    return m;
}
void add_budget(std::size_t &current, std::size_t count, std::size_t unit, std::size_t limit) {
    if (count>(limit-current)/unit) assets::fail(ARS_E_OUT_OF_BUDGET);
    current+=count*unit;
}
using validation::Value;
Texture3D texture(const assets::GlbModel &source,Value info) {
    const auto root=source.metadata();
    if (info.has("extensions") || (info.has("texCoord") && info.at("texCoord").u32()!=0))
        assets::fail(ARS_E_UNSUPPORTED);
    const auto declaration=root.at("textures").at(info.at("index").u32());
    if (!declaration.has("source") || declaration.has("extensions")) assets::fail(ARS_E_UNSUPPORTED);
    Texture3D result;
    result.image=declaration.at("source").u32();
    assets::require(result.image<source.data().images.size(),ARS_E_BAD_ASSET);
    if (declaration.has("sampler")) {
        const auto sampler=root.at("samplers").at(declaration.at("sampler").u32());
        if (sampler.has("magFilter")) result.mag_filter=sampler.at("magFilter").u32();
        if (sampler.has("minFilter")) result.min_filter=sampler.at("minFilter").u32();
        if (sampler.has("wrapS")) result.wrap_s=sampler.at("wrapS").u32();
        if (sampler.has("wrapT")) result.wrap_t=sampler.at("wrapT").u32();
    }
    return result;
}
Material3D material(const assets::GlbModel &source, std::uint32_t index) {
    Material3D result;
    if (index==assets::glb_none) return result;
    const auto root=source.metadata();
    const auto m=root.at("materials").at(index);
    if (m.has("extensions") || m.has("normalTexture") || m.has("occlusionTexture") ||
        m.has("emissiveTexture") || m.has("emissiveFactor")) assets::fail(ARS_E_UNSUPPORTED);
    if (m.has("doubleSided")) result.double_sided=m.at("doubleSided").boolean();
    if (m.has("alphaMode")) {
        const auto mode=m.at("alphaMode").string();
        if (mode=="MASK") result.alpha=Material3D::Alpha::mask;
        else if (mode=="BLEND") result.alpha=Material3D::Alpha::blend;
    }
    if (m.has("alphaCutoff")) result.alpha_cutoff=checked(m.at("alphaCutoff").number());
    if (!m.has("pbrMetallicRoughness")) return result;
    const auto p=m.at("pbrMetallicRoughness");
    if (p.has("baseColorFactor"))
        for (std::size_t i=0;i<4;++i) result.base_color[i]=checked(p.at("baseColorFactor").at(i).number());
    if (p.has("metallicFactor")) result.metallic=checked(p.at("metallicFactor").number());
    if (p.has("roughnessFactor")) result.roughness=checked(p.at("roughnessFactor").number());
    if (p.has("baseColorTexture")) result.base_texture=texture(source,p.at("baseColorTexture"));
    if (p.has("metallicRoughnessTexture"))
        result.metallic_roughness_texture=texture(source,p.at("metallicRoughnessTexture"));
    return result;
}
struct Instance { std::uint32_t node; Mat world; };
} // namespace

ars_status prepare_static_3d(std::shared_ptr<const assets::GlbModel> source,
    PreparedScene3D &out, std::uint32_t scene, Scene3DLimits limits) noexcept {
    if (!source || source->requires_disabled()) return ARS_E_UNSUPPORTED;
    if (!limits.gpu_bytes || !limits.pose_bytes || limits.gpu_bytes>256*assets::MiB ||
        limits.pose_bytes>64*assets::MiB) return ARS_E_INVALID_ARGUMENT;
    try {
        const auto &data=source->data();
        if (scene==assets::glb_none) scene=data.scene;
        if (scene==assets::glb_none || scene>=data.scenes.size()) return ARS_E_UNSUPPORTED;
        auto *memory=out.draws.get_allocator().resource();
        std::pmr::vector<Instance> reachable(memory);
        std::pmr::vector<Mat> worlds(data.nodes.size(),identity,memory);
        std::pmr::vector<bool> visited(data.nodes.size(),false,memory);
        // Explicit stack in reverse declaration order; C2 already rules out cycles.
        std::pmr::vector<Instance> pending(memory);
        for (auto i=data.scenes[scene].roots.rbegin();i!=data.scenes[scene].roots.rend();++i)
            pending.push_back({*i,identity});
        while (!pending.empty()) {
            auto parent=pending.back(); pending.pop_back();
            if (parent.node>=data.nodes.size() || visited[parent.node]) assets::fail(ARS_E_BAD_ASSET);
            visited[parent.node]=true;
            const auto &node=data.nodes[parent.node];
            auto world=multiply(parent.world,local(node));
            if (!finite(world)) assets::fail(ARS_E_UNSUPPORTED);
            worlds[parent.node]=world;
            reachable.push_back({parent.node,world});
            for (auto i=node.children.rbegin();i!=node.children.rend();++i)
                pending.push_back({*i,world});
        }
        PreparedScene3D next(memory);
        next.source=std::move(source);
        next.gpu_budget=limits.gpu_bytes;
        std::size_t scratch=0;
        for (const auto &instance:reachable) {
            const auto &node=data.nodes[instance.node];
            const auto declaration=next.source->metadata().at("nodes").at(instance.node);
            if (declaration.has("camera")) {
                CameraNode3D camera;
                camera.node=instance.node;camera.world=instance.world;
                const auto source_camera=next.source->metadata().at("cameras").at(declaration.at("camera").u32());
                camera.perspective=source_camera.at("type").string()=="perspective";
                const auto projection=source_camera.at(camera.perspective?"perspective":"orthographic");
                camera.znear=projection.at("znear").number();
                if (camera.perspective) {
                    camera.yfov=projection.at("yfov").number();
                    camera.infinite_far=!projection.has("zfar");
                    if (!camera.infinite_far) camera.zfar=projection.at("zfar").number();
                    if (projection.has("aspectRatio")) camera.aspect=projection.at("aspectRatio").number();
                } else {
                    camera.zfar=projection.at("zfar").number();
                    camera.xmag=projection.at("xmag").number();
                    camera.ymag=projection.at("ymag").number();
                }
                next.cameras.push_back(camera);
            }
            if (node.mesh==assets::glb_none) continue;
            assets::require(node.mesh<data.meshes.size(), ARS_E_BAD_ASSET);
            const auto &mesh=data.meshes[node.mesh];
            std::pmr::vector<Mat> palette(memory);
            if (node.skin!=assets::glb_none) {
                assets::require(node.skin<data.skins.size(), ARS_E_BAD_ASSET);
                const auto &skin=data.skins[node.skin];
                const auto *bind=skin.inverse_bind==assets::glb_none?nullptr:&data.accessors.at(skin.inverse_bind);
                for (std::size_t i=0;i<skin.joints.size();++i) {
                    const auto joint=skin.joints[i];
                    // A skin joint outside the selected scene cannot have a
                    // committed world pose in this private default-pose pass.
                    if (joint>=visited.size() || !visited[joint]) assets::fail(ARS_E_UNSUPPORTED);
                    auto p=multiply(worlds[joint],bind?read_matrix(*bind,i):identity);
                    if (!finite(p)) assets::fail(ARS_E_UNSUPPORTED);
                    palette.push_back(p);
                }
            }
            for (std::size_t pi=0;pi<mesh.primitives.size();++pi) {
                const auto &primitive=mesh.primitives[pi];
                if (primitive.mode!=4 || primitive.position>=data.accessors.size()) assets::fail(ARS_E_UNSUPPORTED);
                const auto &positions=data.accessors[primitive.position];
                if (positions.count>limits.pose_bytes/sizeof(Vertex3D)) assets::fail(ARS_E_OUT_OF_BUDGET);
                add_budget(scratch,positions.count,sizeof(Vertex3D),limits.pose_bytes);
                add_budget(next.estimated_gpu_bytes,positions.count,80,limits.gpu_bytes);
                add_budget(next.estimated_gpu_bytes,primitive.indices.size(),sizeof(std::uint32_t),limits.gpu_bytes);
                auto &draw=next.draws.emplace_back(memory);
                draw.node=instance.node; draw.primitive=static_cast<std::uint32_t>(pi);
                draw.material=material(*next.source,primitive.material);
                for (const auto &texture:{draw.material.base_texture,draw.material.metallic_roughness_texture})
                    if (texture.image!=assets::glb_none) {
                        const auto &image=data.images[texture.image];
                        // Mip chain is below 4/3 of base; reserve a conservative
                        // six bytes per pixel before any GPU object is created.
                        add_budget(next.estimated_gpu_bytes,
                                   std::size_t(image.width)*image.height,6,limits.gpu_bytes);
                    }
                draw.indices.assign(primitive.indices.begin(),primitive.indices.end());
                draw.vertices.reserve(positions.count);
                const auto &weights=node.weights.empty()?mesh.weights:node.weights;
                for (std::size_t vi=0;vi<positions.count;++vi) {
                    Vec p=read3(positions,vi);
                    Vec n=primitive.normal==assets::glb_none?Vec{0,0,1}:read3(data.accessors.at(primitive.normal),vi);
                    const bool has_tangent=primitive.tangent!=assets::glb_none;
                    Vec tangent=has_tangent?read3(data.accessors.at(primitive.tangent),vi):Vec{1,0,0};
                    const double handedness=has_tangent?
                        data.accessors.at(primitive.tangent).values[vi*4+3]:1;
                    for (std::size_t mi=0;mi<primitive.morphs.size();++mi) {
                        const double w=mi<weights.size()?weights[mi]:0;
                        const auto &m=primitive.morphs[mi];
                        if (m.position!=assets::glb_none) {
                            const auto d=read3(data.accessors.at(m.position),vi);
                            for (int k=0;k<3;++k) p[k]+=w*d[k];
                        }
                        if (m.normal!=assets::glb_none) {
                            const auto d=read3(data.accessors.at(m.normal),vi);
                            for (int k=0;k<3;++k) n[k]+=w*d[k];
                        }
                        if (m.tangent!=assets::glb_none) {
                            const auto d=read3(data.accessors.at(m.tangent),vi);
                            for (int k=0;k<3;++k) tangent[k]+=w*d[k];
                        }
                    }
                    n=unit(n);
                    if (!palette.empty()) {
                        if (primitive.joints==assets::glb_none || primitive.weights==assets::glb_none)
                            assets::fail(ARS_E_UNSUPPORTED);
                        const auto &j=data.accessors.at(primitive.joints), &w=data.accessors.at(primitive.weights);
                        Vec sp{},sn{},st{};
                        for (std::size_t k=0;k<4;++k) {
                            const auto weight=normalized_component(w,vi*4+k);
                            if (weight==0) continue;
                            const auto joint=static_cast<std::size_t>(j.values[vi*4+k]);
                            if (joint>=palette.size()) assets::fail(ARS_E_BAD_ASSET);
                            auto tp=point(palette[joint],p),tn=direction(palette[joint],n);
                            auto tt=has_tangent?vector(palette[joint],tangent):Vec{};
                            for (int c=0;c<3;++c) {
                                sp[c]+=weight*tp[c];sn[c]+=weight*tn[c];st[c]+=weight*tt[c];
                            }
                        }
                        p=sp;n=unit(sn);if (has_tangent) tangent=st;
                    } else {
                        p=point(instance.world,p);n=unit(direction(instance.world,n));
                        if (has_tangent) tangent=vector(instance.world,tangent);
                    }
                    if (has_tangent) {
                        const double parallel=tangent[0]*n[0]+tangent[1]*n[1]+tangent[2]*n[2];
                        for (int c=0;c<3;++c) tangent[c]-=parallel*n[c];
                        tangent=unit(tangent);
                    }
                    Vertex3D v;
                    for (int c=0;c<3;++c) { v.position[c]=checked(p[c]); v.normal[c]=checked(n[c]); }
                    for (int c=0;c<3;++c) v.tangent[c]=checked(tangent[c]);
                    v.tangent[3]=checked(handedness);
                    if (primitive.uv!=assets::glb_none) {
                        const auto &uv=data.accessors.at(primitive.uv);
                        v.uv={checked(normalized_component(uv,vi*2)),checked(normalized_component(uv,vi*2+1))};
                    }
                    draw.vertices.push_back(v);
                }
            }
        }
        if (next.draws.empty()) return ARS_E_UNSUPPORTED;
        out=std::move(next);
        return ARS_OK;
    } catch (const assets::Failure &f) { return f.status;
    } catch (const std::bad_alloc &) { return ARS_E_OUT_OF_BUDGET;
    } catch (...) { return ARS_E_INTERNAL; }
}

ars_status select_gltf_camera(const PreparedScene3D &scene,std::uint32_t node,
    std::uint32_t width,std::uint32_t height,Camera3D &out) noexcept {
    if (!width || !height) return ARS_E_INVALID_ARGUMENT;
    try {
        const auto found=std::find_if(scene.cameras.begin(),scene.cameras.end(),
                                      [node](const CameraNode3D &c){return c.node==node;});
        if (found==scene.cameras.end()) return ARS_E_UNSUPPORTED;
        const auto &c=*found;
        Mat view{};
        const auto &m=c.world;
        const Vec a{m[0],m[1],m[2]},b{m[4],m[5],m[6]},z{m[8],m[9],m[10]};
        const Vec bz{b[1]*z[2]-b[2]*z[1],b[2]*z[0]-b[0]*z[2],b[0]*z[1]-b[1]*z[0]};
        const Vec za{z[1]*a[2]-z[2]*a[1],z[2]*a[0]-z[0]*a[2],z[0]*a[1]-z[1]*a[0]};
        const Vec ab{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
        const double det=a[0]*bz[0]+a[1]*bz[1]+a[2]*bz[2];
        if (!std::isfinite(det) || std::abs(det)<1e-12) return ARS_E_UNSUPPORTED;
        for (int column=0;column<3;++column) {
            view[column*4]=bz[column]/det;
            view[column*4+1]=za[column]/det;
            view[column*4+2]=ab[column]/det;
        }
        for (int row=0;row<3;++row)
            view[12+row]=-(view[row]*m[12]+view[4+row]*m[13]+view[8+row]*m[14]);
        view[15]=1;
        Mat projection{};
        if (c.perspective) {
            const double aspect=c.aspect>0?c.aspect:double(width)/height;
            const double f=1/std::tan(c.yfov/2);
            projection[0]=f/aspect;projection[5]=f;projection[11]=-1;
            projection[10]=c.infinite_far?-1:c.zfar/(c.znear-c.zfar);
            projection[14]=c.infinite_far?-c.znear:c.znear*c.zfar/(c.znear-c.zfar);
        } else {
            projection[0]=1/c.xmag;projection[5]=1/c.ymag;
            projection[10]=1/(c.znear-c.zfar);
            projection[14]=c.znear/(c.znear-c.zfar);
            projection[15]=1;
        }
        auto result=multiply(projection,view);
        if (!finite(result)) return ARS_E_UNSUPPORTED;
        out.clip_from_world=result;
        out.front_counter_clockwise=true;
        return ARS_OK;
    } catch (...) { return ARS_E_INTERNAL; }
}
} // namespace ars::rendering
