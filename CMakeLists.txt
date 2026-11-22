cmake_minimum_required(VERSION 3.21.1)
project(Quaderno VERSION 3.0.0 LANGUAGES CXX)

set_property(SOURCE src/main.cpp PROPERTY COMPILE_DEFINITIONS "QUADERNO_VERSION=\"${PROJECT_VERSION}\"")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2 DBus Network)
find_package(Qt6 6.5 REQUIRED COMPONENTS LinguistTools)
find_package(OpenSSL 3 REQUIRED COMPONENTS SSL)
find_package(ZLIB REQUIRED)
qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(quaderno MACOSX_BUNDLE src/main.cpp)
target_include_directories(quaderno PRIVATE src)
target_link_libraries(quaderno PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::DBus Qt6::Network OpenSSL::SSL ZLIB::ZLIB)

set(qml_sources qml/Main.qml qml/PaperButton.qml qml/PaperIcon.qml qml/SettingsView.qml qml/TabletSettingsPanel.qml qml/AccountsPanel.qml
    qml/CloudAccountsDialog.qml qml/CloudImportDialog.qml qml/QuickControlsPopup.qml qml/ReadingPreferencesDialog.qml qml/SyncAutomation.qml qml/AppleShareView.qml qml/SmtpDialog.qml
    qml/TouchKeyboard.qml qml/PinScreen.qml qml/PinKeypad.qml qml/PageDecorations.qml
    qml/PageTemplateDialog.qml qml/PageItemDialog.qml qml/TableHandwritingDialog.qml qml/PageOverview.qml qml/NotebookPageStrip.qml qml/LibraryOrganizer.qml
    qml/PageLayers.qml qml/PageTags.qml qml/ChartDialog.qml qml/ReadingControls.qml qml/ReadingGestures.qml
    qml/InkSelection.qml qml/SelectionOverlay.qml qml/UsbTransferDialog.qml qml/UsbImportDialog.qml qml/TableChartDialog.qml
    qml/WirelessSyncDialog.qml qml/ScreenStreamWindow.qml qml/RemoteInput.qml qml/DocumentDropArea.qml qml/PdfDragHandle.qml qml/MarkdownDialog.qml qml/MarkdownEditor.qml
    qml/ObsidianSyncDialog.qml qml/DiagramEditor.qml qml/DiagramHandwritingDialog.qml qml/DiagramArrangement.js
    qml/WorkspaceTools.qml qml/BookNoteDialog.qml qml/LinkTargetDialog.qml qml/MixedPageDialog.qml)
get_target_property(qt_disabled_gui_features Qt6::Gui QT_DISABLED_PUBLIC_FEATURES)
foreach(qml_file IN LISTS qml_sources)
    get_filename_component(qml_name "${qml_file}" NAME)
    if("accessibility" IN_LIST qt_disabled_gui_features)
        # The Paper Pro firmware omits Qt's Accessible attached object.
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${qml_file}")
        file(READ "${qml_file}" qml_contents)
        string(REGEX REPLACE "\n[ \t]*Accessible\\.(name|description|ignored|role|checkable|checked|onPressAction):[^\n]*" "" qml_contents "${qml_contents}")
        if(qml_contents MATCHES "Accessible\\.")
            message(FATAL_ERROR "${qml_file}: put each Accessible binding on its own physical line so the firmware build can remove it.")
        endif()
        set(qml_file "${CMAKE_CURRENT_BINARY_DIR}/qml-portable/${qml_name}")
        file(WRITE "${qml_file}" "${qml_contents}")
    endif()
    set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_name}")
    list(APPEND qml_files "${qml_file}")
endforeach()

