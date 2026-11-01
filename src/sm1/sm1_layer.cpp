// HERO VR 0.0.76 SM1: the add-on's OpenXR API layer (herovr_sm1_layer.dll). Made by Agent Mulder.
// Mulder's Heroes add-on for iilyn's Spider-Man VR mod (SpidyVR by Ilya Mezerowsky, GPL-3.0,
// https://github.com/IlyaMez/SpidyVR). See include/spidy/sm1_addon.hpp for what it does and does not touch.
//
// Addresses: only two parts read game memory, both READ ONLY (the research scan and the enemy list). Everything else here goes through OpenXR (the loader hands the layer his calls) and through his own
// exported functions, found by name with GetProcAddress (CONFIRMED in his 0.7.0 sources):
//   SpidyXrData (data)  spidy_stereo_probe.dll    XrData v43, 1312 bytes (seqlock `sequence`)
//   SpidyStrikeSample   spidy_ray_bridge.dll      Strikes v3, 328 bytes (cueTarget, cuePoint)
//   SpidyPunchTest      spidy_ray_bridge.dll      Test v1, 80 bytes (blocks up to 1 s: called on a worker thread)
//   SpidyMotionSubmit   spidy_movement_bridge.dll Command v1, 48 bytes (serial must rise; 2001 bad, 2002 off, 2003 old)
//   SpidyMotionSample   spidy_movement_bridge.dll Data v4, 176 bytes (serial at +48, grounded at +168)
// The research scan (sm1_scan.cpp, live.txt sm1scan) is the one part that reads game addresses: CANDIDATE RVAs from
// his 0.7.0 sources, READ ONLY (VirtualQuery + SEH copies, no writes, no game or Spidy calls), on its own
// lowest-priority thread, into %APPDATA%\HERO VR SM1\scan. Nothing else in the layer uses what it finds.
// The enemy list (sm1_targets.cpp) reads the same CANDIDATE registry/RTTI RVAs, READ ONLY, on its own lowest-priority
// thread every 500 ms and only while a hero with powers is picked. It starts only on Spider-Man.exe 4.630.0.0 and only
// after the two health vtable RVAs check out by their RTTI names at run time; otherwise powers keep using his cueTarget.
// Its records go to SpidyPunchTest (his export) as victims: every Bot with health that is no CivilianBot, ally or neutral.
// Every failure is logged to %APPDATA%\HERO VR SM1\layer.log and turns off only its own feature.
#include "spidy/sm1_addon.hpp"
#include "spidy/sm1_combat.hpp"
#include "spidy/sm1_grapple.hpp"
#include "spidy/sm1_holo.hpp"
#include "spidy/sm1_scan.hpp"
#include "spidy/wrist_hud.hpp"

#include <windows.h>
#include <d3d12.h>
#include <objbase.h>
#include <unknwn.h>
#include <wincodec.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_loader_negotiation.h>

#include <MinHook.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#if defined(_MSC_VER)
#define SM1_TRY __try
#define SM1_EXCEPT __except (EXCEPTION_EXECUTE_HANDLER)
#else
// mingw (the syntax check) has no __try: the guarded block runs plainly.
#define SM1_TRY if (true)
#define SM1_EXCEPT else
#endif

namespace {
using namespace spidy;
using namespace spidy::sm1;
namespace holo = spidy::sm1::holo;

// --- His structures (copied from his 0.7.0 headers; sizes checked) ------------------------------------------------
namespace his {
struct XrData { // src/game_xr.cpp, only the leading part the layer reads
    uint32_t magic, version, bytes, status;
    int64_t sequence;
    uint64_t frames, tracked, submitted, dropped, leftHands, rightHands, serial, generation;
    uint32_t nativeKeys, error;
    float head[16], hands[2][16];
};
static_assert(offsetof(XrData, head) == 96 && offsetof(XrData, hands) == 160);
constexpr size_t xrDataBytes = 1312, playerRecordOffset = 608;
static_assert(xrDataBytes == scan::xrDataBytes && playerRecordOffset == scan::xrPlayerRecordAt);
struct Command { // include/spidy/native_movement.hpp
    uint32_t magic = 0x534d564d, version = 1, bytes = sizeof(Command), enabled{};
    uint64_t serial{};
    uint32_t leaseMs{}, reserved{};
    Vec3 velocity{};
    uint32_t reserved2{};
};
static_assert(sizeof(Command) == 48);
struct MotionData { // native_movement::Data v4
    uint32_t magic, version, bytes, status;
    int64_t sequence;
    uint64_t qpc, steps, controlled, serial, gravityCorrections;
    Vec3 position, requested, achievedVelocity;
    float dt;
    uint32_t moverFlags, collisionFlags, thread, error;
    uint64_t airEvents, airOverrides, airState;
    Vec3 airVelocity;
    float airVertical, airGravity, airDt;
    uint32_t grounded, contact;
};
static_assert(sizeof(MotionData) == 176 && offsetof(MotionData, serial) == 48);
struct Strikes { // include/spidy/game_punch.hpp
    uint32_t magic, version, bytes, status;
    int64_t sequence;
    uint64_t opened, landed, missed, held;
    uint64_t blows, heavies, launches, juggles, slams, finishers;
    uint64_t flown, unflown;
    uint32_t phase, hand, flow, enemies;
    uint64_t target;
    Vec3 destination;
    float distance, seconds;
    Vec3 lastPoint, lastDirection;
    float lastStrength, lastDamage;
    uint32_t lastKind, lastCount, lastHand, lastStrike, lastCalled;
    uint64_t calls, called, missedCalls;
    uint64_t cueTarget;
    Vec3 cuePoint;
    uint32_t cueWay, cueFinisher;
    float cueLeft;
    uint64_t locked, knockouts;
    Vec3 knockoutPoint;
    uint32_t cueLocked;
    uint64_t swung, strikeCalled, passedOver;
};
static_assert(sizeof(Strikes) == 328);
// The research scan reads the same exports by offset (sm1_scan.hpp): kept equal to these copies.
static_assert(offsetof(Strikes, landed) == scan::strikeLandedAt && offsetof(Strikes, enemies) == scan::strikeEnemiesAt &&
              offsetof(Strikes, target) == scan::strikeTargetAt && offsetof(Strikes, cueTarget) == scan::strikeCueTargetAt &&
              offsetof(Strikes, cuePoint) == scan::strikeCuePointAt && offsetof(Strikes, lastPoint) == scan::strikeLastPointAt &&
              offsetof(Strikes, lastDamage) == scan::strikeLastDamageAt &&
              offsetof(Strikes, knockouts) == scan::strikeKnockoutsAt && sizeof(Strikes) == scan::strikeBytes);
static_assert(offsetof(MotionData, position) == scan::motionPositionAt &&
              offsetof(MotionData, grounded) == scan::motionGroundedAt && sizeof(MotionData) == scan::motionDataBytes);
static_assert(offsetof(XrData, head) == scan::xrHeadAt && offsetof(XrData, hands) == scan::xrHandsAt);
struct Aim { // include/spidy/game_swing.hpp (AimKind: 0 none, 1 anchor, 2 air, 3 blocked, 4 prop, ...)
    uint32_t kind;
    float radius;
    Vec3 point, normal;
};
struct AimData { // SpidyAimSample (spidy_ray_bridge.dll): version 2, 128 bytes; status 2 while previews come
    uint32_t magic, version, bytes, status;
    int64_t sequence;
    uint64_t serial;
    Aim hands[2];
    Aim gaze;
};
static_assert(sizeof(Aim) == 32 && sizeof(AimData) == 128 && offsetof(AimData, hands) == 32);
struct Test {
    uint32_t magic = 0x53505554, version = 1, bytes = sizeof(Test), type = 1;
    uint64_t victim{}, damager{};
    Vec3 point{}, direction{};
    float amount{}, knockbackAmount = -1, impulse = -1;
    int32_t knockback = -1;
    uint32_t hash{}, reserved{};
};
static_assert(sizeof(Test) == 80);
using Call = DWORD(WINAPI*)(void*);
} // namespace his

// --- Log ------------------------------------------------------------------------------------------------------------
std::wstring folder() {
    wchar_t buf[MAX_PATH]{};
    const DWORD n = GetEnvironmentVariableW(L"APPDATA", buf, MAX_PATH);
    if (!n || n >= MAX_PATH)
        return L"";
    std::wstring dir = std::wstring(buf) + L"\\HERO VR SM1";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}
SRWLOCK logLock = SRWLOCK_INIT;
void logLine(const char* fmt, ...) {
    char text[768];
    va_list a;
    va_start(a, fmt);
    vsnprintf(text, sizeof(text), fmt, a);
    va_end(a);
    SYSTEMTIME t;
    GetLocalTime(&t);
    char line[900];
    const int n = snprintf(line, sizeof(line), "%02u:%02u:%02u.%03u %s\r\n", t.wHour, t.wMinute, t.wSecond,
                           t.wMilliseconds, text);
    AcquireSRWLockExclusive(&logLock);
    static std::wstring path = folder().empty() ? L"" : folder() + L"\\layer.log";
    if (!path.empty() && n > 0) {
        HANDLE f = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f != INVALID_HANDLE_VALUE) {
            DWORD w = 0;
            WriteFile(f, line, static_cast<DWORD>(std::min<int>(n, sizeof(line) - 1)), &w, nullptr);
            CloseHandle(f);
        }
    }
    ReleaseSRWLockExclusive(&logLock);
}

// One feature that can turn itself off (and stays off for the session) without touching the others.
struct FeatureState {
    explicit FeatureState(const char* n) : name(n) {}
    const char* name;
    std::atomic<bool> on{true};
    std::string reason;
    void turnOff(const char* why) {
        if (on.exchange(false)) {
            reason = why;
            logLine("%s OFF: %s", name, why);
        }
    }
};
FeatureState fInput{"INPUT"}, fMenu{"WRIST MENU"}, fDraw{"DRAWING"}, fPowers{"POWERS"}, fFlight{"FLIGHT"};

// --- Files ----------------------------------------------------------------------------------------------------------
bool readFile(const std::wstring& path, std::string& out, FILETIME* stamp = nullptr) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return false;
    LARGE_INTEGER size{};
    bool ok = GetFileSizeEx(f, &size) && size.QuadPart >= 0 && size.QuadPart < (1 << 20);
    if (ok) {
        out.assign(static_cast<size_t>(size.QuadPart), '\0');
        DWORD got = 0;
        ok = out.empty() || (ReadFile(f, out.data(), static_cast<DWORD>(out.size()), &got, nullptr) && got == out.size());
    }
    if (ok && stamp)
        GetFileTime(f, nullptr, nullptr, stamp);
    CloseHandle(f);
    return ok;
}
bool writeFile(const std::wstring& path, const std::string& text) {
    const std::wstring temp = path + L".tmp";
    HANDLE f = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return false;
    DWORD w = 0;
    const bool ok = WriteFile(f, text.data(), static_cast<DWORD>(text.size()), &w, nullptr) && w == text.size();
    CloseHandle(f);
    return ok && MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
}

// --- Settings: addon.ini (the launcher's switches and the menu's picks) and live.txt (the add-on's own switches) --
struct Settings {
    SRWLOCK lock = SRWLOCK_INIT;
    Config config;
    Live live;
    FILETIME liveStamp{};
    ULONGLONG nextLiveCheck = 0;
} settings;
std::wstring iniPath() {
    const auto d = folder();
    return d.empty() ? L"" : d + L"\\addon.ini";
}
std::wstring livePath() {
    const auto d = folder();
    return d.empty() ? L"" : d + L"\\live.txt";
}
void loadSettings() {
    std::string text;
    Config c;
    if (readFile(iniPath(), text))
        c = parseConfig(text);
    Live l;
    FILETIME stamp{};
    if (readFile(livePath(), text, &stamp)) {
        l = parseLive(text);
        // A switch added by a later build (sm1scan in 0.0.76): its line and subtext appended, the player's values kept.
        if (const std::string add = missingLive(text); !add.empty()) {
            const std::string joined = text + (text.empty() || text.back() == '\n' ? "" : "\n") + add;
            if (writeFile(livePath(), joined))
                readFile(livePath(), text, &stamp);
            else
                logLine("live.txt: the new switches could not be added (their defaults apply)");
        }
    } else if (!livePath().empty() && !writeFile(livePath(), liveText(l)))
        logLine("live.txt could not be written (the defaults stay)");
    AcquireSRWLockExclusive(&settings.lock);
    settings.config = c;
    settings.live = l;
    settings.liveStamp = stamp;
    ReleaseSRWLockExclusive(&settings.lock);
    logLine("settings: heroes=%d wrist_menu=%d flight=%d hero=%u powers=%d hero_webs=%d beams=%d damage=%d menu=%d sm1scan=%d",
            c.heroes, c.wristMenu, c.flight, c.hero, l.powers, l.heroWebs, l.beams, l.damage, l.menuSize, l.scan);
}
// Every 2 s: live.txt again when its time stamp moved.
void refreshLive() {
    const ULONGLONG now = GetTickCount64();
    if (now < settings.nextLiveCheck)
        return;
    settings.nextLiveCheck = now + 2000;
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if (!GetFileAttributesExW(livePath().c_str(), GetFileExInfoStandard, &a))
        return;
    if (CompareFileTime(&a.ftLastWriteTime, &settings.liveStamp) == 0)
        return;
    std::string text;
    FILETIME stamp{};
    if (!readFile(livePath(), text, &stamp))
        return;
    const Live l = parseLive(text);
    AcquireSRWLockExclusive(&settings.lock);
    settings.live = l;
    settings.liveStamp = stamp;
    ReleaseSRWLockExclusive(&settings.lock);
    logLine("live.txt: powers=%d hero_webs=%d beams=%d damage=%d menu=%d sm1scan=%d", l.powers, l.heroWebs, l.beams,
            l.damage, l.menuSize, l.scan);
}
// The menu's picks: addon.ini is read again first so the launcher's switches stay as it wrote them.
void saveConfig(uint32_t hero, const uint32_t* outfit) {
    std::string text;
    Config c;
    if (readFile(iniPath(), text))
        c = parseConfig(text);
    c.hero = hero;
    for (uint32_t i = 0; i < heroCount; ++i)
        c.outfit[i] = outfit[i];
    if (!writeFile(iniPath(), configText(c)))
        logLine("addon.ini could not be written (the pick holds until the game closes)");
}

