// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Draws Update 14's wrist hologram (wrist_hud.hpp) without a headset, at
// about the headset's pixel density, over a sky: one sample a pixel with
// the overlay's translucent blending. Two views side by side:
//   left   the panel as shown over the left wrist, looking north
//   right  the same, the head turned 60 degrees right (the map turns; N
//          moves left) and the placeholder health at 40 percent
//   cells 3 and 4 (Update 15/16) the VR page (with the FPS readout, fed a
//          made-up run below, and RETICLES) and the GRAPHICS page
// Update 17: a second argument picks another sheet instead:
//   pages   every page at its defaults (STATUS, VR SETTINGS, GRAPHICS, HOLOGRAM), nothing pointed at
//   scroll  GRAPHICS scrolled halfway with the pointer on its scrollbar; VR SETTINGS at 150% text,
//           at the top and scrolled to its end; HOLOGRAM at 150% text halfway
//   themes  the VR SETTINGS page in each colour theme (Spider-Man, high contrast, deutan/protan,
//           tritan, monochrome)
// Update 18:
//   webstyle  VR SETTINGS scrolled to WEB STYLE and WEB COLOR: 90s CREAM, MESSY FROST, SYMBIOTE
//             CRIMSON SHEEN (pointing at WEB COLOR) and ENERGY GOLD
//
// Update 19:
//   music  the MUSIC page in six cells, one each visualizer (ORIGINAL, BARS with the song list
//          open and a row pointed at, FRACTAL BLOOM pointing at PLAY, TUNNEL paused at a
//          skipped-song note, STARBURST) and the page with no songs (the folder line), fed a
//          made-up song (a chord and a beat through music::Analyzer, not a recording); the last
//          cell scrolled to the end (VISUALIZER, the folder line, MUTE GAME MUSIC: COMING SOON)
//
// Update 36:
//   raimi  the "2000s Raimi" theme: GRAPHICS pointing at FAR SHADOWS (its name lit bright yellow),
//          and HOLOGRAM pointing at COLOR THEME (2000s RAIMI, with its subtext)
//
// Update 38.5:
//   subtext  a long subtext (175% TEXT SIZE) pointed at and scrolled in its rows: at the top (the
//            fade and the down arrow), mid-step (the crossfade), at its end (the up arrow), and at
//            its end in the 2000s Raimi theme's pixel font
//
//   remap    GESTURE REMAP on EXTRAS (a made-up motion, not a recording): take 1 RED, take 2 matched
//            BLUE, take 2 that didn't match (the reason), take 3 GREEN and saved, and the CONFLICTS
//            WITH popup (KEEP ANYWAY / TRY AGAIN / CANCEL)
//
// Version 0.0.2:
//   v002     STATUS (VERSION 0.0.2 top right), COMBAT's BALL LOOK OURS with AIM STRENGTH STRONG pointed
//            at (its subtext), and EXTRAS' TRAIN MOVE in TRAINING pointed at
//
//   spidy_hud_preview <path.bmp> [pages|scroll|themes|webstyle|music|raimi|subtext|remap|v002|heroes|holo]
//   spidy_hud_preview <path.bmp> v0062 <page>   (Update 39.0.2: a page, each section opened, the end)
#include "spidy/soft_shapes.hpp"
#include "spidy/music_player.hpp"
#include "spidy/wrist_hud.hpp"
#include "spidy/combat38.hpp"
#include "spidy/gesture_session.hpp"
#include "preview_image.hpp"
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>
using namespace spidy;
using namespace spidy::preview;

