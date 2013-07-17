set(firefly_src_files
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol.c
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol_channel.c
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol_connection.c
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol_labcomm.c
	${Firefly_SOURCE_DIR}/utils/firefly_errors_utils.c
)
add_library(firefly ${firefly_src_files})
target_link_libraries(firefly gen-files)

if (NOT CMAKE_CROSSCOMPILING)
	add_library(firefly-werr
		${firefly_src_files}
		${Firefly_SOURCE_DIR}/utils/firefly_errors.c
	)
	target_link_libraries(firefly-werr gen-files event-queue)
endif (NOT CMAKE_CROSSCOMPILING)
