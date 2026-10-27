// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Version 0.0.7 BATARANG STYLE: the strict glTF 2.0 reader (mesh_model.hpp).
#include "spidy/mesh_model.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <unordered_map>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace spidy::mesh_model {
namespace {
// Sets `error` and returns false (one statement for every refusal).
bool refuse(std::string& error, std::string why) {
    error = std::move(why);
    return false;
}

// ---- a small JSON reader (objects, arrays, strings, numbers, true / false / null) -------------
struct Json {
    enum Kind { null, boolean, number, string, array, object } kind = null;
    double num = 0;
    bool yes = false;
    std::string text;
    std::vector<Json> items;
    std::vector<std::pair<std::string, Json>> fields;
    const Json* get(const char* key) const {
        if (kind != object)
            return nullptr;
        for (const auto& [k, v] : fields)
            if (k == key)
                return &v;
        return nullptr;
    }
    const Json* at(size_t i) const { return kind == array && i < items.size() ? &items[i] : nullptr; }
    bool isInt() const { return kind == number && num >= 0 && num <= 4294967295.0 && std::floor(num) == num; }
};

struct Parser {
    const char* p;
    const char* end;
    int depth = 0;
    bool fail = false;
    void space() {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
            ++p;
    }
    bool literal(const char* word) {
        const size_t n = std::strlen(word);
        if (static_cast<size_t>(end - p) < n || std::memcmp(p, word, n) != 0)
            return false;
        p += n;
        return true;
    }
    bool str(std::string& out) {
        if (p >= end || *p != '"')
            return false;
        ++p;
        while (p < end && *p != '"') {
            if (*p == '\\') {
                if (++p >= end)
                    return false;
                const char c = *p++;
                switch (c) {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'u': // keep ASCII, others as '?': names only, never used for paths here
                    if (end - p < 4)
                        return false;
                    {
                        unsigned v = 0;
                        for (int i = 0; i < 4; ++i) {
                            const char h = *p++;
                            const int d = h >= '0' && h <= '9' ? h - '0' : h >= 'a' && h <= 'f' ? h - 'a' + 10 : h >= 'A' && h <= 'F' ? h - 'A' + 10 : 99;
                            v = v * 16 + static_cast<unsigned>(d);
                            if (v > 0xFFFF)
                                return false;
                        }
                        out += v < 128 ? static_cast<char>(v) : '?';
                    }
                    break;
                default: out += c;
                }
            } else
                out += *p++;
        }
        if (p >= end)
            return false;
        ++p;
        return true;
    }
    bool value(Json& v) {
        if (++depth > 64)
            return false;
        space();
        if (p >= end)
            return false;
        bool good = true;
        if (*p == '{') {
            v.kind = Json::object;
            ++p;
            space();
            if (p < end && *p == '}')
                ++p;
            else
                for (;;) {
                    space();
                    std::string key;
                    if (!str(key)) { good = false; break; }
                    space();
                    if (p >= end || *p != ':') { good = false; break; }
                    ++p;
                    v.fields.emplace_back(std::move(key), Json{});
                    if (!value(v.fields.back().second)) { good = false; break; }
                    space();
                    if (p < end && *p == ',') { ++p; continue; }
                    if (p < end && *p == '}') { ++p; break; }
                    good = false;
                    break;
                }
        } else if (*p == '[') {
            v.kind = Json::array;
            ++p;
            space();
            if (p < end && *p == ']')
                ++p;
            else
                for (;;) {
                    v.items.emplace_back();
                    if (!value(v.items.back())) { good = false; break; }
                    space();
                    if (p < end && *p == ',') { ++p; continue; }
                    if (p < end && *p == ']') { ++p; break; }
                    good = false;
                    break;
                }
        } else if (*p == '"') {
            v.kind = Json::string;
            good = str(v.text);
        } else if (literal("true")) {
            v.kind = Json::boolean;
            v.yes = true;
        } else if (literal("false")) {
            v.kind = Json::boolean;
        } else if (literal("null")) {
            v.kind = Json::null;
        } else {
            const char* start = p;
            while (p < end && (std::strchr("+-.eE", *p) || (*p >= '0' && *p <= '9')))
                ++p;
            if (p == start || p - start > 40)
                return false;
            const std::string digits(start, p);
            char* stop = nullptr;
            v.kind = Json::number;
            v.num = std::strtod(digits.c_str(), &stop);
            good = stop && *stop == 0 && std::isfinite(v.num);
        }
        --depth;
        return good;
    }
};

bool parseJson(const std::string& text, Json& out) {
    Parser parser{text.data(), text.data() + text.size()};
    if (!parser.value(out))
        return false;
    parser.space();
    return parser.p == parser.end && out.kind == Json::object;
}

// ---- matrices (column-major, as glTF) -----------------------------------------------------------
using Mat = std::array<float, 16>;
Mat identity() { return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; }
Mat mul(const Mat& a, const Mat& b) {
    Mat r{};
    for (size_t c = 0; c < 4; ++c)
        for (size_t row = 0; row < 4; ++row) {
            float s = 0;
            for (size_t k = 0; k < 4; ++k)
                s += a[k * 4 + row] * b[c * 4 + k];
            r[c * 4 + row] = s;
        }
    return r;
}
Vec3 apply(const Mat& m, Vec3 v) {
    return {m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12], m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13],
            m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]};
}
bool numbers(const Json* j, float* out, size_t n) {
    if (!j || j->kind != Json::array || j->items.size() != n)
        return false;
    for (size_t i = 0; i < n; ++i) {
        if (j->items[i].kind != Json::number || !std::isfinite(j->items[i].num) || std::abs(j->items[i].num) > 1e6)
            return false;
        out[i] = static_cast<float>(j->items[i].num);
    }
    return true;
}
bool nodeMatrix(const Json& node, Mat& out, std::string& error) {
    out = identity();
    if (const Json* m = node.get("matrix")) {
        if (!numbers(m, out.data(), 16))
            return refuse(error, "bad node matrix");
        return true;
    }
    float t[3]{0, 0, 0}, r[4]{0, 0, 0, 1}, s[3]{1, 1, 1};
    if ((node.get("translation") && !numbers(node.get("translation"), t, 3)) ||
        (node.get("rotation") && !numbers(node.get("rotation"), r, 4)) || (node.get("scale") && !numbers(node.get("scale"), s, 3)))
        return refuse(error, "bad node translation / rotation / scale");
    const float n = std::sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2] + r[3] * r[3]);
    if (n < 1e-6f)
        return refuse(error, "bad node rotation");
    const float x = r[0] / n, y = r[1] / n, z = r[2] / n, w = r[3] / n;
    const Mat rot{1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w), 0,
                  2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w), 0,
                  2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y), 0,
                  0, 0, 0, 1};
    Mat scale = identity();
    scale[0] = s[0];
    scale[5] = s[1];
    scale[10] = s[2];
    Mat move = identity();
    move[12] = t[0];
    move[13] = t[1];
    move[14] = t[2];
    out = mul(move, mul(rot, scale));
    return true;
}

