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

#include "DCSApp/DCSApplication.h"
#include <algorithm>
#include <fstream>
#include "DCSApp/RecordAudioManager.h"
#include "DCSApp/ApplicationManager.h"

#include "DCSApp/VoiceAndTouchWakeUpObserver.h"

#include "DCSApp/Configuration.h"
#include "TtsPlayerProxy.h"
#include "MediaPlayerProxy.h"
#include "AlertsPlayerProxy.h"
#include "LocalPlayerProxy.h"
#include "BluetoothPlayerImpl.h"
#include "DlnaPlayerImpl.h"
#include "Mp3UrlPlayerImpl.h"
#include "Mp3TtsPlayerImpl.h"
#include "Mp3FilePlayerImpl.h"
#include "PcmTtsPlayerImpl.h"
#include "LocalTtsPlayerImpl.h"
#ifdef LOCALTTS
#include "LocalTtsProxy.h"
#endif
#include "BlueToothPlayerProxy.h"
#include "LoggerUtils/DcsSdkLogger.h"
#include "DCSApp/ActivityMonitorSingleton.h"

#ifdef DUEROS_DLNA
#include "Dlna/DlnaDmrSdk.h"
#include "Dlna/DlnaDmrOutputFfmpeg.h"
#endif
typedef std::shared_ptr<duerOSDcsApp::application::AudioMicrophoneInterface> createT(std::shared_ptr<duerOSDcsSDK::sdkInterfaces::DcsSdk> dcsSdk);
namespace duerOSDcsApp {
namespace application {

#ifdef MTK8516
#define PCM_DEVICE "track:10"
#else
#define PCM_DEVICE "default"
#endif

/// The config path of DCSSDK
static const std::string PATH_TO_CONFIG = "./resources/config.json";

#ifdef MTK8516
/// The runtime config path of DCSSDK
static const std::string PATH_TO_RUNTIME_CONFIG = "/data/duer/runtime.json";

static const std::string PATH_TO_BDUSS_FILE = "/data/duer/bduss.txt";
#else
/// The runtime config path of DCSSDK
static const std::string PATH_TO_RUNTIME_CONFIG = "./resources/runtime.json";

static const std::string PATH_TO_BDUSS_FILE = "./bduss.txt";
#endif

std::unique_ptr<DCSApplication> DCSApplication::create() {
    auto dcsApplication = std::unique_ptr<DCSApplication>(new DCSApplication);
    if (!dcsApplication->initialize()) {
        APP_ERROR("Failed to initialize SampleApplication");
        return nullptr;
    }
    return dcsApplication;
}

void DCSApplication::run() {
    while(true) {
        struct timeval wait = {0, 100 * 1000};
        ::select(0, NULL, NULL, NULL, &wait);
    }
}

bool DCSApplication::initialize() {
    m_audioDyLib = std::make_shared<deviceCommonLib::deviceTools::DyLibrary>();
    DeviceIoWrapper::getInstance()->initCommonVolume(Configuration::getInstance()->getCommVol());
    DeviceIoWrapper::getInstance()->initAlertVolume(Configuration::getInstance()->getAlertsVol());
    m_keyHandler = DCSKeyHandler::create(DeviceIoWrapper::getInstance());

    if (m_keyHandler == nullptr) {
      printf("Start key handler failed!\n");
    }

    /*
     * Creating the media players.
     */
    Configuration *configuration = Configuration::getInstance();
    auto speakMediaPlayer = mediaPlayer::TtsPlayerProxy::create(configuration->getTtsPlaybackDevice());
    if (!speakMediaPlayer) {
        APP_ERROR("Failed to create media player for speech!");
        return false;
    }

    auto audioMediaPlayer = mediaPlayer::MediaPlayerProxy::create(configuration->getMusicPlaybackDevice());
    if (!audioMediaPlayer) {
        APP_ERROR("Failed to create media player for content!");
        return false;
    }

    auto alertsMediaPlayer = mediaPlayer::AlertsPlayerProxy::create(configuration->getAlertPlaybackDevice());
    if (!alertsMediaPlayer) {
        APP_ERROR("Failed to create media player for alerts!");
        return false;
    }
    //alertsMediaPlayer->setFadeIn(10);

    auto localPromptPlayer = mediaPlayer::LocalPlayerProxy::create(configuration->getInfoPlaybackDevice());
    if (!localPromptPlayer) {
        APP_ERROR("Failed to create media player for local!");
        return false;
    }
#ifdef LOCALTTS
    auto localTtsPlayer = mediaPlayer::LocalTtsProxy::create(configuration->getNattsPlaybackDevice());
    if (!localTtsPlayer) {
        APP_ERROR("Failed to create media player for local tts!");
        return false;
    }
#endif

    auto blueToothPlayer = mediaPlayer::BlueToothPlayerProxy::create();
    if (!blueToothPlayer) {
        APP_ERROR("Failed to create blueToothPlayer!");
        return false;
    }

    auto applicationManager = std::make_shared<ApplicationManager>();


    parameters.setPathToConfig(PATH_TO_CONFIG);
    parameters.setPathToRuntimeConfig(PATH_TO_RUNTIME_CONFIG);
    parameters.setDeviceId(DeviceIoWrapper::getInstance()->getDeviceId());
    parameters.setSpeakMediaPlayer(speakMediaPlayer);
    parameters.setAudioMediaPlayer(audioMediaPlayer);
    parameters.setAlertsMediaPlayer(alertsMediaPlayer);
    parameters.setLocalPromptPlayer(localPromptPlayer);
    parameters.setDialogStateObservers({applicationManager});
    parameters.setConnectionObservers(applicationManager);
    parameters.setApplicationImplementation(applicationManager);
    parameters.setLocalMediaPlayer(blueToothPlayer);
    parameters.setDebugFlag(Configuration::getInstance()->getDebug());

#if (defined KITTAI_KEY_WORD_DETECTOR) || (defined BDSAI_KEY_WORD_DETECTOR)
    // This observer is notified any time a keyword is detected and notifies the DCSSDK to start recognizing.
    auto voiceAndTouchWakeUpObserver = std::make_shared<VoiceAndTouchWakeUpObserver>();
    parameters.setKeyWordObserverInterface(voiceAndTouchWakeUpObserver);
    auto keywordInterface = parameters.getKeyWordObserverInterface();
    keywordInterface->printInfo();
    printf("%s---->func %s Line %d----------\n", __FILE__, __FUNCTION__, __LINE__);
#endif

    auto voiceAndTouchWakeUpObserver = std::make_shared<VoiceAndTouchWakeUpObserver>();
    parameters.setKeyWordObserverInterface(voiceAndTouchWakeUpObserver);
    auto keywordInterface = parameters.getKeyWordObserverInterface();
    keywordInterface->printInfo();
#ifdef LOCALTTS
    parameters.setLocalTtsMediaPlayer(localTtsPlayer);
#endif
    duerOSDcsSDK::sdkInterfaces::DcsSdkParameters t_parameters;
    t_parameters = parameters;
    APP_ERROR("before dcssdk create!");
    m_dcsSdk = duerOSDcsSDK::sdkInterfaces::DcsSdk::create(parameters);

    if (!m_dcsSdk) {
        APP_ERROR("Failed to create default SDK handler!");
        return false;
    }
#if 0
    m_audioLibName = Configuration::getInstance()->getAudioLibName();

    bool ret = m_audioDyLib->load(m_audioLibName);
    if (!ret) {
        LOGGER_ERROR(LX("initialize Failed").d("reason", "load library error"));
        return false;
    }
    createT * m_create = (createT *)m_audioDyLib->getSym("create");

    if (!m_create) {
        LOGGER_ERROR(LX("initialize Failed").d("reason", "get symbol error"));
        return false;
    }

    std::shared_ptr<AudioMicrophoneInterface>  micWrapper = m_create(m_dcsSdk);

    micWrapper->setRecordDataInputCallback(recordDataInputCallback);

    applicationManager->setMicrophoneWrapper(micWrapper);
#endif
    applicationManager->setDcsSdk(m_dcsSdk);
    blueToothPlayer->setDcsSdk(m_dcsSdk);

    //create player client and attach to the player proxy. you may replace playerclient with your own
    auto mp3UrlPlayerImpl = mediaPlayer::Mp3UrlPlayerImpl::create(configuration->getMusicPlaybackDevice());
    mp3UrlPlayerImpl->registerListener(audioMediaPlayer);
    audioMediaPlayer->setImpl(mp3UrlPlayerImpl);

    auto mp3TtsPlayerImpl = mediaPlayer::Mp3TtsPlayerImpl::create(configuration->getTtsPlaybackDevice());
    mp3TtsPlayerImpl->registerListener(speakMediaPlayer);
    speakMediaPlayer->setMp3TtsImpl(mp3TtsPlayerImpl);

    auto pcmTtsPlayerImpl = mediaPlayer::PcmTtsPlayerImpl::create(PCM_DEVICE);
    pcmTtsPlayerImpl->registerListener(speakMediaPlayer);
    speakMediaPlayer->setPcmTtsImpl(pcmTtsPlayerImpl);
    auto mp3AlertPlayerImpl = mediaPlayer::Mp3FilePlayerImpl::create(configuration->getAlertPlaybackDevice());
    mp3AlertPlayerImpl->registerListener(alertsMediaPlayer);
    alertsMediaPlayer->setImpl(mp3AlertPlayerImpl);

    auto mp3FilePlayerImpl = mediaPlayer::Mp3FilePlayerImpl::create(configuration->getInfoPlaybackDevice());
    mp3FilePlayerImpl->registerListener(localPromptPlayer);
    localPromptPlayer->setImpl(mp3FilePlayerImpl);

    auto localTtsPlayerImpl = mediaPlayer::LocalTtsPlayerImpl::create(configuration->getNattsPlaybackDevice(),
                                                                          "./resources/");
    localTtsPlayerImpl->registerListener(localTtsPlayer);
    localTtsPlayer->setImpl(localTtsPlayerImpl);

    auto btPlayerImpl = mediaPlayer::BluetoothPlayerImpl::create();
    btPlayerImpl->registerListener(blueToothPlayer);
    blueToothPlayer->setBluetoothImpl(btPlayerImpl);
#ifdef DUEROS_DLNA
    auto dlnaPlayerImpl = mediaPlayer::DlnaPlayerImpl::create();
    dlnaPlayerImpl->registerListener(blueToothPlayer);
    blueToothPlayer->setDlnaImpl(dlnaPlayerImpl);
#endif
//#if (defined KITTAI_KEY_WORD_DETECTOR) || (defined BDSAI_KEY_WORD_DETECTOR)
    voiceAndTouchWakeUpObserver->setDcsSdk(m_dcsSdk);
//#endif

#ifdef LOCALTTS
    SoundController::getInstance()->addObserver(localTtsPlayer);
#endif
    SoundController::getInstance()->addObserver(localPromptPlayer);
//#if (defined KITTAI_KEY_WORD_DETECTOR) || (defined BDSAI_KEY_WORD_DETECTOR)
    DeviceIoWrapper::getInstance()->addObserver(voiceAndTouchWakeUpObserver);
//#endif
    DeviceIoWrapper::getInstance()->addObserver(btPlayerImpl);
#ifdef DUEROS_DLNA
    DeviceIoWrapper::getInstance()->addObserver(dlnaPlayerImpl);
#endif
    DeviceIoWrapper::getInstance()->setApplicationManager(applicationManager);
    applicationManager->microphoneOn();
    DeviceIoWrapper::getInstance()->volumeChanged();

    DuerLinkWrapper::getInstance()->setCallback(this);

    if (framework::DeviceIo::getInstance()->inOtaMode()) {
        DeviceIoWrapper::getInstance()->setMute(true);
    }

#if defined (MTK8516) || defined (Rk3308)
#ifdef DUERLINK_V2
    DuerLinkWrapper::getInstance()->initDuerLink();
#else
    DuerLinkWrapper::getInstance()->initDuerLink(PATH_TO_BDUSS_FILE, m_dcsSdk->getClientId());
#endif

    DuerLinkWrapper::getInstance()->startNetworkRecovery();
#if 0
    std::chrono::seconds secondsToWait{1};
    if (!m_detectNTPTimer.start(
            secondsToWait, deviceCommonLib::deviceTools::Timer::PeriodType::ABSOLUTE,
            deviceCommonLib::deviceTools::Timer::FOREVER,
            std::bind(&DCSApplication::detectNTPReady, this))) {
        APP_ERROR("Failed to start m_checkNtpTimer");
    }
#endif
#else
    m_dcsSdk->notifySystemTimeReady();
    m_dcsSdk->notifyNetworkReady(true, "testWifi");
#endif

#ifdef DUERLINK_V2
    DuerLinkWrapper::getInstance()->startDiscoverAndBound(m_dcsSdk->getClientId());
#else
    DuerLinkWrapper::getInstance()->startDiscoverAndBound(m_dcsSdk->getClientId(), PATH_TO_BDUSS_FILE);
#endif
#ifdef DUEROS_DLNA
    std::shared_ptr<dueros_dlna::IOutput> sp = std::make_shared<dueros_dlna::OutputFfmpeg>();
    dueros_dlna::DlnaDmrSdk dlnaDmrSdk;
    dlnaDmrSdk.add_output_module(sp);
    dlnaDmrSdk.start();
#endif
    m_systemUpdateRevWrapper = SystemUpdateRevWrapper::create();

    return true;
}

void DCSApplication::detectNTPReady() {
    if (access("/tmp/ntp_successful", F_OK) == 0) {
        m_dcsSdk->notifySystemTimeReady();
        m_detectNTPTimer.stop();
        ActivityMonitorSingleton::getInstance()->updatePlayerStatus(PLAYER_STATUS_OFF);
    }
}

void DCSApplication::networkReady() {
    if (m_dcsSdk) {
        DuerLinkWrapper::getInstance()->OnNetworkReady();
        DeviceIoWrapper::getInstance()->OnNetworkReady();
        APP_INFO("networkReady isFromConfigNetwork(): %d\n",
                 DuerLinkWrapper::getInstance()->isFromConfigNetwork());
        m_dcsSdk->notifyNetworkReady(
            DuerLinkWrapper::getInstance()->isFromConfigNetwork(),
            DeviceIoWrapper::getInstance()->getWifiBssid());
#if defined (Rk3308)
        m_dcsSdk->notifySystemTimeReady();
        ActivityMonitorSingleton::getInstance()->updatePlayerStatus(PLAYER_STATUS_OFF);
#endif
    }

#ifdef Build_CrabSdk
    /**
     * 网络连接成功，调用该接口上传上次崩溃转存的Crash文件
     */
    APP_INFO("crab upload crash dump file");
    baidu_crab_sdk::CrabSDK::upload_crash_async();
#endif
}

void DCSApplication::duerlinkNotifyReceivedData(const std::string& jsonPackageData, int sessionId) {
    if (!DeviceIoWrapper::getInstance()->isSleepMode()) {
        // APP_INFO("DCSApplication duerlink_notify_received_data: [%s], sessionId: [%d]",
        //          jsonPackageData.c_str(), sessionId);
        if (m_dcsSdk) {
            m_dcsSdk->consumeMessage(jsonPackageData);
        }
    }
}

void DCSApplication::duerlink_notify_received_bduss(const std::string& bdussValue) {
    if (m_dcsSdk) {
        m_dcsSdk->setBDUSS(bdussValue);
    }
}

void DCSApplication::duerlinkNetworkOnlineStatus(bool status) {
    if (m_dcsSdk) {
#ifdef Box86
        if (status) {
            if (!m_dcsSdk->isOAuthByPassPair()) {
                DuerLinkWrapper::getInstance()->waitLogin();
                DuerLinkWrapper::getInstance()->setFromConfigNetwork(true);
            } else {
                DuerLinkWrapper::getInstance()->setFromConfigNetwork(false);
            }
            m_dcsSdk->notifyNetworkReady(DuerLinkWrapper::getInstance()->isFromConfigNetwork(), DeviceIoWrapper::getInstance()->getWifiBssid());
        }
#endif
        m_dcsSdk->informOnlineStatus(status);
    }
}

void DCSApplication::recordDataInputCallback(const char* buffer, unsigned int size) {
    DeviceIoWrapper::getInstance()->transmitAudioToBluetooth(buffer, size);
    /// save record data to file.
    RecordAudioManager::getInstance()->transmitAudioRecordData(buffer, size);
}

bool DCSApplication::isPlayerRunning() {
    if (parameters.getSpeakMediaPlayer()->m_audioPlayerStatus == duerOSDcsSDK::sdkInterfaces::AudioPlayerStatus::PLAY_START
        ||parameters.getSpeakMediaPlayer()->m_audioPlayerStatus == duerOSDcsSDK::sdkInterfaces::AudioPlayerStatus::PLAYING) {
        return true;
    }

    if (parameters.getAudioMediaPlayer()->m_audioPlayerStatus == duerOSDcsSDK::sdkInterfaces::AudioPlayerStatus::PLAY_START
        ||parameters.getAudioMediaPlayer()->m_audioPlayerStatus == duerOSDcsSDK::sdkInterfaces::AudioPlayerStatus::PLAYING) {
        return true;
    }

    if (parameters.getAlertsMediaPlayer()->m_audioPlayerStatus == duerOSDcsSDK::sdkInterfaces::AudioPlayerStatus::PLAY_START
        ||parameters.getAlertsMediaPlayer()->m_audioPlayerStatus == duerOSDcsSDK::sdkInterfaces::AudioPlayerStatus::PLAYING) {
        return true;
    }

    if (parameters.getLocalMediaPlayer()->m_audioPlayerStatus == duerOSDcsSDK::sdkInterfaces::AudioPlayerStatus::PLAY_START
        ||parameters.getLocalMediaPlayer()->m_audioPlayerStatus == duerOSDcsSDK::sdkInterfaces::AudioPlayerStatus::PLAYING) {
        return true;
    }

    if (parameters.getLocalPromptPlayer()->m_audioPlayerStatus == duerOSDcsSDK::sdkInterfaces::AudioPlayerStatus::PLAY_START
        ||parameters.getLocalMediaPlayer()->m_audioPlayerStatus == duerOSDcsSDK::sdkInterfaces::AudioPlayerStatus::PLAYING) {
        return true;
    }

    if (parameters.getLocalTtsMediaPlayer()->m_audioPlayerStatus == duerOSDcsSDK::sdkInterfaces::AudioPlayerStatus::PLAY_START
        ||parameters.getLocalTtsMediaPlayer()->m_audioPlayerStatus == duerOSDcsSDK::sdkInterfaces::AudioPlayerStatus::PLAYING) {
        return true;
    }
    return false;
}

bool DCSApplication::isBusy() {
    return true;
}

void DCSApplication::enterSleepMode() {
    duerOSDcsSDK::sdkInterfaces::DcsSdk::enterSleepMode();
}

void DCSApplication::enterWakeupMode() {
    duerOSDcsSDK::sdkInterfaces::DcsSdk::enterWakeupMode();
}

bool DCSApplication::recordingStatus() {
// arm32 libdcssdk.so has no recordingStatus
#ifdef __aarch64__
    return duerOSDcsSDK::sdkInterfaces::DcsSdk::recordingStatus();
#else
	return false;
#endif
}

}  // namespace application
}  // namespace duerOSDcsApp
