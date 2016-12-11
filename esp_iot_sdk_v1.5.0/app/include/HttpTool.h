#ifndef __HTTPTOOL_H
	#define __HTTPTOOL_H

#define httpHead_size	(1024)
#define httpBody_size   (512)
#define URLSize 10


typedef enum ProtocolType {
	GET = 0,
	POST,
} ProtocolType;

typedef struct URL_Frame {
    enum ProtocolType Type;
    char pSelect[URLSize];
    char pCommand[URLSize];
    char pFilename[URLSize];
} URL_Frame;

extern void http_request_data_fill(char *sBuf, char *outBuf, ProtocolType method);
extern void http_response_data_fill(char *sBuf, char *outBuf, bool responseOK);
extern void http_parse_request_url(char *precv, URL_Frame *purl_frame);
extern void http_parse_url(char *precv, URL_Frame *purl_frame);
extern bool http_check_data(char *precv, uint16 length);

#endif	
