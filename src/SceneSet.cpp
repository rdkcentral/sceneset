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
#include <fstream>
#include <filesystem>
#include <string_view>
#include <optional>

#ifndef SCENESET_DEFAULT_APPNAME
#define SCENESET_DEFAULT_APPNAME ""
#endif

#ifndef FACTORY_APP_PATH
#define FACTORY_APP_PATH ""
#endif

#ifndef APP_PREINSTALL_DIRECTORY
#define APP_PREINSTALL_DIRECTORY ""
#endif

#define SCENESET_CONFIG_FILE "/opt/sceneset_app.conf"
#define FACTORY_APPS_COPIED_MARKER "/opt/persistent/.sceneset_factory_apps_copied"

static std::string getDefaultAppName() {
    std::ifstream configFile(SCENESET_CONFIG_FILE);
    if (configFile.is_open()) {
        std::string appName;
        std::getline(configFile, appName);

        if (!appName.empty()) {
            std::cout << "Using sceneset default app from config file: " << appName << std::endl;
            return appName;
        }
    }

    std::string appDefault = SCENESET_DEFAULT_APPNAME;
    if (!appDefault.empty()) {
        std::cout << "Using sceneset default app: " << appDefault << std::endl;
    }
    return appDefault;
}

SceneSetApp::SceneSetApp()
    :  m_act_cv(), m_isActive(false), m_lock(), m_appManager(nullptr), m_preinstallManager(nullptr), m_appManagerEventHandler(nullptr), m_preinstallManagerEventHandler(nullptr), m_appmgrCallsign("org.rdk.AppManager"), m_preinstallCallsign("org.rdk.PreinstallManager"), m_referenceAppId(getDefaultAppName()), m_comrpcPath("/tmp/communicator"), m_launchThread(nullptr), m_stopLaunchThread(false), m_appLaunched(false), m_pendingRestart(false), m_launchThreadMutex() {
}

SceneSetApp::~SceneSetApp() {
    stopCurrentLaunchThread();
    unRegisterForAppEvents();
    unRegisterForPreinstallEvents();
}

