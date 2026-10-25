// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
#pragma once
// Update 24: the DLL names a Windows module (PE file) imports, read from its bytes, so the launcher can tell
// whether Spidy's modules still need Microsoft's Visual C++ Redistributable (a build before Update 24's /MT)
// or have the runtime built in. tools/player_check.py reads them the same way. Portable (tests on Linux).
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace spidy::pe {

namespace detail {
template <class T> bool read(const std::vector<unsigned char>& data, std::size_t at, T& out) {
    if (at > data.size() || data.size() - at < sizeof(T))
        return false;
    std::memcpy(&out, data.data() + at, sizeof(T));
    return true;
}
} // namespace detail

// Lower-case DLL names in import order; empty for a file that is not a PE module (or is damaged).
inline std::vector<std::string> imports(const std::vector<unsigned char>& data) {
    using detail::read;
    std::vector<std::string> names;
    std::uint32_t pe{};
    if (data.size() < 64 || data[0] != 'M' || data[1] != 'Z' || !read(data, 0x3C, pe) ||
        !(pe + 4 <= data.size() && std::memcmp(data.data() + pe, "PE\0\0", 4) == 0))
        return names;
    std::uint16_t sections{}, optionalSize{}, magic{};
    if (!read(data, pe + 6, sections) || !read(data, pe + 20, optionalSize) || !read(data, pe + 24, magic))
        return names;
    const std::size_t optional = pe + 24u, directories = optional + (magic == 0x20B ? 112u : 96u);
    std::uint32_t count{}, rva{};
    if (!read(data, directories - 4, count) || count < 2 || !read(data, directories + 8, rva) || !rva)
        return names;
    struct Section { std::uint32_t address, size, raw; };
    std::vector<Section> table;
    for (std::uint16_t i = 0; i < sections; ++i) {
        const std::size_t header = optional + optionalSize + 40u * i;
        std::uint32_t virtualSize{}, address{}, rawSize{}, raw{};
        if (!read(data, header + 8, virtualSize) || !read(data, header + 12, address) ||
            !read(data, header + 16, rawSize) || !read(data, header + 20, raw))
            return {};
        table.push_back({address, std::max(virtualSize, rawSize), raw});
    }
    const auto offset = [&](std::uint32_t address, std::size_t& out) {
        for (const auto& s : table)
            if (address >= s.address && address - s.address < s.size) {
                out = std::size_t{s.raw} + (address - s.address);
                return out < data.size();
            }
        return false;
    };
    std::size_t at{};
    if (!offset(rva, at))
        return names;
    for (int entry = 0; entry < 4096; ++entry, at += 20) {
        std::uint32_t fields[5]{};
        for (int f = 0; f < 5; ++f)
            if (!read(data, at + 4u * f, fields[f]))
                return names;
        if (!(fields[0] | fields[1] | fields[2] | fields[3] | fields[4]))
            break;
        std::size_t name{};
        if (!offset(fields[3], name))
            return names;
        const auto end = std::find(data.begin() + static_cast<std::ptrdiff_t>(name), data.end(), '\0');
        std::string text(data.begin() + static_cast<std::ptrdiff_t>(name), end);
        for (auto& ch : text)
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        names.push_back(std::move(text));
    }
    return names;
}

// One of Microsoft's Visual C++ Redistributable DLLs (not the Universal CRT, which Windows 10 and 11 have).
inline bool vcRedistDll(std::string_view name) {
    for (std::string_view stem : {"vcruntime140", "msvcp140", "concrt140", "vcomp140"})
        if (name.substr(0, stem.size()) == stem && name.size() >= 4 && name.substr(name.size() - 4) == ".dll")
            return true;
    return false;
}

inline bool needsVcRedist(const std::vector<unsigned char>& data) {
    const auto names = imports(data);
    return std::any_of(names.begin(), names.end(), [](const std::string& n) { return vcRedistDll(n); });
}

} // namespace spidy::pe
