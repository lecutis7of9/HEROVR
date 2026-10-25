// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR (Ilya Mezerowsky).
#pragma once
// Update 19: the MUSIC page's player (Windows): the songs in the Music folder
// (phoneMusicFolder: <install folder>\Music), decoded by Media Foundation (MP3,
// WAV, FLAC, M4A/AAC, WMA: what Windows 10 and 11 decode out of the box) and
// played through WASAPI (the default speakers or headset, shared, so the
// game's own sound carries on beside it), on a thread of its own. The folder
// is looked through on another (at start, every 3 s and on rescan()): no
// file call, decode or sound call is ever made on the caller's thread.
//
// Fail-safe: every call returns at once and never throws. A song that will
// not open or decode is skipped (status().note says which) and the next one
// plays; with no sound device the player waits for one; should the thread
// itself fail, status().available is false and the page shows MUSIC OFF;
// the game, VR and the rest of the mod are untouched either way.
#include "music_player.hpp"
#include <memory>
#include <string>
#include <vector>

namespace spidy::music {
class Player {
  public:
    struct Status {
        bool available = true; // false: the player failed this session (note says why)
        int tracks{}, track = -1;
        bool playing{}, paused{};
        float elapsed{}, total{}; // seconds; total 0 not known
        std::string song, note, folder;
        std::vector<std::string> names; // every song's shown name, A-Z
        uint32_t serial{};               // grows with every change
    };
    Player();
    ~Player(); // stops the music, joins its threads
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;
    // The newest status into `out`, unless the player is publishing one at
    // this instant (false: `out` as it was; ask again next frame).
    bool status(Status& out) const;
    // The newest spectrum (false: none newer than the last read).
    bool spectrum(Spectrum& out);
    void rescan();
    void play(int track);    // from its start
    void toggle();           // play or pause (none picked: the first)
    void stop();             // to the song's start, silent
    void previous();         // previousTrack's
    void next();
    void seekBy(float seconds);
    void seekTo(float fraction); // 0..1 of the song
    void setVolume(float percent); // 0-100, gainOf's
    // The song to start from at the next toggle (launch: live.txt music_track), no sound.
    void select(int track);
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace spidy::music
