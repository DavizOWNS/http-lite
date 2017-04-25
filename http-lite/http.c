#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>

#include "http.h"
#include "log.h"

#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

void accept_request(int);
void bad_request(int);
void error_die(const char *);
int get_line(int, char *, int);
int startup(u_int16_t *);
void ok(int);
void fail(int, int);
void write_response(int client, const http_response response);
void free_response(http_response response);
void* process_request(void* arg);

http_handler mHandler = NULL;

void http_set_listener(http_handler handler)
{
	mHandler = handler;
}

void* process_request(void* arg)
{
	accept_request(*((int*)arg));
	free(arg);

	return NULL;
}

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
* return.  Process the request appropriately.
* Parameters: the socket connected to the client */
/**********************************************************************/
void accept_request(int client)
{
	char buf[1024];
	char method[255];
	char url[255];
	size_t i, j;
	char *query_string = NULL;

	//get method
	get_line(client, buf, sizeof(buf));
	i = 0; j = 0;
	while (!ISspace(buf[j]) && (i < sizeof(method) - 1))
	{
		method[i] = buf[j];
		i++; j++;
	}
	method[i] = '\0';

	//get url e.g. /getSurname
	i = 0;
	while (ISspace(buf[j]) && (j < sizeof(buf)))
		j++;
	while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)))
	{
		url[i] = buf[j];
		i++; j++;
	}
	url[i] = '\0';

	//get query string  e.g. name=david&home=7
	query_string = url;
	while ((*query_string != '?') && (*query_string != '\0'))
		query_string++;
	if (*query_string == '?')
	{
		*query_string = '\0';
		query_string++;
	}

	debug_print(dbg_level_trace, "Processing request: %s %s %s", method, url, query_string);

	http_response response = mHandler(method, url, query_string);
	write_response(client, response);
	free_response(response);
	close(client);
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
* Parameters: client socket */
/**********************************************************************/
void bad_request(int client)
{
	char buf[1024];

	sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-type: text/plain\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Only GET is allowed\r\n");
	send(client, buf, strlen(buf), 0);
}

void http_get_query_param(const char* query, const char* param_name, char** const param)
{
	*param = NULL;

	char name[100];
	int q_idx = 0;
	int name_idx = 0;
	while (query[q_idx] != '\0')
	{
		if (query[q_idx] == '=')
		{
			name[name_idx] = '\0';
			if (strcasecmp(param_name, name) == 0)
			{
				q_idx++;
				int val_idx = 0;
				size_t value_size = 1024;
				char* value = (char*)calloc(value_size, sizeof(char));
				while (query[q_idx] != '\0' && query[q_idx] != '&') {
					if (val_idx == value_size - 1)
					{
						value_size = value_size + 1024;
						char* new_val = (char*)calloc(value_size, sizeof(char));
						strcpy(new_val, value);
						free(value);
						value = new_val;
					}
					value[val_idx] = query[q_idx];
					val_idx++;
					q_idx++;
				}
				value[val_idx] = '\0';

				*param = value;
			}
			else
			{
				while (query[q_idx] != '\0' && query[q_idx] != '&') q_idx++;
			}
		}
		else if (query[q_idx] == '&')
		{
			name_idx = 0;
			q_idx++;
		}
		else
		{
			name[name_idx] = query[q_idx];
			name_idx++;
			q_idx++;
		}
	}
}

void free_response(http_response response)
{
	//if (response.response_body)
	//	free(response.response_body);
}

void write_response(int client, const http_response response)
{
	char buf[1024];

	ssize_t ret;
	switch (response.response_code)
	{
	case STATUS_CODE_OK:
		sprintf(buf, "HTTP/1.0 200 OK\r\n");
		ret = send(client, buf, strlen(buf), MSG_NOSIGNAL);
		break;
	case STATUS_CODE_BAD_REQUEST:
		break;
	case STATUS_CODE_NOT_FOUND:
		break;
	case STATUS_CODE_INTERNAL_SERVER_ERROR:
	default:
		sprintf(buf, "HTTP/1.0 500 Internal server error\r\n");
		ret = send(client, buf, strlen(buf), MSG_NOSIGNAL);
		break;
	}
	if (ret == -1 && errno == EPIPE)
	{
		debug_print(dbg_level_error, strerror(errno));
		close(client);
		return;
	}
	
	sprintf(buf, "Content-type: text/plain\r\n");
	ret = send(client, buf, strlen(buf), MSG_NOSIGNAL);
	if (ret == -1 && errno == EPIPE)
	{
		debug_print(dbg_level_error, strerror(errno));
		close(client);
		return;
	}

	sprintf(buf, "\r\n");
	ret = send(client, buf, strlen(buf), MSG_NOSIGNAL);
	if (ret == -1 && errno == EPIPE)
	{
		debug_print(dbg_level_error, strerror(errno));
		close(client);
		return;
	}

	if (response.response_body)
	{
		sprintf(buf, response.response_body);
		ret = send(client, buf, strlen(buf), MSG_NOSIGNAL);
		if (ret == -1 && errno == EPIPE)
		{
			debug_print(dbg_level_error, strerror(errno));
			close(client);
			return;
		}

	}
	sprintf(buf, "\r\n");
	send(client, buf, strlen(buf), MSG_NOSIGNAL);
}

