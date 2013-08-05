set(firefly_src_files
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol.c
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol_channel.c
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol_connection.c
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol_labcomm.c
	${Firefly_SOURCE_DIR}/utils/firefly_errors_utils.c
	${Firefly_SOURCE_DIR}/utils/firefly_event_queue.c
	${Firefly_PROJECT_DIR}/gen/firefly_protocol.c
)

set_property(SOURCE ${Firefly_PROJECT_DIR}/gen/firefly_protocol.c
	PROPERTY COMPILE_FLAGS "-Wno-all -Wno-extra"
)
set_source_files_properties(${Firefly_PROJECT_DIR}/gen/firefly_protocol.c PROPERTIES GENERATED TRUE)

if (CMAKE_CROSSCOMPILING)
	set(firefly_name firefly-arm)
else ()
	set(firefly_name firefly)
endif (CMAKE_CROSSCOMPILING)

add_library(${firefly_name} ${firefly_src_files})
target_link_libraries(${firefly_name} gen-files)

if (NOT CMAKE_CROSSCOMPILING)
	add_library(firefly-werr
		${firefly_src_files}
		${Firefly_SOURCE_DIR}/utils/firefly_errors.c
	)
	target_link_libraries(firefly-werr gen-files)
endif (NOT CMAKE_CROSSCOMPILING)
