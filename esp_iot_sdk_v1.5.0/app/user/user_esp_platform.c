/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: user_esp_platform.c
 *
 * Description: The client mode configration.
 *              Check your hardware connection with the host while use this mode.
 *
 * Modification history:
 *     2014/5/09, v1.0 create this file.
*******************************************************************************/
#include "ets_sys.h"
#include "os_type.h"
#include "mem.h"
#include "osapi.h"
#include "user_interface.h"

#include "HttpTool.h"

#include "espconn.h"
#include "user_esp_platform.h"
#include "user_iot_version.h"
#include "upgrade.h"

#include "user_json.h"

#if ESP_PLATFORM
#include "user_plug.h"

#define ESP_DEBUG

#ifdef ESP_DEBUG
#define ESP_DBG os_printf
#else
#define ESP_DBG
#endif

#ifdef USE_DNS
ip_addr_t esp_server_ip;
#endif

LOCAL os_timer_t beacon_timer;
LOCAL os_timer_t client_timer;	// 域名解析 和 路由连接 共用一个时间结构

LOCAL struct espconn user_conn;
LOCAL struct _esp_tcp user_tcp;

LOCAL char pSendBuf[packet_size];
LOCAL char pHttpBody[httpBody_size];

struct esp_platform_saved_param esp_system_param;
/******************************************************************************/
typedef struct
{
	uint8 ping_status;			// 心跳包发送状态
	uint8 device_recon_count;	// 网络重链接或域名解析次数
    uint8 device_status;		// 设备状态
    uint8 rc_flag;
	uint8 reg_ack_flag;
	uint8 hb_ack_flag;
}ESP_PARA_STR;
LOCAL ESP_PARA_STR esp_param;


/******************************************************************************/
void user_esp_platform_check_ip(uint8 reset_flag);


/******************************************************************************/
// 注册json结构
/******************************************************************************
 * FunctionName : regFillData
 * Description  : set up the device reg paramer as a JSON format
 * Parameters   : js_ctx -- A pointer to a JSON set up
 * Returns      : result
*******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR
regFillData(struct jsontree_context *js_ctx)
{
    const char *path = jsontree_path_name(js_ctx, js_ctx->depth - 1);
    char string[32]={0};

    if (os_strncmp(path, "cmd", 3) == 0) 
	{
        os_sprintf(string, "REG");
    } 
	else if (os_strncmp(path, "key", 3) == 0) 
    {
    	// 有待修改
        //os_sprintf(string, "%s", esp_system_param.devkey);
        os_memcpy(string, esp_system_param.devkey, sizeof(esp_system_param.devkey));
    } 
	else if (os_strncmp(path, "deviceID", 8) == 0) 
    {
    	// 有待修改
    	//os_sprintf(string, "%s", esp_system_param.deviceID);
        os_memcpy(string, esp_system_param.deviceID, sizeof(esp_system_param.deviceID));
    }
    jsontree_write_string(js_ctx, string);

    return 0;
}


/******************************************************************************
 * FunctionName : reg_bid_set
 * Description  : set the device BID
 * Parameters   : js_ctx -- A pointer to a JSON set up
 * Returns      : result
*******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR
reg_bid_set(struct jsontree_context *js_ctx, struct jsonparse_state *parser)
{
	int type;
	int resFlag = 0;
	char buffer[20];	
	ESP_DBG("regack json=%s\r\n", parser->json);
	while ((type = jsonparse_next(parser)) != 0)
	{
		if (type == JSON_TYPE_PAIR_NAME) 
		{
			os_bzero(buffer, sizeof(buffer));
			if ((jsonparse_strcmp_value(parser, "Response")==0) || (resFlag==1))
			{
				resFlag = 1;
				if (jsonparse_strcmp_value(parser, "cmd") == 0)
				{
					jsonparse_next(parser);
					jsonparse_next(parser);
					jsonparse_copy_value(parser, buffer, sizeof(buffer));
					if (os_memcmp(buffer, "REGACK", 6))
					{
						ESP_DBG("reg REGACK err=%s\r\n", buffer);
						break;
					}
				}
				else if (jsonparse_strcmp_value(parser, "key") == 0)
				{
					jsonparse_next(parser);
					jsonparse_next(parser);
					jsonparse_copy_value(parser, buffer, sizeof(buffer));
					if (os_memcmp(buffer, esp_system_param.devkey, 
										sizeof(esp_system_param.devkey)))
					{
						ESP_DBG("reg key err=%s\r\n", buffer);
						break;
					}
				}
				else if (jsonparse_strcmp_value(parser, "deviceID") == 0)
				{
					jsonparse_next(parser);
					jsonparse_next(parser);
					jsonparse_copy_value(parser, buffer, sizeof(buffer));
					if (os_memcmp(buffer, esp_system_param.deviceID, 
										sizeof(esp_system_param.deviceID)))
					{
						ESP_DBG("reg deviceID err=%s\r\n", buffer);
						break;
					}
				}
				else if (jsonparse_strcmp_value(parser, "BID") == 0)
				{
					jsonparse_next(parser);
					jsonparse_next(parser);
					esp_system_param.BID = jsonparse_get_value_as_int(parser);						
					os_printf("reg BID=%d\r\n", esp_system_param.BID);
					system_param_save_with_protect(ESP_PARAM_START_SEC, &esp_system_param, sizeof(esp_system_param));

					/* 注册成功, 启动心跳包*/					
					esp_param.ping_status = 1;	// 每50s发一次心跳包
					os_timer_arm(&beacon_timer, BEACON_TIME, 0);
				}
				else
				{
					ESP_DBG("reg json1 error\r\n");
					break;
				}
			}
			else
			{
				ESP_DBG("reg json2 error\r\n");
				break;
			}
		}
	}
	return 0;
}