// --- His exports -----------------------------------------------------------------------------------------------------
const his::XrData* xrData() {
    static const his::XrData* data = nullptr;
    if (!data)
        if (HMODULE m = GetModuleHandleA(spidyVrModule))
            data = reinterpret_cast<const his::XrData*>(GetProcAddress(m, "SpidyXrData"));
    return data;
}
struct GameView {
    bool ok = false;
    Vec3 headPos, headRight, headDown, headForward;
    Vec3 handPos[2], handForward[2];
    uint64_t player = 0;
};
// A consistent copy of his XrData (seqlock: an even sequence that did not move while it was copied).
GameView gameView() {
    GameView v;
    const his::XrData* d = xrData();
    if (!d)
        return v;
    alignas(8) unsigned char copy[his::xrDataBytes];
    bool got = false;
    SM1_TRY {
        if (d->magic == 0x53585244 && d->version >= 43 && d->bytes >= his::xrDataBytes) {
            for (int tries = 0; tries < 4 && !got; ++tries) {
                const int64_t before = InterlockedCompareExchange64(const_cast<volatile LONG64*>(&d->sequence), 0, 0);
                if (before & 1)
                    continue;
                std::memcpy(copy, d, sizeof(copy));
                MemoryBarrier();
                got = InterlockedCompareExchange64(const_cast<volatile LONG64*>(&d->sequence), 0, 0) == before;
            }
        }
    }
    SM1_EXCEPT {
        got = false;
    }
    if (!got)
        return v;
    his::XrData x;
    std::memcpy(&x, copy, sizeof(x));
    std::memcpy(&v.player, copy + his::playerRecordOffset, sizeof(v.player));
    const float* h = x.head;
    v.headRight = {h[0], h[1], h[2]};
    v.headDown = {h[4], h[5], h[6]};
    v.headForward = {h[8], h[9], h[10]};
    v.headPos = {h[12], h[13], h[14]};
    for (int i = 0; i < 2; ++i) {
        const float* m = x.hands[i];
        v.handForward[i] = xrDataForward(m); // +row 2: where the hand points (not -row 2, which is behind it)
        v.handPos[i] = {m[12], m[13], m[14]};
    }
    v.ok = finite(v.headPos) && length(v.headForward) > .5f;
    return v;
}
his::Call strikeSample = nullptr, punchTest = nullptr;
bool rayReady() {
    if (strikeSample && punchTest)
        return true;
    if (HMODULE m = GetModuleHandleA(spidyRayModule)) {
        strikeSample = reinterpret_cast<his::Call>(GetProcAddress(m, "SpidyStrikeSample"));
        punchTest = reinterpret_cast<his::Call>(GetProcAddress(m, "SpidyPunchTest"));
    }
    return strikeSample && punchTest;
}
// His per-hand web previews (SpidyAimSample: what a grip press would hit; sampling keeps them coming for 250 ms).
his::Call aimSample = nullptr;
bool sampleAims(his::AimData& d) {
    if (!aimSample)
        if (HMODULE m = GetModuleHandleA(spidyRayModule))
            aimSample = reinterpret_cast<his::Call>(GetProcAddress(m, "SpidyAimSample"));
    if (!aimSample)
        return false;
    DWORD r = 9999;
    SM1_TRY {
        r = aimSample(&d);
    }
    SM1_EXCEPT {
        r = 9999;
    }
    return r == 0 && d.magic == 0x5357414d && d.version >= 1 && d.bytes >= 96 && d.status == 2;
}

// --- The hand-over on his two movement exports (MinHook on the exported functions) ---------------------------------
SRWLOCK motionLock = SRWLOCK_INIT;
Handover handover;
his::Call submitOriginal = nullptr, sampleOriginal = nullptr;
std::atomic<bool> motionReady{false}, motionThreadStarted{false};
DWORD WINAPI submitDetour(void* input) {
    his::Command c;
    bool copied = false;
    SM1_TRY {
        if (input) {
            std::memcpy(&c, input, sizeof(c));
            copied = true;
        }
    }
    SM1_EXCEPT {
        copied = false;
    }
    if (!copied || c.magic != 0x534d564d || c.bytes != sizeof(his::Command) || !c.serial)
        return submitOriginal(input); // not a command this layer knows: his module answers it
    AcquireSRWLockExclusive(&motionLock);
    const uint64_t serial = handover.his(c.serial);
    DWORD r = 0;
    if (serial) {
        c.serial = serial;
        r = submitOriginal(&c);
    }
    ReleaseSRWLockExclusive(&motionLock);
    return r; // held back while a hero flies: he is told it went through
}
DWORD WINAPI sampleDetour(void* out) {
    const DWORD r = sampleOriginal(out);
    if (r || !out)
        return r;
    SM1_TRY {
        auto* d = static_cast<his::MotionData*>(out);
        if (d->magic == 0x534d5644 && d->bytes == sizeof(his::MotionData)) {
            AcquireSRWLockShared(&motionLock);
            d->serial = handover.sampleSerial(d->serial);
            ReleaseSRWLockShared(&motionLock);
        }
    }
    SM1_EXCEPT {
    }
    return r;
}
DWORD WINAPI installMotionHooks(void*) {
    HMODULE m = nullptr;
    for (int i = 0; i < 600 && !m; ++i) { // up to 300 s: his session modules load before his VR starts
        m = GetModuleHandleA(spidyMovementModule);
        if (!m)
            Sleep(500);
    }
    if (!m) {
        fFlight.turnOff("spidy_movement_bridge.dll is not loaded");
        return 0;
    }
    void* submit = reinterpret_cast<void*>(GetProcAddress(m, "SpidyMotionSubmit"));
    void* sample = reinterpret_cast<void*>(GetProcAddress(m, "SpidyMotionSample"));
    if (!submit || !sample) {
        fFlight.turnOff("SpidyMotionSubmit / SpidyMotionSample exports missing");
        return 0;
    }
    MH_STATUS s = MH_Initialize();
    if (s != MH_OK && s != MH_ERROR_ALREADY_INITIALIZED) {
        logLine("MH_Initialize %d", static_cast<int>(s));
        fFlight.turnOff("MinHook did not start");
        return 0;
    }
    if ((s = MH_CreateHook(submit, reinterpret_cast<void*>(&submitDetour), reinterpret_cast<void**>(&submitOriginal))) !=
            MH_OK ||
        (s = MH_CreateHook(sample, reinterpret_cast<void*>(&sampleDetour), reinterpret_cast<void**>(&sampleOriginal))) !=
            MH_OK) {
        logLine("MH_CreateHook %d", static_cast<int>(s));
        MH_RemoveHook(submit);
        MH_RemoveHook(sample);
        fFlight.turnOff("the movement hand-over could not hook his exports");
        return 0;
    }
    if (MH_QueueEnableHook(submit) != MH_OK || MH_QueueEnableHook(sample) != MH_OK ||
        (s = MH_ApplyQueued()) != MH_OK) {
        logLine("MH_ApplyQueued %d", static_cast<int>(s));
        MH_RemoveHook(submit);
        MH_RemoveHook(sample);
        fFlight.turnOff("the movement hand-over could not switch on");
        return 0;
    }
    motionReady = true;
    logLine("movement hand-over on SpidyMotionSubmit and SpidyMotionSample (exports, CONFIRMED by name)");
    return 0;
}
// Ours: one command through his original export, in order with his.
DWORD submitOurs(bool enabled, Vec3 velocity) {
    his::Command c;
    c.enabled = enabled ? 1 : 0;
    c.leaseMs = enabled ? 120 : 0;
    c.velocity = enabled ? velocity : Vec3{};
    AcquireSRWLockExclusive(&motionLock);
    c.serial = handover.ours();
    DWORD r = submitOriginal(&c);
    if (r == 2003) {
        handover.refused();
        c.serial = handover.ours();
        r = submitOriginal(&c);
    }
    ReleaseSRWLockExclusive(&motionLock);
    return r;
}
bool sampleOurs(his::MotionData& d) {
    return sampleOriginal && sampleOriginal(&d) == 0 && d.magic == 0x534d5644 && d.bytes == sizeof(d);
}

// --- The blows: SpidyPunchTest waits for the game's next physics step, so they run on their own thread, in order ------
// Up to 8 wait (a chain of lightning, a shield's ricochets, both claws); a ninth pushes out the oldest.
SRWLOCK punchLock = SRWLOCK_INIT;
CONDITION_VARIABLE punchWake = CONDITION_VARIABLE_INIT;
constexpr int punchSlots = 8;
his::Test punchQueue[punchSlots];
int punchHead = 0, punchCount = 0;
std::atomic<DWORD> punchResult{0};
std::atomic<uint64_t> punchHits{0}, punchDropped{0};
std::atomic<bool> punchRun{true}; // never cleared: the worker lives as long as the game
DWORD callPunch(his::Test* t) {
    DWORD r = 9999;
    SM1_TRY {
        r = punchTest(t);
    }
    SM1_EXCEPT {
        r = 9999;
    }
    return r;
}
DWORD WINAPI punchWorker(void*) {
    ULONGLONG lastSaid = 0;
    while (punchRun.load()) {
        AcquireSRWLockExclusive(&punchLock);
        while (!punchCount)
            SleepConditionVariableSRW(&punchWake, &punchLock, INFINITE, 0);
        his::Test t = punchQueue[punchHead];
        punchHead = (punchHead + 1) % punchSlots;
        --punchCount;
        ReleaseSRWLockExclusive(&punchLock);
        const DWORD r = callPunch(&t);
        punchResult = r;
        if (r == 0)
            ++punchHits;
        else if (r == 9502 || r == 9999)
            fPowers.turnOff(r == 9502 ? "SpidyPunchTest refused the blow (9502)" : "SpidyPunchTest faulted");
        else if (GetTickCount64() - lastSaid > 2000) { // 9504 no punch module, 9505 no ticket, 9506 paused, 9507 dropped
            lastSaid = GetTickCount64();
            logLine("SpidyPunchTest %lu", static_cast<unsigned long>(r));
        }
    }
    return 0; // MSVC C4716: a thread function with __try needs a return it can see
}
void queuePunch(const his::Test& t) {
    static std::atomic<bool> started{false};
    if (!started.exchange(true)) {
        HANDLE h = CreateThread(nullptr, 0, punchWorker, nullptr, 0, nullptr);
        if (!h) {
            fPowers.turnOff("the power's worker thread did not start");
            return;
        }
        CloseHandle(h);
    }
    AcquireSRWLockExclusive(&punchLock);
    if (punchCount == punchSlots) {
        punchHead = (punchHead + 1) % punchSlots;
        --punchCount;
        ++punchDropped;
    }
    punchQueue[(punchHead + punchCount) % punchSlots] = t;
    ++punchCount;
    ReleaseSRWLockExclusive(&punchLock);
    WakeConditionVariable(&punchWake);
}
// One blow on an actor (SpidyPunchTest's victim and damager are actor records).
void blow(uint64_t victim, uint64_t player, Vec3 point, Vec3 from, float amount, int32_t knockback) {
    his::Test t;
    t.victim = victim;
    t.damager = player;
    t.point = point;
    t.direction = normalized(point - from);
    if (!(length(t.direction) > .5f))
        t.direction = {0, 0, 1};
    t.amount = amount;
    t.knockback = knockback;
    queuePunch(t);
}
// The game's enemies (sm1_targets.cpp): every bot type with health that is no civilian, ally or neutral.
bool targetsUsable() {
    static int ok = -1;
    if (ok < 0)
        ok = targets::start([](const char* line) { logLine("%s", line); }) ? 1 : 0;
    return ok == 1;
}

// --- The next layer's (or the runtime's) functions ------------------------------------------------------------------
struct Next {
    PFN_xrGetInstanceProcAddr getInstanceProcAddr = nullptr;
    PFN_xrDestroyInstance destroyInstance = nullptr;
    PFN_xrCreateSession createSession = nullptr;
    PFN_xrDestroySession destroySession = nullptr;
    PFN_xrSuggestInteractionProfileBindings suggestBindings = nullptr;
    PFN_xrSyncActions syncActions = nullptr;
    PFN_xrGetActionStateBoolean getBoolean = nullptr;
    PFN_xrGetActionStateFloat getFloat = nullptr;
    PFN_xrGetActionStateVector2f getVector2f = nullptr;
    PFN_xrWaitFrame waitFrame = nullptr;
    PFN_xrEndFrame endFrame = nullptr;
    PFN_xrPathToString pathToString = nullptr;
    PFN_xrStringToPath stringToPath = nullptr;
    PFN_xrCreateActionSpace createActionSpace = nullptr;
    PFN_xrLocateSpace locateSpace = nullptr;
    PFN_xrDestroySpace destroySpace = nullptr;
    PFN_xrGetSystemProperties getSystemProperties = nullptr;
    PFN_xrEnumerateSwapchainFormats enumerateFormats = nullptr;
    PFN_xrCreateSwapchain createSwapchain = nullptr;
    PFN_xrDestroySwapchain destroySwapchain = nullptr;
    PFN_xrEnumerateSwapchainImages enumerateImages = nullptr;
    PFN_xrAcquireSwapchainImage acquireImage = nullptr;
    PFN_xrWaitSwapchainImage waitImage = nullptr;
    PFN_xrReleaseSwapchainImage releaseImage = nullptr;
} next;
XrInstance instance = XR_NULL_HANDLE;
bool passthrough = true; // not Spider-Man.exe, or a function the layer needs is missing: everything goes straight on

// What his bindings say each of his actions is.
struct ActionInfo {
    Role role = Role::none;
    bool sides[2]{};
    int kind = 0; // 0 unknown, 1 float, 2 boolean (the trigger and grip read as either)
};
SRWLOCK actionLock = SRWLOCK_INIT;
std::map<XrAction, ActionInfo> actions;

struct Quad {
    XrSwapchain swapchain = XR_NULL_HANDLE;
    std::vector<ID3D12Resource*> images;
    int width = 0, height = 0;
    bool released = false; // an image went out at least once (only then can a layer show it)
};
struct Session {
    XrSession handle = XR_NULL_HANDLE;
    ID3D12Device* device = nullptr;
    ID3D12CommandQueue* queue = nullptr;
    XrPath hand[2]{};
    uint32_t maxLayers = 16;
    int64_t format = 0;
    bool bgra = false;
    XrSpace gripSpace[2]{}, aimSpace[2]{};
    bool spacesTried = false;
    // 0.0.76 SM1 HOLO: the SM2 build's hologram (holo::width x height), the PLAY AS note, the pointer's dot; the beam
    Quad holo, note, dot, beam;
    // the upload
    ID3D12CommandAllocator* allocator = nullptr;
    ID3D12GraphicsCommandList* list = nullptr;
    ID3D12Resource* upload = nullptr;
    UINT64 uploadBytes = 0;
    ID3D12Fence* fence = nullptr;
    UINT64 fenceValue = 0;
    HANDLE fenceEvent = nullptr;
} session;

// The add-on's state for one session (the frame thread only, except where atomic).
struct Play {
    LARGE_INTEGER lastSync{};
    WristMenu menu;
    // The SM2 build's hologram toggle, unchanged (its FOREARM mode, the SM2 default): the right hand on the
    // left wrist or forearm and a right-trigger click opens or closes the menu.
    spidy::wrist_hud::Toggle holo;
    uint32_t holoHands = 0;
    FlightState flight;
    bool flying = false;
    Mask mask;
    // raw inputs this sync (before any hiding)
    float trigger[2]{}, grip[2]{};
    XrVector2f stick[2]{};
    float faceLower[2]{}, faceUpper[2]{}; // 0.0.76 FLIGHT: A/X and B/Y, where his bindings name them
    bool faceBound = false;               // his bindings name the right A
    bool aOurs = false;                   // an A held in flight, until it is let go
    bool handOut[2]{};
    bool triggerWas[2]{};
    bool triggerOurs[2]{}; // a press hidden from his reads when it went down, until it is let go
    float cooldown = 0;
    // the beam on screen
    ULONGLONG beamUntil = 0;
    int beamHand = 1;
    Vec3 beamDirGame{};
    float beamLength = 0;
    float beamWidth = .06f;
    uint32_t beamColor = 0xffffffff;
    // CAPTAIN AMERICA's and DAREDEVIL's grapple (sm1_grapple.hpp)
    GrappleDriver grapple;
    bool grappling = false; // our velocity goes to his movement module
    std::string grappleState = "GRAPPLE: READY";
    std::string powerState = "READY";
    std::string suitNotice;    // the holo's suit line after a hero pick
    ULONGLONG suitNoticeUntil = 0;
    // 0.0.76 SM1 HOLO: the SM2 build's own hologram menu (wrist_hud::Menu, its POWERS page with the heroes' tiles)
    spidy::wrist_hud::Menu holoMenu;
    Pose holoPose;              // where it shows (his projection space, this frame)
    bool holoPlaced = false;
    float holoScale = 1;        // menu_size / 100
    spidy::wrist_hud::Pointer pointer; // the right hand's, on the hologram
    uint64_t holoAsked = 0;     // the picture asked of the painter (its vertices' key)
    bool holoShown = false;     // a picture of this opening is on the quad
    std::vector<Vertex> holoTranslucent, holoTextured;
    float holoClock = 0;
    // the PLAY AS note: in front of the eyes for a few seconds after a hero pick
    std::string noteTitle;
    std::vector<std::string> noteLines;
    ULONGLONG noteUntil = 0;
    uint64_t noteKey = 0, noteUploaded = 0;
    bool noteFresh = false;     // place it once, where the eyes look now
    Pose notePose;
    bool dotReady = false;
    bool holoArmed = false;     // the right trigger let go since the hologram opened
    ULONGLONG ignoredAt[2]{};   // the last "trigger did nothing" log line per hand (rate limit)
    uint64_t lastGameSerial = 0;
    XrTime displayTime = 0;
    // the last head pose in his projection space
    bool headKnown = false;
    Pose head;
} play;

