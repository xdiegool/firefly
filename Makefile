# TODO .d files
# TODO labcomm

# Makefile for project Firely. run `make help` for help on targets.
### Macros

## Compiler optons.
# Use LLVM clang if it's found.
CC = $(shell hash clang 2>/dev/null && echo clang || echo gcc)

# Change with $make -e DEBUG=false
DEBUG = true
DEBUGOPT = -g
ifeq ($(DEBUG), true)
	CFLAGS += $(DEBUGOPT) -O0
	LDFLAGS += $(DEBUGOPT)
else
	CFLAGS += -O$(OPTLVL)
endif

# Enable instrumented compilation with $make -e COVERAGE=true
ifeq ($(COVERAGE), true)
	CFLAGS += --coverage
	LDFLAGS += --coverage
endif

# Add options depending on target. Default is x86/x64.
# Set with $make -e TARGET_ISA=<isa>
# TODO test this.
ifeq ($(TARGET_ISA), arm_thumb)
	CFLAGS += -Wfloat-equal -Werror-implicit-function-declaration \
		-mthumb -mcpu=cortex-m3 -T$(LDSCRIPT) \
		-ffunction-sections -fdata-sections
endif

# Clang does not support noexecstack it seems.
ifeq ($(CC), gcc)
	CFLAGS += -z noexecstack
endif

ERRFLAGS = -Wall -Wextra
OPTLVL= 2
INC_COMMON = $(addprefix -I,. $(INCLUDE_DIR) $(LABCOMMPATH) $(SRC_DIR)) # TODO do we need labcommpath here?
CFLAGS = -std=c99 $(ERRFLAGS) $(INC_COMMON)

FREERTOS_LWIP_INCLUDES= -I $(RTOS_SOURCE_DIR)/include \
       			-I $(LWIP_SOURCE_DIR)/src/include/ipv4 \
			-I $(LWIP_SOURCE_DIR)/src/include \
			-I $(LWIP_SOURCE_DIR)/contrib/port/FreeRTOS/LM3S \
			-I $(COMMOM_INCLUDE) \
			-I $(LWIP_DRIVER_DIR) \
			-I $(DRIVERLIB_DIR) \
			-I ../ft-sense/lib/ \
			-I $(DRIVERLIB_DIR)/inc \
			-I $(RTOS_SOURCE_DIR)/portable/GCC/ARM_CM3 \
			-I $(WEB_SERVER_DIR) \
			-I $(LUMINARY_DRIVER_DIR) \
			-I $(LWIP_PORT_DIR) \
			-I $(LWIP_INCLUDE_DIR) \
			-I $(COMMOM_ADC)

## File and path macros.

# We want to have Makefile in / and have source and builds separated. Therefore Make can't find targets in CWD so we have to tell is where to look for targets.
VPATH = $(SRC_DIR) $(INCLUDE_DIR)

# Project dirctories.
BUILD_DIR = build
DOC_DIR = doc
DOC_GEN_API_DIR = $(DOC_GEN_DIR)/api
DOC_GEN_DIR = $(DOC_DIR)/gen
DOC_GEN_FULL_DIR = $(DOC_GEN_DIR)/full
GEN_DIR = gen
INCLUDE_DIR = include
LC_DIR = lc
INSTALL_LIB_DIR = /usr/local/lib/
INSTALL_INCLUDE_DIR = /usr/local/include/firefly/
SRC_DIR = src
TESTFILES_DIR = testfiles

# Package/namespace dirs
NS_PROTOCOL_DIR = $(BUILD_DIR)/protocol
NS_EVENTQUEUE_DIR = $(BUILD_DIR)/eventqueue
NS_TRANSPORT_DIR = $(BUILD_DIR)/transport
NS_TEST_DIR = $(BUILD_DIR)/test
NS_TESTPINGPONG_DIR = $(NS_PROTOCOL_DIR)/pingpong

