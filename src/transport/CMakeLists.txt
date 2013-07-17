if(NOT CMAKE_CROSSCOMPILING)
	# Ethernet POSIX
	set(transport_eth_posix_files
		${Firefly_SOURCE_DIR}/transport/firefly_transport.c
		${Firefly_SOURCE_DIR}/transport/firefly_transport_eth_posix.c
	)
	add_library(transport-eth-posix ${transport_eth_posix_files})
	target_link_libraries(transport-eth-posix gen-files)

	# UDP POSIX
	set(transport_udp_posix_files
		${Firefly_SOURCE_DIR}/transport/firefly_transport.c
		${Firefly_SOURCE_DIR}/transport/firefly_transport_udp_posix.c
		${Firefly_SOURCE_DIR}/utils/firefly_resend_posix.c
	)
	add_library(transport-udp-posix ${transport_udp_posix_files})
	target_link_libraries(transport-udp-posix gen-files)
endif(NOT CMAKE_CROSSCOMPILING)

if(CMAKE_CROSSCOMPILING)
	# Ethernet Stellaris
	set(transport_eth_stellaris_files
		${Firefly_SOURCE_DIR}/transport/firefly_transport.c
		${Firefly_SOURCE_DIR}/transport/firefly_transport_eth_stellaris.c
	)
	add_library(transport-eth-stellaris ${transport_eth_stellaris_files})
	target_link_libraries(transport-eth-stellaris gen-files)

	# UDP LWIP
	set(transport_udp_lwip_files
		${Firefly_SOURCE_DIR}/transport/firefly_transport.c
		${Firefly_SOURCE_DIR}/transport/firefly_transport_udp_lwip.c
	)
	add_library(transport-udp-lwip ${transport_udp_lwip_files})
	target_link_libraries(transport-udp-lwip gen-files)
endif(CMAKE_CROSSCOMPILING)
