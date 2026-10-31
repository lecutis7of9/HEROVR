#include "spidy/phone_music.hpp"
#include "spidy/music_player.hpp"
#include "spidy/vehicle074.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#include <objbase.h>
#include <shlobj.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <cwctype>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

namespace spidy {
namespace {
std::atomic<unsigned> players{};
bool isSong(const std::wstring& name) {
    const auto dot = name.find_last_of(L'.');
    if (dot == std::wstring::npos)
        return false;
    std::wstring ext = name.substr(dot + 1);
    for (auto& c : ext)
        c = static_cast<wchar_t>(std::towlower(c));
    return ext == L"mp3" || ext == L"wav" || ext == L"m4a" || ext == L"wma" || ext == L"aac";
}
std::string utf8(const std::wstring& w) {
    const int size = static_cast<int>(w.size());
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), size, nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(std::max(bytes, 0)), '\0');
    if (bytes > 0)
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), size, s.data(), bytes, nullptr, nullptr);
    return s;
}
// phone.ini, beside the launcher's settings; empty without an APPDATA.
std::wstring settingsFile() {
    PWSTR roaming{};
    std::wstring folder;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roaming)) && roaming)
        folder = std::wstring(roaming) + L"\\Spidy";
    CoTaskMemFree(roaming);
    if (folder.empty())
        return {};
    CreateDirectoryW(folder.c_str(), nullptr);
    return folder + L"\\phone.ini";
}
} // namespace

// The player's thread, and everything MCI's: it alone sends MCI commands.
struct PhoneMusic::Player {
    enum class Command { load, toggle, next, previous, settings };
    mutable std::mutex lock;
    // Shared: what the phone shows, and the commands for the thread. The
    // volume and AUTOPLAY are the caller's once it sets them (volumeSet,
    // autoplaySet); the thread reads them from phone.ini only until then.
    Status shown;
    bool volumeSet{}, autoplaySet{};
    std::deque<Command> commands;
    std::atomic<bool> quit{};
    HANDLE wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    std::thread thread;
    // The thread's own.
    std::wstring alias = L"spidyphone" + std::to_wstring(players++), settings;
    std::vector<std::wstring> files;
    std::vector<std::string> names;
    int track{}, volume = phoneVolume;
    // starting: a song is being opened to play, which the phone shows as
    // playing (no PAUSED for a moment at each change of song).
    bool opened{}, playing{}, failed{}, autoplay = true, starting{};
    MCIDEVICEID device{};

