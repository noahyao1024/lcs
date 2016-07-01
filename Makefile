CC=gcc

lcs : ConnectManager.o mylog.o lcs.h lcs.c
	$(CC) -g -lpthread -o lcs ConnectManager.o mylog.o lcs.c

ConnectManager.o : ConnectManager.c ConnectManager.h
	$(CC) -g -c -o ConnectManager.o ConnectManager.c

mylog.o : mylog.h mylog.c
	$(CC) -g -c -o mylog.o mylog.c

clean :
	rm -rf *.o lcs
