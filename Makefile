# Makefile for project Firely. run `make help` for help on targets.
# Modeline {
# vi: foldmarker={,} foldmethod=marker foldlevel=0
# }

### Macros {
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
INSTALL_INCLUDE_DIR = /usr/local/include/firefly
INSTALL_LIB_DIR = /usr/local/lib
LC_DIR = lc
SRC_DIR = src
TESTFILES_DIR = testfiles

# Namespace/package dirs
NS_GEN_DIR = $(BUILD_DIR)/$(GEN_DIR)
NS_PROTOCOL_DIR = $(BUILD_DIR)/protocol
NS_UTILS_DIR = $(BUILD_DIR)/utils
NS_TRANSPORT_DIR = $(BUILD_DIR)/transport
NS_TEST_DIR = $(BUILD_DIR)/test
NS_TEST_SYSTEM_DIR = $(BUILD_DIR)/test/system
NS_TESTPINGPONG_DIR = $(NS_TEST_DIR)/pingpong

# All volatile directories that are to be created.
DIRS_TO_CREATE= $(BUILD_DIR) $(LIB_DIR) $(DOC_GEN_DIR) $(DOC_GEN_FULL_DIR) $(DOC_GEN_API_DIR) $(GEN_DIR) $(TESTFILES_DIR) $(INSTALL_INCLUDE_DIR) $(NS_GEN_DIR) $(NS_PROTOCOL_DIR) $(NS_UTILS_DIR) $(NS_TRANSPORT_DIR) $(NS_TEST_DIR) $(NS_TESTPINGPONG_DIR) $(NS_TEST_SYSTEM_DIR)

# LabComm
LIBPATH = ../lib
LABCOMMPATH = $(LIBPATH)/labcomm
LABCOMMLIBPATH = $(LABCOMMPATH)/lib/c
LABCOMMC = $(LABCOMMPATH)/compiler/labComm.jar

# FreeRTOS and LWIP
COMPILER_DIR=$$HOME/bin/arm-2010q1/bin
FT_SENSE_DIR = ../ft-sense
LDSCRIPT= $(FT_SENSE_DIR)/src/adc_freertos_lwip/standalone.ld
RTOS_BASE = $(FT_SENSE_DIR)/lib/freertos/
RTOS_SOURCE_DIR = $(RTOS_BASE)/Source
RTOS_COMMON_PORTS = $(RTOS_BASE)/Demo/Common
LWIP_DRIVER_DIR = $(RTOS_COMMON_PORTS)/ethernet
LWIP_SOURCE_DIR = $(LWIP_DRIVER_DIR)/lwip-1.3.0
LWIP_INCLUDE_DIR = $(LWIP_SOURCE_DIR)/src/include
LWIP_PORT_DIR = $(LWIP_SOURCE_DIR)/contrib/port/FreeRTOS/LM3S/arch
WEB_SERVER_DIR = $(LWIP_SOURCE_DIR)/Apps
LUMINARY_DRIVER_DIR = $(RTOS_COMMON_PORTS)/drivers/LuminaryMicro
COMMOM_INCLUDE = $(RTOS_COMMON_PORTS)/include
COMMOM_ADC = $(FT_SENSE_DIR)/src/common/
DRIVERLIB_DIR = $(FT_SENSE_DIR)/lib/driverlib/

# Tagsfile
TAGSFILE_VIM = tags
TAGSFILE_EMACS = TAGS

## }

## Compiler options. {
# NOTE the order of macro declaration is, apparently, of importance. E.g. having "ifeq(...) CFLAGS+=..." before "CFLAGS=.." will not work.

# Use LLVM clang if it's found.
CC = $(shell hash clang 2>/dev/null && echo clang || echo cc)
# Use arm toolchain to compile for arm
CC_ARM = arm-none-eabi-gcc

# Error flags to the compiler.
ERRFLAGS = -Wall -Wextra

# Common flags to the compiler.
CFLAGS_COM = -std=c99 $(ERRFLAGS) $(INC_COMMON) -D DEBUG

# x86 specific compiler flags
CFLAGS = $(CFLAGS_COM)

# ARM specific compiler flags
CFLAGS_ARM = $(CFLAGS_COM) -Wfloat-equal -Werror-implicit-function-declaration \
		  -mthumb -mcpu=cortex-m3 -T$(LDSCRIPT) \
		  -ffunction-sections -fdata-sections \
		  $(INC_TRANSPORT_UDP_LWIP) \
		  -D sprintf=usprintf \
		  -D snprintf=usnprintf \
		  -D vsnprintf=uvsnprintf \
		  -D printf=uipprintf \
		  -D malloc=pvPortMalloc \
		  -D calloc=pvPortCalloc \
		  -D free=vPortFree \
		  -D ARM_CORTEXM3_CODESOURCERY \
		  -D LABCOMM_NO_STDIO \
		  -D GCC_ARMCM3=1

