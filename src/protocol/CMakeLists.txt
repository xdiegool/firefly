set(firefly_header_files
	${INC_DIR}/protocol/firefly_protocol.h
	${LABCOMM_INC_DIR}/labcomm.h
)

set(firefly_src_files
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol.c
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol_channel.c
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol_connection.c
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol_labcomm.c
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol_private.h
	${firefly_header_files}
)

add_library(firefly ${firefly_src_files})

# Make firefly depend on the generated Labcomm files
add_dependencies(firefly lc_files)

