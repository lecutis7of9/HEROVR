// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Version 0.0.5 SOUND EFFECTS: the embedded WAVs (portable part; sfx.hpp).
#include "spidy/sfx.hpp"

#include <cstring>

namespace spidy::sfx_data {
#define SPIDY_SFX(stem)                    \
    extern const std::uint8_t stem[];      \
    extern const std::size_t stem##Size;
SPIDY_SFX(sonic_boom)
SPIDY_SFX(repulsor_fire)
SPIDY_SFX(chest_beam)
SPIDY_SFX(beam_ready)
SPIDY_SFX(baton_whoosh)
SPIDY_SFX(baton_hit)
SPIDY_SFX(repulsor_charge) // Version 0.0.6 (Update 39.0.2) REPULSOR: the take's charge whine
SPIDY_SFX(pistol_shot)
SPIDY_SFX(shotgun_blast)
SPIDY_SFX(dry_click)
SPIDY_SFX(mag_drop)
SPIDY_SFX(mag_seat)
SPIDY_SFX(holster)
SPIDY_SFX(claw_snikt)
SPIDY_SFX(heat_vision)
SPIDY_SFX(freeze_breath)
SPIDY_SFX(shield_throw) // Version 0.0.7 CAPTAIN AMERICA
SPIDY_SFX(shield_ricochet)
SPIDY_SFX(shield_catch)
SPIDY_SFX(shield_clang)
SPIDY_SFX(thor_lightning_1) // Version 0.0.72: THOR's lightning, the player's own
SPIDY_SFX(thor_lightning_2)
SPIDY_SFX(pistol_reload) // Version 0.0.72: the player's own (tools/sfx/user_guns.py)
SPIDY_SFX(van_start)  // Version 0.0.73: the PUNISHER VAN, the player's own (tools/sfx/van_engine_0073.py)
SPIDY_SFX(van_engine)
SPIDY_SFX(van_door_slam)
#undef SPIDY_SFX
} // namespace spidy::sfx_data

namespace spidy::sfx {
namespace {
uint32_t u32(const uint8_t* p) { return uint32_t{p[0]} | uint32_t{p[1]} << 8 | uint32_t{p[2]} << 16 | uint32_t{p[3]} << 24; }
uint16_t u16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | p[1] << 8); }
} // namespace

Pcm parseWav(const uint8_t* file, size_t size) {
    Pcm pcm;
    if (!file || size < 12 || std::memcmp(file, "RIFF", 4) || std::memcmp(file + 8, "WAVE", 4))
        return pcm;
    bool fmt = false;
    size_t at = 12;
    while (at + 8 <= size) {
        const uint8_t* chunk = file + at;
        const uint32_t length = u32(chunk + 4);
        if (length > size - at - 8)
            return Pcm{};
        if (!std::memcmp(chunk, "fmt ", 4)) {
            if (length < 16 || u16(chunk + 8) != 1)
                return Pcm{};
            pcm.channels = u16(chunk + 10);
            pcm.rate = u32(chunk + 12);
            pcm.bits = u16(chunk + 22);
            fmt = pcm.channels >= 1 && pcm.channels <= 2 && pcm.rate >= 8000 && pcm.rate <= 192000 &&
                  (pcm.bits == 16 || pcm.bits == 8);
            if (!fmt)
                return Pcm{};
        } else if (!std::memcmp(chunk, "data", 4)) {
            if (!fmt || length == 0)
                return Pcm{};
            pcm.data = chunk + 8;
            pcm.bytes = length - length % (pcm.channels * (pcm.bits / 8));
            pcm.ok = pcm.bytes > 0;
            return pcm;
        }
        at += 8 + length + (length & 1);
    }
    return Pcm{};
}

const Pcm& sound(Id id) {
    static const Pcm table[sounds] = {
        parseWav(sfx_data::sonic_boom, sfx_data::sonic_boomSize),
        parseWav(sfx_data::repulsor_fire, sfx_data::repulsor_fireSize),
        parseWav(sfx_data::chest_beam, sfx_data::chest_beamSize),
        parseWav(sfx_data::beam_ready, sfx_data::beam_readySize),
        parseWav(sfx_data::baton_whoosh, sfx_data::baton_whooshSize),
        parseWav(sfx_data::baton_hit, sfx_data::baton_hitSize),
        parseWav(sfx_data::repulsor_charge, sfx_data::repulsor_chargeSize),
        parseWav(sfx_data::pistol_shot, sfx_data::pistol_shotSize),
        parseWav(sfx_data::shotgun_blast, sfx_data::shotgun_blastSize),
        parseWav(sfx_data::dry_click, sfx_data::dry_clickSize),
        parseWav(sfx_data::mag_drop, sfx_data::mag_dropSize),
        parseWav(sfx_data::mag_seat, sfx_data::mag_seatSize),
        parseWav(sfx_data::holster, sfx_data::holsterSize),
        parseWav(sfx_data::claw_snikt, sfx_data::claw_sniktSize),
        parseWav(sfx_data::heat_vision, sfx_data::heat_visionSize),
        parseWav(sfx_data::freeze_breath, sfx_data::freeze_breathSize),
        parseWav(sfx_data::shield_throw, sfx_data::shield_throwSize),
        parseWav(sfx_data::shield_ricochet, sfx_data::shield_ricochetSize),
        parseWav(sfx_data::shield_catch, sfx_data::shield_catchSize),
        parseWav(sfx_data::shield_clang, sfx_data::shield_clangSize),
        parseWav(sfx_data::pistol_reload, sfx_data::pistol_reloadSize),
        parseWav(sfx_data::thor_lightning_1, sfx_data::thor_lightning_1Size),
        parseWav(sfx_data::thor_lightning_2, sfx_data::thor_lightning_2Size),
        parseWav(sfx_data::van_start, sfx_data::van_startSize),   // 0.0.73: the player's van (van_engine_0073.py)
        parseWav(sfx_data::van_engine, sfx_data::van_engineSize),
        parseWav(sfx_data::van_door_slam, sfx_data::van_door_slamSize)};
    static const Pcm none{};
    return id < sounds ? table[id] : none;
}

Requests& requests() {
    static Requests r;
    return r;
}

} // namespace spidy::sfx

