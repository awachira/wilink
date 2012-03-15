# Macros for generating various icons from SVG.

include(CMakeParseArguments)

find_program(ICOTOOL icotool)
find_program(INKSCAPE inkscape)
find_program(PNGTOPNM pngtopnm)
find_program(PPMTOXPM ppmtoxpm)
find_program(PNG2ICNS png2icns)

macro(generate_name OUTPUT INPUT EXT)
	get_filename_component(_svg_name ${INPUT} NAME_WE)
	set(${OUTPUT} ${_svg_name}${EXT})
endmacro(generate_name)

# GENERATE_ICNS(OUTPUT SVGS..)
#
# Generate MacOS/X .icns icons from SVG files.
#
macro(generate_icns OUTPUT)
	set(OUTPUT "${OUTPUT}")
	foreach(_this_svg ${ARGN})
		generate_name(_this_icns ${_this_svg} .icns)
		add_custom_command(OUTPUT ${_this_icns} DEPENDS ${_this_svg}
			COMMAND ${INKSCAPE} -z --file=${CMAKE_CURRENT_SOURCE_DIR}/${_this_svg} --export-png=${_this_icns}.256 --export-width=256 --export-height=256
			COMMAND ${INKSCAPE} -z --file=${CMAKE_CURRENT_SOURCE_DIR}/${_this_svg} --export-png=${_this_icns}.128 --export-width=128 --export-height=128
			COMMAND ${INKSCAPE} -z --file=${CMAKE_CURRENT_SOURCE_DIR}/${_this_svg} --export-png=${_this_icns}.32 --export-width=32 --export-height=32
			COMMAND ${INKSCAPE} -z --file=${CMAKE_CURRENT_SOURCE_DIR}/${_this_svg} --export-png=${_this_icns}.16 --export-width=16 --export-height=16
			COMMAND ${PNG2ICNS} ${_this_icns} ${_this_icns}.256 ${_this_icns}.128 ${_this_icns}.32 ${_this_icns}.16
			COMMAND rm -f ${_this_icns}.256 ${_this_icns}.128 ${_this_icns}.32 ${_this_icns}.16)
		list(APPEND ${OUTPUT} ${_this_icns})
	endforeach()
endmacro(generate_icns)

# GENERATE_ICO(OUTPUT SVGS..)
#
# Generate Windows .ico icons from SVG files.
#
macro(generate_ico OUTPUT)
	set(OUTPUT "${OUTPUT}")
	foreach(_this_svg ${ARGN})
		generate_name(_this_ico ${_this_svg} .ico)
		add_custom_command(OUTPUT ${_this_ico} DEPENDS ${_this_svg}
			COMMAND ${INKSCAPE} -z --file=${CMAKE_CURRENT_SOURCE_DIR}/${_this_svg} --export-png=${_this_ico}.48 --export-width=48 --export-height=48
			COMMAND ${INKSCAPE} -z --file=${CMAKE_CURRENT_SOURCE_DIR}/${_this_svg} --export-png=${_this_ico}.32 --export-width=32 --export-height=32
			COMMAND ${INKSCAPE} -z --file=${CMAKE_CURRENT_SOURCE_DIR}/${_this_svg} --export-png=${_this_ico}.16 --export-width=16 --export-height=16
			COMMAND ${ICOTOOL} -c -o ${_this_ico} ${_this_ico}.48 ${_this_ico}.32 ${_this_ico}.16
			COMMAND rm -f ${_this_ico}.48 ${_this_ico}.32 ${_this_ico}.16)
		list(APPEND ${OUTPUT} ${_this_ico})
	endforeach()
endmacro(generate_ico)

# GENERATE_PNG(OUTPUT SVGS..)
#
# Generate PNG icons from SVG files.
#
macro(generate_png OUTPUT)
	set(OUTPUT "${OUTPUT}")
	cmake_parse_arguments(PNG "" "DESTINATION;SIZE;WIDTH;HEIGHT" "" ${ARGN})
	if(NOT PNG_SIZE)
		set(PNG_SIZE 32)
	endif()
    if(NOT PNG_WIDTH)
        set(PNG_WIDTH ${PNG_SIZE})
    endif()
    if(NOT PNG_HEIGHT)
        set(PNG_HEIGHT ${PNG_SIZE})
    endif()
	if(PNG_DESTINATION)
		file(MAKE_DIRECTORY ${PNG_DESTINATION})
	endif()
	foreach(_this_svg ${PNG_UNPARSED_ARGUMENTS})
		generate_name(_this_png ${_this_svg} .png)
		if(PNG_DESTINATION)
			set(_this_png ${PNG_DESTINATION}/${_this_png})
		endif()
		add_custom_command(OUTPUT ${_this_png} DEPENDS ${_this_svg}
			COMMAND ${INKSCAPE} -z --file=${CMAKE_CURRENT_SOURCE_DIR}/${_this_svg} --export-png=${_this_png} --export-width=${PNG_WIDTH} --export-height=${PNG_HEIGHT})
		list(APPEND ${OUTPUT} ${_this_png})
	endforeach()
endmacro(generate_png)

# GENERATE_XPM(OUTPUT SVGS..)
#
# Generate XPM icons from SVG files.
#
macro(generate_xpm OUTPUT)
	set(OUTPUT "${OUTPUT}")
	set(SIZE 32)
	foreach(_this_svg ${ARGN})
		generate_name(_this_xpm ${_this_svg} .xpm)
		get_filename_component(_this_xpm_title ${_this_svg} NAME_WE)
		string(REPLACE "-" "_" _this_xpm_title ${_this_xpm_title})
		set(_this_alpha ${_this_xpm}.tmp.alpha)
		set(_this_png ${_this_xpm}.tmp.png)
		add_custom_command(OUTPUT ${_this_xpm} DEPENDS ${_this_svg}
			COMMAND ${INKSCAPE} -z --file=${CMAKE_CURRENT_SOURCE_DIR}/${_this_svg} --export-png=${_this_png} --export-width=${SIZE} --export-height=${SIZE}
			COMMAND ${PNGTOPNM} -alpha ${_this_png} > ${_this_alpha}
			COMMAND ${PNGTOPNM} ${_this_png} | ${PPMTOXPM} -alpha ${_this_alpha} -name ${_this_xpm_title}_xpm | sed -e 's/static char/static const char/g' > ${_this_xpm}
			COMMAND rm -f ${_this_alpha} ${_this_png})
		list(APPEND ${OUTPUT} ${_this_xpm})
	endforeach()
endmacro(generate_xpm)

