#include <stdio.h>
#include <string.h>    
#include <sys/socket.h>
#include <arpa/inet.h> 
#include <unistd.h>    
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#define MAXBUFSIZE 1024
#define MAXCLIENTS 1000
#define LARGEBUFSIZE 4096
#define HUGEBUFSIZE 131072
#define CACHESIZE 131072

struct cache {
    char pageData[HUGEBUFSIZE];
    char url[MAXBUFSIZE];
    int pageSize; 
    time_t lastUsed;
    struct cache *next;
    // struct cache *prev;
};

struct HTTPHeader {
    char *method;
    char *URI;
    char *httpversion;
    char *connection;
    char *postdata;
};

struct HTTPResponse {
    char *path;
    char body[MAXBUFSIZE * MAXBUFSIZE];
    int status_code;
};

typedef struct cache cache;
cache* head = NULL;
int cache_expiration;
// typedef struct web_object * web_object;

//Via Ctrl+C
void signal_handler(int sig_num) {
    signal(SIGINT, signal_handler);
    //shutdown(listen_fd, SHUT_RD);
    printf("\nExiting httpserver. Bye. \n");
    fflush(stdout);
    exit(0);
}
void addPageToCache(char *data, char *url, int pageSize)
{
    //Allocate memory and assign values to struct object
    cache *page = (cache *)malloc(sizeof(page)*HUGEBUFSIZE);
    // page->pageData = (char *)malloc(HUGEBUFSIZE*sizeof(char));
    // page->url = (char *)malloc(MAXBUFSIZE*sizeof(char));
    // memset(&page->pageData, 0, HUGEBUFSIZE);
    // memset(&page->url, 0, MAXBUFSIZE);
    page->next = head;
    strcpy(page->pageData, data);
    strcpy(page->url,url);
    page->pageSize = pageSize;
    page->lastUsed = time(NULL);
    head = page;
}

cache* findCachePage(char * url)
{
    cache *page = NULL;
    if(head != NULL)
    {
        for(page = head; page != NULL; page = page->next)
        {
            if(strcmp(page->url, url) == 0)
            {
                return page;
            }
        }
    }
    else
    {
        // printf("Couldn't find page\n");
        return NULL;
    }

    return NULL;
}

//Adapted from http://cboard.cprogramming.com/c-programming/81901-parse-url.html
void parseHostName(char* requestURI, char* hostName)
{
    sscanf(requestURI, "http://%511[^/\n]", hostName);
}

//Read in the server's HTTP header message
int readHeader(int server_sock, char *headerBuf, int headerBufSize) {
    int i = 0;
    char c = '\0';
    int byteRec;
    //Read char by char until end of line
    while (i < (headerBufSize - 1) && (c != '\n')) 
    {
        byteRec = recv(server_sock, &c, 1, 0);
        if (byteRec > 0) 
        {
            if (c == '\r') 
            {
                //Look at the next line without reading it to evaluate
                byteRec = recv(server_sock, &c, 1, MSG_PEEK);
                //Read in the new line character if there is one
                if ((byteRec > 0) && (c == '\n')) 
                {
                    recv(server_sock, &c, 1, 0);
                }
                //Or set a new line character if there isn't
                else 
                {
                    c = '\n';
                }
            }
            headerBuf[i] = c;
            i++;
        } 
        else 
        {
            //Recv was zero or error. Set c to exit while loop
            c = '\n';
        }
    }
    headerBuf[i] = '\0';
    return i;
}

//Read in the content from server
int receiveFromServer(int server_sock, char *buf) {
    char msgBuffer[LARGEBUFSIZE];
    int contentLength = 0;
    unsigned int offset = 0;
    while (1) 
    {
        int length = readHeader(server_sock, msgBuffer, LARGEBUFSIZE);
        if( length <= 0)
        {
            return -1;
        }

        memcpy((buf + offset), msgBuffer, length);
        offset += length;
        if (strlen(msgBuffer) == 1) 
        {
            break;
        }
        if (strncmp(msgBuffer, "Content-Length", strlen("Content-Length")) == 0) 
        {
            char s1[256];
            sscanf(msgBuffer, "%*s %s", s1);
            contentLength = atoi(s1);
        }
    }
    //Read in the content from server
    char* contentBuffer = malloc((contentLength * sizeof(char)) + 3);
    int i;
    //Receive until length from header
    for (i = 0; i < contentLength; i++) 
    {
        char c;
        int byteRec = recv(server_sock, &c, 1, 0);
        if (byteRec <= 0) {
            // printf("Error in this thing\n");
            return -1;
        }
        contentBuffer[i] = c;
    }
    //Append carriage returns to end of content
    contentBuffer[i + 1] = '\r';
    contentBuffer[i + 2] = '\n';
    contentBuffer[i + 3] = '\0';
    memcpy((buf + offset), contentBuffer, (contentLength + 3));
    free(contentBuffer);
    return (offset + i + 4);
}