# Specify alternative memory allocation/deallocation functions
ifdef FIREFLY_MALLOC
CFLAGS_COM += -D FIREFLY_MALLOC\(size\)=$(FIREFLY_MALLOC)
endif
ifdef FIREFLY_FREE
CFLAGS_COM += -D FIREFLY_FREE\(p\)=$(FIREFLY_FREE)
endif


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

# Inluces common to all transport libs
INC_TRANSPORT_COMMON = $(addprefix -I, \
		$(LABCOMMLIBPATH) \
	      )

# Inluces for $(LIB_TRANSPORT_UDP_POSIX_NAME).
INC_TRANSPORT_UDP_POSIX = $(addprefix -I, \
		$(LABCOMMLIBPATH) \
	      )

# Inluces for $(LIB_TRANSPORT_ETH_POSIX_NAME).
INC_TRANSPORT_ETH_POSIX = $(addprefix -I, \
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
		$(FT_SENSE_DIR)/lib/ \
		$(FT_SENSE_DIR)/src/adc_freertos_lwip/src \
		)

# Inluces for $(LIB_TRANSPORT_ETH_STELLARIS_NAME).
INC_TRANSPORT_ETH_STELLARIS = $(INC_TRANSPORT_UDP_LWIP)

# Includes for test programs.
INC_TEST = $(addprefix -I, \
		$(LABCOMMLIBPATH) \
		)

### }

### Linker flags. {

# Common linker flags.
LDFLAGS=

# Linker flags for test programs.
LDFLAGS_TEST= -L$(BUILD_DIR) -L$(LABCOMMLIBPATH)

# Libraries to link with test programs.
LDLIBS_TEST= -llabcomm -lcunit -lpthread -lrt

### }

### Conditional flags {
DEBUG = true
# DEBUG = false
DEBUGOPT = -g
# Change with $make -e DEBUG=false
ifeq ($(DEBUG), true)
	# Disalbe optimizations when deubbing.
	CFLAGS_COM += $(DEBUGOPT) -O0
	LDFLAGS += -g
else
	CFLAGS += -O2
	CFLAGS_ARM += -Os
endif

# Enable instrumented compilation with $make -e COVERAGE=true
ifeq ($(COVERAGE), true)
	CFLAGS_COMM += --coverage
	LDFLAGS += --coverage
endif

# Clang does not support noexecstack it seems.
ifeq ($(CC), gcc)
	CFLAGS += -z noexecstack
endif
### }

## }

## Target macros and translations. {

### Our libraries {
LIB_FIREFLY_NAME = firefly
LIB_FIREFLY_WERR_NAME = firefly-werr
LIB_TRANSPORT_UDP_POSIX_NAME = transport-udp-posix
LIB_TRANSPORT_ETH_POSIX_NAME = transport-eth-posix
LIB_TRANSPORT_UDP_LWIP_NAME = transport-udp-lwip
LIB_TRANSPORT_ETH_STELLARIS_NAME = transport-eth-stellaris

# Libraries to build.
OUR_LIBS=$(patsubst %,$(BUILD_DIR)/lib%.a,$(LIB_FIREFLY_NAME) $(LIB_TRANSPORT_UDP_POSIX_NAME) $(LIB_TRANSPORT_UDP_LWIP_NAME) $(LIB_TRANSPORT_ETH_POSIX_NAME) $(LIB_TRANSPORT_ETH_STELLARIS_NAME))

# Automatically generated prerequisities files.
DFILES= $(patsubst %.o,%.d,$(filter-out $(BUILD_DIR)/$(GEN_DIR)/firefly_protocol.o,$(FIREFLY_OBJS)) $(TEST_OBJS) $(GEN_OBJS))

### }

### LabComm {
# LabComm sample files to used for code generation.
LC_FILE_NAMES = firefly_protocol.lc test.lc pingpong.lc
LC_FILES = $(addprefix $(LC_DIR)/,$(LC_FILE_NAMES))
# LabComm generated files to be compiled.
GEN_FILES= $(patsubst $(LC_DIR)/%.lc,$(GEN_DIR)/%.h,$(LC_FILES)) $(patsubst $(LC_DIR)/%.lc,$(GEN_DIR)/%.c,$(LC_FILES))
GEN_OBJS= $(patsubst %.c,$(BUILD_DIR)/%.o,$(filter-out %.h,$(GEN_FILES)))
### }

### Firefly {
# Source files for libfirefly.
#FIREFLY_SRC = $(shell find $(SRC_DIR)/protocol/ -type f -name '*.c' -print | sed 's/^$(SRC_DIR)\///') $(filter-out utils/firefly_errors.c,$(shell find $(SRC_DIR)/utils/ -type f -name '*.c' -print| sed 's/^$(SRC_DIR)\///')) $(GEN_DIR)/firefly_protocol.c
FIREFLY_SRC = $(shell find $(SRC_DIR)/protocol/ -type f -name '*.c' -print | sed 's/^$(SRC_DIR)\///') $(patsubst %,utils/%.c, firefly_errors_utils firefly_event_queue) $(GEN_DIR)/firefly_protocol.c

