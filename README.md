### Web Proxy

This web proxy is written in C and s capable of relaying HTTP requests from clients to the HTTP servers. It only supports GET.

To build the proxy run : make proxy. To start the proxy run : "./server <port_number> <timeout>"


It contains the following files: 

1. README.md
2. web-proxy.c  
3. blocked.txt 
4. host-ip-cache.txt
5. makefile

**Compile and Start the Proxy:**
1. Put the web-proxy folder in appropriate location, either on your local machine or remote server.
2. Type _make clean_ followed by _make proxy_. You will see an executable called proxy in that folder.
3. If you are on a remote linux machine, get the IP address of the server by entering _ifconfig | grep "inet addr"_. 
4. Start the server by entering _./proxy <port_number> <timeout>
5. If you get the error "unable to bind socket", please use a different port or kill the process using that port.


**GET:**

	Usage: First, you need to configure the proxy on your browser with hostname 127.0.0.1 and the port. Enter an http website like http://conference2013.m.marbigen.org/ on your browser 

	Purpose: The proxy will first check if the reqeusted page is present in its cache tries to find a default web page such as “index.html” on the requested directory (root directory www).

**Description:**
1. Only GET method is supported. For any other method, HTTP 400 Bad Request error message is sent to the client.
2. If the requested hostname is not resolved to any IP address, then the error message (HTTP 404 Not Found error message) is sent to the client and displayed in the browser.
3. Page cache is implemented. When the browser requests a page, the proxy checks to see if a page exists in the proxy before retrieving a page from a remote server. If there is a valid cached copy of the page, that should be presented to the client instead of creating a new server connection. The cached page is stored in a local file.
4. Timeout setting configured while starting the proxy is used to check expiration time of cache. It does not serve a page from the cache if the request time exceeds the expiration time limit. 
5. Hostname cache is implemented. It stores a cache of IP addresses it resolved for any hostname and store (hostname,IP) pair in local file. Thus, if same hostname is requested again, the proxy skips the DNS query to reduce DNS query time. 
6. The proxy maintains a list of blocked hostnames and IP addresses. For every incoming request, the proxy looks up this data and denies requests for these websites or IPs by returning “ERROR 403 Forbidden”.

**Design Decisions**:
1. One thread per request and a new connection is opened each time, in case page is not present in the cache

2. One single main thread for accepting new requests concurrently which then spawns another thread to handle the requests
3. All the hosts/ip to be blocked can be placed in BLOCKED file. 
4. Time stamp is stored as first line of the file followed by the page data. Cache expiration is checked through this.