//Check for valid requests and output errors
int errorHandler(int client_sock, int statusCode, char* requestMethod, char* requestURI, char* requestVersion)
{
    char errorHeader[MAXBUFSIZE];
    char errorContent[MAXBUFSIZE];
    int length = 0;

    //Clear the buffers before use
    memset(&errorHeader, 0, MAXBUFSIZE);
    memset(&errorContent, 0, MAXBUFSIZE);

    //If we don't have a specific error to print (i.e. validation)
    if(statusCode == 0)
    {
        //501: not implemented
        if( (strcmp(requestMethod, "POST") == 0) || (strcmp(requestMethod, "HEAD") == 0) 
            || (strcmp(requestMethod, "PUT") ==0) || (strcmp(requestMethod, "DELETE") == 0)
            || (strcmp(requestMethod, "OPTIONS") == 0 ) || (strcmp(requestMethod, "CONNECT") == 0) )
        {
            statusCode = 501;
        }
        if ( !( (strcmp(requestVersion, "HTTP/1.1") == 0) || (strcmp(requestVersion, "HTTP/1.0") == 0) ) )
        {
            snprintf(errorContent, MAXBUFSIZE, "<html><body>400 Bad Request Reason: "
                "Invalid Version:%s</body></html>\r\n\r\n", requestVersion);

            //Create the header structure
            length += snprintf(errorHeader, MAXBUFSIZE, "HTTP/1.1 400 Bad Request\r\n");
            length += snprintf(errorHeader+length, MAXBUFSIZE-length, "Content-Type: text/html\r\n");
            length += snprintf(errorHeader+length, MAXBUFSIZE-length, "Content-Length: %lu\r\n\r\n", strlen(errorContent));

            //Send header to client
            send(client_sock, errorHeader, strlen(errorHeader), 0);
            //Write data to the client
            write(client_sock, errorContent, strlen(errorContent));

            return -1;
        }
        //400 Error: bad request method
        if( !( (strcmp(requestMethod, "GET") == 0) ) && statusCode != 501 )
        {
            snprintf(errorContent, MAXBUFSIZE, "<html><body>400 Bad Request Reason: "
                "Invalid Method:%s</body></html>\r\n\r\n", requestMethod);

            //Create the header structure
            length += snprintf(errorHeader, MAXBUFSIZE, "HTTP/1.1 400 Bad Request\r\n");
            length += snprintf(errorHeader+length, MAXBUFSIZE-length, "Content-Type: text/html\r\n");
            length += snprintf(errorHeader+length, MAXBUFSIZE-length, "Content-Length: %lu\r\n\r\n", strlen(errorContent));

            //Send header to client
            send(client_sock, errorHeader, strlen(errorHeader), 0);
            //Write data to the client
            write(client_sock, errorContent, strlen(errorContent));

            return -1;
        }
    }
    //If we want to output a specific status code
    switch(statusCode)
    {
        //File not found
        case 0:
            return 0;
            break;
        case 404:
            snprintf(errorContent, MAXBUFSIZE, "<html><body>404 Not Found "
            "Reason URL does not exist: %s</body></html>\r\n\r\n", requestURI);

            length += snprintf(errorHeader, MAXBUFSIZE, "HTTP/1.1 404 Not Found\r\n");
            length += snprintf(errorHeader+length, MAXBUFSIZE-length, "Content-Type: text/html\r\n");
            length += snprintf(errorHeader+length, MAXBUFSIZE-length, "Content-Length: %lu\r\n\r\n", strlen(errorContent));
            send(client_sock, errorHeader, strlen(errorHeader), 0);
            
            write(client_sock, errorContent, strlen(errorContent));
            return -1;
            break;
        //Catch-all for other errors
        case 500:
            length += snprintf(errorHeader, MAXBUFSIZE, "HTTP/1.1 500 Internal Server Error\r\n");
            length += snprintf(errorHeader+length, MAXBUFSIZE-length, "Content-Type: text/html\r\n");
            length += snprintf(errorHeader+length, MAXBUFSIZE-length, "Content-Length: %lu\r\n\r\n", strlen(errorContent));
            snprintf(errorContent, MAXBUFSIZE, "<html><body>500 Internal Server Error: "
                "Cannot allocate memory</body></html>\r\n\r\n");
            send(client_sock, errorHeader, strlen(errorHeader), 0);
            
            write(client_sock, errorContent, strlen(errorContent));
            return -1;
            break;
        //Duplicate, but in case we want to call it specifically
        case 501:
            snprintf(errorContent, MAXBUFSIZE, "<html><body>501 Not Implemented "
            "Method: %s</body></html>\r\n\r\n", requestMethod);

            length += snprintf(errorHeader, MAXBUFSIZE, "HTTP/1.1 501 Not Implemented\r\n");
            length += snprintf(errorHeader+length, MAXBUFSIZE-length, "Content-Type: text/html\r\n");
            length += snprintf(errorHeader+length, MAXBUFSIZE-length, "Content-Length: %lu\r\n\r\n", strlen(errorContent));
            send(client_sock, errorHeader, strlen(errorHeader), 0);
            
            write(client_sock, errorContent, strlen(errorContent));
            return -1;
            break;
    }

    return 0;
}

