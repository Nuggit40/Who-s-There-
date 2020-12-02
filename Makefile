all:
	gcc Asst3.c -o KKJserver
	gcc testclient.c -o TC
clean:
	rm KKJserver
	rm TC