set(firefly_src_files
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol.c
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol_channel.c
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol_connection.c
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol_labcomm.c
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol_private.h
	${INC_DIR}/protocol/firefly_protocol.h
	${LABCOMM_INC_DIR}/labcomm.h
)

add_library(firefly ${firefly_src_files})

target_link_libraries(firefly gen-files)

