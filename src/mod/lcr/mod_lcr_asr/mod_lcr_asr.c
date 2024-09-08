#include <switch.h>
#include <libwebsockets.h>

#define MAX_PAYLOAD_SIZE 10 * 1024
typedef enum {
    YY_FLAG_HAS_TEXT = (1 << 0),
    YY_FLAG_WS_READY = (1 << 1),
    YY_FLAG_WS_CLOSE = (1 << 2),
} lcr_asr_flag_t;

typedef struct lcr_lws {
    struct lws *g_wsi;
    // 客户端连接参数
    struct lws_client_connect_info conn_info;
} lcr_lws_t;

typedef struct lcr_pr {
    uint32_t flags;
    // 设置flag时加锁
    switch_mutex_t *flag_mutex;

    // ws响应的文字
    switch_buffer_t *text_buffer;

    switch_buffer_t *audio_buffer;
    int msg_count;
} lcr_pr_t;

typedef struct lcr_asr {
    lcr_lws_t lws_obj;
    lcr_pr_t pr_obj;
} lcr_asr_t;

char lws_pre_arr[LWS_PRE]={0};

// 用于创建vhost或者context的参数
struct lws_context_creation_info ctx_info = {0};
struct lws_context *lws_context_obj;

SWITCH_DECLARE(switch_status_t) get_ws_rx_text(switch_buffer_t *buffer ,char *in);
static char *get_switch_buffer_ptr(switch_buffer_t *buffer);
int ws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_lcr_asr_shutdown);
SWITCH_MODULE_LOAD_FUNCTION(mod_lcr_asr_load);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_lcr_asr_runtime);
SWITCH_MODULE_DEFINITION(mod_lcr_asr, mod_lcr_asr_load, mod_lcr_asr_shutdown, mod_lcr_asr_runtime);

/**
 * 支持的WebSocket子协议数组
 * 子协议即JavaScript客户端WebSocket(url, protocols)第2参数数组的元素
 * 你需要为每种协议提供回调函数
 */
struct lws_protocols protocols[] = {
        {"lws-minimal-client",ws_callback,sizeof(lcr_pr_t),MAX_PAYLOAD_SIZE,},
        {NULL, NULL, 0 }  /* 结束标识 */
};

/**
 * 某个协议下的连接发生事件时，执行的回调函数
 *
 * wsi：指向WebSocket实例的指针
 * reason：导致回调的事件
 * user 库为每个WebSocket会话分配的内存空间
 * in 某些事件使用此参数，作为传入数据的指针
 * len 某些事件使用此参数，说明传入数据的长度
 */
int ws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    lcr_pr_t *asr_obj = (lcr_pr_t *)user;
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED: // 连接到服务器后的回调
            lws_callback_on_writable(wsi);
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connected to server ok!\n");
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE: // 接收到服务器数据后的回调，数据为in，其长度为len
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Rx: %s\n", (char *)in);
            if (get_ws_rx_text(asr_obj->text_buffer,(char *)in) == SWITCH_STATUS_SUCCESS){
                switch_set_flag_locked(asr_obj, YY_FLAG_HAS_TEXT);
            }
            break;
        case LWS_CALLBACK_CLIENT_WRITEABLE: // 当此客户端可以发送数据时的回调
            if (asr_obj->msg_count < 1) {
            	char tmp_msg[] = "{\"chunk_size\":[5,10,5],\"wav_name\":\"h5\",\"is_speaking\":true,\"chunk_interval\":10,\"itn\":false,\"mode\":\"2pass\",\"hotwords\":\"{\\\"阿里巴巴\\\":20,\\\"hello world\\\":40}\"}";
            	// 前面LWS_PRE个字节必须留给LWS
            	char all_msg[LWS_PRE + strlen(tmp_msg)];
            	int pre_len = sprintf(&all_msg[LWS_PRE], "%s", tmp_msg);

                switch_set_flag_locked(asr_obj, YY_FLAG_WS_READY);
                asr_obj->msg_count++;

            	// 通过WebSocket发送文本消息
            	lws_write(wsi, (unsigned char*)&all_msg[LWS_PRE], pre_len, LWS_WRITE_TEXT);
            }
            if ( switch_test_flag(asr_obj, YY_FLAG_WS_CLOSE) ) {
//            	char close_msg[] = "{\"chunk_size\":[5,10,5],\"wav_name\":\"h5\",\"is_speaking\":false,\"chunk_interval\":10,\"mode\":\"2pass\"}";
//            	lws_close_reason(wsi, LWS_CLOSE_STATUS_NORMAL,(unsigned char *)close_msg, strlen(close_msg));
                return -1;
            }
