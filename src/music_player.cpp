// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR (Ilya Mezerowsky).
// Update 19: the MUSIC page's portable half (music_player.hpp).
#include "spidy/music_player.hpp"
#include "spidy/phone.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace spidy::music {
namespace {
constexpr float pi = 3.14159265358979f;
std::string lower(std::string_view s) {
    std::string out(s);
    for (char& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}
std::string_view baseName(std::string_view path) {
    const auto slash = path.find_last_of("\\/");
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}
const std::array<float, fftSize>& hann() {
    static const std::array<float, fftSize> w = [] {
        std::array<float, fftSize> a{};
        for (int i = 0; i < fftSize; ++i)
            a[static_cast<size_t>(i)] = .5f - .5f * std::cos(2 * pi * static_cast<float>(i) / (fftSize - 1));
        return a;
    }();
    return w;
}
float unit(float db) {
    return std::clamp((db - floorDb) / -floorDb, 0.f, 1.f);
}
// Eases v toward target: quickly up, slowly down.
void follow(float& v, float target, float dt, float up, float down) {
    const float rate = target > v ? up : down;
    v += (target - v) * std::min(1.f, dt * rate);
    if (!std::isfinite(v))
        v = 0;
}
} // namespace

const char* visualizerName(int v) {
    switch (v) {
    case 1:
        return "BARS";
    case 2:
        return "FRACTAL BLOOM";
    case 3:
        return "TUNNEL";
    case 4:
        return "STARBURST";
    default:
        return "ORIGINAL";
    }
}
const char* visualizerKey(int v) {
    switch (v) {
    case 1:
        return "bars";
    case 2:
        return "fractal_bloom";
    case 3:
        return "tunnel";
    case 4:
        return "starburst";
    default:
        return "original";
    }
}
float gainOf(float percent) {
    if (!std::isfinite(percent))
        return 0;
    const float f = std::clamp(percent, 0.f, 100.f) / 100;
    return f * f;
}

void fft(std::complex<float>* x, int n) {
    if (n < 2 || (n & (n - 1)) != 0 || !x)
        return;
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(x[i], x[j]);
    }
    for (int len = 2; len <= n; len <<= 1) {
        const float a = -2 * pi / static_cast<float>(len);
        const std::complex<float> step(std::cos(a), std::sin(a));
        for (int i = 0; i < n; i += len) {
            std::complex<float> w(1, 0);
            for (int k = 0; k < len / 2; ++k) {
                const std::complex<float> u = x[i + k], v = x[i + k + len / 2] * w;
                x[i + k] = u + v;
                x[i + k + len / 2] = u - v;
                w *= step;
            }
        }
    }
}

