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
	if(DEFINED SHADER_OUTPUT_DIRECTORY)
		file(READ "${INPUT_FILE}" SHADER_TEXT)
		if(SHADER_PROFILE STREQUAL "gles300")
			# ES requires the version directive before even the copyright comment.
			string(REPLACE "#version 150" "" SHADER_TEXT "${SHADER_TEXT}")
			# Texture samples contain signed dB values, outside lowp's required range.
			string(PREPEND SHADER_TEXT "#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;\n")
		endif()
		file(MAKE_DIRECTORY "${SHADER_OUTPUT_DIRECTORY}")
		set(PREPARED_SHADER "${SHADER_OUTPUT_DIRECTORY}/${FILENAME}")
		file(WRITE "${PREPARED_SHADER}" "${SHADER_TEXT}")
		file(READ "${PREPARED_SHADER}" FILE_DATA HEX)
	else()
		file(READ "${INPUT_FILE}" FILE_DATA HEX)
	endif()
	string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," FILE_DATA "${FILE_DATA}")
	file(APPEND "${OUTPUT_FILE}"
		"inline constexpr unsigned char ${SYMBOL_NAME}[] = {${FILE_DATA}};\n"
		"inline constexpr unsigned ${SYMBOL_NAME}_size = sizeof(${SYMBOL_NAME});\n\n")
endforeach()