bool SceneSetApp::initialize() {
    const char *thunderAccess = std::getenv("THUNDER_ACCESS");
    std::string envThunderAccess = (thunderAccess != nullptr) ? thunderAccess : m_comrpcPath;

    std::cout << "Thunder Access Path: " << envThunderAccess << std::endl;

    Core::SystemInfo::SetEnvironment(_T("THUNDER_ACCESS"), envThunderAccess.c_str());

    auto appManagerClient = Core::ProxyType<RPC::CommunicatorClient>::Create(
        Core::NodeId(envThunderAccess.c_str()));

    if (!appManagerClient.IsValid()) {
        std::cerr << "Failed to create COMRPC client for AppManager." << std::endl;
        return false;
    }

    std::cout << "AppManager COMRPC client created successfully" << std::endl;

    // Open AppManager interface
    m_appManager = appManagerClient->Open<Exchange::IAppManager>(m_appmgrCallsign.c_str());
    if (m_appManager == nullptr) {
        std::cerr << "Failed to open IAppManager interface." << std::endl;
        return false;
    }

    std::cout << "Successfully opened " << m_appmgrCallsign << " interface" << std::endl;

    auto preinstallClient = Core::ProxyType<RPC::CommunicatorClient>::Create(
        Core::NodeId(envThunderAccess.c_str()));

    if (!preinstallClient.IsValid()) {
        std::cerr << "Failed to create COMRPC client for PreinstallManager." << std::endl;
        // Clean up appManager before returning
        if (m_appManager != nullptr) {
            m_appManager->Release();
            m_appManager = nullptr;
        }
        return false;
    }

    std::cout << "PreinstallManager COMRPC client created successfully" << std::endl;
    // Open PreinstallManager interface
    m_preinstallManager = preinstallClient->Open<Exchange::IPreinstallManager>(m_preinstallCallsign.c_str());
    if (m_preinstallManager == nullptr) {
        std::cerr << "Failed to open IPreinstallManager interface." << std::endl;
        // Clean up appManager before returning
        if (m_appManager != nullptr) {
            m_appManager->Release();
            m_appManager = nullptr;
        }
        return false;
    }

    std::cout << "Successfully opened " << m_preinstallCallsign << " interface" << std::endl;

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

bool SceneSetApp::registerForPreinstallEvents() {
    if (nullptr == m_preinstallManagerEventHandler) {
        m_preinstallManagerEventHandler = std::make_shared<PreinstallManagerEventHandler>();
    }
    if (m_preinstallManager != nullptr) {
        m_preinstallManager->Register(m_preinstallManagerEventHandler.get());
        return true;
    }
    return false;
}

bool SceneSetApp::unRegisterForAppEvents() {
    std::cout << " Unregistering for AppManager Events " << endl;
    if (nullptr != m_appManagerEventHandler && nullptr != m_appManager) {
        try {
            m_appManager->Unregister(m_appManagerEventHandler.get());
            std::cout << " Unregistered AppManager Events " << endl;
        } catch (...) {
            std::cerr << "Exception during AppManager unregister" << endl;
        }
    } else {
        std::cout << "AppManager or EventHandler is null, cannot unregister" << endl;
    }
    m_appManagerEventHandler = nullptr;
    return true;
}

bool SceneSetApp::unRegisterForPreinstallEvents() {
    std::cout << " Unregistering for Preinstall Events " << endl;
    if (nullptr != m_preinstallManagerEventHandler && nullptr != m_preinstallManager) {
        try {
            m_preinstallManager->Unregister(m_preinstallManagerEventHandler.get());
            std::cout << " Unregistered Preinstall Events " << endl;
        } catch (...) {
            std::cerr << "Exception during PreinstallManager unregister" << endl;
        }
    } else {
        std::cout << "PreinstallManager or EventHandler is null, cannot unregister" << endl;
    }
    m_preinstallManagerEventHandler = nullptr;
    return true;
}

bool SceneSetApp::launchDefaultApp() {
    if (m_referenceAppId.empty()) {
        std::cout << "No reference app specified" << std::endl;
        return false;
    }
    std::cout << "Launching default app: " << m_referenceAppId << std::endl;
    Core::hresult result = m_appManager->LaunchApp(m_referenceAppId, "", "");
    if (result != Core::ERROR_NONE) {
        std::cerr << "LaunchApp failed with error code: " << result << std::endl;
        return false;
    }
    return true;
}

bool SceneSetApp::killReferenceApp() {
    if (m_referenceAppId.empty()) {
        std::cout << "No reference app specified" << std::endl;
        return false;
    }
    if (m_appManager == nullptr) {
        std::cerr << "AppManager is not initialized" << std::endl;
        return false;
    }
    std::cout << "Killing reference app: " << m_referenceAppId << std::endl;
    Core::hresult result = m_appManager->KillApp(m_referenceAppId);
    if (result == Core::ERROR_NONE) {
        std::cout << "Successfully requested kill of reference app" << std::endl;
    }
    return true;
}

bool SceneSetApp::startPreinstall(bool forceInstall) {
    if (m_preinstallManager == nullptr) {
        std::cerr << "PreinstallManager is not initialized, cannot start preinstall." << std::endl;
        return false;
    }
    std::cout << "Starting preinstall with forceInstall=" << (forceInstall ? "true" : "false") << std::endl;
    Core::hresult result = m_preinstallManager->StartPreinstall(forceInstall);
    if (result != Core::ERROR_NONE) {
        std::cerr << "StartPreinstall failed with error code: " << result << std::endl;
        return false;
    }
    return true;
}

bool SceneSetApp::isReferenceAppInstalled() {
    if (m_referenceAppId.empty()) {
        std::cout << "No reference app ID specified" << std::endl;
        return false;
    }

    if (m_appManager == nullptr) {
        std::cerr << "AppManager is not initialized" << std::endl;
        return false;
    }

    std::string installedApps;
    Core::hresult result = m_appManager->GetInstalledApps(installedApps);

    if (result != Core::ERROR_NONE) {
        std::cerr << "GetInstalledApps failed with error code: " << result << std::endl;
        return false;
    }

    std::cout << "Installed apps: " << installedApps << std::endl;

    // Parse JSON array
    JsonArray apps;
    if (!apps.FromString(installedApps)) {
        std::cerr << "Failed to parse installed apps JSON response" << std::endl;
        return false;
    }

    // Iterate through the array to find reference app
    JsonArray::Iterator index = apps.Elements();
    while (index.Next()) {
        const JsonValue& element = index.Current();
        
        if (element.Content() == JsonValue::type::OBJECT) {
            JsonObject appObj = element.Object();
            
            if (appObj.HasLabel("appId")) {
                const JsonValue& appIdValue = appObj["appId"];
                if (appIdValue.Content() == JsonValue::type::STRING) {
                    std::string appId = appIdValue.String();
                    if (appId == m_referenceAppId) {
                        std::cout << "Reference app '" << m_referenceAppId << "' is already installed" << std::endl;
                        return true;
                    }
                }
            }
        }
    }

    std::cout << "Reference app '" << m_referenceAppId << "' is not installed yet" << std::endl;
    return false;
}

bool SceneSetApp::isFactoryAppsCopied() {
    if (std::filesystem::exists(FACTORY_APPS_COPIED_MARKER)) {
        std::cout << "Factory apps marker file exists at: " << FACTORY_APPS_COPIED_MARKER << std::endl;
        return true;
    }
    std::cout << "Factory apps marker file does not exist. This is the first boot." << std::endl;
    return false;
}

void SceneSetApp::markFactoryAppsCopied() {
    std::ofstream markerFile(FACTORY_APPS_COPIED_MARKER);
    if (markerFile.is_open()) {
        markerFile << "Factory apps copied on first boot" << std::endl;
        markerFile.close();
        std::cout << "Factory apps marker file created at: " << FACTORY_APPS_COPIED_MARKER << std::endl;
    } else {
        std::cerr << "Failed to create factory apps marker file at: " << FACTORY_APPS_COPIED_MARKER << std::endl;
    }
}

bool SceneSetApp::copyFactoryAppsToPreinstall() {
    namespace fs = std::filesystem;
    
    std::cout << "Copying factory apps from " << FACTORY_APP_PATH << " to " << APP_PREINSTALL_DIRECTORY << std::endl;

    fs::path sourcePath(FACTORY_APP_PATH);
    fs::path destPath(APP_PREINSTALL_DIRECTORY);

    if (!fs::exists(sourcePath)) {
        std::cerr << "Failed to open factory apps location: " << FACTORY_APP_PATH << std::endl;
        return false;
    }

    // Create preinstall directory if it doesn't exist
    if (!fs::exists(destPath)) {
        std::cout << "Creating preinstall directory: " << APP_PREINSTALL_DIRECTORY << std::endl;
        try {
            fs::create_directories(destPath);
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Failed to create preinstall directory: " << e.what() << std::endl;
            return false;
        }
    }

    // Copy bundle files from factory location to preinstall folder
    int fileCount = 0;

    try {
        for (const auto& entry : fs::directory_iterator(sourcePath)) {
            if (!fs::is_regular_file(entry.status())) {
                continue; // Skip directories and non-regular files
            }

            const std::string fileName = entry.path().filename().string();
            
            // Copy the bundle file directly to the preinstall directory
            fs::path destination = destPath / fileName;
            
            try {
                std::cout << "Copying bundle: " << fileName << " to preinstall directory" << std::endl;
                
                fs::copy_file(entry.path(), destination, 
                             fs::copy_options::overwrite_existing);
                fileCount++;
                std::cout << "Successfully copied bundle: " << fileName << std::endl;
                
            } catch (const fs::filesystem_error& e) {
                std::cerr << "Failed to copy bundle: " << fileName 
                          << " - " << e.what() << std::endl;
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error iterating directory: " << e.what() << std::endl;
        return false;
    }

    if (fileCount > 0) {
        std::cout << "Successfully copied " << fileCount << " factory app bundles to preinstall folder" << std::endl;
        markFactoryAppsCopied();
        return true;
    } else {
        std::cout << "No factory app bundles found to copy" << std::endl;
        markFactoryAppsCopied();
        return true;
    }
}

void SceneSetApp::cleanupPreinstallFolder() {
    namespace fs = std::filesystem;
    
    std::cout << "Cleaning up preinstall folder: " << APP_PREINSTALL_DIRECTORY << std::endl;
    
    const fs::path preinstallPath(APP_PREINSTALL_DIRECTORY);
    
    if (!fs::exists(preinstallPath)) {
        std::cout << "Preinstall directory does not exist, nothing to clean up" << std::endl;
        return;
    }
    
    try {
        int removedCount = 0;
        for (const auto& entry : fs::directory_iterator(preinstallPath)) {
            const auto entryName = entry.path().filename().string();
            
            try {
                if (const auto status = entry.status(); fs::is_directory(status)) {
                    std::cout << "Removing directory: " << entryName << std::endl;
                    fs::remove_all(entry.path());
                    removedCount++;
                    std::cout << "Successfully removed directory: " << entryName << std::endl;
                } else if (fs::is_regular_file(status)) {
                    std::cout << "Removing file: " << entryName << std::endl;
                    fs::remove(entry.path());
                    removedCount++;
                    std::cout << "Successfully removed file: " << entryName << std::endl;
                }
            } catch (const fs::filesystem_error& e) {
                std::cerr << "Failed to remove: " << entryName
                          << " - " << e.what() << std::endl;
            }
        }
        
        if (removedCount > 0) {
            std::cout << "Successfully cleaned up " << removedCount << " items from preinstall folder" << std::endl;
        } else {
            std::cout << "No items found to clean up in preinstall folder" << std::endl;
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error cleaning up preinstall folder: " << e.what() << std::endl;
    }
}

void SceneSetApp::checkAndLaunchIfAlreadyInstalled() {
    if (!m_appLaunched) {
        std::cout << "Checking if reference app is installed" << std::endl;
        if (isReferenceAppInstalled()) {
            bool expected = false;
            if (m_appLaunched.compare_exchange_strong(expected, true)) {
                std::cout << "Reference app '" << m_referenceAppId << "' is already installed. Launching default app." << std::endl;
                startLaunchThread();
            }
        } else {
            std::cout << "Reference app '" << m_referenceAppId << "' is not installed." << std::endl;
        }
    }
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
    m_act_cv.notify_one();
    unRegisterForAppEvents();
    unRegisterForPreinstallEvents();
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
    if (m_referenceAppId.empty()) {
        std::cout << "No reference app ID specified, skipping preinstall and app launch" << std::endl;
        return;
    }
    registerForPreinstallEvents();
    registerForAppEvents();

    // Determine if this is a Factory Setting Reset (FSR) / first boot scenario
    bool isFactoryReset = !isFactoryAppsCopied();
    
    // Copy factory apps to preinstall folder on first boot only
    if (isFactoryReset) {
        std::cout << "First boot/Factory reset detected. Copying factory apps to preinstall folder." << std::endl;
        if (!copyFactoryAppsToPreinstall()) {
            std::cerr << "Failed to copy factory apps. Continuing with preinstall anyway." << std::endl;
        }
    } else {
        std::cout << "Factory apps already copied on first boot. Skipping copy." << std::endl;
    }

    // Start preinstall - this is SYNCHRONOUS and BLOCKS until all bundles are installed
    // Use forceInstall=true for FSR cases (force reinstall all packages)
    // Use forceInstall=false for normal boots (only install if newer version)
    std::cout << "Starting preinstall process" << std::endl;
    if (startPreinstall(isFactoryReset)) {
        std::cout << "Preinstall process completed. Proceeding with cleaning up preinstall folder" << std::endl;
        // Clean up preinstall folder after preinstall succeeds
        cleanupPreinstallFolder();
    }

    // Check if reference app is installed and launch it
    checkAndLaunchIfAlreadyInstalled();

    waitForTermSignal();
}

// AppManagerEventHandler implementations
SceneSetApp::AppManagerEventHandler::~AppManagerEventHandler() {}

void SceneSetApp::AppManagerEventHandler::OnAppInstalled(const string &appId, const string &version) {
    std::cout << "App Installed: " << appId << " Version: " << version << std::endl;
    
    SceneSetApp& instance = SceneSetApp::getInstance();
    if (!instance.m_referenceAppId.empty() && appId == instance.m_referenceAppId) {
        if (instance.m_appLaunched) {
            std::cout << "New version of reference app '" << appId << "' (version: " << version << ") installed. App is running, killing and restarting reference app." << std::endl;
            // Kill the running app  before launching the new version
            // The lifecycle events will handle the state transitions
            instance.m_pendingRestart = true;
            if (instance.killReferenceApp()) {
                std::cout << "Kill requested. App will be restarted when it reaches UNLOADED state." << std::endl;
                // Note: The launch will be triggered by OnAppLifecycleStateChanged when app reaches UNLOADED
            } else {
                std::cerr << "Failed to kill reference app" << std::endl;
                instance.m_pendingRestart = false;
            }
        }
    }
}

void SceneSetApp::AppManagerEventHandler::OnAppUninstalled(const string &appId) {
    std::cout << "App Uninstalled: " << appId << std::endl;
}

void SceneSetApp::AppManagerEventHandler::OnAppLifecycleStateChanged(const string &appId, const string &appInstanceId, const Exchange::IAppManager::AppLifecycleState newState, const Exchange::IAppManager::AppLifecycleState oldState, const Exchange::IAppManager::AppErrorReason errorReason) {

    SceneSetApp& instance = SceneSetApp::getInstance();
    std::cout << "App Lifecycle State Changed for " << appId
              << " from " << getAppStateString(oldState) << " (" << static_cast<int>(oldState) << ")"
              << " to " << getAppStateString(newState) << " (" << static_cast<int>(newState) << ")" << std::endl;
    if (!instance.m_referenceAppId.empty() && appId == instance.m_referenceAppId) {
        // Track if reference app is running using m_appLaunched
        if (newState == Exchange::IAppManager::AppLifecycleState::APP_STATE_RUNNING ||
            newState == Exchange::IAppManager::AppLifecycleState::APP_STATE_ACTIVE) {
            instance.m_appLaunched = true;
        } else if (newState == Exchange::IAppManager::AppLifecycleState::APP_STATE_UNLOADED) {
            instance.m_appLaunched = false;
            
            // Check if we need to restart after new version installation
            if (instance.m_pendingRestart) {
                std::cout << "App reached UNLOADED state after new version installation. Restarting with new version." << std::endl;
                instance.m_pendingRestart = false;
                instance.startLaunchThread();
            }
            // Handle ABORT error case for crash restart (only if not pending restart from new version)
            else if (oldState == Exchange::IAppManager::AppLifecycleState::APP_STATE_TERMINATING &&
                     errorReason == Exchange::IAppManager::AppErrorReason::APP_ERROR_ABORT) {
                std::cout << "App " << appId << " terminated with ABORT error. Restarting reference app." << std::endl;
                instance.startLaunchThread();
            }
        } else if (newState == Exchange::IAppManager::AppLifecycleState::APP_STATE_TERMINATING) {
            instance.m_appLaunched = false;
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

// Launching the default app from a thread as this is also called from appmanager OnAppLifecycleStateChanged event handler
void SceneSetApp::startLaunchThread() {
    stopCurrentLaunchThread();

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

SceneSetApp::PreinstallManagerEventHandler::~PreinstallManagerEventHandler() {}

void SceneSetApp::PreinstallManagerEventHandler::OnComplete() {
    std::cout << "Received OnComplete event" << std::endl;
}

uint32_t SceneSetApp::PreinstallManagerEventHandler::AddRef() const {
    return Core::ERROR_NONE;
}

uint32_t SceneSetApp::PreinstallManagerEventHandler::Release() const {
    return Core::ERROR_NONE;
}