// --- Hero suits: the holo's hint (no live switch: see features[] "holo suit switch") ---------------------------------
std::string gameFolder; // set in xrCreateApiLayerInstance from the host exe's path

// The launcher's state.json (our own file in the game's asset_archive\HeroVR; never SpidyVR's). Read only, small.
std::string readSuitState() {
    if (gameFolder.empty())
        return "";
    const std::string path = gameFolder + "asset_archive\\HeroVR\\state.json";
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f)
        return "";
    std::string s;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0 && s.size() < (1u << 20))
        s.append(buf, n);
    std::fclose(f);
    return s;
}

// Cached: the view asks every frame, the file is read at most every 5 seconds (and on every hero pick).
std::string suitState;
ULONGLONG suitStateAt = 0;
bool suitsInstalledFor(uint32_t hero, bool fresh = false) {
    const ULONGLONG now = GetTickCount64();
    if (fresh || suitStateAt == 0 || now - suitStateAt > 5000) {
        suitState = readSuitState();
        suitStateAt = now ? now : 1;
    }
    return suitInstalled(suitState, hero);
}

void pickSuit(uint32_t hero) {
    const bool installed = suitIdOf(hero) && suitsInstalledFor(hero, true);
    play.suitNotice = suitHint(hero, installed);
    play.suitNoticeUntil = GetTickCount64() + 10000;
    if (suitIdOf(hero))
        logLine("suit: %s %s; no confirmed suit-equip call in this game version, so no live switch: the holo asks the "
                "player to pick it in the game's Suits menu",
                suitIdOf(hero), installed ? "installed" : "NOT installed (launcher: Install suits)");
}

// --- Paths and bindings ------------------------------------------------------------------------------------------------
std::string pathText(XrPath p) {
    char buf[XR_MAX_PATH_LENGTH]{};
    uint32_t n = 0;
    if (!next.pathToString || next.pathToString(instance, p, sizeof(buf), &n, buf) != XR_SUCCESS)
        return "";
    return std::string(buf, n ? n - 1 : 0);
}
int sideOf(XrPath p) {
    if (p == XR_NULL_PATH)
        return -1;
    if (p == session.hand[0] && p)
        return 0;
    if (p == session.hand[1] && p)
        return 1;
    const std::string s = pathText(p);
    return s == "/user/hand/left" ? 0 : s == "/user/hand/right" ? 1 : -2;
}
XRAPI_ATTR XrResult XRAPI_CALL suggestBindings(XrInstance inst, const XrInteractionProfileSuggestedBinding* s) {
    const XrResult r = next.suggestBindings(inst, s);
    if (XR_SUCCEEDED(r) && s && fInput.on) {
        AcquireSRWLockExclusive(&actionLock);
        for (uint32_t i = 0; i < s->countSuggestedBindings; ++i) {
            const Binding b = classify(pathText(s->suggestedBindings[i].binding));
            if (b.role == Role::none)
                continue;
            ActionInfo& a = actions[s->suggestedBindings[i].action];
            if (a.role == Role::none)
                a.role = b.role;
            if (a.role == b.role)
                a.sides[b.side] = true;
        }
        ReleaseSRWLockExclusive(&actionLock);
        logLine("bindings for %s: %zu actions the add-on knows", pathText(s->interactionProfile).c_str(),
                actions.size());
    }
    return r;
}
std::vector<std::pair<XrAction, ActionInfo>> knownActions() {
    AcquireSRWLockShared(&actionLock);
    std::vector<std::pair<XrAction, ActionInfo>> v(actions.begin(), actions.end());
    ReleaseSRWLockShared(&actionLock);
    return v;
}

// The add-on reads its own copy of the inputs right after his xrSyncActions (straight from the runtime).
float readValue(XrAction action, ActionInfo& info, int side) {
    XrActionStateGetInfo g{XR_TYPE_ACTION_STATE_GET_INFO};
    g.action = action;
    g.subactionPath = session.hand[side];
    if (info.kind != 2) {
        XrActionStateFloat f{XR_TYPE_ACTION_STATE_FLOAT};
        const XrResult r = next.getFloat(session.handle, &g, &f);
        if (r == XR_SUCCESS) {
            info.kind = 1;
            return f.isActive ? f.currentState : 0.f;
        }
        if (r != XR_ERROR_ACTION_TYPE_MISMATCH)
            return 0;
    }
    XrActionStateBoolean b{XR_TYPE_ACTION_STATE_BOOLEAN};
    if (next.getBoolean(session.handle, &g, &b) == XR_SUCCESS) {
        info.kind = 2;
        return b.isActive && b.currentState ? 1.f : 0.f;
    }
    return 0;
}
void readInputs() {
    for (int i = 0; i < 2; ++i) {
        play.trigger[i] = play.grip[i] = 0;
        play.faceLower[i] = play.faceUpper[i] = 0;
        play.stick[i] = {};
    }
    play.faceBound = false;
    auto list = knownActions();
    for (auto& [action, info] : list) {
        if (info.role == Role::faceLower || info.role == Role::faceUpper) {
            // His A, B, X and Y have no subaction paths (CONFIRMED in his 0.7.0 xr_session.cpp: jump, button_b,
            // button_x... made with perHand false), so they are read without one, for the hand their binding names.
            XrActionStateGetInfo g{XR_TYPE_ACTION_STATE_GET_INFO};
            g.action = action;
            XrActionStateBoolean b{XR_TYPE_ACTION_STATE_BOOLEAN};
            const float v = next.getBoolean(session.handle, &g, &b) == XR_SUCCESS && b.isActive && b.currentState ? 1.f : 0.f;
            float* face = info.role == Role::faceLower ? play.faceLower : play.faceUpper;
            for (int side = 0; side < 2; ++side)
                if (info.sides[side]) {
                    face[side] = std::max(face[side], v);
                    if (info.role == Role::faceLower && side == 1)
                        play.faceBound = true;
                }
            continue;
        }
        for (int side = 0; side < 2; ++side) {
            if (!info.sides[side])
                continue;
            if (info.role == Role::trigger)
                play.trigger[side] = std::max(play.trigger[side], readValue(action, info, side));
            else if (info.role == Role::grip)
                play.grip[side] = std::max(play.grip[side], readValue(action, info, side));
            else if (info.role == Role::stick) {
                XrActionStateGetInfo g{XR_TYPE_ACTION_STATE_GET_INFO};
                g.action = action;
                g.subactionPath = session.hand[side];
                XrActionStateVector2f v{XR_TYPE_ACTION_STATE_VECTOR2F};
                if (next.getVector2f(session.handle, &g, &v) == XR_SUCCESS && v.isActive &&
                    std::fabs(v.currentState.x) + std::fabs(v.currentState.y) >
                        std::fabs(play.stick[side].x) + std::fabs(play.stick[side].y))
                    play.stick[side] = v.currentState;
            }
        }
    }
    // the kinds learned
    AcquireSRWLockExclusive(&actionLock);
    for (auto& [action, info] : list) {
        auto it = actions.find(action);
        if (it != actions.end())
            it->second.kind = info.kind;
    }
    ReleaseSRWLockExclusive(&actionLock);
}
// What his own reads see: the inputs the add-on uses this moment read as untouched (0, not changed).
bool hidden(XrAction action, XrPath subaction, bool* yOnly) {
    if (yOnly)
        *yOnly = false;
    AcquireSRWLockShared(&actionLock);
    auto it = actions.find(action);
    const ActionInfo info = it == actions.end() ? ActionInfo{} : it->second;
    ReleaseSRWLockShared(&actionLock);
    if (info.role == Role::none || !fInput.on)
        return false;
    const Mask& m = play.mask;
    const int side = sideOf(subaction);
    auto any = [&](const bool* bits) {
        if (side >= 0)
            return bits[side];
        return (info.sides[0] && bits[0]) || (info.sides[1] && bits[1]);
    };
    switch (info.role) {
    case Role::trigger:
        return any(m.trigger);
    case Role::grip:
        return any(m.grip);
    case Role::faceLower:
        return any(m.faceLower);
    case Role::faceUpper:
        return any(m.faceUpper);
    case Role::stick:
        if (any(m.stick))
            return true;
        if (yOnly && any(m.stickY)) {
            *yOnly = true;
            return true;
        }
        return false;
    default:
        return false;
    }
}
XRAPI_ATTR XrResult XRAPI_CALL getBoolean(XrSession s, const XrActionStateGetInfo* g, XrActionStateBoolean* out) {
    const XrResult r = next.getBoolean(s, g, out);
    if (r == XR_SUCCESS && g && out && hidden(g->action, g->subactionPath, nullptr)) {
        out->currentState = XR_FALSE;
        out->changedSinceLastSync = XR_FALSE;
    }
    return r;
}
XRAPI_ATTR XrResult XRAPI_CALL getFloat(XrSession s, const XrActionStateGetInfo* g, XrActionStateFloat* out) {
    const XrResult r = next.getFloat(s, g, out);
    if (r == XR_SUCCESS && g && out && hidden(g->action, g->subactionPath, nullptr)) {
        out->currentState = 0;
        out->changedSinceLastSync = XR_FALSE;
    }
    return r;
}
XRAPI_ATTR XrResult XRAPI_CALL getVector2f(XrSession s, const XrActionStateGetInfo* g, XrActionStateVector2f* out) {
    const XrResult r = next.getVector2f(s, g, out);
    bool yOnly = false;
    if (r == XR_SUCCESS && g && out && hidden(g->action, g->subactionPath, &yOnly)) {
        if (yOnly)
            out->currentState.y = 0;
        else
            out->currentState = {};
        out->changedSinceLastSync = XR_FALSE;
    }
    return r;
}

// --- Quads: swapchains the layer owns, uploaded only when their picture changes -----------------------------------
template <class T> void releaseCom(T*& p) {
    if (p) {
        p->Release();
        p = nullptr;
    }
}
bool pickFormat() {
    uint32_t n = 0;
    if (next.enumerateFormats(session.handle, 0, &n, nullptr) != XR_SUCCESS || !n)
        return false;
    std::vector<int64_t> formats(n);
    if (next.enumerateFormats(session.handle, n, &n, formats.data()) != XR_SUCCESS)
        return false;
    // R8G8B8A8_UNORM_SRGB, R8G8B8A8_UNORM, B8G8R8A8_UNORM_SRGB, B8G8R8A8_UNORM
    for (int64_t want : {29, 28, 91, 87})
        if (std::find(formats.begin(), formats.end(), want) != formats.end()) {
            session.format = want;
            session.bgra = want == 91 || want == 87;
            return true;
        }
    return false;
}
bool makeQuad(Quad& q, int w, int h) {
    XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    ci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT |
                    XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
    ci.format = session.format;
    ci.sampleCount = 1;
    ci.width = static_cast<uint32_t>(w);
    ci.height = static_cast<uint32_t>(h);
    ci.faceCount = ci.arraySize = ci.mipCount = 1;
    if (next.createSwapchain(session.handle, &ci, &q.swapchain) != XR_SUCCESS)
        return false;
    uint32_t n = 0;
    if (next.enumerateImages(q.swapchain, 0, &n, nullptr) != XR_SUCCESS || !n)
        return false;
    std::vector<XrSwapchainImageD3D12KHR> images(n, {XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
    if (next.enumerateImages(q.swapchain, n, &n,
                             reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())) != XR_SUCCESS)
        return false;
    for (auto& i : images)
        q.images.push_back(i.texture);
    q.width = w;
    q.height = h;
    return true;
}
void dropQuad(Quad& q) {
    if (q.swapchain && next.destroySwapchain)
        next.destroySwapchain(q.swapchain);
    q = {};
}
bool makeUploader() {
    ID3D12Device* d = session.device;
    if (FAILED(d->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&session.allocator))) ||
        FAILED(d->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, session.allocator, nullptr,
                                    IID_PPV_ARGS(&session.list))) ||
        FAILED(session.list->Close()) || FAILED(d->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&session.fence))))
        return false;
    session.fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    // one buffer big enough for the largest picture (the hologram), rows 256-byte aligned
    static_assert(holo::width >= holo::noteWidth && holo::height >= holo::noteHeight && holo::width >= 16 && holo::height >= 64);
    const UINT64 pitch = (static_cast<UINT64>(holo::width) * 4 + 255) & ~UINT64(255);
    session.uploadBytes = pitch * holo::height;
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = session.uploadBytes;
    desc.Height = desc.DepthOrArraySize = desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return session.fenceEvent && SUCCEEDED(d->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                                                      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                                      IID_PPV_ARGS(&session.upload)));
}
void dropUploader() {
    releaseCom(session.list);
    releaseCom(session.allocator);
    releaseCom(session.upload);
    releaseCom(session.fence);
    if (session.fenceEvent)
        CloseHandle(session.fenceEvent);
    session.fenceEvent = nullptr;
}
// Sets up the quads once per session; any failure turns drawing off (the menu still works, unseen, and logs why).
bool drawReady() {
    static XrSession tried = XR_NULL_HANDLE;
    if (!fDraw.on)
        return false;
    if (session.holo.swapchain && session.note.swapchain && session.dot.swapchain && session.beam.swapchain && session.upload)
        return true;
    if (tried == session.handle)
        return false;
    tried = session.handle;
    if (!session.device || !session.queue) {
        fDraw.turnOff("the session is not D3D12 (no device or queue)");
        return false;
    }
    if (!pickFormat()) {
        fDraw.turnOff("no RGBA8 swapchain format offered");
        return false;
    }
    if (!makeQuad(session.holo, holo::width, holo::height) || !makeQuad(session.note, holo::noteWidth, holo::noteHeight) ||
        !makeQuad(session.dot, holo::dotSize, holo::dotSize) || !makeQuad(session.beam, 16, 64)) {
        dropQuad(session.holo);
        dropQuad(session.note);
        dropQuad(session.dot);
        dropQuad(session.beam);
        fDraw.turnOff("the quad swapchains could not be made");
        return false;
    }
    if (!makeUploader()) {
        dropUploader();
        dropQuad(session.holo);
        dropQuad(session.note);
        dropQuad(session.dot);
        dropQuad(session.beam);
        fDraw.turnOff("the upload buffer could not be made");
        return false;
    }
    logLine("quads ready: format %lld, %u layers at most", static_cast<long long>(session.format), session.maxLayers);
    return true;
}
// One picture into the quad's next image (premultiplied alpha), on his queue, waited for before release.
bool upload(Quad& q, const Canvas& c) {
    if (c.width != q.width || c.height != q.height)
        return false;
    uint32_t index = 0;
    XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    if (next.acquireImage(q.swapchain, &ai, &index) != XR_SUCCESS || index >= q.images.size())
        return false;
    XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    wi.timeout = 100000000; // 100 ms
    bool ok = next.waitImage(q.swapchain, &wi) == XR_SUCCESS;
    const UINT pitch = (static_cast<UINT>(c.width) * 4 + 255) & ~255u;
    if (ok) {
        unsigned char* mapped = nullptr;
        D3D12_RANGE none{0, 0};
        ok = SUCCEEDED(session.upload->Map(0, &none, reinterpret_cast<void**>(&mapped)));
        if (ok) {
            for (int y = 0; y < c.height; ++y) {
                auto* row = reinterpret_cast<uint32_t*>(mapped + static_cast<size_t>(pitch) * y);
                for (int x = 0; x < c.width; ++x) {
                    const uint32_t p = c.pixels[static_cast<size_t>(y) * c.width + x];
                    const uint32_t a = p >> 24, r = (p & 255) * a / 255, g = (p >> 8 & 255) * a / 255,
                                   b = (p >> 16 & 255) * a / 255;
                    row[x] = session.bgra ? (a << 24 | r << 16 | g << 8 | b) : (a << 24 | b << 16 | g << 8 | r);
                }
            }
            session.upload->Unmap(0, nullptr);
        }
    }
    if (ok) {
        ID3D12Resource* image = q.images[index];
        ok = SUCCEEDED(session.allocator->Reset()) && SUCCEEDED(session.list->Reset(session.allocator, nullptr));
        if (ok) {
            D3D12_RESOURCE_BARRIER b{};
            b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b.Transition.pResource = image;
            b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            session.list->ResourceBarrier(1, &b);
            D3D12_TEXTURE_COPY_LOCATION dst{}, src{};
            dst.pResource = image;
            dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            src.pResource = session.upload;
            src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            src.PlacedFootprint.Footprint.Format = static_cast<DXGI_FORMAT>(session.format);
            src.PlacedFootprint.Footprint.Width = static_cast<UINT>(c.width);
            src.PlacedFootprint.Footprint.Height = static_cast<UINT>(c.height);
            src.PlacedFootprint.Footprint.Depth = 1;
            src.PlacedFootprint.Footprint.RowPitch = pitch;
            session.list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
            std::swap(b.Transition.StateBefore, b.Transition.StateAfter);
            session.list->ResourceBarrier(1, &b);
            ok = SUCCEEDED(session.list->Close());
        }
        if (ok) {
            ID3D12CommandList* lists[] = {session.list};
            session.queue->ExecuteCommandLists(1, lists);
            const UINT64 v = ++session.fenceValue;
            ok = SUCCEEDED(session.queue->Signal(session.fence, v));
            if (ok && session.fence->GetCompletedValue() < v) {
                ok = SUCCEEDED(session.fence->SetEventOnCompletion(v, session.fenceEvent)) &&
                     WaitForSingleObject(session.fenceEvent, 500) == WAIT_OBJECT_0;
            }
        }
    }
    XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    const bool released = next.releaseImage(q.swapchain, &ri) == XR_SUCCESS;
    if (ok && released)
        q.released = true;
    return ok && released;
}

