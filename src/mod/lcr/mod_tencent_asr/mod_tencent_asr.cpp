/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Veasion <veasion@qq.com>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Veasion <veasion@qq.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Veasion <veasion@qq.com>
 *
 * mod_tencent_asr.cpp -- Aliyun Interface
 *
 */

#include <switch.h>
#include <fstream>
#include "speech_recognizer.h"
#include <sys/time.h>
#include <curl/curl.h>
#include <cstdlib> // 包含 atoi 的头文件
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>

#define MAX_FRAME_BUFFER_SIZE (1024*1024)
#define SAMPLE_RATE 8000

/* module name */
#define MOD_NAME "tencent_asr"
/* module config file */
#define CONFIG_FILE "tencent_asr.conf"


struct AsrParamCallBack {
    std::string caller;
    std::string callee;
	char *sUUID;
	switch_core_session_t *session;
};


//================= aliyun asr start ===============

typedef struct {

    switch_core_session_t   *session;
    switch_media_bug_t      *bug;
    SpeechRecognizer *request;

    int                     started;
    int                     stoped;
    int                     starting;
    int                     datalen;

    switch_mutex_t          *mutex;
    switch_memory_pool_t *pool;

    switch_audio_resampler_t *resampler;


} switch_da_t;

std::string g_appid = "";
std::string g_secret_id = "";
std::string g_secret_key = "";
std::string g_engine_model_type = "8k_zh";
std::string g_max_sentence_silence = "800";
long g_expireTime = -1;

SpeechRecognizer* generateAsrRequest(AsrParamCallBack * cbParam);

/**
 * 识别启动回调函数
 *
 * @param cbEvent
 * @param cbParam
 */
void OnRecognitionStart(SpeechRecognitionResponse *rsp, void* cbParam) {
    AsrParamCallBack* tmpParam = (AsrParamCallBack*)cbParam;

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "onAsrTranscriptionStarted: %s\n", tmpParam->sUUID);
   switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"OnRecognitionStart: status code=%d, message id=%s\n", rsp->code, rsp->message_id.c_str());

   switch_da_t *pvt;
   switch_core_session_t *ses = tmpParam->session;
   // if ((ses = switch_core_session_force_locate(tmpParam->sUUID))) {
   //     switch_core_session_rwunlock(ses);
   // }
   switch_channel_t *channel = switch_core_session_get_channel(ses);
   if((pvt = (switch_da_t*)switch_channel_get_private(channel, "asr")))
   {
      switch_mutex_lock(pvt->mutex);
      pvt->started = 1;
      pvt->starting = 0;
      switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"I need lock!\n");
      switch_mutex_unlock(pvt->mutex);
   }
}


/**
 * @brief 一句话开始回调函数
 *
 * @param cbEvent
 * @param cbParam
 */
void OnSentenceBegin(SpeechRecognitionResponse *rsp, void* cbParam) {
    AsrParamCallBack* tmpParam = (AsrParamCallBack*)cbParam;
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"OnSentenceBegin: %s\n", tmpParam->sUUID);
//     switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"OnSentenceBegin: status code=%d, task id=%s, index=%d, time=%d\n", cbEvent->getStatusCode(), cbEvent->getTaskId(),
}

/**
 * @brief 一句话结束回调函数
 *
 * @param cbEvent
 * @param cbParam
 */
