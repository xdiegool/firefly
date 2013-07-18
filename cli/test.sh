#!/usr/bin/env bash

# Check if running as root:
if [[ $EUID -eq 0 ]]; then
	WITH_ROOT=1
fi

# Check if should run system tests:
if [ "$1" == "system" ]; then
	RUN_SYSTEM_TESTS=1
fi

# Set up some variables, e.g. paths and what programs to run:
CWD=`pwd`
UNIT_TEST_PROGS="../build/test/test_event_main
../build/test/test_protocol_main
../build/test/test_transport_main ../build/test/test_resend_posix"
UNIT_TEST_ROOT_PROGS="../build/test/test_transport_eth_posix_main
../build/test/test_transport_eth_posix_main"

SYSTEM_TEST_PROGS="../build/test/pingpong_main ../build/test/udp_posix"
# TODO: Should these (below) run at all on a single computer?
SYSTEM_TEST_ROOT_PROGS="../build/test/pong_eth_main ../build/test/ping_eth_main
../build/test/pingpong_multi_main"

# Run unit tests:
if [[ "${RUN_SYSTEM_TESTS}" -eq 0 ]]; then
	for prog in ${UNIT_TEST_PROGS}; do
		echo "=========================>BEGIN TEST: ${prog}";
		valgrind --quiet --error-exitcode=1 --leak-check=full --show-reachable=yes $prog;
		test "$?" -ne 0 && echo "FAILURE: Program exited with failure status." >&2 && break;
		echo "=========================>END TEST: ${prog}";
	done
	if [[ "${WITH_ROOT}" -eq 1 ]]; then
		for prog in ${UNIT_TEST_ROOT_PROGS}; do
			echo "=========================>BEGIN TEST: ${prog}";
			valgrind --quiet --error-exitcode=1 --leak-check=full --show-reachable=yes $prog;
			test "$?" -ne 0 && echo "FAILURE: Program exited with failure status." >&2 && break;
			echo "=========================>END TEST: ${prog}";
		done
	fi
fi

# If script is run with parameter system, also run the system tests:
if [[ "${RUN_SYSTEM_TESTS}" -eq 1 ]]; then
	for prog in ${SYSTEM_TEST_PROGS}; do
		echo "=========================>BEGIN TEST: ${prog}";
		valgrind --quiet --error-exitcode=1 --leak-check=full --show-reachable=yes $prog;
		test "$?" -ne 0 && echo "FAILURE: Program exited with failure status." >&2 && break;
		echo "=========================>END TEST: ${prog}";
	done
fi
