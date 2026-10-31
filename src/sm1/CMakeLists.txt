# HERO VR 0.0.75 SM1: Mulder's Heroes add-on for iilyn's Spider-Man VR mod (Spidy by Ilya Mezerowsky, GPL-3.0,
# https://github.com/IlyaMez/SpidyVR). Separate files: nothing here builds into or changes his mod.
add_library(herovr_sm1_core STATIC sm1_core.cpp)
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
  target_compile_definitions(herovr_sm1_addon PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  if(MSVC)
    target_compile_options(herovr_sm1_addon PRIVATE /W4 /permissive-)
  endif()
  target_link_libraries(herovr_sm1_addon PRIVATE herovr_sm1_core version)
  set_target_properties(herovr_sm1_addon PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
                        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
