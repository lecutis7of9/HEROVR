// HERO VR 0.0.76 SM1: the add-on's OpenXR API layer (herovr_sm1_layer.dll). Made by Agent Mulder.
// Mulder's Heroes add-on for iilyn's Spider-Man VR mod (SpidyVR by Ilya Mezerowsky, GPL-3.0,
// https://github.com/IlyaMez/SpidyVR). See include/spidy/sm1_addon.hpp for what it does and does not touch.
//
// Addresses: none. Everything here goes through OpenXR (the loader hands the layer his calls) and through his own
// exported functions, found by name with GetProcAddress (CONFIRMED in his 0.7.0 sources):
//   SpidyXrData (data)  spidy_stereo_probe.dll    XrData v43, 1312 bytes (seqlock `sequence`)
//   SpidyStrikeSample   spidy_ray_bridge.dll      Strikes v3, 328 bytes (cueTarget, cuePoint)
//   SpidyPunchTest      spidy_ray_bridge.dll      Test v1, 80 bytes (blocks up to 1 s: called on a worker thread)
//   SpidyMotionSubmit   spidy_movement_bridge.dll Command v1, 48 bytes (serial must rise; 2001 bad, 2002 off, 2003 old)
//   SpidyMotionSample   spidy_movement_bridge.dll Data v4, 176 bytes (serial at +48, grounded at +168)
// Every failure is logged to %APPDATA%\HERO VR SM1\layer.log and turns off only its own feature.
#include "spidy/sm1_addon.hpp"

#include <windows.h>
#include <d3d12.h>
#include <unknwn.h>

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
    if (readFile(livePath(), text, &stamp))
        l = parseLive(text);
    else if (!livePath().empty() && !writeFile(livePath(), liveText(l)))
        logLine("live.txt could not be written (the defaults stay)");
    AcquireSRWLockExclusive(&settings.lock);
    settings.config = c;
    settings.live = l;
    settings.liveStamp = stamp;
    ReleaseSRWLockExclusive(&settings.lock);
    logLine("settings: heroes=%d wrist_menu=%d flight=%d hero=%u powers=%d hero_webs=%d beams=%d damage=%d menu=%d",
            c.heroes, c.wristMenu, c.flight, c.hero, l.powers, l.heroWebs, l.beams, l.damage, l.menuSize);
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
    logLine("live.txt: powers=%d hero_webs=%d beams=%d damage=%d menu=%d", l.powers, l.heroWebs, l.beams, l.damage,
            l.menuSize);
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
        v.handForward[i] = normalized(Vec3{-m[8], -m[9], -m[10]});
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

