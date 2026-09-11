CC ?= cc
CFLAGS += -Wall -Wextra -Werror -std=gnu11
LDFLAGS += -lubus -lubox -lblobmsg_json -ljson-c -luci -lsqlite3 -lqmodem-sms
TARGET = qmodem-smsd
OBJECTS = main.o sms_db.o legacy_migrate.o tom_response.o

.PHONY: all clean
all: $(TARGET)
$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)
clean:
	rm -f $(OBJECTS) $(TARGET)
