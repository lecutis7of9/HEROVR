// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
// ULTIMATE ALLIANCE EXPANSION (ua_expansion.hpp).
#include "spidy/ua_expansion.hpp"

#include <cctype>
#include <cmath>
#include <sstream>

namespace spidy::ua_expansion {

const char* refuse(const Gate& g) {
    if (!g.optionOn)
        return "UA LOCATIONS is OFF (turn it on, then restart)";
    if (!g.modelOk || !g.roomOk)
        return "not converted yet - run tools/ua/convert_ua.py on your own disc";
    if (!g.texturesReady)
        return "the models' textures are not ready";
    if (g.flatScreen || !g.headTracked)
        return "put the headset on";
    if (g.hideoutActive)
        return "leave the hideout first";
    if (g.inVehicle)
        return "get out of the vehicle first";
    if (g.airborne)
        return "land first";
    return nullptr;
}

Row rowOf(bool optionOn, bool modelOk, bool roomOk, bool inside) {
    if (inside)
        return Row::inside;
    if (!optionOn)
        return Row::off;
    return modelOk && roomOk ? Row::ready : Row::missing;
}

const char* rowValue(Row row) {
    switch (row) {
    case Row::ready:
        return "ENTER";
    case Row::inside:
        return "RETURN TO CITY";
    case Row::off:
        return "OFF";
    default:
        return "NOT CONVERTED";
    }
}

bool parseNpcs(const std::string& text, std::vector<Npc>& out, std::string& error) {
    out.clear();
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line) || line.rfind("HEROVR-UA-NPCS 1", 0) != 0) {
        error = "not a HERO VR UA npcs.txt";
        return false;
    }
    int n = 1;
    while (std::getline(in, line)) {
        ++n;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream l(line);
        std::string word;
        Npc npc;
        if (!(l >> word) || word != "npc" || !(l >> npc.id >> npc.at.x >> npc.at.y >> npc.at.z >> npc.yawDeg) ||
            !finite(npc.at) || !std::isfinite(npc.yawDeg) || std::abs(npc.at.x) > 1e4f || std::abs(npc.at.y) > 1e4f ||
            std::abs(npc.at.z) > 1e4f) {
            error = "line " + std::to_string(n) + ": not \"npc <id> x y z yaw\"";
            out.clear();
            return false;
        }
        if (out.size() >= mostNpcs) {
            error = "more than " + std::to_string(mostNpcs) + " characters";
            out.clear();
            return false;
        }
        out.push_back(std::move(npc));
    }
    return true;
}

std::string npcLabel(const std::string& id) {
    // the map's ids: <name>[npc]<nn> ("nickfurynpc01", "wyattwingfoot01")
    std::string s = id;
    while (!s.empty() && std::isdigit(static_cast<unsigned char>(s.back())))
        s.pop_back();
    if (s.size() > 3 && s.compare(s.size() - 3, 3, "npc") == 0)
        s.resize(s.size() - 3);
    if (s.size() > 3 && s.compare(0, 3, "sp_") == 0)
        s.erase(0, 3);
    static const struct {
        const char* id;
        const char* name;
    } known[] = {{"nickfury", "NICK FURY"},       {"wyattwingfoot", "WYATT WINGFOOT"}, {"blackwidow", "BLACK WIDOW"}, {"hankpym", "HANK PYM"},
                 {"spiderwoman", "SPIDER-WOMAN"}, {"captainamerica", "CAPTAIN AMERICA"}, {"ironman", "IRON MAN"},
                 {"spiderman", "SPIDER-MAN"},     {"wolverine", "WOLVERINE"},           {"thor", "THOR"},
                 {"shield_agent", "S.H.I.E.L.D. AGENT"}, {"shieldagent", "S.H.I.E.L.D. AGENT"}};
    for (const auto& k : known)
        if (s == k.id)
            return k.name;
    for (auto& c : s)
        c = c == '_' ? ' ' : static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s.empty() ? std::string("CHARACTER") : s;
}

} // namespace spidy::ua_expansion
