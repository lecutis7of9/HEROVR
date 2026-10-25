#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

// The local player, found in the game's own component registry (decoded at
// 16798f0; tools/capture_game_state.py reads the same table from outside).
// Loading a save, a respawn or a switch to Peter, MJ or Miles replaces the
// player's actor and components, so a player found once goes stale.
namespace spidy::game_player {
struct Player {
    // Hero component (vtable 38a93c8), its actor record (whose first field
    // points to the actor's transform) and the actor's MoverStandard.
    uint64_t hero{}, record{}, mover{};
    bool operator==(const Player&) const = default;
};
// Exactly one local hero with exactly one movement manager, or false: no
// save is loaded, or a level change has two heroes or none for a moment.
bool find(uintptr_t base, Player& out);
// The player's components are still registered and still belong to its actor.
bool live(uintptr_t base, const Player& player);
// The hero's HeroHealth (in Spider-Man 2 vtable 541edd0: a SegmentedHealth,
// a Health; Remastered's was 38a6df0), a component of his actor `record`.
// 0 for none, or for more than one.
uint64_t heroHealth(uintptr_t base, uint64_t record);
// Update 36, static (the exe, October 24): his hit points are a float at
// +0xd0 and their most at +0xa0, not Remastered's +0x88. Health's virtual
// getter (vtable slot 13, 43df30: movss xmm0,[rcx+0d0h]) and its is-dead test
// (slot 10, 43df40: 0 >= [rcx+0d0h]) read +0xd0, which HeroHealth inherits;
// its setter (slot 14, 34797c0) clamps the new value to 0..[+0a0h] before it
// stores it at +0xd0 (the old one goes to +0xd8). HeroHealth's own setter
// (ee75b0) first caps it at (1 - [+208h]) times [+0a0h].
inline constexpr uintptr_t hitPointsAt = 0xd0, maxHitPointsAt = 0xa0;

// Keeps finding the player on its own thread: a registry scan takes a few
// milliseconds, too long for the headset's frame loop.
class Watch {
  public:
    ~Watch() {
        stop();
    }
    void start(uintptr_t base);
    void stop();
    // The live player (all zero while there is none), and how many times
    // the answer has changed.
    Player current(uint64_t& changes) const;
    // The current player's HeroHealth (heroHealth), 0 while there is none.
    uint64_t health() const;

  private:
    void run();
    uintptr_t base_{};
    mutable std::mutex lock_;
    std::condition_variable wake_;
    std::thread thread_;
    bool stopping_{};
    Player player_{};
    uint64_t changes_{}, health_{};
};
} // namespace spidy::game_player
