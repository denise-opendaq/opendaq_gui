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
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_LIST_DIR}/MacOSXBundleInfo.plist.in"
    )

    # Use install RPATH at build time too so CMake never adds the build-tree
    # library directory to the binary's LC_RPATH. Without this, CMake tries to
    # strip that path during `cmake --install`, but macdeployqt (POST_BUILD) has
    # already modified the binary so the path is gone — causing install_name_tool errors.
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
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

    set(APP_BUNDLE "${CMAKE_BINARY_DIR}/bin/${PROJECT_NAME}.app")
    set(BIN_DIR "${CMAKE_BINARY_DIR}/bin")
    set(MACDEPLOYQT "${MACDEPLOYQT_EXECUTABLE}")
    configure_file(
        "${CMAKE_CURRENT_LIST_DIR}/prepare_app_bundle.cmake.in"
        "${_BUNDLE_PREP_SCRIPT}"
        @ONLY
    )

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -P "${_BUNDLE_PREP_SCRIPT}"
        COMMENT "Preparing app bundle for packaging"
        VERBATIM
    )

elseif(UNIX)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH "\$ORIGIN/../lib/opendaq-qt-gui:\$ORIGIN/../lib/opendaq-qt-gui/qt6"
        INSTALL_RPATH "\$ORIGIN/../lib/opendaq-qt-gui:\$ORIGIN/../lib/opendaq-qt-gui/qt6"
    )
endif()
