## Macros

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
DOC_DIR=doc
DOC_GEN_DIR=$(DOC_DIR)/gen
DOC_GEN_FULL_DIR=$(DOC_GEN_DIR)/full
DOC_GEN_API_DIR=$(DOC_GEN_DIR)/api
LC_DIR=lc
TESTFILES_DIR=testfiles

VPATH=$(SRC_DIR) $(INCLUDE_DIR)

LC_FILE_NAMES= firefly_protocol.lc test.lc
LC_FILES = $(addprefix $(LC_DIR)/, $(LC_FILE_NAMES))

# .c and .h files are always generated togheter. So lets only work with the .h
# files for simplicity
GEN_FILES= $(patsubst $(LC_DIR)/%.lc,$(GEN_DIR)/%.h,$(LC_FILES)) $(patsubst $(LC_DIR)/%.lc,$(GEN_DIR)/%.c,$(LC_FILES))
GEN_OBJ_FILES= $(patsubst %.h,$(BUILD_DIR)/%.o,$(filter-out %.c,$(GEN_FILES)))

FIREFLY_SRC= transport/firefly_transport_udp_posix.c protocol/firefly_protocol.c protocol/firefly_protocol_labcomm.c protocol/firefly_protocol_connection.c protocol/firefly_protocol_channel.c eventqueue/event_queue.c

# Disable default error handler that prints with fprintf. If set to true, you
# must provide an own implementation at link time.
ifneq ($(FIREFLY_ERROR_USER_DEFINED),true)
	FIREFLY_SRC += firefly_errors.c
endif


FIREFLY_OBJS= $(patsubst %.c,$(BUILD_DIR)/%.o,$(FIREFLY_SRC)) $(GEN_OBJ_FILES)

DEPENDS_GEN=$(SRC_DIR)/protocol/firefly_protocol.c $(SRC_DIR)/protocol/firefly_protocol_private.h \
	    $(SRC_DIR)/test/test_protocol.c $(SRC_DIR)/transport/firefly_transport_udp_posix.c \
	    $(SRC_DIR)/eventqueue/event_queue.h

LIBS=$(patsubst %,$(BUILD_DIR)/lib%.a,firefly)

TEST_PROTO_OBJS= $(patsubst %,$(BUILD_DIR)/test/%.o,test_labcomm_utils test_proto_chan test_proto_translc test_proto_protolc test_proto_errors proto_helper test_protocol_main)

TEST_TRANSP_OBJS= $(patsubst %,$(BUILD_DIR)/test/%.o,test_transport_main test_transport_udp_posix)

TEST_EVENT_OBJS= $(patsubst %,$(BUILD_DIR)/test/%.o,test_event_main)
TEST_EVENT_OBJS+= $(patsubst %,$(BUILD_DIR)/%.o,eventqueue/event_queue)

TEST_PROGS= $(addprefix $(BUILD_DIR)/test/test_,protocol_main transport_main event_main)

## Targets

# Non file targets.
.PHONY: all doc doc-full doc-apiclean clean cleaner install test
#.PHONY: all doc doc-open clean cleaner install test

# This will enable expression involving automatic variables in the prerequisities list. it must be defined before any usage of these feauteres.
.SECONDEXPANSION:

# target: all - Build all libs and tests.
all: $(BUILD_DIR) $(LIBS) $(TEST_PROGS) tags

$(BUILD_DIR) $(LIB_DIR) $(DOC_GEN_DIR) $(DOC_GEN_FULL_DIR) $(DOC_GEN_API_DIR) $(GEN_DIR) $(TESTFILES_DIR):
	mkdir -p $@

#$(BUILD_DIR)/libfirefly.a: $(FIREFLY_OBJS) $(GEN_DIR)/firefly_sample.o
$(BUILD_DIR)/libfirefly.a: $(FIREFLY_OBJS)
	ar -rc $@ $^

#$(BUILD_DIR)/%.o: %.c %.h
$(BUILD_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) -o $@ $<

$(DEPENDS_GEN): $(GEN_OBJ_FILES)

# target: install - Install libraries and headers to your system.
install: $(LIB_DIR) $(LIBS)
	install -C $(filter-out $(LIB_DIR), $^) $(LIB_DIR)

$(BUILD_DIR)/test/create_lc_files: $(patsubst %,$(BUILD_DIR)/%,test/test_labcomm_utils.o firefly_errors.o gen/firefly_protocol.o test/create_lc_files.o)
	$(CC) -L$(LABCOMMLIBPATH) $(filter %.o,$^) -lcunit -llabcomm -o $@

