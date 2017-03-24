SRC = $(wildcard *.c)

CC = arm-none-linux-gnueabi-gcc

CFLAGS += -Wall
CFLAGS += -O2
CFLAGS += -g

CPPFLAGS += -Iinc
CPPFLAGS += -Iinc/alsa

LDFLAGS += -Llib
LDFLAGS += -ljpeg
LDFLAGS += -lts
LDFLAGS += -lpthread
LDFLAGS += -lasound

LDFLAGS += -Wl,-rpath=./lib
LDFLAGS += -Wl,-rpath=.

wechat:$(SRC)
	$(CC) $^ -o $@ $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)

clean:
	$(RM) wechat .*.sw? core
