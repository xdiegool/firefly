# Find the CUnit headers and libraries
#
# CUNIT_INCLUDE_DIRS - The CUnit include directory.
# CUNIT_LIBRARIES - The libraries needed to use CUnit.
# CUNIT_FOUND - True if CUnit found in system.
#

include(LibFindMacros)

find_path(CUNIT_INCLUDE_DIR NAMES CUnit/CUnit.h PATH_SUFFIXES cunit)
find_library(CUNIT_LIBRARY NAMES cunit libcunit cunitlib)
libfind_process(CUNIT)

