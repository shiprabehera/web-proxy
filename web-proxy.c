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
#define MAXLINE 1024
#define MAX 100

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
void get_links_prefetch(){
    
    printf("\n---1 \n");
    FILE *f = fopen("temp.txt", "r");
    //bzero(buf,MAXLINE);
    char buf[MAXBUFSIZE];
    fgets(buf, MAXBUFSIZE, f);
    char * line = NULL;
    size_t len = 0;
    pclose(f);
    int i = 0;
    ssize_t read;
    while ((read = getline(&line, &len, f)) != -1) {
        printf("\n---2 \n");
        printf("Retrieved line of length %zu :\n", read);
        printf("%s", line);
        char *request_URL;
        sscanf(request_URL, "http://%[^/]", line);
        printf("Found link %s", request_URL);
    }
}
void add_page_to_cache(char *data, char *url, int page_size) {
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
    //get_links_prefetch();
    rename("temp.txt", &buf[i]);
}

//Read server's HTTP header message
int read_header(int server_sock, char *header_buffer, int header_buff_size) {
    int i = 0;
    char c = '\0';
    int bytes_receieved;
    //Read char by char until end of line
    while (i < (header_buff_size - 1) && (c != '\n')) {
        bytes_receieved = recv(server_sock, &c, 1, 0);
        if (bytes_receieved > 0) {
            if (c == '\r') {
                bytes_receieved = recv(server_sock, &c, 1, MSG_PEEK);
                if ((bytes_receieved > 0) && (c == '\n')) {
                    recv(server_sock, &c, 1, 0);
                } else {
                    c = '\n';
                }
            }
            header_buffer[i] = c;
            i++;
        } else {
            c = '\n';
        }
    }
    header_buffer[i] = '\0';
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

int check_hostip_cache(char *url_ip, char *host_ip) {
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
            strcpy(host_ip, cache_ip);
            return 1;
        } else if(strcmp(cache_ip, url_ip) == 0) {
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

//Check for valid requests and output errors
int check_error(int client_sock, char* request_method, char* request_URL, char* request_version) {
    char err_header[MAXBUFSIZE];
    char err_content[MAXBUFSIZE];
    int length = 0;
    
    //Clear the buffers before use
    memset(&err_header, 0, MAXBUFSIZE);
    memset(&err_content, 0, MAXBUFSIZE);
    
    struct hostent* hostt;
    char host_name[MAXBUFSIZE];
    sscanf(request_URL, "http://%[^/]", host_name);
    hostt = gethostbyname(host_name);
    //printf("hostname is %s\n", host_name);
    if(hostt == NULL) {
        
        printf("gethostbyname error: %s\n", strerror(h_errno));
        snprintf(err_content, MAXBUFSIZE, "<html><body>ERROR 404 Not Found: %s"
                 "</body></html>\r\n\r\n", request_URL);
        //Create the header structure
        length += snprintf(err_header, MAXBUFSIZE, "HTTP/1.1 ERROR 404 Not Found\r\n");
        length += snprintf(err_header + length, MAXBUFSIZE - length, "Content-Type: text/html\r\n");
        length += snprintf(err_header + length, MAXBUFSIZE - length, "Content-Length: %lu\r\n\r\n", strlen(err_content));
       
        //Send header to client
        send(client_sock, err_header, strlen(err_header), 0);
        //Write data to the client
        write(client_sock, err_content, strlen(err_content));
        
        return -1;
        
    }
    FILE *f = fopen("blocked.txt", "r");
    
    while(!feof(f)) {
        char blocked[MAXBUFSIZE];
        fgets(blocked, MAXBUFSIZE, f);
        blocked[strlen(blocked) - 1] = '\0';

        struct in_addr **addr_list;
        //printf("Official name is: %s\n", hostt->h_name);
        //printf("    IP addresses: ");
        addr_list = (struct in_addr **)hostt->h_addr_list;
        /*printf("############################################\n");
        printf("ip is %s \n", inet_ntoa(*addr_list[0]));
        printf("blocked in file %s \n", blocked);
        printf("hostname %s \n", host_name);
        printf("comparing---- %d\n", strcmp(host_name, blocked));
        printf("############################################\n");*/
        //printf();
        if(strcmp(host_name, blocked) == 0 || strcmp(inet_ntoa(*addr_list[0]), blocked) == 0) {
            printf("Blocked: %s\n", inet_ntoa(*addr_list[0]));
            snprintf(err_content, MAXBUFSIZE, "<html><body>ERROR 403 Forbidden: %s"
                     "</body></html>\r\n\r\n", request_URL);
            
            //Create the header structure
            length += snprintf(err_header, MAXBUFSIZE, "HTTP/1.1 ERROR 403 Forbidden\r\n");
            length += snprintf(err_header + length, MAXBUFSIZE - length, "Content-Type: text/html\r\n");
            length += snprintf(err_header + length, MAXBUFSIZE - length, "Content-Length: %lu\r\n\r\n", strlen(err_content));
            
            //Send header to client
            send(client_sock, err_header, strlen(err_header), 0);
            //Write data to the client
            write(client_sock, err_content, strlen(err_content));
            
            return -1;
        }
        
    }
    
    if((strcmp(request_method, "GET") != 0)) {
        snprintf(err_content, MAXBUFSIZE, "<html><body>400 Bad Request "
                 "Method: %s</body></html>\r\n\r\n", request_method);
        
        length += snprintf(err_header, MAXBUFSIZE, "HTTP/1.1 400 Bad Request\r\n");
        length += snprintf(err_header + length, MAXBUFSIZE - length, "Content-Type: text/html\r\n");
        length += snprintf(err_header + length, MAXBUFSIZE - length, "Content-Length: %lu\r\n\r\n", strlen(err_content));
        send(client_sock, err_header, strlen(err_header), 0);
        
        write(client_sock, err_content, strlen(err_content));
        return -1;
    }  

    return 0;
}

//Handles client requests
void handle_request(int client_sock, char* client_response, char* request_uri, char* host_name) {
    char server_response[HUGEBUFSIZE];
    memset(&server_response, 0, HUGEBUFSIZE);

    int server_sock;
    struct sockaddr_in server;

    //Create socket
    server_sock = socket(AF_INET , SOCK_STREAM , 0);
    if (server_sock == -1) {
        printf("Could not create socket to server");
    }
    // puts("Server socket created");

    //Alow the socket to be used immediately
    //This resolves "Address already in use" error
    int option = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
     server.sin_family = AF_INET;

    char host_ip[MAXBUFSIZE]; 
    int host_present = check_hostip_cache(host_name, host_ip);
    printf("host present flag is %d\n", host_present);
    
    if(host_present == 1) {
        //memcpy(&server.sin_addr, host_ip, strlen(host_ip));
        printf("Skipping DNS query.....\n");
        server.sin_addr.s_addr = inet_addr(host_ip);  
        printf("cached ip is %s\n", host_ip);

        //printf("cached ip length is %lu\n", strlen(host_ip));
    } else {
        printf("Doing DNS query......\n");
            
        //Prepare the sockaddr_in structure
        struct hostent* hostname;
        hostname = gethostbyname(host_name);
        if(hostname == NULL) {
            char err_header[MAXBUFSIZE];
            char err_content[MAXBUFSIZE];
            int length = 0;
            
            //Clear the buffers before use
            memset(&err_header, 0, MAXBUFSIZE);
            memset(&err_content, 0, MAXBUFSIZE);
            printf("gethostbyname error: %s\n", strerror(h_errno));
            printf("sent 404--1\n");
            snprintf(err_content, MAXBUFSIZE, "<html><body>ERROR 404 Not Found: %s"
                         "</body></html>\r\n\r\n", request_uri);
                
            //Create the header structure
            length += snprintf(err_header, MAXBUFSIZE, "HTTP/1.1 ERROR 404 Not Found\r\n");
            length += snprintf(err_header+length, MAXBUFSIZE-length, "Content-Type: text/html\r\n");
            length += snprintf(err_header+length, MAXBUFSIZE-length, "Content-Length: %lu\r\n\r\n", strlen(err_content));
            printf("sent 404--2\n");
            //Send header to client
            send(client_sock, err_header, strlen(err_header), 0);
            //Write data to the client
            write(client_sock, err_content, strlen(err_content));
            printf("sent 404\n");

        }
        struct in_addr **addr_list;
        addr_list = (struct in_addr **)hostname->h_addr_list;

        
        memcpy(&server.sin_addr, hostname->h_addr_list[0], hostname->h_length);
        
        add_hostip_to_cache(host_name, inet_ntoa(*addr_list[0]));
        
    }


    server.sin_port = htons( 80 );
    socklen_t server_size = sizeof(server);

    int server_conn = connect(server_sock, (struct sockaddr *) &server, server_size);
    if (server_conn == -1) {
        perror("Failed to connect to host");
        close(client_sock);
        return;
    }

    // Forward the HTTP request
    int mgslen = strlen(client_response);
    snprintf(client_response + mgslen, HUGEBUFSIZE, "\r\n\r\n");
    send(server_sock, client_response, strlen(client_response), 0);

    int bytes_received = get_server_response(server_sock, server_response);
    if(bytes_received == 0) {
        printf("Host disconnected: %d\n", server_sock);
        fflush(stdout);
    }
    else if(bytes_received == -1) {
        //fprintf(stderr, "Recv error: \n");
    }
    //printf("server response is ---- 1%s\n", server_response);
    add_page_to_cache(server_response, request_uri, bytes_received);

    

    //Send server contents back to client
    send(client_sock, server_response, bytes_received, 0);
    close(server_sock);
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
    } else {
        char buffer[MAXBUFSIZE];
        fgets(buffer, MAXBUFSIZE, f);
        buffer[strlen(buffer)-1] = '\0';
        
        time_t before;
        sscanf(buffer, "%ld", &before);
        
        time_t now = time(NULL);
        if(difftime(now, before) >= cache_expiration) {
            printf("Cache expired with time: %lf seconds\n", difftime(now, before));
            remove("temp.txt");
            remove(&buf[i]);
            fclose(f);
            
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
        errnum = check_error(client_sock, request_method, request_uri, request_version);
        //errnum = 0;
        if (errnum == 0) {
            FILE *f = check_page_in_cache(request_uri);
            if(f == NULL) {
                printf("Not found in cache, requesting from server\n");
                handle_request(client_sock, client_message, request_uri, host_name);
            } else {
                send_page_from_cache(f, client_sock);
            }
        } else {
            printf("Erorr!!!!\n");
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