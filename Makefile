output:
	gcc -o client Client/client.c
	gcc -o server Server/server.c 

clean:
	rm *.out