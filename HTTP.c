#include <openssl/ssl.h>
#include <time.h>
#include <uuid/uuid.h>
#include "SQL.h"
#include "session.h"
#include "User.h"

 char* get_cookie(unsigned char* buf){
	char *cookie_header = strstr(buf, "Cookie: ");
	if (cookie_header != NULL){
		const char *end = strchr(cookie_header, '=');
		const char* cookie = end+1;
		const char *start = cookie;
		static char guid[37];  
		strncpy(guid, start, 36);
		guid[36] = '\0';
		return guid; 
	}
}

char* retrieve_request_body(unsigned char* buf){
            char* requestBody = strstr(buf, "\r\n\r\n");
            if (requestBody != NULL) {
                requestBody += 4;	
		const char *end = strchr(requestBody, '}');
	        size_t jsonLength = end - requestBody + 1; 
	        char jsonPart[jsonLength + 1];             
	        strncpy(jsonPart, requestBody, jsonLength);
	        jsonPart[jsonLength] = '\0';
		static char buffer[100];
		strcpy(buffer, jsonPart);
		return buffer;
	    }
}

char *get_file_buffer(char* filename){
	FILE *html_pcontent;
	long content_size;
	char *buffer; 
	html_pcontent = fopen(filename, "r");
	if (!html_pcontent){
		perror(filename);
		exit(1);
	}
	fseek(html_pcontent, 0L, SEEK_END);
	content_size = ftell(html_pcontent);
	rewind(html_pcontent);
	buffer = malloc(content_size+1);
	if (!buffer){
		fclose(html_pcontent);
		free(buffer);
		fputs("Entire read fails", stderr);
		exit(1);
	}
	if (1 != fread(buffer, content_size, 1, html_pcontent)){
		fclose(html_pcontent);
		free(buffer);
		fputs("entire reads fail", stderr);
		exit(1);
	}
	buffer[content_size] = '\0';
	fclose(html_pcontent);
	return buffer;


}

char *get_route(unsigned char* buf){
	char *route = buf + 4;
	char *end =  strchr(route, ' ');
	if (end) {
		*end = '\0';
		return route;
	}
}


void send_json_to_client(SSL *cSSL, char*json){
	int html_length = strlen(json);
	char http_header[2048];
	snprintf(http_header, sizeof(http_header),
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: application/json\r\n"
			"Content-Length: %d\r\n"
			"\r\n", html_length);
	printf("%s\n", http_header);
	if (SSL_write(cSSL, http_header, strlen(http_header)) <=0){
		printf("ERROR HTTP HEADER SENDING DATA TO CLIENT\n");
	}
	if(SSL_write(cSSL, json, html_length)<=0){
		printf("ERROR SENDING JSON DATA TO CLIENT\n");
	}
}



void render_template(unsigned char *buf, SSL *cSSL, char* request_cookie){
	static char user_json[255];
	int user_found = 0;
	if (request_cookie == NULL){	
		printf("No Request cookie found.\n");
		buf = "index.html";
	}else{
		struct Session session = get_session(request_cookie);
		if (!session.exists){
			printf("SESSION DOES NOT EXIST.\n");
			buf = "index.html";
		}else{
			struct User user = get_user_by_id(session.userId);
			if (!user.exists){
				buf = "index.html";
			}else{
				snprintf(user_json, sizeof(user_json), "{\"username\":\"%s\",\"userid\":\"%s\"email\":\"%s\"}",user.fullname,user.Id, user.email);
				user_found = 1;
			
			}
		}
	}
	printf("Request cookie %s to render %s\n",  request_cookie, buf);
	char *html_buffer = get_file_buffer(buf);
	int html_length = strlen(html_buffer);
	char http_header[2048];
	snprintf(http_header, sizeof(http_header),
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: text/html\r\n"
			"Connection: close\r\n"
			"Content-Length: %d\r\n"
			"\r\n", html_length);

	SSL_write(cSSL, http_header, strlen(http_header));
	SSL_write(cSSL, html_buffer, html_length);
	free(html_buffer);
}


char* create_cookie(char* key, char* value){
	static char cookie[255];
	snprintf(cookie, sizeof(cookie), "%s=%s;Path=/home;Secure;HttpOnly",key,value);
	return cookie;
}


void send_response_code(int code, SSL *cSSL, char* cookie){
	char http_header[2048];
	if (code == 200){
		snprintf(http_header, sizeof(http_header),
				"HTTP/1.1 200 OK\r\n"
				"Set-Cookie: %s\r\n"
				"\r\n", cookie);
		SSL_write(cSSL, http_header, strlen(http_header));
	}else if (code == 401) {
		snprintf(http_header, sizeof(http_header),
				"HTTP/1.1 401 Unauthorized\r\n"
				"\r\n");
		SSL_write(cSSL, http_header, strlen(http_header));
	
	}
}
