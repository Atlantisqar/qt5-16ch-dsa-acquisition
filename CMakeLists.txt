cmake_minimum_required(VERSION 3.16)

project(Dsa16ChAcquisition VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Svg Network)

set(APP_SOURCES
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/models/projectfilemodel.h
    src/models/projectfilemodel.cpp
    src/core/projectmanager.h
    src/core/projectmanager.cpp
    src/core/dsa16chtypes.h
    src/core/dsa16chtypes.cpp
    src/core/dsa16chdllloader.h
    src/core/dsa16chdllloader.cpp
    src/core/dsa16chdeviceservice.h
    src/core/dsa16chdeviceservice.cpp
    src/core/circularbuffer.h
    src/core/circularbuffer.cpp
    src/core/multichanneldatastore.h
    src/core/multichanneldatastore.cpp
    src/core/acquisitionservice.h
    src/core/acquisitionservice.cpp
    src/core/acquisitiontcpstreamservice.h
    src/core/acquisitiontcpstreamservice.cpp
    src/core/acquisitiontcpreceiverservice.h
    src/core/acquisitiontcpreceiverservice.cpp
    src/core/plotservice.h
    src/core/plotservice.cpp
    src/widgets/channelselectorwidget.h
    src/widgets/channelselectorwidget.cpp
    src/widgets/menubutton.h
    src/widgets/menubutton.cpp
    src/widgets/waveformview.h
    src/widgets/waveformview.cpp
    src/widgets/waveformgridwidget.h
    src/widgets/waveformgridwidget.cpp
    src/pages/homepage.h
    src/pages/homepage.cpp
    src/pages/settingspage.h
    src/pages/settingspage.cpp
    src/pages/acquisitionpage.h
    src/pages/acquisitionpage.cpp
    src/dialogs/newprojectdialog.h
    src/dialogs/newprojectdialog.cpp
    src/dialogs/aboutdialog.h
    src/dialogs/aboutdialog.cpp
    src/theme/designtokens.h
    src/theme/themehelper.h
    src/theme/themehelper.cpp
    resources/resources.qrc
)

if(WIN32)
    list(APPEND APP_SOURCES resources/app_icon.rc)
endif()

add_executable(${PROJECT_NAME} WIN32 ${APP_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/device_sdk
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt5::Widgets
    Qt5::Svg
    Qt5::Network
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()

if(WIN32)
    set(SDK_DLL_NAME atom_16ch_dsa_api.dll)
    set(SDK_DLL_CANDIDATE_1 ${CMAKE_SOURCE_DIR}/third_party/device_sdk/${SDK_DLL_NAME})
    set(SDK_DLL_CANDIDATE_2 ${CMAKE_SOURCE_DIR}/${SDK_DLL_NAME})
    if(EXISTS ${SDK_DLL_CANDIDATE_1})
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SDK_DLL_CANDIDATE_1}
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/${SDK_DLL_NAME}
        )
    elseif(EXISTS ${SDK_DLL_CANDIDATE_2})
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SDK_DLL_CANDIDATE_2}
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/${SDK_DLL_NAME}
        )
    endif()
endif()
