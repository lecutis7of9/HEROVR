cmake_minimum_required(VERSION 3.24)
project(Spidy VERSION 0.0.73 LANGUAGES C CXX)
# Every target on Windows: windows.h must never define min/max macros (they break std::min/std::max, MSVC C2589).
if(WIN32)
  add_compile_definitions(NOMINMAX)
endif()
# The public version (semver, docs/VERSIONING.md): the launcher, the zip, the tag and VERSION all follow
# this line; PORT-VERSION keeps the internal build ("Update 39.6"). Version 0.0.73 is build 39.6 (the 0.0.73 test 1-6 fixes: hero play, claws, THOR, IRON MAN, the van, the picture; GAME TAA off by default; no MP SOLO TEST; launcher build 14). Version 0.0.72 is build 39.5 (the heroes' new models and suits, THOR, IRON MAN, test builds 1-12; launcher build 13). Version 0.0.71 is 0.0.70's build 39.4 unchanged plus the launcher fix for VR not starting after Stop VR ("handle is invalid" at finding the headset; launcher build 12). Version 0.0.70 ("0.0.7"; the trailing 0 makes every launcher see it as newer than 0.0.63) is build 39.4 (the heroes: CAPTAIN AMERICA, BATMAN, WOLVERINE, SUPERMAN and PUNISHER with their weapon models, the MIRROR VIEW fix and launcher build 11). Version 0.0.63 was build 39.3 (0.0.62's mod unchanged; the new opt-in HERO VR launcher design, launcher build 10, and Revert to 0.0.62). Version 0.0.62 was build 39.2 (0.0.6 + ghost-hand fix, bolt walls, baton hits, instant punches, live enemy tracking, hit indicator, hand flips, lens flares). Version 0.0.6 was build 39.1.1 (39.0 + the hero pages, then the holo sections and the new repulsor look, then the headset recovery, then the frame rate work, then the 0.0.6 hotfix eye swapchain retry; 0.0.5 was 38.9, 0.0.4 38.8, 0.0.3 38.7, 0.0.2 38.6, 0.0.1 38.5).
add_compile_definitions(SPIDY_PUBLIC_VERSION="${PROJECT_VERSION}")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/EHsc> /utf-8 $<$<COMPILE_LANGUAGE:CXX>:/bigobj>)
endif()
# Update 24: Microsoft's C++ runtime linked into every module and program (/MT), so a player needs no Visual
# C++ Redistributable. The modules run inside the game, which may already hold an older msvcp140.dll (before
# 14.40 the /MD build crashed on its first lock); /MT never uses it. Safe here: our modules call each other only
# through DWORD(void*) exports on plain data, never passing CRT objects (strings, heap blocks, FILE*) across.
# bHaptics' prebuilt library keeps its own (VCRUNTIME140, which the game itself loads).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# SM2 VR: this tree is the port of SpidyVR to Marvel's Spider-Man 2 (Spider-Man2.exe 2.810.0.0).
add_compile_definitions(SM2_PORT=1)
option(SPIDY_BUILD_XR "Build the Windows OpenXR / D3D12 swing lab" ${WIN32})
# 0.0.72: a test / experimental build shows the TEST BUILD NOTICE after calibration (test_notice.hpp).
# Turn it OFF for a public release.
option(HERO_TEST_BUILD "Test build: show the test-version notice after calibration" ON) # 0.0.74 test 1 (private test build): ON; the public 0.0.73 release: OFF
option(SPIDY_BUILD_OBSERVER "Build the opt-in camera diagnostic DLL" OFF)
option(SPIDY_BUILD_LAUNCHER "Build the Spidy launcher window (Dear ImGui from tools/bootstrap.ps1)" ${SPIDY_BUILD_XR})

