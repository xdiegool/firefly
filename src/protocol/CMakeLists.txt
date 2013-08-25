# The set of firefly source files to compile:
set(firefly_src_files
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol.c
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol_channel.c
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol_memory.c
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol_connection.c
	${Firefly_SOURCE_DIR}/protocol/firefly_protocol_labcomm.c
	${Firefly_SOURCE_DIR}/utils/firefly_errors_utils.c
	${Firefly_SOURCE_DIR}/utils/firefly_event_queue.c
	${Firefly_PROJECT_DIR}/gen/firefly_protocol.c
)

# Remove warnings when compiling gen/firefly_protocol.c (Labcomm's fault):
set_property(SOURCE ${Firefly_PROJECT_DIR}/gen/firefly_protocol.c
	PROPERTY COMPILE_FLAGS "-Wno-all -Wno-extra"
)

# Tell CMake that gen/firefly_protocol.c is generated:
set_source_files_properties(${Firefly_PROJECT_DIR}/gen/firefly_protocol.c PROPERTIES GENERATED TRUE)

# Add the firefly lib as a target:
add_library(firefly ${firefly_src_files})

# If not cross compiling, add firefly with error lib target:
if (NOT CMAKE_CROSSCOMPILING)
	add_library(firefly-werr
		${firefly_src_files}
		${Firefly_SOURCE_DIR}/utils/firefly_errors.c
	)

	# Add firefly-werr to install target:
	set(firefly_install_werr
		firefly-werr
	)
endif (NOT CMAKE_CROSSCOMPILING)

# Add libs to install target:
install(TARGETS ${firefly_install_werr} firefly
	ARCHIVE DESTINATION lib
)

