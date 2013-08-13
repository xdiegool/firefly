if(NOT CMAKE_CROSSCOMPILING)

	# Set targets to install
	set(transport_install_libs
		transport-eth-posix
		transport-udp-posix
	)

	# Ethernet POSIX
	add_library(transport-eth-posix
		${Firefly_SOURCE_DIR}/transport/firefly_transport.c
		${Firefly_SOURCE_DIR}/transport/firefly_transport_eth_posix.c
		${Firefly_SOURCE_DIR}/utils/firefly_resend_posix.c
		${Firefly_SOURCE_DIR}/utils/firefly_event_queue_posix.c
	)
	target_link_libraries(transport-eth-posix gen-files)

	if (XENOMAI_FOUND AND RTNET_FOUND)
		include_directories(${XENOMAI_INCLUDE_DIRS} ${RTNET_INCLUDE_DIRS})
		# Ethernet Xenomai
		add_library(transport-eth-xenomai
			${Firefly_SOURCE_DIR}/transport/firefly_transport.c
			${Firefly_SOURCE_DIR}/transport/firefly_transport_eth_xeno.c
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

	# UDP POSIX
	add_library(transport-udp-posix
		${Firefly_SOURCE_DIR}/transport/firefly_transport.c
		${Firefly_SOURCE_DIR}/transport/firefly_transport_udp_posix.c
		${Firefly_SOURCE_DIR}/utils/firefly_resend_posix.c
		${Firefly_SOURCE_DIR}/utils/firefly_event_queue_posix.c
	)
	target_link_libraries(transport-udp-posix gen-files)

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
endif(NOT CMAKE_CROSSCOMPILING)

install(TARGETS ${transport_install_libs}
	ARCHIVE DESTINATION lib
)
