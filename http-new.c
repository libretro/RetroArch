////////////////////////////////
//http.h
////////////////////////////////
#define _POSIX_SOURCE
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

struct http;
struct http* http_new(const char * url);

//You can use this to call http_poll only when something will happen; select() it for reading.
int http_fd(struct http* state);

//Returns true if it's done. 'total' will be SIZE_MAX if it's not known.
bool http_poll(struct http* state, size_t* progress, size_t* total);

//200, 404, or whatever.
int http_status(struct http* state);

//Returns the downloaded data. The returned buffer is owned by the HTTP handler; it's freed by http_delete.
//If the status is not 20x and accept_error is false, it returns NULL.
uint8_t* http_data(struct http* state, size_t* len, bool accept_error);

//Cleans up all memory.
void http_delete(struct http* state);

////////////////////////////////
//test.c
////////////////////////////////
#include<stdio.h>
int main()
{
	struct http* http3=http_new("http://www.wikipedia.org/");
	while (!http_poll(http3, NULL, NULL)) {}
	//struct http* http1=http_new("http://floating.muncher.se:22/");
	//struct http* http2=http_new("http://floating.muncher.se/sepulcher/");
	//struct http* http3=http_new("http://www.wikipedia.org/");
	//while (!http_poll(http3, NULL, NULL)) {}
	//while (!http_poll(http2, NULL, NULL)) {}
	//while (!http_poll(http1, NULL, NULL)) {}
	//printf("%i %i %i %p %s %s\n",
	//http_status(http1),http_status(http2),http_status(http3),
	//(char*)http_data(http1, NULL, false),http_data(http2, NULL, true),http_data(http3, NULL, true));
}
////////////////////////////////
//http.c
////////////////////////////////
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#if defined(_WIN32)
	//much of this is copypasta from elsewhere, I don't know if it works.
	#define _WIN32_WINNT 0x501
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#define isagain(bytes) (false)
	#define MSG_NOSIGNAL 0
	#define close closesocket
	#ifdef _MSC_VER
		#pragma comment(lib, "ws2_32.lib")
	#endif
#else
	//#include <sys/types.h>
	#include <sys/socket.h>
	#include <sys/time.h>
	//#include <sys/un.h>
	//#include <sys/stat.h>
	//#include <netinet/in.h>
	//#include <netinet/tcp.h>
	//#include <arpa/inet.h>
	#include <netdb.h>
	#include <fcntl.h>
	//#include <signal.h>
	#include <errno.h>
	#include <unistd.h>
	#define isagain(bytes) (bytes<0 && (errno==EAGAIN || errno==EWOULDBLOCK))
#endif

struct http {
	int fd;
	int status;
	uint8_t* data;
	size_t pos;
	size_t len;
};

static bool http_parse_url(char * url, char* * domain, int* port, char* * location)
{
	char* scan;
	if (strncmp(url, "http://", strlen("http://"))!=0) return false;
	scan = url+strlen("http://");
	*domain = scan;
	while (*scan!='/' && *scan!=':' && *scan!='\0') scan++;
	if (*scan=='\0') return false;
	if (*scan==':')
	{
		*scan='\0';
		if (!isdigit(scan[1])) return false;
		*port=strtoul(scan+1, &scan, 10);
		if (*scan!='/') return false;
	}
	else // known '/'
	{
		*scan='\0';
		*port=80;
	}
	*location=scan+1;
	return true;
}

static int http_new_socket(const char * domain, int port)
{
	char portstr[16];
	sprintf(portstr, "%i", port);
	
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family=AF_UNSPEC;
	hints.ai_socktype=SOCK_STREAM;
	hints.ai_flags=0;
	
	struct addrinfo* addr=NULL;
	getaddrinfo(domain, portstr, &hints, &addr);
	if (!addr) return -1;
	
	int fd=socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
#ifndef _WIN32
	//30 second pauses annoy me
	struct timeval timeout;
	timeout.tv_sec=4;
	timeout.tv_usec=0;
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof timeout);
#endif
	if (connect(fd, addr->ai_addr, addr->ai_addrlen)!=0)
	{
		freeaddrinfo(addr);
		close(fd);
		return -1;
	}
	freeaddrinfo(addr);
#ifndef _WIN32
	//Linux claims to not know that select() should only give sockets where read() is nonblocking
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0)|O_NONBLOCK);
#endif
	return fd;
}

static void http_send(int fd, bool * error, const char * data, size_t len)
{
	if (*error) return;
	while (len)
	{
		ssize_t thislen=send(fd, data, len, MSG_NOSIGNAL);
		if (thislen<=0)
		{
			if (!isagain(thislen))
			{
				continue;
			}
			else
			{
				*error=true;
				return;
			}
		}
		data+=thislen;
		len-=thislen;
	}
	return;
}

static void http_send_str(int fd, bool * error, const char * text)
{
	http_send(fd, error, text, strlen(text));
}

static ssize_t http_recv(int fd, bool * error, uint8_t* data, size_t maxlen)
{
	if (*error) return -1;
	
	ssize_t bytes=recv(fd, data, maxlen, 0);
	if (bytes>0) return bytes;
	else if (bytes==0) return -1;
	else if (isagain(bytes)) return 0;
	else
	{
		*error=true;
		return -1;
	}
}

struct http* http_new(const char * url)
{
	char* urlcopy=malloc(strlen(url)+1);
	char* domain;
	int port;
	char* location;
	struct http* state=NULL;
	int fd=-1;
	bool error;
	strcpy(urlcopy, url);
	if (!http_parse_url(urlcopy, &domain, &port, &location)) goto fail;
	fd=http_new_socket(domain, port);
	if (fd==-1) goto fail;
	
