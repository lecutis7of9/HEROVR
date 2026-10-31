#pragma once
// The public version players see ("Version 0.0.6"), semver, from project(Spidy VERSION) in CMakeLists.txt
// (VERSION beside it says the same; tests/version_tests.py). The internal build (PORT-VERSION, "Update
// 39.0") is kept for the changelog and the reports. docs/VERSIONING.md: the next builds are 0.0.6, 0.0.7...
namespace spidy::version {
#ifndef SPIDY_PUBLIC_VERSION
#define SPIDY_PUBLIC_VERSION "0.0.74" // a build without CMake's definition (the strict checks)
#endif
inline constexpr const char* number = SPIDY_PUBLIC_VERSION;
inline constexpr const char* statusLine = "VERSION " SPIDY_PUBLIC_VERSION; // top right of the hologram's STATUS page
// Version 0.0.6: the on-screen credit (STATUS under the version, the console banner, the start overlay):
// this build's maker only. SpidyVR's author is credited in LICENSE, CREDITS.md and the source headers.
inline constexpr const char* madeBy = "MADE BY AGENT MULDER";
inline constexpr const char* madeByText = "Made by Agent Mulder";
inline constexpr const char* build = "39.7"; // PORT-VERSION's "Update 39.7" (Version 0.0.74: the 0.0.74 test 1-10 work, SM1STYLE eye AA, the window-edge lights fix, the multiplayer server list, launcher build 15; Version 0.0.73: the 0.0.73 test 1-6 fixes, GAME TAA off by default, no MP SOLO TEST, launcher build 14; Version 0.0.72: the 0.0.72 heroes, models and suits, launcher build 13; Version 0.0.71: 0.0.70's 39.4 unchanged + the Stop VR "handle is invalid" fix, launcher build 12; Version 0.0.70, "0.0.7": the heroes, their models, the MIRROR VIEW fix, launcher build 11; 0.0.63 was 39.3: 0.0.62's mod unchanged, the new opt-in launcher design; 0.0.62 was 39.2: 0.0.6 + the ghost-hand, wall, baton and combat fixes; 0.0.6 was 39.1.1 (39.0 + the hero pages, the holo sections, the repulsor look, the headset recovery, the frame rate work; .1: the eye swapchain retry hotfix)
} // namespace spidy::version
