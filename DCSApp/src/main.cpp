/*
 * Copyright (c) 2017 Baidu, Inc. All Rights Reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
#if 1
#include <signal.h>
#include <sys/time.h>
#include <cstdlib>
#include <string>
#include <execinfo.h>
#include "LoggerUtils/DcsSdkLogger.h"
#include "DCSApp/DCSApplication.h"
#include "DCSApp/DeviceIoWrapper.h"
#include "DCSApp/Configuration.h"
#include <DeviceTools/SingleApplication.h>
#include <DeviceTools/PrintTickCount.h>
#include "DCSApp/DuerLinkWrapper.h"
#include "DeviceTools/DeviceUtils.h"

#ifndef __SAMPLEAPP_VERSION__
#define __SAMPLEAPP_VERSION__ "Unknown SampleApp Version"
#endif

#ifndef __DCSSDK_VERSION__
#define __DCSSDK_VERSION__ "Unknown DcsSdk Version"
#endif

#ifndef __DUER_LINK_VERSION__
#define __DUER_LINK_VERSION__ "Unknown DuerLink Version"
#endif
pthread_mutex_t mylock=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t mycond=PTHREAD_COND_INITIALIZER;
const unsigned int voice_inactive_max_count = 16000 * 3; //16k, 3 seconds

unsigned int read_voice_inactive_frames(void)
{
    FILE *fp = nullptr;
    unsigned int frames = 0;

    fp = fopen("/sys/module/snd_soc_rockchip_vad/parameters/voice_inactive_frames", "r");
    if (!fp) {
        perror("fopen voice_inactive_frames\n");
        return -1;
    }
    fscanf(fp, "%u\n", &frames);
    fclose(fp);
    return frames;
}

bool IsRunningStatus(void* arg) {
    duerOSDcsApp::application::DCSApplication * app = (duerOSDcsApp::application::DCSApplication *)arg;
    duerOSDcsApp::application::DuerLinkNetworkStatus durlink_status = duerOSDcsApp::application::DuerLinkWrapper::getInstance()->getNetworkStatus();
    if (durlink_status == duerOSDcsApp::application::DUERLINK_NETWORK_CONFIG_STARTED
            || durlink_status == duerOSDcsApp::application::DUERLINK_NETWORK_CONFIGING) {
        return true;
    }

    if (app->isPlayerRunning()) {
        return true;
    }

    return false;
}

bool sleep_check(void* arg) {
    unsigned int inactive_frames = read_voice_inactive_frames();

    if (IsRunningStatus(arg)) {
        deviceCommonLib::deviceTools::DeviceUtils::clear_voice_inactive_frames();
    }

    if ((inactive_frames > voice_inactive_max_count))
        return true;
    return false;
}

void wait_device_mode_timeout_ms(int microseconds)
{
    struct timeval tv;
    long long absmsec;
    struct timespec abstime;

    gettimeofday(&tv, NULL);
    absmsec = tv.tv_sec * 1000ll + tv.tv_usec / 1000ll;
    absmsec += microseconds;

    abstime.tv_sec = absmsec / 1000ll;
    abstime.tv_nsec = absmsec % 1000ll * 1000000ll;

    printf("#### public sleep mode ####");
    pthread_mutex_lock(&mylock);
    pthread_cond_timedwait(&mycond, &mylock, &abstime);
    pthread_mutex_unlock(&mylock);
    printf("#### return sleep mode succeed ####");
}

void *vad_detect_func(void* arg) {
    //system("../bin/aispeech_led  -m single -i 2 0");
    system("echo 0 > /sys/module/snd_soc_rockchip_vad/parameters/voice_inactive_frames");
    while (true) {
        if (sleep_check(arg)) {
            fprintf(stderr,"voice inactive timeout,go to sleep\n");
            //enter sleep mode
            duerOSDcsApp::application::DCSApplication::enterSleepMode();
            //......
            while (!duerOSDcsApp::application::DCSApplication::recordingStatus()) {
                //wait_device_mode_timeout_ms(10);
                usleep(1000*10);
            }
            printf("pause >>>>\n");
            system("echo 0 > /sys/module/snd_soc_rockchip_vad/parameters/voice_inactive_frames");
            system("echo mem > /sys/power/state");
            printf("resume >>>>\n");
            //enter normal mode
            //......
            duerOSDcsApp::application::DCSApplication::enterWakeupMode();
        } else {
            //fprintf(stderr,"read_voice_inactive_frames:%d\n",read_voice_inactive_frames());
        }
        usleep(1000*1000);
    }
}


int main(int argc, char** argv) {
    deviceCommonLib::deviceTools::printTickCount("duer_linux main begin");

    LOGGER_ENABLE(false);
    if (!duerOSDcsApp::application::Configuration::getInstance()->readConfig()) {
        deviceCommonLib::deviceTools::printTickCount("Failed to init Configuration!");
        return 1;
    }

    if (duerOSDcsApp::application::Configuration::getInstance()->getDebug()) {
        deviceCommonLib::deviceTools::printTickCount("open logs!!");
        LOGGER_ENABLE(true);
    }

#ifdef Build_CrabSdk
    /**
     * 首先用Crab平台分配的App Key和宿主程序的版本进行初始化
     * 最好在main函数中进行调用
     */
    std::string m_softwareVersion = duerOSDcsApp::application::DeviceIoWrapper::getInstance()->getVersion();
    std::string m_deviceId = duerOSDcsApp::application::DeviceIoWrapper::getInstance()->getDeviceId();
    baidu_crab_sdk::CrabSDK::init("78ceb6f2024be4e7", m_softwareVersion, "/tmp/coredump");
    /**
     * 设置设备DEVICEID
     */
    baidu_crab_sdk::CrabSDK::set_did(m_deviceId);
