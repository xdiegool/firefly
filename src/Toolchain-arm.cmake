set(FT_SENSE_DIR ${Firefly_SOURCE_DIR}/../../ft-sense)
set(LDSCRIPT ${FT_SENSE_DIR}/src/adc_freertos_lwip/standalone.ld)
set(RTOS_BASE ${FT_SENSE_DIR}/lib/freertos)
set(RTOS_SOURCE_DIR ${RTOS_BASE}/Source)
set(RTOS_COMMON_PORTS ${RTOS_BASE}/Demo/Common)
set(LWIP_DRIVER_DIR ${RTOS_COMMON_PORTS}/ethernet)
set(LWIP_SOURCE_DIR ${LWIP_DRIVER_DIR}/lwip-1.3.0)
set(LWIP_INCLUDE_DIR ${LWIP_SOURCE_DIR}/src/include)
set(LWIP_PORT_DIR ${LWIP_SOURCE_DIR}/contrib/port/FreeRTOS/LM3S/arch)
set(WEB_SERVER_DIR ${LWIP_SOURCE_DIR}/Apps)
set(LUMINARY_DRIVER_DIR ${RTOS_COMMON_PORTS}/drivers/LuminaryMicro)
set(COMMOM_INCLUDE ${RTOS_COMMON_PORTS}/include)
set(COMMOM_ADC ${FT_SENSE_DIR}/src/common)
set(DRIVERLIB_DIR ${FT_SENSE_DIR}/lib/driverlib)

# this one is important
set(CMAKE_SYSTEM_NAME Generic)

# specify the cross compiler
set(CMAKE_C_COMPILER arm-none-eabi-gcc)

# where is the target environment
set(CMAKE_FIND_ROOT_PATH
	${COMMOM_ADC}
	${COMMOM_INCLUDE}
	${DRIVERLIB_DIR}
	${DRIVERLIB_DIR}/inc
	${FT_SENSE_DIR}/lib
	${FT_SENSE_DIR}/src/adc_freertos_lwip/src
	${LUMINARY_DRIVER_DIR}
	${LWIP_DRIVER_DIR}
	${LWIP_INCLUDE_DIR}
	${LWIP_INCLUDE_DIR}/ipv4
	${LWIP_SOURCE_DIR}/contrib/port/FreeRTOS/LM3S
	${LWIP_PORT_DIR}
	${RTOS_SOURCE_DIR}/include
	${RTOS_SOURCE_DIR}/portable/GCC/ARM_CM3
	${WEB_SERVER_DIR}
)
include_directories(${CMAKE_FIND_ROOT_PATH})
# search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
