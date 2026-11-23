cmake_minimum_required(VERSION 3.20)
project(OSD-AUDIO VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Multimedia Sql)

include_directories(include)

set(SOURCES
    src/main.cpp
    src/core/AudioMixer.cpp
    src/core/AudioChannel.cpp
    src/core/PriorityChannel.cpp
    src/core/BackgroundChannel.cpp
    src/core/HotkeyManager.cpp
    src/core/DuckingController.cpp
    src/core/ApplicationDucking.cpp
    src/core/TtsManager.cpp
    src/core/ProfileManager.cpp
    src/network/TikTokBridge.cpp
    src/gui/MainWindow.cpp
    src/gui/SoundDeckWidget.cpp
    src/gui/SoundCard.cpp
    src/gui/SoundCardEditor.cpp
    include/persistence/JsonPersistence.cpp
)

set(HEADERS
    include/core/AudioChannel.h
    include/core/AudioMixer.h
    include/core/PriorityChannel.h
    include/core/BackgroundChannel.h
    include/core/HotkeyManager.h
    include/core/DuckingController.h
    src/core/TtsManager.h
    include/core/ProfileManager.h
    include/interfaces/IStreamAdapter.h
    include/interfaces/IPersistence.h
    include/utils/ThreadSafeQueue.h
    include/utils/Repository.h
    include/persistence/JsonPersistence.h
    src/gui/MainWindow.h
    src/gui/SoundDeckWidget.h
    src/gui/SoundCard.h
    src/gui/SoundCardEditor.h
    src/network/TikTokBridge.h
    src/gui/Icon.h
)

qt6_wrap_cpp(MOC_SOURCES ${HEADERS})
qt6_add_resources(QRC_SOURCES resources/resources.qrc)

if(WIN32)
    set(WIN_RC resources/app.rc)
endif()

add_executable(OSD-AUDIO ${SOURCES} ${MOC_SOURCES} ${QRC_SOURCES} ${WIN_RC})
target_link_libraries(OSD-AUDIO Qt6::Core Qt6::Widgets Qt6::Multimedia Qt6::Sql)

# Ocultar ventana de consola en Windows
if(WIN32)
    target_link_options(OSD-AUDIO PRIVATE -mwindows)
    target_link_libraries(OSD-AUDIO ole32 uuid)
endif()

# Tests
enable_testing()
find_package(GTest QUIET)
if(GTest_FOUND)
    add_executable(test_mixer tests/test_mixer.cpp src/core/AudioMixer.cpp)
    target_link_libraries(test_mixer GTest::GTest GTest::Main Qt6::Core)
    add_test(NAME MixerTest COMMAND test_mixer)
endif()
