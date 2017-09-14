#-
# ==========================================================================
# Copyright (c) 2011 Autodesk, Inc.
# All rights reserved.
# 
# These coded instructions, statements, and computer programs contain
# unpublished proprietary information written by Autodesk, Inc., and are
# protected by Federal copyright law. They may not be disclosed to third
# parties or copied or duplicated in any form, in whole or in part, without
# the prior written consent of Autodesk, Inc.
# ==========================================================================
#+

ifndef INCL_BUILDRULES

#
# Include platform specific build settings
#
TOP := ..
include $(TOP)/buildrules


#
# Always build the local plug-in when make is invoked from the
# directory.
#
all : plugins

endif

#
# Variable definitions
#

SRCDIR := $(TOP)/helloWorldCmd
DSTDIR := $(TOP)/helloWorldCmd

helloWorldCmd_SOURCES  := $(TOP)/helloWorldCmd/helloWorldCmd.cpp
helloWorldCmd_OBJECTS  := $(TOP)/helloWorldCmd/helloWorldCmd.o
helloWorldCmd_PLUGIN   := $(DSTDIR)/helloWorldCmd.$(EXT)
helloWorldCmd_MAKEFILE := $(DSTDIR)/Makefile

#
# Include the optional per-plugin Makefile.inc
#
#    The file can contain macro definitions such as:
#       {pluginName}_EXTRA_CFLAGS
#       {pluginName}_EXTRA_C++FLAGS
#       {pluginName}_EXTRA_INCLUDES
#       {pluginName}_EXTRA_LIBS
-include $(SRCDIR)/Makefile.inc


#
# Set target specific flags.
#

$(helloWorldCmd_OBJECTS): CFLAGS   := $(CFLAGS)   $(helloWorldCmd_EXTRA_CFLAGS)
$(helloWorldCmd_OBJECTS): C++FLAGS := $(C++FLAGS) $(helloWorldCmd_EXTRA_C++FLAGS)
$(helloWorldCmd_OBJECTS): INCLUDES := $(INCLUDES) $(helloWorldCmd_EXTRA_INCLUDES)

depend_helloWorldCmd:     INCLUDES := $(INCLUDES) $(helloWorldCmd_EXTRA_INCLUDES)

$(helloWorldCmd_PLUGIN):  LFLAGS   := $(LFLAGS) $(helloWorldCmd_EXTRA_LFLAGS) 
$(helloWorldCmd_PLUGIN):  LIBS     := $(LIBS)   -lOpenMaya -lFoundation $(helloWorldCmd_EXTRA_LIBS) 

#
# Rules definitions
#

.PHONY: depend_helloWorldCmd clean_helloWorldCmd Clean_helloWorldCmd


$(helloWorldCmd_PLUGIN): $(helloWorldCmd_OBJECTS) 
	-rm -f $@
	$(LD) -o $@ $(LFLAGS) $^ $(LIBS)

depend_helloWorldCmd :
	makedepend $(INCLUDES) $(MDFLAGS) -f$(DSTDIR)/Makefile $(helloWorldCmd_SOURCES)

clean_helloWorldCmd:
	-rm -f $(helloWorldCmd_OBJECTS)

Clean_helloWorldCmd:
	-rm -f $(helloWorldCmd_MAKEFILE).bak $(helloWorldCmd_OBJECTS) $(helloWorldCmd_PLUGIN)


plugins: $(helloWorldCmd_PLUGIN)
depend:	 depend_helloWorldCmd
clean:	 clean_helloWorldCmd
Clean:	 Clean_helloWorldCmd

