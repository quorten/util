X = .exe

# Files for asman that are not built: dateapply.c dategather.c
# DelEmptyDir.cpp

# run-no-cons.c

# autoclicker$(X) commented out
# cplstub is missing the rc file
all: fasthelp$(X) hidewin$(X) clipdraw$(X) sVOLExtract$(X)	\
	temp-timer$(X) palscrn$(X) winspool.dll guessbits$(X)	\
	orlyview$(X)

autoclicker$(X): autoclicker.c
	gcc -o $@ $< -mwindows

clipdraw-rc.o: clipdraw.rc
	windres -Ocoff -o $@ $<

targa.o: ../targa.c ../targa.h
	gcc -c -o $@ $<

clipdraw$(X): clipdraw.c targa.o clipdraw-rc.o
	gcc -o $@ $^ -mwindows

fasthelp-rc.o: fasthelp.rc fasthelp.ico
	windres -Ocoff -o $@ $<

fasthelp$(X): fasthelp.c fasthelp-rc.o
	gcc -o $@ $^

hidewin-rc.o: hidewin.rc hidewin.h
	windres -Ocoff -o $@ $<

hidewin$(X): hidewin.c hidewin-rc.o
	gcc -o $@ $^ -mwindows -lpsapi

sVOLExtract$(X): sVOLExtract.cpp
	g++ -o $@ $<

temp-timer$(X): temp-timer.c
	gcc -o $@ $< -mwindows -lwinmm

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

orlyview$(X): orlyview.c
	gcc -o $@ $^ -mwindows

clean:
	rm -f fasthelp-rc.o fasthelp$(X) hidewin-rc.o hidewin$(X)	\
	  clipdraw-rc.o clipdraw$(X) sVOLExtract$(X) temp-timer$(X)	\
	  autoclicker$(X)
	rm -f palscrn-rc.o palscrn$(X) winspool.dll guessbits$(X)	\
	  orlyview$(X)
