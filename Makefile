DISTFILES = \
	art.txt art2.txt ascart.c autoclicker.c bool.h clipdraw.c \
	clipdraw.rc cplstub.c cplstub.def cplstub.h dateapply.c \
	dategather.c delempty.c DelEmptyDir.cpp dirtrans.c dlrename.c \
	eqnedit.c exp-gradient.c exparray.h fasthelp.c fasthelp.ico \
	fasthelp.rc fmsimp.c frameinfo.txt hidewin.c \
	hidewin.h hidewin.rc lnbreak.c lnmerge.c lumchars.h Makefile \
	mpfproof.c null.h palscrn.c palscrn.ico palscrn.rc pi_tester.c \
	run-no-cons.c shrtname.c svg2mf.c svg2mf.txt sVOLExtract.cpp \
	targa.c targa.h temp-timer.c winmain.c winspool.c winspool.def \
	guessbits.c derle.c mhkbreak.c orlyview.c gnread.c toext.c \
	twavconv.c

X = .exe

# Files for asman that are not built: dateapply.c dategather.c
# delempty.c DelEmptyDir.cpp dirtrans.c dlrename.c shrtname.c

# run-no-cons.c

# svg2mf$(X) commented out
# autoclicker$(X) commented out
# cplstub is missing the rc file
all: pi_tester$(X) exp-gradient$(X) ascart$(X) lnbreak$(X) lnmerge$(X)	\
	eqnedit$(X) mpfproof$(X) fasthelp$(X) hidewin$(X) clipdraw$(X)	\
	sVOLExtract$(X) temp-timer$(X) palscrn$(X) winspool.dll		\
	guessbits$(X) derle$(X) mhkbreak$(X) orlyview$(X) gnread$(X)	\
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

# NOTE: eqnedit should be compiled with DJGPP for a DJGPP texmf
# installation.
eqnedit$(X): eqnedit.c
	gcc -o $@ $<

mpfproof$(X): mpfproof.c
	gcc -o $@ $<

svg2mf$(X): svg2mf.c
	gcc -o $@ $< -lexpat

fasthelp-rc.o: fasthelp.rc fasthelp.ico
	windres -Ocoff -o $@ $<

fasthelp$(X): fasthelp.c fasthelp-rc.o
	gcc -o $@ $^

hidewin-rc.o: hidewin.rc hidewin.h
	windres -Ocoff -o $@ $<

hidewin$(X): hidewin.c hidewin-rc.o
	gcc -o $@ $^ -mwindows -lpsapi

clipdraw-rc.o: clipdraw.rc
	windres -Ocoff -o $@ $< 

clipdraw$(X): clipdraw.c targa.o clipdraw-rc.o
	gcc -o $@ $^ -mwindows

sVOLExtract$(X): sVOLExtract.cpp
	g++ -o $@ $<

temp-timer$(X): temp-timer.c
	gcc -o $@ $< -mwindows -lwinmm

autoclicker$(X): autoclicker.c
	gcc -o $@ $< -mwindows

palscrn-rc.o: palscrn.rc palscrn.ico
	windres -Ocoff -o $@ $<

palscrn$(X): palscrn.c palscrn-rc.o
#	gcc -DBENCHMARK -o $@ $^ -mwindows -lwinmm
	gcc -o $@ $^ -mwindows

winspool.o: winspool.def
	dlltool -d $< -e $@

winspool.dll: winspool.o
	gcc -shared $< -lwinspool -o $@

guessbits$(X): guessbits.c
	gcc -o $@ $^ -mwindows

derle$(X): derle.c
	gcc -o $@ $^

mhkbreak$(X): mhkbreak.c
	gcc -o $@ $^

orlyview$(X): orlyview.c
	gcc -o $@ $^ -mwindows

gnread$(X): gnread.c
	gcc -o $@ $<

toext$(X): toext.c
	gcc -o $@ $<

twavconv$(X): twavconv.c
	gcc -o $@ $<

clean:
	rm -f pi_tester$(X) targa.o exp-gradient$(X) ascart$(X)		\
	  lnbreak$(X) lnmerge$(X) eqnedit$(X) mpfproof$(X) svg2mf$(X)	\
	  fasthelp-rc.o fasthelp$(X) hidewin-rc.o hidewin$(X)		\
	  clipdraw-rc.o clipdraw$(X) sVOLExtract$(X) temp-timer$(X)	\
	  autoclicker$(X)
	rm -f palscrn-rc.o palscrn$(X) winspool.dll guessbits$(X)	\
	  derle$(X) mhkbreak$(X) orlyview$(X) gnread$(X) toext$(X)	\
	  twavconv$(X)

dist:
	mkdir util-0.1
	cp -p $(DISTFILES) util-0.1
	zip -9rq util-0.1.zip util-0.1
	rm -rf util-0.1