gen_lc_files: build/test/create_lc_files
	build/test/create_lc_files

$(BUILD_DIR)/test/%: test/%.c $(BUILD_DIR)/libfirefly.a
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -L$(BUILD_DIR) -L$(LABCOMMLIBPATH) $< -lcunit -lfirefly -llabcomm -o $@

# Test programs depends on liblabcomm.
$(BUILD_DIR)/test/pingpong/ping_pudp: $(BUILD_DIR) $(LIBS) $(BUILD_DIR)/test/pingpong/ping_pudp.o $(BUILD_DIR)/test/pingpong/pingpong_pudp.o $(BUILD_DIR)/test/pingpong/hack_lctypes.o $(BUILD_DIR)/gen/firefly_protocol.o $(LABCOMMLIBPATH)/liblabcomm.a
	$(CC) -L$(BUILD_DIR) -L$(LABCOMMLIBPATH) $(filter %.o,$^) -lpthread -lfirefly -llabcomm -o $@

$(BUILD_DIR)/test/pingpong/pong_pudp: $(BUILD_DIR) $(LIBS) $(BUILD_DIR)/test/pingpong/pong_pudp.o $(BUILD_DIR)/test/pingpong/pingpong_pudp.o $(BUILD_DIR)/test/pingpong/hack_lctypes.o $(BUILD_DIR)/gen/firefly_protocol.o $(LABCOMMLIBPATH)/liblabcomm.a
	$(CC) -L$(BUILD_DIR) -L$(LABCOMMLIBPATH) $(filter %.o,$^) -lpthread -lfirefly -llabcomm -o $@

$(BUILD_DIR)/test/test_protocol_main: $(TEST_PROTO_OBJS) $(BUILD_DIR) $(LIBS) $(LABCOMMLIBPATH)/liblabcomm.a $(BUILD_DIR)/gen/test.o $(BUILD_DIR)/gen/firefly_protocol.o
	$(CC) -L$(BUILD_DIR) -L$(LABCOMMLIBPATH) $(filter %.o,$^) -lcunit -lfirefly -llabcomm -o $@

$(BUILD_DIR)/test/test_transport_main: $(TEST_TRANSP_OBJS) $(BUILD_DIR)/transport/firefly_transport_udp_posix.o $(BUILD_DIR)/protocol/firefly_protocol_connection.o $(BUILD_DIR)/protocol/firefly_protocol_labcomm.o $(BUILD_DIR)/protocol/firefly_protocol_channel.o $(BUILD_DIR)/eventqueue/event_queue.o $(BUILD_DIR)/firefly_errors.o $(BUILD_DIR)/gen/firefly_protocol.o $(BUILD_DIR) $(LABCOMMLIBPATH)/liblabcomm.a
	$(CC) -L$(BUILD_DIR) -L$(LABCOMMLIBPATH) $(filter %.o,$^) -lcunit -lpthread -llabcomm -o $@

$(BUILD_DIR)/test/test_event_main: $(TEST_EVENT_OBJS) $(BUILD_DIR) $(LIBS) $(LABCOMMLIBPATH)/liblabcomm.a $(BUILD_DIR)/gen/test.o $(BUILD_DIR)/gen/firefly_protocol.o
	$(CC) -L$(LABCOMMLIBPATH) -L$(BUILD_DIR) $(filter %.o,$^) -lcunit -lfirefly -llabcomm -o $@

$(TEST_PROGS): $(TESTFILES_DIR)

# target: test - Run all tests.
test: $(TEST_PROGS) gen_lc_files
	@for prog in $(filter-out $(BUILD_DIR) $(LIBS) $(LABCOMMLIBPATH)/liblabcomm.a gen_lc_files,$^); do \
		echo "=========================>BEGIN TEST: $${prog}"; \
		./$$prog; \
		echo "=========================>END TEST: $${prog}"; \
	done

ping: $(BUILD_DIR)/test/pingpong/ping_pudp
	./$^

pong: $(BUILD_DIR)/test/pingpong/pong_pudp
	./$^

# target: testc - Run all tests with sound effects!
testc:
	./celebrate.sh

$(LABCOMMC):
	@echo "======Building LabComm compiler======"
	cd $(LABCOMMPATH)/compiler; ant jar
	@echo "======End building LabComm compiler======"

