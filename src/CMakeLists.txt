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

set(LABCOMM_DIR ${Firefly_PROJECT_DIR}/../lib/labcomm)
set(LABCOMMC ${LABCOMM_DIR}/compiler/labComm.jar)
set(LABCOMM_INC_DIR ${LABCOMM_DIR}/lib/c/)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${Firefly_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${Firefly_BINARY_DIR})

### }

### FLAGS {
set(CMAKE_C_FLAGS "-std=c99")
### }

### CUSTOM COMMANDS & TARGETS {
add_custom_command(
	OUTPUT ${GEN_DIR}
	DEPENDS ${LABCOMMC}
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

add_custom_target(
	lc_files
	DEPENDS ${GEN_DIR}/firefly_protocol.c ${GEN_DIR}/firefly_protocol.h ${GEN_DIR}/pingpong.c ${GEN_DIR}/pingpong.h ${GEN_DIR}/test.c ${GEN_DIR}/test.h
)

add_custom_command(
	OUTPUT ${LABCOMMC}
	COMMAND ant -f ${LABCOMM_DIR}/compiler/build.xml jar
)
### }

### MISC {
include_directories(
	${LABCOMM_INC_DIR}
	${INC_DIR}
	${Firefly_PROJECT_DIR}
	${Firefly_SOURCE_DIR}
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
### }§