#endif

#ifdef MTK8516
    if (geteuid() != 0) {
        APP_ERROR("This program must run as root, such as \"sudo ./duer_linux\"");
        return 1;
    }
    if (deviceCommonLib::deviceTools::SingleApplication::is_running()) {
        APP_ERROR("duer_linux is already running");
        return 1;
    }
#endif

    /// print current version
    APP_INFO("SampleApp Version: [%s]", __SAMPLEAPP_VERSION__);
    APP_INFO("DcsSdk Version: [%s]", __DCSSDK_VERSION__);
    APP_INFO("DuerLink Version: [%s]", __DUER_LINK_VERSION__);
    printf("%s------>Line %d -----\n", __FUNCTION__, __LINE__);

    auto dcsApplication = duerOSDcsApp::application::DCSApplication::create();

    if (!dcsApplication) {
        APP_ERROR("Failed to create to SampleApplication!");
        duerOSDcsApp::application::SoundController::getInstance()->release();
        duerOSDcsApp::application::DeviceIoWrapper::getInstance()->release();
        return EXIT_FAILURE;
    }

    pthread_t softvad_detect;
    pthread_create(&softvad_detect,NULL,vad_detect_func,dcsApplication.get());

    // This will run until application quit.
    dcsApplication->run();

    return EXIT_SUCCESS;
}
#else
#include <iostream>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <vector>
#include <signal.h>

// Generic SDK interface
#include "BDSpeechSDK.hpp"
#include "BDSSDKMessage.hpp"

// Definitions for ASR engine
#include "bds_asr_key_definitions.hpp"
#include "bds_ASRDefines.hpp"

// Definitions for wakeup engine
#include "bds_wakeup_key_definitions.hpp"
#include "bds_WakeupDefines.hpp"

