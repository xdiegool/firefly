# Main CMake file for project Firefly.
# Modeline {{{
# vi: foldmarker={{{,}}} foldmethod=marker foldlevel=0
# }}}

## General {{{
cmake_minimum_required (VERSION 2.8)
project (Firefly C)
if(NOT CMAKE_CROSSCOMPILING)
	enable_testing()
endif(NOT CMAKE_CROSSCOMPILING)

# Helper function to join a list into a string:
function(JOIN VALUES OUTPUT)
	string(REGEX REPLACE ";" " " _TMP_STR ${VALUES})
	set(${OUTPUT} "${_TMP_STR}" PARENT_SCOPE)
endfunction()
## }}}

## DIRS {{{
# Firefly directories:
set(Firefly_PROJECT_DIR ${Firefly_SOURCE_DIR}/..)
set(GEN_DIR ${Firefly_PROJECT_DIR}/gen)
set(LC_DIR ${Firefly_PROJECT_DIR}/lc)
set(INC_DIR ${Firefly_PROJECT_DIR}/include)
set(TEST_FILES_DIR ${Firefly_PROJECT_DIR}/testfiles)

# Documentation directories:
set(DOC_DIR ${Firefly_PROJECT_DIR}/doc)
set(DOC_GEN_DIR ${DOC_DIR}/gen)
set(DOC_GEN_API_DIR ${DOC_GEN_DIR}/api)
Set(DOC_GEN_FULL_DIR ${DOC_GEN_DIR}/full)

# Labcomm compiler:
set(LABCOMMC ${Firefly_PROJECT_DIR}/tools/labComm.jar)

# Tell CMake to output library and executables directly in the build
# directory:
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${Firefly_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${Firefly_BINARY_DIR})

# Append our own CMake modules path:
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${Firefly_PROJECT_DIR}/cmake_modules)

# Tell CMake that we prefer static libraries before dynamic ones:
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a;.so")
## }}}

## FIND LIBRARIES {{{
message("-- Looking for Xenomai")
find_package(Xenomai)
message("-- Looking for RTNet")
find_package(RTNet)
message("-- Looking for Labcomm")
find_package(LabComm)

# Add Labcomm lib dir.
link_directories(${LINK_DIRECTORIES} ${LABCOMM_LIBRARIES})
## }}}

## FLAGS {{{
set(C_STD "-std=c99")
set(ERR_FLAGS "-Wall -Wextra")
set(MACROS_DEF "-DDEBUG")

if(CMAKE_CROSSCOMPILING)
	if (VXWORKS_COMPILING)
		# CFLAGS when cross compiling to the ARM-card:
		set(CMAKE_C_FLAGS
			"${C_STD} "
			"${ERR_FLAGS} "
			"${MACROS_DEF} "
			"-DLABCOMM_COMPAT=\\\"labcomm_compat_vxworks.h\\\" "
		)
		# Join the list stuff above to a single string:
		JOIN("${CMAKE_C_FLAGS}" CMAKE_C_FLAGS)
	else()
		# CFLAGS when cross compiling to the ARM-card:
		set(CMAKE_C_FLAGS
			"${C_STD} "
			"${ERR_FLAGS} -Wfloat-equal -Werror-implicit-function-declaration "
			"-mthumb -mcpu=cortex-m3 -T../ft-sense/src/adc_freertos_lwip/standalone.ld"
			"-ffunction-sections -fdata-sections "
			"${MACROS_DEF} -DUART_BUFFERED -Dsprintf=usprintf "
			"-Dsnprintf=usnprintf -Dvsnprintf=uvsnprintf -Dprintf=uipprintf "
			"-DFIREFLY_MALLOC=pvPortMalloc -Dcalloc=pvPortCalloc -DFIREFLY_FREE=vPortFree "
			"-DARM_CORTEXM3_CODESOURCERY -DLABCOMM_NO_STDIO "
			"-DLABCOMM_COMPAT=\\\"labcomm_compat_arm_cortexm3.h\\\" "
			"-DGCC_ARMCM3=1"
		)
		# Join the list stuff above to a single string:
		JOIN("${CMAKE_C_FLAGS}" CMAKE_C_FLAGS)

	endif()
else()
	# CFLAGS when compiling normally:
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_STD} ${ERR_FLAGS} ${MACROS_DEF} -g")
endif(CMAKE_CROSSCOMPILING)
## }}}

## CUSTOM COMMANDS & TARGETS {{{
# Custom command to create the directory for the generated Labcomm
# files, it depends upon the Labcomm compiler and library to make sure
# they are generated before trying to generate files with Labcomm.
add_custom_command(
	OUTPUT ${GEN_DIR}
	COMMAND mkdir -p ${GEN_DIR}
)
# Custom commands to generate files from .lc files:
add_custom_command(
	OUTPUT ${GEN_DIR}/firefly_protocol.c ${GEN_DIR}/firefly_protocol.h
	DEPENDS ${LC_DIR}/firefly_protocol.lc ${GEN_DIR}
	COMMAND java -jar ${LABCOMMC} --c=${GEN_DIR}/firefly_protocol.c --h=${GEN_DIR}/firefly_protocol.h ${LC_DIR}/firefly_protocol.lc
)
add_custom_command(
	OUTPUT ${GEN_DIR}/pingpong.c ${GEN_DIR}/pingpong.h
	DEPENDS ${LC_DIR}/pingpong.lc ${GEN_DIR}
	COMMAND java -jar ${LABCOMMC} --c=${GEN_DIR}/pingpong.c --h=${GEN_DIR}/pingpong.h ${LC_DIR}/pingpong.lc
)
add_custom_command(
	OUTPUT ${GEN_DIR}/test.c ${GEN_DIR}/test.h
	DEPENDS ${LC_DIR}/test.lc ${GEN_DIR}
	COMMAND java -jar ${LABCOMMC} --c=${GEN_DIR}/test.c --h=${GEN_DIR}/test.h ${LC_DIR}/test.lc
)

