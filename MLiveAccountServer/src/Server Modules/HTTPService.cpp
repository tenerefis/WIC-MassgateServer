#include "../stdafx.h"
#include <stdio.h>
#include <cstdlib>

#define HTTPLog(format, ...) DebugLog(L_INFO, "[Http]: "format, __VA_ARGS__)

mg_context *g_MongooseContext;

void HTTPService_Startup()
{
	const char *options[] =
	{
		"listening_ports", "80",
		"num_threads", "2",
		nullptr,
	};

	mg_callbacks callbacks;
	memset(&callbacks, 0, sizeof(mg_callbacks));

	callbacks.begin_request = HTTPService_HandleRequest;
	callbacks.log_message = HTTPService_HandleLog;

	g_MongooseContext = mg_start(&callbacks, nullptr, options);

	if(!g_MongooseContext)
		DebugLog(L_ERROR, "[Http]: Error: Failed to start sever module");
	else
		HTTPLog("Service started");
}

void HTTPService_Shutdown()
{
	if(g_MongooseContext)
		mg_stop(g_MongooseContext);
}

int HTTPService_HandleRequest(mg_connection *conn)
{
	const mg_request_info *request_info	= mg_get_request_info(conn);

	HTTPLog("Url: %s?%s", request_info->uri, request_info->query_string);

	if(request_info->query_string && strstr(request_info->uri, "/texts/gettext"))
		return HTTPService_HandleGettext(conn, request_info);

	if(strstr(request_info->uri, "/massgatebutton/"))
		return HTTPService_HandleButton(conn, request_info);

	return 0;
}

int HTTPService_HandleLog(const mg_connection *, const char *message)
{
	HTTPLog("%s", message);

	return 1;
}

int HTTPService_HandleGettext(mg_connection *conn, const mg_request_info *request_info)
{
	mg_printf(conn, "%s\r\n%s\r\n%s\r\n\r\n", "HTTP/1.1 200 OK", "Connection: Close", "Content-Type: text/plain; charset=utf-8");

	if(strstr(request_info->query_string, "type=welcome"))
	{
		char *textData = "<INSERT WELCOME MESSGAE HERE>\r\n";
		HTTPService_WriteMultibyte(conn, textData);

		return 1;
	}
	
	if(strstr(request_info->query_string, "type=news"))
	{
		char *textData = "WORLD IN CONFLICT - NEWS UPDATE JUNE 26\r\n"
					"-----------------------\r\n"
					"\r\n"
					"- Test message\r\n";
		HTTPService_WriteUnicode(conn, textData);

		return 1;
	}

	return 0;
}

int HTTPService_HandleButton(mg_connection *conn, const mg_request_info *request_info)
{
	if(strstr(request_info->uri, "button_url_"))
	{
		mg_printf(conn, "%s\r\n%s\r\n%s\r\n\r\n", "HTTP/1.1 200 OK", "Connection: Close", "Content-Type: text/plain; charset=utf-8");

		HTTPService_WriteMultibyte(conn, "http://some_non_working_url.nope\r\n");

		return 1;
	}
	
	if(strstr(request_info->uri, "button_image_"))
	{
		char directoryBuffer[MAX_PATH];
		GetCurrentDirectory(sizeof(directoryBuffer), directoryBuffer);

		strcat_s(directoryBuffer, "\\data\\button_image_EN.tga");
		mg_send_file(conn, directoryBuffer);

		return 1;
	}

	return 0;
}

bool HTTPService_UsesUnicode(const mg_request_info *request_info)
{
	for(int i = 0; i < request_info->num_headers; i++)
	{
		if(_stricmp(request_info->http_headers[i].name, "Accept"))
			continue;

		if(_stricmp(request_info->http_headers[i].value, "*/*"))
			break;

		return true;
	}

	return false;
}

void HTTPService_WriteUnicode(mg_connection *conn, char *string)
{
	size_t stringLen		= strlen(string);
	wchar_t *temp_buffer	= new wchar_t[stringLen + 1];

	uint newLen = MC_Misc::MultibyteToUnicode(string, temp_buffer, stringLen + 1);

	mg_write(conn, temp_buffer, newLen * sizeof(wchar_t));

	delete [] temp_buffer;
}

void HTTPService_WriteMultibyte(mg_connection *conn, char *string)
{
	mg_write(conn, string, strlen(string));
}