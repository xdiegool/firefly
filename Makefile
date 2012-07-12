## Macros
# TODO fix 80 col width.

# Use LLVM clang if it's found.
CC = $(shell hash clang 2>/dev/null && echo clang || echo gcc)
CFLAGS= -g -O2 -std=c99 -Wall -Werror -I$(INCLUDE_DIR) -I$(LABCOMMLIBPATH) -I. -I$(SRC_DIR)

LIBPATH=../lib
LABCOMMPATH=$(LIBPATH)/labcomm
LABCOMMLIBPATH=$(LABCOMMPATH)/lib/c
LABCOMMC=$(LABCOMMPATH)/compiler/labComm.jar

BUILD_DIR=build
INCLUDE_DIR=include
LIB_DIR=lib
GEN_DIR=gen
SRC_DIR=src
LC_DIR=lc
DOC_DIR=doc/gen
VPATH=$(SRC_DIR) $(INCLUDE_DIR)

LC_FILES=$(LC_DIR)/firefly_protocol.lc
# .c and .h files are always generated togheter. So lets only work with the .c
# files for simplicity
GEN_FILES= $(patsubst $(LC_DIR)/%.lc,$(GEN_DIR)/%.c,$(LC_FILES))
GEN_OBJ_FILES= $(patsubst %.c,$(BUILD_DIR)/%.o,$(GEN_FILES))

FIREFLY_SRC= transport/firefly_transport_udp_posix.c

# Disable default error handler that prints with fprintf. If set to true, you
# must provide an own implementation at link time.
ifneq ($(FIREFLY_ERROR_USER_DEFINED),true)
	FIREFLY_SRC += firefly_errors.c
endif


FIREFLY_OBJS= $(patsubst %.c,$(BUILD_DIR)/%.o,$(FIREFLY_SRC))

SAMPLE_TEST_BINS= $(patsubst %,$(BUILD_DIR)/test/%,labcomm_test_decoder labcomm_test_encoder)

LIBS=$(patsubst %,$(BUILD_DIR)/lib%.a,firefly)

TEST_PROGS= test/test_llp_udp_posix test/test_protocol

## Targets

.PHONY: all doc clean cleaner install test

# target: all - Build most of the interesting targets.
all: $(BUILD_DIR) $(LIBS)

$(BUILD_DIR) $(LIB_DIR) $(DOC_DIR) $(GEN_DIR):
	mkdir -p $@

#$(BUILD_DIR)/libfirefly.a: $(FIREFLY_OBJS) $(GEN_DIR)/firefly_sample.o
$(BUILD_DIR)/libfirefly.a: $(FIREFLY_OBJS)
	ar -rc $@ $^

#$(BUILD_DIR)/%.o: %.c %.h
$(BUILD_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) -o $@ $<

# target: install - Install libraries and headers to your system.
install: $(LIB_DIR) $(LIBS)
	install -C $(filter-out $(LIB_DIR), $^) $(LIB_DIR)

$(BUILD_DIR)/test/%: test/%.c $(BUILD_DIR)/libfirefly.a
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -L$(BUILD_DIR) -L$(LABCOMMLIBPATH) $< -lcunit -lfirefly -llabcomm -o $@

# target: test - Build and run all tests.
test: $(BUILD_DIR) $(LIBS) $(LABCOMMLIBPATH)/liblabcomm.a $(patsubst %,$(BUILD_DIR)/%,$(TEST_PROGS))
	@for prog in $(filter-out $(BUILD_DIR) $(LIBS) $(LABCOMMLIBPATH)/liblabcomm.a,$^); do \
		echo "=========================>BEGIN TEST: $${prog}"; \
		./$$prog; \
		echo "=========================>END TEST: $${prog}"; \
	done

testa:
	./celebrate.sh


sample-test:  $(BUILD_DIR) $(LIBS) $(LABCOMMLIBPATH)/liblabcomm.a $(SAMPLE_TEST_BINS)
	@for prog in $(filter-out $(BUILD_DIR) $(LIBS) $(LABCOMMLIBPATH)/liblabcomm.a,$^); do \
		./$$prog; \
	done

$(LABCOMMC):
	@echo "======Building LabComm compiler======"
	cd $(LABCOMMPATH)/compiler; ant jar
	@echo "======End building LabComm compiler======"

$(LABCOMMLIBPATH)/liblabcomm.a:
	@echo "======Building LabComm======"
	$(MAKE) -C $(LABCOMMLIBPATH) -e LABCOMM_NO_EXPERIMENTAL=true all
	@echo "======End building LabComm======"




build/test/test_protocol: $(BUILD_DIR)/gen/firefly_protocol.o

# Let the labcomm .o_file depend on the generated .c and .h files.
.SECONDEXPANSION $(GEN_OBJ_FILES): $$(patsubst $$(BUILD_DIR)/%.o,%.c,$$@) $$(patsubst $$(BUILD_DIR)/%.o,%.h,$$@)

#$(GEN_FILES): $(GEN_DIR) $$(patsubst $$(GEN_DIR)/%,c,$$(LC_DIR)/%.lc,$$@)
.SECONDEXPANSION $(GEN_FILES): $(GEN_DIR) $$(patsubst $$(GEN_DIR)/%.c,$(LC_DIR)/%.lc,$$@)
	java -jar $(LABCOMMC) --c=$@ --h=$(patsubst %.c,%.h,$@) $(filter-out $(GEN_DIR),$^)


#$(GEN_DIR)/firefly_sample.o: $(GEN_DIR)/firefly_sample.c

#$(INCLUDE_DIR)/ariel_protocol.h: $(GEN_DIR)/firefly_sample.h


# target: doc - Generate documentation.
doc:
	doxygen doxygen.cfg


# target: help - Display all targets.
help :
	@egrep "#\starget:" [Mm]akefile  | sed 's/\s-\s/\t\t\t/' | cut -d " " -f3- | sort -d

# target: clean  - Clean most generated files..
clean:
	$(RM) $(LIBS)
	$(RM) $(FIREFLY_OBJS)
	$(RM) $(addprefix $(BUILD_DIR)/,$(TEST_PROGS))
	$(RM) $(GEN_DIR)/*
	@echo "======Cleaning LabComm======"
	$(MAKE) -C $(LABCOMMLIBPATH) distclean
	@echo "======End cleaning LabComm======"

# target: cleaner - Clean all generated files.
cleaner: clean
	$(RM) -r $(DOC_DIR)
	$(RM) -r $(BUILD_DIR)
	$(RM) -r $(LIB_DIR)
	$(RM) -r $(GEN_DIR)
	@echo "======Cleaning LabComm compiler======"
	cd $(LABCOMMPATH)/compiler; ant clean
	@echo "======End cleaning LabComm compiler======"