LOCAL struct jsontree_callback reg_callback =
    JSONTREE_CALLBACK(regFillData, reg_bid_set);
//===================== request ======================================
JSONTREE_OBJECT(reg_request_tree,
                JSONTREE_PAIR("cmd", 	  &reg_callback),
                JSONTREE_PAIR("key",      &reg_callback),
                JSONTREE_PAIR("deviceID", &reg_callback),
                );
JSONTREE_OBJECT(reg_req_tree,
                JSONTREE_PAIR("Request", &reg_request_tree));

JSONTREE_OBJECT(RegReqTree,
                JSONTREE_PAIR("regreq", &reg_req_tree));

//=============== respone ===========================================
JSONTREE_OBJECT(reg_response_tree,
                JSONTREE_PAIR("cmd", 	  &reg_callback),
                JSONTREE_PAIR("key",      &reg_callback),
                JSONTREE_PAIR("deviceID", &reg_callback),
				JSONTREE_PAIR("BID",      &reg_callback),
                );
JSONTREE_OBJECT(reg_res_tree,
                JSONTREE_PAIR("Response", &reg_response_tree));

JSONTREE_OBJECT(RegResTree,
                JSONTREE_PAIR("regres", &reg_res_tree));


/******************************************************************************/
//心跳包Json结构
/******************************************************************************
 * FunctionName : hbFillData
 * Description  : set up the device HB paramer as a JSON format
 * Parameters   : js_ctx -- A pointer to a JSON set up
 * Returns      : result
*******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR
hbFillData(struct jsontree_context *js_ctx)
{
    const char *path = jsontree_path_name(js_ctx, js_ctx->depth - 1);
    char string[32]={0};

    if (os_strncmp(path, "cmd", 3) == 0) 
	{
        os_sprintf(string, "PING");
		jsontree_write_string(js_ctx, string);
    } 
	else if (os_strncmp(path, "key", 3) == 0) 
    {
    	// 有待修改
        os_memcpy(string, esp_system_param.devkey, sizeof(esp_system_param.devkey));
		jsontree_write_string(js_ctx, string);
    } 
	else if (os_strncmp(path, "deviceID", 8) == 0) 
    {
    	// 有待修改
        os_memcpy(string, esp_system_param.deviceID, sizeof(esp_system_param.deviceID));
		jsontree_write_string(js_ctx, string);
    }
	else if (os_strncmp(path, "BID") == 0)
	{
		 jsontree_write_int(js_ctx, esp_system_param.BID);
		 return 0;
	}

    return 0;
}


/******************************************************************************
 * FunctionName : status_set
 * Description  : parse the device status parmer as a JSON format
 * Parameters   : js_ctx -- A pointer to a JSON set up
 *                parser -- A pointer to a JSON parser state
 * Returns      : result
*******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR
hb_ack_parse(struct jsontree_context *js_ctx, struct jsonparse_state *parser)
{
    int type;
	int pingFlag = 0;
    char buffer[20];	
	ESP_DBG("hback json=%s\r\n", parser->json);
    while ((type = jsonparse_next(parser)) != 0)
	{
        if (type == JSON_TYPE_PAIR_NAME) 
		{
			os_bzero(buffer, sizeof(buffer));
			if ((jsonparse_strcmp_value(parser, "Response")==0) || (pingFlag==1))
			{
				pingFlag = 1;
	            if (jsonparse_strcmp_value(parser, "cmd") == 0)
				{
					jsonparse_next(parser);
	                jsonparse_next(parser);
					jsonparse_copy_value(parser, buffer, sizeof(buffer));
					if (os_memcmp(buffer, "PINGACK", 7))
					{
						ESP_DBG("PINGACK err=%s\r\n", buffer);
						break;
					}
	            }
				else if (jsonparse_strcmp_value(parser, "key") == 0)
				{
					jsonparse_next(parser);
	                jsonparse_next(parser);
					jsonparse_copy_value(parser, buffer, sizeof(buffer));
					if (os_memcmp(buffer, esp_system_param.devkey, 
										sizeof(esp_system_param.devkey)))
					{
						ESP_DBG("key err=%s\r\n", buffer);
						break;
					}
				}
				else if (jsonparse_strcmp_value(parser, "deviceID") == 0)
				{
					jsonparse_next(parser);
	                jsonparse_next(parser);
					jsonparse_copy_value(parser, buffer, sizeof(buffer));
					if (os_memcmp(buffer, esp_system_param.deviceID, 
										sizeof(esp_system_param.deviceID)))
					{
						ESP_DBG("deviceID err=%s\r\n", buffer);
						break;
					}					
				}				
				else
				{
					ESP_DBG("hb json1 err=%s\r\n", buffer);
					break;
				}
			}
			else
			{
				ESP_DBG("hb json2 err=%s\r\n", buffer);
				break;
			}
        }
    }
	return 0;
}

LOCAL struct jsontree_callback hb_callback =
    JSONTREE_CALLBACK(hbFillData, hb_ack_parse);

JSONTREE_OBJECT(hb_request_tree,
                JSONTREE_PAIR("cmd", 	  &hb_callback),
                JSONTREE_PAIR("key",      &hb_callback),
                JSONTREE_PAIR("deviceID", &hb_callback),
				JSONTREE_PAIR("BID", 	  &hb_callback),
                );
JSONTREE_OBJECT(hb_req_tree,
                JSONTREE_PAIR("Request", &hb_request_tree));

JSONTREE_OBJECT(HBReqTree,
                JSONTREE_PAIR("hbreq", &hb_req_tree));

//=============== response ==================================
JSONTREE_OBJECT(hb_response_tree,
                JSONTREE_PAIR("cmd", 	  &hb_callback),
                JSONTREE_PAIR("key",      &hb_callback),
                JSONTREE_PAIR("deviceID", &hb_callback),
                );
JSONTREE_OBJECT(hb_res_tree,
                JSONTREE_PAIR("Response", &hb_response_tree));

JSONTREE_OBJECT(HBResTree,
                JSONTREE_PAIR("hbres", &hb_res_tree));


/******************************************************************************/
//远程控制开关
#if PLUG_DEVICE
/******************************************************************************
 * FunctionName : rcFillData
 * Description  : 远程控制开关应答json
 * Parameters   : js_ctx -- A pointer to a JSON set up
 * Returns      : result
*******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR
rcFillData(struct jsontree_context *js_ctx)
{
    const char *path = jsontree_path_name(js_ctx, js_ctx->depth - 1);
    char string[32]={0};

    if (os_strncmp(path, "cmd", 3) == 0) 
	{
        os_sprintf(string, "RCACK");
		jsontree_write_string(js_ctx, string);
    } 
	else if (os_strncmp(path, "key", 3) == 0) 
    {
    	// 有待修改
        os_memcpy(string, esp_system_param.devkey, sizeof(esp_system_param.devkey));
		jsontree_write_string(js_ctx, string);
    } 
	else if (os_strncmp(path, "deviceID", 8) == 0) 
    {
    	// 有待修改
        os_memcpy(string, esp_system_param.deviceID, sizeof(esp_system_param.deviceID));		
		jsontree_write_string(js_ctx, string);
	}
	else if (os_strncmp(path, "status", 8) == 0) 
	{
        jsontree_write_int(js_ctx, user_plug_get_status());	
	}
	else if (os_strncmp(path, "BID") == 0)
	{
		 jsontree_write_int(js_ctx, esp_system_param.BID);
		 return 0;
	}
    return 0;
}

/******************************************************************************
 * FunctionName : status_set
 * Description  : parse the device status parmer as a JSON format
 * Parameters   : js_ctx -- A pointer to a JSON set up
 *                parser -- A pointer to a JSON parser state
 * Returns      : result
*******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR
rc_status_set(struct jsontree_context *js_ctx, struct jsonparse_state *parser)
{
    int type;
	int reqFlag = 0;
	int errorFlag = 0;
    char buffer[20];	
	ESP_DBG("json=%s\r\n", parser->json);
    while ((type = jsonparse_next(parser)) != 0)
	{
        if (type == JSON_TYPE_PAIR_NAME) 
		{
            os_bzero(buffer, 20);
			if ((jsonparse_strcmp_value(parser, "Request")==0) || (reqFlag==1))
			{
				reqFlag = 1;
	            if (jsonparse_strcmp_value(parser, "cmd") == 0)
				{
					jsonparse_next(parser);
	                jsonparse_next(parser);
					jsonparse_copy_value(parser, buffer, sizeof(buffer));
					if (os_memcmp(buffer, "RC", 2))
					{
						ESP_DBG("RC=%s\r\n", buffer);
						errorFlag++;
					}
	            }
				else if (jsonparse_strcmp_value(parser, "key") == 0)
				{
					jsonparse_next(parser);
	                jsonparse_next(parser);
					jsonparse_copy_value(parser, buffer, sizeof(buffer));
					if (os_memcmp(buffer, esp_system_param.devkey, 
										sizeof(esp_system_param.devkey)))
					{
						ESP_DBG("key=%s\r\n", buffer);
						errorFlag++;
					}
				}
				else if (jsonparse_strcmp_value(parser, "deviceID") == 0)
				{
					jsonparse_next(parser);
	                jsonparse_next(parser);
					jsonparse_copy_value(parser, buffer, sizeof(buffer));
					if (os_memcmp(buffer, esp_system_param.deviceID, 
										sizeof(esp_system_param.deviceID)))
					{
						ESP_DBG("deviceID=%s\r\n", buffer);
						errorFlag++;
					}
				}
				else if (jsonparse_strcmp_value(parser, "status") == 0)
				{
					if (0 == errorFlag)
					{
						jsonparse_next(parser);
	                	jsonparse_next(parser);
	                	user_plug_set_status(jsonparse_get_value_as_int(parser));
						esp_param.rc_flag = 1;
					}
				}
				else
				{
					// 错误码
				}
			}
			else
			{
				// 错误码
			}
        }
    }
	
	ESP_DBG("error=%d\n", errorFlag);
    return 0;
}

LOCAL struct jsontree_callback rc_status_callback =
    JSONTREE_CALLBACK(rcFillData, rc_status_set);
//response
JSONTREE_OBJECT(get_status_tree,
                JSONTREE_PAIR("cmd",      &rc_status_callback),
				JSONTREE_PAIR("key",      &rc_status_callback),
				JSONTREE_PAIR("deviceID", &rc_status_callback),
				JSONTREE_PAIR("status",   &rc_status_callback),
				JSONTREE_PAIR("BID",      &rc_status_callback)
                );
JSONTREE_OBJECT(response_tree,
                JSONTREE_PAIR("Response", &get_status_tree));
JSONTREE_OBJECT(RcResTree,
                JSONTREE_PAIR("rcResponse", &response_tree));
//request
JSONTREE_OBJECT(set_status_tree,
                JSONTREE_PAIR("cmd",      &rc_status_callback),
				JSONTREE_PAIR("ked",      &rc_status_callback),
				JSONTREE_PAIR("deviceID", &rc_status_callback),
				JSONTREE_PAIR("status",   &rc_status_callback)
                );
JSONTREE_OBJECT(request_tree,
                JSONTREE_PAIR("Request", &set_status_tree));
JSONTREE_OBJECT(RcReqTree,
                JSONTREE_PAIR("rcRequest", &request_tree));
#endif


/******************************************************************************
 * FunctionName : user_esp_platform_get_token
 * Description  : get the espressif's device token
 * Parameters   : token -- the parame point which write the flash
 * Returns      : none
*******************************************************************************/
uint16 ICACHE_FLASH_ATTR
user_esp_platform_get_bid(void)
{
	return esp_system_param.BID;
}

