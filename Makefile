# Makefile for project Firely. run `make help` for help on targets.
# Modeline {
# vi: foldmarker={,} foldmethod=marker foldlevel=0
# }

# TODO testsprograms
# TODO fix so pinpong is run automatically with make test
# TODO copy residual functionallity
# TODO update clean(er) targets
# TODO fix so multiple targets can be run like `make clean all`

### Macros {
## Compiler options. {
# NOTE the order of macro declaration is, apparently, of importance. E.g. having "ifeq(...) CFLAGS+=..." before "CFLAGS=.." will not work.

# Use LLVM clang if it's found.
CC = $(shell hash clang 2>/dev/null && echo clang || echo cc)

# Error flags to the compiler.
ERRFLAGS = -Wall -Wextra

# Common flags to the compiler.
CFLAGS = -std=c99 $(ERRFLAGS) $(INC_COMMON)


### Includes {
# Common include paths.
INC_COMMON = $(addprefix -I, \
		. \
		$(INCLUDE_DIR) \
		$(SRC_DIR) \
		)


# Inluces for $(LIB_FIREFLY_NAME).
INC_FIREFLY = $(addprefix -I, \
		$(LABCOMMLIBPATH) \
		)

# Inluces for $(LIB_TRANSPORT_UDP_POSIX_NAME).
INC_TRANSPORT_UDP_POSIX = $(addprefix -I, \
		$(LABCOMMLIBPATH) \
	      )

