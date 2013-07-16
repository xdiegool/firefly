link_directories(${Firefly_BINARY_DIR} ${LABCOMM_INC_DIR})

add_library(test_helpers
	${Firefly_SOURCE_DIR}/test/error_helper.c
	${Firefly_SOURCE_DIR}/test/proto_helper.c
	${Firefly_SOURCE_DIR}/test/event_helper.c
)
add_dependencies(test_helpers gen-files)

add_executable(test_protocol_main
	${Firefly_SOURCE_DIR}/test/test_protocol_main.c
	${Firefly_SOURCE_DIR}/test/test_proto_labcomm.c
	${Firefly_SOURCE_DIR}/test/test_labcomm_utils.c
	${Firefly_SOURCE_DIR}/test/test_proto_chan.c
	${Firefly_SOURCE_DIR}/test/test_proto_conn.c
	${Firefly_SOURCE_DIR}/test/test_proto_important.c
	${Firefly_SOURCE_DIR}/test/test_proto_errors.c
)

target_link_libraries(test_protocol_main
	cunit labcomm event-queue
	firefly gen-files test_helpers
)

add_executable(test_transport_main
	${Firefly_SOURCE_DIR}/test/test_transport_main.c
	${Firefly_SOURCE_DIR}/test/test_transport.c
	${Firefly_SOURCE_DIR}/test/test_transport_gen.c
	${Firefly_SOURCE_DIR}/test/test_transport_udp_posix.c
	${Firefly_SOURCE_DIR}/utils/firefly_resend_posix.c
	${Firefly_SOURCE_DIR}/utils/firefly_errors_utils.c
)

target_link_libraries(test_transport_main
	cunit labcomm pthread rt event-queue
	transport-udp-posix firefly gen-files test_helpers
)

### TEST TRANSPORT MAIN
add_executable(test_transport_eth_posix_main
	${Firefly_SOURCE_DIR}/test/test_transport.c
	${Firefly_SOURCE_DIR}/test/test_transport_eth_posix_main.c
	${Firefly_SOURCE_DIR}/test/test_transport_eth_posix.c
	${Firefly_SOURCE_DIR}/test/error_helper.c
	${Firefly_SOURCE_DIR}/test/event_helper.c
	${Firefly_SOURCE_DIR}/utils/firefly_errors_utils.c
)

target_link_libraries(test_transport_eth_posix_main 
	cunit labcomm pthread rt event-queue
	transport-eth-posix firefly gen-files test_helpers
)

### TEST EVENT MAIN
add_executable(test_event_main
	${Firefly_SOURCE_DIR}/test/test_event_main.c
)

target_link_libraries(test_event_main 
	cunit event-queue test_helpers
)

# clang  -g -Lbuild -L../lib/labcomm/lib/c
# build/test/test_event_main.o
# build/test/error_helper.o
# build/utils/firefly_event_queue.o
# -llabcomm
# -lcunit -lpthread -lrt -o build/test/test_event_main