# All volatile directories that are to be created.
DIRS_TO_CREATE= $(BUILD_DIR) $(LIB_DIR) $(DOC_GEN_DIR) $(DOC_GEN_FULL_DIR) $(DOC_GEN_API_DIR) $(GEN_DIR) $(TESTFILES_DIR) $(INSTALL_INCLUDE_DIR) $(NS_PROTOCOL_DIR) $(NS_EVENTQUEUE_DIR) $(NS_TRANSPORT_DIR) $(NS_TEST_DIR) $(NS_TESTPINGPONG_DIR)

# LabComm
LIBPATH = ../lib
LABCOMMPATH = $(LIBPATH)/labcomm
LABCOMMLIBPATH = $(LABCOMMPATH)/lib/c
LABCOMMC = $(LABCOMMPATH)/compiler/labComm.jar

# FreeRTOS and LWIP
RTOS_BASE = ../ft-sense/lib/freertos/
RTOS_SOURCE_DIR = $(RTOS_BASE)/Source
RTOS_COMMON_PORTS = $(RTOS_BASE)/Demo/Common
LWIP_DRIVER_DIR = $(RTOS_COMMON_PORTS)/ethernet
LWIP_SOURCE_DIR = $(LWIP_DRIVER_DIR)/lwip-1.3.0
LWIP_INCLUDE_DIR = $(LWIP_SOURCE_DIR)/src/include
LWIP_PORT_DIR = $(LWIP_SOURCE_DIR)/contrib/port/FreeRTOS/LM3S/arch
WEB_SERVER_DIR = $(LWIP_SOURCE_DIR)/Apps
LUMINARY_DRIVER_DIR = $(RTOS_COMMON_PORTS)/drivers/LuminaryMicro
COMMOM_INCLUDE = $(RTOS_COMMON_PORTS)/include
COMMOM_ADC = ../ft-sense/src/common/
DRIVERLIB_DIR = ../ft-sense/lib/driverlib/

# Tagsfile
TAGSFILE_VIM = tags
TAGSFILE_EMACS = TAGS

## Target macros and translations.

## LabComm
# LabComm sample files to used for code generation.
LC_FILE_NAMES = firefly_protocol.lc test.lc
LC_FILES = $(addprefix $(LC_DIR)/, $(LC_FILE_NAMES))
# LabComm generated files to be compiled.
GEN_FILES= $(patsubst $(LC_DIR)/%.lc,$(GEN_DIR)/%.h,$(LC_FILES)) $(patsubst $(LC_DIR)/%.lc,$(GEN_DIR)/%.c,$(LC_FILES))
GEN_OBJ_FILES= $(patsubst %.h,$(BUILD_DIR)/%.o,$(filter-out %.c,$(GEN_FILES)))

## Firefly
# Source files for libfirefly.
FIREFLY_SRC = $(wildcard $(SRC_DIR)/protocol/**/*.c) $(wildcard $(SRC_DIR)/eventqueue/**/*.c)

# Disable default error handler that prints with fprintf. If set to true, you
# must provide an own implementation at link time.
# Set with $make -e FIREFLY_ERROR_USER_DEFINED=true
ifneq ($(FIREFLY_ERROR_USER_DEFINED),true)
	FIREFLY_SRC += firefly_errors.c
endif

# Object files from sources.
FIREFLY_OBJS= $(patsubst %.c,$(BUILD_DIR)/%.o,$(FIREFLY_SRC)) $(BUILD_DIR)/$(GEN_DIR)/firefly_protocol.o

## Libraries to build.
LIBS=$(patsubst %,$(BUILD_DIR)/lib%.a,firefly ) # TODO add transport_udp_posix transport_udp_lwip

## Tests
TEST_SRC = $(wildcard $(SRC_DIR)/test/**/*.c)
TEST_OBJS= $(patsubst %.c,$(BUILD_DIR)/%.o,$(TEST_SRC)) $(BUILD_DIR)/$(GEN_DIR)/test.o
TEST_PROGS = $(addsuffix _main,$(addprefix $(BUILD_DIR)/test/test_,protocol transport event))





### Targets

## Setting targets.
# Non-file targets.
.PHONY: all clean clean cleaner doc doc-api doc-api-open doc-full doc-full-open doc-open install tags-all test uninstall

# This will enable expression involving automatic variables in the prerequisities list. It must be defined before any usage of these feauteres.
.SECONDEXPANSION:

