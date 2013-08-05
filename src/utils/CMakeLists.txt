#set(event_queue_src_files
	#${Firefly_SOURCE_DIR}/utils/firefly_event_queue.c
	#${Firefly_SOURCE_DIR}/utils/firefly_event_queue_private.h
	#${Firefly_SOURCE_DIR}/protocol/firefly_protocol_private.h
#)

#add_library(event-queue ${event_queue_src_files})

#target_link_libraries(event-queue gen-files)
