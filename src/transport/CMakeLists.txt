# Ethernet POSIX
set(transport_eth_posix_files
	${Firefly_SOURCE_DIR}/transport/firefly_transport.c
	${Firefly_SOURCE_DIR}/transport/firefly_transport_eth_posix.c
	${Firefly_SOURCE_DIR}/transport/firefly_transport_eth_posix_private.h
)
add_library(transport-eth-posix ${transport_eth_posix_files})
add_dependencies(transport-eth-posix lc_files)

# UDP POSIX
set(transport_udp_posix_files
	${Firefly_SOURCE_DIR}/transport/firefly_transport.c
	${Firefly_SOURCE_DIR}/transport/firefly_transport_udp_posix.c
	${Firefly_SOURCE_DIR}/transport/firefly_transport_udp_posix_private.h
)
add_library(transport-udp-posix ${transport_udp_posix_files})
add_dependencies(transport-udp-posix lc_files)

