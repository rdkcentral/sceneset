/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2025 RDK Management
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
**/

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstdlib>
#include <csignal>
#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "SceneSet.h"
#include "RalfPackageSupport.h"

namespace {
std::filesystem::path MakeUniqueTempPath(const std::string& prefix) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
    std::ostringstream name;
    name << prefix << "_" << now << "_" << tid;
    return std::filesystem::temp_directory_path() / name.str();
}
} // namespace

class SceneSetAppTestPeer {
public:
    static const std::string& GetReferenceAppId(const SceneSetApp& app) {
        return app.m_referenceAppId;
    }

    static void SetPreinstallDirectory(SceneSetApp& app, const std::string& preinstallDirectory) {
        app.m_preinstallDirectory = preinstallDirectory;
    }

    static bool MovePackageToPreinstallDirectory(SceneSetApp& app, const std::filesystem::path& sourcePath) {
        return app.movePackageToPreinstallDirectory(sourcePath);
    }

    static bool ShouldRunInitialDownloadSweep(const SceneSetApp& app) {
        return app.shouldRunInitialDownloadSweep();
    }

    static bool ProcessDownloadedPackage(SceneSetApp& app, const std::filesystem::path& packagePath) {
        return app.processDownloadedPackage(packagePath);
    }

    static void SetMetadataExtractorForTesting(bool (*extractor)(const std::filesystem::path&, std::string&, std::string&)) {
        SceneSetApp::setMetadataExtractorForTesting(extractor);
    }

    static void ResetMetadataExtractorForTesting() {
        SceneSetApp::resetMetadataExtractorForTesting();
    }
};

// Thread-local storage for the reference app id to be returned by the fake extractor
thread_local std::string g_fakeExtractorRefAppId;

bool FakeExtractMetadataSuccess(const std::filesystem::path&,
                               std::string& appId,
                               std::string& version) {
    appId = g_fakeExtractorRefAppId;
    version = "1.0.0";
    return true;
}

bool FakeExtractMetadataFail(const std::filesystem::path&,
                             std::string&,
                             std::string&) {
    return false;
}

bool FakeExtractMetadataWrongAppId(const std::filesystem::path&,
                                   std::string& appId,
                                   std::string& version) {
    appId = "com.completely.different.app.not.the.reference";
    version = "1.0.0";
    return true;
}

class MetadataExtractorResetGuard {
public:
    MetadataExtractorResetGuard() = default;
    ~MetadataExtractorResetGuard() {
        SceneSetAppTestPeer::ResetMetadataExtractorForTesting();
    }
    MetadataExtractorResetGuard(const MetadataExtractorResetGuard&) = delete;
    MetadataExtractorResetGuard& operator=(const MetadataExtractorResetGuard&) = delete;
};


class SceneSetTest : public ::testing::Test {
protected:
    SceneSetTest() = default;
    virtual ~SceneSetTest() = default;

    void SetUp() override {
        // Set environment variables for testing
        setenv("THUNDER_ACCESS", "/tmp/communicator", 1);
        setenv("SCENESET_DEFAULT_APPNAME", "TestApp", 1);
    }

    void TearDown() override {
        // Clean up environment variables
        unsetenv("THUNDER_ACCESS");
        unsetenv("SCENESET_DEFAULT_APPNAME");
        unsetenv("SCENESET_INITIAL_DOWNLOAD_SWEEP");
    }
};

// Test SceneSetApp singleton pattern
TEST_F(SceneSetTest, SingletonPattern) {
    SceneSetApp& instance1 = SceneSetApp::getInstance();
    SceneSetApp& instance2 = SceneSetApp::getInstance();

    EXPECT_EQ(&instance1, &instance2);
}

// Test SceneSetApp constructor and destructor
TEST_F(SceneSetTest, ConstructorDestructor) {
    EXPECT_NO_THROW({
        SceneSetApp app;
    });
}