using namespace std;
using namespace bds;
BDSpeechSDK* g_Asr;
void asr_online_start(BDSpeechSDK* SDK);
void wakeup_listener(bds::BDSSDKMessage& message, void* myParam) {
    cout << "wakeup event from " << *((std::string*)myParam) << ": " << message.name << endl;
    std::vector<std::string> keys = message.string_param_keys();

    int status = -1;
    message.get_parameter(CALLBACK_WAK_STATUS, status);
    if (status == bds::EWakeupEngineWorkStatusTriggered) {
        printf("==========****wake up****===========\n");
        asr_online_start(g_Asr);

    }
    for (std::vector<std::string>::iterator it = keys.begin();
         it < keys.end();
         ++it) {
        std::string value;
        message.get_parameter(*it, value);
        cout << "STRKEY: " << *it << " = " << value << endl;
    }

    keys = message.int_param_keys();

    for (std::vector<std::string>::iterator it = keys.begin();
         it < keys.end();
         ++it) {
        int value;
        message.get_parameter(*it, value);
        cout << "INTKEY " << *it << " = " << value << endl;
    }

    keys = message.char_param_keys();

    for (std::vector<std::string>::iterator it = keys.begin();
         it < keys.end();
         ++it) {
        cout << "DATA KEY: " << *it << endl;
    }
}

const std::string WAKEUP_USER_PARAM("wakeup engine");
BDSpeechSDK* wakeup_init() {
    string errorMsg;
    BDSpeechSDK* SDK = bds::BDSpeechSDK::get_instance(bds::SDK_TYPE_WAKEUP, errorMsg);

    if (SDK == NULL) {
        cout << "failed get wakeup SDK " << errorMsg << endl;
    }

    SDK->set_event_listener(&wakeup_listener, (void*)&WAKEUP_USER_PARAM);
    return SDK;
}

void wakeup_config(BDSpeechSDK* SDK) {
    string errorMsg;
    bds::BDSSDKMessage msg;
    msg.name = bds::WAK_CMD_CONFIG;
    msg.set_parameter(bds::COMMON_PARAM_KEY_DEBUG_LOG_LEVEL, 0);
    //msg.set_parameter(bds::WP_PARAM_KEY_ENABLE_DNN_WAKEUP, false);
   // msg.set_parameter(bds::WP_PARAM_KEY_WAKEUP_SUPPORT_ESIS, 1);
    msg.set_parameter(bds::OFFLINE_PARAM_KEY_LICENSE_FILE_PATH, std::string("./resource/bds_license.dat"));
    msg.set_parameter(bds::WP_PARAM_KEY_WAKEUP_WORDS, std::string("{\"words\":[\"小度小度\"]}"));
    msg.set_parameter(bds::WP_PARAM_KEY_WAKEUP_DAT_FILE_PATH, std::string("./resources/esis_carlife.pkg"));
    msg.set_parameter(bds::OFFLINE_PARAM_KEY_APP_CODE, "9959452");
    msg.set_parameter(bds::WP_PARAM_KEY_WAKEUP_ENABLE_ONESHOT, 0);

    if (!SDK->post(msg, errorMsg)) {
        cout << "failed send config message " << errorMsg << endl;
        bds::BDSpeechSDK::release_instance(SDK);
        return;
    }
}

void wakeup_load_engine(BDSpeechSDK* SDK) {
    string errorMsg;
    bds::BDSSDKMessage loadEngine;
    loadEngine.name = bds::WAK_CMD_LOAD_ENGINE;
    if(!SDK->post(loadEngine,errorMsg)){
        cout << "failed send load message " << errorMsg << endl;
        bds::BDSpeechSDK::release_instance(SDK);
        return;
    }
}

void wakeup_unload_engine(BDSpeechSDK* SDK) {
    string errorMsg;
    bds::BDSSDKMessage loadEngine;
    loadEngine.name = bds::WAK_CMD_UNLOAD_ENGINE;
    if(!SDK->post(loadEngine,errorMsg)){
        cout << "failed send unload message " << errorMsg << endl;
        bds::BDSpeechSDK::release_instance(SDK);
        return;
    }
}