/******************************************************************************
 * FunctionName : user_esp_platform_set_token
 * Description  : save the token for the espressif's device
 * Parameters   : token -- the parame point which write the flash
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_esp_platform_set_bid(uint16_t BID)
{
	esp_system_param.BID = BID;
    system_param_save_with_protect(ESP_PARAM_START_SEC, &esp_system_param, sizeof(esp_system_param));
}


/******************************************************************************
 * FunctionName : user_esp_platform_set_token
 * Description  : save the token for the espressif's device
 * Parameters   : token -- the parame point which write the flash
 * Returns      : none
*******************************************************************************/
bool ICACHE_FLASH_ATTR
user_esp_platform_is_register(void)
{
	if ((0 == esp_system_param.BID) || (0xffff == esp_system_param.BID))
	{
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}


/******************************************************************************
 * FunctionName : user_esp_platform_set_active
 * Description  : set active flag
 * Parameters   : activeflag -- 0 or 1
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_esp_platform_set_active(uint8 activeflag)
{
    esp_system_param.activeflag = activeflag;

    system_param_save_with_protect(ESP_PARAM_START_SEC, &esp_system_param, sizeof(esp_system_param));
}

void ICACHE_FLASH_ATTR
user_esp_platform_set_connect_status(uint8 status)
{
    esp_param.device_status = status;
}


/******************************************************************************
 * FunctionName : user_esp_platform_get_info
 * Description  : get and update the espressif's device status
 * Parameters   : pespconn -- the espconn used to connect with host
 *                pbuffer -- prossing the data point
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_esp_platform_get_info(struct espconn *pconn, uint8 *pbuffer)
{
    char *pbuf = NULL;

    pbuf = (char *)os_zalloc(packet_size);

    if (pbuf != NULL) 
	{
        ESP_DBG("%s\n", pbuf);
#ifdef CLIENT_SSL_ENABLE
        espconn_secure_sent(pconn, pbuf, os_strlen(pbuf));
#else
        espconn_sent(pconn, pbuf, os_strlen(pbuf));
#endif
        os_free(pbuf);
        pbuf = NULL;
    }
}


/******************************************************************************
 * FunctionName : user_esp_platform_reconnect
 * Description  : reconnect with host after get ip
 * Parameters   : pespconn -- the espconn used to reconnect with host
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_reconnect(struct espconn *pespconn)
{
    ESP_DBG("user_esp_platform_reconnect\n");

    user_esp_platform_check_ip(0);
}


/******************************************************************************
 * FunctionName : user_esp_platform_discon_cb
 * Description  : disconnect successfully with the host
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_discon_cb(void *arg)
{
    struct espconn *pespconn = arg;
    struct ip_info ipconfig;
	struct dhcp_client_info dhcp_info;
    ESP_DBG("user_esp_platform_discon_cb\n");

    os_timer_disarm(&beacon_timer);

    if (pespconn == NULL) {
        return;
    }

    pespconn->proto.tcp->local_port = espconn_port();

#if (PLUG_DEVICE)
    user_link_led_output(1);
#endif
    user_esp_platform_reconnect(pespconn);
}

/******************************************************************************
 * FunctionName : user_esp_platform_discon
 * Description  : A new incoming connection has been disconnected.
 * Parameters   : espconn -- the espconn used to disconnect with host
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_discon(struct espconn *pespconn)
{
    ESP_DBG("user_esp_platform_discon\n");

#if (PLUG_DEVICE)
    user_link_led_output(1);
#endif

#ifdef CLIENT_SSL_ENABLE
    espconn_secure_disconnect(pespconn);
#else
    espconn_disconnect(pespconn);
#endif
}

/******************************************************************************
 * FunctionName : user_esp_platform_sent_cb
 * Description  : Data has been sent successfully and acknowledged by the remote host.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_sent_cb(void *arg)
{
    struct espconn *pespconn = arg;

    ESP_DBG("user_esp_platform_sent_OK\n");
}

/******************************************************************************
 * FunctionName : user_esp_platform_sent
 * Description  : Processing the application data and sending it to the host
 * Parameters   : pespconn -- the espconn used to connetion with the host
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_sent(struct espconn *pespconn)
{
	os_memset(pSendBuf, 0, packet_size);
	os_memset(pHttpBody, '\0', httpBody_size);
    if (FALSE == user_esp_platform_is_register()) 
	{
		// 没有注册过
        json_ws_send((struct jsontree_value *)&RegReqTree, "regreq", pHttpBody);
		http_request_data_fill(pHttpBody, pSendBuf, POST);		
		esp_param.reg_ack_flag = 1;
	}
    else 
	{
        // 已经注册过了
        return;
    }

    ESP_DBG("%s\n", pSendBuf);
#ifdef CLIENT_SSL_ENABLE
    espconn_secure_sent(pespconn, pbuf, os_strlen(pbuf));
#else
    espconn_sent(pespconn, pSendBuf, os_strlen(pSendBuf));
#endif

}

/******************************************************************************
 * FunctionName : user_esp_platform_sent_beacon
 * Description  : sent beacon frame for connection with the host is activate
 * Parameters   : pespconn -- the espconn used to connetion with the host
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_sent_beacon(struct espconn *pespconn)
{
    if (pespconn == NULL) {
        return;
    }

    if (pespconn->state == ESPCONN_CONNECT) 
	{
        if (FALSE == user_esp_platform_is_register()) 
		{
            ESP_DBG("please check device is register.\n");
			/* 未注册 重新启动注册 */
            user_esp_platform_sent(pespconn);
        }
		else 
		{
            if (esp_param.ping_status == 0) 
			{
				/* 心跳没有应答 */
                ESP_DBG("HB no ACK!\n");
                user_esp_platform_discon(pespconn);
            } 
			else 
			{
                char *pbuf = (char *)os_zalloc(packet_size);
					
                if (pbuf != NULL) 
				{
					char *pHttpBody = (char *)os_zalloc(httpBody_size);
					if (pHttpBody != NULL)
					{
						json_ws_send((struct jsontree_value *)&HBReqTree, "hbreq", pHttpBody);
					}
					else
					{	
					}
					http_request_data_fill(pHttpBody, pbuf, POST);
#ifdef CLIENT_SSL_ENABLE
                    espconn_secure_sent(pespconn, pbuf, os_strlen(pbuf));
#else
                    espconn_sent(pespconn, pbuf, os_strlen(pbuf));
#endif
                    esp_param.ping_status = 0;					
					esp_param.hb_ack_flag = 1;	// 等待应答
                    os_timer_arm(&beacon_timer, BEACON_TIME, 0);
					ESP_DBG("%s\n", pbuf);
					
					os_free(pHttpBody);
                    os_free(pbuf);
                }
            }
        }
    } 
	else 
	{ 
        ESP_DBG("network disconnect!\n");
        user_esp_platform_discon(pespconn);
    }
}