//Handles client requests
void processRequest(int client_sock, char* clientMessage, char* requestURI, char* hostName)
{
    char serverMessage[HUGEBUFSIZE];
    memset(&serverMessage, 0, HUGEBUFSIZE);

    int server_sock;
    struct sockaddr_in server;

    //Create socket
    server_sock = socket(AF_INET , SOCK_STREAM , 0);
    if (server_sock == -1)
    {
        printf("Could not create socket to server");
    }
    // puts("Server socket created");

    //Alow the socket to be used immediately
    //This resolves "Address already in use" error
    int option = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
     
    //Prepare the sockaddr_in structure
    struct hostent* hostt;
    hostt = gethostbyname(hostName);
    if(hostt == NULL)
    {
        printf("gethostbyname error: %s\n", strerror(h_errno));
    }

    server.sin_family = AF_INET;
    memcpy(&server.sin_addr, hostt->h_addr_list[0], hostt->h_length);
    server.sin_port = htons( 80 );
    socklen_t server_size = sizeof(server);

    int server_conn = connect(server_sock, (struct sockaddr *) &server, server_size);
    if (server_conn == -1) 
    {
        perror("Failed to connect to host");
        close(client_sock);
        return;
    }

    // Forward the HTTP request
    int mesLen = strlen(clientMessage);
    snprintf(clientMessage+mesLen, HUGEBUFSIZE, "\r\n\r\n");
    send(server_sock, clientMessage, strlen(clientMessage), 0);

    int bytesRec = receiveFromServer(server_sock, serverMessage);
    if(bytesRec == 0)
    {
        printf("Host disconnected: %d\n", server_sock);
        fflush(stdout);
    }
    else if(bytesRec == -1)
    {
        fprintf(stderr, "Recv error: \n");
    }

    addPageToCache(serverMessage, requestURI, bytesRec);

    //Send server contents back to client
    send(client_sock, serverMessage, bytesRec, 0);
    close(server_sock);
}

void get_request_headers(char *req, struct HTTPHeader *header) {
    char postDataBuffer[4000] = {0};
    char pipelineBuffer[4000] = {0};
    bzero(pipelineBuffer, sizeof(pipelineBuffer));
    bzero(postDataBuffer, sizeof(postDataBuffer));
    char *requestLine;

    strncpy(postDataBuffer, req,strlen(req));
    strncpy(pipelineBuffer, req,strlen(req));
    requestLine = strtok (req, "\n");
    header->method = malloc(sizeof(char) * (strlen(requestLine) + 1));
    header->URI = malloc(sizeof(char) * (strlen(requestLine) + 1));
    header->httpversion = malloc(sizeof(char) * (strlen(requestLine) + 1));
    sscanf(requestLine, "%s %s %s", header->method,  header->URI,  header->httpversion);
    /*
    char *buff;
    int count;
    char received_string[MAXBUFSIZE];
    buff = strtok (postDataBuffer, "\n");
    while(buff != NULL) {   
        if((strlen(buff) == 1)) {
            count = 1;
        }
        //printf("post buf %s\n", buff);
        buff = strtok (NULL, "\n");
        if(count == 1) {
            bzero(received_string, sizeof(received_string));
            sprintf(received_string, "%s", buff);
            count = 0;
        }       
    }
    //store postdata if any
    header->postdata = malloc(sizeof(char) * (strlen(received_string) + 1));
    strcpy(header->postdata, received_string);


    char *temp;
    char pp[MAXBUFSIZE];
    temp = strtok (pipelineBuffer, "\n");
    while(temp != NULL) {   
        if(strstr(temp, "Connection") != NULL) {
            bzero(pp, sizeof(pp));
            sprintf(pp,"%s", temp+12);
        }
        temp = strtok (NULL, "\n");         
    }
    //store connection config e.g keep-alive
    header->connection = malloc(sizeof(char) * (strlen(pp) + 1));
    strcpy(header->connection, pp);*/

}