void wakeup_start(BDSpeechSDK* SDK) {
    string errorMsg;
    bds::BDSSDKMessage msg;
    msg.name = bds::WAK_CMD_START;
    msg.set_parameter(bds::MIC_PARAM_KEY_CHANNEL_TYPE, 2);
    if (!SDK->post(msg, errorMsg)) {
        cout << "failed send config message " << errorMsg << endl;
        bds::BDSpeechSDK::release_instance(SDK);
        return;
    }
}

void wakeup_stop(BDSpeechSDK* SDK) {
    string error;
    BDSSDKMessage msg;
    msg.name = WAK_CMD_STOP;
    SDK->post(msg, error);
}

void wakeup_release(BDSpeechSDK* SDK) {
    BDSpeechSDK::release_instance(SDK);
}

void wakeup_offline_setpath(BDSpeechSDK* SDK) {
    string errorMsg;
    bds::BDSSDKMessage msg;
    msg.name = bds::BDS_COMMAND_SET_WRITABLE_USER_DATA_PATH;
    msg.set_parameter(bds::BDS_PARAM_KEY_WRITABLE_USER_DATA_PATH, "./");
    if (!SDK->post(msg, errorMsg)) {
        cout << "failed send config message " << errorMsg << endl;
        return;
    }
}

void asr_listener(bds::BDSSDKMessage& message, void* myParam) {
    cout << "asr event from " << *((std::string*)myParam) << ": " << message.name << endl;
    std::vector<std::string> keys = message.string_param_keys();

    for (std::vector<std::string>::iterator it = keys.begin();
         it < keys.end();
         ++it) {
        std::string value;
        message.get_parameter(*it, value);
        cout << "STRKEY: " << *it << " = " << value << endl;
    }

    keys = message.int_param_keys();

    for (std::vector<std::string>::iterator it = keys.begin();
         it < keys.end();
         ++it) {
        int value;
        message.get_parameter(*it, value);
        cout << "INTKEY " << *it << " = " << value << endl;
    }

    keys = message.char_param_keys();

    for (std::vector<std::string>::iterator it = keys.begin();
         it < keys.end();
         ++it) {
        cout << "DATA KEY: " << *it << endl;
    }
}

const std::string ASR_USER_PARAM("ASR engine");
BDSpeechSDK* asr_online_init() {
    string errorMsg;
    BDSpeechSDK* SDK = bds::BDSpeechSDK::get_instance(bds::SDK_TYPE_ASR, errorMsg);

    if (SDK == NULL) {
        cout << "failed get ASR SDK " << errorMsg << endl;
    }

    SDK->set_event_listener(&asr_listener, (void*)&ASR_USER_PARAM);
    return SDK;
}

void asr_online_config(BDSpeechSDK* SDK) {
    string errorMsg;
    bds::BDSSDKMessage msg;
    msg.name = bds::ASR_CMD_CONFIG;
    msg.set_parameter(bds::COMMON_PARAM_KEY_DEBUG_LOG_LEVEL, 0);
    //msg.set_parameter(bds::ASR_PARAM_KEY_CHUNK_PARAM, std::string("{   \"CUID\" : \"audiounicast_debug_monitor\",   \"app_ver\" : \"1.0.0\",   \"appid\" : \"dmE8101CFDEA394AA5\",   \"appkey\" : \"E87F998435904CB1B050CE169CC9073C\",   \"client_msg_id\" : \"1234556\",   \"debug\" : 0,   \"from_client\" : \"sdk\",   \"latitude\" : 0.0,   \"location_system\" : \"wgs84\",   \"longitude\" : 0.0,   \"operation_system\" : \"android\",   \"operation_system_version\" : \"\",   \"request_from\" : \"0\",   \"request_query\" : \"播放一首歌，\",   \"request_type\" : \"0\",   \"request_uid\" : \"audiounicast_debug_monitor\",   \"sample_name\" : \"bear_brain_wireless\",   \"sdk_ui\" : \"no\",   \"service_id\" : \"1234556\",   \"speech_id\" : \"6393155938941717864\"}"));
    msg.set_parameter(bds::ASR_PARAM_KEY_SERVER_URL, std::string("http://vse.baidu.com/v2"));
    msg.set_parameter(bds::ASR_PARAM_KEY_CHUNK_ENABLE, 1);
    msg.set_parameter(bds::ASR_PARAM_KEY_CHUNK_KEY, std::string("com.baidu.dumi.dcs31.rk3308"));
    msg.set_parameter(bds::ASR_PARAM_KEY_PRODUCT_ID, std::string("1400"));
    msg.set_parameter(bds::ASR_PARAM_KEY_ENABLE_LOCAL_VAD, 0);
//    msg.set_parameter(bds::ASR_PARAM_KEY_MFE_DNN_DAT_FILE, "../../resources/asr_resource/bds_easr_mfe_dnn.dat");
    msg.set_parameter(bds::ASR_PARAM_KEY_SAVE_VAD_AUDIO_ENABLE, true);

    if (!SDK->post(msg, errorMsg)) {
        cout << "failed send config message " << errorMsg << endl;
        bds::BDSpeechSDK::release_instance(SDK);
        return;
    }
}

