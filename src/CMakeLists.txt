# Main CMake file for project Firely.
# Modeline {
# vi: foldmarker={,} foldmethod=marker foldlevel=0
# }

## General {
cmake_minimum_required (VERSION 2.8)
project (Firefly)
if(NOT CMAKE_CROSSCOMPILING)
	enable_testing()
endif(NOT CMAKE_CROSSCOMPILING)
## }

## DIRS {
# Firefly directories:
set(Firefly_PROJECT_DIR ${Firefly_SOURCE_DIR}/..)
set(GEN_DIR ${Firefly_PROJECT_DIR}/gen)
set(LC_DIR ${Firefly_PROJECT_DIR}/lc)
set(INC_DIR ${Firefly_PROJECT_DIR}/include)
set(GEN_DIR ${Firefly_PROJECT_DIR}/gen)
set(TEST_FILES_DIR ${Firefly_PROJECT_DIR}/testfiles)

# Labcomm directories:
set(LABCOMM_DIR ${Firefly_PROJECT_DIR}/../lib/labcomm)
set(LABCOMMC ${LABCOMM_DIR}/compiler/labComm.jar)
set(LABCOMM_INC_DIR ${LABCOMM_DIR}/lib/c/)

# Tell CMake to output library and executables directly in the build
# directory:
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${Firefly_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${Firefly_BINARY_DIR})

## }

## FLAGS {
# CFLAGS when cross compiling to the ARM-card:
if(CMAKE_CROSSCOMPILING)
	set(CMAKE_C_FLAGS
		"-std=c99 -Wall -Wextra -Wno-missing-field-initializers -D DEBUG -D LABCOMM_ENCODER_LINEAR_SEARCH -Wfloat-equal -Werror-implicit-function-declaration -mthumb -mcpu=cortex-m3 -T$(LDSCRIPT) -ffunction-sections -fdata-sections $(INC_TRANSPORT_UDP_LWIP) -D UART_BUFFERED -D sprintf=usprintf -D snprintf=usnprintf -D vsnprintf=uvsnprintf -D printf=uipprintf -D malloc=pvPortMalloc -D calloc=pvPortCalloc -D free=vPortFree -D ARM_CORTEXM3_CODESOURCERY -D LABCOMM_NO_STDIO -DLABCOMM_COMPAT=\\\"labcomm_compat_arm_cortexm3.h\\\" -D GCC_ARMCM3=1"
	)
endif(CMAKE_CROSSCOMPILING)

# CFLAGS when compiling normally:
if(NOT CMAKE_CROSSCOMPILING)
	set(CMAKE_C_FLAGS
		"-std=c99 -Wall -Wextra -Wno-missing-field-initializers -D DEBUG -D LABCOMM_ENCODER_LINEAR_SEARCH"
	)
endif(NOT CMAKE_CROSSCOMPILING)
## }

## CUSTOM COMMANDS & TARGETS {
# Custom command to create the directory for the generated Labcomm
# files, it depends upon the Labcomm compiler and library to make sure
# they are generated before trying to generate files with Labcomm.
add_custom_command(
	OUTPUT ${GEN_DIR}
	DEPENDS ${LABCOMMC} ${LABCOMM_INC_DIR}/liblabcomm.a
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

# Custom command to build the Labcomm compiler:
add_custom_command(
	OUTPUT ${LABCOMMC}
	COMMAND ant -f ${LABCOMM_DIR}/compiler/build.xml jar
)

# Custom command to build the static Labcomm library:
add_custom_command(
	OUTPUT ${LABCOMM_INC_DIR}/liblabcomm.a
	COMMAND make -C ${LABCOMM_INC_DIR} -e LABCOMM_NO_EXPERIMENTAL=true -e CFLAGS=\"-std=c99 -Wall -Wno-missing-field-initializers -I. -Iinclude -Isrc -D DEBUG -D LABCOMM_ENCODER_LINEAR_SEARCH -g -O0\" liblabcomm.a
)
## }

## MISC {
# Specify which directories should be searched for header files:
include_directories(
	${LABCOMM_INC_DIR}
	${INC_DIR}
	${Firefly_PROJECT_DIR}
	${Firefly_SOURCE_DIR}
	${Firefly_BINARY_DIR}
)

# Dump variables available to cmake:
# get_cmake_property(_variableNames VARIABLES)
# foreach (_variableName ${_variableNames})
#     message(STATUS "${_variableName}=${${_variableName}}")
# endforeach()

## }

## INCLUDE {
# Include subdirectories with their own CMake files:
add_subdirectory(utils)
add_subdirectory(protocol)
add_subdirectory(transport)
add_subdirectory(test)
## }§