// Test AppManagerEventHandler functionality
TEST_F(SceneSetTest, AppManagerEventHandlerCreation) {
    SceneSetApp app;
    EXPECT_NO_THROW({
        app.registerForAppEvents();
    });
}

// Test launching default app without environment variable
TEST_F(SceneSetTest, LaunchDefaultAppWithoutEnvVar) {
    unsetenv("SCENESET_DEFAULT_APPNAME");
    SceneSetApp app;

    bool result = app.launchDefaultApp();
    EXPECT_FALSE(result);
}


// Test initialization without Thunder connection
TEST_F(SceneSetTest, InitializationWithoutThunder) {
    setenv("THUNDER_ACCESS", "/invalid/path", 1);
    SceneSetApp app;

    bool result = app.initialize();
    EXPECT_FALSE(result);
}

// Test termination signal handling
TEST_F(SceneSetTest, TerminationSignalHandling) {
    // Test static signal handler
    EXPECT_NO_THROW({
        SceneSetApp::handleTerminationSignal(SIGTERM);
    });
}

// Test thread safety and concurrent operations
TEST_F(SceneSetTest, ThreadSafety) {
    SceneSetApp& app = SceneSetApp::getInstance();

    // Test that multiple threads can access getInstance safely
    std::vector<std::thread> threads;
    std::vector<SceneSetApp*> instances(10);

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&instances, i]() {
            instances[i] = &SceneSetApp::getInstance();
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // All instances should be the same
    for (int i = 1; i < 10; ++i) {
        EXPECT_EQ(instances[0], instances[i]);
    }
}

// Test onTerminate functionality
TEST_F(SceneSetTest, OnTerminate) {
    SceneSetApp app;
    EXPECT_NO_THROW({
        app.onTerminate();
    });
}

// Test unregister functionality
TEST_F(SceneSetTest, UnregisterForAppEvents) {
    SceneSetApp app;
    bool result = app.unRegisterForAppEvents();
    EXPECT_FALSE(result);
}

TEST_F(SceneSetTest, ExtractPackageMetadataReturnsFalseForMissingCertDirectory) {
    std::string appId;
    std::string version;

    const std::filesystem::path missingCertDir = MakeUniqueTempPath("sceneset_missing_cert_dir");
    const std::filesystem::path fakePackage = missingCertDir / "fake.bolt";

    std::error_code ec;
    std::filesystem::remove_all(missingCertDir, ec);

    EXPECT_NO_THROW({
        const bool result = ralf_support::ExtractPackageMetadata(fakePackage, missingCertDir, appId, version);
        EXPECT_FALSE(result);
    });

    std::filesystem::remove_all(missingCertDir, ec);
}

TEST_F(SceneSetTest, ExtractPackageMetadataReturnsFalseWhenCertPathIsNotDirectory) {
    std::string appId;
    std::string version;

    const std::filesystem::path certPathFile = MakeUniqueTempPath("sceneset_cert_path_file");
    const std::filesystem::path fakePackage = certPathFile.parent_path() / "fake.bolt";

    {
        std::ofstream file(certPathFile);
        file << "not a certificate directory" << std::endl;
    }

    EXPECT_NO_THROW({
        const bool result = ralf_support::ExtractPackageMetadata(fakePackage, certPathFile, appId, version);
        EXPECT_FALSE(result);
    });

    std::error_code ec;
    std::filesystem::remove_all(certPathFile, ec);
}