// --- Poses in his projection space --------------------------------------------------------------------------------
Pose toPose(const XrPosef& p) {
    return {{p.position.x, p.position.y, p.position.z},
            {p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w}};
}
XrPosef toXr(const Pose& p) {
    XrPosef x;
    x.orientation = {p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w};
    x.position = {p.position.x, p.position.y, p.position.z};
    return x;
}
void makeSpaces() {
    if (session.spacesTried)
        return;
    XrAction grip = XR_NULL_HANDLE, aim = XR_NULL_HANDLE;
    bool gripSides[2]{}, aimSides[2]{};
    for (auto& [action, info] : knownActions()) {
        if (info.role == Role::gripPose && !grip) {
            grip = action;
            gripSides[0] = info.sides[0];
            gripSides[1] = info.sides[1];
        } else if (info.role == Role::aimPose && !aim) {
            aim = action;
            aimSides[0] = info.sides[0];
            aimSides[1] = info.sides[1];
        }
    }
    if (!grip)
        return; // his bindings are not in yet: try at the next sync
    session.spacesTried = true;
    for (int side = 0; side < 2; ++side) {
        XrActionSpaceCreateInfo ci{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        ci.poseInActionSpace.orientation.w = 1;
        ci.subactionPath = session.hand[side];
        if (gripSides[side]) {
            ci.action = grip;
            next.createActionSpace(session.handle, &ci, &session.gripSpace[side]);
        }
        if (aim && aimSides[side]) {
            ci.action = aim;
            next.createActionSpace(session.handle, &ci, &session.aimSpace[side]);
        }
    }
    if (!session.gripSpace[0])
        fMenu.turnOff("no left grip pose to put the wrist menu on");
    logLine("hand spaces: grip %d/%d aim %d/%d", session.gripSpace[0] != XR_NULL_HANDLE,
            session.gripSpace[1] != XR_NULL_HANDLE, session.aimSpace[0] != XR_NULL_HANDLE,
            session.aimSpace[1] != XR_NULL_HANDLE);
}
bool locate(XrSpace space, XrSpace base, XrTime t, Pose& out) {
    if (!space || !base || !t)
        return false;
    XrSpaceLocation l{XR_TYPE_SPACE_LOCATION};
    if (next.locateSpace(space, base, t, &l) != XR_SUCCESS)
        return false;
    constexpr XrSpaceLocationFlags need = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    if ((l.locationFlags & need) != need)
        return false;
    out = toPose(l.pose);
    return true;
}

// --- One step of the add-on per xrSyncActions -------------------------------------------------------------------------
SRWLOCK playLock = SRWLOCK_INIT;
XrSpace frameSpace = XR_NULL_HANDLE; // his projection layer's space (from his last xrEndFrame)
bool flightSwitch = true;            // the menu's FLIGHT row (this session; the launcher's flight= stays as it is)
void writeLive(const Live& l) {
    AcquireSRWLockExclusive(&settings.lock);
    settings.live = l;
    ReleaseSRWLockExclusive(&settings.lock);
    if (!writeFile(livePath(), liveText(l)))
        logLine("live.txt could not be written (the switch holds until the game closes)");
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if (GetFileAttributesExW(livePath().c_str(), GetFileExInfoStandard, &a)) {
        AcquireSRWLockExclusive(&settings.lock);
        settings.liveStamp = a.ftLastWriteTime;
        ReleaseSRWLockExclusive(&settings.lock);
    }
}
void endFlight(const char* why) {
    if (!play.flying)
        return;
    if (motionReady) {
        const DWORD r = submitOurs(false, {});
        if (r && r != 2002)
            logLine("flight end: SpidyMotionSubmit %lu", static_cast<unsigned long>(r));
    }
    AcquireSRWLockExclusive(&motionLock);
    handover.release();
    ReleaseSRWLockExclusive(&motionLock);
    play.flying = false;
    play.flight = {};
    play.flight.start.reset(); // an A still down never takes off again by itself
    logLine("flight ended: %s", why);
}
void endGrapple(const char* why) {
    play.grapple.stop();
    if (!play.grappling)
        return;
    if (motionReady) {
        const DWORD r = submitOurs(false, {});
        if (r && r != 2002)
            logLine("grapple end: SpidyMotionSubmit %lu", static_cast<unsigned long>(r));
    }
    AcquireSRWLockExclusive(&motionLock);
    handover.release();
    ReleaseSRWLockExclusive(&motionLock);
    play.grappling = false;
    logLine("grapple ended: %s", why);
}
// The line from the hand to the anchor: drawn with the beam's quad (our own picture, never his web).
void showLine(const GameView& gv, int hand, Vec3 anchor) {
    const Vec3 d = anchor - gv.handPos[hand];
    const float len = length(d);
    if (!(len > .3f) || !play.headKnown)
        return;
    const Vec3 dir = d / len;
    const Vec3 local = gameToHeadLocal(dir, gv.headRight, gv.headDown, gv.headForward);
    play.beamDirGame = normalized(play.head.orientation.rotate(local));
    play.beamLength = len;
    play.beamHand = hand;
    play.beamWidth = .025f;
    play.beamColor = 0xffe8e8e8;
    play.beamUntil = GetTickCount64() + 60;
}
// His strike sample behind SEH, in a function of its own (MSVC C2712: no __try where C++ objects unwind).
DWORD guardedStrikeSample(his::Strikes* s) {
    DWORD r = 9999;
    SM1_TRY {
        r = strikeSample(s);
    }
    SM1_EXCEPT {
        r = 9999;
    }
    return r;
}
void firePower(int hand, uint32_t hero, const Live& live, Vec3 xrFrom) {
    const Power& p = powerOf(hero);
    play.cooldown = p.cooldown;
    const GameView gv = gameView();
    if (!gv.ok) {
        play.powerState = "NO VR DATA";
        return;
    }
    his::Strikes s{};
    const DWORD r = guardedStrikeSample(&s);
    if (r || s.magic != 0x53535452 || s.bytes != sizeof(s)) {
        logLine("SpidyStrikeSample %lu magic %08x bytes %u", static_cast<unsigned long>(r), s.magic, s.bytes);
        fPowers.turnOff("SpidyStrikeSample did not answer");
        return;
    }
    const bool eyes = p.source == Source::eyes;
    const Vec3 from = eyes ? gv.headPos : gv.handPos[hand];
    const Vec3 dir = eyes ? gv.headForward : gv.handForward[hand];
    Vec3 beamDir = dir;
    float beamLength = std::min(p.range, 12.f);
    const float amount = p.damage * static_cast<float>(live.damage) / 100.f;
    // His combat's ringed enemy first (his own pick); else the nearest enemy of any type in the aim (sm1_targets).
    std::vector<combat::Target> foes;
    if (targetsUsable())
        targets::fighting(foes);
    uint64_t first = 0;
    Vec3 hitPoint{};
    if (s.cueTarget && aimedAt(from, dir, s.cuePoint, aimDegrees, p.range)) {
        first = s.cueTarget;
        hitPoint = s.cuePoint;
    } else if (const int k = combat::aimed(foes, from, dir, p.range, aimDegrees); k >= 0) {
        first = foes[static_cast<size_t>(k)].record;
        hitPoint = foes[static_cast<size_t>(k)].chest();
    }
    int hits = 0;
    if (first) {
        blow(first, gv.player, hitPoint, from, amount, p.knockback);
        ++hits;
        beamDir = normalized(hitPoint - from);
        beamLength = length(hitPoint - from);
        // Area powers: HULK's clap hits everyone close in front, THOR's lightning jumps to two more.
        if (hero == hero_modes::hulk || hero == hero_modes::thor) {
            const Vec3 centre = hero == hero_modes::hulk ? from + normalized(dir) * 2.5f : hitPoint;
            const float reach = hero == hero_modes::hulk ? 3.f : 5.f;
            const size_t most = hero == hero_modes::hulk ? 6 : 3;
            for (const int k : combat::nearby(foes, centre, reach, most)) {
                const auto& t = foes[static_cast<size_t>(k)];
                if (t.record == first)
                    continue;
                blow(t.record, gv.player, t.chest(), hero == hero_modes::hulk ? from : hitPoint, amount * .7f, p.knockback);
                if (++hits >= static_cast<int>(most))
                    break;
            }
        }
        play.powerState = hits > 1 ? "HIT " + std::to_string(hits) : "HIT";
    } else {
        play.powerState = s.cueTarget || !foes.empty() ? "MISSED" : "NO TARGET";
    }
    // 0.0.76: every shot in the log (the first test's log showed no shot at all: the menu stayed open)
    logLine("power: %s fired from the %s, %s (%.0f damage%s)", p.name, eyes ? "eyes" : hand ? "right hand" : "left hand",
            play.powerState.c_str(), static_cast<double>(amount), first ? "" : ", nothing hit");
    if (live.beams && fDraw.on) {
        // game direction -> his head's axes -> the headset's view -> his projection space
        const Vec3 local = gameToHeadLocal(beamDir, gv.headRight, gv.headDown, gv.headForward);
        play.beamDirGame = play.headKnown ? normalized(play.head.orientation.rotate(local)) : Vec3{};
        play.beamLength = beamLength;
        play.beamHand = eyes ? -1 : hand;
        play.beamColor = p.color;
        play.beamWidth = .06f;
        play.beamUntil = GetTickCount64() + 180;
        (void)xrFrom;
    }
}
// --- 0.0.76 SM1 HOLO: the SM2 build's hologram ----------------------------------------------------------------------
// The user (0.0.76 test): "the wrist menu is awful it should look exactly like sm2 holo does". The SM2 build's own panel
// (wrist_hud::appendPanelFlat: its frame, fonts, colours, tabs, the heroes' tiles with their portraits) is painted into
// the hologram quad's picture by holo::Painter off the frame thread, and worked by the SM2 build's own wrist_hud::Menu
// and wrist_hud::point (the right hand's pointer). Spider-Man 1's own rows only: heroes, outfits, powers, COLOR.
holo::Painter* painter = nullptr; // made once, never deleted (the DLL is pinned; no thread join at unload)
std::atomic<int> portraitsAsked{0};
std::atomic<const holo::Portraits*> portraitsNow{nullptr}; // published once, kept for good
// mixed into every picture's key, new each session (the painter skips a key it was asked before; a new session's
// quads are empty, so its pictures are painted afresh)
uint64_t holoGen = 0x9e3779b97f4a7c15ull;
// A Roster texture (PNG or JPEG) to RGBA8 with WIC, as the SM2 build reads it (game_xr's wicDecode); COM set up by
// the caller (the portraits' own thread).
bool wicDecode(const uint8_t* data, size_t size, mesh_model::Picture& out) {
    if (!data || !size || size > mesh_model::mostImageBytes)
        return false;
    IWICImagingFactory* factory = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    bool ok = SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) &&
              SUCCEEDED(factory->CreateStream(&stream)) &&
              SUCCEEDED(stream->InitializeFromMemory(const_cast<BYTE*>(data), static_cast<DWORD>(size))) &&
              SUCCEEDED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder)) &&
              SUCCEEDED(decoder->GetFrame(0, &frame)) && SUCCEEDED(factory->CreateFormatConverter(&converter)) &&
              SUCCEEDED(converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0,
                                              WICBitmapPaletteTypeCustom));
    UINT w = 0, h = 0;
    ok = ok && SUCCEEDED(converter->GetSize(&w, &h)) && w && h && w <= 8192 && h <= 8192;
    if (ok) {
        out.width = w;
        out.height = h;
        out.rgba.assign(size_t{w} * h * 4, 0);
        ok = SUCCEEDED(converter->CopyPixels(nullptr, w * 4, static_cast<UINT>(out.rgba.size()), out.rgba.data()));
    }
    releaseCom(converter);
    releaseCom(frame);
    releaseCom(decoder);
    releaseCom(stream);
    releaseCom(factory);
    return ok;
}
// HERO VR's folder: this layer is <HERO VR>\build\windows-ninja\herovr_sm1_layer.dll.
std::wstring heroVrRoot() {
    HMODULE self = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&heroVrRoot), &self))
        return L"";
    wchar_t buf[MAX_PATH]{};
    const DWORD n = GetModuleFileNameW(self, buf, MAX_PATH);
    if (!n || n >= MAX_PATH)
        return L"";
    std::wstring p(buf, n);
    for (int up = 0; up < 3; ++up) {
        const size_t i = p.find_last_of(L"\\/");
        if (i == std::wstring::npos)
            return L"";
        p.resize(i);
    }
    return p;
}
// The heroes' portraits from HERO VR's Models\Roster (the SM2 build's own), once, on their own thread.
DWORD WINAPI portraitsThread(void*) {
    const HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const std::wstring path = heroVrRoot() + L"\\Models\\Roster\\scene.glb";
    std::string glb;
    auto pics = std::make_shared<holo::Portraits>();
    std::string why;
    const bool ok = readFile(path, glb) &&
                    holo::loadPortraits(reinterpret_cast<const uint8_t*>(glb.data()), glb.size(), &wicDecode, *pics, why);
    if (glb.empty())
        why = "Models\\Roster\\scene.glb was not found beside HERO VR";
    if (ok) {
        static std::shared_ptr<holo::Portraits> kept;
        kept = pics;
        painter->setPortraits(pics);
        portraitsNow.store(kept.get(), std::memory_order_release);
        logLine("holo: portraits ready (%s)", why.c_str());
    } else {
        logLine("holo: no portraits, the tiles show initials (%s)", why.c_str());
    }
    if (SUCCEEDED(co))
        CoUninitialize();
    return 0;
}
bool holoReady() {
    if (!painter) {
        try {
            painter = new holo::Painter();
        } catch (...) {
            fMenu.turnOff("the hologram's painter thread did not start");
            return false;
        }
    }
    if (!portraitsAsked.exchange(1)) {
        if (HANDLE t = CreateThread(nullptr, 0, portraitsThread, nullptr, 0, nullptr))
            CloseHandle(t);
        else
            logLine("holo: the portraits' thread did not start (initials shown)");
    }
    return true;
}
// What the hologram shows now, from the add-on's own state.
holo::Now holoNow(const Config& config, const Live& live) {
    holo::Now n;
    n.config = config;
    n.live = live;
    n.flightSwitch = flightSwitch;
    n.flying = play.flying;
    n.powersWork = fPowers.on && rayReady();
    n.flightWorks = fFlight.on && motionReady;
    n.grappleWorks = n.flightWorks;
    n.powerState = fPowers.on ? play.powerState : "OFF (SEE LOG)";
    n.why[0] = !fPowers.on ? "see layer.log" : "waiting for SpidyVR's ray bridge";
    n.why[1] = !fFlight.on ? "see layer.log" : "waiting for SpidyVR's movement module";
    n.why[2] = n.why[1];
    const uint32_t hero = config.hero;
    n.flightLine = !fFlight.on ? "FLIGHT: OFF (SEE LOG)" : !motionReady ? "FLIGHT: WAITING FOR SPIDY" :
                   play.flying ? "FLIGHT: FLYING" : flies(hero) ? (play.faceBound ? "FLIGHT: ARMS UP OR HOLD A" : "FLIGHT: BOTH ARMS UP") : "FLIGHT: NOT THIS HERO";
    return n;
}
void closeHolo(const char* why) {
    if (!play.menu.open)
        return;
    play.menu.open = false;
    play.holo.set(false);
    play.holoShown = false;
    play.holoPlaced = false;
    play.pointer = {};
    play.holoArmed = false;
    logLine("holo closed (%s)", why);
}
void showNote(const std::string& title, const std::vector<std::string>& lines) {
    play.noteTitle = title;
    play.noteLines = lines;
    play.noteUntil = GetTickCount64() + 6000;
    play.noteFresh = true;
    if (!painter)
        return;
    std::vector<Vertex> verts;
    holo::note(title, lines, verts);
    play.noteKey = holo::hashOf(verts, {}) ^ holoGen ^ 1;
    play.noteUploaded = 0;
    painter->ask(play.noteKey, std::move(verts), {}, holo::noteHalfWidth, holo::noteHalfHeight, holo::noteWidth,
                 holo::noteHeight);
}
// The hologram this frame: placed on the left wrist as before, pointed at with the right hand, its picks applied.
void stepHolo(Config& config, Live& live, const Pose* grips, const bool* gripTracked, float dt) {
    play.holoScale = std::clamp(static_cast<float>(live.menuSize) / 100.f, .6f, 1.6f);
    const float h = 2 * holo::halfHeight * play.holoScale;
    play.holoPose = menuPose(grips[0], play.head.position, .06f + h / 2);
    play.holoPlaced = true;
    const Vec3 right = play.holoPose.orientation.rotate({1, 0, 0}), up = play.holoPose.orientation.rotate({0, 1, 0});
    play.pointer = gripTracked[1] ? wrist_hud::point(wrist_hud::Frame{play.holoPose.position, right, up}, play.holoScale, grips[1], 1)
                                  : wrist_hud::Pointer{};
    const uint32_t hero = config.hero < heroCount ? config.hero : 0;
    holo::Now now = holoNow(config, live);
    const holo_roster::Data data = holo::rosterData(now);
    wrist_hud::Settings set = holo::colorSettings(live);
    wrist_hud::Menu& m = play.holoMenu;
    if (m.page() != wrist_hud::powersPage)
        m.setPage(wrist_hud::powersPage);
    wrist_hud::Menu::Input in;
    in.active = true;
    in.pointer = play.pointer;
    // the trigger that opened it (or a pick that is still held) does nothing until it is let go once
    if (play.trigger[1] < .3f)
        play.holoArmed = true;
    in.trigger = play.holoArmed ? play.trigger[1] : 0.f;
    in.stick = play.stick[1].y;
    in.seconds = dt;
    in.hero = static_cast<int>(hero);
    in.heroTiles = true;
    in.rosterData = &data;
    const wrist_hud::Menu::Result r = m.update(in, set);
    if (r.rosterPlayAs >= 0 && static_cast<uint32_t>(r.rosterPlayAs) < heroCount) {
        AcquireSRWLockExclusive(&settings.lock);
        settings.config.hero = static_cast<uint32_t>(r.rosterPlayAs);
        config = settings.config;
        ReleaseSRWLockExclusive(&settings.lock);
        saveConfig(config.hero, config.outfit);
        pickSuit(config.hero);
        now = holoNow(config, live); // the picked hero's own lines (flight's for him, not the last hero's)
        now.suitLine = play.suitNotice;
        const uint32_t picked = config.hero;
        logLine("hero: %s (PLAY AS on the holo); powers %s, flight %s, grapple %s", heroName(picked),
                powerOf(picked).source == Source::none ? "none (SpidyVR's own)" : live.powers ? (fPowers.on ? "on" : "OFF (see above)") : "switched off",
                !flies(picked) ? "not this hero" : config.flight && flightSwitch ? (fFlight.on && motionReady ? "ready" : "waiting/off") : "switched off",
                !holo::grapples(picked) ? "not this hero" : live.grapple ? (fFlight.on && motionReady ? "ready" : "waiting/off") : "switched off");
        closeHolo("PLAY AS: the hologram closes so the hero's powers work at once");
        showNote(std::string("PLAYING ") + heroName(picked), holo::playLines(picked, now));
        return;
    }
    if (r.rosterSuitHero >= 0 && static_cast<uint32_t>(r.rosterSuitHero) < heroCount && r.rosterSuitIndex >= 0) {
        const uint32_t sh = static_cast<uint32_t>(r.rosterSuitHero);
        AcquireSRWLockExclusive(&settings.lock);
        settings.config.outfit[sh] = static_cast<uint32_t>(r.rosterSuitIndex);
        config = settings.config;
        ReleaseSRWLockExclusive(&settings.lock);
        saveConfig(config.hero, config.outfit);
        logLine("outfit %u saved for %s (the game's model does not change: no confirmed suit call)", config.outfit[sh], heroName(sh));
    }
    if (r.rosterSwitch >= 0) {
        switch (r.rosterSwitch) {
        case holo::swPowers: live.powers = !live.powers; break;
        case holo::swBeams: live.beams = !live.beams; break;
        case holo::swGrapple: live.grapple = !live.grapple; break;
        case holo::swHeroWebs: live.heroWebs = !live.heroWebs; break;
        case holo::swFlight: flightSwitch = !flightSwitch; break;
        default: break;
        }
        if (r.rosterSwitch != holo::swFlight)
            writeLive(live);
        logLine("holo switch %s: %s", holo::switchName(r.rosterSwitch),
                (r.rosterSwitch == holo::swPowers ? live.powers : r.rosterSwitch == holo::swBeams ? live.beams
                 : r.rosterSwitch == holo::swGrapple ? live.grapple : r.rosterSwitch == holo::swHeroWebs ? live.heroWebs
                 : flightSwitch) ? "ON" : "OFF");
    }
    if (r.colorFx) {
        live.colorFx = !live.colorFx;
        writeLive(live);
        logLine("COLOR FX: %s", live.colorFx ? "ON" : "OFF");
    }
    if (r.colorFilter) {
        live.colorFilter = holo::nextFilter(live.colorFilter);
        writeLive(live);
        logLine("COLOR FILTER: %u (COLOR FX %s)", live.colorFilter, live.colorFx ? "ON" : "OFF");
    }
    if (r.colorSlider) {
        holo::takeColor(set, live);
        writeLive(live);
    }
    if (r.rosterClosed) {
        closeHolo("CLOSE pressed");
        return;
    }
    // the picture: the SM2 panel as it stands now (painted off this thread when it changed)
    play.holoClock = std::fmod(play.holoClock + dt, 3600.f);
    now = holoNow(config, live);
    const holo_roster::Data shown = holo::rosterData(now);
    wrist_hud::View v;
    v.page = m.page();
    v.scroll = m.scroll();
    v.roster = m.roster();
    v.heroTiles = true;
    v.rosterData = &shown;
    const holo::Portraits* pics = portraitsNow.load(std::memory_order_acquire);
    v.rosterPictures = pics ? pics->at : nullptr; // none (yet): the SM2 holo's initials discs
    v.settings = holo::colorSettings(live);
    v.hover = m.hover();
    v.held = m.held();
    v.helpFor = m.helpFor();
    v.helpScroll = m.helpScroll();
    v.color = holo::colorBits(live);
    v.seconds = std::floor(play.holoClock * 10.f) / 10.f; // the light's slow pulse, ten steps a second
    holo::panel(v, play.holoTranslucent, play.holoTextured);
    const uint64_t key = holo::hashOf(play.holoTranslucent, play.holoTextured) ^ holoGen;
    if (key != play.holoAsked) {
        play.holoAsked = key;
        painter->ask(key, play.holoTranslucent, play.holoTextured, holo::halfWidth, holo::halfHeight, holo::width, holo::height);
    }
}
void step() {
    LARGE_INTEGER now, freq;
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&freq);
    float dt = play.lastSync.QuadPart ? static_cast<float>(now.QuadPart - play.lastSync.QuadPart) /
                                            static_cast<float>(freq.QuadPart)
                                      : 0.f;
    play.lastSync = now;
    dt = std::clamp(dt, 0.f, .1f);
    refreshLive();
    AcquireSRWLockShared(&settings.lock);
    Config config = settings.config;
    Live live = settings.live;
    ReleaseSRWLockShared(&settings.lock);

    readInputs();
    makeSpaces();

    // poses (his projection space, the frame he is about to show)
    Pose grips[2], aim[2];
    bool gripTracked[2]{};
    for (int i = 0; i < 2; ++i)
        gripTracked[i] = locate(session.gripSpace[i], frameSpace, play.displayTime, grips[i]);
    const bool gripOk = gripTracked[0];
    bool aimOk[2]{};
    for (int i = 0; i < 2; ++i) {
        aimOk[i] = locate(session.aimSpace[i], frameSpace, play.displayTime, aim[i]);
        play.handOut[i] = aimOk[i] && play.headKnown && length(aim[i].position - play.head.position) > outstretched;
    }

    // the wrist menu: the SM2 build's hologram (0.0.76 SM1 HOLO), opened and closed by the SM2 wrist click
    WristMenu& menu = play.menu; // menu.open: the hologram is up (powers, flight and grapple wait while it is)
    play.holoHands = 0;
    bool holoClicked = false; // the wrist click opened or closed the hologram this frame
    if (config.wristMenu && fMenu.on && gripOk && holoReady()) {
        // The SM2 wrist click (wrist_hud::Toggle mode 4, FOREARM): hologram on the left wrist (hand 0), the
        // right hand touches it and clicks its trigger. Its thresholds are the SM2 build's own.
        play.holo.setMode(4);
        play.holo.set(menu.open);
        play.holo.look(play.head, play.headKnown);
        const float triggers[2]{play.trigger[0], play.trigger[1]};
        const float squeezes[2]{play.grip[0], play.grip[1]};
        play.holo.update(dt, true, 0, grips, gripTracked, triggers, squeezes);
        play.holoHands = play.holo.hands();
        if (play.holo.on() != menu.open) {
            holoClicked = true;
            if (play.holo.on()) {
                menu.open = true;
                play.holoShown = false;
                play.holoArmed = false;
                play.holoAsked = 0;
                play.noteUntil = 0; // the note gives way to the hologram
                logLine("holo open (SM2 wrist click): powers, flight and grapple wait until it closes");
            } else {
                closeHolo("SM2 wrist click");
            }
        }
        if (menu.open)
            stepHolo(config, live, grips, gripTracked, dt);
    } else if (menu.open) {
        closeHolo(!config.wristMenu ? "wrist menu switched off" : !gripOk ? "left hand lost tracking" : "the hologram is off (see above)");
    }
    const uint32_t nowHero = config.hero < heroCount ? config.hero : 0;
    const bool nowSpiderMan = !config.heroes || nowHero == 0;

    // flight
    const bool flightArmed = config.heroes && config.flight && flightSwitch && flies(nowHero) && fFlight.on && motionReady;
    if (fFlight.on && motionReady) {
        const GameView gv = gameView();
        FlightInput in;
        in.allowed = flightArmed && gv.ok && !menu.open;
        in.stop = !flightSwitch;
        // HERO VR 0.0.76 FLIGHT: the arms in his projection space (metres, Y up), from the head; their steer is
        // laid onto the game's head (its level forward and right) so turning the head and the arms agree.
        Vec3 offset[2]{};
        bool tracked[2]{};
        for (int i = 0; i < 2; ++i) {
            tracked[i] = gripTracked[i] && play.headKnown;
            offset[i] = tracked[i] ? grips[i].position - play.head.position : Vec3{};
        }
        const Vec3 up{0, 1, 0};
        const auto flat = [](Vec3 v) { return Vec3{v.x, 0, v.z}; };
        in.armsUp = fly076::armsUp(offset, tracked, up);
        const Vec3 way = fly076::steerWay(offset, tracked, up);
        const Vec3 tf = fly076::unitOr(flat(play.head.orientation.rotate({0, 0, -1})), {0, 0, -1});
        const Vec3 tr{-tf.z, 0, tf.x};
        const Vec3 gf = fly076::unitOr(flat(gv.headForward), {0, 0, 1});
        const Vec3 gr = fly076::unitOr(flat(gv.headRight), {gf.z, 0, -gf.x});
        in.steer = length(way) > .5f ? normalized(gf * dot(way, tf) + gr * dot(way, tr) + up * way.y) : Vec3{};
        in.a = play.faceLower[1] > .5f;
        in.b = play.faceUpper[1] > .5f;
        in.headForward = gv.headForward;
        in.headRight = gv.headRight;
        in.throttle = play.stick[0].y;
        his::MotionData d{};
        in.grounded = sampleOurs(d) && d.steps && d.grounded;
        in.dt = dt;
        const FlightEvent ev = stepFlight(play.flight, in);
        if (ev == FlightEvent::tookOff) {
            AcquireSRWLockExclusive(&motionLock);
            handover.take();
            ReleaseSRWLockExclusive(&motionLock);
            play.flying = true;
            logLine("flight: took off as %s", heroName(nowHero));
        }
        if (play.flight.on && play.flying) {
            const DWORD r = submitOurs(true, play.flight.velocity);
            if (r) {
                logLine("flight: SpidyMotionSubmit %lu", static_cast<unsigned long>(r));
                endFlight(r == 2002 ? "his movement module is not running (2002)" : "his movement module refused");
                if (r == 2001)
                    fFlight.turnOff("SpidyMotionSubmit refused the command (2001)");
            }
        } else if (play.flying) {
            endFlight(ev == FlightEvent::landed ? "landed" : "stopped");
        }
    } else if (play.flying) {
        endFlight("flight turned off");
    }

    // CAPTAIN AMERICA's and DAREDEVIL's grapple (never with the flight: those heroes do not fly)
    const bool grappleHero = nowHero == hero_modes::captainAmerica || nowHero == hero_modes::daredevil;
    const bool grappleArmed = config.heroes && !nowSpiderMan && grappleHero && live.grapple && fFlight.on && motionReady &&
                              !play.flying;
    if (grappleArmed) {
        const GameView gv = gameView();
        GrappleInput in;
        in.allowed = gv.ok && !menu.open;
        in.captain = nowHero == hero_modes::captainAmerica;
        for (int i = 0; i < 2; ++i) {
            in.grip[i] = play.holoHands >> i & 1u ? 0.f : play.grip[i];
            in.trigger[i] = play.holoHands >> i & 1u ? 0.f : play.trigger[i];
        }
        his::AimData aims{};
        if ((play.grip[0] > .3f || play.grip[1] > .3f) && sampleAims(aims))
            for (int i = 0; i < 2; ++i) {
                in.aimOk[i] = true;
                in.aimKind[i] = aims.hands[i].kind;
                in.aimPoint[i] = aims.hands[i].point;
                in.aimNormal[i] = aims.hands[i].normal;
            }
        his::MotionData d{};
        const bool moved = sampleOurs(d) && d.steps;
        in.body = moved ? d.position : gv.headPos;
        in.velocity = moved ? d.achievedVelocity : Vec3{};
        in.grounded = moved && d.grounded;
        in.jump = play.faceLower[1] > .5f; // the right A (his jump)
        in.dt = dt;
        if (!moved)
            in.allowed = false;
        const GrappleStep g = play.grapple.step(in);
        if (g.started)
            logLine("grapple: line from hand %d to (%.1f, %.1f, %.1f)", g.line, static_cast<double>(g.anchor.x),
                    static_cast<double>(g.anchor.y), static_cast<double>(g.anchor.z));
        play.grappleState = g.started ? "GRAPPLE: PULLING" : g.missed ? "GRAPPLE: NOTHING TO HOOK" : g.jumped ? "GRAPPLE: JUMP OUT"
                            : g.cancelled ? "GRAPPLE: COAST" : play.grappleState;
        if (g.drive) {
            if (!play.grappling) {
                AcquireSRWLockExclusive(&motionLock);
                handover.take();
                ReleaseSRWLockExclusive(&motionLock);
                play.grappling = true;
            }
            const DWORD r = submitOurs(true, g.velocity);
            if (r) {
                logLine("grapple: SpidyMotionSubmit %lu", static_cast<unsigned long>(r));
                endGrapple(r == 2002 ? "his movement module is not running (2002)" : "his movement module refused");
                if (r == 2001)
                    fFlight.turnOff("SpidyMotionSubmit refused the command (2001)");
            }
        } else if (play.grappling) {
            endGrapple(g.jumped ? "jump out" : "arrived");
            play.grappleState = "GRAPPLE: READY";
        }
        if (g.line >= 0 && gv.ok && fDraw.on)
            showLine(gv, g.line, g.anchor);
    } else if (play.grappling || play.grapple.driving()) {
        endGrapple("grapple turned off");
    }

    // powers (the enemy scan runs only while a hero with powers is picked)
    const bool fighting = !nowSpiderMan && live.powers && fPowers.on;
    if (fighting && targetsUsable())
        targets::want(true);
    else
        targets::want(false);
    play.cooldown = std::max(0.f, play.cooldown - dt);
    // The trigger that clicked the hologram (and any trigger held while it was up) is never also a power: it has to be
    // let go first (0.0.76 test: the click that closed the hologram fired a power in the same frame).
    for (int i = 0; i < 2; ++i)
        if (holoClicked || menu.open || (play.holoHands >> i & 1u))
            play.triggerWas[i] = true;
    if (!nowSpiderMan && live.powers && fPowers.on && !menu.open && rayReady()) {
        for (int i = 0; i < 2; ++i) {
            const bool down = play.trigger[i] > .5f;
            if (down && !play.triggerWas[i] && play.handOut[i] &&
                (play.grip[i] < .5f || (!live.heroWebs && !grappleArmed)) && play.cooldown <= 0)
                firePower(i, nowHero, live, aim[i].position);
        }
    }
    for (int i = 0; i < 2; ++i)
        play.triggerWas[i] = play.trigger[i] > .5f;

    // what his reads see from now on
    MaskInput mi;
    mi.menuOpen = menu.open;
    mi.holoHands = play.holoHands;
    mi.spiderMan = nowSpiderMan;
    mi.powers = live.powers && fPowers.on;
    mi.heroWebs = live.heroWebs;
    mi.flightArmed = flightArmed;
    mi.flying = play.flying;
    mi.grappleArmed = grappleArmed;
    // An A that came down in flight stays ours until it is let go (no jump on landing).
    play.aOurs = play.faceLower[1] > .5f && (play.flying || play.aOurs);
    mi.aHeld = play.aOurs;
    for (int i = 0; i < 2; ++i) {
        mi.grip[i] = play.grip[i] > .5f;
        mi.handOut[i] = play.handOut[i];
        mi.triggerOurs[i] = play.triggerOurs[i];
    }
    play.mask = config.heroes || menu.open ? maskFor(mi) : Mask{};
    for (int i = 0; i < 2; ++i)
        play.triggerOurs[i] = keepTrigger(play.trigger[i], play.mask.trigger[i]);
}
XRAPI_ATTR XrResult XRAPI_CALL syncActions(XrSession s, const XrActionsSyncInfo* info) {
    const XrResult r = next.syncActions(s, info);
    if (r == XR_SUCCESS && s == session.handle && fInput.on) {
        AcquireSRWLockExclusive(&playLock);
        SM1_TRY {
            step();
        }
        SM1_EXCEPT {
            play.mask = {};
            fInput.turnOff("a fault in the add-on's step");
        }
        ReleaseSRWLockExclusive(&playLock);
    }
    return r;
}
XRAPI_ATTR XrResult XRAPI_CALL waitFrame(XrSession s, const XrFrameWaitInfo* wi, XrFrameState* state) {
    const XrResult r = next.waitFrame(s, wi, state);
    if (XR_SUCCEEDED(r) && state && s == session.handle)
        play.displayTime = state->predictedDisplayTime;
    return r;
}

