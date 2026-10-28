// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Version 0.0.7 BATARANG STYLE: the strict glTF 2.0 reader (mesh_model.hpp).
#include "spidy/mesh_model.hpp"

#include "spidy/calibration_art.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cctype>
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
    // Version 0.0.72: TEXCOORD_0 (VEC2) / NORMAL (VEC3): FLOAT, or normalized UNSIGNED_BYTE / SHORT.
    bool floatAccessor(uint32_t index, size_t want, size_t comps, std::vector<float>& out_) {
        const Json* list = root.get("accessors");
        const Json* a = list ? list->at(index) : nullptr;
        if (!a || a->kind != Json::object || a->get("sparse"))
            return refuse(error, "bad TEXCOORD_0 / NORMAL accessor");
        const Json* bv = a->get("bufferView");
        const Json* type = a->get("componentType");
        const Json* count = a->get("count");
        const Json* kind = a->get("type");
        const Json* off = a->get("byteOffset");
        if (!bv || !bv->isInt() || !type || !type->isInt() || !count || !count->isInt() || !kind || kind->kind != Json::string ||
            (off && !off->isInt()) || static_cast<size_t>(count->num) != want ||
            kind->text != (comps == 2 ? "VEC2" : "VEC3"))
            return refuse(error, "bad TEXCOORD_0 / NORMAL accessor");
        const auto component = static_cast<uint32_t>(type->num);
        const Json* norm = a->get("normalized");
        const size_t one = component == 5126 ? 4 : (component == 5123 || component == 5121) && norm && norm->yes ? (component == 5123 ? 2 : 1) : 0;
        if (!one)
            return refuse(error, "TEXCOORD_0 / NORMAL must be FLOAT (or normalized UNSIGNED_BYTE / SHORT)");
        size_t offset{}, length{}, stride{};
        if (!view(static_cast<uint32_t>(bv->num), offset, length, stride))
            return false;
        const size_t extra = off ? static_cast<size_t>(off->num) : 0, size = comps * one, step = stride ? stride : size;
        if (want == 0 || step < size || extra > length || (want - 1) * step + size > length - extra)
            return refuse(error, "TEXCOORD_0 / NORMAL outside its bufferView");
        out_.resize(want * comps);
        for (size_t i = 0; i < want; ++i) {
            const uint8_t* at = bin.data() + offset + extra + i * step;
            for (size_t k = 0; k < comps; ++k) {
                float f;
                if (one == 4)
                    std::memcpy(&f, at + k * 4, 4);
                else if (one == 2)
                    f = static_cast<float>(at[k * 2] | (at[k * 2 + 1] << 8)) / 65535.f;
                else
                    f = static_cast<float>(at[k]) / 255.f;
                if (!std::isfinite(f) || std::abs(f) > 1e4f)
                    return refuse(error, "a NaN or huge TEXCOORD_0 / NORMAL");
                out_[i * comps + k] = f;
            }
        }
        return true;
    }
    // Build 9 SUITS: JOINTS_0 (UNSIGNED_BYTE / SHORT VEC4, each under 16: suit_wear's bones) or WEIGHTS_0 (FLOAT,
    // or normalized UNSIGNED_BYTE / SHORT VEC4: summed to 255 here). false: the skin is not read (the mesh still is).
    bool skinAccessor(uint32_t index, size_t want, bool weights, std::vector<uint8_t>& out_) {
        const Json* list = root.get("accessors");
        const Json* a = list ? list->at(index) : nullptr;
        if (!a || a->kind != Json::object || a->get("sparse"))
            return false;
        const Json* bv = a->get("bufferView");
        const Json* type = a->get("componentType");
        const Json* count = a->get("count");
        const Json* kind = a->get("type");
        const Json* off = a->get("byteOffset");
        if (!bv || !bv->isInt() || !type || !type->isInt() || !count || !count->isInt() || !kind || kind->kind != Json::string ||
            (off && !off->isInt()) || static_cast<size_t>(count->num) != want || kind->text != "VEC4")
            return false;
        const auto component = static_cast<uint32_t>(type->num);
        const Json* norm = a->get("normalized");
        const bool normal = norm && norm->yes;
        const size_t one = component == 5121 ? 1 : component == 5123 ? 2 : component == 5126 && weights ? 4 : 0;
        if (!one || (weights && one != 4 && !normal) || (!weights && normal))
            return false;
        size_t offset{}, length{}, stride{};
        if (!view(static_cast<uint32_t>(bv->num), offset, length, stride))
            return false;
        const size_t extra = off ? static_cast<size_t>(off->num) : 0, size = 4 * one, step = stride ? stride : size;
        if (want == 0 || step < size || extra > length || (want - 1) * step + size > length - extra)
            return false;
        out_.resize(want * 4);
        for (size_t i = 0; i < want; ++i) {
            const uint8_t* at = bin.data() + offset + extra + i * step;
            float f[4];
            for (size_t k = 0; k < 4; ++k) {
                if (one == 4)
                    std::memcpy(&f[k], at + k * 4, 4);
                else if (one == 2)
                    f[k] = static_cast<float>(at[k * 2] | (at[k * 2 + 1] << 8));
                else
                    f[k] = at[k];
                if (!std::isfinite(f[k]) || f[k] < 0)
                    return false;
            }
            if (!weights) {
                for (size_t k = 0; k < 4; ++k) {
                    if (f[k] >= 16)
                        return false;
                    out_[i * 4 + k] = static_cast<uint8_t>(f[k]);
                }
                continue;
            }
            const float sum = f[0] + f[1] + f[2] + f[3];
            if (!(sum > 0))
                return false;
            int given = 0;
            for (size_t k = 0; k < 4; ++k) {
                const int w = static_cast<int>(f[k] / sum * 255.f + .5f);
                out_[i * 4 + k] = static_cast<uint8_t>(std::clamp(w, 0, 255));
                given += out_[i * 4 + k];
            }
            const size_t top = static_cast<size_t>(std::max_element(f, f + 4) - f); // the rounding onto the heaviest
            out_[i * 4 + top] = static_cast<uint8_t>(std::clamp(out_[i * 4 + top] + 255 - given, 0, 255));
        }
        return true;
    }
    // Version 0.0.72: the material's base-colour image (an index into out.images, -1 none) and its cut.
    int16_t materialImage(const Json* index, bool& cutout) {
        cutout = false;
        const Json* list = root.get("materials");
        const Json* m = index && index->isInt() && list ? list->at(static_cast<size_t>(index->num)) : nullptr;
        if (!m)
            return -1;
        if (const Json* mode = m->get("alphaMode"); mode && mode->kind == Json::string && (mode->text == "MASK" || mode->text == "BLEND"))
            cutout = true;
        const Json* pbr = m->get("pbrMetallicRoughness");
        const Json* ref = pbr ? pbr->get("baseColorTexture") : nullptr;
        const Json* ti = ref ? ref->get("index") : nullptr;
        if (const Json* set = ref ? ref->get("texCoord") : nullptr; set && (!set->isInt() || set->num != 0))
            return -1; // only TEXCOORD_0 is read
        const Json* textures = root.get("textures");
        const Json* t = ti && ti->isInt() && textures ? textures->at(static_cast<size_t>(ti->num)) : nullptr;
        const Json* src = t ? t->get("source") : nullptr;
        if (!src || !src->isInt() || src->num >= static_cast<double>(out.images.size()))
            return -1;
        const auto i = static_cast<int16_t>(src->num);
        if (out.images[static_cast<size_t>(i)].bytes.empty() && out.images[static_cast<size_t>(i)].uri.empty())
            return -1;
        if (cutout)
            out.images[static_cast<size_t>(i)].cutout = true;
        return i;
    }
    // Version 0.0.72: the document's images (a bufferView's bytes, or a relative file name).
    bool readImages() {
        const Json* list = root.get("images");
        if (!list)
            return true;
        if (list->kind != Json::array || list->items.size() > 256)
            return refuse(error, "bad images");
        for (const Json& im : list->items) {
            Mesh::Image one;
            if (const Json* bv = im.get("bufferView"); bv && bv->isInt()) {
                size_t offset{}, length{}, stride{};
                if (!view(static_cast<uint32_t>(bv->num), offset, length, stride))
                    return false;
                if (length <= mostImageBytes)
                    one.bytes.assign(bin.begin() + static_cast<std::ptrdiff_t>(offset), bin.begin() + static_cast<std::ptrdiff_t>(offset + length));
            } else if (const Json* uri = im.get("uri"); uri && uri->kind == Json::string && uri->text.rfind("data:", 0) != 0) {
                // a plain relative name only (no folder above, no drive, no absolute path)
                std::string name;
                for (size_t i = 0; i < uri->text.size(); ++i) {
                    const char c = uri->text[i];
                    if (c == '%' && i + 2 < uri->text.size() && std::isxdigit(static_cast<unsigned char>(uri->text[i + 1])) &&
                        std::isxdigit(static_cast<unsigned char>(uri->text[i + 2]))) {
                        name += static_cast<char>(std::stoi(uri->text.substr(i + 1, 2), nullptr, 16));
                        i += 2;
                    } else
                        name += c == '\\' ? '/' : c;
                }
                if (!name.empty() && name[0] != '/' && name.find(':') == std::string::npos && name.find("..") == std::string::npos)
                    one.uri = name;
            }
            out.images.push_back(std::move(one));
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
            // Version 0.0.72: UVs, normals, the base-colour image
            std::vector<float> uv, nrm;
            if (const Json* t = attrs->get("TEXCOORD_0")) {
                if (!t->isInt() || !floatAccessor(static_cast<uint32_t>(t->num), points.size(), 2, uv))
                    return error.empty() ? refuse(error, "bad TEXCOORD_0") : false;
            }
            if (const Json* nn = attrs->get("NORMAL")) {
                if (!nn->isInt() || !floatAccessor(static_cast<uint32_t>(nn->num), points.size(), 3, nrm))
                    return error.empty() ? refuse(error, "bad NORMAL") : false;
            }
            std::vector<uint8_t> sj, sw; // Build 9 SUITS: the skin (both, or neither)
            if (const Json* j0 = attrs->get("JOINTS_0"), *w0 = attrs->get("WEIGHTS_0"); j0 && w0 && j0->isInt() && w0->isInt())
                if (!skinAccessor(static_cast<uint32_t>(j0->num), points.size(), false, sj) ||
                    !skinAccessor(static_cast<uint32_t>(w0->num), points.size(), true, sw))
                    sj.clear(), sw.clear();
            bool cutout = false;
            const int16_t image = uv.empty() ? int16_t{-1} : materialImage(prim.get("material"), cutout);
            // normals go through the inverse transpose (the cofactors) of the node's 3 x 3
            const float a = world[0], b = world[4], c = world[8], d = world[1], e = world[5], f = world[9], g = world[2], h = world[6],
                        k = world[10];
            const float cof[9]{e * k - f * h, f * g - d * k, d * h - e * g, c * h - b * k, a * k - c * g, b * g - a * h, b * f - c * e, c * d - a * f, a * e - b * d};
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
                const Vec3 p0 = out.corners[out.corners.size() - 3], p1 = out.corners[out.corners.size() - 2], p2 = out.corners.back();
                const Vec3 face = cross(p1 - p0, p2 - p0);
                const Vec3 flat = length(face) > 1e-20f ? normalized(face) : Vec3{0, 1, 0};
                for (size_t k2 = 0; k2 < 3; ++k2) {
                    const uint32_t v = idx[i + k2];
                    out.uvs.push_back(uv.empty() ? 0.f : uv[v * 2]);
                    out.uvs.push_back(uv.empty() ? 0.f : uv[v * 2 + 1]);
                    Vec3 n = flat;
                    if (!nrm.empty()) {
                        const Vec3 m{nrm[v * 3], nrm[v * 3 + 1], nrm[v * 3 + 2]};
                        const Vec3 w{cof[0] * m.x + cof[3] * m.y + cof[6] * m.z, cof[1] * m.x + cof[4] * m.y + cof[7] * m.z,
                                     cof[2] * m.x + cof[5] * m.y + cof[8] * m.z};
                        if (std::isfinite(w.x) && std::isfinite(w.y) && std::isfinite(w.z) && length(w) > 1e-12f)
                            n = normalized(w);
                    }
                    out.normals.push_back(n);
                    if (!sj.empty())
                        for (size_t q = 0; q < 4; ++q)
                            out.skinJoints.push_back(sj[v * 4 + q]), out.skinWeights.push_back(sw[v * 4 + q]);
                }
                out.image.push_back(image);
                out.tints.push_back(painted.empty() ? tint : Vec3{1, 1, 1});
                if (painted.empty()) {
                    out.colors.push_back(image >= 0 ? tint : color);
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
        // Build 9 SUITS: asset.extras.suit {joints: 48, ends: 9 (head top, fingertips), handBack: 6}
        if (const Json* extras = asset->get("extras"); extras && extras->get("suit")) {
            const Json* suit = extras->get("suit");
            float j[48], e[9], hb[6];
            bool sane = numbers(suit->get("joints"), j, 48) && numbers(suit->get("ends"), e, 9) && numbers(suit->get("handBack"), hb, 6);
            for (int k = 0; sane && k < 48; ++k)
                sane = std::abs(j[k]) < 100.f;
            for (int k = 0; sane && k < 9; ++k)
                sane = std::abs(e[k]) < 100.f;
            for (int k = 0; sane && k < 6; ++k)
                sane = std::abs(hb[k]) <= 2.f;
            if (sane) {
                std::memcpy(out.suitEnds, j, sizeof j);
                std::memcpy(out.suitEnds + 48, e, sizeof e);
                std::memcpy(out.suitHandBack, hb, sizeof hb);
                out.hasSuit = true;
            }
        }
        if (!readImages())
            return false;
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
    if (out.uvs.size() != out.colors.size() * 6 || out.normals.size() != out.colors.size() * 3 || out.image.size() != out.colors.size() ||
        out.tints.size() != out.colors.size()) {
        out.uvs.clear(); // never expected (every triangle gets them); untextured then
        out.normals.clear();
        out.image.clear();
        out.tints.clear();
    }
    if (!out.hasSkin()) { // Build 9 SUITS: a primitive without its skin (or a bad one): none worn
        out.skinJoints.clear();
        out.skinWeights.clear();
    }
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
            if (mesh.hasUvs()) { // Version 0.0.72: each kept triangle keeps its corners' UVs, normals, image
                next.uvs.insert(next.uvs.end(), mesh.uvs.begin() + t * 6, mesh.uvs.begin() + t * 6 + 6);
                next.image.push_back(mesh.image[t]);
                next.tints.push_back(mesh.tints[t]);
                const Vec3 n0 = next.corners[next.corners.size() - 3], fc = cross(next.corners[next.corners.size() - 2] - n0, next.corners.back() - n0);
                const Vec3 flat = length(fc) > 1e-20f ? normalized(fc) : Vec3{0, 1, 0};
                next.normals.insert(next.normals.end(), {flat, flat, flat});
            }
        }
        if (next.ok() && next.triangles() <= most) {
            next.sourceTriangles = mesh.sourceTriangles;
            next.images = mesh.images;
            next.decimated = true;
            bounds(next);
            mesh = std::move(next);
            return true;
        }
    }
    return false;
}