TEST_F(SceneSetTest, MovePackageToPreinstallDirectoryOverwritesExistingFile) {
    SceneSetApp app;
    const auto rootDir = MakeUniqueTempPath("sceneset_stage_overwrite");
    const auto srcDir = rootDir / "download";
    const auto dstDir = rootDir / "preinstall";
    const auto srcFile = srcDir / "bundle.bolt";
    const auto dstFile = dstDir / "bundle.bolt";

    std::error_code ec;
    std::filesystem::create_directories(srcDir, ec);
    std::filesystem::create_directories(dstDir, ec);
    ASSERT_FALSE(ec);

    {
        std::ofstream source(srcFile);
        source << "new_payload";
    }
    {
        std::ofstream destination(dstFile);
        destination << "old_payload";
    }

    SceneSetAppTestPeer::SetPreinstallDirectory(app, dstDir.string());
    const bool result = SceneSetAppTestPeer::MovePackageToPreinstallDirectory(app, srcFile);
    EXPECT_TRUE(result);
    EXPECT_FALSE(std::filesystem::exists(srcFile));
    EXPECT_TRUE(std::filesystem::exists(dstFile));

    std::ifstream finalFile(dstFile);
    std::string finalContent;
    std::getline(finalFile, finalContent);
    EXPECT_EQ(finalContent, "new_payload");

    std::filesystem::remove_all(rootDir, ec);
}

TEST_F(SceneSetTest, MovePackageToPreinstallDirectoryReturnsFalseForMissingSource) {
    SceneSetApp app;
    const auto rootDir = MakeUniqueTempPath("sceneset_stage_missing_source");
    const auto dstDir = rootDir / "preinstall";
    const auto missingSource = rootDir / "download" / "missing.pkg";

    std::error_code ec;
    std::filesystem::create_directories(dstDir, ec);
    ASSERT_FALSE(ec);

    SceneSetAppTestPeer::SetPreinstallDirectory(app, dstDir.string());
    const bool result = SceneSetAppTestPeer::MovePackageToPreinstallDirectory(app, missingSource);
    EXPECT_FALSE(result);

    std::filesystem::remove_all(rootDir, ec);
}

TEST_F(SceneSetTest, InitialDownloadSweepDecisionHonorsEnvironmentFlag) {
    SceneSetApp app;

    unsetenv("SCENESET_INITIAL_DOWNLOAD_SWEEP");
    EXPECT_FALSE(SceneSetAppTestPeer::ShouldRunInitialDownloadSweep(app));

    setenv("SCENESET_INITIAL_DOWNLOAD_SWEEP", "1", 1);
    EXPECT_TRUE(SceneSetAppTestPeer::ShouldRunInitialDownloadSweep(app));

    setenv("SCENESET_INITIAL_DOWNLOAD_SWEEP", "off", 1);
    EXPECT_FALSE(SceneSetAppTestPeer::ShouldRunInitialDownloadSweep(app));
}

TEST_F(SceneSetTest, ProcessDownloadedPackageStagesReferenceBundleWithInjectedMetadata) {
    SceneSetApp app;
    const auto rootDir = MakeUniqueTempPath("sceneset_process_downloaded_package");
    const auto srcDir = rootDir / "download";
    const auto dstDir = rootDir / "preinstall";
    const auto srcFile = srcDir / "bundle.bolt";
    const auto dstFile = dstDir / "bundle.bolt";

    std::error_code ec;
    std::filesystem::create_directories(srcDir, ec);
    std::filesystem::create_directories(dstDir, ec);
    ASSERT_FALSE(ec);

    {
        std::ofstream source(srcFile);
        source << "payload";
    }

    SceneSetAppTestPeer::SetPreinstallDirectory(app, dstDir.string());
    const std::string& referenceAppId = SceneSetAppTestPeer::GetReferenceAppId(app);
    if (referenceAppId.empty()) {
        GTEST_SKIP() << "SceneSet reference app id is empty; SCENESET_DEFAULT_APPNAME is not configured for this build.";
    }
    g_fakeExtractorRefAppId = referenceAppId;
    SceneSetAppTestPeer::SetMetadataExtractorForTesting(&FakeExtractMetadataSuccess);
    MetadataExtractorResetGuard metadataExtractorResetGuard;

    const bool result = SceneSetAppTestPeer::ProcessDownloadedPackage(app, srcFile);
    EXPECT_TRUE(result);
    EXPECT_FALSE(std::filesystem::exists(srcFile));
    EXPECT_TRUE(std::filesystem::exists(dstFile));

    std::filesystem::remove_all(rootDir, ec);
}

