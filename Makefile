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
TLINK	= ucon


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
	@rm -f $(INSTALL_DIR)/$(TLINK)
	@ln -s $(INSTALL_DIR)/$(TARGET) $(INSTALL_DIR)/$(TLINK)

uninstall:
	@rm -f $(INSTALL_DIR)/$(TLINK) $(INSTALL_DIR)/$(TARGET)