qt_add_qml_module(quaderno
    URI Quaderno
    SOURCES
        src/inkitem.cpp src/inkitem.h
        src/applanguage.cpp src/applanguage.h
        src/inkrender.cpp src/inkrender.h
        src/inkselection.cpp src/inkselection.h
        src/shaperecognition.cpp src/shaperecognition.h
        src/chartitem.cpp src/chartitem.h
        src/diagramitem.cpp src/diagramitem.h
        src/diagramrecognizer.cpp src/diagramrecognizer.h
        src/mixedpageanalyzer.cpp src/mixedpageanalyzer.h
        src/mixedpagegeometry.cpp src/mixedpagegeometry.h
        src/mixedpageregions.cpp src/mixedpageregions.h
        src/mixedpagetable.cpp src/mixedpagetable.h
        src/mixedpagetextink.cpp src/mixedpagetextink.h
        src/diagramexport.cpp src/diagramexport.h src/diagramimport.cpp
        src/chartrecognizer.cpp src/chartrecognizer.h
        src/handwriting.cpp src/handwriting.h
        src/pagecontent.cpp src/pagecontent.h
        src/pagetemplateitem.cpp src/pagetemplateitem.h
        src/pagepreviewitem.cpp src/pagepreviewitem.h
        src/pdfimport.cpp src/pdfimport.h
        src/epubimport.cpp src/epubimport.h
        src/documentreader.cpp src/documentreader.h
        src/notebook.cpp src/notebook.h
        src/pagestore.cpp src/pagestore.h src/syncpackage.cpp src/syncpackage.h
        src/tablecsv.cpp
        src/appleshare.cpp src/appleshare.h
        src/smtpclient.cpp src/smtpclient.h
        src/smtpmail.cpp src/smtpmail.h
        src/deviceinfo.cpp src/deviceinfo.h
        src/screenorientation.cpp src/screenorientation.h
        src/markerbattery.cpp src/markerbattery.h
        src/tabletpreferences.cpp src/tabletpreferences.h
        src/portabledocuments.cpp src/portabledocuments.h
        src/cloudaccounts.cpp src/cloudaccounts.h src/cloudprotocol.cpp src/cloudprotocol.h src/cloudstore.cpp src/cloudstore.h
        src/librarymaintenance.cpp src/librarymaintenance.h
        src/epaperrefresh.cpp src/epaperrefresh.h
        src/externalkeyboard.cpp src/externalkeyboard.h
        src/pinlock.cpp src/pinlock.h
        src/paperprostandby.cpp src/paperprostandby.h
        src/usbssh.cpp src/usbssh.h
        src/paperprosshaccess.cpp src/paperprosshaccess.h
        src/usbtransfer.cpp src/usbtransfer.h
        src/wifisettings.cpp src/wifisettings.h
        src/bluetoothsettings.cpp src/bluetoothsettings.h
        src/blesync.cpp src/blesync.h
        src/deltatransfer.cpp src/deltatransfer.h
        src/wirelesssync.cpp src/wirelesssync.h
        src/screencapture.cpp src/screencapture.h
        src/screenstream.cpp src/screenstream.h
        src/syncchannel.cpp src/syncchannel.h
        src/blepipe.cpp src/blepipe.h
        src/wifisynclink.cpp src/wifisynclink.h
        src/syncrendezvous.cpp src/syncrendezvous.h
        src/macdocumentexchange.cpp src/macdocumentexchange.h
        src/markdownweb.cpp src/markdownweb.h
        src/obsidianintegration.cpp src/obsidianintegration.h
        src/obsidiansync.cpp src/obsidiansync.h
    QML_FILES ${qml_files}
)

qt_add_translation(translation_files
    translations/quaderno_main_en.ts
    translations/quaderno_dialogs_en.ts
    translations/quaderno_cpp_en.ts
    translations/quaderno_notebook_en.ts
    translations/quaderno_sync_en.ts)
qt_add_resources(quaderno "translations" PREFIX "/translations"
    BASE "${CMAKE_CURRENT_BINARY_DIR}" FILES ${translation_files})

if(APPLE)
    enable_language(OBJCXX)
    set_target_properties(quaderno PROPERTIES OBJCXX_STANDARD 17 OBJCXX_STANDARD_REQUIRED ON
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.quaderno.desktop"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/src/Info.plist.in"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}")
    target_sources(quaderno PRIVATE src/appleshare_mac.mm src/pdftext_mac.mm src/blecentral_mac.mm src/blecentral.h)
    set_source_files_properties(src/appleshare_mac.mm src/pdftext_mac.mm src/blecentral_mac.mm PROPERTIES COMPILE_OPTIONS -fobjc-arc)
    find_library(security_framework Security REQUIRED)
    find_library(appkit_framework AppKit REQUIRED)
    find_library(uti_framework UniformTypeIdentifiers REQUIRED)
    find_library(coregraphics_framework CoreGraphics REQUIRED)
    find_library(pdfkit_framework PDFKit REQUIRED)
    find_library(bluetooth_framework CoreBluetooth REQUIRED)
    target_link_libraries(quaderno PRIVATE ${appkit_framework} ${uti_framework} ${coregraphics_framework}
        ${pdfkit_framework} ${bluetooth_framework} ${security_framework})

    find_program(xcrun_program xcrun)
    if(xcrun_program)
        execute_process(COMMAND "${xcrun_program}" --sdk macosx --show-sdk-version
            OUTPUT_VARIABLE mac_sdk_version OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(mac_sdk_version VERSION_GREATER_EQUAL 27)
            set(handwriting_helper "${CMAKE_CURRENT_BINARY_DIR}/quaderno-handwriting")
            set(handwriting_target_arch "${CMAKE_OSX_ARCHITECTURES}")
            if(NOT handwriting_target_arch OR handwriting_target_arch MATCHES ";")
                set(handwriting_target_arch "${CMAKE_HOST_SYSTEM_PROCESSOR}")
            endif()
            add_custom_command(OUTPUT "${handwriting_helper}"
                COMMAND "${xcrun_program}" --sdk macosx swiftc -parse-as-library -O
                    -target "${handwriting_target_arch}-apple-macos27.0"
                    -module-cache-path "${CMAKE_CURRENT_BINARY_DIR}/swift-module-cache"
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/handwriting_helper.swift" -o "${handwriting_helper}"
                DEPENDS src/handwriting_helper.swift VERBATIM)
            add_custom_target(quaderno_handwriting DEPENDS "${handwriting_helper}")
            add_dependencies(quaderno quaderno_handwriting)
            add_custom_command(TARGET quaderno POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${handwriting_helper}"
                    "$<TARGET_FILE_DIR:quaderno>/quaderno-handwriting" VERBATIM)
            set_property(TARGET quaderno APPEND PROPERTY LINK_DEPENDS "${handwriting_helper}")
        endif()
    endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(quaderno PRIVATE src/bleperipheral.cpp src/bleperipheral.h src/paperprobluetooth.h)
    target_link_libraries(quaderno PRIVATE ${CMAKE_DL_LIBS})
endif()

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