$(LABCOMMLIBPATH)/liblabcomm.a:
	@echo "======Building LabComm======"
	$(MAKE) -C $(LABCOMMLIBPATH) -e LABCOMM_NO_EXPERIMENTAL=true all
	@echo "======End building LabComm======"

# Let the labcomm .o-file depend on the generated .c and .h files.
$(GEN_OBJ_FILES): $$(patsubst $$(BUILD_DIR)/%.o,%.c,$$@) $$(patsubst $$(BUILD_DIR)/%.o,%.h,$$@)

#$(GEN_FILES): $(GEN_DIR) $$(patsubst $$(GEN_DIR)/%,c,$$(LC_DIR)/%.lc,$$@)
# TODO try to not have .c in $@
$(GEN_FILES): $(LABCOMMC) $(GEN_DIR) $$(patsubst $$(GEN_DIR)/%.h,$$(LC_DIR)/%.lc,$$@)
	java -jar $(LABCOMMC) --c=$(patsubst %.h,%.c,$@) --h=$(patsubst %.c,%.h,$@) $(filter-out $(LABCOMMC) $(GEN_DIR),$^)

#<<<<<<< HEAD
$(DOC_GEN_DIR)/doxygen_full.cfg: $(DOC_DIR)/doxygen_template.cfg
	cp $^ $@
	echo "INPUT                  = doc/gen/index.dox include src" >> $@
	echo "OUTPUT_DIRECTORY       = \"$(DOC_GEN_FULL_DIR)\"" >> $@

$(DOC_GEN_DIR)/doxygen_api.cfg: $(DOC_DIR)/doxygen_template.cfg
	cp $^ $@
	echo "INPUT                  = doc/gen/index.dox include" >> $@
	echo "OUTPUT_DIRECTORY       = \"$(DOC_GEN_API_DIR)\"" >> $@

$(DOC_GEN_DIR)/index.dox: $(DOC_GEN_DIR) $(DOC_DIR)/README.dox $(DOC_DIR)/index_template.dox
	#cp $(DOC_DIR)/index_template.dox $@
	#sed -e 's/^.*$$/ \* \0/g' $(DOC_DIR)/README.dox >> $@
	cat $(DOC_DIR)/README.dox >> $@
	#echo " */" >> $@

# target: doc - Generate both full and API documentation.
doc: doc-full doc-api

# target: doc - Generate full documentation.
doc-full: $(DOC_GEN_FULL_DIR) $(DOC_GEN_DIR)/doxygen_full.cfg $(DOC_GEN_DIR)/index.dox
	doxygen $(DOC_GEN_DIR)/doxygen_full.cfg

# target: doc-api - Generate API documentation.
doc-api: $(DOC_GEN_API_DIR) $(DOC_GEN_DIR)/doxygen_api.cfg $(DOC_GEN_DIR)/index.dox
	doxygen $(DOC_GEN_DIR)/doxygen_api.cfg

$(DOC_GEN_API_DIR)/html/index.html: doc-api

$(DOC_GEN_FULL_DIR)/html/index.html: doc-full

doc-open: $(DOC_GEN_FULL_DIR)/html/index.html $(DOC_GEN_API_DIR)/html/index.html
	xdg-open $(DOC_GEN_FULL_DIR)/html/index.html
	xdg-open $(DOC_GEN_API_DIR)/html/index.html

doc-api-open: $(DOC_GEN_API_DIR)/html/index.html
	xdg-open $(DOC_GEN_API_DIR)/html/index.html

doc-full-open: $(DOC_GEN_FULL_DIR)/html/index.html
	xdg-open $(DOC_GEN_FULL_DIR)/html/index.html

# target: tags - Generate tags with ctags for all files.
tags:
	ctags -R --tag-relative=yes -f $@

# target: help - Display all targets.
help:
	@egrep "#\starget:" [Mm]akefile  | sed 's/\s-\s/\t\t\t/' | cut -d " " -f3- | sort -d

# target: %.d - Make dependency files
$(BUILD_DIR)/%.d: %.c $(BUILD_DIR) $(GEN_FILES)
	mkdir -p $(dir $@)
	$(SHELL) -ec '$(CC) -M $(CFLAGS) $< \
	| sed '\''s/\(.*\)\.o[ :]*/$(patsubst %.d,%.o,$(subst /,\/,$@)) : /g'\'' > $@; \
	[ -s $@ ] || rm -f $@'

# Include dependency files and ignore ".d file missing" the first time.
#-include $(FIREFLY_OBJS:.o=.d) /dev/null

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
	$(RM) tags

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
