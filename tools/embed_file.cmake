# Writes OUT, a C++ file holding IN's bytes as spidy::calibration_art::atlasFile
# (Update 36). Run as: cmake -DIN=... -DOUT=... -DNAME=... -P embed_file.cmake
# Version 0.0.5: -DSYMBOL=... names the array (default atlasFile; its size is SYMBOL then Size).
if(NOT DEFINED SYMBOL OR "${SYMBOL}" STREQUAL "")
  set(SYMBOL atlasFile)
endif()
file(READ "${IN}" hex HEX)
string(LENGTH "${hex}" digits)
math(EXPR bytes "${digits} / 2")
string(REGEX REPLACE "([0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f])" "\\1\n" hex "${hex}")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," hex "${hex}")
file(WRITE "${OUT}.tmp" "// Generated from ${IN} by tools/embed_file.cmake; do not edit.\n#include <cstddef>\n#include <cstdint>\nnamespace spidy::${NAME} {\nextern const std::uint8_t ${SYMBOL}[];\nextern const std::size_t ${SYMBOL}Size;\nconst std::uint8_t ${SYMBOL}[] = {\n${hex}0};\nconst std::size_t ${SYMBOL}Size = ${bytes};\n}\n")
file(RENAME "${OUT}.tmp" "${OUT}")
