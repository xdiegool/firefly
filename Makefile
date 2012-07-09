## Macros

# Use LLVM clang if it's found.
CC = $(shell hash clang 2>/dev/null && echo clang || echo gcc)
CFLAGS= -g -O2 -std=c99 -Wall -Werror -I ./include -I $(LABCOMMLIBPATH) -I $(GEN_DIR)/ -I.

LIBPATH=../lib
LABCOMMPATH=$(LIBPATH)/labcomm
LABCOMMLIBPATH=$(LABCOMMPATH)/lib/c
LABCOMMC=$(LABCOMMPATH)/compiler/labComm.jar

BUILD_DIR=build
INCLUDE_DIR=include
LIB_DIR=lib
GEN_DIR=gen
SRC_DIR=src
VPATH=$(SRC_DIR) $(INCLUDE_DIR)

FIREFLY_SRC=firefly_protocol.c
FIREFLY_OBJS=$(patsubst %.c,$(BUILD_DIR)/%.o,$(FIREFLY_SRC))

SAMPLE_TEST_BINS= $(patsubst %,$(BUILD_DIR)/test/%,labcomm_test_decoder labcomm_test_encoder)

LIBS=$(patsubst %,$(BUILD_DIR)/lib%protocol.a,firefly)

#TEST_PROGS=test/test_ariel

## Targets

.PHONY: all clean install test

all: $(BUILD_DIR) $(LIBS)

$(BUILD_DIR) $(LIB_DIR):
	mkdir -p $@


$(BUILD_DIR)/libfirefly.a: $(FIREFLY_OBJS) $(GEN_DIR)/firefly_sample.o
	ar -rc $@ $^

$(BUILD_DIR)/%.o: %.c %.h
	$(CC) -c $(CFLAGS) -o $@ $<

install: $(LIB_DIR) $(LIBS)
	install -C $(filter-out $(LIB_DIR), $^) $(LIB_DIR)

$(BUILD_DIR)/test/%: test/%.c $(BUILD_DIR)/libfirefly.a
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -L$(BUILD_DIR) -L$(LABCOMMLIBPATH) $< -lcunit -lfirefly -llabcomm -o $@

# Run all tests.
test: $(BUILD_DIR) $(LIBS) $(LABCOMMLIBPATH)/liblabcomm.a $(patsubst %,$(BUILD_DIR)/%,$(TEST_PROGS)) 
	@for prog in $(filter-out $(BUILD_DIR) $(LIBS) $(LABCOMMLIBPATH)/liblabcomm.a,$^); do \
		./$$prog; \
	done


sample-test:  $(BUILD_DIR) $(LIBS) $(LABCOMMLIBPATH)/liblabcomm.a $(SAMPLE_TEST_BINS)
	@for prog in $(filter-out $(BUILD_DIR) $(LIBS) $(LABCOMMLIBPATH)/liblabcomm.a,$^); do \
		./$$prog; \
	done

$(LABCOMMC):
	cd $(LABCOMMPATH)/compiler; ant jar

$(LABCOMMLIBPATH)/liblabcomm.a:
	$(MAKE) -C $(LABCOMMLIBPATH) all

$(GEN_DIR)/firefly_sample.h $(GEN_DIR)/firefly_sample.c: firefly_sample.lc $(LABCOMMC)
	mkdir -p $(GEN_DIR)
	java -jar $(LABCOMMC) --c=$(GEN_DIR)/firefly_sample.c --h=$(GEN_DIR)/firefly_sample.h $<

$(GEN_DIR)/firefly_sample.o: $(GEN_DIR)/firefly_sample.c

#$(INCLUDE_DIR)/ariel_protocol.h: $(GEN_DIR)/firefly_sample.h

clean:
	$(RM) $(FIREFLY_OBJS)
	$(RM) -r $(GEN_DIR)/
	$(MAKE) -C $(LABCOMMLIBPATH) distclean

cleaner: clean
	$(RM) $(LIBS)
	$(RM) -r $(BUILD_DIR)
	$(RM) -r $(LIB_DIR)
	cd $(LABCOMMPATH)/compiler; ant clean