void Analyzer::reset() {
    ring_.fill(0);
    at_ = 0;
    const float travel = s_.travel;
    const uint32_t serial = s_.serial;
    s_ = Spectrum{};
    s_.travel = travel;
    s_.serial = serial;
}
void Analyzer::push(const float* samples, size_t frames, int channels, int rate) {
    if (!samples || channels < 1 || channels > 16)
        return;
    if (rate >= 4000 && rate <= 768000)
        rate_ = rate;
    for (size_t f = 0; f < frames; ++f) {
        float sum = 0;
        for (int c = 0; c < channels; ++c) {
            const float x = samples[f * static_cast<size_t>(channels) + static_cast<size_t>(c)];
            sum += std::isfinite(x) ? std::clamp(x, -4.f, 4.f) : 0.f;
        }
        ring_[at_] = sum / static_cast<float>(channels);
        at_ = (at_ + 1) % fftSize;
    }
}
Spectrum Analyzer::update(float seconds) {
    const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .25f) : 0.f;
    thread_local std::array<std::complex<float>, fftSize> x;
    const auto& w = hann();
    double square = 0;
    for (size_t i = 0; i < fftSize; ++i) {
        const float v = ring_[(at_ + i) % fftSize];
        square += static_cast<double>(v) * v;
        x[i] = {v * w[i], 0};
    }
    fft(x.data(), fftSize);
    const float binHz = static_cast<float>(rate_) / fftSize;
    const float top = std::min(highHz, static_cast<float>(rate_) * .47f);
    const float norm = fftSize / 4.f; // a full-scale sine's peak through the Hann window
    for (int b = 0; b < bands; ++b) {
        const float f0 = lowHz * std::pow(top / lowHz, static_cast<float>(b) / bands);
        const float f1 = lowHz * std::pow(top / lowHz, static_cast<float>(b + 1) / bands);
        const int i0 = std::max(1, static_cast<int>(std::ceil(f0 / binHz)));
        const int i1 = std::min(fftSize / 2 - 1, static_cast<int>(std::floor(f1 / binHz)));
        float m = 0;
        if (i1 >= i0) {
            for (int i = i0; i <= i1; ++i)
                m = std::max(m, std::abs(x[static_cast<size_t>(i)]));
        } else { // narrower than a bin: the nearest bins, weighted
            const float at = std::clamp(std::sqrt(f0 * f1) / binHz, 1.f, fftSize / 2.f - 2);
            const int i = static_cast<int>(at);
            const float t = at - static_cast<float>(i);
            m = std::abs(x[static_cast<size_t>(i)]) * (1 - t) + std::abs(x[static_cast<size_t>(i + 1)]) * t;
        }
        const float target = unit(20 * std::log10(m / norm + 1e-9f));
        float& v = s_.band[static_cast<size_t>(b)];
        follow(v, target, dt, 28, 5);
        float& p = s_.peak[static_cast<size_t>(b)];
        p = v > p ? v : std::max(0.f, p - dt * .6f);
    }
    const float rms = static_cast<float>(std::sqrt(square / fftSize));
    follow(s_.level, unit(20 * std::log10(rms * 1.4142f + 1e-9f)), dt, 20, 4);
    s_.bass = (s_.band[0] + s_.band[1] + s_.band[2] + s_.band[3]) / 4;
    for (int i = 0; i < wavePoints; ++i) {
        const size_t back = static_cast<size_t>(wavePoints - i) * 8;
        s_.wave[static_cast<size_t>(i)] = std::clamp(ring_[(at_ + fftSize - back) % fftSize], -1.f, 1.f);
    }
    s_.travel = std::fmod(s_.travel + dt * (.25f + 1.5f * s_.level), 4096.f);
    ++s_.serial;
    return s_;
}
Spectrum Analyzer::quiet(float seconds) {
    const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .25f) : 0.f;
    for (int b = 0; b < bands; ++b) {
        follow(s_.band[static_cast<size_t>(b)], 0, dt, 1, 5);
        float& p = s_.peak[static_cast<size_t>(b)];
        p = std::max(0.f, p - dt * .6f);
    }
    follow(s_.level, 0, dt, 1, 4);
    s_.bass = (s_.band[0] + s_.band[1] + s_.band[2] + s_.band[3]) / 4;
    for (float& v : s_.wave)
        v *= std::max(0.f, 1 - dt * 6);
    s_.travel = std::fmod(s_.travel + dt * .1f, 4096.f);
    ++s_.serial;
    return s_;
}

bool playable(std::string_view fileName, uint64_t bytes) {
    const std::string_view name = baseName(fileName);
    if (bytes == 0 || name.empty() || name.front() == '.')
        return false;
    const auto dot = name.find_last_of('.');
    if (dot == std::string_view::npos || dot == 0 || dot + 1 >= name.size())
        return false;
    const std::string ext = lower(name.substr(dot + 1));
    for (const char* e : {"mp3", "wav", "flac", "m4a", "aac", "wma"})
        if (ext == e)
            return true;
    return false;
}
std::vector<Track> songList(const std::vector<std::pair<std::string, uint64_t>>& files) {
    std::vector<std::string> keep;
    for (const auto& [file, bytes] : files)
        if (playable(file, bytes))
            keep.push_back(file);
    std::sort(keep.begin(), keep.end(), [](const std::string& a, const std::string& b) {
        const std::string x = lower(baseName(a)), y = lower(baseName(b));
        return x != y ? x < y : a < b;
    });
    std::vector<Track> out;
    out.reserve(keep.size());
    for (const auto& f : keep)
        out.push_back({f, phoneSongName(f, static_cast<int>(out.size()))});
    return out;
}
int indexOf(const std::vector<Track>& list, const std::string& file) {
    for (size_t i = 0; i < list.size(); ++i)
        if (list[i].file == file)
            return static_cast<int>(i);
    return -1;
}
std::string clockText(float seconds) {
    if (!std::isfinite(seconds) || seconds < 0 || seconds > 359999)
        return "--:--";
    const int s = static_cast<int>(seconds);
    char t[16];
    if (s >= 3600)
        std::snprintf(t, sizeof t, "%d:%02d:%02d", s / 3600, s / 60 % 60, s % 60);
    else
        std::snprintf(t, sizeof t, "%d:%02d", s / 60, s % 60);
    return t;
}
int previousTrack(int track, int tracks, float elapsed) {
    if (tracks < 1)
        return -1;
    if (track < 0 || track >= tracks)
        return 0;
    if (std::isfinite(elapsed) && elapsed > restartSeconds)
        return track;
    return (track + tracks - 1) % tracks;
}
int nextTrack(int track, int tracks) {
    if (tracks < 1)
        return -1;
    if (track < 0 || track >= tracks)
        return 0;
    return (track + 1) % tracks;
}
} // namespace spidy::music
