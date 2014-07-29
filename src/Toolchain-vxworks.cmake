# Set system name to Generic when compiling for embedded systems
# according to CMake manual:
set(CMAKE_SYSTEM_NAME Generic)

set(VXWORKS_COMPILING TRUE)

if("$ENV{LABCOMM_VX}" STREQUAL "")
	message(FATAL_ERROR "Set env. variable LABCOMM_VX to labcomm root.")
endif()
set(LABCOMM_ROOT_DIR $ENV{LABCOMM_VX}/lib/c)

# Specify the cross compiler:
set(CMAKE_C_COMPILER i586-wrs-vxworks-gcc)
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DLABCOMM_COMPAT=\\\"labcomm_compat_vxworks.h\\\" -DLABCOMM_NO_STDIO")

# Add additional includes
include_directories(
	/opt/robot/include
	/opt/robot/include/vxworks/5.5.1/
	/opt/robot/include/vxworks/5.5.1/wrn/coreip/
)