/******************************************************************************
 * FunctionName : user_platform_timer_get
 * Description  : get the timers from server
 * Parameters   : pespconn -- the espconn used to connetion with the host
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_platform_timer_get(struct espconn *pespconn)
{

    char *pbuf = (char *)os_zalloc(packet_size);

    if (pespconn == NULL) {
        return;
    }

    ESP_DBG("%s\n", pbuf);
#ifdef CLIENT_SSL_ENABLE
    espconn_secure_sent(pespconn, pbuf, os_strlen(pbuf));
#else
    espconn_sent(pespconn, pbuf, os_strlen(pbuf));
#endif
    os_free(pbuf);
}

/******************************************************************************
 * FunctionName : user_esp_platform_upgrade_cb
 * Description  : Processing the downloaded data from the server
 * Parameters   : pespconn -- the espconn used to connetion with the host
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_upgrade_rsp(void *arg)
{
    struct upgrade_server_info *server = arg;
    struct espconn *pespconn = server->pespconn;
    uint8 *pbuf = NULL;
    char *action = NULL;

    pbuf = (char *)os_zalloc(packet_size);

    if (server->upgrade_flag == true) {
        ESP_DBG("user_esp_platform_upgarde_successfully\n");
        action = "device_upgrade_success";
        ESP_DBG("%s\n",pbuf);

#ifdef CLIENT_SSL_ENABLE
        espconn_secure_sent(pespconn, pbuf, os_strlen(pbuf));
#else
        espconn_sent(pespconn, pbuf, os_strlen(pbuf));
#endif

        if (pbuf != NULL) {
            os_free(pbuf);
            pbuf = NULL;
        }
    } else {
        ESP_DBG("user_esp_platform_upgrade_failed\n");
        action = "device_upgrade_failed";
        ESP_DBG("%s\n",pbuf);

#ifdef CLIENT_SSL_ENABLE
        espconn_secure_sent(pespconn, pbuf, os_strlen(pbuf));
#else
        espconn_sent(pespconn, pbuf, os_strlen(pbuf));
#endif

        if (pbuf != NULL) {
            os_free(pbuf);
            pbuf = NULL;
        }
    }

    os_free(server->url);
    server->url = NULL;
    os_free(server);
    server = NULL;
}

/******************************************************************************
 * FunctionName : user_esp_platform_upgrade_begin
 * Description  : Processing the received data from the server
 * Parameters   : pespconn -- the espconn used to connetion with the host
 *                server -- upgrade param
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_upgrade_begin(struct espconn *pespconn, struct upgrade_server_info *server)
{
    uint8 user_bin[9] = {0};

    server->pespconn = pespconn;

    os_memcpy(server->ip, pespconn->proto.tcp->remote_ip, 4);

#ifdef UPGRADE_SSL_ENABLE
    server->port = 443;
#else
    server->port = 80;
#endif

    server->check_cb = user_esp_platform_upgrade_rsp;
    server->check_times = 120000;

    if (server->url == NULL) {
        server->url = (uint8 *)os_zalloc(512);
    }

    if (system_upgrade_userbin_check() == UPGRADE_FW_BIN1) {
        os_memcpy(user_bin, "user2.bin", 10);
    } else if (system_upgrade_userbin_check() == UPGRADE_FW_BIN2) {
        os_memcpy(user_bin, "user1.bin", 10);
    }

    ESP_DBG("%s\n",server->url);

#ifdef UPGRADE_SSL_ENABLE

    if (system_upgrade_start_ssl(server) == false) {
#else

    if (system_upgrade_start(server) == false) {
#endif
        ESP_DBG("upgrade is already started\n");
    }
}

/******************************************************************************
 * FunctionName : user_esp_platform_recv_cb
 * Description  : Processing the received data from the server
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                pusrdata -- The received data (or NULL when the connection has been closed!)
 *                length -- The length of received data
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_recv_cb(void *arg, char *pusrdata, unsigned short length)
{
	uint8_t sendFlag = 0;
	char *pParseHttpBody = NULL;
	char *pParseHttpHead = NULL;
	struct jsontree_context js;
    struct espconn *pespconn = arg;

    ESP_DBG("user_esp_platform_recv_cb %s\n", pusrdata);

    os_timer_disarm(&beacon_timer);

	os_printf("len:%u\n",length);

	os_memset(pSendBuf, 0, packet_size);
	os_memset(pHttpBody, 0, httpBody_size);
	
	/* 校验数据长度 得到json数据长度dat_sumlength */
    if(http_check_data(pusrdata, length) == false)
    {
        os_printf("rx error length\r\n");
    }
	else
	{		
		/*******************************************
		* 这边分为两部分
		* 1.本机设备发数据给服务器后，服务器的应答数据
		*		由""HTTP/1.1"开头的数据。
		* 2.服务器主动发数据给本机设备
		*		有"POST/GET"开头的数据。
		********************************************/
		if (0 == os_memcmp(pusrdata, "POST", 4))
		{
			pParseHttpBody = (char *)os_strstr(pusrdata, "\r\n\r\n");
			if (pParseHttpBody == NULL) 
			{
	            ESP_DBG("rx rc req error data\r\n");
	        }
			else
			{
	            pParseHttpBody += 4;	// 指向http body 的第一个字节
			}
			
			if (NULL != pParseHttpBody)
			{	
				jsontree_setup(&js, (struct jsontree_value *)&RcReqTree, json_putchar);
				json_parse(&js, pParseHttpBody);
				if (esp_param.rc_flag)
				{					
					esp_param.rc_flag = 0;
					json_ws_send((struct jsontree_value *)&RcResTree, "rcResponse", pHttpBody);
					http_response_data_fill(pHttpBody, pSendBuf, 1);
				}
				else
				{
					http_response_data_fill(NULL, pSendBuf, 0);
				}
				sendFlag = 1;
			}
		}
		else if (0 == os_memcmp(pusrdata, "GET", 3))
		{
			URL_Frame urlFrameTemp;
			http_parse_request_url(pusrdata, &urlFrameTemp);
			ESP_DBG("s=%s\r\n", urlFrameTemp.pSelect);
			ESP_DBG("c=%s\r\n", urlFrameTemp.pCommand);
			ESP_DBG("f=%s\r\n", urlFrameTemp.pFilename);
			if (0 == os_memcmp(urlFrameTemp.pSelect, "switch", 6))
			{
				if (0 == os_memcmp(urlFrameTemp.pCommand, "cmd", 3))
				{
					if (0 == os_memcmp(urlFrameTemp.pFilename, "status", 6))
					{						
						json_ws_send((struct jsontree_value *)&RcResTree, "rcResponse", pHttpBody);
						http_response_data_fill(pHttpBody, pSendBuf, 1);
					}
					else
					{
						http_response_data_fill(NULL, pSendBuf, 0);
					}
					sendFlag = 1;
				}
			}
		}
		else if (0 == os_memcmp(pusrdata, "HTTP", 4))	
		{
			if(NULL != (char *)os_strstr(pusrdata, "200 OK"))
			{
				pParseHttpBody = (char *)os_strstr(pusrdata, "\r\n\r\n");
				if (pParseHttpBody == NULL) 
				{
		            ESP_DBG("rx reg/hb res error data\r\n");
		        }
				else
				{
		            pParseHttpBody += 4;	// 指向http body 的第一个字节
				}
				
				if (NULL != pParseHttpBody)
				{	
					if (esp_param.reg_ack_flag)
					{
						esp_param.reg_ack_flag = 0;
						jsontree_setup(&js, (struct jsontree_value *)&RegResTree, json_putchar);
						json_parse(&js, pParseHttpBody);
					}					
					else if (esp_param.hb_ack_flag)
					{
						esp_param.hb_ack_flag = 0;	
						jsontree_setup(&js, (struct jsontree_value *)&HBResTree, json_putchar);
						json_parse(&js, pParseHttpBody);
					}
				}
			}
			else
			{
				// 发送数据有错，失败的应答
				if (esp_param.reg_ack_flag)
				{
					ESP_DBG("REG HTTP RES ERROR\r\n");
				}					
				else if (esp_param.hb_ack_flag)
				{
					ESP_DBG("HB HTTP RES ERROR\r\n");
				}
			}			
		}
		else
		{
			ESP_DBG("not need\n\r");
		}
	}

	if (sendFlag)
	{
	#ifdef SERVER_SSL_ENABLE
        espconn_secure_sent(pespconn, pbuf, length);
	#else
        espconn_sent(pespconn, pSendBuf, os_strlen(pSendBuf));
	#endif	
	    ESP_DBG("%s\n", pSendBuf);
	}
	
	os_timer_arm(&beacon_timer, BEACON_TIME, 0);
	esp_param.ping_status = 1;
}