//		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "ws_callback LWS_CALLBACK_CLIENT_WRITEABLE Tx: %s\n", msg);
            break;
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "LWS_CALLBACK_CLIENT_CONNECTION_ERROR: %s\n",  in ? (char *)in : "(null)");
            break;
        case LWS_CALLBACK_CLIENT_CLOSED:
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "LWS_CALLBACK_CLIENT_CLOSED\n");
            break;
        default:
            break;
    }
    return lws_callback_http_dummy(wsi, reason, user, in, len);
}

SWITCH_DECLARE(switch_status_t) get_ws_rx_text(switch_buffer_t *buffer ,char *in){
    /* for JSON result parse*/
    cJSON *json_result = NULL, *cursor = NULL;
    if ( buffer == NULL ) {
        return SWITCH_STATUS_FALSE;
    }
    // 清空text_buffer之前的值
    switch_buffer_zero(buffer);
    json_result = cJSON_Parse(in);
    if (json_result == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Rx: cJSON_Parse fail");
        return SWITCH_STATUS_FALSE;
    }
    cursor = cJSON_GetObjectItem(json_result, "text");
    if (cursor == NULL){
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Rx: cJSON_GetObjectItem fail");
        return SWITCH_STATUS_FALSE;
    }
    switch_buffer_write(buffer, cursor->valuestring, strlen(cursor->valuestring));
    cJSON_Delete(json_result);
    return SWITCH_STATUS_SUCCESS;
}

void lws_create_ctx( void )
{
	memset(lws_pre_arr, 1, sizeof(lws_pre_arr));
    ctx_info.port = CONTEXT_PORT_NO_LISTEN;
    ctx_info.iface = NULL;
    ctx_info.protocols = protocols;
    ctx_info.gid = -1;
    ctx_info.uid = -1;
    lws_context_obj = lws_create_context(&ctx_info);
}

void connect_ws(switch_asr_handle_t *ah)
{

    lcr_asr_t *asr_obj = (lcr_asr_t *)ah->private_info;
    char address[] = "10.135.18.243";
    int port = 10196;
    //	char address[] = "192.168.31.183";
    //	 int port = 8099;

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "connect_ws %s:%u\n", address, port & 65535);

    asr_obj->lws_obj.conn_info.context = lws_context_obj;
    asr_obj->lws_obj.conn_info.address = address;
    asr_obj->lws_obj.conn_info.port = port;
    asr_obj->lws_obj.conn_info.path = "/";
    asr_obj->lws_obj.conn_info.host = address;
    asr_obj->lws_obj.conn_info.origin = address;
    asr_obj->lws_obj.conn_info.protocol = "ws";
    asr_obj->lws_obj.conn_info.local_protocol_name = protocols[ 0 ].name;
    asr_obj->lws_obj.conn_info.userdata = &asr_obj->pr_obj;
    asr_obj->lws_obj.g_wsi = lws_client_connect_via_info(&asr_obj->lws_obj.conn_info);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "lws_set_opaque_user_data ptr %p\n",
                      (void *)asr_obj->lws_obj.g_wsi);
}

/**
 * get the switch buffer ptr which is safe for std string operation
 */

static char *get_switch_buffer_ptr(switch_buffer_t *buffer)
{
    char *ptr = (char *)switch_buffer_get_head_pointer(buffer);
    ptr[switch_buffer_inuse(buffer)] = '\0';
    return ptr;
}

static switch_status_t asr_open(switch_asr_handle_t *ah, const char *codec, int rate, const char *dest,
                                switch_asr_flag_t *flags)
{
    lcr_asr_t *asr_obj = switch_core_alloc(ah->memory_pool, sizeof(lcr_asr_t));
    if (asr_obj == NULL) {
        return SWITCH_STATUS_MEMERR;
    }
    //初始化锁的一些信息
    asr_obj->pr_obj.flags = 0;
    switch_mutex_init(&asr_obj->pr_obj.flag_mutex, SWITCH_MUTEX_NESTED, ah->memory_pool);
    switch_buffer_create_dynamic(&asr_obj->pr_obj.audio_buffer, 32768, 32768, 2097152);
    switch_buffer_create_dynamic(&asr_obj->pr_obj.text_buffer, 30720, 30720, 2097152);


    ah->private_info = asr_obj;
    //连接ws
    connect_ws(ah);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "asr_open codec:%s ,rate:%d ,dest:%s\n",codec,rate,dest);
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t asr_load_grammar(switch_asr_handle_t *ah, const char *grammar, const char *name)
{
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "asr_load_grammar \n");
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "asr_load_grammar grammar: %s \n", grammar);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "asr_load_grammar name: %s \n", name);
    return SWITCH_STATUS_SUCCESS;
}