void OnSentenceEnd(SpeechRecognitionResponse *rsp, void* cbParam) {
    AsrParamCallBack* tmpParam = (AsrParamCallBack*)cbParam;
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"onAsrSentenceEnd: %s\n", tmpParam->sUUID);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"onAsrSentenceEnd: %s\n", rsp->result.voice_text_str.c_str());
//    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"OnSentenceBegin: status code=%d, task id=%s, index=%d, time=%d, begin_time=%d, result=%s\n", cbEvent->getStatusCode(), cbEvent->getTaskId(),
//	   cbEvent->getSentenceIndex(),
//	   cbEvent->getSentenceTime(),
//	   cbEvent->getSentenceBeginTime(),
//	   cbEvent->getResult()
//	);
//    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "onAsrSentenceEnd: all response=%s\n", cbEvent->getAllResponse());
    switch_event_t *event = NULL;
	switch_core_session_t *ses = tmpParam->session;
    // if ((ses = switch_core_session_force_locate(tmpParam->sUUID))) {
	//     switch_core_session_rwunlock(ses);
	// }
    switch_channel_t *channel = switch_core_session_get_channel(ses);
    if(switch_event_create(&event, SWITCH_EVENT_CUSTOM) == SWITCH_STATUS_SUCCESS) {
         event->subclass_name = strdup("start_asr");
         switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Event-Subclass", event->subclass_name);
         switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Caller-Unique-ID", tmpParam->sUUID);
         switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "ASR-Response", rsp->result.voice_text_str.c_str());
         switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Channel-Name", switch_channel_get_name(channel));
         switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "variable_bot_id", switch_channel_get_variable(channel, "bot_id"));
         switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Caller-Caller-ID-Number", switch_channel_get_variable(channel, "caller_id_number"));
         switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Caller-Destination-Number", switch_channel_get_variable(channel, "destination_number"));
         switch_event_fire(&event);
    }
}

/**
 * @brief 识别结果变化回调函数
 *
 * @param cbEvent
 * @param cbParam
 */
void OnRecognitionResultChange(SpeechRecognitionResponse *rsp, void* cbParam) {
	AsrParamCallBack* tmpParam = (AsrParamCallBack*)cbParam;
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"onAsrTranscriptionResultChanged: %s\n", tmpParam->sUUID);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"onAsrTranscriptionResultChanged: %s\n", rsp->result.voice_text_str.c_str());
//     switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"onAsrTranscriptionResultChanged: status code=%d, task id=%s, index=%d, time=%d, result=%s\n", cbEvent->getStatusCode(), cbEvent->getTaskId(),
//                cbEvent->getSentenceIndex(),
//                cbEvent->getSentenceTime(),
//                cbEvent->getResult()
//                );
//    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "onAsrTranscriptionResultChanged: all response=%s\n", cbEvent->getAllResponse());

    switch_event_t *event = NULL;
	switch_core_session_t *ses = tmpParam->session;
    // if ((ses = switch_core_session_force_locate(tmpParam->sUUID))) {
	//    switch_core_session_rwunlock(ses);
	// }
    switch_channel_t *channel = switch_core_session_get_channel(ses);
    if (switch_event_create(&event, SWITCH_EVENT_CUSTOM) == SWITCH_STATUS_SUCCESS) {
        event->subclass_name = strdup("update_asr");
        switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Event-Subclass", event->subclass_name);
        switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Caller-Unique-ID", tmpParam->sUUID);
        switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "ASR-Response", rsp->result.voice_text_str.c_str());
        switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Channel-Name", switch_channel_get_name(channel));
        switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "variable_bot_id", switch_channel_get_variable(channel, "bot_id"));
        switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Caller-Caller-ID-Number", switch_channel_get_variable(channel, "caller_id_number"));
        switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Caller-Destination-Number", switch_channel_get_variable(channel, "destination_number"));
        switch_event_fire(&event);
    }
}

/**
 * @brief 语音转写结束回调函数
 *
 * @param cbEvent
 * @param cbParam
 */
void OnRecognitionComplete(SpeechRecognitionResponse *rsp, void* cbParam) {
	AsrParamCallBack* tmpParam = (AsrParamCallBack*)cbParam;
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"onAsrTranscriptionCompleted: %s\n", tmpParam->sUUID);
//    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"onAsrTranscriptionCompleted: status code=%d, task id=%s\n", cbEvent->getStatusCode(), cbEvent->getTaskId());

    switch_da_t *pvt;
	switch_core_session_t *ses = tmpParam->session;
    // if ((ses = switch_core_session_force_locate(tmpParam->sUUID))) {
	//     switch_core_session_rwunlock(ses);
	// }
    switch_channel_t *channel = switch_core_session_get_channel(ses);
    if((pvt = (switch_da_t*)switch_channel_get_private(channel, "asr")))
    {
       // if(pvt->frameDataBuffer){
       //   free(pvt->frameDataBuffer);
       // }
    }
}

