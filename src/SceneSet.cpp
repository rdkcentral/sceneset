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
    :  m_act_cv(), m_isActive(false), m_lock(), m_appManager(nullptr), m_appManagerEventHandler(nullptr), m_appmgrCallsign("org.rdk.AppManager"), m_defaultAppName(""), m_comrpcPath("/tmp/communicator"), m_launchThread(nullptr), m_stopLaunchThread(false), m_launchThreadMutex() {
    const char* envAppName = std::getenv("SCENESET_DEFAULT_APPNAME");
    m_defaultAppName = envAppName ? envAppName : "";
}

SceneSetApp::~SceneSetApp() {
    stopCurrentLaunchThread();
    unRegisterForAppEvents();
}

bool SceneSetApp::initialize() {
    const char *thunderAccess = std::getenv("THUNDER_ACCESS");
    std::string envThunderAccess = (thunderAccess != nullptr) ? thunderAccess : m_comrpcPath;

    Core::SystemInfo::SetEnvironment(_T("THUNDER_ACCESS"), envThunderAccess.c_str());
    Core::ProxyType<RPC::CommunicatorClient> client = Core::ProxyType<RPC::CommunicatorClient>::Create(
        Core::NodeId(envThunderAccess.c_str()));

    if (client.IsValid()) {
        cout << " Registered to Thunder" << endl;
        m_appManager = client->Open<Exchange::IAppManager>(m_appmgrCallsign.c_str());
        if (m_appManager == nullptr) {
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
    if (nullptr == m_appManagerEventHandler) {
        m_appManagerEventHandler = std::make_shared<AppManagerEventHandler>();
    }
    if (m_appManager != nullptr) {
        m_appManager->Register(m_appManagerEventHandler.get());
        return true;
    }
    return false;
}

bool SceneSetApp::unRegisterForAppEvents() {
    cout << " Unregistering for App Events " << endl;
    if (nullptr != m_appManagerEventHandler && nullptr != m_appManager) {
        m_appManager->Unregister(m_appManagerEventHandler.get());
        m_appManagerEventHandler = nullptr;
        cout << " Unregistered  App Events " << endl;
        return true;
    } else {
        cout << "AppManager or EventHandler is null, cannot unregister" << endl;
    }
    return false;
}

bool SceneSetApp::launchDefaultApp() {
    if (m_defaultAppName.empty()) {
        std::cout << "No app name specified in SCENESET_DEFAULT_APPNAME env variable." << std::endl;
        return false;
    }
    std::cout << "Launching default app: " << m_defaultAppName << std::endl;
    m_appManager->LaunchApp(m_defaultAppName, "", "");
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
    registerForAppEvents();
    startLaunchThread();
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
    
    SceneSetApp& instance = SceneSetApp::getInstance();
    std::cout << "App Lifecycle State Changed for " << appId
              << " from " << getAppStateString(oldState) << " (" << static_cast<int>(oldState) << ")"
              << " to " << getAppStateString(newState) << " (" << static_cast<int>(newState) << ")" << std::endl;
    if (!instance.m_defaultAppName.empty() && appId == instance.m_defaultAppName) {
         
        if (oldState == Exchange::IAppManager::AppLifecycleState::APP_STATE_TERMINATING &&
            newState == Exchange::IAppManager::AppLifecycleState::APP_STATE_UNLOADED &&
            errorReason == Exchange::IAppManager::AppErrorReason::APP_ERROR_ABORT) {
            std::cout << "App " << appId << " terminated with ABORT error. Restarting reference app." << std::endl;
            instance.startLaunchThread();
        }
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

/**
 * Stops the currently running launch thread, if any.
 */
void SceneSetApp::stopCurrentLaunchThread() {
    std::lock_guard<std::mutex> lock(m_launchThreadMutex);
    if (m_launchThread && m_launchThread->joinable()) {
        std::cout << "Stopping current launch thread" << std::endl;
        m_stopLaunchThread = true;
        m_launchThread->join();
        m_launchThread.reset();
        std::cout << "Launch thread stopped successfully" << std::endl;
    }
    m_stopLaunchThread = false;
}

void SceneSetApp::startLaunchThread() {
    std::lock_guard<std::mutex> lock(m_launchThreadMutex);

    if (m_launchThread && m_launchThread->joinable()) {
        m_stopLaunchThread = true;
        m_launchThread->join();
        m_launchThread.reset();
    }

    m_stopLaunchThread = false;
    std::cout << "Launching default application" << std::endl;

    m_launchThread = std::make_unique<std::thread>([this]() {
        try {
            if (!m_stopLaunchThread) {
                std::cout << "Executing launchDefaultApp from thread" << std::endl;
                if (!launchDefaultApp()) {
                    std::cerr << "Failed to launch default app from thread" << std::endl;
                }
            } else {
                std::cout << "Launch thread cancelled before execution" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception in launch thread: " << e.what() << std::endl;
        }
    });
}
