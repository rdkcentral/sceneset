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

#include "SceneSet.h"

SceneSetApp::SceneSetApp()
    : m_isActive(false), appManager(nullptr), preinstallManager(nullptr), appManagerEventHandler(nullptr), preinstallManagerEventHandler(nullptr), appmgrCallsign("org.rdk.AppManager"), preinstallCallsign("org.rdk.PreinstallManager"), comrpcPath("/tmp/communicator") {}

SceneSetApp::~SceneSetApp() {
    unRegisterForAppEvents();
    unRegisterForPreinstallEvents();
}

bool SceneSetApp::initialize() {
    const char *thunderAccess = std::getenv("THUNDER_ACCESS");
    std::string envThunderAccess = (thunderAccess != nullptr) ? thunderAccess : comrpcPath;

    Core::SystemInfo::SetEnvironment(_T("THUNDER_ACCESS"), envThunderAccess.c_str());
    Core::ProxyType<RPC::CommunicatorClient> client1 = Core::ProxyType<RPC::CommunicatorClient>::Create(
        Core::NodeId(envThunderAccess.c_str()));
    Core::ProxyType<RPC::CommunicatorClient> client2 = Core::ProxyType<RPC::CommunicatorClient>::Create(
        Core::NodeId(envThunderAccess.c_str()));

    if (client1.IsValid()) {
        cout << " Registered to Thunder" << endl;
	preinstallManager = client1->Open<Exchange::IPreinstallManager>(preinstallCallsign.c_str());
	if (preinstallManager == nullptr) {
            std::cerr << "Failed to open IPreinstallManager interface." << std::endl;
            return false;
        }
    }
    if (client2.IsValid()) {
        appManager = client2->Open<Exchange::IAppManager>(appmgrCallsign.c_str());
        if (appManager == nullptr) {
            std::cerr << "Failed to open IAppManager interface." << std::endl;
            return false;
        }
    } else {
        std::cerr << "Failed to create COMRPC client." << std::endl;
        return false;
    }

    {
        lock_guard<mutex> lkgd(m_lock);
        m_isActive = true;
    }
    cout << "Registered to AppManager. Setting term signal" << endl;
    // Register term signal handler
    signal(SIGTERM, [](int x) {
        SceneSetApp::handleTerminationSignal(x);
    });
    return m_isActive;
}

bool SceneSetApp::registerForAppEvents() {
    if (nullptr == appManagerEventHandler) {
        appManagerEventHandler = std::make_shared<AppManagerEventHandler>();
    }
    if (appManager != nullptr) {
        appManager->Register(appManagerEventHandler.get());
        return true;
    }
    return false;
}

bool SceneSetApp::registerForPreinstallEvents() {
    if (nullptr == preinstallManagerEventHandler) {
        preinstallManagerEventHandler = std::make_shared<PreinstallManagerEventHandler>();
    }
    if (preinstallManager != nullptr) {
        preinstallManager->Register(preinstallManagerEventHandler.get());
        return true;
    }
    return false;
}

bool SceneSetApp::unRegisterForAppEvents() {
    cout << " Unregistering for App Events " << endl;
    if (nullptr != appManagerEventHandler && nullptr != appManager) {
        appManager->Unregister(appManagerEventHandler.get());
        appManagerEventHandler = nullptr;
        cout << " Unregistered  App Events " << endl;
        return true;
    } else {
        cout << "AppManager or EventHandler is null, cannot unregister" << endl;
    }
    return false;
}

bool SceneSetApp::unRegisterForPreinstallEvents() {
    cout << " Unregistering for Preinstall Events " << endl;
    if (nullptr != preinstallManagerEventHandler && nullptr != preinstallManager) {
        preinstallManager->Unregister(preinstallManagerEventHandler.get());
        preinstallManagerEventHandler = nullptr;
        cout << " Unregistered Preinstall Events " << endl;
        return true;
    } else {
        cout << "preinstallManager or EventHandler is null, cannot unregister" << endl;
    }
    return false;
}

bool SceneSetApp::launchDefaultApp() {
    const char* envAppName = std::getenv("SCENESET_DEFAULT_APPNAME");
    std::string appName = envAppName ? envAppName : "";
    if (appName.empty()) {
        std::cout << "No app name specified in SCENESET_DEFAULT_APPNAME env variable." << std::endl;
        return false;
    }
    std::cout << "Launching default app: " << appName << std::endl;
    appManager->LaunchApp(appName, "", "");
    return true;
}

bool SceneSetApp::startPreinstall() {
    std::cout << "Starting preinstall" << std::endl;
    preinstallManager->StartPreinstall(false);
    return true;
}

void SceneSetApp::waitForTermSignal() {
    std::thread termThread([&]() {
        while (m_isActive) {
            std::unique_lock<std::mutex> ulock(m_lock);
            m_act_cv.wait(ulock);
        }
        std::cout << "Exiting application..." << std::endl;
    });
    termThread.join();
}

void SceneSetApp::handleTerminationSignal(int signal) {
    std::cout << "Received termination signal: " << signal << std::endl;
    SceneSetApp::getInstance().onTerminate();
}

void SceneSetApp::onTerminate() {
    std::unique_lock<std::mutex> ulock(m_lock);
    m_isActive = false;
    unRegisterForAppEvents();
    unRegisterForPreinstallEvents();
    m_act_cv.notify_one();
}

SceneSetApp& SceneSetApp::getInstance() {
    static SceneSetApp instance;
    return instance;
}