    Player() {
        try {
            if (wake)
                thread = std::thread([this] { run(); });
        } catch (...) {
        }
    }
    ~Player() {
        quit = true;
        if (wake)
            SetEvent(wake);
        if (thread.joinable())
            thread.join();
        if (wake)
            CloseHandle(wake);
    }
    void send(Command c) {
        {
            std::lock_guard guard(lock);
            commands.push_back(c);
        }
        if (wake)
            SetEvent(wake);
    }
    bool mci(const std::wstring& command) const {
        return mciSendStringW(command.c_str(), nullptr, 0, nullptr) == 0;
    }
    // What the thread knows, for the phone; the volume and AUTOPLAY are the
    // caller's own.
    void publish() {
        std::lock_guard guard(lock);
        shown.tracks = static_cast<int>(files.size());
        shown.track = track;
        shown.playing = playing || starting;
        shown.failed = failed;
        const auto t = static_cast<size_t>(track);
        strncpy_s(shown.song, t < names.size() ? names[t].c_str() : "", _TRUNCATE);
    }
    void close() {
        if (opened)
            mci(L"close " + alias);
        opened = playing = false;
        device = 0;
    }
    void applyVolume() {
        // MCI's volume runs 0 to 1000.
        if (opened)
            mci(L"setaudio " + alias + L" volume to " + std::to_wstring(std::clamp(volume, 0, 100) * 10));
    }
    // Plays `from`, or the first after it `step`-wise that will play; with
    // none that will, none plays and the phone says CAN'T PLAY on `from`.
    void start(int from, int step) {
        close();
        failed = false;
        const int count = static_cast<int>(files.size());
        if (!count) {
            track = 0;
            publish();
            return;
        }
        from = (from % count + count) % count;
        starting = true;
        for (int k = 0; k < count && !quit; ++k) {
            track = ((from + step * k) % count + count) % count;
            publish();
            MCI_OPEN_PARMSW open{};
            open.lpstrDeviceType = L"mpegvideo";
            open.lpstrElementName = files[static_cast<size_t>(track)].c_str();
            open.lpstrAlias = alias.c_str();
            if (mciSendCommandW(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_ELEMENT | MCI_OPEN_ALIAS,
                                reinterpret_cast<DWORD_PTR>(&open)))
                continue;
            opened = true;
            device = open.wDeviceID;
            applyVolume();
            if (mci(L"play " + alias)) {
                playing = true;
                starting = false;
                publish();
                return;
            }
            close();
        }
        starting = false;
        track = from;
        failed = true;
        publish();
    }
    void load() {
        if (opened)
            return;
        files.clear();
        names.clear();
        // 0.0.74 test 10 MUSIC KEPT: Documents\HERO VR\Music, then the install's Music folder, sub-folders
        // too (musicFiles skips folders' links, hidden, system and empty files and macOS's ._ twins, and
        // keeps a song found in both once). The phone's MCI player takes the formats isSong names.
        std::vector<std::wstring> songs;
        for (auto& song : musicFiles(musicFolders()))
            if (isSong(song.first))
                songs.push_back(std::move(song.first));
        const auto base = [](const std::wstring& p) {
            const auto slash = p.find_last_of(L"\\/");
            return slash == std::wstring::npos ? p.c_str() : p.c_str() + slash + 1;
        };
        std::stable_sort(songs.begin(), songs.end(), [&](const std::wstring& a, const std::wstring& b) {
            return _wcsicmp(base(a), base(b)) < 0;
        });
        for (auto& path : songs) {
            names.push_back(phoneSongName(utf8(base(path)), static_cast<int>(files.size())));
            files.push_back(std::move(path));
        }
        track = std::clamp(track, 0, std::max(0, static_cast<int>(files.size()) - 1));
        failed = false;
        publish();
    }
    void toggle() {
        if (!opened) {
            start(track, 1);
        } else if (playing) {
            playing = !mci(L"pause " + alias);
            publish();
        } else {
            playing = mci(L"resume " + alias) || mci(L"play " + alias);
            publish();
        }
    }
    // A song that has ended starts the next one (AUTOPLAY), or leaves the
    // phone paused at its start. The mode as a number: its word (`status ...
    // mode`) is the display language's.
    void poll() {
        if (!opened || !playing || !device)
            return;
        MCI_STATUS_PARMS mode{};
        mode.dwItem = MCI_STATUS_MODE;
        if (mciSendCommandW(device, MCI_STATUS, MCI_STATUS_ITEM, reinterpret_cast<DWORD_PTR>(&mode))) {
            close();
            publish();
            return;
        }
        if (mode.dwReturn != MCI_MODE_STOP)
            return;
        if (autoplay && files.size() > 1) {
            start(track + 1, 1);
        } else {
            close();
            publish();
        }
    }
    void readSettings() {
        settings = settingsFile();
        if (settings.empty())
            return;
        const int savedVolume =
            static_cast<int>(GetPrivateProfileIntW(L"phone", L"volume", phoneVolume, settings.c_str()));
        const bool savedAutoplay = GetPrivateProfileIntW(L"phone", L"autoplay", 1, settings.c_str()) != 0;
        std::lock_guard guard(lock);
        if (!volumeSet)
            shown.volume = std::clamp(savedVolume, 0, 100);
        if (!autoplaySet)
            shown.autoplay = savedAutoplay;
        volume = shown.volume;
        autoplay = shown.autoplay;
    }
    // The caller's volume and AUTOPLAY: into effect, and kept.
    void takeSettings() {
        {
            std::lock_guard guard(lock);
            volume = shown.volume;
            autoplay = shown.autoplay;
        }
        applyVolume();
        if (!settings.empty()) {
            const auto percent = std::to_wstring(volume);
            WritePrivateProfileStringW(L"phone", L"volume", percent.c_str(), settings.c_str());
            WritePrivateProfileStringW(L"phone", L"autoplay", autoplay ? L"1" : L"0", settings.c_str());
        }
    }
    void run() {
        // MCI's mpegvideo player is DirectShow's, which needs COM: a
        // single-threaded apartment, with its messages pumped below. (In the
        // multithreaded one every open fails.)
        const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        readSettings();
        ULONGLONG pollAt{};
        while (!quit) {
            const ULONGLONG now = GetTickCount64();
            const DWORD wait = !playing ? INFINITE : pollAt > now ? static_cast<DWORD>(pollAt - now) : 0;
            MsgWaitForMultipleObjects(1, &wake, FALSE, wait, QS_ALLINPUT);
            MSG message;
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            std::deque<Command> todo;
            {
                std::lock_guard guard(lock);
                todo.swap(commands);
            }
            for (const auto c : todo) {
                if (quit)
                    break;
                switch (c) {
                case Command::load:
                    load();
                    break;
                case Command::toggle:
                    toggle();
                    break;
                case Command::next:
                    start(track + 1, 1);
                    break;
                case Command::previous:
                    start(track - 1, -1);
                    break;
                case Command::settings:
                    takeSettings();
                    break;
                }
            }
            // Twice a second while one plays: has it ended?
            if (playing && GetTickCount64() >= pollAt) {
                pollAt = GetTickCount64() + 500;
                poll();
            }
        }
        close();
        if (SUCCEEDED(com))
            CoUninitialize();
    }
};

