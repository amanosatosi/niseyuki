cmake_minimum_required(VERSION 3.21)
project(niseyuki VERSION 0.1.0 LANGUAGES CXX)

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
target_link_libraries(niseyuki PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
)


qt_finalize_executable(niseyuki)

include(GNUInstallDirs)

install(TARGETS niseyuki
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .
)

install(DIRECTORY audio/
        DESTINATION ${CMAKE_INSTALL_BINDIR}/sfx
        FILES_MATCHING
        PATTERN "*.wav")

set(CPACK_PACKAGE_NAME "Niseyuki")
set(CPACK_PACKAGE_VENDOR "Niseyuki Project")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-windows")
include(CPack)
