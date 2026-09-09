CC ?= cc
CFLAGS ?= -O2
CFLAGS += -fPIC -Wall -Wextra -Werror -std=gnu99 -I.
LDFLAGS ?=

SONAME = libqmodem-sms.so.1
TARGET = libqmodem-sms.so.1.0.0
OBJECTS = pdu.o ucs2_to_utf8.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -shared -Wl,-soname,$(SONAME) $(LDFLAGS) -o $@ $^

clean:
	rm -f $(OBJECTS) $(TARGET)
