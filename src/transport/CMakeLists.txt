if(NOT CMAKE_CROSSCOMPILING OR VXWORKS_COMPILING)


	if (NOT VXWORKS_COMPILING)
		# Ethernet POSIX
		add_library(transport-eth-posix
			${Firefly_SOURCE_DIR}/transport/firefly_transport.c
			${Firefly_SOURCE_DIR}/transport/firefly_transport_eth_posix.c
			${Firefly_SOURCE_DIR}/utils/firefly_resend_posix.c
			${Firefly_SOURCE_DIR}/utils/firefly_event_queue_posix.c
		)
		set(transport_install_libs
			${transport_install_libs}
			transport-eth-posix
		)
		target_link_libraries(transport-eth-posix gen-files)
	endif (NOT VXWORKS_COMPILING)

	if (XENOMAI_FOUND AND RTNET_FOUND)
		include_directories(${XENOMAI_INCLUDE_DIRS} ${RTNET_INCLUDE_DIRS})
		# Ethernet Xenomai
		add_library(transport-eth-xenomai
			${Firefly_SOURCE_DIR}/transport/firefly_transport.c
			${Firefly_SOURCE_DIR}/transport/firefly_transport_eth_xeno.c
			${Firefly_SOURCE_DIR}/utils/firefly_event_queue_xeno.c
		)
		set_target_properties(transport-eth-xenomai
			PROPERTIES COMPILE_FLAGS "-std=gnu99 -D_GNU_SOURCE -D_REENTRANT -D__XENO__"
		)
		target_link_libraries(transport-eth-xenomai
			gen-files
			${XENOMAI_LIBRARIES}
		)

		# Append xenomai to libs to install
		set(transport_install_libs
			${transport_install_libs}
			transport-eth-xenomai
		)
	endif (XENOMAI_FOUND AND RTNET_FOUND)

	# if (NOT VXWORKS_COMPILING)
		# UDP POSIX
		add_library(transport-udp-posix
			${Firefly_SOURCE_DIR}/transport/firefly_transport.c
			${Firefly_SOURCE_DIR}/transport/firefly_transport_udp_posix.c
			${Firefly_SOURCE_DIR}/utils/firefly_resend_posix.c
			${Firefly_SOURCE_DIR}/utils/firefly_event_queue_posix.c
		)
		target_link_libraries(transport-udp-posix gen-files)
		set(transport_install_libs
			${transport_install_libs}
			transport-udp-posix
		)
	# endif (NOT VXWORKS_COMPILING)

	if (NOT VXWORKS_COMPILING)
		# TCP POSIX
		add_library(transport-tcp-posix
			${Firefly_SOURCE_DIR}/transport/firefly_transport.c
			${Firefly_SOURCE_DIR}/transport/firefly_transport_tcp_posix.c
			${Firefly_SOURCE_DIR}/utils/firefly_resend_posix.c
			${Firefly_SOURCE_DIR}/utils/firefly_event_queue_posix.c
		)
		target_link_libraries(transport-tcp-posix gen-files)
		set(transport_install_libs
			${transport_install_libs}
			transport-tcp-posix
		)
	endif (NOT VXWORKS_COMPILING)

else()

	# Ethernet Stellaris
	add_library(transport-eth-stellaris
		${Firefly_SOURCE_DIR}/transport/firefly_transport.c
		${Firefly_SOURCE_DIR}/transport/firefly_transport_eth_stellaris.c
	)
	target_link_libraries(transport-eth-stellaris gen-files)

	# UDP LWIP
	add_library(transport-udp-lwip
		${Firefly_SOURCE_DIR}/transport/firefly_transport.c
		${Firefly_SOURCE_DIR}/transport/firefly_transport_udp_lwip.c
	)
	target_link_libraries(transport-udp-lwip gen-files)

	# Set targets to install
	set(transport_install_libs
		transport-eth-stellaris
		transport-udp-lwip
	)
endif(NOT CMAKE_CROSSCOMPILING OR VXWORKS_COMPILING)

install(TARGETS ${transport_install_libs}
	ARCHIVE DESTINATION lib
)
