#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "http.h"

http_response handle_request(char* method, char* url, char* query);

int main(void)
{
	http_set_listener(handle_request);
	http_start_server();
}

http_response handle_request(char* method, char* url, char* query)
{
	http_response resp;

	resp.response_code = STATUS_CODE_OK;
	sprintf(resp.response_body, "Method: %s\r\nURL: %s\r\nQuery: %s", method, url, query);

	char* param = NULL;
	http_get_query_param(query, "name", &param);
	free(param);

	/*if (strcasecmp(method, "GET") == 0)
	{
		int ret = system("/home/david/script");
		if (ret == 0)
		{
			ok(client);
			close(client);
			return;
		}
		else
		{
			fail(client, WEXITSTATUS(ret));
			close(client);
			return;
		}
	}
	else
	{
		bad_request(client);
		close(client);
		return;
	}*/

	return resp;
}