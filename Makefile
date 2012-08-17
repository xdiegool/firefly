# TODO .d files
# TODO labcomm
# TODO update clean(er) targets

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
INC_COMMON = $(addprefix -I, \
		. \
		$(INCLUDE_DIR) \
		$(SRC_DIR) \
		)


INC_FIREFLY = $(addprefix -I, \
	      $(LABCOMMLIBPATH) \
	      )

INC_FREERTOS_LWIP= $(addprefix -I, \
			$(RTOS_SOURCE_DIR)/include \
       			$(LWIP_SOURCE_DIR)/src/include/ipv4 \
			$(LWIP_SOURCE_DIR)/src/include \
			$(LWIP_SOURCE_DIR)/contrib/port/FreeRTOS/LM3S \
			$(COMMOM_INCLUDE) \
			$(LWIP_DRIVER_DIR) \
			$(DRIVERLIB_DIR) \
			$(DRIVERLIB_DIR)/inc \
			$(RTOS_SOURCE_DIR)/portable/GCC/ARM_CM3 \
			$(WEB_SERVER_DIR) \
			$(LUMINARY_DRIVER_DIR) \
			$(LWIP_PORT_DIR) \
			$(LWIP_INCLUDE_DIR) \
			$(COMMOM_ADC) \
			../ft-sense/lib/ \
			)

CFLAGS = -std=c99 $(ERRFLAGS) $(INC_COMMON)

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

# Namespace/package dirs
NS_GEN_DIR = $(BUILD_DIR)/$(GEN_DIR)
NS_PROTOCOL_DIR = $(BUILD_DIR)/protocol
NS_UTILS_DIR = $(BUILD_DIR)/utils
NS_TRANSPORT_DIR = $(BUILD_DIR)/transport
NS_TEST_DIR = $(BUILD_DIR)/test
NS_TESTPINGPONG_DIR = $(NS_TEST_DIR)/pingpong

# All volatile directories that are to be created.
DIRS_TO_CREATE= $(BUILD_DIR) $(LIB_DIR) $(DOC_GEN_DIR) $(DOC_GEN_FULL_DIR) $(DOC_GEN_API_DIR) $(GEN_DIR) $(TESTFILES_DIR) $(INSTALL_INCLUDE_DIR) $(NS_GEN_DIR) $(NS_PROTOCOL_DIR) $(NS_UTILS_DIR) $(NS_TRANSPORT_DIR) $(NS_TEST_DIR) $(NS_TESTPINGPONG_DIR)

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

## Libraries to build.
LIBS=$(patsubst %,$(BUILD_DIR)/lib%.a,firefly ) # TODO add transport_udp_posix transport_udp_lwip

# Automatically generated prerequisities files.
DFILES= $(patsubst %.o,%.d,$(FIREFLY_OBJS) $(TEST_OBJS) $(GEN_OBJS))

## LabComm
# LabComm sample files to used for code generation.
LC_FILE_NAMES = firefly_protocol.lc test.lc
LC_FILES = $(addprefix $(LC_DIR)/, $(LC_FILE_NAMES))
# LabComm generated files to be compiled.
GEN_FILES= $(patsubst $(LC_DIR)/%.lc,$(GEN_DIR)/%.h,$(LC_FILES)) $(patsubst $(LC_DIR)/%.lc,$(GEN_DIR)/%.c,$(LC_FILES))
GEN_OBJS= $(patsubst %.c,$(BUILD_DIR)/%.o,$(filter-out %.h,$(GEN_FILES)))

## Firefly
# Source files for libfirefly.
FIREFLY_SRC = $(shell find $(SRC_DIR)/protocol/ -type f -name '*.c' -print | sed 's/^$(SRC_DIR)\///') $(filter-out utils/firefly_errors.c,$(shell find $(SRC_DIR)/utils/ -type f -name '*.c' -print| sed 's/^$(SRC_DIR)\///')) $(BUILD_DIR)/$(GEN_DIR)/firefly_protocol.o

# Disable default error handler that prints with fprintf. If set to true, you
# must provide an own implementation at link time.
# Set with $make -e FIREFLY_ERROR_USER_DEFINED=true
ifneq ($(FIREFLY_ERROR_USER_DEFINED),true)
	FIREFLY_SRC += utils/firefly_errors.c
endif

# Object files from sources.
FIREFLY_OBJS= $(patsubst %.c,$(BUILD_DIR)/%.o,$(FIREFLY_SRC))


## Tests
TEST_SRC = $(shell find $(SRC_DIR)/test/ -type f -name '*.c' | sed 's/^$(SRC_DIR)\///')
TEST_OBJS= $(patsubst %.c,$(BUILD_DIR)/%.o,$(TEST_SRC)) $(BUILD_DIR)/$(GEN_DIR)/test.o
TEST_PROGS = $(addsuffix _main,$(addprefix $(BUILD_DIR)/test/test_,protocol transport event))





### Targets

## Setting targets.
# Non-file targets.
.PHONY: all clean clean cleaner doc doc-api doc-api-open doc-full doc-full-open doc-open install tags-all test uninstall

# This will enable expression involving automatic variables in the prerequisities list. It must be defined before any usage of these feauteres.
.SECONDEXPANSION:


## General targets.

# target: all - Build all libs and tests.
# TODO enable targets successively
#all: $(LIBS) $(TEST_PROGS) $(TAGSFILE_VIM) $(TAGSFILE_EMACS)
all: $(LIBS)

echo:
	@echo $(FIREFLY_SRC)