/*! function to unload a grammar to the asr interface */
static switch_status_t asr_unload_grammar(switch_asr_handle_t *ah, const char *name)
{
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "asr_unload_grammar \n");
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "asr_unload_grammar name: %s \n", name);
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t asr_close(switch_asr_handle_t *ah, switch_asr_flag_t *flags)
{
    lcr_asr_t *asr_obj = (lcr_asr_t *)ah->private_info;
    if ( asr_obj->pr_obj.audio_buffer ) {
        switch_buffer_destroy(&asr_obj->pr_obj.audio_buffer);
    }

    if ( asr_obj->pr_obj.text_buffer ) {
        switch_buffer_destroy(&asr_obj->pr_obj.text_buffer);
    }
    if (asr_obj->lws_obj.g_wsi && switch_test_flag(&asr_obj->pr_obj, YY_FLAG_WS_READY)){
        char tmp_msg[] = "{\"is_speaking\":false}";
        // 前面LWS_PRE个字节必须留给LWS
        char all_msg[LWS_PRE + strlen(tmp_msg)];
        int pre_len = sprintf(&all_msg[LWS_PRE], "%s", tmp_msg);

        switch_set_flag_locked(&asr_obj->pr_obj, YY_FLAG_WS_CLOSE);
        // 通过WebSocket发送文本消息
        lws_write(asr_obj->lws_obj.g_wsi, (unsigned char*)&all_msg[LWS_PRE], pre_len, LWS_WRITE_TEXT);
    	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "asr_closexxxxxxxxx \n");
    }
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "asr_close \n");
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t asr_feed(switch_asr_handle_t *ah, void *data, unsigned int len, switch_asr_flag_t *flags)
{
    lcr_asr_t *asr_obj = (lcr_asr_t *)ah->private_info;
    int lws_write_res = 0;
    int samples_index;
    int16_t * resamples = data;
    int16_t one_sample;
//    FILE *stream = fopen("/usr/local/freeswitch/conf/lcr.pcm", "ab");

    // check the asr close flag
    if ( switch_test_flag(ah, SWITCH_ASR_FLAG_CLOSED) ) {
        return SWITCH_STATUS_BREAK;
    }
    if ( !switch_test_flag(&asr_obj->pr_obj, YY_FLAG_WS_READY) ) {
        return SWITCH_STATUS_SUCCESS;
    }

    // switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "asr_feed \n");
    switch_buffer_zero(asr_obj->pr_obj.audio_buffer);

    lws_write_res = switch_buffer_write(asr_obj->pr_obj.audio_buffer, lws_pre_arr, sizeof(lws_pre_arr));
//    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "asr_feed lws_write_res: %d audio_buffer_len:%lu\n",  lws_write_res,sizeof(lws_pre_arr));

    /**
     * 码率由48000转为16000
     * 对于48000采样率的单声道音频，每个样本需要2个字节来存储。
     * len/2就是计算采样点,每三个采样点合并成一个
     */
    for(samples_index= 0 ;samples_index < (len/2) ; samples_index += 3){
        one_sample =(resamples[samples_index] + resamples[samples_index+1] + resamples[samples_index+2]) / 3;
        switch_buffer_write(asr_obj->pr_obj.audio_buffer,  &one_sample, sizeof(int16_t));
    }
//    lws_write_res = switch_buffer_write(asr_obj->pr_obj.audio_buffer, data, len);
//    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "asr_feed lws_write_res: %d audio_buffer_len:%d\n",  lws_write_res,len);

	switch_buffer_toss(asr_obj->pr_obj.audio_buffer,sizeof(lws_pre_arr));
//    fwrite( get_switch_buffer_ptr(asr_obj->pr_obj.audio_buffer),
//            switch_buffer_inuse(asr_obj->pr_obj.audio_buffer),1, stream);
    lws_write_res = lws_write(asr_obj->lws_obj.g_wsi, (unsigned char *)switch_buffer_get_head_pointer(asr_obj->pr_obj.audio_buffer),
                              switch_buffer_inuse(asr_obj->pr_obj.audio_buffer), LWS_WRITE_BINARY);

//    fclose(stream);
//    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "asr_feed lws_write_res: %d audio_buffer_len:%d\n",  lws_write_res,len);
    if (lws_write_res < 0) {
        return SWITCH_STATUS_FALSE;
    }
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t asr_resume(switch_asr_handle_t *ah)
{
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "asr_resume \n");
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t asr_pause(switch_asr_handle_t *ah)
{
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "asr_pause \n");
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t asr_check_results(switch_asr_handle_t *ah, switch_asr_flag_t *flags)
{
    lcr_asr_t *asr_obj = (lcr_asr_t *)ah->private_info;
    // switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "check_results\n");
    return switch_test_flag(&asr_obj->pr_obj, YY_FLAG_HAS_TEXT) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}

static switch_status_t asr_get_results(switch_asr_handle_t *ah, char **xmlstr, switch_asr_flag_t *flags)
{

    lcr_asr_t *asr_obj = (lcr_asr_t *)ah->private_info;
    // switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "asr_get_results \n");
    if (switch_test_flag(&asr_obj->pr_obj, YY_FLAG_HAS_TEXT)) {
        *xmlstr = switch_mprintf("%s", get_switch_buffer_ptr(asr_obj->pr_obj.text_buffer));
        switch_clear_flag_locked(&asr_obj->pr_obj, YY_FLAG_HAS_TEXT);
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "asr_get_results xmlstr: %s\n",*xmlstr);
        return SWITCH_STATUS_SUCCESS;
    }
    return SWITCH_STATUS_FALSE;

    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t asr_start_input_timers(switch_asr_handle_t *ah)
{
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "asr_start_input_timers \n");
    return SWITCH_STATUS_SUCCESS;
}

