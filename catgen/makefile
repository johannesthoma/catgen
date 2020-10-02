#
# Copyright 2020, HP Inc.
# Author: Christian Limpach <Christian.Limpach@gmail.com>
# SPDX-License-Identifier: ISC
#

# named "makefile" because "Makefile" is in .gitignore

TARGET = catgen

all: $(TARGET)

CC = x86_64-w64-mingw32-gcc
#CC = i686-w64-mingw32-gcc

CFLAGS += -Wall -Werror -O2
libwdi.o: CFLAGS += -Wno-stringop-truncation # -Wno-error=stringop-truncation

CPPFLAGS += -I.. -DCONFIG_CATGEN

OBJS = $(TARGET).o pki.o logging.o libwdi.o

LDLIBS = -lsetupapi

VPATH = ../libwdi

$(TARGET): $(OBJS)

clean:
	@rm -f $(OBJS) $(TARGET) $(TARGET).exe

