# Embed a list of binary resources into a generated C++ header.

if(NOT DEFINED OUTPUT_FILE)
	message(FATAL_ERROR "OUTPUT_FILE is required")
endif()

if(NOT DEFINED INPUT_FILES)
	message(FATAL_ERROR "INPUT_FILES is required")
endif()

get_filename_component(OUTPUT_DIRECTORY "${OUTPUT_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${OUTPUT_DIRECTORY}")
file(WRITE "${OUTPUT_FILE}" "#pragma once\n\n")

foreach(INPUT_FILE IN LISTS INPUT_FILES)
	get_filename_component(FILENAME "${INPUT_FILE}" NAME)
	string(REGEX REPLACE "\\.| |-" "_" SYMBOL_NAME "${FILENAME}")
	file(READ "${INPUT_FILE}" FILE_DATA HEX)
	string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," FILE_DATA "${FILE_DATA}")
	file(APPEND "${OUTPUT_FILE}"
		"inline constexpr unsigned char ${SYMBOL_NAME}[] = {${FILE_DATA}};\n"
		"inline constexpr unsigned ${SYMBOL_NAME}_size = sizeof(${SYMBOL_NAME});\n\n")
endforeach()