int client_handler(int client_sock, int cache_expiration) {
    int read_size, errnum;
    cache *returnedPage;
    char request[MAXBUFSIZE];
    char client_message[LARGEBUFSIZE];
    char requestMethod[MAXBUFSIZE];
    char requestURI[MAXBUFSIZE];
    char requestVersion[MAXBUFSIZE];
    char hostName[MAXBUFSIZE];
    struct HTTPHeader request_headers;
    while((read_size = recv(client_sock , client_message , MAXBUFSIZE , 0)) > 0 ) {
        //get_request_headers((char *)&client_message, &request_headers);
        
    
        memset(&requestMethod, 0, MAXBUFSIZE);
        memset(&requestURI, 0, MAXBUFSIZE);
        memset(&requestVersion, 0, MAXBUFSIZE);

        sscanf(client_message, "%s %s %s", requestMethod, requestURI, requestVersion);
        printf("\n************************** REQUEST *********************************************\n\n");
        printf("Request Method: %s\n", requestMethod);
        printf("Request URL: %s\n", requestURI);
        printf("HTTP Version: %s\n", requestVersion);
        //printf("Connection: %s\n", request_headers.connection);
        printf("\n********************************************************************************\n\n");

        parseHostName(requestURI, hostName);

        //Pass in 0 to do method, version validation
        //errnum = errorHandler(client_sock, 0, requestMethod, requestURI, requestVersion);
        errnum = 0;
        if (errnum == 0) {
            //Process the client's request
            // removePageFromCache();
            returnedPage = findCachePage(requestURI);
            if(returnedPage == NULL)
            {
                printf("Not found in cache, requesting from server\n");
                processRequest(client_sock, client_message, requestURI, hostName);
            }
            else
            {
                printf("Found in cache\n");
                strcat(returnedPage->pageData, "\r\n\r\n");
                send(client_sock, returnedPage->pageData, returnedPage->pageSize, 0);
            }
        }

        memset(&client_message, 0, MAXBUFSIZE);
    }
             
    if(read_size == 0) {
        printf("Client disconnected: %d\n", client_sock);
        fflush(stdout);
    } else if(read_size == -1) {
        perror("recv failed");
    }

    //Close sockets and terminate child process
    close(client_sock);
    return 0;
}

int main(int argc , char *argv[])
{
    int sock;                           //This will be our socket
    struct sockaddr_in sin, remote;     //"Internet socket address structure"
    unsigned int remote_length = sizeof(remote);;         //length of the sockaddr_in structure
    int nbytes;                        //number of bytes we receive in our message
    char buffer[MAXBUFSIZE];             //a buffer to store our received message
    FILE *infp;                             // input file pointer
    FILE *outfp;                            // output file pointer
    FILE *md5fp;                            //md5 file pointer
    int client_fd;                      // client file descriptor for the accepted socket
    int pid;
    int port, read_size;
    
    
    char checksum[100], cmd[100];

    if (argc != 3) {
        printf("USAGE: [PORT NUMBER] [CACHE TIMEOUT]\n");
        exit(1);
    }
    port = atoi(argv[1]);
    cache_expiration = atoi(argv[2]);

    // print information of server
    printf("\n********************************************************************************\n\n");
    printf("Port Number: %d\n", port);
    printf("Cache Expiration: %d\n", cache_expiration);
    printf("\n********************************************************************************\n\n");

    /******************
      This code populates the sockaddr_in struct with
      the information about our socket
     ******************/
    bzero(&sin,sizeof(sin));                    //zero the struct
    sin.sin_family = AF_INET;                   //address family
    sin.sin_port = htons(atoi(argv[1]));        //htons() sets the port # to network byte order
    sin.sin_addr.s_addr = INADDR_ANY;           //supplies the IP address of the local machine

    //int timer = ws_conf.keep_alive;

    //Causes the system to create a generic socket of type TCP (stream)
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("unable to create socket");
    }


    /******************
     Once we've created a socket, we must bind that socket to the 
     local address and port we've supplied in the sockaddr_in struct
     ******************/
    if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        printf("unable to bind socket\n");
    }

    if ((listen(sock, MAXCLIENTS)) < 0) {
        perror("Listen");
        exit(1);
    }
  
    signal(SIGINT, signal_handler); // via Ctrl-C
    printf("\nListening and waiting for clients to accept\n");
    while (1) {
        int c = sizeof(struct sockaddr_in);
        // accept client
        client_fd = accept(sock, (struct sockaddr *)&remote, (socklen_t*)&c);
        if(client_fd < 0) {
            perror("Accept error");
            exit(1);
        }
        pid = fork();
        if (pid < 0) {
          perror("ERROR on fork");
          exit(1);
        }
        //Return to parent
        /*if (pid > 0) {
          close(client_fd);
          waitpid(0, NULL, WNOHANG); //indicates that the parent process shouldn’t wait
        }*/
        //The child process will handle individual clients, so we can  close the main socket
        if (pid == 0) {
          close(sock);
          exit(client_handler(client_fd, cache_expiration));
        }
        /* Close the connection */
        if(close(client_fd)<0)
        {
            printf("Error closing socket\n");
        }

    }
}