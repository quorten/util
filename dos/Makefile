X = .exe

all: eqnedit$(X)

# NOTE: eqnedit should be compiled with DJGPP for a DJGPP texmf
# installation.
eqnedit$(X): eqnedit.c
	gcc -o $@ $<

clean:
	rm -f eqnedit$(X)
