CC=gcc

lcs : ConnectManager.o mylog.o lcs.h lcs.c
	$(CC) ConnectManager.o mylog.o lcs.c -o lcs -g

ConnectManager.o : ConnectManager.c ConnectManager.h
	$(CC) -c ConnectManager.c -o ConnectManager.o

mylog.o : mylog.h mylog.c
	$(CC) -c mylog.c -o mylog.o
