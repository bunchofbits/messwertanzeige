CC=gcc
FLAGS= -Wall -g
LOADLIBES= -lXm -lXt -lX11 -lm

all: messwertanzeige sonde_sim sonde

install:
	install -d /usr/local/bin
	install messwertanzeige /usr/local/bin
	install sonde_sim /usr/local/bin
	install sonde/sonde /usr/local/bin
	install -d /usr/share/X11/icons/messwertanzeige/imgs
	install imgs/* /usr/share/X11/icons/messwertanzeige/imgs

messwertanzeige:  
sonde_sim:

.PHONY:sonde
sonde:
	$(MAKE) -C sonde sonde
clean:
	$(RM) *.o messwertanzeige sonde_sim test
	$(MAKE) -C sonde clean