/**
 * @brief 异常识别回调函数
 *
 * @param cbEvent
 * @param cbParam
 */
void OnFail(SpeechRecognitionResponse *rsp, void* cbParam) {
	AsrParamCallBack* tmpParam = (AsrParamCallBack*)cbParam;
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"onAsrTaskFailed: %s\n", tmpParam->sUUID);
//    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"onAsrTaskFailed: status code=%d, task id=%s, error message=%s\n", cbEvent->getStatusCode(), cbEvent->getTaskId(), cbEvent->getErrorMessage());
//    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"onAsrTaskFailed: all response=%s\n", cbEvent->getAllResponse());

    switch_da_t *pvt;
	switch_core_session_t *ses = tmpParam->session;
    // if ((ses = switch_core_session_force_locate(tmpParam->sUUID))) {
	//     switch_core_session_rwunlock(ses);
	// }
    switch_channel_t *channel = switch_core_session_get_channel(ses);
    if((pvt = (switch_da_t*)switch_channel_get_private(channel, "asr")))
    {
        switch_mutex_lock(pvt->mutex);
        pvt->started = 0;
        switch_mutex_unlock(pvt->mutex);
    }
}

/**
 * @brief asr请求构建
 *
 * @param cbParam
 * @return SpeechRecognizer*
 */
SpeechRecognizer* generateAsrRequest() {

    //gen unique voice_id
    boost::uuids::uuid a_uuid = boost::uuids::random_generator()();
    std::string voice_id_str = boost::uuids::to_string(a_uuid);
    SpeechRecognizer *request =
            new SpeechRecognizer(g_appid, g_secret_id, g_secret_key);
    if (request == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "createTranscriberRequest failed.\n" );
        return NULL;
    }

    request->SetVoiceId(voice_id_str);
    request->SetOnRecognitionStart(OnRecognitionStart);
    request->SetOnFail(OnFail);
    request->SetOnRecognitionComplete(OnRecognitionComplete);
    request->SetOnRecognitionResultChanged(OnRecognitionResultChange);
    request->SetOnSentenceBegin(OnSentenceBegin);
    request->SetOnSentenceEnd(OnSentenceEnd);
    request->SetEngineModelType(g_engine_model_type);
    request->SetNeedVad(1);
    request->SetVadSilenceTime(std::stoi(g_max_sentence_silence));

    return request;
}



//======================================== ali asr end ===============


//======================================== freeswitch module start ===============


SWITCH_MODULE_LOAD_FUNCTION(mod_tencent_asr_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_tencent_asr_shutdown);

extern "C" {
     SWITCH_MODULE_DEFINITION(mod_tencent_asr, mod_tencent_asr_load, mod_tencent_asr_shutdown, NULL);
};


/**
 * 配置加载
 *
 * @return switch_status_t 执行状态：
 */
static switch_status_t load_config()
{
    switch_xml_t cfg, xml, settings, param;

    if (!(xml = switch_xml_open_cfg(CONFIG_FILE, &cfg, NULL))) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", CONFIG_FILE);
        switch_xml_free(xml);
        return SWITCH_STATUS_TERM;
    }

    settings = switch_xml_child(cfg, "settings");
    if (!settings) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No settings in asr config\n");
        switch_xml_free(xml);
        return SWITCH_STATUS_TERM;
    }

    for (param = switch_xml_child(settings, "param"); param; param = param->next) {
        char *var = (char *) switch_xml_attr_soft(param, "name");
        char *val = (char *) switch_xml_attr_soft(param, "value");

        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Read conf: %s = %s\n", var, val);

        if (!strcasecmp(var, "appid")) {
            g_appid = val;
            continue;
        }

        if (!strcasecmp(var, "secret_id")) {
            g_secret_id =  val;
            continue;
        }

        if (!strcasecmp(var, "secret_key")) {
            g_secret_key=  val;
            continue;
        }

        if (!strcasecmp(var, "engine_model_type")) {
            g_engine_model_type =  val;
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "engine_model_type:%s\n", g_engine_model_type.c_str());
            continue;
        }

        if (!strcasecmp(var, "max_sentence_silence")) {
            g_max_sentence_silence =  val;
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "max_sentence_silence:%s\n", g_max_sentence_silence.c_str());
            continue;
        }
    }

	return SWITCH_STATUS_SUCCESS;
}


