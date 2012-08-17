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

ifeq ($(CC), gcc)
	CFLAGS += -z noexecstack
endif

ERRFLAGS = -Wall -Wextra
OPTLVL= 2
INC_COMMON= $(add-prefix -I, . $(INCLUDE_DIR) $(LABCOMMPATH) $(SRC_DIR) $(FREERTOS_LWIP_INCLUDES)) # TODO no freertos here
CFLAGS = -std=c99 $(ERRFLAGS) $(INC_COMMON)

## File and path macros.

# We want to have Makefile in / and have source and builds separated. Therefore Make can't find targets in CWD so we have to tell is where to look for targets.
VPATH = $(SRC_DIR) $(INCLUDE_DIR)

# Project dirctories.
BUILD_DI = uild
DOC_DI = oc
DOC_GEN_API_DI = (DOC_GEN_DIR)/api
DOC_GEN_DI = (DOC_DIR)/gen
DOC_GEN_FULL_DI = (DOC_GEN_DIR)/full
GEN_DI = en
INCLUDE_DI = nclude
LC_DI = c
LIB_DI = ib
SRC_DI = rc
TESTFILES_DI = estfiles

# All volatile directories that are to be created.
DIRS_TO_CREATE = $(BUILD_DIR) $(LIB_DIR) $(DOC_GEN_DIR) $(DOC_GEN_FULL_DIR) $(DOC_GEN_API_DIR) $(GEN_DIR) $(TESTFILES_DIR):

# LabComm
LIBPAT = ./lib
LABCOMMPAT = (LIBPATH)/labcomm
LABCOMMLIBPAT = (LABCOMMPATH)/lib/c
LABCOMM = (LABCOMMPATH)/compiler/labComm.jar


## Target macros and translations.

# LabComm sample files to used for code generation.
LC_FILE_NAMES = firefly_protocol.lc test.lc
LC_FILES = $(addprefix $(LC_DIR)/, $(LC_FILE_NAMES))
