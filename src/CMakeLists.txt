# Main CMake file for project Firely.
# Modeline {
# vi: foldmarker={,} foldmethod=marker foldlevel=0
# }

### General {
cmake_minimum_required (VERSION 2.6)
project (Firefly)
### }

### DIRS {

set(Firefly_PROJECT_DIR ${Firefly_SOURCE_DIR}/..)
set(GEN_DIR ${Firefly_PROJECT_DIR}/gen)
set(LC_DIR ${Firefly_PROJECT_DIR}/lc)
set(INC_DIR ${Firefly_PROJECT_DIR}/include)
set(GEN_DIR ${Firefly_PROJECT_DIR}/gen)
set(TEST_FILES_DIR ${Firefly_PROJECT_DIR}/testfiles)

set(LABCOMM_DIR ${Firefly_PROJECT_DIR}/../lib/labcomm)
set(LABCOMMC ${LABCOMM_DIR}/compiler/labComm.jar)
set(LABCOMM_INC_DIR ${LABCOMM_DIR}/lib/c/)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${Firefly_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${Firefly_BINARY_DIR})

### }

### FLAGS {
set(CMAKE_C_FLAGS
	"-std=c99 -Wall -Wextra -Wno-missing-field-initializers -D DEBUG -D LABCOMM_ENCODER_LINEAR_SEARCH"
)

# TODO: fix this, includes and stuff
# set(ARM_C_FLAGS
# 	"-Wfloat-equal -Werror-implicit-function-declaration"
# 	"-mthumb -mcpu=cortex-m3 -T$(LDSCRIPT)"
# 	"-ffunction-sections -fdata-sections"
# 	"$(INC_TRANSPORT_UDP_LWIP) -D UART_BUFFERED"
# 	"-D sprintf=usprintf"
# 	"-D snprintf=usnprintf"
# 	"-D vsnprintf=uvsnprintf"
# 	"-D printf=uipprintf"
# 	"-D malloc=pvPortMalloc"
# 	"-D calloc=pvPortCalloc"
# 	"-D free=vPortFree"
# 	"-D ARM_CORTEXM3_CODESOURCERY"
# 	"-D LABCOMM_NO_STDIO"
# 	"-DLABCOMM_COMPAT=\"labcomm_compat_arm_cortexm3.h\""
# 	"-D GCC_ARMCM3=1"
# )
### }

### CUSTOM COMMANDS & TARGETS {
add_custom_command(
	OUTPUT ${GEN_DIR}
	DEPENDS ${LABCOMMC} ${LABCOMM_INC_DIR}/liblabcomm.a
	COMMAND mkdir -p ${GEN_DIR}
)
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

add_library(gen-files
	${Firefly_PROJECT_DIR}/gen/test.c
	${Firefly_PROJECT_DIR}/gen/firefly_protocol.c
)
# Tell cmake that the following files are generated at compile time,
# otherwise it will complain on missing files.
set_source_files_properties(${Firefly_PROJECT_DIR}/gen/test.c PROPERTIES GENERATED TRUE)
set_source_files_properties(${Firefly_PROJECT_DIR}/gen/firefly_protocol.c PROPERTIES GENERATED TRUE)

# add_custom_target(
#     lc_files
#     DEPENDS ${GEN_DIR}/firefly_protocol.c ${GEN_DIR}/firefly_protocol.h ${GEN_DIR}/pingpong.c ${GEN_DIR}/pingpong.h ${GEN_DIR}/test.c ${GEN_DIR}/test.h
# )

add_custom_command(
	OUTPUT ${LABCOMMC}
	COMMAND ant -f ${LABCOMM_DIR}/compiler/build.xml jar
)

add_custom_target(
	labcomm_lib
	DEPENDS ${LABCOMM_INC_DIR}/liblabcomm.a
)

add_custom_command(
	OUTPUT ${LABCOMM_INC_DIR}/liblabcomm.a
	COMMAND make -C ${LABCOMM_INC_DIR} -e LABCOMM_NO_EXPERIMENTAL=true -e CFLAGS=\"-std=c99 -Wall -Wno-missing-field-initializers -I. -Iinclude -Isrc -D DEBUG -D LABCOMM_ENCODER_LINEAR_SEARCH -g -O0\" liblabcomm.a
)

add_custom_command(
	TARGET test
	POST_BUILD
	COMMAND echo "testing"
)
### }

### MISC {
include_directories(
	${LABCOMM_INC_DIR}
	${INC_DIR}
	${Firefly_PROJECT_DIR}
	${Firefly_SOURCE_DIR}
	${Firefly_BINARY_DIR}
)

# # Dumps variables available to cmake:
# get_cmake_property(_variableNames VARIABLES)
# foreach (_variableName ${_variableNames})
#     message(STATUS "${_variableName}=${${_variableName}}")
# endforeach()

### }

### INCLUDE {
add_subdirectory(utils)
add_subdirectory(protocol)
add_subdirectory(transport)
add_subdirectory(test)
### }§

