all: proxy

proxy: web-proxy.c
	gcc web-proxy.c -o proxy

clean:
	rm proxy