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
    char page_data[HUGEBUFSIZE];
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
int header_length = 0;
// typedef struct web_object * web_object;

//Via Ctrl+C
void signal_handler(int sig_num) {
    signal(SIGINT, signal_handler);
    //shutdown(listen_fd, SHUT_RD);
    printf("\nExiting httpserver. Bye. \n");
    fflush(stdout);
    exit(0);
}
void add_page_to_cache(char *data, char *url, int page_size)
{
    //Allocate memory and assign values to struct object
    /*cache *page = (cache *)malloc(sizeof(page)*HUGEBUFSIZE);
    page->next = head;
    strcpy(page->page_data, data);
    strcpy(page->url,url);
    page->pageSize = page_size;
    page->lastUsed = time(NULL);
    head = page;*/
    //printf("##########################################Add page cache--------");
    FILE *f = fopen("temp.txt", "w");
    fprintf(f, "%s", url);
    fclose(f);
    f = popen("md5 temp.txt", "r");
    char buf[MAXBUFSIZE];
    fgets(buf, MAXBUFSIZE, f);
    pclose(f);
    int i = 0;
    while(buf[i] != '=') {
        i++;
    }
    i += 2;
    buf[strlen(buf)-1] = '\0';
    strcat(buf, ".txt");
    f = fopen("temp.txt", "w");
    fprintf(f, "%ld\n", time(NULL));
    
    //printf("########header lnegth %d\n ", header_length);
    //printf("data is 1 %s\n", data);
    int count = 0;
    if (strlen(data) > 300) {
        while(1) {
            if(*data == '<')
                break;

            data = data+1;
        }
        fprintf(f, "%s\n", data);
    } else {
        fprintf(f, "%s\n", data);
    }
    
    //printf("data is 2 %s\n", data);
    //printf("size of data %lu", strlen(data));
    //printf("data without header is %s\n", data+page_size);
    fclose(f);
    rename("temp.txt", &buf[i]);
}

cache* find_cached_page(char * url)
{
    cache *page = NULL;
    if(head != NULL) {
        for(page = head; page != NULL; page = page->next) {
            if(strcmp(page->url, url) == 0) {
                return page;
            }
        }
    }
    else {
        // printf("Couldn't find page\n");
        return NULL;
    }

    return NULL;
}

//Adapted from http://cboard.cprogramming.com/c-programming/81901-parse-url.html
void parseHostName(char* requestURI, char* hostName) {
    sscanf(requestURI, "http://%511[^/\n]", hostName);
}

