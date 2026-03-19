CC      = gcc
CFLAGS  = -Wall -Wextra -g
LDFLAGS = -lpthread
TARGET  = httpd

all: $(TARGET)

$(TARGET): httpd.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