LOCAL bool ICACHE_FLASH_ATTR
user_esp_platform_reset_mode(void)
{
	/********************************* 
	* 三种情况调用此函数 
	* 1.连不上路由器
	* 2.TCP连接超时(5次)
	* 3.域名解析失败(5次)
	*********************************/
    if (wifi_get_opmode() == STATION_MODE) 
	{
		/* 连接不上后台切换到初始模式 */
        wifi_set_opmode(STATIONAP_MODE);
    }

    return false;
}

/******************************************************************************
 * FunctionName : user_esp_platform_recon_cb
 * Description  : The connection had an error and is already deallocated.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_recon_cb(void *arg, sint8 err)
{
    struct espconn *pespconn = (struct espconn *)arg;

    ESP_DBG("user_esp_platform_recon_cb\n");

    os_timer_disarm(&beacon_timer);

#if (PLUG_DEVICE)
    user_link_led_output(1);
#endif

    if (++esp_param.device_recon_count == 5) 
	{
		/* TCP连接超时 */
        esp_param.device_status = DEVICE_CONNECT_SERVER_FAIL;

        if (user_esp_platform_reset_mode()) {
            return;
        }
    }

    os_timer_disarm(&client_timer);
    os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_reconnect, pespconn);
    os_timer_arm(&client_timer, 1000, 0);
}