PhoneMusic::PhoneMusic() : player_(std::make_unique<Player>()) {}

PhoneMusic::~PhoneMusic() = default;

PhoneMusic::Status PhoneMusic::status() const {
    std::lock_guard guard(player_->lock);
    return player_->shown;
}

void PhoneMusic::load() {
    player_->send(Player::Command::load);
}

void PhoneMusic::toggle() {
    player_->send(Player::Command::toggle);
}

void PhoneMusic::next() {
    player_->send(Player::Command::next);
}

void PhoneMusic::previous() {
    player_->send(Player::Command::previous);
}

void PhoneMusic::setVolume(int percent) {
    {
        std::lock_guard guard(player_->lock);
        player_->shown.volume = std::clamp(percent, 0, 100);
        player_->volumeSet = true;
    }
    player_->send(Player::Command::settings);
}

void PhoneMusic::setAutoplay(bool on) {
    {
        std::lock_guard guard(player_->lock);
        player_->shown.autoplay = on;
        player_->autoplaySet = true;
    }
    player_->send(Player::Command::settings);
}

namespace {
std::wstring installMusicFolder() {
    const std::wstring folder = spidyHome() + L"\\Music";
    CreateDirectoryW(folder.c_str(), nullptr);
    return folder;
}
// The README in the songs' folder: written when missing, or rewritten when it is an older one of ours.
void writeMusicReadme(const std::wstring& folder) {
    const std::wstring path = folder + L"\\README.txt"; // vehicle074::musicReadmeName
    const char* text = vehicle074::musicReadme;
    const DWORD size = static_cast<DWORD>(std::strlen(text));
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        char old[4096]{};
        DWORD got = 0;
        const BOOL read = ReadFile(file, old, sizeof old - 1, &got, nullptr);
        CloseHandle(file);
        const std::string_view was(old, got);
        if (!read || (was.size() == size && was == text) || !was.starts_with("HERO VR ") || !was.ends_with("Made by Agent Mulder.\r\n"))
            return; // the same, or not ours: left alone
    }
    file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;
    DWORD wrote = 0;
    WriteFile(file, text, size, &wrote, nullptr);
    CloseHandle(file);
}
bool sameFolder(const std::wstring& a, const std::wstring& b) {
    auto trim = [](std::wstring s) {
        while (s.size() > 3 && (s.back() == L'\\' || s.back() == L'/'))
            s.pop_back();
        return s;
    };
    return _wcsicmp(trim(a).c_str(), trim(b).c_str()) == 0;
}
void findSongs(const std::wstring& folder, int depth, std::vector<std::pair<std::wstring, uint64_t>>& out,
               std::unordered_set<std::string>& seen, size_t& visited, const std::atomic<bool>* stop) {
    WIN32_FIND_DATAW found{};
    const HANDLE find = FindFirstFileExW((folder + L"\\*").c_str(), FindExInfoBasic, &found, FindExSearchNameMatch,
                                         nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (find == INVALID_HANDLE_VALUE)
        return;
    std::vector<std::wstring> below;
    do {
        if (stop && stop->load(std::memory_order_relaxed))
            break;
        const std::wstring_view name(found.cFileName);
        if (name == L"." || name == L"..")
            continue;
        // Hidden or system (desktop.ini, Thumbs.db, $RECYCLE.BIN), and macOS's ._ twins, which nothing opens.
        if ((found.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) || name.starts_with(L"._"))
            continue;
        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // Sub-folders, but no junction or link (a loop, or a whole other drive).
            if (depth < music::folderDepth && !(found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
                below.push_back(folder + L"\\" + found.cFileName);
            continue;
        }
        const uint64_t bytes = (static_cast<uint64_t>(found.nFileSizeHigh) << 32) | found.nFileSizeLow;
        const std::string name8 = utf8(found.cFileName);
        if (music::playable(name8, bytes) && seen.insert(music::songKey(name8, bytes)).second)
            out.emplace_back(folder + L"\\" + found.cFileName, bytes);
    } while (out.size() < music::songCap && ++visited < 200000 && FindNextFileW(find, &found));
    FindClose(find);
    std::sort(below.begin(), below.end(), [](const std::wstring& a, const std::wstring& b) {
        return _wcsicmp(a.c_str(), b.c_str()) < 0;
    });
    for (const auto& sub : below) {
        if (out.size() >= music::songCap || visited >= 200000 || (stop && stop->load(std::memory_order_relaxed)))
            break;
        findSongs(sub, depth + 1, out, seen, visited, stop);
    }
}
} // namespace

std::wstring phoneMusicFolder() {
    std::wstring folder;
    PWSTR docs{};
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_CREATE, nullptr, &docs)) && docs && *docs)
        folder = std::wstring(docs) + L"\\" + vehicle074::musicFolderUnderDocuments;
    CoTaskMemFree(docs);
    if (!folder.empty()) {
        SHCreateDirectoryExW(nullptr, folder.c_str(), nullptr); // made with HERO VR, or already there
        const DWORD is = GetFileAttributesW(folder.c_str());
        if (is != INVALID_FILE_ATTRIBUTES && (is & FILE_ATTRIBUTE_DIRECTORY)) {
            writeMusicReadme(folder);
            return folder;
        }
    }
    return installMusicFolder(); // no Documents folder (or it cannot be written): the install's own
}

