CC ?= cc
CFLAGS ?= -O2 -g -Wall -Wextra -Werror -std=gnu11
SQLITE_INCLUDE ?= /usr/include
SQLITE_LIB ?= -lsqlite3
JSON_INCLUDE ?= /usr/include
JSON_LIB ?= -ljson-c

.PHONY: all clean

all: test_sms_db test_legacy_migrate test_tom_response
	./test_sms_db
	./test_legacy_migrate
	./test_tom_response

test_sms_db: test_sms_db.c ../src/sms_db.c
	$(CC) $(CFLAGS) -I../src -I$(SQLITE_INCLUDE) $^ $(SQLITE_LIB) -o $@

test_legacy_migrate: test_legacy_migrate.c ../src/sms_db.c ../src/legacy_migrate.c
	$(CC) $(CFLAGS) -I../src -I$(SQLITE_INCLUDE) -I$(JSON_INCLUDE) $^ $(SQLITE_LIB) $(JSON_LIB) -o $@

test_tom_response: test_tom_response.c ../src/tom_response.c
	$(CC) $(CFLAGS) -I../src -I$(JSON_INCLUDE) $^ $(JSON_LIB) -o $@

clean:
	rm -f test_sms_db test_legacy_migrate test_tom_response
