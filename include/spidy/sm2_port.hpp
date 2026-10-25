// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
#pragma once
namespace spidy::sm2 {
// The game this build is for: Spider-Man2.exe, version 2.810.0.0.
inline constexpr const wchar_t* gameModule = L"Spider-Man2.exe";
// Features whose Spider-Man 2 addresses are not known yet start as "not
// ported" and report the code below instead of hooking anything (SM2TODO).
// A volatile flag rather than a constant: the original code stays compiled
// and type-checked, and nothing warns that it cannot be reached.
inline volatile bool notPorted = true;
// Codes the stubs report (one per feature, in that feature's own range).
inline constexpr unsigned menuNotPorted = 9399, promptsNotPorted = 9699, timeNotPorted = 9799,
                          steamNotPorted = 9899, websNotPorted = 6099, bodyNotPorted = 7099,
                          hudNotPorted = 9599, bodiesNotPorted = 8099, gameplayNotPorted = 9990;
} // namespace spidy::sm2
