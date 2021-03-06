#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#define BACKLOG 5

struct connection {
    struct sockaddr_storage addr;
    socklen_t addr_len;
    int fd;
};

int readMsgType(char *message);
void echo(struct connection* arg);
int server(char *port);
int currentFD;
struct connection* currentCon;

int main(int argc, char **argv)
{
	if (argc != 2) {
		printf("Usage: %s [port]\n", argv[0]);
		exit(EXIT_FAILURE);
	}
    if(argv[1] < 0 || atoi(argv[1]) > 65536){
        printf("Invalid Port Number\n");
        exit(EXIT_FAILURE);
    }
    (void) server(argv[1]);
    return EXIT_SUCCESS;
}
//accepts a new client connection
void acceptConnection(struct connection* con, int sfd){
    //printf("Waiting for connection\n");
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
        currentCon = con;
		echo(con);
    }
}

int server(char *port)
{
    struct addrinfo hint, *address_list, *addr;
    struct connection *con;
    int error, sfd;
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
    acceptConnection(con, sfd);
    // never reach here
    return 0;
}
//reads a message from current client connection
char* readMessage(int fd){
    int status = -1;
    int retsize = 256;
    char* buffer = malloc(sizeof(char) * retsize);
    int numread = 0;
    //reading REG or ERR
    for(numread = 0; numread < 4; ++numread){
        read(fd, &buffer[numread], 1);
    }
    if(buffer[0]=='R' && buffer[1]=='E' && buffer[2]=='G' && buffer[3]=='|'){
        do{
            read(fd, &buffer[numread], 1);
            numread++;
        } while (isdigit(buffer[numread-1]));
        //calculate size of content
        int numstart = 4;
        int numend = numread - 1;
        char sizestr[numend-numstart+1];
        memcpy(sizestr, &buffer[numstart], numend-numstart);
        sizestr[numend-numstart] = '\0';
        int contentSize = atoi(sizestr);
        //if digits do not end with pipe, return and becomes format error 
        if(buffer[numread-1] != '|'){
            printf("%c\n",buffer[numread]);
            return buffer;
        }
        //read until we encounter the final |
        do{
            read(fd, &buffer[numread], 1);
            ++numread;
        }while(buffer[numread-1] != '|');
    } else if(buffer[0]=='E' && buffer[1]=='R' && buffer[2]=='R' && buffer[3]=='|'){
        //read until you encounter the ending pipe
        do{
            read(fd, &buffer[numread], 1);
            numread++;
        }while(buffer[numread-1] != '|');
    } 
    buffer[numread] = '\0';
    return buffer;
}
//sends buf to client fd
void buffered_write(int fd, char *buf, int len) {
	int cur = 0;
	while (cur < len) {
		int nwrite = write(fd, buf + cur, len - cur);
		if (nwrite == -1) {
			fprintf(stderr, "%s", strerror(errno));
			exit(EXIT_FAILURE);
		}
		cur += nwrite;
	}
}
//checks if Message 1 is of valid format
//returns: 
//0 if valid message
//1 if content error
//2 if length error
//3 if format error
//4 if ERR message
int checkM1(char* m1){
    switch(readMsgType(m1)){
        case 0:
            //valid format, continue checking for length and content error
            break;
        case 1:
            //valid error message
            return 4;
        case 2:
            //format error
            return 3;
    }
    //format correct, checking content and length
    char contentLen[10];
    char content[256];
    int i = 4;
    while(isdigit(m1[i])){
        contentLen[i-4] = m1[i];
        ++i;
    }
    contentLen[i-4] = '\0';
    //printf("contentlen:%s\n", contentLen);
    ++i;//incrementing over '|'
    int j = 0;
    while(m1[i] != '|'){
        content[j++] = m1[i++];
    }
    content[j] = '\0';
    //printf("content:%s\n", content);
    if(strcmp("Who's there?", content) != 0){
        return 1;
    }
    //checking length
    if((int)strlen(content) != atoi(contentLen)){
        return 2;
    }
    //valid message
    return 0;
}
//checks if Message 3 is of valid format
//returns: 
//0 if valid message
//1 if content error
//2 if length error
//3 if format error
//4 if ERR message
int checkM3(char* m3, char* setup_line){
    //checking format
    switch(readMsgType(m3)){
        case 0:
            //valid format, continue checking for length and content error
            break;
        case 1:
            //valid error message
            return 4;
        case 2:
            //format error
            return 3;
    }
    //format correct, checking content and length
    char contentLen[10];
    char content[256];
    int i = 4;
    while(isdigit(m3[i])){
        contentLen[i-4] = m3[i];
        ++i;
    }
    contentLen[i-4] = '\0';
    //printf("contentlen:%s\n", contentLen);
    ++i;//incrementing over '|'
    int j = 0;
    while(m3[i] != '|'){
        content[j++] = m3[i++];
    }
    content[j] = '\0';
    //checking content
    char* setup = malloc(sizeof(char) * strlen(setup_line) + 7);
    strcpy(setup, setup_line);
    strcat(setup, ", who?");
    if(strcmp(setup, content) != 0){
        free(setup);
        return 1;
    }
    free(setup);
    //checking length
    if(strlen(content) != atoi(contentLen)){
        return 2;
    }
    //valid message
    return 0;
}
//checks if Message 3 is of valid format
//returns: 
//0 if valid message
//1 if content error
//2 if length error
//3 if format error
//4 if ERR message
int checkM5(char* m5){
    //checking format
    switch(readMsgType(m5)){
        case 0:
            //valid format, continue checking for length and content error
            break;
        case 1:
            //valid error message
            return 4;
        case 2:
            //format error
            return 3;
    }
    //format correct, checking content and length
    char contentLen[10];
    char content[256];
    int i = 4;
    while(isdigit(m5[i])){
        contentLen[i-4] = m5[i];
        ++i;
    }
    contentLen[i-4] = '\0';
    //printf("contentlen:%s\n", contentLen);
    ++i;//incrementing over '|'
    int j = 0;
    while(m5[i] != '|'){
        content[j++] = m5[i++];
    }
    content[j] = '\0';
    //checking length
    if(strlen(content) != atoi(contentLen)){
        return 2;
    }
    //checking content
    //content should be a string of letters ended by punctuation
    int k;
    for(k = 0; k < atoi(contentLen) - 1; ++k){
        if(!isalpha(content[k])) return 1;
    }
    if(!ispunct(content[k])) return 1;
    //valid message
    return 0;
}
//prints out the correct error message
void printErrorMsg(char* errormsg){
    //message 0
    if(strcmp(errormsg, "ERR|M0CT|") == 0){
        printf("M0CT - message 0 content was not correct (i.e. should be “Knock, knock.”)\n");
        return;
    }
    if(strcmp(errormsg, "ERR|M0LN|") == 0){
        printf("M0LN - message 0 length value was incorrect (i.e. should be 13 characters long)\n");
        return;
    }
    if(strcmp(errormsg, "ERR|M0FT|") == 0){
        printf("M0FT - message 0 format was broken (did not include a message type, missing or too many '|')\n");
        return;
    }
    //message 1
    if(strcmp(errormsg, "ERR|M1CT|") == 0){
        printf("M1CT - message 1 content was not correct (i.e. should be “Who's there?”)\n");
        return;
    }
    if(strcmp(errormsg, "ERR|M1LN|") == 0){
        printf("M1LN - message 1 length value was incorrect (i.e. should be 12 characters long)\n");
        return;
    }
    if(strcmp(errormsg, "ERR|M1FT|") == 0){
        printf("M1FT - message 1 format was broken (did not include a message type, missing or too many '|')\n");
        return;
    }
    //message 2
    if(strcmp(errormsg, "ERR|M2CT|") == 0){
        printf("M2CT - message 2 content was not correct (i.e. missing punctuation)\n");
        return;
    }
    if(strcmp(errormsg, "ERR|M2LN|") == 0){
        printf("M2LN - message 2 length value was incorrect (i.e. should be the length of the message)\n");
        return;
    }
    if(strcmp(errormsg, "ERR|M2FT|") == 0){
        printf("M2FT - message 2 format was broken (did not include a message type, missing or too many '|')\n");
        return;
    }
    //message 3
    if(strcmp(errormsg, "ERR|M3CT|") == 0){
        printf("M3CT - message 3 content was not correct (i.e. should contain message 2 with “, who?” tacked on)\n");
        return;
    }
    if(strcmp(errormsg, "ERR|M3LN|") == 0){
        printf("M3LN - message 3 length value was incorrect (i.e. should be M2 length plus six)\n");
        return;
    }
    if(strcmp(errormsg, "ERR|M3FT|") == 0){
        printf("M3FT - message 3 format was broken (did not include a message type, missing or too many '|')\n");
        return;
    }
    //message 4
    if(strcmp(errormsg, "ERR|M4CT|") == 0){
        printf("M4CT - message 4 content was not correct (i.e. missing punctuation)\n");
        return;
    }
    if(strcmp(errormsg, "ERR|M4LN|") == 0){
        printf("M4LN - message 4 length value was incorrect (i.e. should be the length of the message)\n");
        return;
    }
    if(strcmp(errormsg, "ERR|M4FT|") == 0){
        printf("M4FT - message 4 format was broken (did not include a message type, missing or too many '|')\n");
        return;
    }
    //message 5
    if(strcmp(errormsg, "ERR|M5CT|") == 0){
        printf("M5CT - message 5 content was not correct (i.e. missing punctuation)\n");
        return;
    }
    if(strcmp(errormsg, "ERR|M5LN|") == 0){
        printf("M5LN - message 5 length value was incorrect (i.e. should be the length of the message)\n");
        return;
    }
    if(strcmp(errormsg, "ERR|M5FT|") == 0){
        printf("M5FT - message 5 format was broken (did not include a message type, missing or too many '|')\n");
        return;
    }
}
//close current socket connection and wait for a new connection
void handleError(char* errormsg){
    printErrorMsg(errormsg);
    close(currentFD);
    free(currentCon);
}
//sends an error message to the current client connected and shuts down current session
void sendError(int errorcode, int turn, int fd){
    char* errmsg;
    switch(errorcode){
            case 1:
                //content error
                switch(turn){
                    case 1:
                        errmsg = "ERR|M1CT|";
                        break;
                    case 3:
                        errmsg = "ERR|M3CT|";
                        break;
                    case 5:
                        errmsg = "ERR|M5CT|";
                        break;
                }
                break;
            case 2:
                //length error
                switch(turn){
                    case 1:
                        errmsg = "ERR|M1LN|";
                        break;
                    case 3:
                        errmsg = "ERR|M3LN|";
                        break;
                    case 5:
                        errmsg = "ERR|M5LN|";
                        break;
                }
                break;
            case 3:
                //format error
                switch(turn){
                    case 1:
                        errmsg = "ERR|M1FT|";
                        break;
                    case 3:
                        errmsg = "ERR|M3FT|";
                        break;
                    case 5:
                        errmsg = "ERR|M5FT|";
                        break;
                }
    }
    buffered_write(fd, errmsg, strlen(errmsg));
    handleError(errmsg);
}