Status loadFolder(const std::string& folder, bool mayDecimate, Mesh& out, std::string& error, uint32_t cap) {
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
    // Version 0.0.72: images beside a .gltf (textures\...): read now; one that is missing is untextured
    size_t imageBytes = 0;
    for (Mesh::Image& im : out.images)
        if (im.bytes.empty() && !im.uri.empty()) {
            if (!readFile(folder + im.uri, im.bytes, mostImageBytes) || (imageBytes += im.bytes.size()) > 2 * mostImageBytes)
                im.bytes.clear();
        }
    if (cap == 0 || cap > vanTriangles)
        cap = drawTriangles;
    if (out.triangles() > cap) {
        if (!mayDecimate) {
            error = std::to_string(out.triangles()) + " triangles, over the " + std::to_string(cap) +
                    " budget, and its licence allows no changes";
            out = {};
            return overBudget;
        }
        if (!decimate(out, cap)) {
            error = "could not reduce it under the budget";
            out = {};
            return invalid;
        }
    }
    out.cap = cap;
    return loaded;
}

namespace {
// Version 0.0.72: the untextured look's light copy (in its current colours), when the model is over liteTriangles.
void makeLite(Mesh& m, bool mayChange) {
    m.lite.reset();
    if (!m.ok() || m.triangles() <= liteTriangles || !mayChange)
        return;
    auto copy = std::make_shared<Mesh>(m);
    copy->lite.reset();
    if (decimate(*copy, liteTriangles) && copy->ok())
        m.lite = std::move(copy);
}
} // namespace