//Read in the server's HTTP header message
int read_header(int server_sock, char *headerBuf, int headerBufSize) {
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

void add_hostip_to_cache(char *url, char *ip) {
    FILE *f = fopen("host-ip-cache.txt", "r");
    char buf[MAXBUFSIZE];
    char cache_url[MAXBUFSIZE];
    char cache_ip[MAXBUFSIZE];
    while(!feof(f)) {
        fgets(buf, MAXBUFSIZE, f);
        buf[strlen(buf) - 1] = '\0';
        int i = 0;
        while(buf[i] != ' ') {
            cache_url[i] = buf[i];
            i++;
        }
        i += 3;
        strcpy(cache_ip, &buf[i]);
        if(strcmp(cache_url, url) == 0) {
            fclose(f);
            return;
        }
        else if(strcmp(cache_ip, ip) == 0) {
            fclose(f);
            return;
        }
    }
    fclose(f);
    f = fopen("host-ip-cache.txt", "a");
    snprintf(buf, MAXBUFSIZE, "%s : %s\n", url, ip);
    fprintf(f, "%s", buf);
    fclose(f);
}

int check_hostip_cache(char *url_ip) {
    FILE *f = fopen("host-ip-cache.txt", "r");
    char buf[MAXBUFSIZE];
    char cache_url[MAXBUFSIZE];
    char cache_ip[MAXBUFSIZE];
    while(!feof(f)) {
        fgets(buf, MAXBUFSIZE, f);
        buf[strlen(buf) - 1] = '\0';
        int i = 0;
        while(buf[i] != ' ') {
            cache_url[i] = buf[i];
            i++;
        }
        i += 3;
        strcpy(cache_ip, &buf[i]);
        if(strcmp(cache_url, url_ip) == 0) {
            fclose(f);
            return 1;
        }
        else if(strcmp(cache_ip, url_ip) == 0) {
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}
//Read in the content from server
int get_server_response(int server_sock, char *buf) {
    char msg_buffer[LARGEBUFSIZE];
    int content_length = 0;
    unsigned int offset = 0;
    while (1) {
        int length = read_header(server_sock, msg_buffer, LARGEBUFSIZE);

        header_length = length;
        //printf("\n########header length %d\n ", header_length);
        if( length <= 0) {
            return -1;
        }

        memcpy((buf + offset), msg_buffer, length);
        offset += length;
        if (strlen(msg_buffer) == 1) {
            break;
        }
        if (strncmp(msg_buffer, "Content-Length", strlen("Content-Length")) == 0) {
            char s1[256];
            sscanf(msg_buffer, "%*s %s", s1);
            content_length = atoi(s1);
        }
    }
    //Read in the content from server
    char* content_buffer = malloc((content_length * sizeof(char)) + 3);
    int i;
    //Receive until length from header
    for (i = 0; i < content_length; i++) {
        char c;
        int byteRec = recv(server_sock, &c, 1, 0);
        if (byteRec <= 0) {
            // printf("Error in this thing\n");
            return -1;
        }
        content_buffer[i] = c;
    }
    //Append carriage returns to end of content
    content_buffer[i + 1] = '\r';
    content_buffer[i + 2] = '\n';
    content_buffer[i + 3] = '\0';
    memcpy((buf + offset), content_buffer, (content_length + 3));
    free(content_buffer);
    //printf("server response ---2 %s\n", (buf + offset));
    //printf("server response ---3%s\n", msg_buffer);
    //printf("offset %d\n", offset);
    //printf("offset %d\n", i);
    //bzero(buf, HUGEBUFSIZE);
    //memcpy((buf), content_buffer + i + 4, sizeof(content_buffer));
    //printf("server response %s\n", content_buffer);
    return (offset + i + 4);
}


//Handles client requests
void handle_request(int client_sock, char* client_response, char* request_uri, char* host_name) {
    char server_response[HUGEBUFSIZE];
    memset(&server_response, 0, HUGEBUFSIZE);

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
    struct hostent* hostname;
    hostname = gethostbyname(host_name);
    if(hostname == NULL)
    {
        printf("gethostbyname error: %s\n", strerror(h_errno));
    }
    struct in_addr **addr_list;
    addr_list = (struct in_addr **)hostname->h_addr_list;

    server.sin_family = AF_INET;
    memcpy(&server.sin_addr, hostname->h_addr_list[0], hostname->h_length);
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
    int mesLen = strlen(client_response);
    snprintf(client_response + mesLen, HUGEBUFSIZE, "\r\n\r\n");
    send(server_sock, client_response, strlen(client_response), 0);

    int bytesRec = get_server_response(server_sock, server_response);
    if(bytesRec == 0) {
        printf("Host disconnected: %d\n", server_sock);
        fflush(stdout);
    }
    else if(bytesRec == -1) {
        //fprintf(stderr, "Recv error: \n");
    }
    //printf("server response is ---- 1%s\n", server_response);
    add_page_to_cache(server_response, request_uri, bytesRec);

    add_hostip_to_cache(host_name, inet_ntoa(*addr_list[0]));

    //Send server contents back to client
    send(client_sock, server_response, bytesRec, 0);
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
FILE *check_page_in_cache(char *url) {
    FILE *f = fopen("temp.txt", "w");
    fprintf(f, "%s", url);
    fclose(f);
    f = popen("md5 temp.txt", "r");
    char buf[MAXBUFSIZE];
    fgets(buf, MAXBUFSIZE, f);
    pclose(f);
    int i = 0;
    while(buf[i] != '=') {
        i++;
    }
    i += 2;
    buf[strlen(buf)-1] = '\0';
    strcat(buf, ".txt");
    f = fopen(&buf[i], "r");
    if(f == NULL) {
        printf("No page in cache...\n");
        fclose(f);
        return NULL;
    }
    else {
        char buffer[MAXBUFSIZE];
        fgets(buffer, MAXBUFSIZE, f);
        buffer[strlen(buffer)-1] = '\0';
        
        time_t before;
        sscanf(buffer, "%ld", &before);
        
        time_t now = time(NULL);
        if(difftime(now, before) >= cache_expiration) {
            printf("Cache expired with time: %lf seconds\n", difftime(now, before));
            //char dummy[MAXBUFSIZE];
            remove("temp.txt");
            remove(&buf[i]);
            fclose(f);
            //snprintf(dummy, MAXBUFSIZE, "cat %s", &buf[i]);
            //FILE *p = popen(dummy, "r");
            //fgets(buffer, MAXBUFSIZE, p);
            //pclose(p);
            
            return NULL;
        }
    }
    printf("Page found in cache...\n");
    return f;
    
}

void send_page_from_cache(FILE *f, int client_sock) {
    char buffer[MAXBUFSIZE];
    fgets(buffer, MAXBUFSIZE, f); // get timestamp
    char data[HUGEBUFSIZE];
    memset(data, '\0', HUGEBUFSIZE);
    fgets(data, HUGEBUFSIZE, f);
    while(!feof(f)) {
        fgets(buffer, MAXBUFSIZE, f);
        strcat(data, buffer);
    }
    //printf("%s\n", data);
    fclose(f);
    send(client_sock, data, sizeof(data), 0);
    close(client_sock);
}

int client_handler(int client_sock, int cache_expiration) {
    int read_size, errnum;
    cache *returned_page;
    char request[MAXBUFSIZE];
    char client_message[LARGEBUFSIZE];
    char request_method[MAXBUFSIZE];
    char request_uri[MAXBUFSIZE];
    char request_version[MAXBUFSIZE];
    char host_name[MAXBUFSIZE];
    struct HTTPHeader request_headers;
    while((read_size = recv(client_sock , client_message , MAXBUFSIZE , 0)) > 0 ) {
        //get_request_headers((char *)&client_message, &request_headers);
        
    
        memset(&request_method, 0, MAXBUFSIZE);
        memset(&request_uri, 0, MAXBUFSIZE);
        memset(&request_version, 0, MAXBUFSIZE);

        sscanf(client_message, "%s %s %s", request_method, request_uri, request_version);
        printf("\n************************** REQUEST *********************************************\n\n");
        printf("Request Method: %s\n", request_method);
        printf("Request URL: %s\n", request_uri);
        printf("HTTP Version: %s\n", request_version);
        //printf("Connection: %s\n", request_headers.connection);
        printf("\n********************************************************************************\n\n");

        sscanf(request_uri, "http://%511[^/\n]", host_name);

        //Pass in 0 to do method, version validation
        //errnum = errorHandler(client_sock, 0, requestMethod, requestURI, requestVersion);
        errnum = 0;
        if (errnum == 0) {
            //Process the client's request
            // removePageFromCache();
            returned_page = find_cached_page(request_uri);
            FILE *f = check_page_in_cache(request_uri);
            if(f == NULL)
            {
                printf("Not found in cache, requesting from server\n");
                handle_request(client_sock, client_message, request_uri, host_name);
            }
            else
            {
                /*printf("Found in cache\n");
                strcat(returned_page->page_data, "\r\n\r\n");
                send(client_sock, returned_page->page_data, returned_page->pageSize, 0);*/
                send_page_from_cache(f, client_sock);
            }
        }

        memset(&client_message, 0, MAXBUFSIZE);
    }
             
    if(read_size == 0) {
        //printf("Client disconnected: %d\n", client_sock);
        fflush(stdout);
    } else if(read_size == -1) {
        //perror("recv failed");
    }

    //Close sockets and terminate child process
    close(client_sock);
    return 0;
}

int main(int argc , char *argv[]) {
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
        if(close(client_fd)<0) {
            printf("Error closing socket\n");
        }

    }
}