/******************************************************************************
 * FunctionName : user_esp_platform_connect_cb
 * Description  : A new incoming connection has been connected.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_connect_cb(void *arg)
{
    struct espconn *pespconn = arg;

    ESP_DBG("user_esp_platform_connect_cb\n");
    if (wifi_get_opmode() ==  STATIONAP_MODE ) 
	{
        wifi_set_opmode(STATION_MODE);
    }

#if (PLUG_DEVICE)
	/* 链接上后台后，灯停止闪烁 */
    user_link_led_timer_done();
#endif
	if (TRUE == user_esp_platform_is_register())
	{
		esp_param.ping_status = 1;	// 每50s发一次心跳包		
		os_timer_arm(&beacon_timer, BEACON_TIME, 0);
	}
    esp_param.device_recon_count = 0;
    espconn_regist_recvcb(pespconn, user_esp_platform_recv_cb);
    espconn_regist_sentcb(pespconn, user_esp_platform_sent_cb);
    user_esp_platform_sent(pespconn);
}

/******************************************************************************
 * FunctionName : user_esp_platform_connect
 * Description  : The function given as the connect with the host
 * Parameters   : espconn -- the espconn used to connect the connection
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_connect(struct espconn *pespconn)
{
    ESP_DBG("user_esp_platform_connect\n");

#ifdef CLIENT_SSL_ENABLE
    espconn_secure_connect(pespconn);
#else
    espconn_connect(pespconn);
#endif
}

#ifdef USE_DNS
/******************************************************************************
 * FunctionName : user_esp_platform_dns_found
 * Description  : dns found callback
 * Parameters   : name -- pointer to the name that was looked up.
 *                ipaddr -- pointer to an ip_addr_t containing the IP address of
 *                the hostname, or NULL if the name could not be found (or on any
 *                other error).
 *                callback_arg -- a user-specified callback argument passed to
 *                dns_gethostbyname
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_dns_found(const char *name, ip_addr_t *ipaddr, void *arg)
{
    struct espconn *pespconn = (struct espconn *)arg;

    if (ipaddr == NULL) 
	{
        ESP_DBG("user_esp_platform_dns_found NULL\n");

        if (++esp_param.device_recon_count == 5) 
		{
			/* 域名解析超时五秒后 */	
            esp_param.device_status = DEVICE_CONNECT_SERVER_FAIL;

            user_esp_platform_reset_mode();
        }

        return;
    }

    ESP_DBG("user_esp_platform_dns_found %d.%d.%d.%d\n",
            *((uint8 *)&ipaddr->addr), *((uint8 *)&ipaddr->addr + 1),
            *((uint8 *)&ipaddr->addr + 2), *((uint8 *)&ipaddr->addr + 3));

    if (esp_server_ip.addr == 0 && ipaddr->addr != 0) 
	{
        os_timer_disarm(&client_timer);		// 停止重新域名解析的定时器
        esp_server_ip.addr = ipaddr->addr;	// 得到解析后的IP地址
        os_memcpy(pespconn->proto.tcp->remote_ip, &ipaddr->addr, 4);

        pespconn->proto.tcp->local_port = espconn_port();
#ifdef CLIENT_SSL_ENABLE
        pespconn->proto.tcp->remote_port = 8443;
#else
        pespconn->proto.tcp->remote_port = 8000;
#endif

		/* 作为客户端连接后台服务器 */
		/* 注册 TCP 连接成功建立后的回调函数 */
        espconn_regist_connectcb(pespconn, user_esp_platform_connect_cb);
        espconn_regist_disconcb(pespconn, user_esp_platform_discon_cb); // TCP正常断开，再踹一次包 
        espconn_regist_reconcb(pespconn, user_esp_platform_recon_cb);	// TCP异常断开，会重新踹五次包
        user_esp_platform_connect(pespconn);
    }
}