Status Library::load(uint32_t style, const std::string& modelsRoot) {
    if (style == 0 || style >= slots)
        return notTried;
    if (status[style] != notTried)
        return status[style];
    status[style] = loadFolder(modelsRoot + slotFolders[style] + "/", slotMayDecimate[style], meshes[style], error[style], slotDrawCap[style]);
    if (status[style] == loaded)
        makeLite(meshes[style], slotMayDecimate[style]);
    return status[style];
}

Library& library() {
    static Library* one = new Library; // never freed (frames in flight point at its meshes)
    return *one;
}

bool Mesh::textured() const {
    if (!hasUvs() || image.size() != colors.size())
        return false;
    bool any = false;
    for (int16_t i : image)
        if (i >= 0) {
            if (static_cast<size_t>(i) >= images.size() || images[static_cast<size_t>(i)].layer < 0)
                return false;
            any = true;
        }
    return any;
}

bool decodeImage(const uint8_t* data, size_t size, Decoder decoder, Picture& out) {
    out = {};
    if (!data || size < 8)
        return false;
    if (std::memcmp(data, "qoif", 4) == 0) {
        calibration_art::Image q;
        if (!calibration_art::decodeQoi({data, size}, q) || q.width <= 0 || q.height <= 0)
            return false;
        out.width = static_cast<uint32_t>(q.width);
        out.height = static_cast<uint32_t>(q.height);
        out.rgba = std::move(q.rgba);
        return true;
    }
    if (!decoder || !decoder(data, size, out))
        return out = {}, false;
    if (!out.width || !out.height || out.width > 16384 || out.height > 16384 || out.rgba.size() != size_t{out.width} * out.height * 4)
        return out = {}, false;
    return true;
}