// Test: processDownloadedPackage returns false when metadata extraction fails
TEST_F(SceneSetTest, ProcessDownloadedPackageReturnsFalseWhenMetadataExtractionFails) {
    SceneSetApp app;
    const auto rootDir = MakeUniqueTempPath("sceneset_process_fail_extract");
    const auto srcDir = rootDir / "download";
    const auto srcFile = srcDir / "bundle.bolt";

    std::error_code ec;
    std::filesystem::create_directories(srcDir, ec);
    ASSERT_FALSE(ec);

    {
        std::ofstream source(srcFile);
        source << "payload";
    }

    SceneSetAppTestPeer::SetMetadataExtractorForTesting(&FakeExtractMetadataFail);
    MetadataExtractorResetGuard guard;

    const bool result = SceneSetAppTestPeer::ProcessDownloadedPackage(app, srcFile);
    EXPECT_FALSE(result);

    std::filesystem::remove_all(rootDir, ec);
}

// Test: processDownloadedPackage returns false when extracted appId doesn't match reference app
TEST_F(SceneSetTest, ProcessDownloadedPackageReturnsFalseWhenAppIdDoesNotMatch) {
    SceneSetApp app;
    const auto rootDir = MakeUniqueTempPath("sceneset_process_wrong_appid");
    const auto srcDir = rootDir / "download";
    const auto srcFile = srcDir / "bundle.bolt";

    std::error_code ec;
    std::filesystem::create_directories(srcDir, ec);
    ASSERT_FALSE(ec);

    {
        std::ofstream source(srcFile);
        source << "payload";
    }

    SceneSetAppTestPeer::SetMetadataExtractorForTesting(&FakeExtractMetadataWrongAppId);
    MetadataExtractorResetGuard guard;

    const bool result = SceneSetAppTestPeer::ProcessDownloadedPackage(app, srcFile);
    EXPECT_FALSE(result);

    std::filesystem::remove_all(rootDir, ec);
}

// Test: MovePackageToPreinstallDirectory creates the preinstall directory when it does not exist
TEST_F(SceneSetTest, MovePackageToPreinstallDirectoryCreatesDirectoryIfAbsent) {
    SceneSetApp app;
    const auto rootDir = MakeUniqueTempPath("sceneset_stage_create_dir");
    const auto srcDir = rootDir / "download";
    const auto dstDir = rootDir / "preinstall"; // intentionally not created
    const auto srcFile = srcDir / "bundle.bolt";

    std::error_code ec;
    std::filesystem::create_directories(srcDir, ec);
    ASSERT_FALSE(ec);

    {
        std::ofstream source(srcFile);
        source << "payload";
    }

    SceneSetAppTestPeer::SetPreinstallDirectory(app, dstDir.string());
    const bool result = SceneSetAppTestPeer::MovePackageToPreinstallDirectory(app, srcFile);
    EXPECT_TRUE(result);
    EXPECT_TRUE(std::filesystem::exists(dstDir / "bundle.bolt"));
    EXPECT_FALSE(std::filesystem::exists(srcFile));

    std::filesystem::remove_all(rootDir, ec);
}

// Test: MovePackageToPreinstallDirectory returns false when preinstall directory is empty
TEST_F(SceneSetTest, MovePackageToPreinstallDirectoryReturnsFalseForEmptyPreinstallDir) {
    SceneSetApp app;
    const auto rootDir = MakeUniqueTempPath("sceneset_stage_empty_preinstall_dir");
    const auto srcFile = rootDir / "bundle.bolt";

    std::error_code ec;
    std::filesystem::create_directories(rootDir, ec);
    ASSERT_FALSE(ec);

    {
        std::ofstream source(srcFile);
        source << "payload";
    }

    SceneSetAppTestPeer::SetPreinstallDirectory(app, "");
    const bool result = SceneSetAppTestPeer::MovePackageToPreinstallDirectory(app, srcFile);
    EXPECT_FALSE(result);

    std::filesystem::remove_all(rootDir, ec);
}

