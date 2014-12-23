client.o: proxy.o
	gcc -Wall clienttest.c -o client
proxy.o: 
	gcc -Wall servertest.c -o proxy
clean:
	rm -rf client
	rm -rf proxy
	rm -rf a.out