std::vector<std::wstring> musicFolders() {
    std::vector<std::wstring> folders{phoneMusicFolder()};
    const std::wstring install = installMusicFolder();
    if (!sameFolder(install, folders.front()))
        folders.push_back(install);
    return folders;
}

std::vector<std::pair<std::wstring, uint64_t>> musicFiles(const std::vector<std::wstring>& folders,
                                                         const std::atomic<bool>* stop) {
    std::vector<std::pair<std::wstring, uint64_t>> out;
    std::unordered_set<std::string> seen;
    size_t visited = 0;
    for (const auto& folder : folders)
        if (!folder.empty() && out.size() < music::songCap)
            findSongs(folder, 0, out, seen, visited, stop);
    return out;
}

std::wstring spidyHome() {
    HMODULE self{};
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&spidyHome), &self);
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(self, path, MAX_PATH);
    std::wstring folder = path, root;
    while (root.empty()) {
        const auto slash = folder.find_last_of(L"\\/");
        if (slash == std::wstring::npos)
            break;
        folder.erase(slash);
        for (const wchar_t* launcher : {L"\\Launch Spidy VR.cmd", L"\\Spidy Launcher.exe"})
            if (GetFileAttributesW((folder + launcher).c_str()) != INVALID_FILE_ATTRIBUTES)
                root = folder;
    }
    return root.empty() ? std::wstring(path).substr(0, std::wstring(path).find_last_of(L"\\/")) : root;
}
} // namespace spidy