# Inluces for $(LIB_TRANSPORT_UDP_LWIP_NAME).
INC_TRANSPORT_UDP_LWIP = $(addprefix -I, \
		$(LABCOMMLIBPATH) \
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

# Includes for test programs.
INC_TEST = $(addprefix -I, \
		$(LABCOMMLIBPATH) \
		)

### }

### Linker flags. {

# Common linker flags.
LDFLAGS=

# Linker flags for test programs.
LDFlAGS_TEST= -L$(LABCOMMLIBPATH)

# Libraries to link with test programs.
LDLIBS_TEST= -llabcomm -lcunit -lpthread

### }

### Conditional flags {
OPTLVL= 2
DEBUG = true
DEBUGOPT = -g

# Change with $make -e DEBUG=false
ifeq ($(DEBUG), true)
	# Disalbe optimizations when deubbing.
	CFLAGS += $(DEBUGOPT) -O0
	LDFLAGS += -g
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
### }

## }

## File and path macros. {

# We want to have Makefile in / and have source and builds separated. Therefore Make can't find targets in CWD so we have to tell is where to look for targets.
VPATH = $(SRC_DIR) $(INCLUDE_DIR)

# Project dirctories.
BUILD_DIR = build
CLI_DIR = cli
DOC_DIR = doc
DOC_GEN_API_DIR = $(DOC_GEN_DIR)/api
DOC_GEN_DIR = $(DOC_DIR)/gen
DOC_GEN_FULL_DIR = $(DOC_GEN_DIR)/full
GEN_DIR = gen
INCLUDE_DIR = include
INSTALL_INCLUDE_DIR = /usr/local/include/firefly/
INSTALL_LIB_DIR = /usr/local/lib/
LC_DIR = lc
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

## }

## Target macros and translations. {

### Our libraries {
LIB_FIREFLY_NAME = firefly
LIB_TRANSPORT_UDP_POSIX_NAME = transport-udp-posix
LIB_TRANSPORT_UDP_LWIP_NAME = transport-udp-lwip

# Libraries to build.
LIBS=$(patsubst %,$(BUILD_DIR)/lib%.a,$(LIB_FIREFLY_NAME) $(LIB_TRANSPORT_UDP_POSIX_NAME) $(LIB_TRANSPORT_UDP_LWIP_NAME))

# Automatically generated prerequisities files.
DFILES= $(patsubst %.o,%.d,$(FIREFLY_OBJS) $(TEST_OBJS) $(GEN_OBJS))

### }

### LabComm {
# LabComm sample files to used for code generation.
LC_FILE_NAMES = firefly_protocol.lc test.lc
LC_FILES = $(addprefix $(LC_DIR)/, $(LC_FILE_NAMES))
# LabComm generated files to be compiled.
GEN_FILES= $(patsubst $(LC_DIR)/%.lc,$(GEN_DIR)/%.h,$(LC_FILES)) $(patsubst $(LC_DIR)/%.lc,$(GEN_DIR)/%.c,$(LC_FILES))
GEN_OBJS= $(patsubst %.c,$(BUILD_DIR)/%.o,$(filter-out %.h,$(GEN_FILES)))
### }

### Firefly {
# Source files for libfirefly.
FIREFLY_SRC = $(shell find $(SRC_DIR)/protocol/ -type f -name '*.c' -print | sed 's/^$(SRC_DIR)\///') $(filter-out utils/firefly_errors.c,$(shell find $(SRC_DIR)/utils/ -type f -name '*.c' -print| sed 's/^$(SRC_DIR)\///')) $(GEN_DIR)/firefly_protocol.c

# Disable default error handler that prints with fprintf. If set to true, you
# must provide an own implementation at link time.
# Set with $make -e FIREFLY_ERROR_USER_DEFINED=true
ifneq ($(FIREFLY_ERROR_USER_DEFINED),true)
	FIREFLY_SRC += utils/firefly_errors.c
endif

# Object files from sources.
FIREFLY_OBJS= $(patsubst %.c,$(BUILD_DIR)/%.o,$(FIREFLY_SRC))

### }

### Transport UPD POSIX {
# Source files for lib$(LIB_TRANSPORT_UDP_POSIX_NAME).a
TRANSPORT_UDP_POSIX_SRC = $(shell find $(SRC_DIR)/transport/ -type f -name '*udp_posix*.c' -print | sed 's/^$(SRC_DIR)\///')

# Object files from sources.
TRANSPORT_UDP_POSIX_OBJS= $(patsubst %.c,$(BUILD_DIR)/%.o,$(TRANSPORT_UDP_POSIX_SRC))

### }

### Transport UPD LWIP {
# Source files for lib$(LIB_TRANSPORT_UDP_LWIP_NAME).a
TRANSPORT_UDP_LWIP_SRC = $(shell find $(SRC_DIR)/transport/ -type f -name '*udp_lwip*.c' -print | sed 's/^$(SRC_DIR)\///')

# Object files from sources.
TRANSPORT_UDP_LWIP_OBJS= $(patsubst %.c,$(BUILD_DIR)/%.o,$(TRANSPORT_UDP_LWIP_SRC))

### }

### Tests {
TEST_SRC = $(shell find $(SRC_DIR)/test/ -type f -name '*.c' | sed 's/^$(SRC_DIR)\///')
TEST_OBJS= $(patsubst %.c,$(BUILD_DIR)/%.o,$(TEST_SRC))
TEST_PROGS = $(shell find $(SRC_DIR)/test/ -type f -name '*_main.c' | sed -e 's/^$(SRC_DIR)\//$(BUILD_DIR)\//' -e 's/\.c//')

### }

## }
### }

### Targets {
## Special built-in targets {
# Non-file targets.
.PHONY: all clean cleaner doc doc-api doc-api-open doc-full doc-full-open doc-open install tags-all test uninstall

# This will enable expression involving automatic variables in the prerequisities list. It must be defined before any usage of these feauteres.
.SECONDEXPANSION:

# Keep my damn labcomm files! Make treats the generated files as intermediate and deletes them when it think that they're not needed anymore. Usually the .PRECIOUS target should solve this but it does not. .SECONDARY solves it though.
# References: http://darrendev.blogspot.se/2008/06/stopping-make-delete-intermediate-files.html
.SECONDARY: $(wildcard $(GEN_DIR)/firefly_protocol.*)

## }

## General targets. {

# target: all - Build all libs and tests.
# TODO enable targets successively
#all: $(LIBS) $(TEST_PROGS) $(TAGSFILE_VIM) $(TAGSFILE_EMACS)
all: $(LIBS) $(TEST_PROGS)

# target: $(BUILD_DIR) - Everything that is to be build depends on the build dir.
$(BUILD_DIR)/%: $(BUILD_DIR)

# target: $(DIRS_TO_CREATE) - Create directories as needed.
$(DIRS_TO_CREATE): 
	mkdir -p $@


### Labcomm targets {
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

# target: GENFILES - Generate labcomm files. 
# NOTE Can not use "$(@D)" since it causes realloc invalid next size.
$(GEN_DIR)/%.c $(GEN_DIR)/%.h: $(LC_DIR)/%.lc $(LABCOMMC) |$(GEN_DIR)
	java -jar $(LABCOMMC) --c=$(patsubst %.h,%.c,$@) --h=$(patsubst %.c,%.h,$@) $<

$(BUILD_DIR)/gen/%.o: %.c $$(dir $$@)
	$(CC) -c $(CFLAGS) -L$(LABCOMMLIBPATH) -o $@ -llabcomm $<

### }

### Firefly targets {

# target: build/libfirefly.a  - Build static library for firefly.
$(BUILD_DIR)/libfirefly.a: $(FIREFLY_OBJS)
	ar -rc $@ $^

# Usually these is an implicit rule but it does not work when the target dir is not the same as the source. Anyhow we need to add some includes for the different namespaces so it doesnt matter.
# "|<target>" means that <target> is order-only so if it changed it won't cause recompilcation of the target. This is needed here since the modify timestamp is changed when new files are added/removed from a dirctory which will cause constant unneccessary recomplications. This soluton requres GNU Make >= 3.80.
# References: http://darrendev.blogspot.se/2008/06/stopping-make-delete-intermediate-files.html
$(BUILD_DIR)/protocol/%.o: protocol/%.c |$$(@D)
	$(CC) -c $(CFLAGS) $(INC_FIREFLY) -o $@ $<

$(BUILD_DIR)/utils/%.o: utils/%.c |$$(@D)
	$(CC) -c $(CFLAGS) $(INC_FIREFLY) -o $@ $<

# Delete compiler warning flags. LabComm's errors is not our fault.
$(BUILD_DIR)/gen/%.o: gen/%.c |$$(@D)
	$(CC) -c $(filter-out -W%,$(CFLAGS)) $(INC_FIREFLY) -o $@ $<


### }

### Transport UDP POSIX targets {

# target: build/lib$(LIB_TRANSPORT_UDP_POSIX_NAME).a  - Build static library for transport udp posix.
$(BUILD_DIR)/lib$(LIB_TRANSPORT_UDP_POSIX_NAME).a: $(TRANSPORT_UDP_POSIX_OBJS)
	ar -rc $@ $^

$(TRANSPORT_UDP_POSIX_OBJS): $$(patsubst $$(BUILD_DIR)/%.o,%.c,$$@) |$$(@D)
	$(CC) -c $(CFLAGS) $(INC_TRANSPORT_UDP_POSIX) -o $@ $<

### }

### Transport UDP LWIP targets {

# target: build/lib$(LIB_TRANSPORT_UDP_LWIP_NAME).a  - Build static library for transport udp lwip.
$(BUILD_DIR)/lib$(LIB_TRANSPORT_UDP_LWIP_NAME).a: $(TRANSPORT_UDP_LWIP_OBJS)
	ar -rc $@ $^

$(TRANSPORT_UDP_LWIP_OBJS): $$(patsubst $$(BUILD_DIR)/%.o,%.c,$$@) |$$(@D)
	$(CC) -c $(CFLAGS) $(INC_TRANSPORT_UDP_LWIP) -o $@ $<


### }

### Test programs. {
$(TEST_OBJS): $$(patsubst $$(BUILD_DIR)/%.o,%.c,$$@) |$$(@D)
	$(CC) -c $(CFLAGS) $(INC_TEST) -o $@ $<

# The test programs all depends on the libraries it uses and the test files output directory.
$(TEST_PROGS): $$(patsubst %,$$(BUILD_DIR)/lib%.a,$$(LIB_FIREFLY_NAME) $$(LIB_TRANSPORT_UDP_POSIX_NAME)) $(LABCOMMLIBPATH)/liblabcomm.a |$(TESTFILES_DIR)

# Main test program for the protocol tests.
$(BUILD_DIR)/test/test_protocol_main: $(patsubst %,$(BUILD_DIR)/test/%.o,test_protocol_main test_labcomm_utils test_proto_chan test_proto_translc test_proto_protolc test_proto_errors proto_helper) $(BUILD_DIR)/$(GEN_DIR)/test.o
	$(CC) $(LDFLAGS) $(LDFlAGS_TEST) $^ $(LDLIBS_TEST) -o $@ 

# Main test program for the transport tests.
$(BUILD_DIR)/test/test_transport_main: $(patsubst %,$(BUILD_DIR)/test/%.o,test_transport_main test_transport_udp_posix)
	$(CC) $(LDFLAGS) $(LDFlAGS_TEST) $^ $(LDLIBS_TEST) -o $@ 

# Main test program for the event queue tests.
$(BUILD_DIR)/test/test_event_main: $(patsubst %,$(BUILD_DIR)/test/%.o,test_event_main) $(patsubst %,$(BUILD_DIR)/%.o,utils/firefly_event_queue)
	$(CC) $(LDFLAGS) $(LDFlAGS_TEST) $^ $(LDLIBS_TEST) -o $@ 

### }
## }

## Utility targets {

# target: test - Run all tests.
test: $(TEST_PROGS)
	@for prog in $^; do \
		echo "=========================>BEGIN TEST: $${prog}"; \
		./$$prog; \
		echo "=========================>END TEST: $${prog}"; \
	done

# target: testc - Run all tests with sound effects!
testc:
	$(CLI_DIR)/celebrate.sh

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
	$(RM) $(GEN_OBJS)
	$(RM) $(FIREFLY_OBJS)
	$(RM) $(TRANSPORT_UDP_POSIX_OBJS)
	$(RM) $(TRANSPORT_UDP_LWIP_OBJS)
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

## }
### }