static void asr_text_param(switch_asr_handle_t *ah, char *param, const char *val)
{
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "asr_text_param \n");
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "asr_text_param param: %s \n", param);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "asr_text_param val: %s \n", val);
}

static void asr_numeric_param(switch_asr_handle_t *ah, char *param, int val)
{
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "asr_numeric_param \n");
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "asr_numeric_param param: %s \n", param);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "asr_numeric_param val: %d \n", val);
}

static void asr_float_param(switch_asr_handle_t *ah, char *param, double val)
{
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "asr_numeric_param \n");
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "asr_numeric_param param: %s \n", param);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "asr_numeric_param val: %.3f \n", val);
}

SWITCH_MODULE_LOAD_FUNCTION(mod_lcr_asr_load)
{
	switch_asr_interface_t *asr_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	// 打印版本信息
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "libwebsockets version: %s\n", lws_get_library_version());
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "mod_lcr_asr_load \n");

	lws_create_ctx();

	asr_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ASR_INTERFACE);
	asr_interface->interface_name = "lcr_asr";
	asr_interface->asr_open = asr_open;
	asr_interface->asr_load_grammar = asr_load_grammar;
	asr_interface->asr_unload_grammar = asr_unload_grammar;
	asr_interface->asr_close = asr_close;
	asr_interface->asr_feed = asr_feed;
	asr_interface->asr_resume = asr_resume;
	asr_interface->asr_pause = asr_pause;
	asr_interface->asr_check_results = asr_check_results;
	asr_interface->asr_get_results = asr_get_results;
	asr_interface->asr_start_input_timers = asr_start_input_timers;
	asr_interface->asr_text_param = asr_text_param;
	asr_interface->asr_numeric_param = asr_numeric_param;
	asr_interface->asr_float_param = asr_float_param;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

// Called when the system shuts down
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_lcr_asr_shutdown)
{
	lws_context_destroy(lws_context_obj);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "mod_lcr_asr_shutdown \n");
	return SWITCH_STATUS_UNLOAD;
}

/**
* If it exists, this is called in it's own thread when the module-load completes
* If it returns anything but SWITCH_STATUS_TERM it will be called again automaticly
 */
SWITCH_MODULE_RUNTIME_FUNCTION(mod_lcr_asr_runtime)
{

	while(1)
	{
	    switch_yield(100);
	    lws_service( lws_context_obj, 100 );
	    return SWITCH_STATUS_CONTINUE;
	}
	return SWITCH_STATUS_TERM;
}



