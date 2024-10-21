TARGET = tom_modem

CFLAGS = -Wall  -Ipdu_lib

SRCS = main.c utils.c pdu_lib/pdu.c pdu_lib/ucs2_to_utf8.c

OBJS = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	rm -f $(OBJS) $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


.PHONY:clean
clean:
	rm -rf *.o *.*~ *~ *.swap $(all)
depend:
	$(CC) $(CFLAGS) -MM $(SRCS) > .depend

# 包含依赖文件
ifneq "$(wildcard .depend)" ""
include .depend
endif
