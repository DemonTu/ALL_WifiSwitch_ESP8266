#include "ets_sys.h"
#include "osapi.h"
#include "mem.h"

#include "user_interface.h"

#include "HttpTool.h"

#define pheadbuffer 	"Content-type: application/json\r\n\
Connection: keep-alive\r\n\
Cache-Control: no-cache\r\n\
User-Agent: Mozilla/5.0 (Windows NT 5.1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/30.0.1599.101 Safari/537.36 \r\n\
Accept: */*\r\n\
Accept-Encoding: gzip,deflate,sdch\r\n\
Accept-Language: zh-CN,zh;q=0.8\r\n\r\n"


/******************************************************************************
 * FunctionName : http_request_data_fill
 * Description  : processing the data as http format and send to the client or server
 * Parameters   : arg -- argument to set for client or server
 *                responseOK -- true or false
 *                psend -- The send data
 * Returns      :
*******************************************************************************/
void ICACHE_FLASH_ATTR
http_request_data_fill(char *sBuf, char *outBuf, ProtocolType method)
{
    uint16 length = 0;
    char *httphead = (char *)os_malloc(httpHead_size);

	switch(method)
	{
		case GET:
			break;
		case POST:			
			length = os_sprintf(httphead, 
							"POST http://iot.espressif.cn/ HTTP/1.1\r\nHost: iot.espressif.cn\r\nContent-Length: %d\r\n"pheadbuffer"",
							sBuf ? os_strlen(sBuf) : 0);
			os_memcpy(outBuf, httphead, length);
			if (sBuf)
			{
				os_memcpy(outBuf+length, sBuf, os_strlen(sBuf));
			}
            
			break;
		default:
			break;
	}
	os_free(httphead);
}


/******************************************************************************
 * FunctionName : http_parse_request_url
 * Description  : parse the received data from the server
 * Parameters   : precv -- the received data
 *                purl_frame -- the result of parsing the url
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
http_parse_request_url(char *precv, URL_Frame *purl_frame)
{
    char *str = NULL;
    uint8 length = 0;
    char *pbuffer = NULL;
    char *pbufer = NULL;

    if (purl_frame == NULL || precv == NULL) {
        return;
    }

    pbuffer = (char *)os_strstr(precv, "Host:");

    if (pbuffer != NULL) 
	{
        length = pbuffer - precv;
        pbufer = (char *)os_zalloc(length + 1);
        pbuffer = pbufer;
        os_memcpy(pbuffer, precv, length);
        os_memset(purl_frame->pSelect, 0, URLSize);
        os_memset(purl_frame->pCommand, 0, URLSize);
        os_memset(purl_frame->pFilename, 0, URLSize);

        if (os_strncmp(pbuffer, "GET ", 4) == 0) {
            purl_frame->Type = GET;
            pbuffer += 4;
        } else if (os_strncmp(pbuffer, "POST ", 5) == 0) {
            purl_frame->Type = POST;
            pbuffer += 5;
        }

        pbuffer ++;
        str = (char *)os_strstr(pbuffer, "?");

        if (str != NULL) 
		{
            length = str - pbuffer;
            os_memcpy(purl_frame->pSelect, pbuffer, length);
            str ++;
            pbuffer = (char *)os_strstr(str, "=");

            if (pbuffer != NULL) 
			{
                length = pbuffer - str;
                os_memcpy(purl_frame->pCommand, str, length);
                pbuffer ++;
                str = (char *)os_strstr(pbuffer, "&");

                if (str != NULL) 
				{
                    length = str - pbuffer;
                    os_memcpy(purl_frame->pFilename, pbuffer, length);
                } 
				else 
				{
                    str = (char *)os_strstr(pbuffer, " HTTP");

                    if (str != NULL) {
                        length = str - pbuffer;
                        os_memcpy(purl_frame->pFilename, pbuffer, length);
                    }
                }
            }
        }

        os_free(pbufer);
    } 
	else 
	{
        return;
    }
}

/******************************************************************************
 * FunctionName : http_check_data
 * Description  : check the received data from the server
 * Parameters   : precv -- the received data
 *                length -- the length of received data
 * Returns      : none
*******************************************************************************/
bool ICACHE_FLASH_ATTR
http_check_data(char *precv, uint16 length)
{
        //bool flag = true;
    char length_buf[10] = {0};
    char *ptemp = NULL;
    char *pdata = NULL;
    char *tmp_precvbuffer;
    uint16 tmp_length = length;
    uint32 tmp_totallength = 0;
    uint32 contentLength = 0;
    ptemp = (char *)os_strstr(precv, "\r\n\r\n");
    
    if (ptemp != NULL) {
        tmp_length -= ptemp - precv;
        tmp_length -= 4;
        tmp_totallength += tmp_length;
        
        pdata = (char *)os_strstr(precv, "Content-Length: ");
        
        if (pdata != NULL)
		{
            pdata += 16; //sizeof("Content-Length: ");
            tmp_precvbuffer = (char *)os_strstr(pdata, "\r\n");
            
            if (tmp_precvbuffer != NULL)
			{
                os_memcpy(length_buf, pdata, tmp_precvbuffer - pdata);
                contentLength = atoi(length_buf);
                os_printf("A_dat:%u,tot:%u,lenght:%u\n",contentLength,tmp_totallength,tmp_length);
                if(contentLength != tmp_totallength)
				{
                    return false;
                }
            }
        }
    }
    return true;
}

