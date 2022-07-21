# sr - screenshot utility
# Copyright (C) 2022 ArcNyxx
# see LICENCE file for licensing information

VERSION = 0.9.1

PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

WPROFILE = -Wall -Wextra -Wstrict-prototypes -Wmissing-declarations \
-Wswitch-default -Wunreachable-code -Wcast-align -Wpointer-arith \
-Wbad-function-cast -Winline -Wundef -Wnested-externs -Wcast-qual -Wshadow \
-Wwrite-strings -Wno-unused-parameter -Wfloat-equal -Wpedantic
STD = -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L
LIB = -lX11 -lImlib2 -lbsd -lXcomposite -lXext -lXfixes

CFLAGS = $(WPROFILE) $(STD) -Os -g
LDFLAGS = $(LIB)