// --- The power's blow: SpidyPunchTest waits for the game's next physics step, so it runs on its own thread --------
SRWLOCK punchLock = SRWLOCK_INIT;
CONDITION_VARIABLE punchWake = CONDITION_VARIABLE_INIT;
his::Test punchQueued;
bool punchPending = false;
std::atomic<DWORD> punchResult{0};
std::atomic<uint64_t> punchHits{0};
DWORD WINAPI punchWorker(void*) {
    for (;;) {
        AcquireSRWLockExclusive(&punchLock);
        while (!punchPending)
            SleepConditionVariableSRW(&punchWake, &punchLock, INFINITE, 0);
        his::Test t = punchQueued;
        punchPending = false;
        ReleaseSRWLockExclusive(&punchLock);
        DWORD r = 9999;
        SM1_TRY {
            r = punchTest(&t);
        }
        SM1_EXCEPT {
            r = 9999;
        }
        punchResult = r;
        if (r == 0)
            ++punchHits;
        else if (r == 9502 || r == 9999)
            fPowers.turnOff(r == 9502 ? "SpidyPunchTest refused the blow (9502)" : "SpidyPunchTest faulted");
        else
            logLine("SpidyPunchTest %lu", static_cast<unsigned long>(r));
    }
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
    punchQueued = t;
    punchPending = true;
    ReleaseSRWLockExclusive(&punchLock);
    WakeConditionVariable(&punchWake);
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
    Quad menu, beam;
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
    FlightState flight;
    bool flying = false;
    Mask mask;
    // raw inputs this sync (before any hiding)
    float trigger[2]{}, grip[2]{};
    XrVector2f stick[2]{};
    bool handOut[2]{};
    bool triggerWas[2]{};
    float cooldown = 0;
    // the beam on screen
    ULONGLONG beamUntil = 0;
    int beamHand = 1;
    Vec3 beamDirGame{};
    float beamLength = 0;
    uint32_t beamColor = 0xffffffff;
    std::string powerState = "READY";
    MenuView shown;
    bool menuPainted = false;
    uint64_t lastGameSerial = 0;
    XrTime displayTime = 0;
    // the last head pose in his projection space
    bool headKnown = false;
    Pose head;
} play;

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
        play.stick[i] = {};
    }
    auto list = knownActions();
    for (auto& [action, info] : list) {
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
    // one buffer big enough for the menu (the largest picture), rows 256-byte aligned
    const UINT64 pitch = (static_cast<UINT64>(menuWidth) * 4 + 255) & ~UINT64(255);
    session.uploadBytes = pitch * menuHeight;
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
    if (session.menu.swapchain && session.beam.swapchain && session.upload)
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
    if (!makeQuad(session.menu, menuWidth, menuHeight) || !makeQuad(session.beam, 16, 64)) {
        dropQuad(session.menu);
        dropQuad(session.beam);
        fDraw.turnOff("the quad swapchains could not be made");
        return false;
    }
    if (!makeUploader()) {
        dropUploader();
        dropQuad(session.menu);
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
    logLine("flight ended: %s", why);
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
    DWORD r = 9999;
    SM1_TRY {
        r = strikeSample(&s);
    }
    SM1_EXCEPT {
        r = 9999;
    }
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
    if (s.cueTarget && aimedAt(from, dir, s.cuePoint, aimDegrees, p.range)) {
        his::Test t;
        t.victim = s.cueTarget;
        t.damager = gv.player;
        t.point = s.cuePoint;
        t.direction = normalized(s.cuePoint - from);
        t.amount = p.damage * static_cast<float>(live.damage) / 100.f;
        t.knockback = p.knockback;
        queuePunch(t);
        play.powerState = "HIT";
        beamDir = t.direction;
        beamLength = length(s.cuePoint - from);
    } else {
        play.powerState = s.cueTarget ? "MISSED" : "NO TARGET";
    }
    if (live.beams && fDraw.on) {
        // game direction -> his head's axes -> the headset's view -> his projection space
        Vec3 local{dot(beamDir, gv.headRight), -dot(beamDir, gv.headDown), -dot(beamDir, gv.headForward)};
        play.beamDirGame = play.headKnown ? normalized(play.head.orientation.rotate(local)) : Vec3{};
        play.beamLength = beamLength;
        play.beamHand = eyes ? -1 : hand;
        play.beamColor = p.color;
        play.beamUntil = GetTickCount64() + 180;
        (void)xrFrom;
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
    const uint32_t hero = config.hero < heroCount ? config.hero : 0;

    // poses (his projection space, the frame he is about to show)
    Pose leftGrip, aim[2];
    const bool gripOk = locate(session.gripSpace[0], frameSpace, play.displayTime, leftGrip);
    bool aimOk[2]{};
    for (int i = 0; i < 2; ++i) {
        aimOk[i] = locate(session.aimSpace[i], frameSpace, play.displayTime, aim[i]);
        play.handOut[i] = aimOk[i] && play.headKnown && length(aim[i].position - play.head.position) > outstretched;
    }

    // the wrist menu
    WristMenu& menu = play.menu;
    if (config.wristMenu && fMenu.on && gripOk) {
        menu.outfits = std::max(1u, outfitCount(hero));
        WristInput in;
        in.palmUp = palmUp(leftGrip.orientation);
        in.stickX = play.stick[1].x;
        in.stickY = play.stick[1].y;
        in.trigger = play.trigger[1] > .5f;
        in.dt = dt;
        const bool wasOpen = menu.open;
        const Pick pick = menu.step(in);
        if (!wasOpen && menu.open)
            logLine("wrist menu open");
        if (pick == Pick::hero && menu.row >= 0 && static_cast<uint32_t>(menu.row) < heroCount) {
            AcquireSRWLockExclusive(&settings.lock);
            settings.config.hero = static_cast<uint32_t>(menu.row);
            config = settings.config;
            ReleaseSRWLockExclusive(&settings.lock);
            saveConfig(config.hero, config.outfit);
            logLine("hero: %s", heroName(config.hero));
        } else if (pick == Pick::outfit && menu.row >= 0) {
            AcquireSRWLockExclusive(&settings.lock);
            settings.config.outfit[hero] = static_cast<uint32_t>(menu.row);
            config = settings.config;
            ReleaseSRWLockExclusive(&settings.lock);
            saveConfig(config.hero, config.outfit);
            logLine("outfit %u for %s: %s", config.outfit[hero], heroName(hero), outfitOf(hero, config.outfit[hero]).name.c_str());
        } else if (pick == Pick::toggle) {
            switch (static_cast<Toggle>(menu.row)) {
            case Toggle::powers:
                live.powers = !live.powers;
                writeLive(live);
                break;
            case Toggle::heroWebs:
                live.heroWebs = !live.heroWebs;
                writeLive(live);
                break;
            case Toggle::beams:
                live.beams = !live.beams;
                writeLive(live);
                break;
            case Toggle::flight:
                flightSwitch = !flightSwitch;
                break;
            default:
                break;
            }
        }
    } else if (menu.open) {
        menu.open = false;
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
        for (int i = 0; i < 2; ++i) {
            in.grip[i] = play.grip[i] > .5f;
            in.aim[i] = gv.handForward[i];
        }
        in.headForward = gv.headForward;
        in.headRight = gv.headRight;
        in.moveX = play.stick[0].x;
        in.moveY = play.stick[0].y;
        in.climb = play.stick[1].y;
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

    // powers
    play.cooldown = std::max(0.f, play.cooldown - dt);
    if (!nowSpiderMan && live.powers && fPowers.on && !menu.open && rayReady()) {
        for (int i = 0; i < 2; ++i) {
            const bool down = play.trigger[i] > .5f;
            if (down && !play.triggerWas[i] && play.handOut[i] &&
                (play.grip[i] < .5f || !live.heroWebs || flightArmed) && play.cooldown <= 0)
                firePower(i, nowHero, live, aim[i].position);
        }
    }
    for (int i = 0; i < 2; ++i)
        play.triggerWas[i] = play.trigger[i] > .5f;

    // what his reads see from now on
    MaskInput mi;
    mi.menuOpen = menu.open;
    mi.palmReady = !menu.open && menu.held > 0;
    mi.spiderMan = nowSpiderMan;
    mi.powers = live.powers && fPowers.on;
    mi.heroWebs = live.heroWebs;
    mi.flightArmed = flightArmed;
    mi.flying = play.flying;
    for (int i = 0; i < 2; ++i) {
        mi.grip[i] = play.grip[i] > .5f;
        mi.handOut[i] = play.handOut[i];
    }
    play.mask = config.heroes || menu.open ? maskFor(mi) : Mask{};
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

// --- xrEndFrame: his frame as it is, plus the add-on's quads on top -----------------------------------------------------
MenuView currentView(uint32_t hero, const Config& config, const Live& live) {
    MenuView v;
    v.page = play.menu.page;
    v.row = play.menu.row;
    v.hero = hero;
    v.outfit = config.outfit[hero];
    v.toggles[static_cast<int>(Toggle::powers)] = live.powers;
    v.toggles[static_cast<int>(Toggle::flight)] = flightSwitch && config.flight;
    v.toggles[static_cast<int>(Toggle::heroWebs)] = live.heroWebs;
    v.toggles[static_cast<int>(Toggle::beams)] = live.beams;
    v.flying = play.flying;
    v.powerState = fPowers.on ? play.powerState : "POWERS OFF";
    v.status[0] = std::string("LAYER ON ") + addonVersion;
    v.status[1] = !fFlight.on ? "FLIGHT: OFF (SEE LOG)" : !motionReady ? "FLIGHT: WAITING FOR SPIDY" :
                  play.flying ? "FLIGHT: FLYING" : flies(hero) ? "FLIGHT: HOLD A GRIP" : "FLIGHT: NOT THIS HERO";
    v.status[2] = !fPowers.on ? "POWERS: OFF (SEE LOG)" : rayReady() ? "POWERS: " + play.powerState : "POWERS: WAITING FOR SPIDY";
    v.status[3] = fDraw.on ? "DRAWING: ON" : "DRAWING: OFF";
    v.status[4] = "HERO MODELS: NOT IN SM1 YET";
    v.status[5] = "OUTFIT: " + outfitOf(hero, config.outfit[hero]).name;
    for (auto& s : v.status)
        for (auto& ch : s)
            ch = static_cast<char>(ch >= 'a' && ch <= 'z' ? ch - 32 : ch);
    return v;
}
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
    AcquireSRWLockShared(&settings.lock);
    const Config config = settings.config;
    const Live live = settings.live;
    ReleaseSRWLockShared(&settings.lock);
    const uint32_t hero = config.hero < heroCount ? config.hero : 0;
    if (play.menu.open) {
        const MenuView v = currentView(hero, config, live);
        if (!play.menuPainted || !sameView(v, play.shown)) {
            Canvas c(menuWidth, menuHeight);
            paintMenu(c, v);
            if (upload(session.menu, c)) {
                play.shown = v;
                play.menuPainted = true;
            } else {
                fDraw.turnOff("the menu picture could not be uploaded");
                return;
            }
        }
        Pose grip;
        if (session.menu.released && locate(session.gripSpace[0], space, info->displayTime, grip)) {
            const float w = .20f * static_cast<float>(live.menuSize) / 100.f;
            const float h = w * static_cast<float>(menuHeight) / static_cast<float>(menuWidth);
            quads.push_back(quadLayer(session.menu, space, menuPose(grip, play.head.position, .06f + h / 2), w, h));
        }
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
                                      beamPose(from.position, play.beamDirGame, play.beamLength, play.head.position), .06f,
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
    endFlight("the session ended");
    for (int i = 0; i < 2; ++i) {
        if (session.gripSpace[i] && next.destroySpace)
            next.destroySpace(session.gripSpace[i]);
        if (session.aimSpace[i] && next.destroySpace)
            next.destroySpace(session.aimSpace[i]);
    }
    dropQuad(session.menu);
    dropQuad(session.beam);
    dropUploader();
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
