## Macros

# Use LLVM clang if it's found.
CC = $(shell hash clang 2>/dev/null && echo clang || echo gcc)
CFLAGS= -g -O2 -std=c99 -Wall -Werror -I$(INCLUDE_DIR) -I $(LABCOMMLIBPATH) -I $(GEN_DIR)/ -I$(SRC_DIR)

LIBPATH=../lib
LABCOMMPATH=$(LIBPATH)/labcomm
LABCOMMLIBPATH=$(LABCOMMPATH)/lib/c
LABCOMMC=$(LABCOMMPATH)/compiler/labComm.jar

BUILD_DIR=build
INCLUDE_DIR=include
LIB_DIR=lib
GEN_DIR=gen
SRC_DIR=src
CLI_DIR=cli
DOC_DIR=doc
DOC_GEN_DIR=doc/gen
VPATH=$(SRC_DIR) $(INCLUDE_DIR)

FIREFLY_SRC= transport/firefly_transport_udp_posix.c

# Disable default error handler that prints with fprintf. If set to true, you must provide an own implementation at link time.
ifneq ($(FIREFLY_ERROR_USER_DEFINED),true)
	FIREFLY_SRC += firefly_errors.c
endif


FIREFLY_OBJS= $(patsubst %.c,$(BUILD_DIR)/%.o,$(FIREFLY_SRC))

SAMPLE_TEST_BINS= $(patsubst %,$(BUILD_DIR)/test/%,labcomm_test_decoder labcomm_test_encoder)

LIBS=$(patsubst %,$(BUILD_DIR)/lib%.a,firefly)

TEST_PROGS=test/test_llp_udp_posix

## Targets

.PHONY: all doc clean cleaner install test

# target: all - Build most of the interesting targets.
all: $(BUILD_DIR) $(LIBS)

$(BUILD_DIR) $(LIB_DIR) $(DOC_GEN_DIR):
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
		./$$prog; \
	done

testa:
	$(CLI_DIR)/celebrate.sh


sample-test:  $(BUILD_DIR) $(LIBS) $(LABCOMMLIBPATH)/liblabcomm.a $(SAMPLE_TEST_BINS)
	@for prog in $(filter-out $(BUILD_DIR) $(LIBS) $(LABCOMMLIBPATH)/liblabcomm.a,$^); do \
		./$$prog; \
	done

$(LABCOMMC):
	cd $(LABCOMMPATH)/compiler; ant jar

$(LABCOMMLIBPATH)/liblabcomm.a:
	$(MAKE) -C $(LABCOMMLIBPATH) all #// TODO add no EXPERIMENTAl

#$(GEN_DIR)/firefly_sample.h $(GEN_DIR)/firefly_sample.c: firefly_sample.lc $(LABCOMMC)
	#mkdir -p $(GEN_DIR)
	#java -jar $(LABCOMMC) --c=$(GEN_DIR)/firefly_sample.c --h=$(GEN_DIR)/firefly_sample.h $<

#$(GEN_DIR)/firefly_sample.o: $(GEN_DIR)/firefly_sample.c

#$(INCLUDE_DIR)/ariel_protocol.h: $(GEN_DIR)/firefly_sample.h


$(DOC_GEN_DIR)/index.dox: $(DOC_GEN_DIR) $(DOC_DIR)/README $(DOC_DIR)/index_template.dox
	cp $(DOC_DIR)/index_template.dox $@
	sed -e 's/^.*$$/ \* \0/g' $(DOC_DIR)/README >> $@
	echo " */" >> $@

# target: doc - Generate documentation.
doc: $(DOC_GEN_DIR) $(DOC_GEN_DIR)/index.dox
	doxygen $(DOC_DIR)/doxygen.cfg


# target: help - Display all targets.
help :
	@egrep "#\starget:" [Mm]akefile  | sed 's/\s-\s/\t\t\t/' | cut -d " " -f3- | sort -d

# target: clean  - Clean most generated files..
clean:
	$(RM) $(FIREFLY_OBJS)
	$(RM) -r $(GEN_DIR)/
	$(MAKE) -C $(LABCOMMLIBPATH) distclean

# target: cleaner - Clean all generated files.
cleaner: clean
	$(RM) $(LIBS)
	$(RM) -r $(DOC_GEN_DIR)
	$(RM) -r $(BUILD_DIR)
	$(RM) -r $(LIB_DIR)
	cd $(LABCOMMPATH)/compiler; ant clean
