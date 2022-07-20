# MakeFile function     :: MakeFile for compiling ALSA Audio test suite 

top_srcdir              ?= ../../..

include $(top_srcdir)/include/mk/env_pre.mk

INSTALL_DIR             := testcases/bin/starfive

FILTER_OUT_DIRS         :=

# We don't want to copy over the Makefile
UNWANTED_FILES          := Makefile

INSTALL_MODE            := 00755

#INSTALL_TARGETS         := $(filter-out $(UNWANTED_FILES),$(notdir $(patsubst $(abs_srcdir)/%,%,$(wildcard $(abs_srcdir)/*))))

CFLAGS          += -g -Wall -O1 -fomit-frame-pointer -fcommon -fPIC
LDFLAGS         += -g -fcommon
INCLUDES        = -I $(KERNEL_USR_INC) -I src/interface/common -I ../utils/user

#List of source files- Update this on adding a new C file
SOURCES := src/testcases/gpiod.c

MAKE_TARGETS            := gpiod_tests

gpiod_tests:
	$(CC) $(CFLAGS) $(LDFLAGS) -o gpiod_tests ${INCLUDES} ${SOURCES} -lgpiod -lpthread


include $(top_srcdir)/include/mk/generic_leaf_target.mk