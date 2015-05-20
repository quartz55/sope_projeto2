CFLAGS= -Wall -g -D_REENTRANT -lpthread -lrt -O3
OUTDIR= ./bin/

all: balcao cliente

balcao: balcao.c
	gcc $(CFLAGS) -o $(OUTDIR)balcao balcao.c

cliente: main.c
	gcc $(CFLAGS) -o $(OUTDIR)cliente cliente.c
