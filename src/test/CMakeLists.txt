# Modeline {{{
# vi: foldmarker={{{,}}} foldmethod=marker foldlevel=0
# }}}

message("-- Looking for CUnit")
find_package(CUnit)

# We can only compile tests when not compiling for ARM
if(NOT CMAKE_CROSSCOMPILING AND CUNIT_FOUND)
	link_directories(${Firefly_BINARY_DIR} ${LABCOMM_INC_DIR})

	## Test helper library {{{
	add_library(test_helpers
		${Firefly_SOURCE_DIR}/test/labcomm_static_buffer_reader.c
		${Firefly_SOURCE_DIR}/test/labcomm_static_buffer_writer.c
		${Firefly_SOURCE_DIR}/test/error_helper.c
		${Firefly_SOURCE_DIR}/test/proto_helper.c
		${Firefly_SOURCE_DIR}/test/event_helper.c
	)
	add_dependencies(test_helpers gen-files)
	## }}}

	## TEST_PROTOCOL_MAIN {{{
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
		cunit firefly gen-files test_helpers
		${LABCOMM_LIBRARIES}
	)
	add_test(test_protocol_main test_protocol_main)
	## }}}

	## TEST_TRANSPORT_MAIN {{{
	add_executable(test_transport_main
		${Firefly_SOURCE_DIR}/test/test_transport_main.c
		${Firefly_SOURCE_DIR}/test/test_transport.c
		${Firefly_SOURCE_DIR}/test/test_transport_gen.c
		${Firefly_SOURCE_DIR}/test/test_transport_udp_posix.c
		${Firefly_SOURCE_DIR}/utils/firefly_resend_posix.c
		${Firefly_SOURCE_DIR}/utils/firefly_errors_utils.c
	)
	target_link_libraries(test_transport_main
		cunit transport-udp-posix firefly
		gen-files test_helpers
		${LABCOMM_LIBRARIES} pthread rt
	)
	add_test(test_transport_main test_transport_main)
	## }}}

	## TEST_TRANSPORT_ETH_POSIX_MAIN {{{
	add_executable(test_transport_eth_posix_main
		${Firefly_SOURCE_DIR}/test/test_transport.c
		${Firefly_SOURCE_DIR}/test/test_transport_eth_posix_main.c
		${Firefly_SOURCE_DIR}/test/test_transport_eth_posix.c
		${Firefly_SOURCE_DIR}/test/error_helper.c
		${Firefly_SOURCE_DIR}/test/event_helper.c
		${Firefly_SOURCE_DIR}/utils/firefly_errors_utils.c
	)
	target_link_libraries(test_transport_eth_posix_main
		cunit transport-eth-posix firefly gen-files test_helpers
		${LABCOMM_LIBRARIES} pthread rt
	)
	add_test(test_transport_eth_posix_main test_transport_eth_posix_main)
	## }}}

	## TEST_EVENT_MAIN {{{
	add_executable(test_event_main
		${Firefly_SOURCE_DIR}/test/test_event_main.c
		${Firefly_SOURCE_DIR}/utils/firefly_event_queue.c
	)
	target_link_libraries(test_event_main
		cunit test_helpers
	)
	add_test(test_event_main test_event_main)
	## }}}

	## TEST_RESEND_POSIX {{{
	add_executable(test_resend_posix
		${Firefly_SOURCE_DIR}/test/test_resend_posix.c
		${Firefly_SOURCE_DIR}/utils/firefly_resend_posix.c
	)
	target_link_libraries(test_resend_posix
		cunit test_helpers rt
	)
	add_test(test_resend_posix test_resend_posix)
	## }}}

	## PINGPONG_MAIN {{{
	add_executable(pingpong_main
		${Firefly_SOURCE_DIR}/test/pingpong/pingpong_main.c
		${Firefly_SOURCE_DIR}/test/pingpong/pingpong_pudp.c
		${Firefly_SOURCE_DIR}/test/pingpong/pingpong.c
		${Firefly_SOURCE_DIR}/test/pingpong/pong_pudp.c
		${Firefly_SOURCE_DIR}/test/pingpong/ping_pudp.c
		${Firefly_SOURCE_DIR}/test/test_labcomm_utils.c
		${Firefly_SOURCE_DIR}/utils/firefly_event_queue_posix.c
		${Firefly_SOURCE_DIR}/utils/firefly_resend_posix.c
	)
	target_link_libraries(pingpong_main
		firefly-werr transport-udp-posix cunit
		gen-files ${LABCOMM_LIBRARIES} pthread rt
	)
	## }}}

	## PINGPONG_MAIN {{{
	add_executable(pingpong_main_tcp
		${Firefly_SOURCE_DIR}/test/pingpong/pingpong_main_tcp.c
		${Firefly_SOURCE_DIR}/test/pingpong/pingpong_ptcp.c
		${Firefly_SOURCE_DIR}/test/pingpong/pingpong.c
		${Firefly_SOURCE_DIR}/test/pingpong/pong_ptcp.c
		${Firefly_SOURCE_DIR}/test/pingpong/ping_ptcp.c
		${Firefly_SOURCE_DIR}/test/test_labcomm_utils.c
		${Firefly_SOURCE_DIR}/utils/firefly_event_queue_posix.c
		${Firefly_SOURCE_DIR}/utils/firefly_resend_posix.c
	)
	target_link_libraries(pingpong_main_tcp
		firefly-werr transport-tcp-posix cunit
		gen-files ${LABCOMM_LIBRARIES} pthread rt
	)
	## }}}

	## UDP_POSIX {{{
	add_executable(udp_posix
		${Firefly_SOURCE_DIR}/test/system/udp_posix.c
		${Firefly_SOURCE_DIR}/test/test_labcomm_utils.c
		${Firefly_SOURCE_DIR}/utils/firefly_resend_posix.c
	)
	target_link_libraries(udp_posix
		firefly transport-udp-posix cunit
		gen-files test_helpers ${LABCOMM_LIBRARIES} pthread rt
	)
	## }}}

	## PONG_ETH_MAIN {{{
	add_executable(pong_eth_main
		${Firefly_SOURCE_DIR}/test/pingpong/pingpong.c
		${Firefly_SOURCE_DIR}/test/pingpong/pong_peth.c
		${Firefly_SOURCE_DIR}/utils/firefly_event_queue_posix.c
	)
	target_link_libraries(pong_eth_main
		firefly-werr transport-eth-posix cunit
		gen-files ${LABCOMM_LIBRARIES} pthread rt
	)
	## }}}

	## PING_ETH_MAIN {{{
	add_executable(ping_eth_main
		${Firefly_SOURCE_DIR}/test/pingpong/pingpong.c
		${Firefly_SOURCE_DIR}/test/pingpong/ping_peth.c
		${Firefly_SOURCE_DIR}/utils/firefly_event_queue_posix.c
	)
	target_link_libraries(ping_eth_main
		firefly-werr transport-eth-posix cunit
		gen-files ${LABCOMM_LIBRARIES} pthread rt
	)
	## }}}

	if (XENOMAI_FOUND)
		## PONG_ETH_XENO {{{
		include_directories(${XENOMAI_INCLUDE_DIRS})
		add_executable(pong_eth_xeno
			${Firefly_SOURCE_DIR}/test/pingpong/pingpong.c
			${Firefly_SOURCE_DIR}/test/pingpong/pingpong_eth_xeno.c
			${Firefly_SOURCE_DIR}/test/pingpong/pong_eth_xeno.c
			${Firefly_SOURCE_DIR}/utils/firefly_event_queue_xeno.c
		)
		set_target_properties(pong_eth_xeno
			PROPERTIES COMPILE_FLAGS "-std=gnu99 -D_GNU_SOURCE -D_REENTRANT -D__XENO__"
		)
		target_link_libraries(pong_eth_xeno
			firefly-werr transport-eth-xenomai
			gen-files ${XENOMAI_LIBRARIES} rtdm ${LABCOMM_LIBRARIES} pthread rt
		)
		## }}}

		## PING_ETH_XENO {{{
		add_executable(ping_eth_xeno
			${Firefly_SOURCE_DIR}/test/pingpong/pingpong.c
			${Firefly_SOURCE_DIR}/test/pingpong/pingpong_eth_xeno.c
			${Firefly_SOURCE_DIR}/test/pingpong/ping_eth_xeno.c
			${Firefly_SOURCE_DIR}/utils/firefly_event_queue_xeno.c
		)
		set_target_properties(ping_eth_xeno
			PROPERTIES COMPILE_FLAGS "-std=gnu99 -D_GNU_SOURCE -D_REENTRANT -D__XENO__"
		)
		target_link_libraries(ping_eth_xeno
			firefly-werr transport-eth-xenomai
			gen-files ${XENOMAI_LIBRARIES} rtdm ${LABCOMM_LIBRARIES} pthread rt
		)
		## }}}
	endif (XENOMAI_FOUND)

	## PINGPONG_MULTI_MAIN {{{
	add_executable(pingpong_multi_main
		${Firefly_SOURCE_DIR}/test/pingpong/pingpong_main.c
		${Firefly_SOURCE_DIR}/test/pingpong/pingpong.c
		${Firefly_SOURCE_DIR}/test/pingpong/pingpong.c
		${Firefly_SOURCE_DIR}/test/pingpong/pong_pmulti.c
		${Firefly_SOURCE_DIR}/test/pingpong/ping_pmulti.c
		${Firefly_SOURCE_DIR}/utils/firefly_event_queue_posix.c
	)
	target_link_libraries(pingpong_multi_main
		firefly-werr transport-udp-posix transport-eth-posix
		cunit gen-files ${LABCOMM_LIBRARIES} pthread rt
	)
	## }}}

	# TODO: Fix test_proto_memman tests and add that to this file.

	## Custom target to run tests w/ valgrind {{{
	add_custom_target(run-test
		COMMAND ${Firefly_PROJECT_DIR}/cli/test.sh
		WORKING_DIRECTORY ${Firefly_PROJECT_DIR}/cli
		DEPENDS test_protocol_main test_transport_main test_transport_eth_posix_main test_event_main test_resend_posix
	)
	## }}}

	## Custom target to run system tests w/ valgrind {{{
	add_custom_target(run-systemtest
		COMMAND ${Firefly_PROJECT_DIR}/cli/test.sh system
		WORKING_DIRECTORY ${Firefly_PROJECT_DIR}/cli
		DEPENDS pingpong_main pong_eth_main ping_eth_main pingpong_multi_main udp_posix
	)
	## }}}

endif(NOT CMAKE_CROSSCOMPILING AND CUNIT_FOUND)