FIREFLY_ERR_SRC += utils/firefly_errors.c

# Object files from sources.
FIREFLY_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(FIREFLY_SRC))
FIREFLY_ARM_OBJS = $(patsubst %.c,$(BUILD_DIR)/%-arm.o,$(FIREFLY_SRC))

FIREFLY_ERR_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(FIREFLY_ERR_SRC))

### }

### Transport common {
# Source files common to all transport libs
TRANSPORT_COMMON_SRC = transport/firefly_transport.c

# Object files from sources.
TRANSPORT_COMMON_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(TRANSPORT_COMMON_SRC))
TRANSPORT_COMMON_ARM_OBJS = $(patsubst %.c,$(BUILD_DIR)/%-arm.o,$(TRANSPORT_COMMON_SRC))

### }

### POSIX common {
# Source files common to all transport libs
TRANSPORT_POSIX_COMMON_SRC = utils/firefly_resend_posix.c

# Object files from sources.
TRANSPORT_POSIX_COMMON_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(TRANSPORT_POSIX_COMMON_SRC))

### }

### Transport UPD POSIX {
# Source files for lib$(LIB_TRANSPORT_UDP_POSIX_NAME).a
TRANSPORT_UDP_POSIX_SRC = $(shell find $(SRC_DIR)/transport/ -type f \( -name '*udp_posix*.c' \) -print | sed 's/^$(SRC_DIR)\///')

# Object files from sources.
TRANSPORT_UDP_POSIX_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(TRANSPORT_UDP_POSIX_SRC))

### }

### Transport ETH POSIX {
# Source files for lib$(LIB_TRANSPORT_ETH_POSIX_NAME).a
TRANSPORT_ETH_POSIX_SRC = $(shell find $(SRC_DIR)/transport/ -type f \( -name '*eth_posix*.c' \) -print | sed 's/^$(SRC_DIR)\///')

# Object files from sources.
TRANSPORT_ETH_POSIX_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(TRANSPORT_ETH_POSIX_SRC))

### }

### Transport UPD LWIP {
# Source files for lib$(LIB_TRANSPORT_UDP_LWIP_NAME).a
TRANSPORT_UDP_LWIP_SRC = $(shell find $(SRC_DIR)/transport/ -type f \( -name '*udp_lwip*.c' \) -print | sed 's/^$(SRC_DIR)\///')

# Object files from sources.
TRANSPORT_UDP_LWIP_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(TRANSPORT_UDP_LWIP_SRC))

### }

### Transport ETH STELLARIS {
# Source files for lib$(LIB_TRANSPORT_ETH_STELLARIS_NAME).a
TRANSPORT_ETH_STELLARIS_SRC = $(shell find $(SRC_DIR)/transport/ -type f \( -name '*eth_stellaris*.c' \) -print | sed 's/^$(SRC_DIR)\///')

# Object files from sources.
TRANSPORT_ETH_STELLARIS_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(TRANSPORT_ETH_STELLARIS_SRC))

### }

### Tests {
TEST_SRC = $(shell find $(SRC_DIR)/test/ -type f -name '*.c' | sed 's/^$(SRC_DIR)\///')
TEST_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(TEST_SRC))
TEST_UNIT_PROGS = $(patsubst %,$(BUILD_DIR)/test/%,test_event_main test_protocol_main test_transport_eth_posix_main test_transport_main test_resend_posix)
TEST_UNIT_ROOT_PROGS = $(patsubst %,$(BUILD_DIR)/test/%,test_transport_eth_posix_main)
TEST_SYSTEM_PROGS = $(patsubst %,$(BUILD_DIR)/test/%,pingpong/pingpong_main pingpong/pong_eth_main pingpong/ping_eth_main pingpong/pingpong_multi_main system/udp_posix)
TEST_SYSTEM_ROOT_PROGS =
TEST_SYSTEM_NORUN_PROGS = $(patsubst %,$(BUILD_DIR)/test/%,pingpong/pong_eth_main pingpong/ping_eth_main pingpong/pingpong_multi_main)

TEST_PROGS = $(TEST_UNIT_PROGS) $(TEST_SYSTEM_PROGS)

### }

## }
### }

### Targets {
## Special built-in targets {
# Non-file targets.
.PHONY: all clean cleaner doc doc-api doc-api-open doc-full doc-full-open doc-open install ping pong tags-all test test-system testc uninstall

# This will enable expression involving automatic variables in the prerequisities list. It must be defined before any usage of these feauteres.
.SECONDEXPANSION:

# Keep my damn labcomm files! Make treats the generated files as intermediate and deletes them when it think that they're not needed anymore. Usually the .PRECIOUS target should solve this but it does not. .SECONDARY solves it though.
# References: http://darrendev.blogspot.se/2008/06/stopping-make-delete-intermediate-files.html
.SECONDARY: $(GEN_FILES)

