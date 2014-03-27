# Set system name to Generic when compiling for embedded systems
# according to CMake manual:
set(CMAKE_SYSTEM_NAME Generic)

set(VXWORKS_COMPILING TRUE)

# Specify the cross compiler:
set(CMAKE_C_COMPILER i586-wrs-vxworks-gcc)
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DLABCOMM_COMPAT=\\\"labcomm_compat_vxworks.h\\\"")

# Add additional includes
include_directories(
	/opt/robot/include
	/opt/robot/include/vxworks/5.5.1/
	/opt/robot/include/vxworks/5.5.1/wrn/coreip/
)