// Test: registerForPreinstallEvents returns false when preinstall manager is not initialized
TEST_F(SceneSetTest, RegisterForPreinstallEventsReturnsFalseWithoutManager) {
    SceneSetApp app;
    bool result = app.registerForPreinstallEvents();
    EXPECT_FALSE(result);
}

// Test: registerForPackageInstallerEvents returns false when package installer is not initialized
TEST_F(SceneSetTest, RegisterForPackageInstallerEventsReturnsFalseWithoutInstaller) {
    SceneSetApp app;
    bool result = app.registerForPackageInstallerEvents();
    EXPECT_FALSE(result);
}

// Test: unRegisterForPreinstallEvents always returns true regardless of manager state
TEST_F(SceneSetTest, UnregisterForPreinstallEventsReturnsTrueWithoutManager) {
    SceneSetApp app;
    bool result = app.unRegisterForPreinstallEvents();
    EXPECT_TRUE(result);
}

// Test: unRegisterForPackageInstallerEvents always returns true regardless of installer state
TEST_F(SceneSetTest, UnregisterForPackageInstallerEventsReturnsTrueWithoutInstaller) {
    SceneSetApp app;
    bool result = app.unRegisterForPackageInstallerEvents();
    EXPECT_TRUE(result);
}

// Test: killReferenceApp returns false when reference app id is empty
TEST_F(SceneSetTest, KillReferenceAppReturnsFalseWhenReferenceAppIdEmpty) {
    unsetenv("SCENESET_DEFAULT_APPNAME");
    SceneSetApp app;
    bool result = app.killReferenceApp();
    EXPECT_FALSE(result);
}

// Test: killReferenceApp returns false when AppManager is not initialized
TEST_F(SceneSetTest, KillReferenceAppReturnsFalseWhenAppManagerNotInitialized) {
    // SCENESET_DEFAULT_APPNAME is set to "TestApp" by SetUp so m_referenceAppId is non-empty
    SceneSetApp app;
    // m_appManager is null because initialize() was not called
    bool result = app.killReferenceApp();
    EXPECT_FALSE(result);
}

// Test: startPreinstall returns false when preinstall manager is not initialized
TEST_F(SceneSetTest, StartPreinstallReturnsFalseWithoutPreinstallManager) {
    SceneSetApp app;
    bool result = app.startPreinstall(false);
    EXPECT_FALSE(result);
}

// Test: isReferenceAppInstalled returns false when reference app id is empty
TEST_F(SceneSetTest, IsReferenceAppInstalledReturnsFalseForEmptyAppId) {
    unsetenv("SCENESET_DEFAULT_APPNAME");
    SceneSetApp app;
    bool result = app.isReferenceAppInstalled();
    EXPECT_FALSE(result);
}

// Test: isReferenceAppInstalled returns false when AppManager is not initialized
TEST_F(SceneSetTest, IsReferenceAppInstalledReturnsFalseWithoutAppManager) {
    // SCENESET_DEFAULT_APPNAME is set to "TestApp" by SetUp so m_referenceAppId is non-empty
    SceneSetApp app;
    // m_appManager is null because initialize() was not called
    bool result = app.isReferenceAppInstalled();
    EXPECT_FALSE(result);
}

// Test: reference app ID is sourced from SCENESET_DEFAULT_APPNAME environment variable
TEST_F(SceneSetTest, ReferenceAppIdMatchesEnvVar) {
    setenv("SCENESET_DEFAULT_APPNAME", "MyReferenceApp", 1);
    SceneSetApp app;
    EXPECT_EQ(SceneSetAppTestPeer::GetReferenceAppId(app), "MyReferenceApp");
}