void asr_online_start(BDSpeechSDK* SDK) {
    string errorMsg;
    bds::BDSSDKMessage msg;
    msg.name = bds::ASR_CMD_START;
    msg.set_parameter(bds::ASR_PARAM_KEY_PLATFORM, std::string("linux"));
    msg.set_parameter(bds::ASR_PARAM_KEY_APP, std::string("EchoTest"));
    msg.set_parameter(bds::ASR_PARAM_KEY_SDK_VERSION, std::string("LINUX TEST"));

    //msg.set_parameter(bds::ASR_PARAM_KEY_API_KEY,std::string("dVl7kAI2AMCIIkEGRR7lmCAw"));
    //msg.set_parameter(bds::ASR_PARAM_KEY_SECRET_KEY,std::string("f4a670f08725dbcdf376a447844aa2e6"));
    if (!SDK->post(msg, errorMsg)) {
        cout << "failed send config message " << errorMsg << endl;
        bds::BDSpeechSDK::release_instance(SDK);
        return;
    }
}

void asr_online_cancel(BDSpeechSDK* SDK) {
    cout << "---------------------------cancel start--------------------------" << endl;
    string error;
    BDSSDKMessage cancel;
    cancel.name = ASR_CMD_CANCEL;
    SDK->post(cancel, error);
    cout << "---------------------------cancel  end--------------------------" << endl;
}

void asr_online_release(BDSpeechSDK* SDK) {
    BDSpeechSDK::release_instance(SDK);
}

void start_all(BDSpeechSDK *asr, BDSpeechSDK *wakeup) {
    wakeup_start(wakeup);
}

void cancel_all(BDSpeechSDK *asr, BDSpeechSDK *wakeup) {
    //wakeup_stop(wakeup);
    //asr_online_cancel(asr);
}

void test1(BDSpeechSDK *asr, BDSpeechSDK *wakeup) {
    start_all(asr, wakeup);
    sleep(20);
    cancel_all(asr, wakeup);
}

bool isRunning = true;
void signal_handler(int signal) {
    std::cout << "---> signal = " << signal << endl;
    isRunning = false;
}

int main(int argc, char** argv) {
    cout << "---------------------------main start---------------------------" << endl;

    BDSpeechSDK *asr = asr_online_init();
    g_Asr = asr;
    asr_online_config(asr);

    BDSpeechSDK *wakeup = wakeup_init();
    wakeup_config(wakeup);
    wakeup_load_engine(wakeup);
    test1(asr, wakeup);
    signal(SIGINT, &signal_handler);
    while (isRunning) {
        sleep(1);
    }
    wakeup_release(wakeup);
    asr_online_release(asr);
    cout << "---------------------------main  end---------------------------" << endl;
    return 0;
}

#endif
