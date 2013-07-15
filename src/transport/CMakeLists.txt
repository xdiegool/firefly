# Ethernet POSIX
set(transport_eth_posix_files
	${Firefly_SOURCE_DIR}/transport/firefly_transport.c
	${Firefly_SOURCE_DIR}/transport/firefly_transport_eth_posix.c
	${Firefly_SOURCE_DIR}/transport/firefly_transport_eth_posix_private.h
)
add_library(transport_eth_posix ${transport_eth_posix_files})
add_dependencies(transport_eth_posix lc_files)

# UDP POSIX
set(transport_udp_posix_files
	${Firefly_SOURCE_DIR}/transport/firefly_transport.c
	${Firefly_SOURCE_DIR}/transport/firefly_transport_udp_posix.c
	${Firefly_SOURCE_DIR}/transport/firefly_transport_udp_posix_private.h
)
add_library(transport_udp_posix ${transport_udp_posix_files})
add_dependencies(transport_udp_posix lc_files)

