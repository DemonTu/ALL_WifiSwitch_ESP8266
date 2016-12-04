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

LOCAL uint8 ping_status;
LOCAL uint8 device_status;
LOCAL uint8 device_recon_count = 0;
LOCAL uint8 iot_version[20] = {0};

LOCAL os_timer_t beacon_timer;
LOCAL os_timer_t client_timer;

LOCAL struct espconn user_conn;
LOCAL struct _esp_tcp user_tcp;

struct esp_platform_saved_param esp_param;


/******************************************************************************/
void user_esp_platform_check_ip(uint8 reset_flag);


/******************************************************************************/
// ע��json�ṹ
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
    char string[32];

    if (os_strncmp(path, "cmd", 3) == 0) 
	{
        os_sprintf(string, "REG");
    } 
	else if (os_strncmp(path, "key", 3) == 0) 
    {
    	// �д��޸�
        os_sprintf(string, "%s", "12345678");
    } 
	else if (os_strncmp(path, "deviceID", 8) == 0) 
    {
    	// �д��޸�
    	os_sprintf(string, "%s", "0123456789000000");
    }

    jsontree_write_string(js_ctx, string);

    return 0;
}

LOCAL struct jsontree_callback reg_callback =
    JSONTREE_CALLBACK(regFillData, NULL);

JSONTREE_OBJECT(reg_request_tree,
                JSONTREE_PAIR("cmd", 	  &reg_callback),
                JSONTREE_PAIR("key",      &reg_callback),
                JSONTREE_PAIR("deviceID", &reg_callback),
                );
JSONTREE_OBJECT(reg_tree,
                JSONTREE_PAIR("Request", &reg_request_tree));

JSONTREE_OBJECT(RegTree,
                JSONTREE_PAIR("reg", &reg_tree));


/******************************************************************************/
//������Json�ṹ
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
    char string[32];

    if (os_strncmp(path, "cmd", 3) == 0) 
	{
        os_sprintf(string, "PING");
    } 
	else if (os_strncmp(path, "key", 3) == 0) 
    {
    	// �д��޸�
        os_sprintf(string, "%s", "12345678");
    } 
	else if (os_strncmp(path, "deviceID", 8) == 0) 
    {
    	// �д��޸�
    	os_sprintf(string, "%s", "0123456789000000");
    }
	else if (os_strncmp(path, "BID") == 0)
	{
		 jsontree_write_int(js_ctx, 0x55aa); //esp_param.BID);
		 return 0;
	}

    jsontree_write_string(js_ctx, string);

    return 0;
}

LOCAL struct jsontree_callback hb_callback =
    JSONTREE_CALLBACK(hbFillData, NULL);

JSONTREE_OBJECT(hb_request_tree,
                JSONTREE_PAIR("cmd", 	  &hb_callback),
                JSONTREE_PAIR("key",      &hb_callback),
                JSONTREE_PAIR("deviceID", &hb_callback),
				JSONTREE_PAIR("BID", 	  &hb_callback),
                );
JSONTREE_OBJECT(hb_tree,
                JSONTREE_PAIR("Request", &hb_request_tree));

JSONTREE_OBJECT(HBTree,
                JSONTREE_PAIR("hb", &hb_tree));