// ---- the glTF document ---------------------------------------------------------------------------
struct Doc {
    const Json& root;
    const std::vector<uint8_t>& bin;
    Mesh& out;
    std::string& error;
    size_t reads = 0;

    bool view(uint32_t index, size_t& offset, size_t& length, size_t& stride) {
        const Json* views = root.get("bufferViews");
        const Json* v = views ? views->at(index) : nullptr;
        if (!v || v->kind != Json::object)
            return refuse(error, "bufferView missing");
        const Json* buffer = v->get("buffer");
        if (!buffer || !buffer->isInt() || buffer->num != 0)
            return refuse(error, "only buffer 0 (the .bin / GLB chunk) is read");
        const Json* len = v->get("byteLength");
        const Json* off = v->get("byteOffset");
        const Json* str = v->get("byteStride");
        if (!len || !len->isInt() || (off && !off->isInt()) || (str && (!str->isInt() || str->num < 4 || str->num > 252)))
            return refuse(error, "bad bufferView");
        offset = off ? static_cast<size_t>(off->num) : 0;
        length = static_cast<size_t>(len->num);
        stride = str ? static_cast<size_t>(str->num) : 0;
        if (offset > bin.size() || length > bin.size() - offset)
            return refuse(error, "bufferView outside the buffer");
        return true;
    }
    // An accessor's elements, read as floats (component counts 1 or 3).
    bool accessor(uint32_t index, bool positions, std::vector<Vec3>& vecs, std::vector<uint32_t>& ints) {
        const Json* list = root.get("accessors");
        const Json* a = list ? list->at(index) : nullptr;
        if (!a || a->kind != Json::object)
            return refuse(error, "accessor missing");
        if (a->get("sparse"))
            return refuse(error, "sparse accessors are not read");
        const Json* bv = a->get("bufferView");
        const Json* type = a->get("componentType");
        const Json* count = a->get("count");
        const Json* kind = a->get("type");
        const Json* off = a->get("byteOffset");
        if (!bv || !bv->isInt() || !type || !type->isInt() || !count || !count->isInt() || !kind || kind->kind != Json::string ||
            (off && !off->isInt()))
            return refuse(error, "bad accessor");
        const auto n = static_cast<size_t>(count->num);
        if (n == 0 || n > size_t{mostSourceTriangles} * 3)
            return refuse(error, "accessor count out of range");
        size_t offset{}, length{}, stride{};
        if (!view(static_cast<uint32_t>(bv->num), offset, length, stride))
            return false;
        const size_t extra = off ? static_cast<size_t>(off->num) : 0;
        const auto component = static_cast<uint32_t>(type->num);
        if (positions) {
            if (kind->text != "VEC3" || component != 5126)
                return refuse(error, "POSITION must be FLOAT VEC3");
            const size_t size = 12, step = stride ? stride : size;
            if (step < size || extra > length || (n - 1) * step + size > length - extra)
                return refuse(error, "POSITION outside its bufferView");
            vecs.resize(n);
            for (size_t i = 0; i < n; ++i) {
                float f[3];
                std::memcpy(f, bin.data() + offset + extra + i * step, 12);
                for (float c : f)
                    if (!std::isfinite(c) || std::abs(c) > 1e5f)
                        return refuse(error, "a NaN or huge coordinate");
                vecs[i] = {f[0], f[1], f[2]};
            }
        } else {
            if (kind->text != "SCALAR")
                return refuse(error, "indices must be SCALAR");
            const size_t size = component == 5121 ? 1 : component == 5123 ? 2 : component == 5125 ? 4 : 0;
            if (!size)
                return refuse(error, "indices must be UNSIGNED_BYTE / SHORT / INT");
            const size_t step = stride ? stride : size;
            if (step < size || extra > length || (n - 1) * step + size > length - extra)
                return refuse(error, "indices outside their bufferView");
            ints.resize(n);
            for (size_t i = 0; i < n; ++i) {
                const uint8_t* at = bin.data() + offset + extra + i * step;
                uint32_t v = 0;
                if (size == 1)
                    v = at[0];
                else if (size == 2)
                    v = static_cast<uint32_t>(at[0] | (at[1] << 8));
                else
                    v = static_cast<uint32_t>(at[0] | (at[1] << 8) | (at[2] << 16)) | (static_cast<uint32_t>(at[3]) << 24);
                ints[i] = v;
            }
        }
        return true;
    }
    // COLOR_0 (the converter's baked colours): FLOAT, or normalized UNSIGNED_BYTE / SHORT; VEC3 or VEC4.
    bool colorAccessor(uint32_t index, size_t want, std::vector<Vec3>& vecs) {
        const Json* list = root.get("accessors");
        const Json* a = list ? list->at(index) : nullptr;
        if (!a || a->kind != Json::object || a->get("sparse"))
            return refuse(error, "bad COLOR_0 accessor");
        const Json* bv = a->get("bufferView");
        const Json* type = a->get("componentType");
        const Json* count = a->get("count");
        const Json* kind = a->get("type");
        const Json* off = a->get("byteOffset");
        if (!bv || !bv->isInt() || !type || !type->isInt() || !count || !count->isInt() || !kind || kind->kind != Json::string ||
            (off && !off->isInt()) || static_cast<size_t>(count->num) != want)
            return refuse(error, "bad COLOR_0 accessor");
        const size_t comps = kind->text == "VEC3" ? 3 : kind->text == "VEC4" ? 4 : 0;
        const auto component = static_cast<uint32_t>(type->num);
        const size_t one = component == 5126 ? 4 : component == 5123 ? 2 : component == 5121 ? 1 : 0;
        if (!comps || !one)
            return refuse(error, "COLOR_0 must be VEC3 / VEC4 of FLOAT, UNSIGNED_BYTE or UNSIGNED_SHORT");
        size_t offset{}, length{}, stride{};
        if (!view(static_cast<uint32_t>(bv->num), offset, length, stride))
            return false;
        const size_t extra = off ? static_cast<size_t>(off->num) : 0, size = comps * one, step = stride ? stride : size;
        if (want == 0 || step < size || extra > length || (want - 1) * step + size > length - extra)
            return refuse(error, "COLOR_0 outside its bufferView");
        vecs.resize(want);
        for (size_t i = 0; i < want; ++i) {
            const uint8_t* at = bin.data() + offset + extra + i * step;
            float f[3];
            for (size_t k = 0; k < 3; ++k) {
                if (one == 4)
                    std::memcpy(&f[k], at + k * 4, 4);
                else if (one == 2)
                    f[k] = static_cast<float>(at[k * 2] | (at[k * 2 + 1] << 8)) / 65535.f;
                else
                    f[k] = static_cast<float>(at[k]) / 255.f;
                if (!std::isfinite(f[k]))
                    return refuse(error, "a NaN colour");
                f[k] = std::clamp(f[k], 0.f, 1.f);
            }
            vecs[i] = {f[0], f[1], f[2]};
        }
        return true;
    }
    bool hasMaterial(const Json* index) {
        const Json* list = root.get("materials");
        return index && index->isInt() && list && list->at(static_cast<size_t>(index->num));
    }
    Vec3 materialColor(const Json* index) {
        Vec3 c{.55f, .55f, .58f};
        const Json* list = root.get("materials");
        const Json* m = index && index->isInt() && list ? list->at(static_cast<size_t>(index->num)) : nullptr;
        const Json* pbr = m ? m->get("pbrMetallicRoughness") : nullptr;
        float f[4];
        if (pbr && numbers(pbr->get("baseColorFactor"), f, 4))
            c = {std::clamp(f[0], 0.f, 1.f), std::clamp(f[1], 0.f, 1.f), std::clamp(f[2], 0.f, 1.f)};
        return c;
    }
    bool mesh(uint32_t index, const Mat& world) {
        const Json* list = root.get("meshes");
        const Json* m = list ? list->at(index) : nullptr;
        const Json* prims = m ? m->get("primitives") : nullptr;
        if (!prims || prims->kind != Json::array)
            return refuse(error, "mesh without primitives");
        for (const Json& prim : prims->items) {
            const Json* mode = prim.get("mode");
            if (mode && (!mode->isInt() || mode->num != 4))
                continue; // points / lines / strips: not drawn
            const Json* attrs = prim.get("attributes");
            const Json* pos = attrs ? attrs->get("POSITION") : nullptr;
            if (!pos || !pos->isInt())
                return refuse(error, "primitive without POSITION");
            std::vector<Vec3> points;
            std::vector<uint32_t> idx, none;
            if (!accessor(static_cast<uint32_t>(pos->num), true, points, none))
                return false;
            if (const Json* ind = prim.get("indices")) {
                if (!ind->isInt() || !accessor(static_cast<uint32_t>(ind->num), false, points, idx))
                    return error.empty() ? refuse(error, "bad indices") : false;
            } else {
                idx.resize(points.size());
                for (uint32_t i = 0; i < idx.size(); ++i)
                    idx[i] = i;
            }
            if (idx.size() % 3)
                return refuse(error, "a triangle list not a multiple of 3");
            const Vec3 color = materialColor(prim.get("material"));
            std::vector<Vec3> painted; // COLOR_0: each triangle the mean of its corners (x the material's factor)
            if (const Json* col = attrs->get("COLOR_0")) {
                if (!col->isInt() || !colorAccessor(static_cast<uint32_t>(col->num), points.size(), painted))
                    return error.empty() ? refuse(error, "bad COLOR_0") : false;
            }
            const Vec3 tint = hasMaterial(prim.get("material")) ? color : Vec3{1, 1, 1};
            for (size_t i = 0; i < idx.size(); i += 3) {
                for (size_t k = 0; k < 3; ++k)
                    if (idx[i + k] >= points.size())
                        return refuse(error, "an index out of range");
                if (out.colors.size() >= mostSourceTriangles)
                    return refuse(error, "too many triangles");
                for (size_t k = 0; k < 3; ++k) {
                    const Vec3 p = apply(world, points[idx[i + k]]);
                    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) || std::abs(p.x) > 1e6f ||
                        std::abs(p.y) > 1e6f || std::abs(p.z) > 1e6f)
                        return refuse(error, "a NaN or huge coordinate");
                    out.corners.push_back(p);
                }
                if (painted.empty()) {
                    out.colors.push_back(color);
                } else {
                    const Vec3 mean = (painted[idx[i]] + painted[idx[i + 1]] + painted[idx[i + 2]]) * (1.f / 3);
                    out.colors.push_back({mean.x * tint.x, mean.y * tint.y, mean.z * tint.z});
                }
            }
        }
        return true;
    }
    bool node(uint32_t index, const Mat& parent, int depth) {
        if (depth > 32 || ++reads > 10000)
            return refuse(error, "node tree too deep (or a loop)");
        const Json* list = root.get("nodes");
        const Json* n = list ? list->at(index) : nullptr;
        if (!n || n->kind != Json::object)
            return refuse(error, "node missing");
        Mat local;
        if (!nodeMatrix(*n, local, error))
            return false;
        const Mat world = mul(parent, local);
        if (const Json* m = n->get("mesh")) {
            if (!m->isInt() || !mesh(static_cast<uint32_t>(m->num), world))
                return error.empty() ? refuse(error, "bad node mesh") : false;
        }
        if (const Json* kids = n->get("children")) {
            if (kids->kind != Json::array)
                return refuse(error, "bad node children");
            for (const Json& k : kids->items)
                if (!k.isInt() || !node(static_cast<uint32_t>(k.num), world, depth + 1))
                    return error.empty() ? refuse(error, "bad child") : false;
        }
        return true;
    }
    bool run() {
        const Json* asset = root.get("asset");
        const Json* version = asset ? asset->get("version") : nullptr;
        if (!version || version->kind != Json::string || version->text.rfind("2.", 0) != 0)
            return refuse(error, "not glTF 2.x");
        float pv[3]; // the converter's pivot (a sawed-off's hinge) in asset.extras.pivot, model space
        if (const Json* extras = asset->get("extras"); extras && numbers(extras->get("pivot"), pv, 3) &&
                                                       std::abs(pv[0]) < 1e3f && std::abs(pv[1]) < 1e3f && std::abs(pv[2]) < 1e3f) {
            out.pivot = {pv[0], pv[1], pv[2]};
            out.hasPivot = true;
        }
        float ly[7]; // Version 0.0.7: the shield's layer (mesh_model.hpp); checked against the triangles after the read
        if (const Json* extras = asset->get("extras"); extras && numbers(extras->get("layer"), ly, 7) && ly[0] >= 1 &&
                                                       ly[0] < 1e6f) {
            bool sane = true;
            for (int k = 1; k < 7; ++k)
                sane = sane && std::abs(ly[k]) < 1e3f;
            const Vec3 nrm{ly[4], ly[5], ly[6]};
            if (sane && length(nrm) > .5f) {
                out.layerSplit = static_cast<uint32_t>(ly[0]);
                out.layerCentre = {ly[1], ly[2], ly[3]};
                out.layerNormal = normalized(nrm);
                out.hasLayer = true;
            }
        }
        if (const Json* req = root.get("extensionsRequired"); req && req->kind == Json::array && !req->items.empty())
            return refuse(error, "needs an extension (" + (req->items[0].kind == Json::string ? req->items[0].text : std::string("?")) +
                                     "): re-download the uncompressed glTF");
        const Json* scenes = root.get("scenes");
        const Json* pick = root.get("scene");
        const Json* scene = scenes ? scenes->at(pick && pick->isInt() ? static_cast<size_t>(pick->num) : 0) : nullptr;
        const Json* roots = scene ? scene->get("nodes") : nullptr;
        if (roots && roots->kind == Json::array) {
            for (const Json& r : roots->items)
                if (!r.isInt() || !node(static_cast<uint32_t>(r.num), identity(), 0))
                    return error.empty() ? refuse(error, "bad scene node") : false;
        } else if (const Json* meshes = root.get("meshes"); meshes && meshes->kind == Json::array) {
            for (uint32_t i = 0; i < meshes->items.size(); ++i)
                if (!mesh(i, identity()))
                    return false;
        }
        if (out.colors.empty())
            return refuse(error, "no triangles");
        return true;
    }
};

