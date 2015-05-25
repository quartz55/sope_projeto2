CFLAGS= -Wall -g -D_REENTRANT -lpthread -lrt -O3
OUTDIR= ./bin/

all: balcao cliente ger_cl

balcao: balcao.c
	gcc $(CFLAGS) -o $(OUTDIR)balcao balcao.c

cliente: cliente.c
	gcc $(CFLAGS) -o $(OUTDIR)cliente cliente.c

ger_cl: ger_cl.c
	gcc $(CFLAGS) -o $(OUTDIR)ger_cl ger_cl.c