// --- COLOR (0.0.76): the SM2 build's colour grade, drawn on his two eye images ----------------------------------------
// eye_color.hpp's apply() (the SM2 build's COLOR / ATMOSPHERE, its looks METROPOLIS .. SYNTHWAVE and its sliders), as
// one fullscreen draw per eye image at his xrReleaseSwapchainImage: the image is copied, then drawn back graded. It goes
// on his own graphics-binding queue (the game's queue, his 0.7.0 xr_session.cpp), right after his own work and before
// the runtime gets the image, so nothing waits on the CPU. His swapchains are learnt from his projection layer (the
// first graded frame is the one after); our quads never pass through these hooks. live.txt color_fx=0 (the default)
// or a neutral grade: no GPU work at all, his picture, anti-aliasing and timing exactly as without the add-on.
// Anything that fails (the shader, a format it does not know) turns COLOR off for the session, logged; nothing else.
FeatureState fColor{"COLOR"};
struct EyeChain {
    int64_t format = 0; // what he asked for (the view format)
    uint32_t width = 0, height = 0;
    bool usable = false; // 8-bit RGBA / BGRA, one sample, one layer, a colour attachment
    bool his = false;    // in his projection layer
    std::vector<ID3D12Resource*> images;
    uint32_t acquired = UINT32_MAX;
    ID3D12Resource* copy = nullptr; // the source the draw reads (typeless, same size)
    bool warned = false;
};
SRWLOCK colorLock = SRWLOCK_INIT;
std::map<XrSwapchain, EyeChain> chains;
std::atomic<bool> colorOn{false};
eye_color::Params colorParams; // colorLock
int colorFamily(int64_t f) {   // 1 RGBA8, 2 BGRA8, 0 another format
    switch (static_cast<DXGI_FORMAT>(f)) {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        return 1;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        return 2;
    default:
        return 0;
    }
}
bool isSrgb(DXGI_FORMAT f) { return f == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || f == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB; }
bool isTypeless(DXGI_FORMAT f) { return f == DXGI_FORMAT_R8G8B8A8_TYPELESS || f == DXGI_FORMAT_B8G8R8A8_TYPELESS; }
// eye_color.hpp apply(), the same steps in the same order, display-encoded (the SM2 build's native_eye_gpu.cpp grade()).
const char* colorShader = R"(
Texture2D source : register(t0);
cbuffer Grade : register(b0) {
    float gain; float contrast; float crush; float saturation;
    float vibrance; float mono; float vignette; float linearOut;
    float4 balance; float4 shadowTone; float4 highTone;
};
float4 vs(uint id : SV_VertexID) : SV_POSITION {
    const float2 corner = float2((id << 1) & 2, id & 2);
    return float4(corner * float2(2, -2) + float2(-1, 1), 0, 1);
}
float3 toLinear(float3 c) {
    return lerp(pow((c + 0.055) / 1.055, 2.4), c / 12.92, step(c, 0.04045));
}
float3 toDisplay(float3 l) {
    return lerp(1.055 * pow(max(l, 0.0), 1.0 / 2.4) - 0.055, l * 12.92, step(l, 0.0031308));
}
float3 shoulder(float3 x) {
    return lerp(x, 0.8 + 0.2 * (1.0 - exp(-max(x - 0.8, 0.0) / 0.2)), step(0.8, x));
}
float3 grade(float3 c, float2 uv) {
    c = saturate(c);
    if (gain != 1.0)
        c = shoulder(toDisplay(toLinear(c) * gain));
    c = saturate(((c - 0.5) * contrast + 0.5 - crush) / (1.0 - crush));
    const float3 lw = float3(0.2126, 0.7152, 0.0722);
    float y = dot(c, lw);
    const float mx = max(c.r, max(c.g, c.b)), mn = min(c.r, min(c.g, c.b));
    const float skin = saturate((c.r - c.g) * 4.0) * saturate((c.g - c.b) * 4.0);
    const float boost = saturation * (1.0 + vibrance * (1.0 - (mx - mn)) * (1.0 - 0.7 * skin));
    c = saturate(y + (c - y) * boost);
    c = saturate(c * balance.rgb);
    y = dot(c, lw);
    c = saturate(c + shadowTone.rgb * (1.0 - y) * (1.0 - y) + highTone.rgb * y * y);
    y = dot(c, lw);
    c = c + (y - c) * mono;
    const float2 d = uv - 0.5;
    const float d2 = dot(d, d) * 2.0;
    return saturate(c * saturate(1.0 - vignette * 0.7 * d2));
}
float4 ps(float4 position : SV_POSITION) : SV_TARGET {
    uint w, h;
    source.GetDimensions(w, h);
    const float4 s = source.Load(int3(int2(position.xy), 0));
    const float3 o = grade(s.rgb, position.xy / float2(w, h));
    return float4(lerp(o, toLinear(o), linearOut), s.a);
}
)";
using CompileFn = HRESULT(WINAPI*)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*, ID3DInclude*, LPCSTR, LPCSTR, UINT,
                                   UINT, ID3DBlob**, ID3DBlob**);