void ok(int client)
{
	char buf[1024];

	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-type: text/plain\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Logstash completed successfully.\r\n");
	send(client, buf, strlen(buf), 0);
}

void fail(int client, int error_code)
{
	char buf[1024];

	sprintf(buf, "HTTP/1.0 500 Internal server error\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-type: text/plain\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Logstash encountered error. Code: ");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "%d\r\n", error_code);
	send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
* on value of errno, which indicates system call errors) and exit the
* program indicating an error. */
/**********************************************************************/
void error_die(const char *sc)
{
	perror(sc);
	exit(1);
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
* carriage return, or a CRLF combination.  Terminates the string read
* with a null character.  If no newline indicator is found before the
* end of the buffer, the string is terminated with a null.  If any of
* the above three line terminators is read, the last character of the
* string will be a linefeed and the string will be terminated with a
* null character.
* Parameters: the socket descriptor
*             the buffer to save the data in
*             the size of the buffer
* Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
int get_line(int sock, char *buf, int size)
{
	int i = 0;
	char c = '\0';
	int n;

	while ((i < size - 1) && (c != '\n'))
	{
		n = (int)recv(sock, &c, 1, 0);
		/* DEBUG printf("%02X\n", c); */
		if (n > 0)
		{
			if (c == '\r')
			{
				n = (int)recv(sock, &c, 1, MSG_PEEK);
				/* DEBUG printf("%02X\n", c); */
				if ((n > 0) && (c == '\n'))
					recv(sock, &c, 1, 0);
				else
					c = '\n';
			}
			buf[i] = c;
			i++;
		}
		else
			c = '\n';
	}
	buf[i] = '\0';

	return(i);
}

/**********************************************************************/
/* This function starts the process of listening for web connections
* on a specified port.  If the port is 0, then dynamically allocate a
* port and modify the original port variable to reflect the actual
* port.
* Parameters: pointer to variable containing the port to connect on
* Returns: the socket */
/**********************************************************************/
int startup(u_int16_t *port)
{
	int httpd = 0;
	struct sockaddr_in name;

	httpd = socket(PF_INET, SOCK_STREAM, 0);
	if (httpd == -1)
		error_die("socket");
	memset(&name, 0, sizeof(name));
	name.sin_family = AF_INET;
	name.sin_port = htons(*port);
	name.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
		error_die("bind");
	if (*port == 0)  /* if dynamically allocating a port */
	{
		int namelen = sizeof(name);
		if (getsockname(httpd, (struct sockaddr *)&name, (socklen_t*)&namelen) == -1)
			error_die("getsockname");
		*port = ntohs(name.sin_port);
	}
	if (listen(httpd, 5) < 0)
		error_die("listen");
	return(httpd);
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
* implemented.
* Parameter: the client socket */
/**********************************************************************/
void unimplemented(int client)
{
	char buf[1024];

	sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, SERVER_STRING);
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Type: text/html\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "</TITLE></HEAD>\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "</BODY></HTML>\r\n");
	send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
void http_start_server(void)
{
	int server_sock = -1;
	int* new_sock;
	u_int16_t port = 5501;
	int client_sock = -1;
	struct sockaddr_in client_name;
	int client_name_len = sizeof(client_name);

	server_sock = startup(&port);
	printf("httpd running on port %d\n", port);

	while (1)
	{
		client_sock = accept(server_sock,
			(struct sockaddr *)&client_name,
			(socklen_t*)&client_name_len);
		if (client_sock == -1)
			error_die("accept");

		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

		pthread_t sniffer_thread;
		new_sock = (int*)malloc(1);
		*new_sock = client_sock;
		pthread_create(&sniffer_thread, &attr, process_request, (void*)new_sock);
		//accept_request(client_sock);
	}

	close(server_sock);
}