## }

## General targets. {

# target: all - Build everything that can be built (except documentation).
all: $(OUR_LIBS) $(TEST_PROGS) $(TAGSFILE_VIM) $(TAGSFILE_EMACS)

# $(BUILD_DIR) - Everything that is to be build depends on the build dir.
$(BUILD_DIR)/%: $(BUILD_DIR)

# $(DIRS_TO_CREATE) - Create directories as needed.
$(DIRS_TO_CREATE): 
	mkdir -p $@


### Labcomm targets {
# $(LABCOMMC) - Construct the LabComm compiler.
$(LABCOMMC):
	@echo "======Building LabComm compiler======"
	cd $(LABCOMMPATH)/compiler; ant jar
	@echo "======End building LabComm compiler======"

# $(LABCOMMLIBPATH/liblabconn.a) - Build static LabComm library.
$(LABCOMMLIBPATH)/liblabcomm-arm.a:
	@echo "======Building LabComm======"
	$(MAKE) -C $(LABCOMMLIBPATH) -e CC=$(CC_ARM) CFLAGS="$(filter-out -Werror -Wextra,$(CFLAGS_ARM)) -D MEM_WRITER_ENCODED_BUFFER=1" -e LABCOMM_NO_EXPERIMENTAL=true liblabcomm.a
	mv $(LABCOMMLIBPATH)/liblabcomm.a $(LABCOMMLIBPATH)/liblabcomm-arm.a
	@echo "======End building LabComm======"

$(LABCOMMLIBPATH)/liblabcomm.a:
	@echo "======Building LabComm======"
	$(MAKE) -C $(LABCOMMLIBPATH) -e CC=$(CC) CFLAGS="$(filter-out -Werror -Wextra,$(CFLAGS)) -D MEM_WRITER_ENCODED_BUFFER=1" -e LABCOMM_NO_EXPERIMENTAL=true liblabcomm.a
	@echo "======End building LabComm======"

# Generate labcomm files. 
# NOTE Can not use "$(@D)" since it causes realloc invalid next size.
$(GEN_DIR)/%.c $(GEN_DIR)/%.h: $(LC_DIR)/%.lc $(LABCOMMC) |$(GEN_DIR)
	java -jar $(LABCOMMC) --c=$(patsubst %.h,%.c,$@) --h=$(patsubst %.c,%.h,$@) $<

# Compile the generated LabComm files.
# Remove compiler warning flags. LabComm's errors is not our fault.
$(BUILD_DIR)/gen/%-arm.o: gen/%.c |$$(@D)
	$(CC_ARM) -c $(filter-out -W%,$(CFLAGS_ARM)) $(INC_FIREFLY) -o $@ $<

$(BUILD_DIR)/gen/%.o: gen/%.c |$$(@D)
	$(CC) -c $(filter-out -W%,$(CFLAGS)) $(INC_FIREFLY) -o $@ $<

### }

### Firefly targets {

# target: build/libfirefly.a  - Build static library for firefly.
$(BUILD_DIR)/lib$(LIB_FIREFLY_NAME).a: $(FIREFLY_OBJS)
	ar -rc $@ $^

$(BUILD_DIR)/lib$(LIB_FIREFLY_WERR_NAME).a: $(FIREFLY_OBJS) $(FIREFLY_ERR_OBJS)
	ar -rc $@ $^

$(BUILD_DIR)/lib$(LIB_FIREFLY_NAME)-arm.a: $(FIREFLY_ARM_OBJS)
	ar -rc $@ $^

# Compile protocol files.
# Usually these is an implicit rule but it does not work when the target dir is not the same as the source. Anyhow we need to add some includes for the different namespaces so it doesnt matter.
# "|<target>" means that <target> is order-only so if it changed it won't cause recompilcation of the target. This is needed here since the modify timestamp is changed when new files are added/removed from a dirctory which will cause constant unneccessary recomplications. This soluton requres GNU Make >= 3.80.
# References: http://darrendev.blogspot.se/2008/06/stopping-make-delete-intermediate-files.html

$(BUILD_DIR)/protocol/%-arm.o: protocol/%.c |$$(@D)
	$(CC_ARM) -c $(CFLAGS_ARM) $(INC_FIREFLY) -o $@ $<

$(BUILD_DIR)/protocol/%.o: protocol/%.c |$$(@D)
	$(CC) -c $(CFLAGS) $(INC_FIREFLY) -o $@ $<

# Compile utils files.
$(BUILD_DIR)/utils/%-arm.o: utils/%.c |$$(@D)
	$(CC_ARM) -c $(CFLAGS_ARM) $(INC_FIREFLY) -o $@ $<

$(BUILD_DIR)/utils/%.o: utils/%.c |$$(@D)
	$(CC) -c $(CFLAGS) $(INC_FIREFLY) -o $@ $<

### }

### Transport common targets {

