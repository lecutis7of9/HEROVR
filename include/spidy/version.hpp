#pragma once
// The public version players see ("Version 0.0.5"), semver, from project(Spidy VERSION) in CMakeLists.txt
// (VERSION beside it says the same; tests/version_tests.py). The internal build (PORT-VERSION, "Update
// 38.9") is kept for the changelog and the reports. docs/VERSIONING.md: the next builds are 0.0.5, 0.0.6...
namespace spidy::version {
#ifndef SPIDY_PUBLIC_VERSION
#define SPIDY_PUBLIC_VERSION "0.0.5" // a build without CMake's definition (the strict checks)
#endif
inline constexpr const char* number = SPIDY_PUBLIC_VERSION;
inline constexpr const char* statusLine = "VERSION " SPIDY_PUBLIC_VERSION; // top right of the hologram's STATUS page
inline constexpr const char* build = "38.9"; // PORT-VERSION's "Update 38.9"
} // namespace spidy::version
