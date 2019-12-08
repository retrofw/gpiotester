#
# gpiotester for the RetroFW
#
# by pingflood; 2019
#

TARGET = gpiotester/gpiotester.dge

CHAINPREFIX=/opt/mipsel-RetroFW-linux-uclibc
CROSS_COMPILE=$(CHAINPREFIX)/usr/bin/mipsel-linux-

BUILDTIME=$(shell date +'\"%Y-%m-%d %H:%M\"')

CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
STRIP = $(CROSS_COMPILE)strip

SYSROOT     := $(shell $(CC) --print-sysroot)
SDL_CFLAGS  := $(shell $(SYSROOT)/usr/bin/sdl-config --cflags)
SDL_LIBS    := $(shell $(SYSROOT)/usr/bin/sdl-config --libs)

CFLAGS = -DTARGET_RETROFW -D__BUILDTIME__="$(BUILDTIME)" -DLOG_LEVEL=0 -g0 -Os $(SDL_CFLAGS) -I$(CHAINPREFIX)/usr/include/ -I$(SYSROOT)/usr/include/  -I$(SYSROOT)/usr/include/SDL/ -mhard-float -mips32 -mno-mips16 -Isrc
CFLAGS += -std=c++11 -fdata-sections -ffunction-sections -fno-exceptions -fno-math-errno -fno-threadsafe-statics

LDFLAGS = $(SDL_LIBS) -lfreetype -lSDL_image -lSDL_ttf -lSDL -lpthread
LDFLAGS +=-Wl,--as-needed -Wl,--gc-sections -s

all:
	$(CXX) $(CFLAGS) $(LDFLAGS) src/gpiotester.c -o $(TARGET)

linux:
	gcc src/gpiotester.c -g -o $(TARGET) -ggdb -O0 -DDEBUG -lSDL_image -lSDL -lSDL_ttf -I/usr/include/SDL -Isrc

ipk: all
	@rm -rf /tmp/.gpiotester-ipk/ && mkdir -p /tmp/.gpiotester-ipk/root/home/retrofw/apps/gpiotester /tmp/.gpiotester-ipk/root/home/retrofw/apps/gmenu2x/sections/applications
	@cp -r gpiotester.dge gpiotester.png backdrop.png /tmp/.gpiotester-ipk/root/home/retrofw/apps/gpiotester
	@cp gpiotester.lnk /tmp/.gpiotester-ipk/root/home/retrofw/apps/gmenu2x/sections/applications
	@sed "s/^Version:.*/Version: $$(date +%Y%m%d)/" control > /tmp/.gpiotester-ipk/control
	@tar --owner=0 --group=0 -czvf /tmp/.gpiotester-ipk/control.tar.gz -C /tmp/.gpiotester-ipk/ control
	@tar --owner=0 --group=0 -czvf /tmp/.gpiotester-ipk/data.tar.gz -C /tmp/.gpiotester-ipk/root/ .
	@echo 2.0 > /tmp/.gpiotester-ipk/debian-binary
	@ar r gpiotester.ipk /tmp/.gpiotester-ipk/control.tar.gz /tmp/.gpiotester-ipk/data.tar.gz /tmp/.gpiotester-ipk/debian-binary

opk: all
	@mksquashfs \
	gpiotester/default.retrofw.desktop \
	gpiotester/gpiotester.dge \
	gpiotester/gpiotester.png \
	gpiotester/gpiotester.opk \
	-all-root -noappend -no-exports -no-xattrs

clean:
	rm -rf gpiotester/gpiotester.dge gpiotester/gpiotester.ipk gpiotester/gpiotester.opk