	error=false;
	
	//this is a bit lazy, but it works
	http_send_str(fd, &error, "GET /");
	http_send_str(fd, &error, location);
	http_send_str(fd, &error, " HTTP/1.1\r\n");
	
	http_send_str(fd, &error, "Host: ");
	http_send_str(fd, &error, domain);
	if (port!=80)
	{
		char portstr[16];
		sprintf(portstr, ":%i", port);
		http_send_str(fd, &error, portstr);
	}
	http_send_str(fd, &error, "\r\n");
	
	http_send_str(fd, &error, "Connection: close\r\n");
	
	http_send_str(fd, &error, "\r\n");
	
	if (error) goto fail;
	
	free(urlcopy);
	
	state=malloc(sizeof(struct http));
	state->fd=fd;
	state->status=-1;
	state->data=NULL;
	return state;
	
fail:
	if (fd!=-1) close(fd);
	free(urlcopy);
	return NULL;
}

int http_fd(struct http* state)
{
	return state->fd;
}

bool http_poll(struct http* state, size_t* progress, size_t* total)
{
	//TODO: these variables belong in struct http - they're here for now because that's easier.
	enum { p_header_top, p_header, p_body, p_body_chunklen, p_done, p_error };
	enum { t_full, t_len, t_chunk };
	char part = p_header_top;
	char bodytype = t_full;
	bool error=false;
	size_t pos=0;
	size_t len=0;
	size_t buflen=512;
	char * data=malloc(buflen);
	//end of struct http
	
	ssize_t newlen;
again:
	newlen=0;
	
	if (part < p_body)
	{
		//newlen=http_recv(state->fd, &error, data+pos, buflen-pos);
		newlen=http_recv(state->fd, &error, data+pos, 1);
		if (newlen<0) goto fail;
		if (newlen==0) goto again;
		if (pos+newlen >= buflen-64)
		{
			buflen*=2;
			data=realloc(data, buflen);
		}
		pos+=newlen;
		while (part < p_body)
		{
			char * dataend=data+pos;
//printf("%li\n",pos);
			char * lineend=memchr(data, '\n', pos);
//printf("%i '%s'\n",pos,data);
			if (!lineend) break;
			*lineend='\0';
			if (lineend!=data && lineend[-1]=='\r') lineend[-1]='\0';
			
//puts(data);
			if (part==p_header_top)
			{
				if (strncmp(data, "HTTP/1.", strlen("HTTP/1."))!=0) goto fail;
				state->status=strtoul(data+strlen("HTTP/1.1 "), NULL, 10);
				part=p_header;
			}
			else
			{
				if (!strncmp(data, "Content-Length: ", strlen("Content-Length: ")))
				{
					bodytype=t_len;
					len=strtol(data+strlen("Content-Length: "), NULL, 10);
				}
				if (!strcmp(data, "Transfer-Encoding: chunked"))
				{
					bodytype=t_chunk;
				}
				//TODO: save headers somewhere
				if (*data=='\0')
				{
					part=p_body;
					if (bodytype==t_chunk) part=p_body_chunklen;
				}
			}
			
			memmove(data, lineend+1, dataend-(lineend+1));
			pos=(dataend-(lineend+1));
//printf("[%s] %li\n",data,pos);
		}
		if (part>=p_body)
		{
			newlen=pos;
			pos=0;
		}
	}
	if (part >= p_body && part < p_done)
	{
//printf("%li/%li %.*s\n",pos,len,pos,data);
		if (!newlen)
		{
			//newlen=http_recv(state->fd, &error, data+pos, buflen-pos);
			newlen=http_recv(state->fd, &error, data+pos, 1);
			if (newlen<0)
			{
				if (bodytype==t_full) part=p_done;
				else goto fail;
				newlen=0;
			}
			if (newlen==0) goto again;
//printf("%lu+%lu >= %lu-64\n",pos,newlen,buflen);
			if (pos+newlen >= buflen-64)
			{
				buflen*=2;
				data=realloc(data, buflen);
			}
		}
		
parse_again:
		if (bodytype==t_chunk)
		{
			if (part==p_body_chunklen)
			{
				pos+=newlen;
				if (pos-len >= 2)
				{
					//len=start of chunk including \r\n
					//pos=end of data
					char * fullend=data+pos;
					char * end=memchr(data+len+2, '\n', pos-len-2);
					if (end)
					{
						size_t chunklen=strtoul(data+len, NULL, 16);
						pos=len;
						end++;
						memmove(data+len, end, fullend-end);
						len=chunklen;
						newlen=(fullend-end);
						//len=num bytes
						//newlen=unparsed bytes after \n
						//pos=start of chunk including \r\n
						part=p_body;
						if (len==0)
						{
							part=p_done;
							len=pos;
						}
						goto parse_again;
					}
				}
			}
			else if (part==p_body)
			{
				if (newlen >= len)
				{
					pos+=len;
					newlen-=len;
					len=pos;
					part=p_body_chunklen;
					goto parse_again;
				}
				else
				{
					pos+=newlen;
					len-=newlen;
				}
			}
		}
		else
		{
			pos+=newlen;
			if (pos==len) part=p_done;
			if (pos>len) goto fail;
		}
	}

if (part==p_done)
{
data[len]='\0';
puts(data);
	return true;
}
else goto again;
	
fail:
puts("ERR");
	part=p_error;
	return true;
}

int http_status(struct http* state)
{
	return state->status;
}

uint8_t* http_data(struct http* state, size_t* len, bool accept_error)
{
	if (len) *len=state->len;
	return state->data;
}

void http_delete(struct http* state)
{
	if (state->fd != -1) close(state->fd);
	if (state->data) free(state->data);
	free(state);
}