Picture resample(const Picture& in, uint32_t size) {
    Picture out;
    out.width = out.height = size;
    out.rgba.assign(size_t{size} * size * 4, 0);
    if (!in.width || !in.height || in.rgba.size() != size_t{in.width} * in.height * 4 || !size)
        return out;
    // each output texel: the mean of the source texels it covers (at least the nearest one, bilinear when growing)
    const double sx = static_cast<double>(in.width) / size, sy = static_cast<double>(in.height) / size;
    for (uint32_t y = 0; y < size; ++y)
        for (uint32_t x = 0; x < size; ++x) {
            double acc[4]{};
            if (sx > 1 || sy > 1) {
                const auto x0 = static_cast<uint32_t>(x * sx), x1 = std::max(x0 + 1, std::min(in.width, static_cast<uint32_t>((x + 1) * sx)));
                const auto y0 = static_cast<uint32_t>(y * sy), y1 = std::max(y0 + 1, std::min(in.height, static_cast<uint32_t>((y + 1) * sy)));
                for (uint32_t v = y0; v < y1 && v < in.height; ++v)
                    for (uint32_t u = x0; u < x1 && u < in.width; ++u)
                        for (int c = 0; c < 4; ++c)
                            acc[c] += in.rgba[(size_t{v} * in.width + u) * 4 + c];
                const double n = static_cast<double>((std::min(x1, in.width) - x0) * (std::min(y1, in.height) - y0));
                for (double& a : acc)
                    a /= std::max(1.0, n);
            } else {
                const double fx = std::max(0.0, (x + .5) * sx - .5), fy = std::max(0.0, (y + .5) * sy - .5);
                const auto u0 = std::min(in.width - 1, static_cast<uint32_t>(fx)), v0 = std::min(in.height - 1, static_cast<uint32_t>(fy));
                const uint32_t u1 = std::min(in.width - 1, u0 + 1), v1 = std::min(in.height - 1, v0 + 1);
                const double ax = fx - u0, ay = fy - v0;
                for (int c = 0; c < 4; ++c) {
                    auto at = [&](uint32_t u, uint32_t v) { return static_cast<double>(in.rgba[(size_t{v} * in.width + u) * 4 + c]); };
                    acc[c] = (at(u0, v0) * (1 - ax) + at(u1, v0) * ax) * (1 - ay) + (at(u0, v1) * (1 - ax) + at(u1, v1) * ax) * ay;
                }
            }
            for (int c = 0; c < 4; ++c)
                out.rgba[(size_t{y} * size + x) * 4 + c] = static_cast<uint8_t>(std::clamp(acc[c] + .5, 0.0, 255.0));
        }
    return out;
}

