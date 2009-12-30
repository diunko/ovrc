OBJS = hash.o server.o task.o \
	client.o callback.o ov_bindings.o util.o
CFLAGS = -shared -fPIC -g
PLUGIN_NAME = ovrc
CC = gcc

%.o: %.c ovrc.h mq.h
all: plugin
plugin: $(OBJS)
	$(CC) $(CFLAGS) -Wl,-soname,$(PLUGIN_NAME).so \
		-o $(PLUGIN_NAME).so $(OBJS) -lc -lpthread -lrt -lradiusclient-ng

clean:
	rm -f $(PLUGIN_NAME).so *.o

install: plugin
	cp $(PLUGIN_NAME).so /etc/openvpn/

