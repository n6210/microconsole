#
#(C) Copyright 2011 Taddy G. <fotonix@pm.me>
#

INSTALL_DIR = /usr/local/bin

CC		= gcc
#CC		= /x/tools/arm-2013.05/bin/arm-none-linux-gnueabi-gcc

CFLAGS	= -Wall 
#-Werror
SRCS	= ucon.c
TARGET	= uc


all:${TARGET}

${TARGET}:${SRCS}
	$(CC) $(CFLAGS) -pthread -o $(TARGET) $<

static:${SRCS}
	$(CC) $(CFLAGS) -pthread -o $(TARGET) -static $(SRCS)
	@cp -f $(TARGET) bin/$(TARGET).static

clean:
	@rm -f *.o  *~  *.static $(TARGET)

install: ${TARGET}
	@install -s -m 755 $(TARGET) $(INSTALL_DIR)

uninstall:
	@rm -f $(INSTALL_DIR)/$(TARGET)