void bounds(Mesh& m) {
    m.low = m.high = m.corners.empty() ? Vec3{} : m.corners[0];
    for (const Vec3& p : m.corners) {
        m.low = {std::min(m.low.x, p.x), std::min(m.low.y, p.y), std::min(m.low.z, p.z)};
        m.high = {std::max(m.high.x, p.x), std::max(m.high.y, p.y), std::max(m.high.z, p.z)};
    }
}

bool finish(Mesh& out, std::string& error) {
    bounds(out);
    const Vec3 size = out.high - out.low;
    if (std::max({size.x, size.y, size.z}) < 1e-6f) {
        out = {};
        return refuse(error, "the model has no size");
    }
    out.sourceTriangles = out.triangles();
    if (out.hasLayer && !(out.layerSplit < out.triangles())) // a layer that does not fit the triangles: none
        out.hasLayer = false;
    return true;
}
} // namespace

bool parseGltf(const std::string& json, const std::vector<uint8_t>& bin, Mesh& out, std::string& error) {
    out = {};
    error.clear();
    Json root;
    if (!parseJson(json, root))
        return refuse(error, "not valid JSON");
    if (const Json* buffers = root.get("buffers"); buffers && buffers->at(0)) {
        const Json* len = buffers->at(0)->get("byteLength");
        const Json* uri = buffers->at(0)->get("uri");
        if (uri && uri->kind == Json::string && uri->text.rfind("data:", 0) == 0)
            return refuse(error, "embedded data: buffers are not read");
        if (!len || !len->isInt() || len->num > static_cast<double>(bin.size()))
            return refuse(error, "the buffer is shorter than the file says");
    }
    Doc doc{root, bin, out, error};
    if (!doc.run()) {
        out = {};
        return false;
    }
    return finish(out, error);
}

