# Platform-specific bundle configuration. Included from CMakeLists.txt.

# ── macOS ──────────────────────────────────────────────────────────────────
if(APPLE)
    set(_APP_ICON_ICNS "${CMAKE_CURRENT_LIST_DIR}/logo/opendaq_qt_gui.icns")
    if(EXISTS "${_APP_ICON_ICNS}")
        set_source_files_properties("${_APP_ICON_ICNS}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
        target_sources(${PROJECT_NAME} PRIVATE "${_APP_ICON_ICNS}")
        set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_ICON_FILE "opendaq_qt_gui.icns")
    endif()

    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.opendaq.qt-gui"
        MACOSX_BUNDLE_BUNDLE_NAME "openDAQ Qt GUI"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_LIST_DIR}/packaging/mac/MacOSXBundleInfo.plist.in"
    )

    # During development the binary resolves dylibs from the build bin/ dir.
    # At install time CMake rewrites RPATH to @executable_path/../Frameworks,
    # after which the bundle-prep install script copies the dylibs there.
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH "${CMAKE_BINARY_DIR}/bin"
        INSTALL_RPATH "@executable_path/../Frameworks"
    )

    install(TARGETS ${PROJECT_NAME}
            BUNDLE DESTINATION "Applications"
            COMPONENT app)

    get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${_qt_bin_dir}")

    set(_BUNDLE_PREP_SCRIPT "${CMAKE_BINARY_DIR}/cmake/prepare_app_bundle.cmake")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/cmake")

    set(BIN_DIR "${CMAKE_BINARY_DIR}/bin")
    set(MACDEPLOYQT "${MACDEPLOYQT_EXECUTABLE}")
    configure_file(
        "${CMAKE_CURRENT_LIST_DIR}/packaging/mac/prepare_app_bundle.cmake.in"
        "${_BUNDLE_PREP_SCRIPT}"
        @ONLY
    )

    install(SCRIPT "${_BUNDLE_PREP_SCRIPT}" COMPONENT app)

elseif(UNIX)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH "${CMAKE_BINARY_DIR}/bin"
        INSTALL_RPATH "\$ORIGIN/../lib/opendaq-qt-gui:\$ORIGIN/../lib/opendaq-qt-gui/qt6"
    )

    # Install rules used by CPack (DEB) on Linux
    install(TARGETS ${PROJECT_NAME}
            RUNTIME DESTINATION "bin"
            COMPONENT app)

    if(TARGET Qt6::qmake)
        get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
        execute_process(
            COMMAND "${_qmake_executable}" -query QT_INSTALL_LIBS
            OUTPUT_VARIABLE _qt_lib_dir
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        execute_process(
            COMMAND "${_qmake_executable}" -query QT_INSTALL_PLUGINS
            OUTPUT_VARIABLE _qt_plugins_dir
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        set(_LINUX_INSTALL_SCRIPT "${CMAKE_BINARY_DIR}/cmake/packaging/linux/install_linux_bundle.cmake")
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/cmake/packaging/linux")

        set(APP_NAME "opendaq-qt-gui")
        set(PROJECT_BINARY_NAME "opendaq_qt_gui")
        set(QT_LIB_DIR "${_qt_lib_dir}")
        set(QT_PLUGINS_DIR "${_qt_plugins_dir}")
        set(BUILD_BIN_DIR "${CMAKE_BINARY_DIR}/bin")

        configure_file(
            "${CMAKE_CURRENT_LIST_DIR}/packaging/linux/install_linux_bundle.cmake.in"
            "${_LINUX_INSTALL_SCRIPT}"
            @ONLY
        )

        install(SCRIPT "${_LINUX_INSTALL_SCRIPT}" COMPONENT app)
    endif()

    # Desktop integration (icon + .desktop)
    set(_LINUX_ICON_PNG "${CMAKE_CURRENT_LIST_DIR}/logo/logo.png")
    if(EXISTS "${_LINUX_ICON_PNG}")
        install(FILES "${_LINUX_ICON_PNG}"
                DESTINATION "share/icons/hicolor/256x256/apps"
                RENAME "opendaq-qt-gui.png"
                COMPONENT app)
    endif()

    configure_file(
        "${CMAKE_CURRENT_LIST_DIR}/packaging/linux/opendaq-qt-gui.desktop.in"
        "${CMAKE_BINARY_DIR}/cmake/packaging/linux/opendaq-qt-gui.desktop"
        @ONLY
    )
    install(FILES "${CMAKE_BINARY_DIR}/cmake/packaging/linux/opendaq-qt-gui.desktop"
            DESTINATION "share/applications"
            COMPONENT app)

elseif(WIN32)

    # Embed the app icon into the .exe (shows in Explorer, taskbar, Alt+Tab)
    set(_WIN_ICON_RC "${CMAKE_CURRENT_LIST_DIR}/logo/opendaq_qt_gui.rc")
    if(EXISTS "${_WIN_ICON_RC}")
        target_sources(${PROJECT_NAME} PRIVATE "${_WIN_ICON_RC}")
    endif()

    install(TARGETS ${PROJECT_NAME}
            RUNTIME DESTINATION "bin"
            COMPONENT app)

    # NSIS installer icon (also uses the same .ico via the installed .exe)
    set(_WIN_ICON_ICO "${CMAKE_CURRENT_LIST_DIR}/logo/logo.ico")
    if(EXISTS "${_WIN_ICON_ICO}")
        set(CPACK_NSIS_MUI_ICON           "${_WIN_ICON_ICO}")
        set(CPACK_NSIS_MUI_UNIICON        "${_WIN_ICON_ICO}")
        set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\opendaq_qt_gui.exe")
    endif()

    if(TARGET Qt6::qmake)
        get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
        get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
        find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

        if(WINDEPLOYQT_EXECUTABLE)
            set(_WIN_INSTALL_SCRIPT "${CMAKE_BINARY_DIR}/cmake/packaging/windows/install_windows_bundle.cmake")
            file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/cmake/packaging/windows")

            set(APP_NAME "OpenDAQ Qt GUI")
            set(PROJECT_BINARY_NAME "opendaq_qt_gui")
            set(BUILD_BIN_DIR "${CMAKE_BINARY_DIR}/bin")
            set(WINDEPLOYQT "${WINDEPLOYQT_EXECUTABLE}")

            configure_file(
                "${CMAKE_CURRENT_LIST_DIR}/packaging/windows/install_windows_bundle.cmake.in"
                "${_WIN_INSTALL_SCRIPT}"
                @ONLY
            )

            install(SCRIPT "${_WIN_INSTALL_SCRIPT}" COMPONENT app)
        endif()
    endif()
endif()
