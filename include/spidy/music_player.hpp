// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR (Ilya Mezerowsky).
#pragma once
// Update 19: the hologram's MUSIC page, its portable half (built on Linux and
// tested there; the Windows decoder and output are music_audio.hpp).
//
// The songs are the files in the music folders (phone_music.hpp musicFolders:
// Documents\HERO VR\Music, then <install folder>\Music; sub-folders too; the
// phone's player reads the same): MP3, WAV, FLAC, M4A/AAC and WMA, as far as the
// PC's Media Foundation decodes them (all of them on Windows 10 and 11), in
// A-Z order of their file names.
//
// The visualizers are fed from the audio thread: the samples it sends to the
// speakers go into an Analyzer (1024-point FFT, Hann window, 32 bands spaced
// evenly in pitch from 40 Hz to 16 kHz, -60..0 dB as 0..1, fast up, slow
// down; the newest 64 samples' shape as the waveform), and each Spectrum is
// handed to the render thread through a TripleBuffer: no lock, no waiting on
// either side.
#include <array>
#include <atomic>
#include <complex>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace spidy::music {
constexpr int bands = 32, wavePoints = 64, fftSize = 1024;
constexpr float lowHz = 40, highHz = 16000, floorDb = -60;
// live.txt music_visualizer: 0 ORIGINAL (red/blue mirrored bars and a
// waveform), 1 BARS, 2 FRACTAL BLOOM, 3 TUNNEL, 4 STARBURST.
constexpr int visualizers = 5;
const char* visualizerName(int); // "ORIGINAL", ...
const char* visualizerKey(int);  // "original", "bars", "fractal_bloom", "tunnel", "starburst"
// live.txt music_volume, percent: the music's own loudness (the game's
// sound is never touched).
constexpr int defaultVolume = 70;
// The gain a volume percent applies to the samples: (v / 100)^2, so the
// slider's middle sounds about half as loud; 0 silent; not finite: 0.
float gainOf(float percent);

struct Spectrum {
    std::array<float, bands> band{}; // 0..1 each, low to high
    std::array<float, bands> peak{}; // each band's falling peak
    std::array<float, wavePoints> wave{}; // -1..1
    float level = 0;  // 0..1, the whole loudness, smoothed
    float bass = 0;   // 0..1, the lowest bands
    float travel = 0; // grows faster the louder the music: what Tunnel and Starburst move by
    uint32_t serial = 0;
};

// One FFT in place (n a power of two; anything else is left as it was).
void fft(std::complex<float>* data, int n);

class Analyzer {
  public:
    // Samples as sent to the speakers but before the volume (a quiet song
    // still moves the picture), `channels` interleaved, `rate` a second.
    // Not finite samples count as silence.
    void push(const float* samples, size_t frames, int channels, int rate);
    // `seconds` since the last update: the new spectrum.
    Spectrum update(float seconds);
    // Paused or stopped: the picture falls to rest.
    Spectrum quiet(float seconds);
    void reset();

  private:
    std::array<float, fftSize> ring_{};
    size_t at_ = 0;
    int rate_ = 48000;
    Spectrum s_;
};

// Lock-free one writer, one reader: the reader always gets the newest
// complete value, never one half written.
template <class T> class TripleBuffer {
  public:
    void write(const T& value) {
        slots_[back_] = value;
        back_ = middle_.exchange(static_cast<uint8_t>(back_ | fresh), std::memory_order_acq_rel) & 3;
    }
    // False if nothing new since the last read (out unchanged).
    bool read(T& out) {
        if (!(middle_.load(std::memory_order_acquire) & fresh))
            return false;
        front_ = middle_.exchange(front_, std::memory_order_acq_rel) & 3;
        out = slots_[front_];
        return true;
    }

  private:
    static constexpr uint8_t fresh = 4;
    std::array<T, 3> slots_{};
    std::atomic<uint8_t> middle_{1};
    uint8_t back_ = 0, front_ = 2;
};

// The song list. A file is a song when its extension is one of
// playableExtensions (any case) and it is not hidden (a name starting with
// "." - macOS's "._" copies included) or empty.
// 0.0.74 test 10 MUSIC KEPT: the folders are read with their sub-folders, this many levels down, and at most
// this many songs in all.
constexpr int folderDepth = 8;
constexpr size_t songCap = 5000;
constexpr const char* playableExtensions = "mp3 wav flac m4a aac wma ogg"; // 0.0.74: + ogg (Media Foundation with Web Media Extensions)
bool playable(std::string_view fileName, uint64_t bytes = 1);
struct Track {
    std::string file; // as given (UTF-8)
    std::string name; // shown: phoneSongName's
};
// 0.0.74 test 10 MUSIC KEPT: one song found twice (Documents\HERO VR\Music and the install's own Music
// folder, or two sub-folders) is the same song when its file name (any case, folders left out) and size are.
std::string songKey(std::string_view path, uint64_t bytes);
// The songs among `files` (name or path, bytes), A-Z by file name ignoring case; a song given twice
// (songKey) is listed once, as given first.
std::vector<Track> songList(const std::vector<std::pair<std::string, uint64_t>>& files);
// Where `file` is in `list` (-1: not there).
int indexOf(const std::vector<Track>& list, const std::string& file);
// "3:07", "1:02:03"; not finite or negative: "--:--".
std::string clockText(float seconds);
// Previous (|<<): back to the start of the song if more than restartSeconds
// in, else the song before (wrapping round).
constexpr float restartSeconds = 3, seekSeconds = 10;
int previousTrack(int track, int tracks, float elapsed);
int nextTrack(int track, int tracks);
} // namespace spidy::music
