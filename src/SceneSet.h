/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SCENESET_H
#define SCENESET_H

#include <iostream>
#include <memory>
#include <thread>
#include <atomic>
#include <csignal>
#include <condition_variable>
#include <mutex>
#include <systemd/sd-daemon.h>

#ifndef MODULE_NAME
#define MODULE_NAME Sceneset
#endif

#include <WPEFramework/com/com.h>
#include <WPEFramework/core/core.h>
#include "WPEFramework/interfaces/IAppManager.h"

using namespace std;
using namespace WPEFramework;

class SceneSetApp {
public:
    SceneSetApp();
    ~SceneSetApp();

    bool initialize();
    bool registerForAppEvents();
    bool unRegisterForAppEvents();
    bool launchDefaultApp();
    void waitForTermSignal();
    static void handleTerminationSignal(int signal);
    void onTerminate();
    static SceneSetApp& getInstance();
    void run();

private:
    std::condition_variable m_act_cv;
    volatile bool m_isActive;
    std::mutex m_lock;
    Exchange::IAppManager *m_appManager;
    std::shared_ptr<WPEFramework::Exchange::IAppManager::INotification> m_appManagerEventHandler;
    std::string m_appmgrCallsign;
    std::string m_defaultAppName;
    const char *m_comrpcPath;

    std::unique_ptr<std::thread> m_launchThread;
    std::atomic<bool> m_stopLaunchThread;
    std::mutex m_launchThreadMutex;

    void stopCurrentLaunchThread();
    void startLaunchThread();

    class AppManagerEventHandler : public Exchange::IAppManager::INotification {
    public:
        ~AppManagerEventHandler();
        void OnAppInstalled(const string &appId, const string &version) override;
        void OnAppUninstalled(const string &appId) override;
        void OnAppLifecycleStateChanged(const string &appId, const string &appInstanceId, const Exchange::IAppManager::AppLifecycleState newState, const Exchange::IAppManager::AppLifecycleState oldState, const Exchange::IAppManager::AppErrorReason errorReason) override;
        void OnAppLaunchRequest(const string &appId, const string &intent, const string &source) override;
        const char* getAppStateString(const Exchange::IAppManager::AppLifecycleState state);
        void OnAppUnloaded(const string &appId, const string &appInstanceId) override;
        uint32_t AddRef() const override;
        uint32_t Release() const override;
        void* QueryInterface(const uint32_t interfaceNumber) override;
    };
};

#endif // SCENESET_H