void SceneSetApp::run() {
    if (!initialize()) return;
    sd_notifyf(0, "READY=1\n"
                  "STATUS=ComRPC client is Successfully Initialized\n"
                  "MAINPID=%lu",
               (unsigned long)getpid());
    registerForPreinstallEvents();
    registerForAppEvents();
    startPreinstall();
    launchDefaultApp();
    waitForTermSignal();
}

// AppManagerEventHandler implementations
SceneSetApp::AppManagerEventHandler::~AppManagerEventHandler() {}

void SceneSetApp::AppManagerEventHandler::OnAppInstalled(const string &appId, const string &version) {
    std::cout << "App Installed: " << appId << " Version: " << version << std::endl;
}

void SceneSetApp::AppManagerEventHandler::OnAppUninstalled(const string &appId) {
    std::cout << "App Uninstalled: " << appId << std::endl;
}

void SceneSetApp::AppManagerEventHandler::OnAppLifecycleStateChanged(const string &appId, const string &appInstanceId, const Exchange::IAppManager::AppLifecycleState newState, const Exchange::IAppManager::AppLifecycleState oldState, const Exchange::IAppManager::AppErrorReason errorReason) {
    std::cout << "App Lifecycle State Changed: " << appId
              << " from " << getAppStateString(oldState) << " (" << static_cast<int>(oldState) << ")"
              << " to " << getAppStateString(newState) << " (" << static_cast<int>(newState) << ")" << " with error " << getAppErrorString(errorReason)  << "("<< static_cast<int>(errorReason) << ")" << std::endl;
    if (oldState == Exchange::IAppManager::AppLifecycleState::APP_STATE_TERMINATING &&
        newState == Exchange::IAppManager::AppLifecycleState::APP_STATE_UNLOADED &&
        errorReason == Exchange::IAppManager::AppErrorReason::APP_ERROR_ABORT) {

        std::cout << "App " << appId << " terminated with ABORT error. Restarting reference app." << std::endl;
        SceneSetApp::getInstance().launchDefaultApp();
    }
}

void SceneSetApp::AppManagerEventHandler::OnAppLaunchRequest(const string &appId, const string &intent, const string &source) {
    std::cout << "App Launch Request: " << appId << " Intent: " << intent << " Source: " << source << std::endl;
}

const char* SceneSetApp::AppManagerEventHandler::getAppStateString(const Exchange::IAppManager::AppLifecycleState state) {
    switch(state) {
        case Exchange::IAppManager::AppLifecycleState::APP_STATE_UNLOADED: return "UNLOADED";
        case Exchange::IAppManager::AppLifecycleState::APP_STATE_LOADING: return "LOADING";  
        case Exchange::IAppManager::AppLifecycleState::APP_STATE_INITIALIZING: return "INITIALIZING";
        case Exchange::IAppManager::AppLifecycleState::APP_STATE_PAUSED: return "PAUSED";
        case Exchange::IAppManager::AppLifecycleState::APP_STATE_RUNNING: return "RUNNING";
        case Exchange::IAppManager::AppLifecycleState::APP_STATE_ACTIVE: return "ACTIVE";
        case Exchange::IAppManager::AppLifecycleState::APP_STATE_SUSPENDED: return "SUSPENDED";
        case Exchange::IAppManager::AppLifecycleState::APP_STATE_HIBERNATED: return "HIBERNATED";
        case Exchange::IAppManager::AppLifecycleState::APP_STATE_TERMINATING: return "TERMINATING";
        default: return "UNKNOWN";
    }
}

void SceneSetApp::AppManagerEventHandler::OnAppUnloaded(const string &appId, const string &appInstanceId) {
    std::cout << "App Unloaded: " << appId << " Instance ID: " << appInstanceId << std::endl;
    std::cout << " Restartng default app " << endl;
    SceneSetApp::getInstance().launchDefaultApp();
}

uint32_t SceneSetApp::AppManagerEventHandler::AddRef() const {
    cout << " AddRef called  " << endl;
    return Core::ERROR_NONE;
}

uint32_t SceneSetApp::AppManagerEventHandler::Release() const {
    cout << " Release called " << endl;
    return Core::ERROR_NONE;
}

void* SceneSetApp::AppManagerEventHandler::QueryInterface(const uint32_t interfaceNumber) {
    cout << " QueryInterface called " << endl;
    if (interfaceNumber == Exchange::IAppManager::INotification::ID) {
        return static_cast<Exchange::IAppManager::INotification*>(this);
    }
    return nullptr;
}

// PreinstallManagerEventHandler implementations
SceneSetApp::PreinstallManagerEventHandler::~PreinstallManagerEventHandler() {}

void SceneSetApp::PreinstallManagerEventHandler::OnAppInstallationStatus(const string &jsonresponse) {
    std::cout << "OnAppInstallationStatus: " << std::endl;
}

uint32_t SceneSetApp::PreinstallManagerEventHandler::AddRef() const {
    return Core::ERROR_NONE;
}

uint32_t SceneSetApp::PreinstallManagerEventHandler::Release() const {
    return Core::ERROR_NONE;
}

void* SceneSetApp::PreinstallManagerEventHandler::QueryInterface(const uint32_t interfaceNumber) {
    if (interfaceNumber == Exchange::IPreinstallManager::INotification::ID) {
        return static_cast<Exchange::IPreinstallManager::INotification*>(this);
    }
    return nullptr;
}

