cmake_minimum_required(VERSION 3.16)

project(dukto VERSION 6.9.6 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required Qt6 modules
find_package(Qt6 REQUIRED COMPONENTS Core Quick Widgets Qml)

# Set Qt policies
qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0002 NEW)
qt_policy(SET QTP0004 NEW)

# Embed Windows icon in the executable
if(WIN32)
    set(app_icon_resource_windows "${CMAKE_CURRENT_SOURCE_DIR}/src/dukto.rc")
    set_source_files_properties(${app_icon_resource_windows} PROPERTIES LANGUAGE RC)
endif()

# Main source files
set(SOURCES
    src/buddylistitemmodel.cpp
    src/destinationbuddy.cpp
    src/duktoprotocol.cpp
    src/guibehind.cpp
    src/ipaddressitemmodel.cpp
    src/main.cpp
    src/miniwebserver.cpp
    src/platform.cpp
    src/recentlistitemmodel.cpp
    src/settings.cpp
    src/theme.cpp
    src/updateschecker.cpp
)

set(HEADERS
    src/buddylistitemmodel.h
    src/destinationbuddy.h
    src/duktoprotocol.h
    src/guibehind.h
    src/ipaddressitemmodel.h
    src/miniwebserver.h
    src/peer.h
    src/platform.h
    src/recentlistitemmodel.h
    src/settings.h
    src/theme.h
    src/updateschecker.h
    src/winhelper.h
)

# Append Windows-specific resource if on Windows
if(WIN32)
    list(APPEND SOURCES ${app_icon_resource_windows})
endif()

qt_add_executable(dukto6 ${SOURCES} ${HEADERS})

# Add QML module
qt_add_qml_module(dukto6
    URI dukto6
    QML_FILES
        src/qml/dukto6/AboutPage.qml
        src/qml/dukto6/BottomToolBar.qml
        src/qml/dukto6/BuddiesPage.qml
        src/qml/dukto6/BuddyListComponent.qml
        src/qml/dukto6/Button.qml
        src/qml/dukto6/ButtonDark.qml
        src/qml/dukto6/ColorBox.qml
        src/qml/dukto6/ColorPicker.qml
        src/qml/dukto6/ColorSlider.qml
        src/qml/dukto6/ColorUtils.js
        src/qml/dukto6/DialogNavBar.qml
        src/qml/dukto6/DuktoInner.qml
        src/qml/dukto6/DuktoOverlay.qml
        src/qml/dukto6/FileFolderDialog.qml
        src/qml/dukto6/IpPage.qml
        src/qml/dukto6/Main.qml
        src/qml/dukto6/MessagePage.qml
        src/qml/dukto6/ProgressPage.qml
        src/qml/dukto6/RecentPage.qml
        src/qml/dukto6/SBPicker.qml
        src/qml/dukto6/SendPage.qml
        src/qml/dukto6/SettingsPage.qml
        src/qml/dukto6/ShowTextPage.qml
        src/qml/dukto6/SmoothText.qml
        src/qml/dukto6/SText.qml
        src/qml/dukto6/STextInput.qml
        src/qml/dukto6/TermsPage.qml
        src/qml/dukto6/TopTabBar.qml
        src/qml/dukto6/UpdatesBox.qml
)

# Set Android package source directory
if(ANDROID)
    set_target_properties(dukto6 PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
    )
endif()

# macOS bundle settings (may be extended later)
set_target_properties(dukto6 PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# Define version macro for C++
target_compile_definitions(dukto6 PRIVATE APP_VERSION="${PROJECT_VERSION}")

# Link libraries
target_link_libraries(dukto6
    PRIVATE Qt6::Quick
    PRIVATE Qt6::Widgets
    PRIVATE Qt6::Qml
    PRIVATE Qt6::Core
)

# Linux: Strip binary to reduce size
if(UNIX AND NOT APPLE AND NOT ANDROID)
    add_custom_command(TARGET dukto6 POST_BUILD
        COMMAND ${CMAKE_STRIP} --strip-unneeded $<TARGET_FILE:dukto6>
        COMMENT "Stripping binary to reduce size"
    )
endif()

# Deploy QML app script only for non-Android platforms
if(NOT ANDROID)
    qt_generate_deploy_qml_app_script(
        TARGET dukto6
        OUTPUT_SCRIPT deploy_script
    )
    install(SCRIPT ${deploy_script})
endif()

# Assets (fonts, icons, etc.)
qt_add_resources(dukto6 "assets"
    PREFIX "/"
    FILES
        src/assets/KGLikeASkyscraper.ttf
        src/assets/Klill-Light.ttf
        src/assets/LiberationSans-Regular.ttf
        src/assets/Accept.svg
        src/assets/AndroidLogo.png
        src/assets/AppleLogo.png
        src/assets/ArrowBack.svg
        src/assets/BlackberryLogo.png
        src/assets/BottomShadow.png
        src/assets/Cancel.svg
        src/assets/dukto.png
        src/assets/DuktoMetroIcon.svg
        src/assets/file-fill-colored.svg
        src/assets/folder-fill-colored.svg
        src/assets/FolderUp.svg
        src/assets/IosLogo.png
        src/assets/IpLogo.png
        src/assets/LinuxLogo.png
        src/assets/OpenFolder.svg
        src/assets/PanelGradient.png
        src/assets/PcLogo.png
        src/assets/RecentFile.png
        src/assets/RecentFiles.png
        src/assets/RecentText.png
        src/assets/SettingsDark.svg
        src/assets/ShowIpDark.svg
        src/assets/SmartphoneLogo.png
        src/assets/SwipeDown.png
        src/assets/SwipeUp.png
        src/assets/SymbianLogo.png
        src/assets/TileGradient.png
        src/assets/TopShadow.png
        src/assets/UnknownLogo.png
        src/assets/WindowsLogo.png
        # src/dukto.rc
)

# Linux-specific desktop integration
if(UNIX AND NOT APPLE AND NOT ANDROID)
    install(FILES debian/dukto.png DESTINATION share/icons/hicolor/128x128/apps RENAME dukto.png)
    install(FILES debian/dukto.png DESTINATION share/icons/hicolor/256x256/apps RENAME dukto.png)
    install(FILES debian/dukto.desktop DESTINATION share/applications RENAME dukto.desktop)
    install(FILES debian/dukto.png DESTINATION share/pixmaps RENAME dukto.png)
endif()

# Installation rules
include(GNUInstallDirs)
install(TARGETS dukto6
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Packaging (DEB format for Linux)
if(UNIX AND NOT APPLE AND NOT ANDROID)
    set(CPACK_PACKAGE_NAME "Dukto")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Caleb <drmaangi@gmail.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Dukto - Simple file transfer tool for LAN")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/qaleb/dukto6")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libstdc++6, libgcc-s1")
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    include(CPack)
endif()

# On Linux, set the output directory for the executable to avoid conflict with the dukto6 directory
if(UNIX AND NOT APPLE AND NOT ANDROID)
    set_target_properties(dukto6 PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()