# Compile source files
$(TRANSPORT_COMMON_ARM_OBJS): $(TRANSPORT_COMMON_SRC) |$$(@D)
	$(CC_ARM) -c $(CFLAGS_ARM) $(INC_TRANSPORT_COMMON) -o $@ $<

$(TRANSPORT_COMMON_OBJS): $(TRANSPORT_COMMON_SRC) |$$(@D)
	$(CC) -c $(CFLAGS) $(INC_TRANSPORT_COMMON) -o $@ $<

### }

### Transport UDP POSIX targets {

# target: build/lib$(LIB_TRANSPORT_UDP_POSIX_NAME).a  - Build static library for transport udp posix.
$(BUILD_DIR)/lib$(LIB_TRANSPORT_UDP_POSIX_NAME).a: $(TRANSPORT_UDP_POSIX_OBJS) $(TRANSPORT_POSIX_COMMON_OBJS) $(TRANSPORT_COMMON_OBJS)
	ar -rc $@ $^

# Compile UDP POSIX files.
$(TRANSPORT_UDP_POSIX_OBJS): $$(patsubst $$(BUILD_DIR)/%.o,%.c,$$@) |$$(@D)
	$(CC) -c $(CFLAGS) $(INC_TRANSPORT_UDP_POSIX) -o $@ $<

### }

### Transport ETH POSIX targets {

# target: build/lib$(LIB_TRANSPORT_ETH_POSIX_NAME).a  - Build static library for transport udp posix.
$(BUILD_DIR)/lib$(LIB_TRANSPORT_ETH_POSIX_NAME).a: $(TRANSPORT_ETH_POSIX_OBJS) $(TRANSPORT_POSIX_COMMON_OBJS) $(TRANSPORT_COMMON_OBJS)
	ar -rc $@ $^

# Compile UDP POSIX files.
$(TRANSPORT_ETH_POSIX_OBJS): $$(patsubst $$(BUILD_DIR)/%.o,%.c,$$@) |$$(@D)
	$(CC) -c $(CFLAGS) $(INC_TRANSPORT_ETH_POSIX) -o $@ $<

### }

### Transport UDP LWIP targets {

# target: build/lib$(LIB_TRANSPORT_UDP_LWIP_NAME).a  - Build static library for transport udp lwip.
$(BUILD_DIR)/lib$(LIB_TRANSPORT_UDP_LWIP_NAME).a: $(TRANSPORT_UDP_LWIP_OBJS) $(TRANSPORT_COMMON_ARM_OBJS)
	ar -rc $@ $^

# Compile UDP LWIP files.
$(TRANSPORT_UDP_LWIP_OBJS): $$(patsubst $$(BUILD_DIR)/%.o,%.c,$$@) |$$(@D)
	$(CC_ARM) -c $(CFLAGS_ARM) $(INC_TRANSPORT_UDP_LWIP) -o $@ $<

### }

### Transport ETH STELLARIS targets {

# target: build/lib$(LIB_TRANSPORT_ETH_STELLARIS_NAME).a  - Build static library for transport Ethernet Stellaris.
$(BUILD_DIR)/lib$(LIB_TRANSPORT_ETH_STELLARIS_NAME).a: $(TRANSPORT_ETH_STELLARIS_OBJS) $(TRANSPORT_COMMON_ARM_OBJS)
	ar -rc $@ $^

# Compile ETH STELLARIS files.
$(TRANSPORT_ETH_STELLARIS_OBJS): $$(patsubst $$(BUILD_DIR)/%.o,%.c,$$@) |$$(@D)
	$(CC_ARM) -c $(CFLAGS_ARM) $(INC_TRANSPORT_ETH_STELLARIS) -o $@ $<

### }

### Test programs. {
$(TEST_OBJS): $$(patsubst $$(BUILD_DIR)/%.o,%.c,$$@) |$$(@D)
	$(CC) -c $(CFLAGS) $(INC_TEST) -o $@ $<

# The test programs all depends on the libraries it uses and the test files output directory.
$(TEST_PROGS): $(LABCOMMLIBPATH)/liblabcomm.a |$(TESTFILES_DIR)

# Main test program for the protocol tests.
# Filter-out libraries since those are not "in-files" but the still depend on them so they can be used for linking.
$(BUILD_DIR)/test/test_protocol_main: $(patsubst %,$(BUILD_DIR)/test/%.o,test_protocol_main test_labcomm_utils test_proto_chan test_proto_conn test_proto_important test_proto_translc test_proto_protolc test_proto_errors error_helper proto_helper event_helper) $(BUILD_DIR)/$(GEN_DIR)/test.o $(BUILD_DIR)/lib$(LIB_FIREFLY_NAME).a
	$(CC) $(LDFLAGS) $(LDFLAGS_TEST) -L/tmp/ $(filter-out %.a,$^) -l$(LIB_FIREFLY_NAME) $(LDLIBS_TEST) -o $@

