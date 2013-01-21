DISTFILES = \
	art.txt art2.txt ascart.c autoclicker.c bool.h clipdraw.c \
	clipdraw.rc cplstub.c cplstub.def cplstub.h dateapply.c \
	dategather.c delempty.c DelEmptyDir.cpp dirtrans.c dlrename.c \
	eqnedit.c exp-gradient.c exparray.h fasthelp.c fasthelp.ico \
	fasthelp.rc fast_palscrn.c fmsimp.c frameinfo.txt hidewin.c \
	hidewin.h hidewin.rc lnbreak.c lnmerge.c lumchars.h Makefile \
	mpfproof.c null.h palscrn.c palscrn.ico palscrn.rc pi_tester.c \
	run-no-cons.c shrtname.c svg2mf.c svg2mf.txt sVOLExtract.cpp \
	targa.c targa.h temp-timer.c winmain.c

dist:
	mkdir util-0.1
	cp -p $(DISTFILES) util-0.1
	zip -9rq util-0.1.zip util-0.1
	rm -rf util-0.1
