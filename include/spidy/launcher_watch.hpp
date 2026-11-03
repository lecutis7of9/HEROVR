#pragma once
// 0.0.76 LAUNCHER WATCH: when a late keep-alive may end VR.
//
// The launcher (tools/run_game_vr.py) renews SpidyXrKeepAlive about every second, each a remote
// thread in the game. At the menu (2026-11-03 01:55 run) Windows held the launcher's remote calls
// up for 5.2 s while the game loaded; the 5 s keep-alive lapsed and VR ended by itself as if the
// launcher had closed, so the game went back to the flat screen. A late keep-alive alone no
// longer ends VR: with the launcher's process id (SpidyXrLauncher) VR ends only once that process
// has exited; without one (an older launcher) only after noAnswerMs with no keep-alive at all.
#include <cstdint>

namespace spidy::launcher_watch {

// Each keep-alive gives this long (unchanged).
constexpr uint64_t keepAliveMs = 5000;
// The first one, at the start.
constexpr uint64_t firstKeepAliveMs = 10000;
// No process id (an older launcher): this long past the deadline with no keep-alive ends VR.
constexpr uint64_t noAnswerMs = 60000;

enum Verdict : uint32_t { keepRunning = 0, launcherClosed = 1, launcherSilent = 2 };

// now / deadline: GetTickCount64 and the keep-alive's deadline. known: the launcher gave its
// process id and it could be opened; exited: that process has ended (its handle is signalled).
inline Verdict decide(uint64_t now, uint64_t deadline, bool known, bool exited) {
    if (known && exited)
        return launcherClosed;
    if (now < deadline || known)
        return keepRunning;
    return now - deadline >= noAnswerMs ? launcherSilent : keepRunning;
}

// How late the keep-alive is (0 when it is not).
inline uint64_t lateMs(uint64_t now, uint64_t deadline) { return now > deadline ? now - deadline : 0; }

} // namespace spidy::launcher_watch