//exchange messages with the connected client
void echo(struct connection* arg)
{
    char host[100], port[10];
    struct connection *c = (struct connection *) arg;
    int error;
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
        return;
    }
    currentFD = c->fd;
	//EXCHANGING MESSAGES
    int err = 0;
    //send m0
	char* m0 = "REG|13|Knock, knock.|";
    buffered_write(c->fd, m0, strlen(m0));
    printf("sent:\t%s\n", m0);

    //read m1
    char* m1 = readMessage(c->fd);
    printf("read:\t%s\n", m1);
    //check m1 for errors
    err = checkM1(m1);
    if(err > 0) {
        if(err == 4){
            handleError(m1);
            return;
        }
        free(m1);
        sendError(err, 1, c->fd);
        return;
    }
    free(m1);

    //send m2
    char* m2 = "REG|4|Who.|";
    char* setup_line = "Who";
    buffered_write(c->fd, m2, strlen(m2));
    printf("sent:\t%s\n", m2);

    //read m3
    char* m3 = readMessage(c->fd);
    printf("read:\t%s\n", m3);
    //check m3 for errors
    err = checkM3(m3, setup_line);
    if(err > 0) {
        if(err == 4){
            handleError(m3);
            return;
        }
        free(m3);
        sendError(err, 3, c->fd);
        return;
    }
    free(m3);

    //send m4
    char* m4 = "REG|30|I didn't know you were an owl!|";
    write(c->fd, m4, strlen(m4));
    printf("sent:\t%s\n", m4);

    //read m5
    char* m5 = readMessage(c->fd);
    printf("read:\t%s\n", m5);
    //check m5 for errors
    err = checkM5(m5);
    if(err > 0) {
        if(err == 4){
            handleError(m5);
            return;
        }
        free(m5);
        sendError(err, 5, c->fd);
        return;
    }
    free(m5);
    close(c->fd);
    free(c);
}
//performs error checking for a given message
//returns:
//0 : valid REG
//1 : valid ERR
//2 : malformed message
int readMsgType(char *message){
       int pipe_count = 0;
       char buff[strlen(message)];
       char num_buff[256];
       int i=0;
       int j=0;
       int k=0;
       int len;
       int word_check=0;
       int num_check=0;
       int err_check=0;
    if(message[0]=='R'&& message[1]=='E' &&message[2]=='G'){
        i=3;
       while(i<strlen(message)){
           if(message[i]=='|'){
               pipe_count++;
           }
           if(pipe_count==1 && isdigit(message[i])){
               num_check=1;
           }else if(pipe_count==1 && !isdigit(message[i])){
               num_check=0;
           }
           if(pipe_count==2 ){
               if(message[i+1]!='|' && isalpha(message[i+1])){
                   word_check=1;
               }else if(message[i+1]!='|' && isalpha(message[i+1])){
                   word_check=0;
               }
           }
           if(pipe_count==3){
               break;
           }
           i++;
        }
        if(word_check ==1 && num_check ==1 && pipe_count==3){
            return 0;
        }
    }else if(message[0]=='E'&& message[1]=='R' && message[2]=='R'){
       
            if(message[3]=='|'){
                err_check+=1;
                if(message[4]=='M'){
                    err_check+=1;
                    if(isdigit(message[5])){
                        err_check+=1;
                        if((message[6]=='C' && message[7]=='T') ||(message[6]=='L' && message[7]=='N')||(message[6]=='F' && message[7]=='T')){
                            err_check+=1;
                        }else{
                            err_check=0;
                        }
                    }else{
                        err_check=0;
                    }
                }else{
                    err_check=0;
                }
            }else{
                err_check=0;
            }
        if(err_check==4){
            return 1;
        }
    }
    return 2;
}

    

