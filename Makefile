################################################################################
# Makefile
# project3                                                          
#                                                                          #
#                                                                              #
################################################################################

CC = gcc
CFLAGS = -g -Wall -I/usr/include/libxml2 -lxml2 -lz -lm
SOURCE = src
VPATH = $(SOURCE)

define build-cmd
$(CC) $(CFLAGS) $< -o $@
endef 
default: proxy
proxy: 
	$(CC) $(CFLAGS) -o proxy proxy.c proxy.h

$(SOURCE)/%.o: %.c
	$(build-cmd)
clean:
	@rm proxy
