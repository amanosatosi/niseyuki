cmake_minimum_required(VERSION 3.21)
project(niseyuki LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)

qt_standard_project_setup()

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/Encoder.cpp
    src/widgets/StartButton.cpp
)

set(HEADERS
    src/MainWindow.h
    src/Encoder.h
    src/EncodeJob.h
    src/widgets/StartButton.h
)

qt_add_executable(niseyuki
    ${SOURCES}
    ${HEADERS}
)

qt_add_resources(niseyuki "sfx"
    PREFIX "/sfx"
    FILES
        audio/done_maou_se_system35.wav
        audio/failed_maou_se_system08.wav
        audio/paused_maou_se_system06.wav
)

target_include_directories(niseyuki PRIVATE src)

qt_finalize_executable(niseyuki)
