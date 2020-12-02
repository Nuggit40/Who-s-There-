
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>

#define BACKLOG 5

// the argument we will pass to the connection-handler threads
struct connection {
    struct sockaddr_storage addr;
    socklen_t addr_len;
    int fd;
};

int server(char *port);
void *echo(void *arg);

int main(int argc, char **argv)
{
	if (argc != 2) {
		printf("Usage: %s [port]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

    (void) server(argv[1]);
    return EXIT_SUCCESS;
}


int server(char *port)
{
    struct addrinfo hint, *address_list, *addr;
    struct connection *con;
    int error, sfd;
    pthread_t tid;

    // initialize hints
    memset(&hint, 0, sizeof(struct addrinfo));
    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_flags = AI_PASSIVE;
    	// setting AI_PASSIVE means that we want to create a listening socket

    // get socket and address info for listening port
    // - for a listening socket, give NULL as the host name (because the socket is on
    //   the local host)
    error = getaddrinfo(NULL, port, &hint, &address_list);
    if (error != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
        return -1;
    }

    // attempt to create socket
    for (addr = address_list; addr != NULL; addr = addr->ai_next) {
        sfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        
        // if we couldn't create the socket, try the next method
        if (sfd == -1) {
            continue;
        }

        // if we were able to create the socket, try to set it up for
        // incoming connections;
        // 
        // note that this requires two steps:
        // - bind associates the socket with the specified port on the local host
        // - listen sets up a queue for incoming connections and allows us to use accept
        if ((bind(sfd, addr->ai_addr, addr->ai_addrlen) == 0) &&
            (listen(sfd, BACKLOG) == 0)) {
            break;
        }

        // unable to set it up, so try the next method
        close(sfd);
    }

    if (addr == NULL) {
        // we reached the end of result without successfuly binding a socket
        fprintf(stderr, "Could not bind\n");
        return -1;
    }

    freeaddrinfo(address_list);

    // at this point sfd is bound and listening
    printf("Waiting for connection\n");
    for (;;) {
    	// create argument struct for child thread
		con = malloc(sizeof(struct connection));
        con->addr_len = sizeof(struct sockaddr_storage);
        	// addr_len is a read/write parameter to accept
        	// we set the initial value, saying how much space is available
        	// after the call to accept, this field will contain the actual address length
        
        // wait for an incoming connection
        con->fd = accept(sfd, (struct sockaddr *) &con->addr, &con->addr_len);
        	// we provide
        	// sfd - the listening socket
        	// &con->addr - a location to write the address of the remote host
        	// &con->addr_len - a location to write the length of the address
        	//
        	// accept will block until a remote host tries to connect
        	// it returns a new socket that can be used to communicate with the remote
        	// host, and writes the address of the remote hist into the provided location
        
        // if we got back -1, it means something went wrong
        if (con->fd == -1) {
            perror("accept");
            continue;
        }

		// spin off a worker thread to handle the remote connection
        error = pthread_create(&tid, NULL, echo, con);

		// if we couldn't spin off the thread, clean up and wait for another connection
        if (error != 0) {
            fprintf(stderr, "Unable to create thread: %d\n", error);
            close(con->fd);
            free(con);
            continue;
        }

		// otherwise, detach the thread and wait for the next connection request
        pthread_detach(tid);
       

    }

    // never reach here
    return 0;
}
int connectToServer(char *host, struct connection *c);
void *echo(void *arg)
{
    char host[100], port[10], buf[1001];
    struct connection *c = (struct connection *) arg;
    int error, mysock,rval;
    mysock=c->fd;

	// find out the name and port of the remote host
    error = getnameinfo((struct sockaddr *) &c->addr, c->addr_len, host, 100, port, 10, NI_NUMERICSERV);
    	// we provide:
    	// the address and its length
    	// a buffer to write the host name, and its length
    	// a buffer to write the port (as a string), and its length
    	// flags, in this case saying that we want the port as a number, not a service name
    if (error != 0) {
        fprintf(stderr, "getnameinfo: %s", gai_strerror(error));
        close(c->fd);
        return NULL;
    }

    printf("[%s:%s] connection\n", host, port);

   /* while ((nread = read(c->fd, buf, 100)) > 0) {
        buf[nread] = '\0';
        printf("[%s:%s] read %d bytes |%s|\n", host, port, nread, buf);
    }*/
     if(mysock==-1){
            perror("DIDN'T Accept");
        }else{
            //initialize buffer
            memset(buf,0,sizeof(buf));
            if((rval=recv(mysock,buf,sizeof(buf),0))<0){
                perror("READING STREAM MESSAGE ERROR");
            }else if(rval==0){
                printf("ENDING CONNECTION\n");
            }else{
               printf("MSG: %s\n", buf);
               //tries to connect to server
               mysock=connectToServer(host,c);
               if (mysock > 0){
                send(mysock,buf,sizeof(buf),0);
               }
               
            }
             printf("GOT the message \n");
             close(mysock);
        }

    printf("[%s:%s] got EOF\n", host, port);

    close(c->fd);
    free(c);
    return NULL;
}
int connectToServer(char *host, struct connection *c){
     //getConfigureDetails();
      //if (getConfigureDetails()!=-1){ 
        struct hostent *server;
        int sockfd;
        char* buffer = malloc(sizeof(char) *1);
        char server_reply[2000];
        ssize_t n;
  
        struct sockaddr_in servaddr; 

        sockfd = socket(AF_INET,SOCK_STREAM,0);
        if (sockfd == -1)
        {
            perror("Could not create socket");
        }
	    printf("Created Socket \n");
        bzero(&servaddr,sizeof (servaddr));
        servaddr.sin_family = AF_INET;
        int port1 = (int)(&c->addr);
        servaddr.sin_port = htons(port1);
        server = gethostbyname(host);
        bcopy((char *)server->h_addr_list[0], 
         (char *)&servaddr.sin_addr.s_addr,
         server->h_length);
        

        //servaddr.sin_addr.s_addr = inet_addr(info.IP);
        printf("%s\n",host);
        int cx  = connect(sockfd, (struct sockaddr *)&servaddr,sizeof(servaddr));
        while (cx==-1){
            printf("trying to reconnect\n");
            //setTimeout(3000);
            cx = connect(sockfd, (struct sockaddr *)&servaddr,sizeof(servaddr));
        }

        return sockfd;
      
         
   

    
    return 0;  
}
