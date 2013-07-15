set(firefly_event_queue_src_files
	${Firefly_SOURCE_DIR}/utils/firefly_event_queue.c
	${Firefly_SOURCE_DIR}/utils/firefly_event_queue_private.h
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol_private.h
)

add_library(firefly_event_queue ${firefly_event_queue_src_files})

# Make event queue depend on the generated Labcomm files
add_dependencies(firefly_event_queue lc_files)