// Test: shouldRunInitialDownloadSweep returns true for various truthy env var values
TEST_F(SceneSetTest, InitialDownloadSweepEnabledWithVariousTruthyValues) {
    SceneSetApp app;

    setenv("SCENESET_INITIAL_DOWNLOAD_SWEEP", "true", 1);
    EXPECT_TRUE(SceneSetAppTestPeer::ShouldRunInitialDownloadSweep(app));

    setenv("SCENESET_INITIAL_DOWNLOAD_SWEEP", "yes", 1);
    EXPECT_TRUE(SceneSetAppTestPeer::ShouldRunInitialDownloadSweep(app));

    setenv("SCENESET_INITIAL_DOWNLOAD_SWEEP", "on", 1);
    EXPECT_TRUE(SceneSetAppTestPeer::ShouldRunInitialDownloadSweep(app));
}

// Test: shouldRunInitialDownloadSweep returns false for various falsy env var values
TEST_F(SceneSetTest, InitialDownloadSweepDisabledWithVariousFalsyValues) {
    SceneSetApp app;

    setenv("SCENESET_INITIAL_DOWNLOAD_SWEEP", "false", 1);
    EXPECT_FALSE(SceneSetAppTestPeer::ShouldRunInitialDownloadSweep(app));

    setenv("SCENESET_INITIAL_DOWNLOAD_SWEEP", "no", 1);
    EXPECT_FALSE(SceneSetAppTestPeer::ShouldRunInitialDownloadSweep(app));

    setenv("SCENESET_INITIAL_DOWNLOAD_SWEEP", "0", 1);
    EXPECT_FALSE(SceneSetAppTestPeer::ShouldRunInitialDownloadSweep(app));
}

// Test: cleanupPreinstallFolder does nothing and does not throw for a non-existent directory
TEST_F(SceneSetTest, CleanupPreinstallFolderDoesNothingForNonExistentDir) {
    SceneSetApp app;
    const auto nonExistentDir = MakeUniqueTempPath("sceneset_cleanup_nonexistent");

    std::error_code ec;
    std::filesystem::remove_all(nonExistentDir, ec);

    SceneSetAppTestPeer::SetPreinstallDirectory(app, nonExistentDir.string());
    EXPECT_NO_THROW({
        app.cleanupPreinstallFolder();
    });
}

// Test: cleanupPreinstallFolder removes all files and subdirectories inside the preinstall folder
TEST_F(SceneSetTest, CleanupPreinstallFolderRemovesContents) {
    SceneSetApp app;
    const auto preinstallDir = MakeUniqueTempPath("sceneset_cleanup_content");

    std::error_code ec;
    std::filesystem::create_directories(preinstallDir, ec);
    ASSERT_FALSE(ec);

    {
        std::ofstream f(preinstallDir / "file1.bolt");
        f << "data";
    }
    {
        std::ofstream f(preinstallDir / "file2.bolt");
        f << "data";
    }
    std::filesystem::create_directories(preinstallDir / "subdir", ec);
    ASSERT_FALSE(ec);

    SceneSetAppTestPeer::SetPreinstallDirectory(app, preinstallDir.string());
    EXPECT_NO_THROW({
        app.cleanupPreinstallFolder();
    });

    EXPECT_TRUE(std::filesystem::exists(preinstallDir));
    EXPECT_TRUE(std::filesystem::is_empty(preinstallDir));

    std::filesystem::remove_all(preinstallDir, ec);
}

// Test: isFactoryAppsCopied returns false when the marker file does not exist
TEST_F(SceneSetTest, IsFactoryAppsCopiedReturnsFalseWhenMarkerAbsent) {
    if (std::filesystem::exists("/opt/persistent/.sceneset_factory_apps_copied")) {
        GTEST_SKIP() << "Marker file exists on this system; skipping test.";
    }
    SceneSetApp app;
    EXPECT_FALSE(app.isFactoryAppsCopied());
}
