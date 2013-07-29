if(NOT CMAKE_CROSSCOMPILING)
	# Ethernet POSIX
	add_library(transport-eth-posix
		${Firefly_SOURCE_DIR}/transport/firefly_transport.c
		${Firefly_SOURCE_DIR}/transport/firefly_transport_eth_posix.c
	)
	target_link_libraries(transport-eth-posix gen-files)

	message("-- Looking for Xenomai")
	find_package(Xenomai)
	if (XENOMAI_FOUND)
		include_directories(${XENOMAI_INCLUDE_DIRS})
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
	endif (XENOMAI_FOUND)

	# UDP POSIX
	add_library(transport-udp-posix
		${Firefly_SOURCE_DIR}/transport/firefly_transport.c
		${Firefly_SOURCE_DIR}/transport/firefly_transport_udp_posix.c
		${Firefly_SOURCE_DIR}/utils/firefly_resend_posix.c
	)
	target_link_libraries(transport-udp-posix gen-files)
endif(NOT CMAKE_CROSSCOMPILING)

if(CMAKE_CROSSCOMPILING)
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
endif(CMAKE_CROSSCOMPILING)
