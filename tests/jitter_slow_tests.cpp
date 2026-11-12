#include "spidy/jitter_check.hpp"
#include "spidy/jitter_slow.hpp"
#include "spidy/webline_marker.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#define CHECK(c) do { if(!(c)) { std::fprintf(stderr, "FAILED %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while(0)
using namespace spidy;
int main() {
    CHECK(sizeof(jitter_slow::Data) == 624 && sizeof(webline_marker::Data) == 104);
    for (uint32_t level = jitter_check::low; level <= jitter_check::full; ++level) {
        const float amplitude = jitter_check::maxStoredPixels(level);
        float px = 0, py = 0, biggest = 0, step = 0, mx = 0, my = 0;
        for (uint64_t frame = 0; frame < 64; ++frame) {
            float x, y;
            jitter_slow::walk(frame, amplitude, x, y);
            biggest = std::fmax(biggest, std::hypot(x, y));
            CHECK(std::fabs(std::hypot(x, y) - amplitude) < 1e-5f); // on the circle
            if (frame) step = std::fmax(step, std::hypot(x - px, y - py));
            px = x, py = y;
            mx += x, my += y;
            // the same phase every 32 frames
            float x2, y2;
            jitter_slow::walk(frame + jitter_slow::steps, amplitude, x2, y2);
            CHECK(std::fabs(x - x2) < 1e-5f && std::fabs(y - y2) < 1e-5f);
        }
        CHECK(std::fabs(biggest - amplitude) < 1e-5f);
        // a job one frame behind is off by at most a fifth of the amplitude (the 8-phase Halton walk: up to a whole pixel)
        CHECK(step <= amplitude * 0.1965f && step >= amplitude * 0.1955f);
        CHECK(std::fabs(mx) < 1e-3f && std::fabs(my) < 1e-3f); // a zero-mean walk: no drift
    }
    CHECK(jitter_check::maxStoredPixels(jitter_check::off) == 0.f);
    CHECK(jitter_slow::bucket(0) == 0 && jitter_slow::bucket(2) == 2 && jitter_slow::bucket(9) == 3);
    std::puts("jitter slow tests ok");
    return 0;
}