namespace {
void sky(Image& image, int x0, int size, int height) {
    for (int y = 0; y < height; ++y)
        for (int x = x0; x < x0 + size; ++x) {
            const float v = static_cast<float>(y) / static_cast<float>(height);
            float* o = image.at(x, y);
            // A daylight sky, paler toward the horizon, and haze below it.
            const bool below = v > .82f;
            o[0] = decode(below ? .62f : .38f + .32f * v);
            o[1] = decode(below ? .66f : .56f + .24f * v);
            o[2] = decode(below ? .70f : .86f + .08f * v);
        }
}
std::vector<Vertex> look(const std::vector<Vertex>& world, Vec3 eye, Vec3 target) {
    const Vec3 back = normalized(eye - target);
    const Vec3 right = normalized(cross(Vec3{0, 1, 0}, back));
    const Vec3 up = cross(back, right);
    auto out = world;
    for (auto& v : out) {
        const Vec3 d = v.position - eye;
        v.position = {dot(d, right), dot(d, up), dot(d, back)};
    }
    return out;
}
// The FPS readout from a made-up run, not a measurement: a 90 Hz headset for
// 6 s, a new eye pair on about two refreshes in three, dipping to one in two
// for a second; the eye passes 0.9 ms.
void madeUpRun(wrist_hud::View& view) {
    wrist_hud::FpsMeter meter;
    for (int i = 0; i < 540; ++i) {
        const bool dip = i >= 300 && i < 390;
        meter.frame(i / 90.0, dip ? i % 2 == 0 : i % 3 != 2, 1000.0 / 90);
    }
    view.fps = meter.fps();
    view.refreshHz = meter.refreshHz();
    view.fpsTrail = meter.trail();
    view.eyeGpuMs = .9f;
}
// The other hand's tip pointing at (u, v) on the panel.
void pointAt(wrist_hud::View& view, Vec3 eye, float u, float v) {
    view.pointU = u;
    view.pointV = v;
    const wrist_hud::Frame frame = wrist_hud::place(view, eye);
    const Vec3 toEye = normalized(eye - frame.centre);
    view.pointing = true;
    view.tip = frame.centre + frame.right * .16f - frame.up * .14f + toEye * .10f;
}
// Update 19: a made-up song's spectrum `seconds` in: a chord, a bass line and a beat.
music::Spectrum madeUpSong(float seconds) {
    music::Analyzer analyzer;
    constexpr int rate = 48000;
    std::vector<float> samples(rate / 30 * 2);
    music::Spectrum out;
    for (int block = 0; block < 30; ++block) {
        for (size_t i = 0; i < samples.size() / 2; ++i) {
            const double t = seconds + (block * samples.size() / 2 + i) / double(rate) - 1;
            const double beat = std::fmod(t, .5);
            double v = .18 * std::sin(2 * 3.14159265 * 220 * t) + .14 * std::sin(2 * 3.14159265 * 277.2 * t) +
                       .12 * std::sin(2 * 3.14159265 * 329.6 * t) + .10 * std::sin(2 * 3.14159265 * 1760 * t) +
                       .25 * std::sin(2 * 3.14159265 * (55 + 20 * std::floor(std::fmod(t, 2))) * t) +
                       .45 * std::exp(-beat * 18) * std::sin(2 * 3.14159265 * 60 * beat) +
                       .05 * std::sin(2 * 3.14159265 * 5200 * t) * std::exp(-std::fmod(t, .25) * 30);
            samples[2 * i] = samples[2 * i + 1] = static_cast<float>(v);
        }
        analyzer.push(samples.data(), samples.size() / 2, 2, rate);
        out = analyzer.update(1.f / 30);
    }
    return out;
}
// Update 39.0.2: a control's page with its section opened (a closed section's rows are not laid out).
int homeOf(wrist_hud::Control c) {
    const auto place = wrist_hud::placeOf(c);
    if (place.section >= 0)
        wrist_hud::setOpenSection(place.page, place.section);
    return place.page < 0 ? 0 : place.page;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: spidy_hud_preview <path.bmp> [pages|scroll|themes|webstyle|music|raimi]\n");
        return 2;
    }
    const std::string sheet = argc > 2 ? argv[2] : "";
    if (!sheet.empty() && sheet != "pages" && sheet != "scroll" && sheet != "themes" && sheet != "webstyle" &&
        sheet != "music" && sheet != "raimi" && sheet != "subtext" &&
        sheet != "remap" && sheet != "v002" && sheet != "heroes" && sheet != "holo" && sheet != "v0062") {
        std::fprintf(stderr, "unknown sheet %s (pages, scroll, themes, webstyle, music or raimi)\n", sheet.c_str());
        return 2;
    }
    constexpr float pixelAngle = .00065f;
    constexpr int size = 1100, height = 1400;
    // Update 39.0.2: "holo" - the eight pages at their defaults; "v0062 <page>" - one page at its default,
    // then each of its sections opened (scrolled to the header), then scrolled to the end.
    const int sheetPage = sheet == "v0062" && argc > 3 ? std::atoi(argv[3]) : 0;
    if (sheet == "v0062" && (sheetPage < 0 || sheetPage >= wrist_hud::pages)) {
        std::fprintf(stderr, "v0062 needs a page 0..%d\n", wrist_hud::pages - 1);
        return 2;
    }
    const int cells = sheet == "themes" ? wrist_hud::themes : sheet == "music" ? 6 : sheet == "raimi" ? 2 : sheet == "remap" ? 5 : sheet == "v002" ? 3 : sheet == "heroes" ? 5 : sheet == "holo" ? wrist_hud::pages : sheet == "v0062" ? wrist_hud::sectionCount(sheetPage) + 2 : 4; // subtext: 4
    Image image(size * cells, height); // Update 15: pages 2 and 3 in cells 2 and 3
    const Vec3 eye{0, 1.62f, 0};
    for (int cell = 0; cell < cells; ++cell) {
        const int x0 = cell * size;
        sky(image, x0, size, height);
        // The left forearm held across in front, the hand to the right.
        const Vec3 wristAt{-.03f, 1.24f, -.40f}, elbow{-.27f, 1.17f, -.30f};
        std::vector<Vertex> arm;
        soft::softBar(arm, elbow, wristAt + Vec3{.06f, .01f, -.01f}, normalized(wristAt - eye), .03f,
                      {.30f, .02f, .03f}, 1, .001f);
        soft::softBar(arm, elbow + Vec3{0, .02f, 0}, wristAt + Vec3{.04f, .03f, 0}, normalized(wristAt - eye),
                      .008f, {.02f, .04f, .25f}, 1, .001f);
        wrist_hud::View view;
        view.emitter = wristAt + Vec3{0, .03f, 0};
        view.seconds = .3f;
        wrist_hud::resetSections();
        if (sheet == "pages") {
            view.page = cell;
            madeUpRun(view);
        } else if (sheet == "holo") {
            view.page = cell;
            view.flyMode = 1;
            madeUpRun(view);
        } else if (sheet == "v0062") {
            view.page = sheetPage;
            view.flyMode = 1;
            madeUpRun(view);
            const int count = wrist_hud::sectionCount(sheetPage);
            if (cell >= 1 && cell <= count) {
                wrist_hud::setOpenSection(sheetPage, cell - 1);
                const auto header = wrist_hud::sectionControl(cell - 1);
                const float at = wrist_hud::controlSpot(sheetPage, header).v;
                view.scroll = wrist_hud::clampScroll(sheetPage, wrist_hud::viewTop - .02f - at);
                view.hover = header;
                const auto spot = wrist_hud::controlSpot(sheetPage, header, view.scroll);
                pointAt(view, eye, spot.u, spot.v);
            } else if (cell == count + 1) {
                view.scroll = wrist_hud::maxScroll(sheetPage, 1);
            }
        } else if (sheet == "scroll") {
            madeUpRun(view);
            const float k[4] = {1, 1.5f, 1.5f, 1.5f};
            const int page[4] = {wrist_hud::vrPage, wrist_hud::vrPage, wrist_hud::vrPage, wrist_hud::graphicsPage};
            view.page = page[cell];
            view.textScale = k[cell];
            view.settings.hudText = k[cell] * 100;
            const float most = wrist_hud::maxScroll(view.page, k[cell]);
            view.scroll = cell == 1 ? 0 : cell == 2 ? most : most / 2;
            if (cell == 0) // pointing at the scrollbar's thumb: it lights up
                pointAt(view, eye, (wrist_hud::scrollbarU0 + wrist_hud::scrollbarU1) / 2,
                        (wrist_hud::scrollThumb(view.page, view.scroll).v0 + wrist_hud::scrollThumb(view.page, view.scroll).v1) / 2);
        } else if (sheet == "webstyle") {
            madeUpRun(view);
            const int style[4] = {0, 1, 3, 4}, shade[4] = {0, 3, 2, 5};
            view.page = homeOf(wrist_hud::Control::webStyle);
            view.settings.webStyle = static_cast<float>(style[cell]);
            view.settings.webColors[style[cell]] = static_cast<float>(shade[cell]);
            const float at = wrist_hud::controlSpot(view.page, wrist_hud::Control::webStyle).v;
            view.scroll = wrist_hud::clampScroll(view.page, wrist_hud::viewTop - .03f - at);
            if (cell == 2) {
                view.hover = wrist_hud::Control::webColor;
                const auto spot = wrist_hud::controlSpot(view.page, view.hover, view.scroll);
                pointAt(view, eye, spot.u + .02f, spot.v);
            }
        } else if (sheet == "music") {
            const char* songs[] = {"01 Web Head Anthem", "02 Queens Rooftops", "03 Swing Low Over Harlem",
                                   "04 Symbiote Static", "05 Night Patrol (Extended Mix)", "06 Brooklyn Bridge Run",
                                   "07 Venom In The Wires", "08 Daily Bugle Breakdown", "09 Fractal Skyline"};
            constexpr int count = 9;
            view.page = wrist_hud::musicPage;
            view.settings.musicVolume = 70;
            view.settings.musicVisualizer = static_cast<float>(std::min(cell, music::visualizers - 1));
            auto& m = view.music;
            m.folder = "C:\\sm2-port\\Music";
            if (cell < 5) {
                m.tracks = count;
                m.track = cell == 1 ? 4 : cell * 2 % count;
                m.song = songs[m.track];
                m.playing = cell != 3;
                m.paused = cell == 3;
                m.total = 180 + 23.f * cell;
                m.elapsed = 37 + 29.f * cell;
                m.spectrum = madeUpSong(3 + 1.37f * cell);
                if (cell == 3)
                    m.note = "SKIPPED: 04 broken download.mp3";
            }
            if (cell == 5) // scrolled to the end: the picker, the folder line and MUTE GAME MUSIC
                view.scroll = wrist_hud::maxScroll(wrist_hud::musicPage, 1);
            if (cell == 1) {
                m.listOpen = true;
                m.listFirst = 2;
                m.listHover = 4;
                for (int i = 0; i < wrist_hud::listRows && m.listFirst + i < count; ++i)
                    m.rows.push_back(songs[m.listFirst + i]);
                const auto spot = wrist_hud::controlSpot(wrist_hud::musicPage, wrist_hud::Control::musicTrack);
                for (float v = spot.v; v > spot.v - .2f; v -= .001f)
                    if (wrist_hud::listRowAt(spot.u, v, 0, 1, m.listFirst, count) == m.listHover - m.listFirst) {
                        pointAt(view, eye, spot.u, v - wrist_hud::listRowHeight() / 2);
                        break;
                    }
                view.hover = wrist_hud::Control::musicPick;
            } else if (cell == 2) {
                view.hover = wrist_hud::Control::musicPlay;
                const auto spot = wrist_hud::controlSpot(wrist_hud::musicPage, view.hover);
                pointAt(view, eye, spot.u, spot.v);
            }
        } else if (sheet == "raimi") {
            madeUpRun(view);
            view.theme = wrist_hud::raimiTheme;
            view.settings.hudTheme = static_cast<float>(wrist_hud::raimiTheme);
            view.hover = cell == 0 ? wrist_hud::Control::farShadows : wrist_hud::Control::hudTheme;
            view.page = homeOf(view.hover);
            const auto spot = wrist_hud::controlSpot(view.page, view.hover);
            pointAt(view, eye, spot.u, spot.v);
        } else if (sheet == "subtext") {
            madeUpRun(view);
            constexpr float k = 1.75f;
            view.textScale = k;
            view.settings.hudText = k * 100;
            // The first long subtext on VR SETTINGS, GRAPHICS or the rest, scrolled near the top of the view.
            for (int p = 1; p < wrist_hud::pages && view.helpFor == wrist_hud::Control::none; ++p)
                for (int c = 0; c < wrist_hud::controls; ++c)
                    if (wrist_hud::helpOverflow(view, p, static_cast<wrist_hud::Control>(c)) > 0) {
                        view.page = p;
                        view.helpFor = static_cast<wrist_hud::Control>(c);
                        break;
                    }
            const int over = wrist_hud::helpOverflow(view, view.page, view.helpFor);
            const float at = wrist_hud::controlSpot(view.page, view.helpFor, 0, k).v;
            view.scroll = wrist_hud::clampScroll(view.page, wrist_hud::viewTop - .03f - at, k);
            view.hover = view.helpFor;
            const float scrolls[4] = {0, .5f, static_cast<float>(over), static_cast<float>(over)};
            view.helpScroll = scrolls[cell];
            if (cell == 3) {
                view.theme = wrist_hud::raimiTheme;
                view.settings.hudTheme = static_cast<float>(wrist_hud::raimiTheme);
            }
            const auto spot = wrist_hud::controlSpot(view.page, view.hover, view.scroll, k);
            pointAt(view, eye, spot.u, spot.v);
            std::printf("subtext: page %d control %d, %d lines over, scroll %.2f\n", view.page,
                        static_cast<int>(view.helpFor), over, static_cast<double>(view.helpScroll));
        } else if (sheet == "remap") {
            using gesture_remap::Step;
            static gesture_session::View states[5];
            gesture_session::View& r = states[cell];
            r.move = cell == 4 ? gestures385::abilityLunge : gestures385::abilityPunch;
            r.trailPoints = gesture_session::points;
            for (int i = 0; i < gesture_session::points; ++i) { // a made-up punch: the right hand out and up
                const float t = static_cast<float>(i) / (gesture_session::points - 1);
                const float wobble = cell == 2 ? .35f * std::sin(t * 9) : .03f * static_cast<float>(cell);
                r.u[0][i] = -.45f;
                r.v[0][i] = -.35f;
                r.u[1][i] = .15f + .85f * t + (cell == 2 ? .0f : .02f * static_cast<float>(cell));
                r.v[1][i] = -.3f + .55f * t * t + wobble;
            }
            const Step step[5] = {Step::take2, Step::take3, Step::take2, Step::saved, Step::conflict};
            const uint8_t trail[5] = {1, 2, 1, 3, 2};
            r.step = static_cast<uint8_t>(step[cell]);
            r.trail = trail[cell];
            r.stepText = gesture_remap::stepName(step[cell]);
            r.recording = cell < 3;
            r.reasonText = cell == 2 ? gesture_remap::reasonText(gesture_remap::direction) : "";
            r.popup = cell == 4;
            r.conflictName = cell == 4 ? "WINGS BOOST" : "";
            r.customMask = cell == 3 ? 1u : 0u;
            view.remap = &r;
            view.page = homeOf(wrist_hud::Control::gestureRecord);
            view.hover = cell == 4 ? wrist_hud::Control::gestureKeep : wrist_hud::Control::gestureRecord;
            const float at = wrist_hud::controlSpot(view.page, wrist_hud::Control::gestureRecord).v;
            view.scroll = wrist_hud::clampScroll(view.page, wrist_hud::viewTop - .03f - at);
            const auto spot = wrist_hud::controlSpot(view.page, view.hover, view.scroll);
            pointAt(view, eye, spot.u, spot.v);
        } else if (sheet == "v002") {
            view.gestureMoves = gestures385::modeTraining;
            view.trainMove = "ABILITY PUNCH";
            if (cell > 0) {
                view.hover = cell == 1 ? wrist_hud::Control::combatAimStrength : wrist_hud::Control::trainMove;
                view.page = homeOf(view.hover);
                const float at = wrist_hud::controlSpot(view.page, view.hover).v;
                view.scroll = wrist_hud::clampScroll(view.page, wrist_hud::viewTop - .10f - at);
                const auto spot = wrist_hud::controlSpot(view.page, view.hover, view.scroll);
                pointAt(view, eye, spot.u, spot.v);
            }
        } else if (sheet == "heroes") {
            // Update 39.0.2 (5 cells): POWERS with FLYING, then each hero profile opened - IRON MAN is the
            // hero being played (its profile ACTIVE), the pointer on the profile's header.
            view.page = wrist_hud::powersPage;
            view.combat38[combat38::shooterType] = 1;
            view.flyMode = 1;
            madeUpRun(view);
            wrist_hud::setOpenSection(view.page, cell);
            const auto header = wrist_hud::sectionControl(cell);
            const float at = wrist_hud::controlSpot(view.page, header).v;
            view.scroll = wrist_hud::clampScroll(view.page, wrist_hud::viewTop - .02f - at);
            view.hover = header;
            const auto spot = wrist_hud::controlSpot(view.page, header, view.scroll);
            pointAt(view, eye, spot.u, spot.v);
        } else if (sheet == "themes") {
            view.page = wrist_hud::vrPage;
            view.theme = cell;
            view.settings.hudTheme = static_cast<float>(cell);
            madeUpRun(view);
        } else {
            if (cell == 1) {
                view.heading = 3.14159265f / 3;
                view.data.health = .4f;
                view.data.gadgets[1].ammo = 2;
            }
            if (cell >= 2) {
                // The other hand's tip points at the panel: page 2 hovering the web thickness slider,
                // page 3 dragging the building reach with a next-launch supersample chosen (restart notice).
                view.page = homeOf(cell == 2 ? wrist_hud::Control::thickness : wrist_hud::Control::reach);
                view.settings.widthCm = 2.1f;
                view.settings.speed = 240;
                view.settings.aa = 1;
                view.settings.history = 30;
                view.settings.reach = 220;
                view.settings.monitor = 15;
                view.settings.supersample = 150;
                view.settings.sharpen = 30;
                view.settings.reticles = 1;
                madeUpRun(view);
                view.hover = cell == 2 ? wrist_hud::Control::thickness : wrist_hud::Control::reach;
                view.held = cell == 2 ? wrist_hud::Control::none : wrist_hud::Control::reach;
                const auto spot = wrist_hud::controlSpot(view.page, view.hover);
                pointAt(view, eye, spot.u + (cell == 2 ? .010f : .025f), spot.v);
            }
        }
        std::vector<Vertex> hud;
        wrist_hud::appendWristHud(hud, view, eye, pixelAngle);
        const Vec3 target = view.emitter + Vec3{0, .16f, 0};
        const Camera camera{1 / pixelAngle, size * .5f, height * .5f};
        draw(image, look(arm, eye, target), camera, 0, 0, x0, 0, size, height);
        draw(image, look(hud, eye, target), camera, 0, 0, x0, 0, size, height);
        std::printf("cell %d: %zu vertices\n", cell, hud.size());
    }
    return save(image, argv[1]) ? 0 : 1;
}
