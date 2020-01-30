CC = cc
LINK = cc
CFLAGS =
LDFLAGS =
X =
O = o

VERSION = 0.1

DISTFILES = \
	art.txt art2.txt ascart.c bool.h delempty.c dirtrans.c \
	dlrename.c exparray.h exp-gradient.c fmsimp.c lnbreak.c \
	lnmerge.c lumchars.h Makefile mpfproof.c null.h pi_tester.c \
	shrtname.c svg2mf.c svg2mf.txt targa.c targa.h \
	derle.c mhkbreak.c gnread.c toext.c twavconv.c

DOS_DISTFILES = eqnedit.c shutscrn.c \
	EXT/README.txt EXT/logos.sys EXT/logow.sys

WIN_DISTFILES = \
	autoclicker.c clipdraw.c clipdraw.rc cplstub.c cplstub.def \
	cplstub.h dateapply.c dategather.c DelEmptyDir.cpp fasthelp.c \
	fasthelp.ico fasthelp.rc fast_palscrn.c frameinfo.txt \
	hidewin.c hidewin.h hidewin.rc palscrn.c palscrn.ico \
	palscrn.rc run-no-cons.c sVOLExtract.cpp temp-timer.c \
	winmain.c \
	winspool.c winspool.def guessbits.c orlyview.c \

ANY_OBJS = ascart.o targa.o exp-gradient.$(O) svg2mf.$(O)

ANY_TARGETS = \
	ascart$(X) delempty$(X) dirtrans$(X) dlrename$(X) \
	exp-gradient$(X) fmsimp$(X) lnbreak$(X) lnmerge$(X) \
	mpfproof$(X) pi_tester$(X) shrtname$(X) svg2mf$(X) \
	derle$(X) mhkbreak$(X) gnread$(X) toext$(X) twavconv$(X)

all: $(ANY_TARGETS)

.PHONY: windows
windows:
	$(MAKE) -C $@

%.$(O): %.c
	$(CC) $(CFLAGS) -c $< -o $@

%$(X): %.$(O)
	$(LINK) $(LDFLAGS) $^ -o $@

ascart.$(O): ascart.c lumchars.h targa.h
targa.$(O): targa.c targa.h
exp-gradient.$(O): exp-gradient.c bool.h exparray.h targa.h
svg2mf.$(O): svg2mf.c

ascart$(X): ascart.$(O) targa.$(O)
delempty$(X): delempty.c exparray.h
dirtrans$(X): dirtrans.c bool.h exparray.h
dlrename$(X): dlrename.c
exp-gradient$(X): exp-gradient.$(O) targa.$(O)
fmsimp$(X): fmsimp.c bool.h exparray.h
lnbreak$(X): lnbreak.c
lnmerge$(X): lnmerge.c
mpfproof$(X): mpfproof.c
pi_tester$(X): pi_tester.c
shrtname$(X): shrtname.c
svg2mf$(X): svg2mf.$(O) -lexpat -lm

derle$(X): derle.c
mhkbreak$(X): mhkbreak.c
gnread$(X): gnread.c
toext$(X): toext.c
twavconv$(X): twavconv.c

dist:
	mkdir util-$(VERSION)
	cp -p $(DISTFILES) util-$(VERSION)
	( cd dos && cp -p $(DOS_DISTFILES) util-$(VERSION) )
	( cd windows && cp -p $(WIN_DISTFILES) util-$(VERSION) )
	zip -9rq util-$(VERSION).zip util-$(VERSION)
	rm -rf util-$(VERSION)

mostlyclean:
	rm -f $(ANY_OBJS)
	$(MAKE) -C windows mostlyclean

clean: mostlyclean
	rm -f $(ANY_TARGETS)
	$(MAKE) -C windows clean