/**
 * asr 回调处理
 *
 * @param bug
 * @param user_data
 * @param type
 * @return switch_bool_t
 */
static switch_bool_t asr_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
    switch_da_t *pvt = (switch_da_t *)user_data;
    switch_channel_t *channel = switch_core_session_get_channel(pvt->session);

    switch (type) {
        case SWITCH_ABC_TYPE_INIT:
        {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "ASR Channel Init:%s\n", switch_channel_get_name(channel));
        }
        break;
        case SWITCH_ABC_TYPE_CLOSE:
        {
            if (pvt->request) {

                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "ASR Stop Succeed channel: %s\n", switch_channel_get_name(channel));

                pvt->request->Stop();

                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "asr stoped:%s\n", switch_channel_get_name(channel));

                //7: 识别结束, 释放request对象
                delete pvt->request;

                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "asr released:%s\n", switch_channel_get_name(channel));

            }
        }
        break;
        case SWITCH_ABC_TYPE_READ_REPLACE:
        {
            if(pvt->stoped ==1 ){
                return SWITCH_TRUE;
            }

            switch_frame_t *frame= switch_core_media_bug_get_read_replace_frame(bug);
            if (!frame) {
                return SWITCH_TRUE;
            }

            // if (frame->channels != 1)
            // {
            //     switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "nonsupport channels:%d!\n",frame->channels);
            //     return SWITCH_TRUE;
            // }

            switch_mutex_lock(pvt->mutex);
            if(pvt->started ==0 ) {

                if(pvt->starting ==0){

                    pvt->starting = 1;
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Starting Transaction \n" );

                    AsrParamCallBack *cbParam  = new AsrParamCallBack;

                    cbParam->sUUID = switch_channel_get_uuid(channel);
					cbParam->session = pvt->session;

                    switch_caller_profile_t  *profile = switch_channel_get_caller_profile(channel);

                    cbParam->caller = profile->caller_id_number;
                    cbParam->callee = profile->callee_id_number;

                    SpeechRecognizer* request = generateAsrRequest();

                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Caller %s. Callee %s\n",cbParam->caller.c_str() , cbParam->callee.c_str() );

                    if(request == NULL){
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Asr Request init failed.%s\n", switch_channel_get_name(channel));

                         return SWITCH_TRUE;
                    }

                    pvt->request = request;

                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Init SpeechRecognizer.%s\n", switch_channel_get_name(channel));

                    if (pvt->request->Start(cbParam) < 0) {

                       pvt->stoped = 1;

                       switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "start() failed. may be can not connect server. please check network or firewalld:%s\n", switch_channel_get_name(channel));

                        delete pvt->request;
                   }
                }

            }else {
                //====== resample ==== ///

                switch_codec_implementation_t read_impl;
                memset(&read_impl, 0, sizeof(switch_codec_implementation_t));

                switch_core_session_get_read_impl(pvt->session, &read_impl);


                int datalen = frame->datalen;
                int16_t *dp = (int16_t *)frame->data;

                switch_core_media_bug_set_read_replace_frame(bug, frame);

                if (read_impl.actual_samples_per_second != 8000) {
                    if (!pvt->resampler) {
                        if (switch_resample_create(&pvt->resampler,
                                                   read_impl.actual_samples_per_second,
                                                   8000,
                                                   8 * (read_impl.microseconds_per_packet / 1000) * 2,
                                                   SWITCH_RESAMPLE_QUALITY,
                                                   1) != SWITCH_STATUS_SUCCESS) {
                            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to allocate resampler\n");
                            return SWITCH_FALSE;
                        }
                    }

                    switch_resample_process(pvt->resampler, dp, (int) datalen / 2 / 1);
                    memcpy(dp, pvt->resampler->to, pvt->resampler->to_len * 2 * 1);
                    int samples = pvt->resampler->to_len;
                    datalen = pvt->resampler->to_len * 2 * 1;

                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "ASR new samples:%d\n", samples);

                }

                pvt->request->Write((uint8_t *)dp, (size_t)datalen);

            }

            switch_mutex_unlock(pvt->mutex);

        }
        break;
        default:
        break;
    }

    return SWITCH_TRUE;
}