bool parseGlb(const uint8_t* data, size_t size, Mesh& out, std::string& error) {
    out = {};
    auto u32 = [&](size_t at) {
        return static_cast<uint32_t>(data[at] | (data[at + 1] << 8) | (data[at + 2] << 16)) | (static_cast<uint32_t>(data[at + 3]) << 24);
    };
    if (!data || size < 20 || size > mostFileBytes)
        return refuse(error, "too short (or too big) for a GLB");
    if (u32(0) != 0x46546C67u)
        return refuse(error, "not a GLB (bad magic)");
    if (u32(4) != 2)
        return refuse(error, "not GLB version 2");
    if (u32(8) != size)
        return refuse(error, "the GLB length does not match the file (truncated?)");
    const uint32_t jsonLength = u32(12);
    if (u32(16) != 0x4E4F534Au || jsonLength > size - 20)
        return refuse(error, "the GLB's first chunk is not its JSON");
    const std::string json(reinterpret_cast<const char*>(data + 20), jsonLength);
    std::vector<uint8_t> bin;
    const size_t next = 20 + size_t{jsonLength};
    if (next + 8 <= size) {
        const uint32_t binLength = u32(next);
        if (u32(next + 4) != 0x004E4942u || binLength > size - next - 8)
            return refuse(error, "the GLB's BIN chunk is broken");
        bin.assign(data + next + 8, data + next + 8 + binLength);
    }
    // trailing spaces in the JSON chunk (padding) are allowed by the parser
    return parseGltf(json, bin, out, error);
}

