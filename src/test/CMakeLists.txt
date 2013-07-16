link_directories(${Firefly_BINARY_DIR} ${LABCOMM_INC_DIR})

add_executable(test_protocol_main
	${Firefly_SOURCE_DIR}/test/test_protocol_main.c
	${Firefly_SOURCE_DIR}/test/test_proto_labcomm.c
	${Firefly_SOURCE_DIR}/test/test_labcomm_utils.c
	${Firefly_SOURCE_DIR}/test/test_proto_chan.c
	${Firefly_SOURCE_DIR}/test/test_proto_conn.c
	${Firefly_SOURCE_DIR}/test/test_proto_important.c
	${Firefly_SOURCE_DIR}/test/test_proto_errors.c
	${Firefly_SOURCE_DIR}/test/error_helper.c
	${Firefly_SOURCE_DIR}/test/proto_helper.c
	${Firefly_SOURCE_DIR}/test/event_helper.c
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol.c
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol_channel.c
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol_connection.c
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol_labcomm.c
	${Firefly_SOURCE_DIR}/utils/firefly_event_queue.c
	${Firefly_PROJECT_DIR}/gen/test.c
	${Firefly_PROJECT_DIR}/gen/firefly_protocol.c
)

add_dependencies(test_protocol_main lc_files labcomm_lib firefly event-queue)

target_link_libraries(test_protocol_main cunit labcomm pthread rt)

add_executable(test_transport_main
	${Firefly_SOURCE_DIR}/test/test_transport_main.c
	${Firefly_SOURCE_DIR}/test/test_transport.c
	${Firefly_SOURCE_DIR}/test/test_transport_gen.c
	${Firefly_SOURCE_DIR}/test/test_transport_udp_posix.c
	${Firefly_SOURCE_DIR}/test/error_helper.c
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol.c
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol_channel.c
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol_connection.c
	${Firefly_SOURCE_DIR}/utils/firefly_event_queue.c
	${Firefly_SOURCE_DIR}/utils/firefly_resend_posix.c
	${Firefly_SOURCE_DIR}/utils/firefly_errors_utils.c
	${Firefly_PROJECT_DIR}/gen/test.c
	${Firefly_PROJECT_DIR}/gen/firefly_protocol.c
)

add_dependencies(test_transport_main lc_files labcomm_lib firefly event-queue)

target_link_libraries(test_transport_main transport-udp-posix firefly cunit labcomm pthread rt)

### TEST TRANSPORT MAIN
add_executable(test_transport_eth_posix_main
	${Firefly_SOURCE_DIR}/test/test_transport.c
	${Firefly_SOURCE_DIR}/test/test_transport_eth_posix_main.c
	${Firefly_SOURCE_DIR}/test/test_transport_eth_posix.c
	${Firefly_SOURCE_DIR}/test/error_helper.c
	${Firefly_SOURCE_DIR}/test/event_helper.c
	${Firefly_SOURCE_DIR}/utils/firefly_event_queue.c
	${Firefly_SOURCE_DIR}/utils/firefly_errors_utils.c
	${Firefly_PROJECT_DIR}/gen/firefly_protocol.c
)

add_dependencies(test_transport_eth_posix_main lc_files labcomm_lib firefly event-queue)

target_link_libraries(test_transport_eth_posix_main transport-eth-posix firefly cunit labcomm pthread rt)

### TEST EVENT MAIN
add_executable(test_event_main
	${Firefly_SOURCE_DIR}/test/test_event_main.c
	${Firefly_SOURCE_DIR}/test/error_helper.c
	${Firefly_SOURCE_DIR}/test/event_helper.c
	${Firefly_SOURCE_DIR}/utils/firefly_event_queue.c
)

add_dependencies(test_event_main lc_files labcomm_lib event-queue)

target_link_libraries(test_event_main event-queue cunit pthread)

# clang  -g -Lbuild -L../lib/labcomm/lib/c
# build/test/test_event_main.o
# build/test/error_helper.o
# build/utils/firefly_event_queue.o
# -llabcomm
# -lcunit -lpthread -lrt -o build/test/test_event_main


