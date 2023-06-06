
#CC = gcc
CFLAGS += -fPIC -Wall -O3 -g -I. -UHAVE_CONFIG_H
#CFLAGS += -fPIC -Wall -O0 -g3 -I. -DDEBUG -UHAVE_CONFIG_H

TARGET_SO = libappf.so
TARGET_A = libappf.a
TCLI_APP = tcli
DAEMONIZE_APP = daemonize

#SRC = appf_main.c appf_exec.c appf_log.c appf_poll.c appf_timer.c appf_server.c appf_client.c cJSON.c redblack.c
SRC = appf_main.c appf_exec.c appf_log.c appf_poll.c appf_timer.c appf_server.c appf_client.c
OBJ = $(SRC:.c=.o)
DEP = $(SRC:.c=.d)

#all: $(TARGET_SO) $(TARGET_A) $(TCLI_APP) $(DAEMONIZE_APP)
all: $(TARGET_SO) $(TARGET_A)

$(TARGET_SO): $(OBJ)
	$(CC) $(LDFLAGS) -lm -shared -o $@ $^

$(TARGET_A): $(OBJ)
	$(AR) -cvq $@ $^

$(DEP):%.d:%.c
	$(CC) $(CFLAGS) -MM $< >$@

#include $(DEP)

$(TCLI_APP): tcli.o
	$(CC) -o $@ $^ -L. -lappf -lm -rt

$(DAEMONIZE_APP): daemonize.o
	$(CC) -o $@ $^ -L. -lappf -lm -rt

install:
	mkdir -p $(DESTDIR)/usr/include
	mkdir -p $(DESTDIR)/usr/lib
	mkdir -p $(DESTDIR)/usr/bin
	cp -a appf.h $(DESTDIR)/usr/include
#	cp -a cJSON.h $(DESTDIR)/usr/include
#	cp -a redblack.h $(DESTDIR)/usr/include
#	cp -a sos_hlist.h $(DESTDIR)/usr/include
#	cp -a kernel-list.h $(DESTDIR)/usr/include
	cp -a libappf.* $(DESTDIR)/usr/lib
#	cp -a tcli $(DESTDIR)/usr/bin
#	cp -a daemonize $(DESTDIR)/usr/bin

uninstall:
	rm -fv $(DESTDIR)/usr/include/appf.h
	rm -fv $(DESTDIR)/usr/lib/libappf.*

clean:
	-$(RM) $(TARGET_LIB) $(OBJ) $(DEP) $(TARGET_A) $(TCLI_APP) $(DAEMONIZE_APP) $(TARGET_SO) tcli.o daemonize.o