bool readFile(const std::string& path, std::vector<uint8_t>& out, size_t most) {
    out.clear();
#ifdef _WIN32 // the path is UTF-8 (the Spidy folder may have any name)
    std::wstring wide(static_cast<size_t>(std::max(0, MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0))), L'\0');
    if (wide.empty() || !MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wide.data(), static_cast<int>(wide.size())))
        return false;
    FILE* raw = _wfopen(wide.c_str(), L"rb");
#else
    FILE* raw = std::fopen(path.c_str(), "rb");
#endif
    if (!raw)
        return false;
    std::unique_ptr<FILE, int (*)(FILE*)> f(raw, &std::fclose);
    if (!f)
        return false;
    uint8_t chunk[65536];
    for (;;) {
        const size_t n = std::fread(chunk, 1, sizeof chunk, f.get());
        if (n == 0)
            break;
        if (out.size() + n > most)
            return out.clear(), false;
        out.insert(out.end(), chunk, chunk + n);
    }
    return !std::ferror(f.get());
}

bool decimate(Mesh& mesh, uint32_t most) {
    if (!mesh.ok())
        return false;
    if (mesh.triangles() <= most)
        return true;
    const Vec3 size = mesh.high - mesh.low;
    const float longest = std::max({size.x, size.y, size.z, 1e-6f});
    for (int cells = 160; cells >= 4; cells = cells * 4 / 5) {
        const float cell = longest / static_cast<float>(cells);
        auto key = [&](Vec3 p) {
            const auto ix = static_cast<uint64_t>(std::clamp(static_cast<int>((p.x - mesh.low.x) / cell), 0, 4095));
            const auto iy = static_cast<uint64_t>(std::clamp(static_cast<int>((p.y - mesh.low.y) / cell), 0, 4095));
            const auto iz = static_cast<uint64_t>(std::clamp(static_cast<int>((p.z - mesh.low.z) / cell), 0, 4095));
            return ix | iy << 12 | iz << 24;
        };
        std::unordered_map<uint64_t, std::pair<Vec3, int>> sums;
        for (const Vec3& p : mesh.corners) {
            auto& s = sums[key(p)];
            s.first = s.first + p;
            ++s.second;
        }
        Mesh next;
        std::map<std::array<uint64_t, 3>, bool> seen;
        for (uint32_t t = 0; t < mesh.triangles(); ++t) {
            std::array<uint64_t, 3> k{key(mesh.corners[t * 3]), key(mesh.corners[t * 3 + 1]), key(mesh.corners[t * 3 + 2])};
            if (k[0] == k[1] || k[1] == k[2] || k[0] == k[2])
                continue;
            std::array<uint64_t, 3> sorted = k;
            std::sort(sorted.begin(), sorted.end());
            if (seen[sorted])
                continue;
            seen[sorted] = true;
            for (uint64_t c : k) {
                const auto& s = sums[c];
                next.corners.push_back(s.first * (1.f / static_cast<float>(s.second)));
            }
            next.colors.push_back(mesh.colors[t]);
        }
        if (next.ok() && next.triangles() <= most) {
            next.sourceTriangles = mesh.sourceTriangles;
            next.decimated = true;
            bounds(next);
            mesh = std::move(next);
            return true;
        }
    }
    return false;
}