constexpr int colorSlots = 8; // two eyes a frame, four frames in flight at most
struct Grader {
    bool tried = false, ready = false;
    ID3D12RootSignature* root = nullptr;
    ID3DBlob* vs = nullptr;
    ID3DBlob* ps = nullptr;
    std::map<DXGI_FORMAT, ID3D12PipelineState*> states;
    ID3D12DescriptorHeap* views = nullptr;   // shader visible, one SRV a slot
    ID3D12DescriptorHeap* targets = nullptr; // one RTV a slot
    UINT viewStep = 0, targetStep = 0;
    ID3D12CommandAllocator* allocators[colorSlots]{};
    UINT64 done[colorSlots]{};
    int slot = 0;
    ID3D12GraphicsCommandList* list = nullptr;
    ID3D12Fence* fence = nullptr;
    UINT64 value = 0;
    HANDLE event = nullptr;
    uint64_t graded = 0, skipped = 0;
} grader;
template <class T> void release(T*& p) {
    if (p)
        p->Release();
    p = nullptr;
}
// The GPU has finished everything COLOR gave it (bounded: a lost device must not hang his frame).
void colorIdle() {
    if (!grader.fence || grader.fence->GetCompletedValue() >= grader.value || !grader.event)
        return;
    if (SUCCEEDED(grader.fence->SetEventOnCompletion(grader.value, grader.event)))
        WaitForSingleObject(grader.event, 2000);
}
void dropChain(EyeChain& c) { release(c.copy); }
void dropGrader() { // colorLock held
    colorIdle();
    for (auto& [h, c] : chains)
        dropChain(c);
    for (auto& [f, s] : grader.states)
        release(s);
    grader.states.clear();
    for (auto*& a : grader.allocators)
        release(a);
    release(grader.list);
    release(grader.fence);
    release(grader.views);
    release(grader.targets);
    release(grader.root);
    release(grader.vs);
    release(grader.ps);
    if (grader.event)
        CloseHandle(grader.event);
    grader = Grader{};
}
bool colorFail(const char* why) {
    fColor.turnOff(why);
    colorOn = false;
    return false;
}
bool prepareGrader() { // colorLock held
    if (grader.ready)
        return true;
    if (grader.tried)
        return false;
    grader.tried = true;
    ID3D12Device* d = session.device;
    if (!d || !session.queue)
        return colorFail("the session is not D3D12 (no device or queue)");
    HMODULE compiler = LoadLibraryW(L"d3dcompiler_47.dll");
    const auto compile = compiler ? reinterpret_cast<CompileFn>(GetProcAddress(compiler, "D3DCompile")) : nullptr;
    if (!compile)
        return colorFail("d3dcompiler_47.dll (D3DCompile) is not there");
    ID3DBlob* error = nullptr;
    const size_t n = std::strlen(colorShader);
    if (FAILED(compile(colorShader, n, "herovr_sm1_color", nullptr, nullptr, "vs", "vs_5_0", 1u << 11, 0, &grader.vs,
                       &error)) ||
        FAILED(compile(colorShader, n, "herovr_sm1_color", nullptr, nullptr, "ps", "ps_5_0", 1u << 11, 0, &grader.ps,
                       &error))) {
        if (error)
            logLine("COLOR shader: %.400s", static_cast<const char*>(error->GetBufferPointer()));
        release(error);
        return colorFail("the colour shader did not compile");
    }
    D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0};
    D3D12_ROOT_PARAMETER parameters[2]{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[0].DescriptorTable = {1, &range};
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[1].Constants.Num32BitValues = 20;
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC rs{2, parameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE};
    ID3DBlob* signature = nullptr;
    const bool rootOk = SUCCEEDED(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)) &&
                        SUCCEEDED(d->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                                         IID_PPV_ARGS(&grader.root)));
    release(signature);
    release(error);
    if (!rootOk)
        return colorFail("the colour pass's root signature could not be made");
    D3D12_DESCRIPTOR_HEAP_DESC h{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, colorSlots,
                                 D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0};
    D3D12_DESCRIPTOR_HEAP_DESC t{D3D12_DESCRIPTOR_HEAP_TYPE_RTV, colorSlots, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0};
    if (FAILED(d->CreateDescriptorHeap(&h, IID_PPV_ARGS(&grader.views))) ||
        FAILED(d->CreateDescriptorHeap(&t, IID_PPV_ARGS(&grader.targets))))
        return colorFail("the colour pass's descriptor heaps could not be made");
    grader.viewStep = d->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    grader.targetStep = d->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (auto*& a : grader.allocators)
        if (FAILED(d->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&a))))
            return colorFail("the colour pass's command allocators could not be made");
    if (FAILED(d->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, grader.allocators[0], nullptr,
                                    IID_PPV_ARGS(&grader.list))) ||
        FAILED(grader.list->Close()) || FAILED(d->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&grader.fence))))
        return colorFail("the colour pass's command list or fence could not be made");
    grader.event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!grader.event)
        return colorFail("the colour pass's fence event could not be made");
    grader.ready = true;
    logLine("COLOR: the colour pass is ready (D3D12, his graphics queue)");
    return true;
}
ID3D12PipelineState* gradeState(DXGI_FORMAT target) { // colorLock held
    auto it = grader.states.find(target);
    if (it != grader.states.end())
        return it->second;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
    d.pRootSignature = grader.root;
    d.VS = {grader.vs->GetBufferPointer(), grader.vs->GetBufferSize()};
    d.PS = {grader.ps->GetBufferPointer(), grader.ps->GetBufferSize()};
    d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    d.NumRenderTargets = 1;
    d.RTVFormats[0] = target;
    d.SampleDesc.Count = 1;
    d.SampleMask = UINT_MAX;
    d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    d.RasterizerState.DepthClipEnable = TRUE;
    auto& blend = d.BlendState.RenderTarget[0];
    blend.SrcBlend = blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlend = blend.DestBlendAlpha = D3D12_BLEND_ZERO;
    blend.BlendOp = blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    ID3D12PipelineState* s = nullptr;
    if (FAILED(session.device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&s))))
        s = nullptr;
    grader.states[target] = s;
    return s;
}
void barrier(ID3D12GraphicsCommandList* l, ID3D12Resource* r, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = r;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = from;
    b.Transition.StateAfter = to;
    l->ResourceBarrier(1, &b);
}
// One of his eye images, graded in place before the runtime gets it. colorLock held.
void gradeImage(EyeChain& c) {
    if (!prepareGrader())
        return;
    ID3D12Resource* image = c.images[c.acquired];
    const D3D12_RESOURCE_DESC desc = image->GetDesc();
    const int family = colorFamily(desc.Format);
    if (family == 0 || desc.SampleDesc.Count != 1 || desc.DepthOrArraySize != 1 ||
        !(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) || desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
        if (!c.warned)
            logLine("COLOR: an eye image it cannot draw on (format %d, %u samples, %u layers, flags %#x): that eye is left "
                    "as it is", static_cast<int>(desc.Format), desc.SampleDesc.Count, desc.DepthOrArraySize,
                    static_cast<unsigned>(desc.Flags));
        c.warned = true;
        return;
    }
    const DXGI_FORMAT typeless = family == 1 ? DXGI_FORMAT_R8G8B8A8_TYPELESS : DXGI_FORMAT_B8G8R8A8_TYPELESS;
    const DXGI_FORMAT unorm = family == 1 ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_B8G8R8A8_UNORM;
    // A typed image takes only its own format; a typeless one the format he asked for.
    const DXGI_FORMAT target = isTypeless(desc.Format) ?
        (colorFamily(c.format) == family ? static_cast<DXGI_FORMAT>(c.format) : unorm) : desc.Format;
    ID3D12PipelineState* state = gradeState(target);
    if (!state) {
        colorFail("the colour pass's pipeline could not be made for his eye format");
        return;
    }
    ID3D12Device* d = session.device;
    if (c.copy) {
        const D3D12_RESOURCE_DESC have = c.copy->GetDesc();
        if (have.Width != desc.Width || have.Height != desc.Height || have.Format != typeless)
            release(c.copy);
    }
    if (!c.copy) {
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC cd{};
        cd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        cd.Width = desc.Width;
        cd.Height = desc.Height;
        cd.DepthOrArraySize = 1;
        cd.MipLevels = 1;
        cd.Format = typeless;
        cd.SampleDesc.Count = 1;
        cd.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        if (FAILED(d->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &cd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                              nullptr, IID_PPV_ARGS(&c.copy)))) {
            c.copy = nullptr;
            colorFail("the colour pass's copy of an eye could not be made (GPU memory)");
            return;
        }
    }
    // The slot's last use has to be finished (4 frames back; normally long done). If not, this eye goes ungraded.
    const int s = grader.slot;
    if (grader.fence->GetCompletedValue() < grader.done[s]) {
        if (FAILED(grader.fence->SetEventOnCompletion(grader.done[s], grader.event)) ||
            WaitForSingleObject(grader.event, 4) != WAIT_OBJECT_0) {
            ++grader.skipped;
            return;
        }
    }
    grader.slot = (s + 1) % colorSlots;
    ID3D12GraphicsCommandList* l = grader.list;
    if (FAILED(grader.allocators[s]->Reset()) || FAILED(l->Reset(grader.allocators[s], state))) {
        colorFail("the colour pass's command list could not be reset");
        return;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE srv = grader.views->GetCPUDescriptorHandleForHeapStart();
    srv.ptr += static_cast<SIZE_T>(s) * grader.viewStep;
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpu = grader.views->GetGPUDescriptorHandleForHeapStart();
    srvGpu.ptr += static_cast<UINT64>(s) * grader.viewStep;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = grader.targets->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += static_cast<SIZE_T>(s) * grader.targetStep;
    D3D12_SHADER_RESOURCE_VIEW_DESC sv{};
    sv.Format = unorm; // display-encoded values, as the SM2 grade reads them
    sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sv.Texture2D.MipLevels = 1;
    d->CreateShaderResourceView(c.copy, &sv, srv);
    D3D12_RENDER_TARGET_VIEW_DESC tv{};
    tv.Format = target;
    tv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    d->CreateRenderTargetView(image, &tv, rtv);
    // OpenXR D3D12: a colour image is in RENDER_TARGET when he releases it, and must be left so.
    barrier(l, image, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
    barrier(l, c.copy, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12_TEXTURE_COPY_LOCATION dst{}, src{};
    dst.pResource = c.copy;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src.pResource = image;
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    l->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    barrier(l, c.copy, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    barrier(l, image, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    const eye_color::Params& p = colorParams;
    const float constants[20] = {p.gain, p.contrast, p.crush, p.saturation, p.vibrance, p.mono, p.vignette,
                                 isSrgb(target) ? 1.f : 0.f, p.balance[0], p.balance[1], p.balance[2], 1,
                                 p.shadow[0], p.shadow[1], p.shadow[2], 0, p.high[0], p.high[1], p.high[2], 0};
    l->SetGraphicsRootSignature(grader.root);
    l->SetDescriptorHeaps(1, &grader.views);
    l->SetGraphicsRootDescriptorTable(0, srvGpu);
    l->SetGraphicsRoot32BitConstants(1, 20, constants, 0);
    const D3D12_VIEWPORT vp{0, 0, static_cast<float>(desc.Width), static_cast<float>(desc.Height), 0, 1};
    const D3D12_RECT sc{0, 0, static_cast<LONG>(desc.Width), static_cast<LONG>(desc.Height)};
    l->RSSetViewports(1, &vp);
    l->RSSetScissorRects(1, &sc);
    l->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    l->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    l->DrawInstanced(3, 1, 0, 0);
    if (FAILED(l->Close())) {
        colorFail("the colour pass's command list could not be closed");
        return;
    }
    ID3D12CommandList* lists[] = {l};
    session.queue->ExecuteCommandLists(1, lists);
    if (FAILED(session.queue->Signal(grader.fence, ++grader.value))) {
        colorFail("his queue refused the colour pass's fence");
        return;
    }
    grader.done[s] = grader.value;
    if (++grader.graded == 1)
        logLine("COLOR: first eye image graded (%ux%u, view format %d)", static_cast<unsigned>(desc.Width), desc.Height,
                static_cast<int>(target));
}
void guardedGrade(EyeChain& c) { // SEH in a function of its own (MSVC C2712)
    SM1_TRY {
        gradeImage(c);
    }
    SM1_EXCEPT {
        colorFail("a fault in the colour pass");
    }
}
void tryGrade(XrSwapchain sc) {
    AcquireSRWLockExclusive(&colorLock);
    auto it = chains.find(sc);
    if (it != chains.end() && it->second.his && it->second.usable && it->second.acquired < it->second.images.size() &&
        fColor.on) {
        guardedGrade(it->second);
    }
    ReleaseSRWLockExclusive(&colorLock);
}
// Once a frame (his xrEndFrame): his projection swapchains, and live.txt's grade.
void colorFrame(const XrFrameEndInfo* info) {
    AcquireSRWLockShared(&settings.lock);
    const eye_color::Params p = eye_color::params(colorConfig(settings.live));
    ReleaseSRWLockShared(&settings.lock);
    const bool want = fColor.on && !eye_color::neutral(p);
    AcquireSRWLockExclusive(&colorLock);
    colorParams = p;
    if (want)
        for (uint32_t i = 0; i < info->layerCount; ++i)
            if (info->layers[i] && info->layers[i]->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
                const auto* pl = reinterpret_cast<const XrCompositionLayerProjection*>(info->layers[i]);
                for (uint32_t v = 0; pl->views && v < pl->viewCount; ++v) {
                    auto it = chains.find(pl->views[v].subImage.swapchain);
                    if (it != chains.end() && !it->second.his) {
                        it->second.his = true;
                        logLine("COLOR: his eye swapchain %ux%u (format %d)%s", it->second.width, it->second.height,
                                static_cast<int>(it->second.format), it->second.usable ? "" : ": not a format it draws on");
                    }
                }
            }
    ReleaseSRWLockExclusive(&colorLock);
    if (colorOn.exchange(want) != want)
        logLine("COLOR %s", want ? "on (the grade is drawn on his eye images)" : "off (his picture as it is, no GPU work)");
}
XRAPI_ATTR XrResult XRAPI_CALL createSwapchain(XrSession s, const XrSwapchainCreateInfo* ci, XrSwapchain* out) {
    const XrResult r = next.createSwapchain(s, ci, out);
    if (XR_SUCCEEDED(r) && ci && out && s == session.handle) {
        EyeChain c;
        c.format = ci->format;
        c.width = ci->width;
        c.height = ci->height;
        c.usable = colorFamily(ci->format) && ci->sampleCount == 1 && ci->arraySize == 1 && ci->faceCount == 1 &&
                   (ci->usageFlags & XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT);
        AcquireSRWLockExclusive(&colorLock);
        chains[*out] = c;
        ReleaseSRWLockExclusive(&colorLock);
    }
    return r;
}
XRAPI_ATTR XrResult XRAPI_CALL destroySwapchain(XrSwapchain sc) {
    AcquireSRWLockExclusive(&colorLock);
    auto it = chains.find(sc);
    if (it != chains.end()) {
        colorIdle();
        dropChain(it->second);
        chains.erase(it);
    }
    ReleaseSRWLockExclusive(&colorLock);
    return next.destroySwapchain(sc);
}
XRAPI_ATTR XrResult XRAPI_CALL enumerateImages(XrSwapchain sc, uint32_t capacity, uint32_t* count,
                                               XrSwapchainImageBaseHeader* images) {
    const XrResult r = next.enumerateImages(sc, capacity, count, images);
    if (XR_SUCCEEDED(r) && images && capacity && count && images->type == XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR) {
        AcquireSRWLockExclusive(&colorLock);
        auto it = chains.find(sc);
        if (it != chains.end()) {
            const auto* d3d = reinterpret_cast<const XrSwapchainImageD3D12KHR*>(images);
            it->second.images.clear();
            for (uint32_t i = 0; i < *count && i < capacity; ++i)
                it->second.images.push_back(d3d[i].texture);
        }
        ReleaseSRWLockExclusive(&colorLock);
    }
    return r;
}
XRAPI_ATTR XrResult XRAPI_CALL acquireImage(XrSwapchain sc, const XrSwapchainImageAcquireInfo* info, uint32_t* index) {
    const XrResult r = next.acquireImage(sc, info, index);
    if (XR_SUCCEEDED(r) && index) {
        AcquireSRWLockExclusive(&colorLock);
        auto it = chains.find(sc);
        if (it != chains.end())
            it->second.acquired = *index;
        ReleaseSRWLockExclusive(&colorLock);
    }
    return r;
}
XRAPI_ATTR XrResult XRAPI_CALL releaseImage(XrSwapchain sc, const XrSwapchainImageReleaseInfo* info) {
    if (colorOn.load(std::memory_order_relaxed))
        tryGrade(sc);
    const XrResult r = next.releaseImage(sc, info);
    AcquireSRWLockExclusive(&colorLock);
    auto it = chains.find(sc);
    if (it != chains.end())
        it->second.acquired = UINT32_MAX;
    ReleaseSRWLockExclusive(&colorLock);
    return r;
}
void dropColor() {
    AcquireSRWLockExclusive(&colorLock);
    dropGrader();
    chains.clear();
    ReleaseSRWLockExclusive(&colorLock);
    colorOn = false;
}

// --- xrEndFrame: his frame as it is, plus the add-on's quads on top -----------------------------------------------------
XrCompositionLayerQuad quadLayer(const Quad& q, XrSpace space, const Pose& pose, float w, float h) {
    XrCompositionLayerQuad l{XR_TYPE_COMPOSITION_LAYER_QUAD};
    l.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    l.space = space;
    l.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
    l.subImage.swapchain = q.swapchain;
    l.subImage.imageRect = {{0, 0}, {q.width, q.height}};
    l.pose = toXr(pose);
    l.size = {w, h};
    return l;
}
void addOns(const XrFrameEndInfo* info, XrSpace space, std::vector<XrCompositionLayerQuad>& quads) {
    if (!drawReady())
        return;
    // 0.0.76 SM1 HOLO: the painter's newest picture onto its quad (the hologram's or the PLAY AS note's)
    if (painter) {
        Canvas c;
        uint64_t key = 0;
        if (painter->take(c, key)) {
            const bool isNote = key == play.noteKey;
            Quad& q = isNote ? session.note : session.holo;
            if (c.width == static_cast<int>(q.width) && c.height == static_cast<int>(q.height)) {
                if (!upload(q, c)) {
                    fDraw.turnOff("the hologram picture could not be uploaded");
                    return;
                }
                if (isNote)
                    play.noteUploaded = key;
                else if (play.menu.open)
                    play.holoShown = true;
            }
        }
    }
    if (play.menu.open && play.holoPlaced && play.holoShown && session.holo.released) {
        const float k = play.holoScale;
        Pose pose = play.holoPose;
        Pose grip;
        if (locate(session.gripSpace[0], space, info->displayTime, grip))
            pose = menuPose(grip, play.head.position, .06f + holo::halfHeight * k);
        quads.push_back(quadLayer(session.holo, space, pose, 2 * holo::halfWidth * k, 2 * holo::halfHeight * k));
        if (play.pointer.hit) {
            if (!play.dotReady) {
                Canvas d(holo::dotSize, holo::dotSize);
                holo::paintDot(d);
                play.dotReady = upload(session.dot, d);
            }
            if (play.dotReady && session.dot.released) {
                Pose at = pose;
                at.position = pose.position + pose.orientation.rotate({play.pointer.u * k, play.pointer.v * k, .003f});
                quads.push_back(quadLayer(session.dot, space, at, .012f * k, .012f * k));
            }
        }
    }
    if (!play.menu.open && play.noteUntil > GetTickCount64() && play.noteKey && play.noteUploaded == play.noteKey &&
        session.note.released) {
        if (play.noteFresh && play.headKnown) {
            // once, where the eyes look now: a metre ahead, a little low, facing them
            Vec3 f = play.head.orientation.rotate({0, 0, -1});
            f.y = 0;
            const float n = length(f);
            f = n > .1f ? f * (1.f / n) : Vec3{0, 0, -1};
            Pose base;
            base.position = play.head.position + f * 1.f + Vec3{0, -.12f, 0};
            play.notePose = menuPose(base, play.head.position, 0);
            play.noteFresh = false;
        }
        if (!play.noteFresh)
            quads.push_back(quadLayer(session.note, space, play.notePose, .44f,
                                      .44f * static_cast<float>(holo::noteHeight) / static_cast<float>(holo::noteWidth)));
    }
    if (play.beamUntil > GetTickCount64() && length(play.beamDirGame) > .5f) {
        static uint32_t painted = 0xffffffff;
        if (painted != play.beamColor) {
            Canvas c(16, 64);
            paintBeam(c, play.beamColor);
            if (!upload(session.beam, c)) {
                fDraw.turnOff("the beam picture could not be uploaded");
                return;
            }
            painted = play.beamColor;
        }
        Pose from = play.head;
        if (play.beamHand >= 0)
            locate(session.aimSpace[play.beamHand], space, info->displayTime, from);
        if (session.beam.released)
            quads.push_back(quadLayer(session.beam, space,
                                      beamPose(from.position, play.beamDirGame, play.beamLength, play.head.position),
                                      play.beamWidth,
                                      play.beamLength));
    }
}
// No C++ objects to unwind here (MSVC's __try needs that): a fault drops this frame's quads and turns drawing off.
void tryAddOns(const XrFrameEndInfo* info, XrSpace space, std::vector<XrCompositionLayerQuad>* quads) {
    SM1_TRY {
        addOns(info, space, *quads);
    }
    SM1_EXCEPT {
        quads->clear();
        fDraw.turnOff("a fault while drawing");
    }
}
XRAPI_ATTR XrResult XRAPI_CALL endFrame(XrSession s, const XrFrameEndInfo* info) {
    if (s == session.handle && info)
        colorFrame(info);
    if (s != session.handle || !info || !fDraw.on)
        return next.endFrame(s, info);
    std::vector<XrCompositionLayerQuad> quads;
    AcquireSRWLockExclusive(&playLock);
    const XrCompositionLayerProjection* projection = nullptr;
    for (uint32_t i = 0; i < info->layerCount; ++i)
        if (info->layers[i] && info->layers[i]->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
            projection = reinterpret_cast<const XrCompositionLayerProjection*>(info->layers[i]);
            break;
        }
    if (projection && projection->viewCount >= 1 && projection->views) {
        frameSpace = projection->space;
        const Pose a = toPose(projection->views[0].pose);
        const Pose b = projection->viewCount >= 2 ? toPose(projection->views[1].pose) : a;
        play.head = {(a.position + b.position) * .5f, a.orientation};
        play.headKnown = true;
        tryAddOns(info, projection->space, &quads);
    }
    ReleaseSRWLockExclusive(&playLock);
    if (quads.empty())
        return next.endFrame(s, info);
    std::vector<const XrCompositionLayerBaseHeader*> layers(info->layers, info->layers + info->layerCount);
    for (auto& q : quads)
        if (layers.size() < session.maxLayers)
            layers.push_back(reinterpret_cast<const XrCompositionLayerBaseHeader*>(&q));
    XrFrameEndInfo mine = *info;
    mine.layerCount = static_cast<uint32_t>(layers.size());
    mine.layers = layers.data();
    const XrResult r = next.endFrame(s, &mine);
    if (XR_SUCCEEDED(r) || r == XR_ERROR_SESSION_LOST || r == XR_ERROR_INSTANCE_LOST)
        return r;
    logLine("xrEndFrame refused the add-on's quads (%d): his frame goes again without them", static_cast<int>(r));
    fDraw.turnOff("the runtime refused the quad layers");
    return next.endFrame(s, info);
}

// --- Sessions and the instance ----------------------------------------------------------------------------------------
void dropSession() {
    holoGen = holoGen * 6364136223846793005ull + 1442695040888963407ull;
    endFlight("the session ended");
    for (int i = 0; i < 2; ++i) {
        if (session.gripSpace[i] && next.destroySpace)
            next.destroySpace(session.gripSpace[i]);
        if (session.aimSpace[i] && next.destroySpace)
            next.destroySpace(session.aimSpace[i]);
    }
    dropQuad(session.holo);
    dropQuad(session.note);
    dropQuad(session.dot);
    dropQuad(session.beam);
    dropUploader();
    dropColor();
    session = Session{};
    play = Play{};
    frameSpace = XR_NULL_HANDLE;
}
XRAPI_ATTR XrResult XRAPI_CALL createSession(XrInstance inst, const XrSessionCreateInfo* ci, XrSession* out) {
    const XrResult r = next.createSession(inst, ci, out);
    if (!XR_SUCCEEDED(r) || !ci || !out)
        return r;
    AcquireSRWLockExclusive(&playLock);
    if (session.handle)
        dropSession();
    session.handle = *out;
    for (auto* n = static_cast<const XrBaseInStructure*>(ci->next); n; n = n->next)
        if (n->type == XR_TYPE_GRAPHICS_BINDING_D3D12_KHR) {
            const auto* b = reinterpret_cast<const XrGraphicsBindingD3D12KHR*>(n);
            session.device = b->device;
            session.queue = b->queue;
        }
    next.stringToPath(inst, "/user/hand/left", &session.hand[0]);
    next.stringToPath(inst, "/user/hand/right", &session.hand[1]);
    XrSystemProperties props{XR_TYPE_SYSTEM_PROPERTIES};
    if (next.getSystemProperties(inst, ci->systemId, &props) == XR_SUCCESS && props.graphicsProperties.maxLayerCount)
        session.maxLayers = props.graphicsProperties.maxLayerCount;
    ReleaseSRWLockExclusive(&playLock);
    loadSettings();
    logLine("session: D3D12 %s, %u layers at most", session.device && session.queue ? "yes" : "no", session.maxLayers);
    // The research scan: read only, its own thread; it waits while live.txt has sm1scan=0.
    scan::Feed feed;
    feed.enabled = [] { // live.txt is re-read by step() every 2 s; the scan only reads the shared value
        AcquireSRWLockShared(&settings.lock);
        const bool on = settings.live.scan;
        ReleaseSRWLockShared(&settings.lock);
        return on;
    };
    feed.log = [](const char* line) { logLine("%s", line); };
    feed.resourceVtable = []() -> uintptr_t { // one of his swapchain images: an ID3D12Resource the runtime made
        uintptr_t vt = 0;
        AcquireSRWLockShared(&colorLock);
        for (const auto& kv : chains)
            if (!kv.second.images.empty() && kv.second.images[0]) {
                vt = *reinterpret_cast<const uintptr_t*>(kv.second.images[0]);
                break;
            }
        ReleaseSRWLockShared(&colorLock);
        return vt;
    };
    scan::start(feed);
    if (!motionThreadStarted.exchange(true)) {
        HANDLE h = CreateThread(nullptr, 0, installMotionHooks, nullptr, 0, nullptr);
        if (h)
            CloseHandle(h);
        else
            fFlight.turnOff("the movement hand-over thread did not start");
    }
    return r;
}
XRAPI_ATTR XrResult XRAPI_CALL destroySession(XrSession s) {
    AcquireSRWLockExclusive(&playLock);
    if (s == session.handle)
        dropSession();
    ReleaseSRWLockExclusive(&playLock);
    return next.destroySession(s);
}
XRAPI_ATTR XrResult XRAPI_CALL destroyInstance(XrInstance inst) {
    const auto destroy = next.destroyInstance;
    if (inst == instance) {
        AcquireSRWLockExclusive(&playLock);
        if (session.handle)
            dropSession();
        ReleaseSRWLockExclusive(&playLock);
        instance = XR_NULL_HANDLE;
        logLine("instance closed");
    }
    return destroy ? destroy(inst) : XR_ERROR_HANDLE_INVALID;
}
XRAPI_ATTR XrResult XRAPI_CALL getInstanceProcAddr(XrInstance inst, const char* name, PFN_xrVoidFunction* fn);
template <class F> bool load(XrInstance inst, const char* name, F& f) {
    PFN_xrVoidFunction v = nullptr;
    if (next.getInstanceProcAddr(inst, name, &v) != XR_SUCCESS || !v)
        return false;
    f = reinterpret_cast<F>(v);
    return true;
}
XRAPI_ATTR XrResult XRAPI_CALL createApiLayerInstance(const XrInstanceCreateInfo* ci, const XrApiLayerCreateInfo* li,
                                                       XrInstance* out) {
    if (!li || !li->nextInfo || !li->nextInfo->nextCreateApiLayerInstance || !li->nextInfo->nextGetInstanceProcAddr)
        return XR_ERROR_INITIALIZATION_FAILED;
    XrApiLayerCreateInfo mine = *li;
    mine.nextInfo = li->nextInfo->next;
    const XrResult r = li->nextInfo->nextCreateApiLayerInstance(ci, &mine, out);
    if (!XR_SUCCEEDED(r))
        return r;
    next = Next{};
    next.getInstanceProcAddr = li->nextInfo->nextGetInstanceProcAddr;
    instance = *out;
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    char exeA[MAX_PATH]{};
    WideCharToMultiByte(CP_UTF8, 0, exe, -1, exeA, MAX_PATH, nullptr, nullptr);
    const bool host = sameFileName(fileNameOf(exeA), hostExe);
    gameFolder = std::string(folderOf(exeA));
    const bool ok =
        load(*out, "xrDestroyInstance", next.destroyInstance) && load(*out, "xrCreateSession", next.createSession) &&
        load(*out, "xrDestroySession", next.destroySession) &&
        load(*out, "xrSuggestInteractionProfileBindings", next.suggestBindings) &&
        load(*out, "xrSyncActions", next.syncActions) && load(*out, "xrGetActionStateBoolean", next.getBoolean) &&
        load(*out, "xrGetActionStateFloat", next.getFloat) && load(*out, "xrGetActionStateVector2f", next.getVector2f) &&
        load(*out, "xrWaitFrame", next.waitFrame) && load(*out, "xrEndFrame", next.endFrame) &&
        load(*out, "xrPathToString", next.pathToString) && load(*out, "xrStringToPath", next.stringToPath) &&
        load(*out, "xrCreateActionSpace", next.createActionSpace) && load(*out, "xrLocateSpace", next.locateSpace) &&
        load(*out, "xrDestroySpace", next.destroySpace) && load(*out, "xrGetSystemProperties", next.getSystemProperties) &&
        load(*out, "xrEnumerateSwapchainFormats", next.enumerateFormats) &&
        load(*out, "xrCreateSwapchain", next.createSwapchain) && load(*out, "xrDestroySwapchain", next.destroySwapchain) &&
        load(*out, "xrEnumerateSwapchainImages", next.enumerateImages) &&
        load(*out, "xrAcquireSwapchainImage", next.acquireImage) && load(*out, "xrWaitSwapchainImage", next.waitImage) &&
        load(*out, "xrReleaseSwapchainImage", next.releaseImage);
    passthrough = !host || !ok;
    if (host)
        logLine("%s %s (%s) in %s: %s", addonName, addonVersion, layerName, exeA,
                passthrough ? "PASSTHROUGH (an OpenXR function is missing)" : "on");
    return r;
}
XRAPI_ATTR XrResult XRAPI_CALL getInstanceProcAddr(XrInstance inst, const char* name, PFN_xrVoidFunction* fn) {
    if (!name || !fn)
        return XR_ERROR_VALIDATION_FAILURE;
    if (!next.getInstanceProcAddr)
        return XR_ERROR_HANDLE_INVALID;
    const XrResult r = next.getInstanceProcAddr(inst, name, fn);
    if (r != XR_SUCCESS || passthrough || inst != instance)
        return r;
    const std::string_view n = name;
    auto mine = [&](auto f) {
        *fn = reinterpret_cast<PFN_xrVoidFunction>(f);
        return XR_SUCCESS;
    };
    if (n == "xrGetInstanceProcAddr")
        return mine(&getInstanceProcAddr);
    if (n == "xrDestroyInstance")
        return mine(&destroyInstance);
    if (n == "xrCreateSession")
        return mine(&createSession);
    if (n == "xrDestroySession")
        return mine(&destroySession);
    if (n == "xrSuggestInteractionProfileBindings")
        return mine(&suggestBindings);
    if (n == "xrSyncActions")
        return mine(&syncActions);
    if (n == "xrGetActionStateBoolean")
        return mine(&getBoolean);
    if (n == "xrGetActionStateFloat")
        return mine(&getFloat);
    if (n == "xrGetActionStateVector2f")
        return mine(&getVector2f);
    if (n == "xrWaitFrame")
        return mine(&waitFrame);
    if (n == "xrEndFrame")
        return mine(&endFrame);
    if (n == "xrCreateSwapchain")
        return mine(&createSwapchain);
    if (n == "xrDestroySwapchain")
        return mine(&destroySwapchain);
    if (n == "xrEnumerateSwapchainImages")
        return mine(&enumerateImages);
    if (n == "xrAcquireSwapchainImage")
        return mine(&acquireImage);
    if (n == "xrReleaseSwapchainImage")
        return mine(&releaseImage);
    return r;
}
} // namespace

extern "C" __declspec(dllexport) XrResult XRAPI_CALL xrNegotiateLoaderApiLayerInterface(
    const XrNegotiateLoaderInfo* loaderInfo, const char* name, XrNegotiateApiLayerRequest* request) {
    if (!loaderInfo || !request || loaderInfo->structType != XR_LOADER_INTERFACE_STRUCT_LOADER_INFO ||
        loaderInfo->structVersion != XR_LOADER_INFO_STRUCT_VERSION || loaderInfo->structSize != sizeof(*loaderInfo) ||
        request->structType != XR_LOADER_INTERFACE_STRUCT_API_LAYER_REQUEST ||
        request->structVersion != XR_API_LAYER_INFO_STRUCT_VERSION || request->structSize != sizeof(*request) ||
        loaderInfo->minInterfaceVersion > XR_CURRENT_LOADER_API_LAYER_VERSION ||
        loaderInfo->maxInterfaceVersion < XR_CURRENT_LOADER_API_LAYER_VERSION)
        return XR_ERROR_INITIALIZATION_FAILED;
    if (name && std::strcmp(name, layerName) != 0)
        return XR_ERROR_INITIALIZATION_FAILED;
    request->layerInterfaceVersion = XR_CURRENT_LOADER_API_LAYER_VERSION;
    XrVersion api = XR_CURRENT_API_VERSION;
    if (api > loaderInfo->maxApiVersion)
        api = loaderInfo->maxApiVersion;
    if (api < loaderInfo->minApiVersion)
        api = loaderInfo->minApiVersion;
    request->layerApiVersion = api;
    request->getInstanceProcAddr = &getInstanceProcAddr;
    request->createApiLayerInstance = &createApiLayerInstance;
    return XR_SUCCESS;
}

BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(self);
        // The loader may unload layers at xrDestroyInstance; the hooks and threads here must outlive that.
        HMODULE pinned = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                           reinterpret_cast<LPCWSTR>(&DllMain), &pinned);
    }
    return TRUE;
}
