# HERO VR 0.0.76 SM1: Mulder's Heroes add-on for iilyn's Spider-Man VR mod (Spidy by Ilya Mezerowsky, GPL-3.0,
# https://github.com/IlyaMez/SpidyVR). Separate files: nothing here builds into or changes his mod.
# sm1_scan_core.cpp: the research scan's pure part (anchors, joint hash, RTTI names), tested here.
add_library(herovr_sm1_core STATIC sm1_core.cpp sm1_scan_core.cpp sm1_combat.cpp sm1_grapple.cpp sm1_holo.cpp)
# spidy_core: the SM2 build's own code reused as is (pixel_font, the outfits, and the wrist click: wrist_hud::Toggle).
target_link_libraries(herovr_sm1_core PUBLIC spidy_core)
target_include_directories(herovr_sm1_core PUBLIC "${PROJECT_SOURCE_DIR}/include")
if(MSVC)
  target_compile_options(herovr_sm1_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(herovr_sm1_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_executable(herovr_sm1_tests "${PROJECT_SOURCE_DIR}/tests/sm1_addon_tests.cpp")
target_link_libraries(herovr_sm1_tests PRIVATE herovr_sm1_core)
add_test(NAME sm1_addon COMMAND herovr_sm1_tests)
if(WIN32)
  # Beside the other modules in build\windows-ninja (tools\package.ps1 ships it when present).
  add_library(herovr_sm1_addon SHARED sm1_addon_dll.cpp)
  target_compile_definitions(herovr_sm1_addon PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
  if(MSVC)
    target_compile_options(herovr_sm1_addon PRIVATE /W4 /permissive-)
  endif()
  target_link_libraries(herovr_sm1_addon PRIVATE herovr_sm1_core version)
  set_target_properties(herovr_sm1_addon PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
                        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
# 0.0.76 SM1 layer: an OpenXR implicit API layer the launcher registers only while SM1 plays. It reads input after his
# xrSyncActions, adds the wrist menu and beams as extra quads on his frame (his projection layers untouched), and
# drives flight through his own exported SpidyMotionSubmit. Needs MinHook (configure with SPIDY_BUILD_OBSERVER, as CI).
if(WIN32 AND TARGET minhook)
  # sm1_scan.cpp: the read-only research scan (live.txt sm1scan), on its own lowest-priority thread.
  add_library(herovr_sm1_layer SHARED sm1_layer.cpp sm1_scan.cpp sm1_targets.cpp)
  target_include_directories(herovr_sm1_layer PRIVATE "${PROJECT_SOURCE_DIR}/include"
                             "${PROJECT_SOURCE_DIR}/third_party/OpenXR-SDK/include")
  target_compile_definitions(herovr_sm1_layer PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS XR_USE_PLATFORM_WIN32
                             XR_USE_GRAPHICS_API_D3D12)
  if(MSVC)
    target_compile_options(herovr_sm1_layer PRIVATE /W4 /permissive-)
  endif()
  set_property(TARGET herovr_sm1_layer PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  target_link_libraries(herovr_sm1_layer PRIVATE herovr_sm1_core minhook d3d12 version windowscodecs ole32)
  set_target_properties(herovr_sm1_layer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
                        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
  configure_file(herovr_sm1_layer.json "${CMAKE_BINARY_DIR}/herovr_sm1_layer.json" COPYONLY)
elseif(WIN32)
  message(STATUS "herovr_sm1_layer skipped: MinHook is only fetched with SPIDY_BUILD_OBSERVER (tools/build.ps1 -Observer)")
endif()
