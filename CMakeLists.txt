cmake_minimum_required(VERSION 3.16)

project(dukto6 VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Widgets Qml)

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

qt_add_executable(appdukto6
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
)

qt_add_qml_module(appdukto6
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

qt_generate_deploy_qml_app_script(
    TARGET appdukto6
    OUTPUT_SCRIPT deploy_script
)

install(SCRIPT ${deploy_script})

qt_add_resources(appdukto6 "assets"
    PREFIX "/"
    FILES
        # Fonts
        src/assets/KGLikeASkyscraper.ttf
        src/assets/Klill-Light.ttf
        src/assets/LiberationSans-Regular.ttf

        # Images and Icons
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
)

set_target_properties(appdukto6 PROPERTIES
    QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appdukto6 PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appdukto6
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appdukto6
    PRIVATE Qt6::Quick
    PRIVATE Qt6::Widgets
    PRIVATE Qt6::Qml
)

# Optimize and strip binary for Linux
if (UNIX AND NOT APPLE)
    add_custom_command(TARGET appdukto6 POST_BUILD
        COMMAND ${CMAKE_STRIP} --strip-unneeded $<TARGET_FILE:appdukto6>
        COMMENT "Stripping binary to reduce size"
    )
endif()

include(GNUInstallDirs)
install(TARGETS appdukto6
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Declare basic package info
set(CPACK_PACKAGE_NAME    "Dukto")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Caleb <drmaangi@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Dukto - Simple file transfer tool for LAN")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/qaleb/dukto6")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

# Choose DEB as the generator
set(CPACK_GENERATOR "DEB")

# Optional: let dpkg-shlibdeps auto-generate runtime dependencies
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

# Optional: specify license file for CPack
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

# Include all CPack functionality
include(CPack)

# Linux desktop integration: install .desktop file and icons (does not affect Android, Windows, or macOS)
install(FILES debian/dukto.png DESTINATION share/icons/hicolor/128x128/apps RENAME dukto.png)
install(FILES debian/dukto.png DESTINATION share/icons/hicolor/256x256/apps RENAME dukto.png)
install(FILES debian/dukto.desktop DESTINATION share/applications RENAME dukto.desktop)

# Optionally, also install icon to /usr/share/pixmaps for compatibility
install(FILES debian/dukto.png DESTINATION share/pixmaps RENAME dukto.png)
