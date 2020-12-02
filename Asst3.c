#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

void *from_Client(){
    int sock;
    struct sockaddr_in server;
    int mysock;
    char buffer [1024];
    int rval;

    //Create Socket

    sock=socket(AF_INET, SOCK_STREAM,0); //makes socket
    if(sock<0){
        perror("FAILED to create socket");
        exit(1);
    }

             //initializes server info
    server.sin_family=AF_INET;
    server.sin_addr.s_addr=INADDR_ANY;
    server.sin_port = htons(3000);

 
    //Call bind

    if(bind(sock, (struct sockaddr *)&server, sizeof(server))){

        perror("BIND Failed");
        exit(1);
    }
    //Listen to the Network

    listen(sock, 5);



    //Accept

    do {

        mysock= accept(sock,(struct sockaddr*)0,0);
        if(mysock==-1){
            perror("DIDN'T Accept");
        }else{
            //initialize buffer
            memset(buffer,0,sizeof(buffer));
            if((rval=recv(mysock,buffer,sizeof(buffer),0))<0){
                perror("READING STREAM MESSAGE ERROR");
            }else if(rval==0){
                printf("ENDING CONNECTION\n");
            }else{
               printf("MSG: %s\n", buffer);
            }
             printf("GOT the message (rval= %d)\n",rval);
             close(mysock);
        }

    }while(1);
}

int main(int argc, char **argv)
{
	struct addrinfo hints, *address_list, *addr;
	int error;
	int sock;
	int i;
	
	if (argc < 4) {
		printf("Usage: %s [host] [port] [message(s)...]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	// we need to provide some additional information to getaddrinfo using hints
	// we don't know how big hints is, so we use memset to zero out all the fields
	memset(&hints, 0, sizeof(hints));
	
	// indicate that we want any kind of address
	// in practice, this means we are fine with IPv4 and IPv6 addresses
	hints.ai_family = AF_UNSPEC;
	
	// we want a socket with read/write streams, rather than datagrams
	hints.ai_socktype = SOCK_STREAM;

	// get a list of all possible ways to connect to the host
	// argv[1] - the remote host
	// argv[2] - the service (by name, or a number given as a decimal string)
	// hints   - our additional requirements
	// address_list - the list of results

	error = getaddrinfo(argv[1], argv[2], &hints, &address_list);
	if (error) {
		fprintf(stderr, "%s", gai_strerror(error));
		exit(EXIT_FAILURE);
	}

	
	// try each of the possible connection methods until we succeed
	for (addr = address_list; addr != NULL; addr = addr->ai_next) {
		// attempt to create the socket
		sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		
		// if we somehow failed, try the next method
		if (sock < 0) continue;
		
		// try to connect to the remote host using the socket
		if (connect(sock, addr->ai_addr, addr->ai_addrlen) == 0) {
			// we succeeded, so break out of the loop
			break;
		}

		// we weren't able to connect; close the socket and try the next method		
		close(sock);
	}
	
	// if we exited the loop without opening a socket and connecting, halt
	if (addr == NULL) {
		fprintf(stderr, "Could not connect to %s:%s\n", argv[1], argv[2]);
		exit(EXIT_FAILURE);
	}
	
	// now that we have connected, we don't need the addressinfo list, so free it
	freeaddrinfo(address_list);

	// send the remaining messages to the remote host
	for (i = 3; i < argc; ++i) {
		write(sock, argv[i], strlen(argv[i]));
		usleep(50 * 1000);
			// wait 50ms between writes, just to make things interesting
	}
	
	// close the socket
	close(sock);
    from_Client();

	return EXIT_SUCCESS;	
}
