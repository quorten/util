CROSS = i586-mingw32msvc-
CC = gcc
CXX = g++
CFLAGS = -mwindows
LDFLAGS = -mwindows
LINK = gcc
LINKXX = g++

OBJS = \
	targa.o clipdraw.o clipdraw-rc.o cplstub.o cplstub-def.o \
	fasthelp.o fasthelp-rc.o fast_palscrn.o hidewin.o \
	hidewin-rc.o palscrn.o palscrn-rc.o temp-timer.o \
	winspool-def.o

TARGETS = \
	autoclicker.exe clipdraw.exe cplstub.dll dateapply.exe \
	dategather.exe DelEmptyDir.exe fasthelp.exe hidewin.exe \
	palscrn.exe run-no-cons.exe sVOLExtract.exe temp-timer.exe \
	winspool.dll guessbits.exe orlyview.exe

all: $(TARGETS)

.c.o:
	$(CROSS)$(CC) $(CFLAGS) -c $< -o $@

%-rc.o: %.rc
	$(CROSS)windres -Ocoff $< -o $@

%-def.o: %.def
	$(CROSS)dlltool -d $< -e $@

%.exe: %.o
	$(CROSS)$(LINK) $(LDFLAGS) $^ -o $@

%.exe: %.c
	$(CROSS)$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

%.exe: %.cpp
	$(CROSS)$(CXX) $(CFLAGS) $(LDFLAGS) $< -o $@

%.dll: %.o
	$(CROSS)$(CC) -shared $^ -o $@

%.dll: %-def.o
	$(CROSS)$(CC) -shared $^ -o $@

.PHONY: -lwinmm -lpsapi -lwinspool

autoclicker.exe: autoclicker.c
clipdraw.o: clipdraw.c
clipdraw-rc.o: clipdraw.rc
targa.o: ../targa.c ../targa.h ../bool.h
	$(CROSS)$(CC) $(CFLAGS) -c $< -o $@
clipdraw.exe: clipdraw.o targa.o clipdraw-rc.o
# cplstub is missing the rc file
cplstub.o: cplstub.c cplstub.h
cplstub-def.o: cplstub.def
cplstub.dll: cplstub.o cplstub-def.o
dateapply.exe: dateapply.c
dategather.exe: dategather.c
DelEmptyDir.exe: DelEmptyDir.cpp
fasthelp.o: fasthelp.c
fasthelp-rc.o: fasthelp.rc fasthelp.ico
fasthelp.exe: fasthelp.o fasthelp-rc.o
fast_palscrn.o: fast_palscrn.c
hidewin.o: hidewin.c hidewin.h
hidewin-rc.o: hidewin.rc hidewin.h
hidewin.exe: hidewin.o hidewin-rc.o -lpsapi
palscrn.o: palscrn.c
palscrn-rc.o: palscrn.rc palscrn.ico
palscrn.exe: palscrn-rc.o palscrn-rc.o
#	$(CROSS)$(CC) -DBENCHMARK -o $@ $^ -mwindows -lwinmm
run-no-cons.exe: run-no-cons.c
sVOLExtract.exe: sVOLExtract.cpp
temp-timer.o: temp-timer.c
temp-timer.exe: temp-timer.o -lwinmm
winspool-def.o: winspool.def
winspool.dll: winspool-def.o -lwinspool
guessbits.exe: guessbits.c
orlyview.exe: orlyview.c

mostlyclean:
	rm -f $(OBJS)

clean: mostlyclean
	rm -f $(TARGETS)