# target: $(BUILD_DIR) - Everything that is to be build depend on the build dir.
$(BUILD_DIR)/%: $(BUILD_DIR)

# target: $(DIRS_TO_CREATE) - Create directories as needed.
$(DIRS_TO_CREATE): 
	mkdir -p $@


## Labcomm targets
# target: $(LABCOMMC) - Construct the LabComm compiler.
$(LABCOMMC):
	@echo "======Building LabComm compiler======"
	cd $(LABCOMMPATH)/compiler; ant jar
	@echo "======End building LabComm compiler======"

# target: $(LABCOMMLIBPATH/liblabconn.a) - Build static LabComm library.
$(LABCOMMLIBPATH)/liblabcomm.a:
	@echo "======Building LabComm======"
	$(MAKE) -C $(LABCOMMLIBPATH) -e LABCOMM_NO_EXPERIMENTAL=true all
	@echo "======End building LabComm======"

$(GEN_DIR)/%.c $(GEN_DIR)/%.h: $(LC_DIR)/%.lc $(LABCOMMC) $(GEN_DIR)
	java -jar $(LABCOMMC) --c=$(patsubst %.h,%.c,$@) --h=$(patsubst %.c,%.h,$@) $<

# Let the LabComm .o file depend on the generated .c and .h files.
$(GEN_OBJ_FILES): $$(patsubst $$(BUILD_DIR)/%.o,%.c,$$@) $$(patsubst $$(BUILD_DIR)/%.o,%.h,$$@)

$(BUILD_DIR)/gen/%.o: %.c $$(dir $$@)
	$(CC) -c $(CFLAGS) -L$(LABCOMMLIBPATH) -o $@ -llabcomm $<

## Firefly targets

# target: build/libfirefly.a  - Build static library for firefly.
$(BUILD_DIR)/libfirefly.a: $(FIREFLY_OBJS)
	ar -rc $@ $^

# Usually these is an implicit rule but it does not work when the target dir is not the same as the source. Anyhow we need to add some includes for the different namespaces so it doesnt matter.
$(BUILD_DIR)/protocol/%.o: protocol/%.c $$(@D)
	$(CC) -c $(CFLAGS) $(INC_FIREFLY) -o $@ $<

$(BUILD_DIR)/utils/%.o: utils/%.c $$(@D)
	$(CC) -c $(CFLAGS) $(INC_FIREFLY) -o $@ $<










# Linking for test programs.
$(BUILD_DIR)/test/test_protocol_main: $(patsubst %,$(BUILD_DIR)/test/%.o,test_protocol_main test_labcomm_utils test_proto_chan test_proto_translc test_proto_protolc test_proto_errors proto_helper)
$(BUILD_DIR)/test/test_transport_main: $(patsubst %,$(BUILD_DIR)/test/%.o,test_transport_main test_transport_udp_posix)
$(BUILD_DIR)/test/test_event_main: $(patsubst %,$(BUILD_DIR)/test/%.o,test_event_main) $(patsubst %,$(BUILD_DIR)/%.o,eventqueue/firefly_event_queue)


## Utility targets

# target: %.d - Automatic prerequisities generation.
$(BUILD_DIR)/%.d: %.c $$(@D) $(GEN_FILES)
	@$(SHELL) -ec '$(CC) -M $(CFLAGS) $(INC_FIREFLY) $< \
	| sed '\''s/\(.*\)\.o[ :]*/$(patsubst %.d,%.o,$(subst /,\/,$@)) : /g'\'' > $@; \
	[ -s $@ ] || rm -f $@'

## Include dependency files and ignore ".d file missing" the first time.
-include $(DFILES) /dev/null

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
install: $(INSTALL_INCLUDE_DIR) $(LIBS) $$(shell find $$(INCLUDE_DIR) -type f -name '*.h')
	install -C $(filter %.a,$^) $(INSTALL_LIB_DIR)
	install -C $(wildcard $(INCLUDE_DIR)/*) $(INSTALL_INCLUDE_DIR)

# target: uninstall - Undo that install done.
# TODO test this.
uninstall:
	$(RM) $(addprefix $(INSTALL_LIB_DIR)/,$(LIBS))
	$(RM) -r $(INSTALL_INCLUDE_DIR)

# target: help - Display all targets.
help:
	@egrep "#\starget:" [Mm]akefile  | sed 's/\s-\s/\t\t\t/' | cut -d " " -f3- | sort -d

# target: clean  - Clean most compiled or generated files.
clean:
	$(RM) $(LIBS)
	$(RM) $(FIREFLY_OBJS)
	$(RM) $(GEN_OBJS)
	$(RM) $(TEST_OBJS)
	$(RM) $(TEST_PROGS)
	$(RM) $(DFILES)
	$(RM) $(wildcard $(GEN_DIR)/*)
	$(RM) $(wildcard $(TESTFILES_DIR)/*)
	@echo "======Cleaning LabComm======"
	$(MAKE) -C $(LABCOMMLIBPATH) distclean
	@echo "======End cleaning LabComm======"
	$(RM) $(TAGSFILE_VIM) $(TAGSFILE_EMACS)

# target: cleaner - Clean all created files.
cleaner: clean
	$(RM) -r $(DOC_GEN_DIR)
	$(RM) -r $(BUILD_DIR)
	$(RM) -r $(LIB_DIR)
	$(RM) -r $(GEN_DIR)
	$(RM) -r $(TESTFILES_DIR)
	@echo "======Cleaning LabComm compiler======"
	cd $(LABCOMMPATH)/compiler; ant clean
	@echo "======End cleaning LabComm compiler======"