# Main test program for the transport tests.
$(BUILD_DIR)/test/test_transport_main: $(patsubst %,$(BUILD_DIR)/test/%.o,test_transport_main test_transport test_transport_gen test_transport_udp_posix error_helper) $(patsubst %,$(BUILD_DIR)/lib%.a,$(LIB_TRANSPORT_UDP_POSIX_NAME))
	$(CC) $(LDFLAGS) $(LDFLAGS_TEST) $^ -l$(LIB_FIREFLY_NAME) -l$(LIB_TRANSPORT_UDP_POSIX_NAME) $(LDLIBS_TEST) -o $@

# Main test program for the eth posix transport tests.
$(BUILD_DIR)/test/test_transport_eth_posix_main: $(patsubst %,$(BUILD_DIR)/test/%.o,test_transport test_transport_eth_posix_main test_transport_eth_posix error_helper event_helper) $(patsubst %,$(BUILD_DIR)/lib%.a,$(LIB_TRANSPORT_ETH_POSIX_NAME) $(LIB_FIREFLY_NAME))
	$(CC) $(LDFLAGS) $(LDFLAGS_TEST) $(filter-out %.a,$^) -l$(LIB_FIREFLY_NAME) -l$(LIB_TRANSPORT_ETH_POSIX_NAME) $(LDLIBS_TEST) -o $@

# Main test program for the event queue tests.
$(BUILD_DIR)/test/test_event_main: $(patsubst %,$(BUILD_DIR)/test/%.o,test_event_main error_helper) $(patsubst %,$(BUILD_DIR)/%.o,utils/firefly_event_queue)
	$(CC) $(LDFLAGS) $(LDFLAGS_TEST) $(filter-out %.a,$^) $(LDLIBS_TEST) -o $@

# Main test program for the pingpong udp program.
$(BUILD_DIR)/test/pingpong/pingpong_main: $(patsubst %,$(BUILD_DIR)/test/pingpong/%.o,pingpong_main pingpong_pudp pingpong pong_pudp ping_pudp) $(patsubst %,$(BUILD_DIR)/%.o,$(GEN_DIR)/pingpong utils/firefly_event_queue_posix) $(patsubst %,$(BUILD_DIR)/lib%.a,$(LIB_FIREFLY_WERR_NAME) $(LIB_TRANSPORT_UDP_POSIX_NAME))
	$(CC) $(LDFLAGS) $(LDFLAGS_TEST) $(filter-out %.a,$^) -l$(LIB_FIREFLY_WERR_NAME) -l$(LIB_TRANSPORT_UDP_POSIX_NAME) $(LDLIBS_TEST) -o $@ 

# Main test program for the pingpong ethernet program.
$(BUILD_DIR)/test/pingpong/ping_eth_main: $(patsubst %,$(BUILD_DIR)/test/pingpong/%.o,pingpong pingpong_peth ping_peth) $(patsubst %,$(BUILD_DIR)/%.o,$(GEN_DIR)/pingpong utils/firefly_event_queue_posix) $(patsubst %,$(BUILD_DIR)/lib%.a,$(LIB_FIREFLY_WERR_NAME) $(LIB_TRANSPORT_ETH_POSIX_NAME))
	$(CC) $(LDFLAGS) $(LDFLAGS_TEST) $(filter-out %.a,$^) -l$(LIB_FIREFLY_WERR_NAME) -l$(LIB_TRANSPORT_ETH_POSIX_NAME) $(LDLIBS_TEST) -o $@ 

$(BUILD_DIR)/test/pingpong/pong_eth_main: $(patsubst %,$(BUILD_DIR)/test/pingpong/%.o,pingpong pingpong_peth pong_peth) $(patsubst %,$(BUILD_DIR)/%.o,$(GEN_DIR)/pingpong utils/firefly_event_queue_posix) $(patsubst %,$(BUILD_DIR)/lib%.a,$(LIB_FIREFLY_WERR_NAME) $(LIB_TRANSPORT_ETH_POSIX_NAME))
	$(CC) $(LDFLAGS) $(LDFLAGS_TEST) $(filter-out %.a,$^) -l$(LIB_FIREFLY_WERR_NAME) -l$(LIB_TRANSPORT_ETH_POSIX_NAME) $(LDLIBS_TEST) -o $@ 

# Main test program for the multiple transport layer functionality.
$(BUILD_DIR)/test/pingpong/pingpong_multi_main: $(patsubst %,$(BUILD_DIR)/test/pingpong/%.o,pingpong_main pingpong pong_pmulti ping_pmulti) $(patsubst %,$(BUILD_DIR)/%.o,$(GEN_DIR)/pingpong utils/firefly_event_queue_posix) $(patsubst %,$(BUILD_DIR)/lib%.a,$(LIB_FIREFLY_WERR_NAME) $(LIB_TRANSPORT_UDP_POSIX_NAME) $(LIB_TRANSPORT_ETH_POSIX_NAME))
	$(CC) $(LDFLAGS) $(LDFLAGS_TEST) $(filter-out %.a,$^) -l$(LIB_FIREFLY_WERR_NAME) -l$(LIB_TRANSPORT_UDP_POSIX_NAME) -l$(LIB_TRANSPORT_ETH_POSIX_NAME) $(LDLIBS_TEST) -o $@ 

