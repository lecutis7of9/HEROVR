// Update 0.0.72 GAME BODY (EXPERIMENTAL): native_game_body.hpp.
#include "spidy/native_game_body.hpp"
#include "spidy/game_body.hpp"
#include "spidy/game_player.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>
using namespace spidy;
extern "C" {
// For the probe (tools/probe_game_body.py --phases find): what the checks found.
struct GameBodyData {
    uint32_t magic = 0x59444247; // GBDY
    uint32_t version = 1;
    uint32_t state{};  // 0 idle, 1 checking, 2 verified, 3 refused
    uint32_t passes{}; // in a row on `record`
    uint64_t serial{}, record{};
    uint64_t writer{}, toModel{}; // RVAs found (0 none)
    uint32_t refound{}, rounds{};
    char check[96]{}; // the failed check, "" none
};
__declspec(dllexport) GameBodyData SpidyGameBodyData;
}
namespace {
std::mutex lock;
std::condition_variable wake;
std::thread worker;
bool stopping{};
uintptr_t base{};
game_body::Gate gate;
native_game_body::Found functions{};
std::string stateText = "off";
uint32_t stateSerial{};
std::atomic<uintptr_t> verifiedRecord{};

bool read(uintptr_t p, void* out, size_t n) {
    __try {
        if (p < 0x10000)
            return false;
        std::memcpy(out, reinterpret_cast<void*>(p), n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
// The whole image readable (every page committed and readable), so a byte
// search over it cannot fault; its size, or 0.
size_t readableImage(uintptr_t at) {
    uint8_t headers[0x400]{};
    if (!read(at, headers, sizeof(headers)))
        return 0;
    uint32_t pe{}, size{};
    std::memcpy(&pe, headers + 0x3c, 4);
    if (pe + 0x58 > sizeof(headers))
        return 0;
    std::memcpy(&size, headers + pe + 0x50, 4); // OptionalHeader.SizeOfImage
    for (uintptr_t p = at; p < at + size;) {
        MEMORY_BASIC_INFORMATION m{};
        if (!VirtualQuery(reinterpret_cast<void*>(p), &m, sizeof(m)) || m.State != MEM_COMMIT ||
            (m.Protect & (PAGE_NOACCESS | PAGE_GUARD)) || !m.RegionSize)
            return 0;
        p = reinterpret_cast<uintptr_t>(m.BaseAddress) + m.RegionSize;
    }
    return size;
}
// The checks that hold for the session: functions and vtables. "" passes.
std::string staticChecks(native_game_body::Found& out) {
    const game_body::Reader reader = [](uintptr_t rva, void* p, size_t n) { return read(base + rva, p, n); };
    out = {};
    if (game_body::candidatesHold(reader)) {
        out = {game_body::candidateWriter, game_body::candidateToModel, false};
    } else {
        const size_t size = readableImage(base);
        if (!size)
            return "writer bytes (image not readable to search)";
        const std::span<const uint8_t> image(reinterpret_cast<const uint8_t*>(base), size);
        std::vector<game_body::Section> code;
        for (const auto& s : game_body::sections(image.first((std::min<size_t>)(size, 0x1000))))
            if (s.code)
                code.push_back(s);
        game_body::Functions f;
        if (code.empty() || !game_body::findFunctions(image, code, f))
            return "writer bytes (not exactly one writer calling the conversion)";
        out = {f.writer, f.toModel, f.refound};
    }
    // The registry's vtables (game_player.cpp) must be the classes the exe's
    // own RTTI names.
    struct Named {
        uintptr_t rva;
        const char* name;
    };
    for (const auto& v : {Named{game_body::candidateHeroLocalVtable, ".?AVHeroLocal@Hero@@"},
                          Named{game_body::candidateMoverManagerVtable, ".?AVHeroMoverManager@Hero@@"},
                          Named{game_body::candidateMoverStandardVtable, ".?AVMoverStandard@"}})
        if (!game_body::rttiNamed(reader, base, v.rva, v.name))
            return std::string("rtti ") + v.name;
    return {};
}
void publish(const native_game_body::Found& f, const std::string& failed, uint32_t rounds) {
    std::string text;
    if (gate.verified) {
        char b[160];
        snprintf(b, sizeof(b), "verified (writer %llx, conversion %llx%s, record %llx)",
                 static_cast<unsigned long long>(f.writer), static_cast<unsigned long long>(f.toModel),
                 f.refound ? ", found again by bytes" : "", static_cast<unsigned long long>(gate.record));
        text = b;
    } else if (!failed.empty()) {
        text = "refused: " + failed;
    } else {
        text = "checking (" + std::to_string(gate.passes) + " of " + std::to_string(gate.needed) + ")";
    }
    if (text.rfind("checking", 0) != 0 && text != stateText) {
        stateText = text;
        ++stateSerial;
    }
    auto& d = SpidyGameBodyData;
    d.state = gate.verified ? 2 : failed.empty() ? 1 : 3;
    d.passes = gate.passes;
    d.record = gate.record;
    d.writer = f.writer;
    d.toModel = f.toModel;
    d.refound = f.refound;
    d.rounds = rounds;
    strncpy_s(d.check, failed.c_str(), _TRUNCATE);
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&d.serial));
}
void run() {
    native_game_body::Found f;
    std::string staticFailed;
    bool staticDone{};
    uint32_t rounds{};
    std::unique_lock held(lock);
    while (!stopping) {
        held.unlock();
        if (!staticDone) {
            staticFailed = staticChecks(f);
            staticDone = true;
        }
        std::string failed = staticFailed;
        game_player::Player p{};
        if (failed.empty() && !game_player::find(base, p))
            failed = "registry (not exactly one hero, mover manager and mover)";
        held.lock();
        ++rounds;
        functions = f;
        gate.feed(p.record, failed.empty() ? nullptr : failed.c_str());
        verifiedRecord = gate.verified ? gate.record : 0;
        publish(f, failed, rounds);
        wake.wait_for(held, std::chrono::milliseconds(250), [] { return stopping; });
    }
    verifiedRecord = 0;
}
} // namespace

void native_game_body::begin(uintptr_t gameBase) {
    std::lock_guard held(lock);
    if (worker.joinable() || !gameBase)
        return;
    base = gameBase;
    stopping = false;
    gate = {};
    stateText = "checking";
    ++stateSerial;
    worker = std::thread(run);
}
void native_game_body::end() {
    {
        std::lock_guard held(lock);
        stopping = true;
    }
    wake.notify_all();
    if (worker.joinable())
        worker.join();
    std::lock_guard held(lock);
    verifiedRecord = 0;
    gate = {};
    if (stateText != "off") {
        stateText = "off";
        ++stateSerial;
    }
    SpidyGameBodyData.state = 0;
}
bool native_game_body::running() {
    std::lock_guard held(lock);
    return worker.joinable() && !stopping;
}
bool native_game_body::verified(uintptr_t record) {
    return record && verifiedRecord.load() == record;
}
native_game_body::Found native_game_body::found() {
    std::lock_guard held(lock);
    return functions;
}
std::string native_game_body::state(uint32_t& serial) {
    std::lock_guard held(lock);
    serial = stateSerial;
    return stateText;
}
