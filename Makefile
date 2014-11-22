################################################################################
# Makefile
# project3                                                          
#                                                                          #
#                                                                              #
################################################################################

CC = gcc
CFLAGS = -g -Wall -I/usr/include/libxml2 -lxml2 -lz -lm 
proxy: 
	$(CC) $(CFLAGS) -o proxy proxy.c proxy.h
clean:
	@rm proxy
