#ifndef _HTTP_H_

typedef enum 
{ 
	STATUS_CODE_OK = 200, 
	STATUS_CODE_NOT_FOUND = 404,
	STATUS_CODE_BAD_REQUEST = 400,
	STATUS_CODE_INTERNAL_SERVER_ERROR = 500
} http_status_code;

typedef struct _http_response {
	int response_code;
	char response_body[2048];
} http_response;

typedef http_response(*http_handler)(char* method, char* url, char* query);

void http_set_listener(http_handler);
void http_start_server(void);
/*
* Value of *param* must be freed by caller
*/
void http_get_query_param(const char* query, const char* param_name, char** const param);

#endif // !_HTTP_H_

