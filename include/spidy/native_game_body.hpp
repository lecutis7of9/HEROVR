#pragma once
// Update 0.0.72 GAME BODY (EXPERIMENTAL): the strict checks the game's own
// hero body waits behind (game_body.hpp). While the setting is on, a thread
// of its own reads the game (read-only, every read guarded) and finds the
// pose writer and conversion again by their bytes, checks the hero, mover
// manager and mover vtables by the exe's own RTTI, and every 250 ms finds
// the player in the registry (game_player::find). Only after three rounds in
// a row pass on the same player record is the body allowed: game_xr hooks
// the writer then (native_body::startAt) and shows the body only while
// verified() holds. Any failed check or another record hides it again (the
// hero hidden, the gloves drawn) until three passes more. Never a crash,
// never flat screen: a failed check only means "no body".
#include <cstdint>
#include <string>

namespace spidy::native_game_body {
struct Found {
    uintptr_t writer{}, toModel{}; // RVAs, 0 until found
    bool refound{};                // not at the CANDIDATE RVAs: found again by bytes
};
// Starts the checks (once per base; again after end()).
void begin(uintptr_t base);
void end();
bool running();
// The body may be shown for this player record.
bool verified(uintptr_t record);
Found found();
// The latest state for the log: "verified ..." or "refused: <check>", and a
// serial that changes with it.
std::string state(uint32_t& serial);
} // namespace spidy::native_game_body