# Main test program for the resend posix queue tests.
$(BUILD_DIR)/test/test_resend_posix: $(patsubst %,$(BUILD_DIR)/test/%.o,test_resend_posix) $(patsubst %,$(BUILD_DIR)/%.o,utils/firefly_resend_posix)
	$(CC) $(LDFLAGS) $(LDFLAGS_TEST) $^ $(LDLIBS_TEST) -o $@

# Main test program for the memory management tests.
$(BUILD_DIR)/test/test_proto_memman: $(patsubst %,$(BUILD_DIR)/test/%.o,test_proto_memman proto_helper test_labcomm_utils error_helper event_helper) $(patsubst %,$(BUILD_DIR)/%.o,gen/test) $(BUILD_DIR)/lib$(LIB_FIREFLY_NAME).a $(LABCOMMLIBPATH)/liblabcomm.a
	$(CC) $(LDFLAGS) $(LDFLAGS_TEST) $(filter-out %.a,$^) -l$(LIB_FIREFLY_NAME) $(LDLIBS_TEST) -o $@

# UDP System tests
$(BUILD_DIR)/test/system/udp_posix: $(patsubst %,$(BUILD_DIR)/test/%.o,system/udp_posix test_labcomm_utils error_helper event_helper) $(patsubst %,$(BUILD_DIR)/%.o,utils/firefly_resend_posix gen/test) $(patsubst %,$(BUILD_DIR)/lib%.a,$(LIB_TRANSPORT_UDP_POSIX_NAME) $(LIB_FIREFLY_NAME)) $(LABCOMMLIBPATH)/liblabcomm.a
	$(CC) $(LDFLAGS) $(LDFLAGS_TEST) $(filter-out %.a,$^) -l$(LIB_FIREFLY_NAME) -l$(LIB_TRANSPORT_UDP_POSIX_NAME) $(LDLIBS_TEST) -o $@

### }

### Documentation {
# Modify the template configuration.
$(DOC_GEN_DIR)/doxygen_full.cfg: $(DOC_DIR)/doxygen_template.cfg |$$(@D)
	cp $^ $@
	echo "INPUT                  = doc/gen/index.dox include src" >> $@
	echo "OUTPUT_DIRECTORY       = \"$(DOC_GEN_FULL_DIR)\"" >> $@

# Modify the template configuration.
$(DOC_GEN_DIR)/doxygen_api.cfg: $(DOC_DIR)/doxygen_template.cfg |$$(@D)
	cp $^ $@
	echo "INPUT                  = doc/gen/index.dox include" >> $@
	echo "OUTPUT_DIRECTORY       = \"$(DOC_GEN_API_DIR)\"" >> $@

# Append our README to the index file.
$(DOC_GEN_DIR)/index.dox: $(DOC_DIR)/README.dox $(DOC_DIR)/index_template.dox |$$(@D) 
	cat $< >> $@

# target: doc - Generate both full and API documentation.
doc: doc-full doc-api

# target: doc - Generate full documentation.
doc-full:  $(DOC_GEN_DIR)/doxygen_full.cfg $(DOC_GEN_DIR)/index.dox |$(DOC_GEN_FULL_DIR)
	doxygen $(DOC_GEN_DIR)/doxygen_full.cfg

# target: doc-api - Generate API documentation.
doc-api: $(DOC_GEN_DIR)/doxygen_api.cfg $(DOC_GEN_DIR)/index.dox |$(DOC_GEN_API_DIR) 
	doxygen $(DOC_GEN_DIR)/doxygen_api.cfg

# target: doc-open - Open all documentation.
doc-open: $(DOC_GEN_FULL_DIR)/html/index.html $(DOC_GEN_API_DIR)/html/index.html
	xdg-open $(DOC_GEN_FULL_DIR)/html/index.html
	xdg-open $(DOC_GEN_API_DIR)/html/index.html

# target: doc-api-open - Open API documentation.
doc-api-open: $(DOC_GEN_API_DIR)/html/index.html
	xdg-open $(DOC_GEN_API_DIR)/html/index.html

# target: doc-full-open - Open full documentation.
doc-full-open: $(DOC_GEN_FULL_DIR)/html/index.html
	xdg-open $(DOC_GEN_FULL_DIR)/html/index.html
### }

## }

## Utility targets {

# target: test - Run all tests.
test: $(TEST_UNIT_PROGS)
	@for prog in $(filter-out $(TEST_UNIT_ROOT_PROGS),$^); do \
		echo "=========================>BEGIN TEST: $${prog}"; \
		valgrind --quiet --error-exitcode=1 --leak-check=full --show-reachable=yes ./$$prog; \
		test "$$?" -ne 0 && \
		echo "FAILURE: Program exited with failure status." >&2 && \
		break; \
		echo "=========================>END TEST: $${prog}"; \
	done