## Real targets.

# target: all - Build all libs and tests.
all: $(BUILD_DIR) $(LIBS) $(TEST_PROGS) $(TAGSFILE_VIM) $(TAGSFILE_EMACS)

# target: build/libfirefly.a  - Build static library for firefly.
$(BUILD_DIR)/libfirefly.a: $(FIREFLY_OBJS)
	ar -rc $@ $^

# Create directories as needed.
$(DIRS_TO_CREATE):
	mkdir -p $@

# Usually this is an implicit rule but it does not work when the target dir is not the same as the source.
# TODO make protocol specific since they need different includes.
$(BUILD_DIR)/%.o: %.c $$(dir $$@)
	mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) -o $@ $<















# Linking for test programs.
$(BUILD_DIR)/test/test_protocol_main: $(patsubst %,$(BUILD_DIR)/test/%.o,test_protocol_main test_labcomm_utils test_proto_chan test_proto_translc test_proto_protolc test_proto_errors proto_helper)
$(BUILD_DIR)/test/test_transport_main: $(patsubst %,$(BUILD_DIR)/test/%.o,test_transport_main test_transport_udp_posix)
$(BUILD_DIR)/test/test_event_main: $(patsubst %,$(BUILD_DIR)/test/%.o,test_event_main) $(patsubst %,$(BUILD_DIR)/%.o,eventqueue/firefly_event_queue)



## Utility targets

# target: tags-all  - Generate all tagsfiles.
tags-all: $(TAGSFILE_VIM) $(TAGSFILE_EMACS)

# target: tags  - Generate a vim-firendly tagsfile.
$(TAGSFILE_VIM):
	ctags -R --tag-relative=yes -f $@

# target: TAGS  - Generate a emacs-firendly tagsfile.
$(TAGSFILE_EMACS):
	ctags --recurse --tag-relative=yes -e -f $@

# target: install - Install libraries and headers to your system (/usr/local/{lib,include}).
# TODO test this.
install: $(INSTALL_INCLUDE_DIR) $(LIBS) $(wildcard $(INCLUDE_DIR)/**/*.h))
	install -C $(filter %.a,$^) $(INSTALL_LIB_DIR)
	install -C $(filter %.h,$^) $(INSTALL_INCLUDE_DIR)

# target: uninstall - Undo that install done.
# TODO test this.
uninstall:
	$(RM) $(addprefix $(INSTALL_LIB_DIR)/,$(LIBS))
	$(RM) -r $(INSTALL_INCLUDE_DIR)

# target: help - Display all targets.
help:
	@egrep "#\starget:" [Mm]akefile  | sed 's/\s-\s/\t\t\t/' | cut -d " " -f3- | sort -d

# target: clean  - Clean most generated files..
clean:
	$(RM) $(LIBS)
	$(RM) $(FIREFLY_OBJS)
	$(RM) $(FIREFLY_OBJS:.o=.d)
	$(RM) $(TEST_EVENT_OBJS)
	$(RM) $(TEST_PROTO_OBJS)
	$(RM) $(TEST_TRANSP_OBJS)
	$(RM) $(TEST_PROGS)
	$(RM) $(GEN_DIR)/*
#	$(RM) $(addprefix $(TESTFILES_DIR)/,$(addsuffix .enc, data sig))
	$(RM) $(TESTFILES_DIR)/*
	@echo "======Cleaning LabComm======"
	$(MAKE) -C $(LABCOMMLIBPATH) distclean
	@echo "======End cleaning LabComm======"
	$(RM) $(TAGSFILE_VIM) $(TAGSFILE_EMACS)

# target: cleaner - Clean all generated files.
cleaner: clean
	$(RM) -r $(DOC_GEN_DIR)
	$(RM) -r $(BUILD_DIR)
	$(RM) -r $(LIB_DIR)
	$(RM) -r $(GEN_DIR)
	$(RM) -r $(TESTFILES_DIR)
	@echo "======Cleaning LabComm compiler======"
	cd $(LABCOMMPATH)/compiler; ant clean
	@echo "======End cleaning LabComm compiler======"