/******************************************************************************/
//Զ�̿��ƿ���
#if PLUG_DEVICE
/******************************************************************************
 * FunctionName : status_get
 * Description  : set up the device status as a JSON format
 * Parameters   : js_ctx -- A pointer to a JSON set up
 * Returns      : result
*******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR
rcFillData(struct jsontree_context *js_ctx)
{
    const char *path = jsontree_path_name(js_ctx, js_ctx->depth - 1);
    char string[32];

    if (os_strncmp(path, "cmd", 3) == 0) 
	{
        os_sprintf(string, "RCACK");
    } 
	else if (os_strncmp(path, "key", 3) == 0) 
    {
    	// �д��޸�
        os_sprintf(string, "%s", "12345678");
    } 
	else if (os_strncmp(path, "deviceID", 8) == 0) 
    {
    	// �д��޸�
    	os_sprintf(string, "%s", "0123456789000000");
    }
	else if (os_strncmp(path, "status", 8) == 0) 
	{
        jsontree_write_int(js_ctx, user_plug_get_status());	
	}
	else if (os_strncmp(path, "BID") == 0)
	{
		 jsontree_write_int(js_ctx, esp_param.BID);
		 return 0;
	}

    jsontree_write_string(js_ctx, string);

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
						errorFlag++;
					}
	            }
				else if (jsonparse_strcmp_value(parser, "key") == 0)
				{
					jsonparse_next(parser);
	                jsonparse_next(parser);
					jsonparse_copy_value(parser, buffer, sizeof(buffer));
					if (os_memcmp(buffer, esp_param.devkey, 
										sizeof(esp_param.devkey)))
					{
						errorFlag++;
					}
				}
				else if (jsonparse_strcmp_value(parser, "deviceID") == 0)
				{
					jsonparse_next(parser);
	                jsonparse_next(parser);
					jsonparse_copy_value(parser, buffer, sizeof(buffer));
					if (os_memcmp(buffer, esp_param.deviceID, 
										sizeof(esp_param.deviceID)))
					{
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
					}
				}
				else
				{
					// ������
				}
				ESP_DBG("%s\n", buffer);
			}
			else
			{
				// ������
			}
        }
    }

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
	return esp_param.BID;
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
	esp_param.BID = BID;
    system_param_save_with_protect(ESP_PARAM_START_SEC, &esp_param, sizeof(esp_param));
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
    esp_param.activeflag = activeflag;

    system_param_save_with_protect(ESP_PARAM_START_SEC, &esp_param, sizeof(esp_param));
}

void ICACHE_FLASH_ATTR
user_esp_platform_set_connect_status(uint8 status)
{
    device_status = status;
}

/******************************************************************************
 * FunctionName : user_esp_platform_get_connect_status
 * Description  : get each connection step's status
 * Parameters   : none
 * Returns      : status
*******************************************************************************/
uint8 ICACHE_FLASH_ATTR
user_esp_platform_get_connect_status(void)
{
    uint8 status = wifi_station_get_connect_status();

    if (status == STATION_GOT_IP) 
	{
        status = (device_status == 0) ? DEVICE_CONNECTING : device_status;
    }

    ESP_DBG("status %d\n", status);
    return status;
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
    char *pbuf = (char *)os_zalloc(packet_size);
	char *jsonbuf = (char *)os_zalloc(httpBody_size); 
	
    if ((pbuf!=NULL) && (jsonbuf!=NULL)) 
	{
        if (esp_param.activeflag != 1) // ���޸�
		{
            json_ws_send((struct jsontree_value *)&RegTree, "reg", jsonbuf);
			http_request_data_fill(jsonbuf, pbuf, POST);
		}
        else 
		{
            // �Ѿ�ע�����
            os_free(jsonbuf);
            os_free(pbuf);
            return;
        }

        ESP_DBG("%s\n", pbuf);

#ifdef CLIENT_SSL_ENABLE
        espconn_secure_sent(pespconn, pbuf, os_strlen(pbuf));
#else
        espconn_sent(pespconn, pbuf, os_strlen(pbuf));
#endif

		os_free(jsonbuf);
        os_free(pbuf);
    }
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
        if (esp_param.BID != 0x55aa) 
		{
            ESP_DBG("please check device is activated.\n");
			/* δע�� ��������ע�� */
            user_esp_platform_sent(pespconn);
        }
		else 
		{
            if (ping_status == 0) 
			{
                ESP_DBG("user_esp_platform_sent_beacon sent fail!\n");
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
						json_ws_send((struct jsontree_value *)&HBTree, "hb", pHttpBody);
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
                    ping_status = 0;
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
        ESP_DBG("user_esp_platform_sent_beacon sent fail!\n");
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
    char *pstr = NULL;
    LOCAL char pbuffer[1024 * 2] = {0};
    struct espconn *pespconn = arg;

    ESP_DBG("user_esp_platform_recv_cb %s\n", pusrdata);

    os_timer_disarm(&beacon_timer);

    if (length == 1460) 
	{
        os_memcpy(pbuffer, pusrdata, length);
    } 
	else 
	{
		 ping_status = 1;
	}

    os_timer_arm(&beacon_timer, BEACON_TIME, 0);
}


LOCAL bool ICACHE_FLASH_ATTR
user_esp_platform_reset_mode(void)
{
	/********************************* 
	* ����������ô˺��� 
	* 1.������·����
	* 2.TCP���ӳ�ʱ(5��)
	* 3.��������ʧ��(5��)
	*********************************/
    if (wifi_get_opmode() == STATION_MODE) 
	{
		/* ���Ӳ��Ϻ�̨�л�����ʼģʽ */
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

    if (++device_recon_count == 5) 
	{
		/* TCP���ӳ�ʱ */
        device_status = DEVICE_CONNECT_SERVER_FAIL;

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
	/* �����Ϻ�̨�󣬵�ֹͣ��˸ */
    user_link_led_timer_done();
#endif
    device_recon_count = 0;
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

    if (ipaddr == NULL) {
        ESP_DBG("user_esp_platform_dns_found NULL\n");

        if (++device_recon_count == 5) 
		{
			/* ����������ʱ����� */	
            device_status = DEVICE_CONNECT_SERVER_FAIL;

            user_esp_platform_reset_mode();
        }

        return;
    }

    ESP_DBG("user_esp_platform_dns_found %d.%d.%d.%d\n",
            *((uint8 *)&ipaddr->addr), *((uint8 *)&ipaddr->addr + 1),
            *((uint8 *)&ipaddr->addr + 2), *((uint8 *)&ipaddr->addr + 3));

    if (esp_server_ip.addr == 0 && ipaddr->addr != 0) 
	{
        os_timer_disarm(&client_timer);		// ֹͣ�������������Ķ�ʱ��
        esp_server_ip.addr = ipaddr->addr;	// �õ��������IP��ַ
        os_memcpy(pespconn->proto.tcp->remote_ip, &ipaddr->addr, 4);

        pespconn->proto.tcp->local_port = espconn_port();
#ifdef CLIENT_SSL_ENABLE
        pespconn->proto.tcp->remote_port = 8443;
#else
        pespconn->proto.tcp->remote_port = 8000;
#endif
        ping_status = 1;	// �������̨�����������ı�־

		/* ��Ϊ�ͻ������Ӻ�̨������ */
		/* ע�� TCP ���ӳɹ�������Ļص����� */
        espconn_regist_connectcb(pespconn, user_esp_platform_connect_cb);
        espconn_regist_disconcb(pespconn, user_esp_platform_discon_cb); // TCP�����Ͽ�������һ�ΰ� 
        espconn_regist_reconcb(pespconn, user_esp_platform_recon_cb);	// TCP�쳣�Ͽ�������������ΰ�
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
		/* ��������·���� */
#if (PLUG_DEVICE)
		/* ��ʼ��TCP����ָʾ�ƵĶ�ʱ�� */
        user_link_led_timer_init();
#endif

//***************************
        user_conn.proto.tcp = &user_tcp;
        user_conn.type = ESPCONN_TCP;
        user_conn.state = ESPCONN_NONE;

        device_status = DEVICE_CONNECTING;

        if (reset_flag) 
		{
			/* �ϵ��һ����������Ϊ0������������++ */
            device_recon_count = 0;
        }
		/* ��ʼ����������ʱ�� */
        os_timer_disarm(&beacon_timer);
        os_timer_setfn(&beacon_timer, (os_timer_func_t *)user_esp_platform_sent_beacon, &user_conn);

#ifdef USE_DNS
        user_esp_platform_start_dns(&user_conn);
#else
        const char esp_server_ip[4] = {114, 215, 177, 97};

        os_memcpy(user_conn.proto.tcp->remote_ip, esp_server_ip, 4);
        user_conn.proto.tcp->local_port = espconn_port();

	#ifdef CLIENT_SSL_ENABLE
        user_conn.proto.tcp->remote_port = 8443;
	#else
        user_conn.proto.tcp->remote_port = 8000;
	#endif
		/* ע�� TCP ���ӳɹ�������Ļص����� */
        espconn_regist_connectcb(&user_conn, user_esp_platform_connect_cb);
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
	os_sprintf(iot_version,"%s%d.%d.%dt%d(%s)",VERSION_TYPE,IOT_VERSION_MAJOR,\
	IOT_VERSION_MINOR,IOT_VERSION_REVISION,device_type,UPGRADE_FALG);
	os_printf("IOT VERSION = %s\n",iot_version);

	/* ��ȡϵͳ���� */
	system_param_load(ESP_PARAM_START_SEC, 0, &esp_param, sizeof(esp_param));
	if (esp_param.flag != 0xabcd)
	{
		/* ��һ���ϵ� ϵͳ������ʼ�� */
		esp_param.flag = 0xabcd;
		esp_param.activeflag = 0;
		esp_param.BID = 0x55aa;
		os_memcpy(esp_param.devkey, "12365489", 8);
		os_printf("BID=%d\r\n", esp_param.BID);
		system_param_save_with_protect(ESP_PARAM_START_SEC, &esp_param, sizeof(esp_param));
	}
    if (esp_param.activeflag != 1) 
	{
		/* ����Ĭ�ϵ�STA-APģʽ(��������) */			
        wifi_set_opmode(STATIONAP_MODE);
    }
	else
	{
		/* �豸����staģʽ(�Ѿ�ע���) */
	}
    user_plug_init();

    if (wifi_get_opmode() != SOFTAP_MODE) 
	{
		/* 100ms��������������״̬���  */
        os_timer_disarm(&client_timer);
        os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_check_ip, 1);
        os_timer_arm(&client_timer, 100, 0);
    }
}

#endif
