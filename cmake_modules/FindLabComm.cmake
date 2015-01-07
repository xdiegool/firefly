# Find the LabComm headers and libraries
#
# LABCOMM_INCLUDE_DIRS - The LabComm include directory.
# LABCOMM_LIBRARIES - The libraries needed to use LabComm.
# LABCOMM_FOUND - True if LabComm found in system.
#

include(LibFindMacros)

if(NOT LABCOMM_ROOT_DIR)
	# No toolchain file used.
        if("$ENV{LABCOMM}" STREQUAL "")
        	message(FATAL_ERROR "Set env. variable LABCOMM to labcomm root.")
        endif()
	set(LABCOMM_ROOT_DIR $ENV{LABCOMM}/lib/c)
endif()

# Header files to find
set(HEADER_NAMES labcomm.h labcomm_private.h)

# Libraries to find
set(LABCOMM_NAME labcomm2014)

find_path(LABCOMM_INCLUDE_DIR NAMES ${HEADER_NAMES} PATHS ${LABCOMM_ROOT_DIR}/2014)
find_library(LABCOMM_LIBRARY  NAMES ${LABCOMM_NAME} PATHS ${LABCOMM_ROOT_DIR})

set(LABCOMM_PROCESS_INCLUDES LABCOMM_INCLUDE_DIR)
set(LABCOMM_PROCESS_LIBS LABCOMM_LIBRARY)
# We really need LabComm for compilation!
set(LABCOMM_FIND_REQUIRED true)

libfind_process(LABCOMM)