Status loadFolder(const std::string& folder, bool mayDecimate, Mesh& out, std::string& error) {
    out = {};
    error.clear();
    std::vector<uint8_t> bytes;
    bool parsed = false, found = false;
    for (const char* name : {"scene.glb", "model.glb"}) {
        if (readFile(folder + name, bytes)) {
            found = true;
            parsed = parseGlb(bytes.data(), bytes.size(), out, error);
            break;
        }
    }
    if (!found) {
        std::vector<uint8_t> text;
        if (readFile(folder + "scene.gltf", text, 16u << 20)) {
            found = true;
            const std::string json(text.begin(), text.end());
            std::vector<uint8_t> bin;
            if (!readFile(folder + "scene.bin", bin))
                error = "scene.gltf without its scene.bin";
            else
                parsed = parseGltf(json, bin, out, error);
        }
    }
    if (!found) {
        error = "no scene.glb, model.glb or scene.gltf in " + folder;
        return missing;
    }
    if (!parsed) {
        out = {};
        return invalid;
    }
    if (out.triangles() > drawTriangles) {
        if (!mayDecimate) {
            error = std::to_string(out.triangles()) + " triangles, over the " + std::to_string(drawTriangles) +
                    " budget, and its licence allows no changes";
            out = {};
            return overBudget;
        }
        if (!decimate(out, drawTriangles)) {
            error = "could not reduce it under the budget";
            out = {};
            return invalid;
        }
    }
    return loaded;
}

Status Library::load(uint32_t style, const std::string& modelsRoot) {
    if (style == 0 || style >= slots)
        return notTried;
    if (status[style] != notTried)
        return status[style];
    status[style] = loadFolder(modelsRoot + slotFolders[style] + "/", slotMayDecimate[style], meshes[style], error[style]);
    return status[style];
}

Library& library() {
    static Library* one = new Library; // never freed (frames in flight point at its meshes)
    return *one;
}

Fit fit(const Mesh& mesh, float span) {
    Fit f;
    const Vec3 size = mesh.high - mesh.low;
    const float s[3]{size.x, size.y, size.z};
    f.centre = (mesh.low + mesh.high) * .5f;
    f.across = s[0] >= s[1] && s[0] >= s[2] ? 0 : s[1] >= s[2] ? 1 : 2;
    f.thin = 3 - f.across;
    for (int k = 0; k < 3; ++k)
        if (k != f.across && s[k] <= s[3 - f.across - k])
            f.thin = k;
    f.along = 3 - f.across - f.thin;
    f.scale = s[f.across] > 1e-6f ? span / s[f.across] : 0;
    return f;
}

} // namespace spidy::mesh_model