ifdef WITH_ROOT
	@for prog in $(TEST_UNIT_ROOT_PROGS); do \
		echo "=========================>BEGIN TEST: $${prog}"; \
		sudo valgrind --quiet --error-exitcode=1 --leak-check=full --show-reachable=yes ./$$prog; \
		test "$$?" -ne 0 && \
		echo "FAILURE: Program exited with failure status." >&2 && \
		break; \
		echo "=========================>END TEST: $${prog}"; \
	done
endif

test-system: $(TEST_SYSTEM_PROGS)
	@for prog in $(filter-out $(TEST_SYSTEM_ROOT_PROGS) $(TEST_SYSTEM_NORUN_PROGS),$^); do \
		echo "=========================>BEGIN TEST: $${prog}"; \
		valgrind --quiet --error-exitcode=1 --leak-check=full --show-reachable=yes ./$$prog; \
		test "$$?" -ne 0 && \
		echo "FAILURE: Program exited with failure status." >&2 && \
		break; \
		echo "=========================>END TEST: $${prog}"; \
	done
ifdef WITH_ROOT
	@for prog in $(TEST_SYSTEM_ROOT_PROGS); do \
		echo "=========================>BEGIN TEST: $${prog}"; \
		sudo valgrind --quiet --error-exitcode=1 --leak-check=full --show-reachable=yes ./$$prog; \
		test "$$?" -ne 0 && \
		echo "FAILURE: Program exited with failure status." >&2 && \
		break; \
		echo "=========================>END TEST: $${prog}"; \
	done
endif

# target: testc - Run all tests with sound effects!
testc:
	$(CLI_DIR)/celebrate.sh

# target: ping - Run ping test program. Must be started _after_ "make pong"
ping: $(BUILD_DIR)/test/pingpong/pingpong_main
	$< $@

# target: pong - Run pong test program. Must be started _before_ "make ping"
pong: $(BUILD_DIR)/test/pingpong/pingpong_main
	$< $@

# %.d - Automatic prerequisities generation.
$(BUILD_DIR)/%.d: %.c $(GEN_FILES) |$$(@D)
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
install: $(OUR_LIBS) $$(shell find $$(INCLUDE_DIR) -type f -name '*.h') |$(INSTALL_INCLUDE_DIR)
	install -C $(filter %.a,$^) $(INSTALL_LIB_DIR)
	cp -r $(wildcard $(INCLUDE_DIR)/*) $(INSTALL_INCLUDE_DIR)

# target: uninstall - Undo that install done.
uninstall:
	$(RM) $(addprefix $(INSTALL_LIB_DIR)/,$(patsubst $(BUILD_DIR)/%,%,$(OUR_LIBS)))
	$(RM) -r $(INSTALL_INCLUDE_DIR)

# target: help - Display all targets.
help:
	@egrep "#\starget:" [Mm]akefile | sed 's/\s-\s/\t\t\t/' | cut -d " " -f3- | sort -d

# target: clean  - Clean most compiled or generated files.
clean:
	$(RM) $(OUR_LIBS)
	$(RM) $(FIREFLY_OBJS)
	$(RM) $(FIREFLY_ARM_OBJS)
	$(RM) $(TRANSPORT_COMMON_OBJS)
	$(RM) $(TRANSPORT_UDP_POSIX_OBJS)
	$(RM) $(TRANSPORT_ETH_POSIX_OBJS)
	$(RM) $(TRANSPORT_UDP_LWIP_OBJS)
	$(RM) $(TRANSPORT_ETH_STELLARIS_OBJS)
	$(RM) $(TEST_OBJS)
	$(RM) $(TEST_PROGS)
	$(RM) $(DFILES)
	$(RM) $(GEN_OBJS)
	$(RM) $(wildcard $(GEN_DIR)/*)
	$(RM) $(wildcard $(TESTFILES_DIR)/*)
	$(RM) $(wildcard $(TESTFILES_DIR)/pingpong/*)
	$(RM) $(TAGSFILE_VIM) $(TAGSFILE_EMACS)

# target: cleaner - Clean all created files.
cleaner: clean
	$(RM) -r $(DOC_GEN_DIR)
	$(RM) -r $(BUILD_DIR)
	$(RM) -r $(LIB_DIR)
	$(RM) -r $(GEN_DIR)
	$(RM) -r $(TESTFILES_DIR)
	@echo "======Cleaning LabComm======"
	$(MAKE) -C $(LABCOMMLIBPATH) distclean
	@echo "======End cleaning LabComm======"
	@echo "======Cleaning LabComm compiler======"
	cd $(LABCOMMPATH)/compiler; ant clean 
	@echo "======End cleaning LabComm compiler======"

## }
### }
