#pragma once
// The public version players see ("Version 0.0.6"), semver, from project(Spidy VERSION) in CMakeLists.txt
// (VERSION beside it says the same; tests/version_tests.py). The internal build (PORT-VERSION, "Update
// 39.0") is kept for the changelog and the reports. docs/VERSIONING.md: the next builds are 0.0.6, 0.0.7...
namespace spidy::version {
#ifndef SPIDY_PUBLIC_VERSION
#define SPIDY_PUBLIC_VERSION "0.0.6" // a build without CMake's definition (the strict checks)
#endif
inline constexpr const char* number = SPIDY_PUBLIC_VERSION;
inline constexpr const char* statusLine = "VERSION " SPIDY_PUBLIC_VERSION; // top right of the hologram's STATUS page
// Version 0.0.6: the on-screen credit (STATUS under the version, the console banner, the start overlay):
// this build's maker only. SpidyVR's author is credited in LICENSE, CREDITS.md and the source headers.
inline constexpr const char* madeBy = "MADE BY AGENT MULDER";
inline constexpr const char* madeByText = "Made by Agent Mulder";
inline constexpr const char* build = "39.1"; // PORT-VERSION's "Update 39.1" (39.0 + the hero pages, the holo sections, the repulsor look, the headset recovery, the frame rate work)
} // namespace spidy::version