/******************************************************************************
 * FunctionName : user_esp_platform_dns_check_cb
 * Description  : 1s time callback to check dns found
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_dns_check_cb(void *arg)
{
    struct espconn *pespconn = arg;

    ESP_DBG("user_esp_platform_dns_check_cb\n");

    espconn_gethostbyname(pespconn, ESP_DOMAIN, &esp_server_ip, user_esp_platform_dns_found);

    os_timer_arm(&client_timer, 1000, 0);
}

LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_start_dns(struct espconn *pespconn)
{
    esp_server_ip.addr = 0;
    espconn_gethostbyname(pespconn, ESP_DOMAIN, &esp_server_ip, user_esp_platform_dns_found);

    os_timer_disarm(&client_timer);
    os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_dns_check_cb, pespconn);
    os_timer_arm(&client_timer, 1000, 0);
}
#endif


/******************************************************************************
 * FunctionName : user_esp_platform_check_ip
 * Description  : espconn struct parame init when get ip addr
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_esp_platform_check_ip(uint8 reset_flag)
{
    struct ip_info ipconfig;

    os_timer_disarm(&client_timer);

    wifi_get_ip_info(STATION_IF, &ipconfig);

    if (wifi_station_get_connect_status() == STATION_GOT_IP && ipconfig.ip.addr != 0) 
	{
		/* 连接上了路由器 */