/**
 *  定义添加的函数
 */
SWITCH_STANDARD_APP(stop_tencent_asr_session_function)
{
    switch_da_t *pvt;
    switch_channel_t *channel = switch_core_session_get_channel(session);

    if ((pvt = (switch_da_t*)switch_channel_get_private(channel, "asr"))) {

        switch_channel_set_private(channel, "asr", NULL);
        switch_core_media_bug_remove(session, &pvt->bug);
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Stop ASR\n", switch_channel_get_name(channel));

    }
}

/**
 *  定义添加的函数
 *
 *  注意：App函数是自带session的，Api中是没有的
 *       App函数中没有stream用于控制台输出的流；Api中是有的
 *       App函数不需要返回值；Api中是有的
 */
SWITCH_STANDARD_APP(start_tencent_asr_session_function)
{
    switch_channel_t *channel = switch_core_session_get_channel(session);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Starting asr:%s\n", switch_channel_get_name(channel));

    switch_status_t status;
    switch_da_t *pvt;
    switch_codec_implementation_t read_impl;

    //memset是计算机中C/C++语言初始化函数。作用是将某一块内存中的内容全部设置为指定的值， 这个函数通常为新申请的内存做初始化工作。
    memset(&read_impl, 0, sizeof(switch_codec_implementation_t));

    //获取读媒体编码实现方法
    switch_core_session_get_read_impl(session, &read_impl);

    if (!(pvt = (switch_da_t*)switch_core_session_alloc(session, sizeof(switch_da_t)))) {
        return;
    }

    pvt->started = 0;
    pvt->stoped = 0;
    pvt->starting = 0;
    pvt->datalen = 0;
    pvt->session = session;

    if ((status = switch_core_new_memory_pool(&pvt->pool)) != SWITCH_STATUS_SUCCESS) {
       switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");

       return;
    }

    switch_mutex_init(&pvt->mutex,SWITCH_MUTEX_NESTED,pvt->pool);

    // session添加media bug
    if ((status = switch_core_media_bug_add(session, MOD_NAME, NULL, asr_callback, pvt, 0, SMBF_READ_REPLACE, &(pvt->bug))) != SWITCH_STATUS_SUCCESS) {
        return;
    }

    switch_channel_set_private(channel, "asr", pvt);
    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "%s Start ASR\n", switch_channel_get_name(channel));
}


/**
 *  定义load函数，加载时运行
 */
SWITCH_MODULE_LOAD_FUNCTION(mod_tencent_asr_load)
{


    if (load_config() != SWITCH_STATUS_SUCCESS) {
    		return SWITCH_STATUS_FALSE;
    }

    switch_application_interface_t *app_interface;

    *module_interface = switch_loadable_module_create_module_interface(pool, modname);

    SWITCH_ADD_APP(app_interface, "start_tencent_asr", MOD_NAME, "tencent_asr", start_tencent_asr_session_function, "", SAF_MEDIA_TAP);
    SWITCH_ADD_APP(app_interface, "stop_tencent_asr", MOD_NAME, "tencent_asr", stop_tencent_asr_session_function, "", SAF_NONE);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, " asr_load\n");

    return SWITCH_STATUS_SUCCESS;
}

/**
 *  定义shutdown函数，关闭时运行
 */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_tencent_asr_shutdown)
{
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, " asr_shutdown\n");

    return SWITCH_STATUS_SUCCESS;
}
