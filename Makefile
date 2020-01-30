DISTFILES = \
	art.txt art2.txt ascart.c bool.h delempty.c dirtrans.c \
	dlrename.c exparray.h exp-gradient.c fmsimp.c lnbreak.c \
	lnmerge.c lumchars.h Makefile mpfproof.c null.h pi_tester.c \
	shrtname.c svg2mf.c svg2mf.txt targa.c targa.h \
	derle.c mhkbreak.c gnread.c toext.c twavconv.c

DOS_DISTFILES = eqnedit.c

WIN_DISTFILES = \
	autoclicker.c clipdraw.c clipdraw.rc cplstub.c cplstub.def \
	cplstub.h dateapply.c dategather.c DelEmptyDir.cpp fasthelp.c \
	fasthelp.ico fasthelp.rc fast_palscrn.c frameinfo.txt \
	hidewin.c hidewin.h hidewin.rc palscrn.c palscrn.ico \
	palscrn.rc run-no-cons.c sVOLExtract.cpp temp-timer.c \
	winmain.c \
	winspool.c winspool.def guessbits.c orlyview.c \

X = .exe

# Files for asman that are not built: delempty.c dirtrans.c dlrename.c
# shrtname.c

# svg2mf$(X) commented out
all: pi_tester$(X) exp-gradient$(X) ascart$(X) lnbreak$(X) lnmerge$(X)	\
	mpfproof$(X) derle$(X) mhkbreak$(X) gnread$(X)	\
	toext$(X) twavconv$(X)

pi_tester$(X): pi_tester.c
	gcc -o $@ $<

targa.o: targa.c targa.h
	gcc -c -o $@ $<

exp-gradient$(X): exp-gradient.c targa.o
	gcc -o $@ $^

ascart$(X): ascart.c targa.o
	gcc -o $@ $^

lnbreak$(X): lnbreak.c
	gcc -o $@ $<

lnmerge$(X): lnmerge.c
	gcc -o $@ $<

mpfproof$(X): mpfproof.c
	gcc -o $@ $<

svg2mf$(X): svg2mf.c
	gcc -o $@ $< -lexpat

derle$(X): derle.c
	gcc -o $@ $^

mhkbreak$(X): mhkbreak.c
	gcc -o $@ $^

gnread$(X): gnread.c
	gcc -o $@ $<

toext$(X): toext.c
	gcc -o $@ $<

twavconv$(X): twavconv.c
	gcc -o $@ $<

clean:
	rm -f pi_tester$(X) targa.o exp-gradient$(X) ascart$(X)	\
	  lnbreak$(X) lnmerge$(X) mpfproof$(X) svg2mf$(X)
	rm -f derle$(X) mhkbreak$(X) gnread$(X) toext$(X)	\
	  twavconv$(X)

dist:
	mkdir util-0.1
	cp -p $(DISTFILES) util-0.1
	( cd dos && cp -p $(DOS_DISTFILES) util-0.1 )
	( cd windows && cp -p $(WIN_DISTFILES) util-0.1 )
	zip -9rq util-0.1.zip util-0.1
	rm -rf util-0.1