#if (PLUG_DEVICE)
		/* 初始化TCP连接指示灯的定时器 */
        user_link_led_timer_init();
#endif

//***************************
        user_conn.proto.tcp = &user_tcp;
        user_conn.type = ESPCONN_TCP;
        user_conn.state = ESPCONN_NONE;

        esp_param.device_status = DEVICE_CONNECTING;

        if (reset_flag) 
		{
			/* 上电第一次连接设置为0，重新连接则++ */
            esp_param.device_recon_count = 0;
        }
		/* 初始化心跳包定时器 */
        os_timer_disarm(&beacon_timer);
        os_timer_setfn(&beacon_timer, (os_timer_func_t *)user_esp_platform_sent_beacon, &user_conn);

#ifdef USE_DNS
        user_esp_platform_start_dns(&user_conn);
#else
   //     const char esp_server_ip[4] = {114, 215, 177, 97};
        const char esp_server_ip[4] = {192, 168, 137, 1};

        os_memcpy(user_conn.proto.tcp->remote_ip, esp_server_ip, 4);
        user_conn.proto.tcp->local_port = espconn_port();

	#ifdef CLIENT_SSL_ENABLE
        user_conn.proto.tcp->remote_port = 8443;
	#else
        user_conn.proto.tcp->remote_port = 8000;
	#endif
		/* 注册 TCP 连接成功建立后的回调函数 */
        espconn_regist_connectcb(&user_conn, user_esp_platform_connect_cb);
    	espconn_regist_disconcb(&user_conn, user_esp_platform_discon_cb); // TCP正常断开，再踹一次包 
        espconn_regist_reconcb(&user_conn, user_esp_platform_recon_cb);
        user_esp_platform_connect(&user_conn);
#endif
    }
	else 
	{
        /* if there are wrong while connecting to some AP, then reset mode */
        if ((wifi_station_get_connect_status() == STATION_WRONG_PASSWORD ||
                wifi_station_get_connect_status() == STATION_NO_AP_FOUND ||
                wifi_station_get_connect_status() == STATION_CONNECT_FAIL)) 
        {
            user_esp_platform_reset_mode();
        } 
		else 
		{
            os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_check_ip, NULL);
            os_timer_arm(&client_timer, 100, 0);
        }
    }
}

/******************************************************************************
 * FunctionName : user_esp_platform_init
 * Description  : device parame init based on espressif platform
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_esp_platform_init(void)
{
	os_memset(&esp_param, 0, sizeof(esp_param));
	/* 读取系统参数 */
	system_param_load(ESP_PARAM_START_SEC, 0, &esp_system_param, sizeof(esp_system_param));
	if (esp_system_param.flag != 0xabcd)
	{
		/* 第一次上电 系统参数初始化 */
		esp_system_param.flag = 0xabcd;
		esp_system_param.activeflag = 0;
		esp_system_param.BID = 0;
		os_memcpy(esp_system_param.devkey, "12345678", 8);
		os_memcpy(esp_system_param.deviceID, "0123456789000000", 16);
		os_printf("BID=%d\r\n", esp_system_param.BID);
		system_param_save_with_protect(ESP_PARAM_START_SEC, &esp_system_param, sizeof(esp_system_param));
	}
    if (esp_system_param.activeflag != 1) 
	{
		/* 启动默认的STA-AP模式(出厂设置) */			
        wifi_set_opmode(STATIONAP_MODE);
    }
	else
	{
		/* 设备处在sta模式(已经注册过) */
	}
    user_plug_init();

    if (wifi_get_opmode() != SOFTAP_MODE) 
	{
		/* 100ms后，启动网络连接状态检测  */
        os_timer_disarm(&client_timer);
        os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_check_ip, 1);
        os_timer_arm(&client_timer, 100, 0);
    }
}

#endif
