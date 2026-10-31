#pragma once
#include "phone.hpp"
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// The phone's music (phone.hpp): the songs in the music folders (musicFolders:
// Documents\HERO VR\Music, then the one beside the launcher), played through Windows' own media player (MCI) on a thread of its
// own. Opening a song takes MCI 60 to 250 ms, more from a slow disk, and on
// the XR worker it held the headset's frames that long at every song, an
// autoplayed one mid-swing included (October 19). Each call here returns at
// once; what plays shows in status() a moment later. A song MCI cannot play
// (an .m4a or .aac without a codec for it, a broken file) is passed over for
// the next one. AUTOPLAY and the volume are kept from session to session, in
// phone.ini beside the launcher's settings (%APPDATA%\Spidy).
namespace spidy {
class PhoneMusic {
  public:
    struct Status {
        int tracks{}, track{}; // track counts from 0; tracks 0: no songs
        bool playing{}, autoplay = true;
        // The song is one none of them would play from: none plays.
        bool failed{};
        int volume = phoneVolume; // percent
        char song[64]{};          // its shown name (phoneSongName)
    };
    PhoneMusic();
    // Stops the music.
    ~PhoneMusic();
    PhoneMusic(const PhoneMusic&) = delete;
    PhoneMusic& operator=(const PhoneMusic&) = delete;
    Status status() const;
    // Looks for songs again (mp3, wav, m4a, wma, aac) in the music folders,
    // unless one is playing or paused, whose list stays.
    void load();
    // Plays from where it was paused, or pauses.
    void toggle();
    void next();
    void previous();
    // Percent, 0 to 100: the song playing at once, and every one after it.
    void setVolume(int percent);
    void setAutoplay(bool on);

  private:
    struct Player;
    std::unique_ptr<Player> player_;
};
// The launcher's folder: the nearest one up from this module with the
// launcher in it (the game loads a copy of the module from
// reports\bridge-modules\<hash>, tools/bridge_game.py).
std::wstring spidyHome();
// 0.0.74 test 10 MUSIC KEPT: the songs' own folder, Documents\HERO VR\Music
// (vehicle074.hpp), made if missing with its README. It is outside the install,
// so an update, a new version unzipped beside the old one or a revert never
// leaves the songs behind (tools/live_settings.py copies an older install's
// songs into it once). Without a Documents folder: <install folder>\Music.
std::wstring phoneMusicFolder();
// Every folder songs are read from, in order: phoneMusicFolder, then the
// install's own Music folder (where they were up to 0.0.73), made if missing.
std::vector<std::wstring> musicFolders();
// The songs (music::playable; not hidden or system, not empty) in `folders`
// and their sub-folders (music::folderDepth down; no links followed), as full
// paths with their sizes, at most music::songCap; one found twice
// (music::songKey: same name and size) is kept once, as found first. `stop`
// set ends the search early.
std::vector<std::pair<std::wstring, uint64_t>> musicFiles(const std::vector<std::wstring>& folders,
                                                         const std::atomic<bool>* stop = nullptr);
} // namespace spidy