# Create a library of the generated Labcomm files to make it easy to
# include in executables:
add_library(gen-files
	${Firefly_PROJECT_DIR}/gen/test.c
	${Firefly_PROJECT_DIR}/gen/firefly_protocol.c
	${Firefly_PROJECT_DIR}/gen/pingpong.c
)

# Tell CMake that the files above are generated at compile time,
# otherwise CMake complains about missing files when generating
# build files:
set_source_files_properties(${Firefly_PROJECT_DIR}/gen/test.c PROPERTIES GENERATED TRUE)
set_source_files_properties(${Firefly_PROJECT_DIR}/gen/firefly_protocol.c PROPERTIES GENERATED TRUE)
set_source_files_properties(${Firefly_PROJECT_DIR}/gen/pingpong.c PROPERTIES GENERATED TRUE)
# Remove warnings when compiling Labcomm generated code since that's not
# our fault:
set_target_properties(gen-files
	PROPERTIES COMPILE_FLAGS "-Wno-all -Wno-extra"
)

# Custom commands to build tags files for vim/emacs.
add_custom_target(tags
	DEPENDS tags-vim tags-emacs
)
add_custom_target(tags-vim
	COMMAND ctags -R --tag-relative=yes -f ../tags ..
)
add_custom_target(tags-emacs
	COMMAND ctags --recurse --tag-relative=yes -e -f ../TAGS ..
)

# Custom command to clean more than clean:
add_custom_target(cleaner
	COMMAND rm -fr ${Firefly_PROJECT_DIR}/build/* &&
			rm -fr ${TEST_FILES_DIR} &&
)


# Documentation targets {{{
add_custom_target(docs
	DEPENDS docs-full docs-api
)

# Custom target to create full documentation (takes FOREVER to run):
add_custom_target(docs-full
	COMMAND doxygen ${DOC_GEN_DIR}/doxygen_full.cfg
	DEPENDS ${DOC_GEN_DIR}/doxygen_full.cfg
	WORKING_DIRECTORY ${Firefly_PROJECT_DIR}
)

# Custom target to create API documentation:
add_custom_target(docs-api
	COMMAND doxygen ${DOC_GEN_DIR}/doxygen_api.cfg
	DEPENDS ${DOC_GEN_DIR}/doxygen_api.cfg
	WORKING_DIRECTORY ${Firefly_PROJECT_DIR}
)

# Custom target to build the conf file needed by docs-full above:
add_custom_command(
	OUTPUT ${DOC_GEN_DIR}/doxygen_full.cfg
	DEPENDS ${DOC_GEN_DIR}/index.dox
	COMMAND mkdir ${DOC_GEN_FULL_DIR} &&
			cp ${DOC_DIR}/doxygen_template.cfg ${DOC_GEN_DIR}/doxygen_full.cfg &&
			echo \"INPUT = doc/gen/index.dox include src\" >> ${DOC_GEN_DIR}/doxygen_full.cfg &&
			echo \"OUTPUT_DIRECTORY = \\"${DOC_GEN_FULL_DIR}\\"\" >> ${DOC_GEN_DIR}/doxygen_full.cfg
)

# Custom target to build the conf file needed by docs-api above:
add_custom_command(
	OUTPUT ${DOC_GEN_DIR}/doxygen_api.cfg
	DEPENDS ${DOC_GEN_DIR}/index.dox
	COMMAND mkdir ${DOC_GEN_API_DIR} &&
			cp ${DOC_DIR}/doxygen_template.cfg ${DOC_GEN_DIR}/doxygen_api.cfg &&
			echo \"INPUT = doc/gen/index.dox include\" >> ${DOC_GEN_DIR}/doxygen_api.cfg &&
			echo \"OUTPUT_DIRECTORY = \\"${DOC_GEN_API_DIR}\\"\" >> ${DOC_GEN_DIR}/doxygen_api.cfg
)

# Append our README to the index file:
add_custom_command(
	OUTPUT ${DOC_GEN_DIR}/index.dox
	DEPENDS ${DOC_GEN_DIR}
	COMMAND cat ${DOC_DIR}/README.dox >> ${DOC_GEN_DIR}/index.dox
)

# Custom target to create doc/gen:
add_custom_command(
	OUTPUT ${DOC_GEN_DIR}
	COMMAND mkdir -p ${DOC_GEN_DIR}
)
# }}}
## }}}

## MISC {{{
# Specify which directories should be searched for header files:
include_directories(
	${LABCOMM_INCLUDE_DIRS}
	${INC_DIR}
	${Firefly_PROJECT_DIR}
	${Firefly_SOURCE_DIR}
)

# Make sure object files are named *.o instead of *.c.o:
set(CMAKE_C_OUTPUT_EXTENSION_REPLACE 1)

# Dump variables available to cmake, good for debugging:
 #get_cmake_property(_variableNames VARIABLES)
 #foreach (_variableName ${_variableNames})
	 #message(STATUS "${_variableName}=${${_variableName}}")
 #endforeach()
## }}}

## INCLUDE {{{
# Include subdirectories with their own CMake files:
add_subdirectory(utils)
add_subdirectory(protocol)
add_subdirectory(transport)
add_subdirectory(test)
## }}}