namespace {
float srgbToLinear(uint8_t v) {
    const float c = static_cast<float>(v) / 255.f;
    return c <= .04045f ? c / 12.92f : std::pow((c + .055f) / 1.055f, 2.4f);
}
uint64_t fnv(const std::vector<uint8_t>& b) {
    uint64_t h = 1469598103934665603ull;
    for (uint8_t c : b)
        h = (h ^ c) * 1099511628211ull;
    return h ^ b.size();
}
} // namespace

TextureArray prepareTextures(Library& lib, Decoder decoder, uint32_t size, uint32_t levels) {
    TextureArray out;
    out.size = size;
    out.levels = levels;
    if (!size || !levels || levels > 16 || (size >> (levels - 1)) == 0) {
        out.notes.push_back("bad texture array size");
        return out;
    }
    std::map<std::pair<uint64_t, bool>, int32_t> seen; // (the image's bytes, cut) -> its layer
    std::map<int32_t, Picture> thumbs;                 // each layer at 64 x 64, for the untextured colours
    for (uint32_t slot = 1; slot < slots; ++slot) {
        if (lib.status[slot] != loaded)
            continue;
        Mesh& m = lib.meshes[slot];
        for (size_t i = 0; i < m.images.size(); ++i) {
            Mesh::Image& im = m.images[i];
            im.layer = -1;
            bool used = false;
            for (int16_t t : m.image)
                used = used || t == static_cast<int16_t>(i);
            if (!used || !m.hasUvs())
                continue;
            const std::string name = std::string(slotFolders[slot]) + " image " + std::to_string(i) + (im.uri.empty() ? "" : " (" + im.uri + ")");
            if (im.bytes.empty()) {
                out.notes.push_back(name + ": NOT READ (missing file) - drawn in its material colour");
                continue;
            }
            const auto key = std::make_pair(fnv(im.bytes), im.cutout);
            if (auto f = seen.find(key); f != seen.end()) {
                im.layer = f->second;
                out.notes.push_back(name + ": layer " + std::to_string(im.layer) + " (shared)");
                continue;
            }
            if (out.layers.size() >= mostTextureLayers) {
                out.notes.push_back(name + ": NOT READ (the texture array is full) - drawn in its material colour");
                continue;
            }
            Picture pic;
            if (!decodeImage(im.bytes.data(), im.bytes.size(), decoder, pic)) {
                out.notes.push_back(name + ": NOT READ (could not decode) - drawn in its material colour");
                continue;
            }
            const uint32_t w0 = pic.width, h0 = pic.height;
            Picture base = resample(pic, size);
            pic = {};
            if (!im.cutout) // opaque materials: their alpha is not a cut
                for (size_t p = 3; p < base.rgba.size(); p += 4)
                    base.rgba[p] = 255;
            calibration_art::Image ci;
            ci.width = ci.height = static_cast<int>(size);
            ci.rgba = base.rgba;
            const auto chain = calibration_art::mips(ci, static_cast<int>(levels));
            if (chain.size() != levels) {
                out.notes.push_back(name + ": NOT READ (mips) - drawn in its material colour");
                continue;
            }
            std::vector<uint8_t> layer;
            for (const auto& lv : chain)
                layer.insert(layer.end(), lv.rgba.begin(), lv.rgba.end());
            im.layer = static_cast<int32_t>(out.layers.size());
            out.layers.push_back(std::move(layer));
            seen[key] = im.layer;
            thumbs[im.layer] = resample(base, 64);
            out.notes.push_back(name + ": layer " + std::to_string(im.layer) + " (" + std::to_string(w0) + "x" + std::to_string(h0) + " -> " +
                                std::to_string(size) + (im.cutout ? ", cut-out alpha)" : ")"));
        }
        // the look without the texture array: each triangle its texture's mean (3 corners + centre) x tint
        if (m.hasUvs())
            for (uint32_t t = 0; t < m.triangles(); ++t) {
                const int16_t i = m.image[t];
                if (i < 0 || static_cast<size_t>(i) >= m.images.size() || m.images[static_cast<size_t>(i)].layer < 0)
                    continue;
                const Picture& p = thumbs[m.images[static_cast<size_t>(i)].layer];
                float sum[3]{};
                for (int c = 0; c < 4; ++c) {
                    float u = 0, v = 0;
                    for (int k = 0; k < 3; ++k)
                        if (c == 3 || c == k) {
                            u += m.uvs[t * 6 + k * 2] / (c == 3 ? 3.f : 1.f);
                            v += m.uvs[t * 6 + k * 2 + 1] / (c == 3 ? 3.f : 1.f);
                        }
                    u -= std::floor(u);
                    v -= std::floor(v);
                    const auto x = std::min(63u, static_cast<uint32_t>(u * 64)), y = std::min(63u, static_cast<uint32_t>(v * 64));
                    for (int k = 0; k < 3; ++k)
                        sum[k] += srgbToLinear(p.rgba[(size_t{y} * 64 + x) * 4 + k]) * .25f;
                }
                const Vec3 tint = m.tints[t];
                m.colors[t] = {sum[0] * tint.x, sum[1] * tint.y, sum[2] * tint.z};
            }
        makeLite(m, slotMayDecimate[slot]);
    }
    return out;
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