# Stamp a shipped Windows binary with version metadata (CompanyName, ProductName, FileVersion...)
# via apps/spidy_module.rc. Unsigned, metadata-less modules that attach to the running game are
# what antivirus heuristics flag; the resource is also how a vendor identifies a false-positive
# report. Signing (tools/build.ps1, when a certificate is configured) builds on top of this.
function(spidy_stamp_version target description)
  if(NOT WIN32)
    return()
  endif()
  get_target_property(kind ${target} TYPE)
  if(kind STREQUAL "EXECUTABLE")
    set(filetype VFT_APP)
  else()
    set(filetype VFT_DLL)
  endif()
  target_sources(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/apps/spidy_module.rc")
  target_compile_definitions(${target} PRIVATE
                             SPIDY_VERSION="${PROJECT_VERSION}"
                             SPIDY_VERSION_NUMBERS=${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0
                             SPIDY_MODULE_DESC="${description}"
                             SPIDY_MODULE_FILE="$<TARGET_FILE_NAME:${target}>"
                             SPIDY_MODULE_FILETYPE=${filetype})
endfunction()

add_library(spidy_core src/swing.cpp src/web_grab.cpp src/lab_world.cpp src/lab_props.cpp src/game_tracking.cpp
            src/web_visual.cpp src/body_ik.cpp src/punch.cpp src/shooter.cpp src/vr_settings.cpp
            src/body_calibration.cpp src/overlay_text.cpp src/pixel_font.cpp src/slow_motion.cpp src/web_strike.cpp
            src/impact_sound.cpp src/prompt_buttons.cpp src/gadget_wheel.cpp src/spider_sense.cpp src/phone.cpp
            src/body_haptics.cpp src/tech_marks.cpp src/wrist_hud.cpp src/music_player.cpp
            src/calibration_art.cpp src/calibration_panel.cpp src/holo_calibration.cpp src/health_ring.cpp src/sfx.cpp
            src/hero007_visual.cpp src/van_fps.cpp
            src/hero072_visual.cpp
            src/game_body.cpp
            src/mesh_model.cpp
            src/suit_wear.cpp
            src/test_notice.cpp
            src/van_screen.cpp
            src/hideout074.cpp
            src/batcave074.cpp
            src/crime_gps074.cpp
            src/batmobile074.cpp
            src/roster_page.cpp src/holo_roster.cpp
            src/mp_net.cpp src/mp_socket.cpp src/mp_session.cpp src/mp_ghost.cpp src/mp_body.cpp) # 0.0.73 MP SOLO TEST (+ MPSUIT PLAYER 2 BODY): the UDP pipe
target_include_directories(spidy_core PUBLIC include)
# 0.0.73 MP SOLO TEST: the network thread (std::thread) and Winsock.
find_package(Threads REQUIRED)
target_link_libraries(spidy_core PUBLIC Threads::Threads)
if(WIN32)
    target_link_libraries(spidy_core PUBLIC ws2_32)
endif()
if(HERO_TEST_BUILD)
    target_compile_definitions(spidy_core PUBLIC SPIDY_TEST_BUILD=1)
endif()
# Build 5: the start line names the build ("HERO VR 0.0.72 test <commit>"), so a session's console says
# which code ran (build 4 ran from an old folder and its console still said 0.0.70). HERO_TEST_COMMIT
# overrides; else the checkout's short hash; else "unknown".
set(HERO_TEST_COMMIT "" CACHE STRING "Commit named by the test build's start line (empty: git rev-parse)")
set(_hero_commit "${HERO_TEST_COMMIT}")
if(NOT _hero_commit)
    find_package(Git QUIET)
    if(GIT_FOUND)
        execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short=7 HEAD WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                        OUTPUT_VARIABLE _hero_commit OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    endif()
endif()
if(NOT _hero_commit MATCHES "^[0-9A-Za-z._-]+$")
    set(_hero_commit "unknown")
endif()
target_compile_definitions(spidy_core PUBLIC SPIDY_TEST_COMMIT="${_hero_commit}")
# Test build 11: the wrist's bottom line and the start line say "TEST <n> <commit>" (build 10's log
# showed build 9 still running; the number makes that plain). Bump per test build.
# 0.0.73 test 4's console still said "HERO VR 0.0.72 TEST 3 a20b9ee" (export-test4): the version was
# a literal and the number was not bumped. Both are here now; bump them per test build.
set(HERO_TEST_NUMBER "8" CACHE STRING "Test build number on the wrist (TEST <n> <commit>)")
if(NOT HERO_TEST_NUMBER MATCHES "^[0-9]+$")
    set(HERO_TEST_NUMBER "0")
endif()
target_compile_definitions(spidy_core PUBLIC SPIDY_TEST_NUMBER="${HERO_TEST_NUMBER}")
set(HERO_TEST_VERSION "0.0.74" CACHE STRING "Version the test build's start line, notice and wrist name")
if(NOT HERO_TEST_VERSION MATCHES "^[0-9][0-9.]*$")
    set(HERO_TEST_VERSION "0.0.74")
endif()
target_compile_definitions(spidy_core PUBLIC SPIDY_TEST_VERSION="${HERO_TEST_VERSION}")
# Update 36: the calibration panel's art atlas, compiled in as bytes
# (tools/calibration_art/make_atlas.py makes the file).
set(SPIDY_CALIBRATION_ART "${CMAKE_CURRENT_SOURCE_DIR}/assets/calibration_art.qoi")
set(SPIDY_CALIBRATION_ART_CPP "${CMAKE_CURRENT_BINARY_DIR}/generated/calibration_art_data.cpp")
add_custom_command(OUTPUT "${SPIDY_CALIBRATION_ART_CPP}"
                   COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
                   COMMAND "${CMAKE_COMMAND}" "-DIN=${SPIDY_CALIBRATION_ART}" "-DOUT=${SPIDY_CALIBRATION_ART_CPP}"
                           -DNAME=calibration_art -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_file.cmake"
                   DEPENDS "${SPIDY_CALIBRATION_ART}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_file.cmake"
                   COMMENT "Embedding the calibration art")
target_sources(spidy_core PRIVATE "${SPIDY_CALIBRATION_ART_CPP}")
# Version 0.0.5 SOUND EFFECTS: the mod's own WAVs (tools/sfx/make_sfx.py), compiled in as bytes
# (spidy::sfx_data::<name>, src/sfx.cpp).
# Version 0.0.6 (Update 39.0.2) REPULSOR: the blast's sound is one of three takes (make_sfx.py): a CLASSIC,
# b HEAVY, c TIGHT. Switching is this one line; the take's _charge plays on the press, _fire at the blast.
# Version 0.0.62: "user", the player's own sound (assets/sfx/source/user-repulsor.mp3, tools/sfx/user_repulsor.py).
set(SPIDY_REPULSOR_TAKE user)
set(SPIDY_SFX_FILE_repulsor_fire "repulsor_mcu_${SPIDY_REPULSOR_TAKE}_fire")
set(SPIDY_SFX_FILE_repulsor_charge "repulsor_mcu_${SPIDY_REPULSOR_TAKE}_charge")
# Version 0.0.72: the Punisher's guns use the player's own sounds (assets/sfx/source/user-*.mp3, decoded
# unchanged by tools/sfx/user_guns.py): the sawed-off, both pistols, and the pistols' reload (pistol_reload).
set(SPIDY_SFX_FILE_shotgun_blast "shotgun_blast_user")
set(SPIDY_SFX_FILE_pistol_shot "pistol_shot_user")
# Version 0.0.73: the PUNISHER VAN's engine, the player's own recording split on its waveform (tools/sfx/van_engine_0073.py).
set(SPIDY_SFX_FILE_van_engine "van_engine_loop")
# Version 0.0.7 HERO PROFILES: nine more (tools/sfx/make_sfx_007.py); CAPTAIN AMERICA: the four shield sounds.
foreach(SPIDY_SFX_NAME sonic_boom repulsor_fire chest_beam beam_ready baton_whoosh baton_hit repulsor_charge
        pistol_shot shotgun_blast dry_click mag_drop mag_seat holster claw_snikt heat_vision freeze_breath
        shield_throw shield_ricochet shield_catch shield_clang pistol_reload
        thor_lightning_1 thor_lightning_2 # 0.0.72: THOR's lightning, the player's own (user_guns.py)
        van_start van_engine van_door_slam) # 0.0.73: the PUNISHER VAN's slam + start-up, its engine loop, the slam (van_engine_0073.py)
  if(DEFINED SPIDY_SFX_FILE_${SPIDY_SFX_NAME})
    set(SPIDY_SFX_IN "${CMAKE_CURRENT_SOURCE_DIR}/assets/sfx/${SPIDY_SFX_FILE_${SPIDY_SFX_NAME}}.wav")
  else()
    set(SPIDY_SFX_IN "${CMAKE_CURRENT_SOURCE_DIR}/assets/sfx/${SPIDY_SFX_NAME}.wav")
  endif()
  set(SPIDY_SFX_CPP "${CMAKE_CURRENT_BINARY_DIR}/generated/sfx_${SPIDY_SFX_NAME}_data.cpp")
  add_custom_command(OUTPUT "${SPIDY_SFX_CPP}"
                     COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
                     COMMAND "${CMAKE_COMMAND}" "-DIN=${SPIDY_SFX_IN}" "-DOUT=${SPIDY_SFX_CPP}" -DNAME=sfx_data
                             "-DSYMBOL=${SPIDY_SFX_NAME}" -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_file.cmake"
                     DEPENDS "${SPIDY_SFX_IN}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_file.cmake"
                     COMMENT "Embedding the sound ${SPIDY_SFX_NAME}")
  target_sources(spidy_core PRIVATE "${SPIDY_SFX_CPP}")
endforeach()
if(MSVC)
  target_compile_options(spidy_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(spidy_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
enable_testing()
add_executable(spidy_realistic_flip_tests tests/realistic_flip_tests.cpp)
target_include_directories(spidy_realistic_flip_tests PRIVATE include)
add_test(NAME realistic_flips COMMAND spidy_realistic_flip_tests)
add_executable(spidy_tests tests/core_tests.cpp)
if(WIN32)
  target_compile_definitions(spidy_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
target_sources(spidy_tests PRIVATE src/sfx_quiet.cpp) # Version 0.0.5: SOUND EFFECTS without audio
target_compile_definitions(spidy_tests PRIVATE SPIDY_REPULSOR_TAKE_${SPIDY_REPULSOR_TAKE}=1) # 39.0.2: the take's lengths
target_link_libraries(spidy_tests PRIVATE spidy_core)
# Version 0.0.72 MODEL CHECK: dumps what the overlay gets of every shipped model (tests/hero072_models_tests.py).
add_executable(spidy_model_dump tools/models072/model_dump.cpp src/sfx_quiet.cpp)
target_link_libraries(spidy_model_dump PRIVATE spidy_core)
add_executable(spidy_suit_bench tools/suits/suit_bench.cpp src/sfx_quiet.cpp)
target_link_libraries(spidy_suit_bench PRIVATE spidy_core)
add_executable(spidy_suit_hands tools/suits/suit_hands.cpp src/sfx_quiet.cpp) # Test build 12 HANDS
target_link_libraries(spidy_suit_hands PRIVATE spidy_core)
if(WIN32)
  # The game target scan's class rules, tried on the test's own classes.
  target_sources(spidy_tests PRIVATE src/game_targets.cpp)
endif()
add_test(NAME core COMMAND spidy_tests)
# 0.0.73 MP SOLO TEST: the packet, interpolation, echo delay and handshake (tests/mp_tests.cpp); the probe
# runs the real session for tests/mp_buddy_tests.py (the Python Test Buddy against the C++ game side).
add_executable(spidy_mp_tests tests/mp_tests.cpp)
target_link_libraries(spidy_mp_tests PRIVATE spidy_core)
add_test(NAME mp COMMAND spidy_mp_tests)
add_executable(spidy_mp_probe apps/mp_probe.cpp)
target_link_libraries(spidy_mp_probe PRIVATE spidy_core)
add_executable(spidy_mp_preview apps/mp_preview.cpp src/sfx_quiet.cpp) # 0.0.73 MP SOLO TEST: desktop previews
target_link_libraries(spidy_mp_preview PRIVATE spidy_core)
# Version 0.0.72 GAME BODY (EXPERIMENTAL): the multiplayer team's spec offline, a synthetic 237-joint rig.
add_executable(spidy_game_body_tests tests/game_body_tests.cpp src/sfx_quiet.cpp)
target_link_libraries(spidy_game_body_tests PRIVATE spidy_core)
add_test(NAME game_body COMMAND spidy_game_body_tests)
# Update 38.2: the fixes replayed on the recorded sessions (tests/replay/*.csv).
add_executable(spidy_replay_tests tests/replay_tests.cpp)
if(WIN32)
  target_compile_definitions(spidy_replay_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
target_sources(spidy_replay_tests PRIVATE src/sfx_quiet.cpp)
target_link_libraries(spidy_replay_tests PRIVATE spidy_core)
target_compile_definitions(spidy_replay_tests PRIVATE SPIDY_REPLAY_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/replay")
add_test(NAME replay COMMAND spidy_replay_tests)
# Update 38.4: the MIRROR PROBE's logic (read-only, cheap); --dump writes a filled record for the parser.
add_executable(spidy_mirror_probe_tests tests/mirror_probe_tests.cpp)
target_include_directories(spidy_mirror_probe_tests PRIVATE include)
add_test(NAME mirror_probe COMMAND spidy_mirror_probe_tests)
# Settings profiles (launcher_profile.hpp) hold the SPIDY VR tab's settings: the launcher's programs build
# vr_settings themselves, as the launcher has the static runtime and spidy_core the DLL one.
add_executable(spidy_launcher_tests tests/launcher_tests.cpp src/vr_settings.cpp)
target_include_directories(spidy_launcher_tests PRIVATE include)
add_test(NAME launcher COMMAND spidy_launcher_tests)
add_executable(spidy_sim apps/sim.cpp)
target_link_libraries(spidy_sim PRIVATE spidy_core)
# Draws the combat marks over eye captures without a headset (tools/preview-marks.ps1).
add_executable(spidy_mark_preview apps/mark_preview.cpp)
target_link_libraries(spidy_mark_preview PRIVATE spidy_core)
add_executable(spidy_web_preview apps/web_preview.cpp)
target_link_libraries(spidy_web_preview PRIVATE spidy_core)
add_executable(spidy_calibration_preview apps/calibration_preview.cpp)
target_link_libraries(spidy_calibration_preview PRIVATE spidy_core)
add_executable(spidy_health_ring_preview apps/health_ring_preview.cpp)
target_link_libraries(spidy_health_ring_preview PRIVATE spidy_core)
add_executable(spidy_hud_preview apps/hud_preview.cpp)
target_link_libraries(spidy_hud_preview PRIVATE spidy_core)
# Version 0.0.2: TRAINING's path guide and the OURS web ball (docs/images/guide-002-sheet.png).
add_executable(spidy_guide_preview apps/guide_preview.cpp)
target_link_libraries(spidy_guide_preview PRIVATE spidy_core)
add_executable(spidy_repulsor_preview apps/repulsor_preview.cpp)
target_link_libraries(spidy_repulsor_preview PRIVATE spidy_core)
add_executable(spidy_beam_ring_preview apps/beam_ring_preview.cpp) # Version 0.0.5
target_link_libraries(spidy_beam_ring_preview PRIVATE spidy_core)
add_executable(spidy_supersonic_preview apps/supersonic_preview.cpp) # Version 0.0.5
target_link_libraries(spidy_supersonic_preview PRIVATE spidy_core)
add_executable(spidy_hero007_preview apps/hero007_preview.cpp) # Version 0.0.7
target_link_libraries(spidy_hero007_preview PRIVATE spidy_core)
add_executable(spidy_hero072b_preview apps/hero072b_preview.cpp) # Version 0.0.72 FIX ROUND
target_link_libraries(spidy_hero072b_preview PRIVATE spidy_core)
add_executable(spidy_van_screen_preview apps/van_screen_preview.cpp) # Version 0.0.73 test 3 VAN SCREEN
target_link_libraries(spidy_van_screen_preview PRIVATE spidy_core)
add_executable(spidy_batcave_preview apps/batcave_preview.cpp) # 0.0.74 test 2b BATCAVE PREVIEW
target_link_libraries(spidy_batcave_preview PRIVATE spidy_core)
add_executable(spidy_dd_baton_preview apps/dd_baton_preview.cpp src/sfx_quiet.cpp) # Version 0.0.73 test 3 DD BATON
target_link_libraries(spidy_dd_baton_preview PRIVATE spidy_core)
add_executable(spidy_hero_hands_preview apps/hero_hands_preview.cpp src/sfx_quiet.cpp) # 0.0.73 HERO HANDS
target_link_libraries(spidy_hero_hands_preview PRIVATE spidy_core)
add_executable(spidy_phone_preview apps/phone_preview.cpp)
target_link_libraries(spidy_phone_preview PRIVATE spidy_core)
if(SPIDY_BUILD_OBSERVER)
  if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The game observer requires 64-bit Windows")
  endif()
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/minhook/CMakeLists.txt")
    message(FATAL_ERROR "Run tools/bootstrap.ps1 -Observer for the pinned MinHook source")
  endif()
  # SM2: MinHook 1.3.4's HDE64 refuses the game's AVX (VEX) hook entries (Input bridge start: 1208).
  file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/minhook/src/hde/hde64.c" spidy_minhook_vex REGEX "VEX")
  if(NOT spidy_minhook_vex)
    message(FATAL_ERROR "third_party/minhook is MinHook 1.3.4, which cannot hook Spider-Man 2's AVX entries. "
                        "Run tools/bootstrap.ps1 -Observer again: it updates MinHook to the pinned revision.")
  endif()
  add_subdirectory(third_party/minhook EXCLUDE_FROM_ALL)
  # Update 24: MinHook's CMakeLists predates CMP0091; static runtime as everything else.
  set_property(TARGET minhook PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  add_library(spidy_observer SHARED src/game_observer.cpp)
  target_compile_definitions(spidy_observer PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(spidy_observer PRIVATE /W4 /permissive-)
  target_link_libraries(spidy_observer PRIVATE minhook)
  add_library(spidy_bridge SHARED src/game_bridge.cpp)
  target_include_directories(spidy_bridge PRIVATE include)
  target_compile_definitions(spidy_bridge PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(spidy_bridge PRIVATE /W4 /permissive-)
  target_link_libraries(spidy_bridge PRIVATE minhook)
  add_library(spidy_view SHARED src/game_view.cpp)
  target_include_directories(spidy_view PRIVATE include)
  target_compile_definitions(spidy_view PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(spidy_view PRIVATE /W4 /permissive-)
  target_link_libraries(spidy_view PRIVATE minhook)
  add_library(spidy_stereo_probe SHARED src/stereo_probe.cpp src/native_eye_gpu.cpp src/world_trace.cpp src/npc_fade.cpp src/native_webs.cpp
              src/native_body.cpp src/native_hud.cpp src/native_address_finder.cpp
              src/native_shadow_distance.cpp src/wrist_muzzle.cpp src/mirror_probe.cpp)
  target_include_directories(spidy_stereo_probe PRIVATE include)
  target_compile_definitions(spidy_stereo_probe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(spidy_stereo_probe PRIVATE /W4 /permissive-)
  target_link_libraries(spidy_stereo_probe PRIVATE minhook d3d12 d3dcompiler winmm spidy_core)
  add_library(spidy_render_probe SHARED src/render_probe.cpp)
  target_compile_definitions(spidy_render_probe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(spidy_render_probe PRIVATE /W4 /permissive-)
  target_link_libraries(spidy_render_probe PRIVATE minhook d3d12 dxgi)
  target_include_directories(spidy_render_probe PRIVATE include)
  add_library(spidy_eye_capture SHARED src/eye_capture.cpp)
  target_compile_definitions(spidy_eye_capture PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(spidy_eye_capture PRIVATE /W4 /permissive-)
  target_link_libraries(spidy_eye_capture PRIVATE minhook d3d12)
  add_library(spidy_scene_trace SHARED src/scene_trace.cpp)
  target_compile_definitions(spidy_scene_trace PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(spidy_scene_trace PRIVATE /W4 /permissive-)
  target_link_libraries(spidy_scene_trace PRIVATE minhook)
  add_library(spidy_camera_watch SHARED src/camera_watch.cpp)
  target_compile_definitions(spidy_camera_watch PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(spidy_camera_watch PRIVATE /W4 /permissive-)
  add_library(spidy_collision_probe SHARED src/collision_probe.cpp)
  target_include_directories(spidy_collision_probe PRIVATE include)
  target_compile_definitions(spidy_collision_probe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(spidy_collision_probe PRIVATE /W4 /permissive-)
  target_link_libraries(spidy_collision_probe PRIVATE minhook)
  add_library(spidy_ray_bridge SHARED src/native_rays.cpp src/game_swing.cpp src/game_targets.cpp
              src/game_grab.cpp src/native_bodies.cpp src/game_punch.cpp src/game_shooter.cpp
              src/game_warp.cpp src/game_offers.cpp src/game_sense.cpp)
  target_include_directories(spidy_ray_bridge PRIVATE include)
  target_compile_definitions(spidy_ray_bridge PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(spidy_ray_bridge PRIVATE /W4 /permissive-)
  target_link_libraries(spidy_ray_bridge PRIVATE minhook spidy_core)
  add_library(spidy_movement_probe SHARED src/movement_probe.cpp)
  target_include_directories(spidy_movement_probe PRIVATE include)
  target_compile_definitions(spidy_movement_probe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(spidy_movement_probe PRIVATE /W4 /permissive-)
  target_link_libraries(spidy_movement_probe PRIVATE minhook)
  add_library(spidy_movement_bridge SHARED src/native_movement.cpp)
  target_include_directories(spidy_movement_bridge PRIVATE include)
  target_compile_definitions(spidy_movement_bridge PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(spidy_movement_bridge PRIVATE /W4 /permissive-)
  target_link_libraries(spidy_movement_bridge PRIVATE minhook)
  # Loaded while the game starts, before its renderer exists; keep it small.
  add_library(spidy_render_memory SHARED src/native_render_memory.cpp src/native_mip_bias.cpp)
  target_include_directories(spidy_render_memory PRIVATE include)
  target_compile_definitions(spidy_render_memory PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(spidy_render_memory PRIVATE /W4 /permissive-)
  target_link_libraries(spidy_render_memory PRIVATE minhook)
  # Version-stamp the modules the player package ships (package.ps1's $modules).
  spidy_stamp_version(spidy_bridge "Spidy VR game bridge")
  spidy_stamp_version(spidy_render_probe "Spidy VR render probe")
  spidy_stamp_version(spidy_ray_bridge "Spidy VR input bridge")
  spidy_stamp_version(spidy_movement_bridge "Spidy VR movement bridge")
  spidy_stamp_version(spidy_stereo_probe "Spidy VR stereo renderer")
  spidy_stamp_version(spidy_render_memory "Spidy VR render-memory helper")
endif()
if(SPIDY_BUILD_XR)
  if(NOT WIN32)
    message(FATAL_ERROR "The XR renderer currently requires Windows D3D12")
  endif()
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/OpenXR-SDK/CMakeLists.txt")
    message(FATAL_ERROR "Run tools/bootstrap.ps1 to fetch the pinned OpenXR SDK")
  endif()
  set(BUILD_API_LAYERS OFF CACHE BOOL "" FORCE)
  set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(BUILD_CONFORMANCE_TESTS OFF CACHE BOOL "" FORCE)
  set(BUILD_WITH_SYSTEM_JSONCPP OFF CACHE BOOL "" FORCE)
  set(DYNAMIC_LOADER OFF CACHE BOOL "" FORCE)
  add_subdirectory(third_party/OpenXR-SDK EXCLUDE_FROM_ALL)
  # Update 24: the static loader asks for the DLL runtime (/MD); it is linked into our /MT modules.
  set_property(TARGET openxr_loader PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  add_library(spidy_d3d12 src/d3d12_renderer.cpp)
  target_link_libraries(spidy_d3d12 PUBLIC spidy_core d3d12 dxgi d3dcompiler)
  target_compile_definitions(spidy_d3d12 PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
  add_library(spidy_openxr src/xr_session.cpp)
  target_link_libraries(spidy_openxr PUBLIC spidy_d3d12 openxr_loader)
  target_compile_definitions(spidy_openxr PUBLIC XR_USE_PLATFORM_WIN32 XR_USE_GRAPHICS_API_D3D12)
  # Body haptics on bHaptics devices (bhaptics_output.hpp) with bHaptics' own SDK: its header names the
  # functions, and its library, beside the modules, is loaded at run time. The Developer Portal app they play as
  # comes from the environment (the release workflow's repository secrets), else from bhaptics-app.local
  # (git-ignored; app_id=... and api_key=... lines); a build without one plays no body haptics.
  set(SPIDY_BHAPTICS_SDK "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tact-cpp2/sdk")
  if(NOT EXISTS "${SPIDY_BHAPTICS_SDK}/include/bhaptics/bhaptics.h")
    message(FATAL_ERROR "Run tools/bootstrap.ps1 to fetch the pinned bHaptics SDK")
  endif()
  set(SPIDY_BHAPTICS_APP_ID "$ENV{SPIDY_BHAPTICS_APP_ID}")
  set(SPIDY_BHAPTICS_API_KEY "$ENV{SPIDY_BHAPTICS_API_KEY}")
  set(bhaptics_app "${CMAKE_CURRENT_SOURCE_DIR}/bhaptics-app.local")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${bhaptics_app}")
  if(EXISTS "${bhaptics_app}" AND (NOT SPIDY_BHAPTICS_APP_ID OR NOT SPIDY_BHAPTICS_API_KEY))
    file(STRINGS "${bhaptics_app}" bhaptics_lines)
    foreach(line IN LISTS bhaptics_lines)
      if(line MATCHES "^[ \t]*app_id[ \t]*=[ \t]*([^ \t]+)")
        set(SPIDY_BHAPTICS_APP_ID "${CMAKE_MATCH_1}")
      elseif(line MATCHES "^[ \t]*api_key[ \t]*=[ \t]*([^ \t]+)")
        set(SPIDY_BHAPTICS_API_KEY "${CMAKE_MATCH_1}")
      endif()
    endforeach()
  endif()
  foreach(value IN ITEMS "${SPIDY_BHAPTICS_APP_ID}" "${SPIDY_BHAPTICS_API_KEY}")
    if(NOT value MATCHES "^[A-Za-z0-9_-]*$")
      message(FATAL_ERROR "A bHaptics App ID or API key has more than letters, digits, - and _ in it")
    endif()
  endforeach()
  if(SPIDY_BHAPTICS_APP_ID AND SPIDY_BHAPTICS_API_KEY)
    message(STATUS "bHaptics app: ${SPIDY_BHAPTICS_APP_ID}")
  else()
    message(STATUS "No bHaptics app (SPIDY_BHAPTICS_APP_ID, SPIDY_BHAPTICS_API_KEY): no body haptics in this build")
  endif()
  add_library(spidy_bhaptics STATIC src/bhaptics_output.cpp)
  target_include_directories(spidy_bhaptics PRIVATE "${SPIDY_BHAPTICS_SDK}/include")
  target_link_libraries(spidy_bhaptics PUBLIC spidy_core)
  target_compile_definitions(spidy_bhaptics PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX
                             SPIDY_BHAPTICS_APP_ID="${SPIDY_BHAPTICS_APP_ID}"
                             SPIDY_BHAPTICS_API_KEY="${SPIDY_BHAPTICS_API_KEY}")
  # The SDK's library beside the modules: the test loads it from there, and a VR session puts a copy beside
  # the stereo module it stages (run_game_vr.py).
  add_custom_command(TARGET spidy_bhaptics POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SPIDY_BHAPTICS_SDK}/lib/x64/bhaptics_library.dll"
                             "${CMAKE_BINARY_DIR}")
  add_executable(spidy_haptics_test apps/haptics_test.cpp)
  target_link_libraries(spidy_haptics_test PRIVATE spidy_bhaptics)
  target_compile_definitions(spidy_haptics_test PRIVATE _CRT_SECURE_NO_WARNINGS) # getenv
  spidy_stamp_version(spidy_haptics_test "Spidy VR haptics test")
  if(SPIDY_BUILD_OBSERVER)
    target_sources(spidy_stereo_probe PRIVATE src/game_xr.cpp src/game_player.cpp src/native_game_body.cpp src/game_pad.cpp src/sm2_damage.cpp
                   src/game_menu.cpp src/game_time.cpp src/game_prompts.cpp src/game_steam.cpp
                   src/phone_music.cpp src/music_audio_win.cpp src/sfx_win.cpp
                   src/game_duck_win.cpp) # 0.0.74 test 3 CAVE QUIET
    # Update 19: the MUSIC page's player (Media Foundation decodes, WASAPI plays).
    target_link_libraries(spidy_stereo_probe PRIVATE spidy_openxr spidy_bhaptics shell32 ole32 uuid mfplat
                          mfreadwrite mfuuid windowscodecs)
    # Update 22: Media Foundation delay-loaded, so the module (and VR) still loads on Windows N/KN without the
    # Media Feature Pack; music_audio_win.cpp checks for both libraries before it calls one.
    target_link_options(spidy_stereo_probe PRIVATE /DELAYLOAD:mfplat.dll /DELAYLOAD:mfreadwrite.dll)
    target_link_libraries(spidy_stereo_probe PRIVATE delayimp)
  endif()
  add_executable(spidy_xr_lab apps/xr_lab.cpp)
  target_link_libraries(spidy_xr_lab PRIVATE spidy_openxr)
  add_executable(spidy_headset_probe apps/xr_headset_probe.cpp)
  target_link_libraries(spidy_headset_probe PRIVATE openxr_loader)
  spidy_stamp_version(spidy_headset_probe "Spidy VR headset probe")
  add_executable(spidy_xr_swapchain_probe apps/xr_swapchain_probe.cpp)
  target_link_libraries(spidy_xr_swapchain_probe PRIVATE spidy_openxr)
  add_executable(spidy_graphics_test tests/graphics_test.cpp)
  target_link_libraries(spidy_graphics_test PRIVATE spidy_d3d12 spidy_core)
  foreach(target spidy_d3d12 spidy_openxr spidy_xr_lab spidy_graphics_test spidy_bhaptics spidy_haptics_test)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
  endforeach()
endif()
if(SPIDY_BUILD_LAUNCHER)
  if(NOT WIN32)
    message(FATAL_ERROR "The Spidy launcher requires Windows")
  endif()
  set(SPIDY_IMGUI "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")
  if(NOT EXISTS "${SPIDY_IMGUI}/imgui.h")
    message(FATAL_ERROR "Run tools/bootstrap.ps1 to fetch the pinned Dear ImGui")
  endif()
  add_executable(spidy_launcher WIN32 apps/launcher/main.cpp apps/launcher/ui.cpp apps/launcher/system.cpp
                 apps/launcher/update.cpp src/vr_settings.cpp apps/launcher/launcher.rc
                 apps/launcher/launcher.manifest
                 ${SPIDY_IMGUI}/imgui.cpp ${SPIDY_IMGUI}/imgui_draw.cpp ${SPIDY_IMGUI}/imgui_tables.cpp
                 ${SPIDY_IMGUI}/imgui_widgets.cpp ${SPIDY_IMGUI}/backends/imgui_impl_win32.cpp
                 ${SPIDY_IMGUI}/backends/imgui_impl_dx11.cpp)
  target_include_directories(spidy_launcher PRIVATE include ${SPIDY_IMGUI} ${SPIDY_IMGUI}/backends)
  target_compile_definitions(spidy_launcher PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
                             SPIDY_VERSION="${PROJECT_VERSION}"
                             SPIDY_VERSION_NUMBERS=${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0)
  set_source_files_properties(apps/launcher/main.cpp apps/launcher/ui.cpp apps/launcher/system.cpp
                              apps/launcher/update.cpp PROPERTIES COMPILE_OPTIONS "/W4;/permissive-")
  # The resource script compiles the fonts and the controls picture in; a changed one builds it again.
  file(GLOB SPIDY_LAUNCHER_FONTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/apps/launcher/fonts/*.ttf")
  set_source_files_properties(apps/launcher/launcher.rc PROPERTIES OBJECT_DEPENDS
                              "${SPIDY_LAUNCHER_FONTS};${CMAKE_CURRENT_SOURCE_DIR}/docs/images/readme/controls-quest3.jpg")
  target_link_libraries(spidy_launcher PRIVATE d3d11 dxgi d3dcompiler dwmapi bcrypt version urlmon wintrust crypt32
                        shell32 ole32 uuid advapi32 winhttp windowscodecs)
  # Static runtime (Update 24: as every module now; CMAKE_MSVC_RUNTIME_LIBRARY above).
  set_property(TARGET spidy_launcher PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
