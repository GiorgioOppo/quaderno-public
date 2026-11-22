find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

add_library(quaderno_test_core STATIC
    ../src/notebook.cpp ../src/notebook.h ../src/tablecsv.cpp
    ../src/portabledocuments.cpp ../src/portabledocuments.h
    ../src/cloudaccounts.cpp ../src/cloudaccounts.h ../src/cloudprotocol.cpp ../src/cloudprotocol.h ../src/cloudstore.cpp ../src/cloudstore.h
    ../src/pagestore.cpp ../src/syncpackage.cpp
    ../src/inkrender.cpp ../src/inkselection.cpp ../src/inkselection.h ../src/shaperecognition.cpp
    ../src/pagecontent.cpp ../src/diagramitem.cpp ../src/diagramitem.h
    ../src/diagramrecognizer.cpp ../src/diagramrecognizer.h
    ../src/diagramexport.cpp ../src/diagramexport.h ../src/diagramimport.cpp
    ../src/screenorientation.cpp ../src/screenorientation.h
    ../src/chartitem.cpp ../src/chartitem.h ../src/chartrecognizer.cpp ../src/chartrecognizer.h
    ../src/pdfimport.cpp ../src/epubimport.cpp ../src/documentreader.cpp
    ../src/pinlock.cpp ../src/pinlock.h ../src/deviceinfo.cpp ../src/deviceinfo.h
    ../src/markerbattery.cpp ../src/markerbattery.h ../src/tabletpreferences.cpp ../src/tabletpreferences.h
    ../src/librarymaintenance.cpp ../src/librarymaintenance.h
    ../src/paperprostandby.cpp ../src/paperprostandby.h ../src/appleshare.cpp ../src/appleshare.h)
target_include_directories(quaderno_test_core PUBLIC ../src)
target_link_libraries(quaderno_test_core PUBLIC Qt6::Quick Qt6::DBus Qt6::Network OpenSSL::SSL ZLIB::ZLIB)
if(APPLE)
    target_sources(quaderno_test_core PRIVATE ../src/pdftext_mac.mm ../src/appleshare_mac.mm)
    set_source_files_properties(../src/pdftext_mac.mm ../src/appleshare_mac.mm PROPERTIES COMPILE_OPTIONS -fobjc-arc)
    target_link_libraries(quaderno_test_core PUBLIC ${appkit_framework} ${uti_framework} ${coregraphics_framework} ${pdfkit_framework} ${security_framework})
elseif(UNIX)
    target_link_libraries(quaderno_test_core PUBLIC ${CMAKE_DL_LIBS})
endif()

foreach(suite regression lasso reader blesync tabletpreferences markerbattery librarymaintenance cloud diagramrecognition diagramcontext screenorientation booknotes)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${suite}_tests.cpp")
        qt_add_executable(${suite}_tests "${suite}_tests.cpp")
        target_link_libraries(${suite}_tests PRIVATE quaderno_test_core Qt6::Test)
        add_test(NAME ${suite} COMMAND ${suite}_tests)
        set_tests_properties(${suite} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 90)
    endif()
endforeach()
target_sources(reader_tests PRIVATE ../src/macdocumentexchange.cpp ../src/macdocumentexchange.h)
target_sources(blesync_tests PRIVATE ../src/blesync.cpp ../src/blesync.h ../src/deltatransfer.cpp ../src/deltatransfer.h)
# Recovery regressions intentionally wait through bounded retry/backoff timers.
set_tests_properties(blesync PROPERTIES TIMEOUT 180)
qt_add_executable(sync_tests sync_tests.cpp ../src/syncpackage.cpp ../src/deltatransfer.cpp ../src/deltatransfer.h)
target_include_directories(sync_tests PRIVATE ../src)
target_link_libraries(sync_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME sync COMMAND sync_tests)
set_tests_properties(sync PROPERTIES TIMEOUT 45)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/wirelesssync_tests.cpp")
    qt_add_executable(wirelesssync_tests wirelesssync_tests.cpp
        ../src/wirelesssync.cpp ../src/wirelesssync.h ../src/blesync.cpp ../src/blesync.h
        ../src/deltatransfer.cpp ../src/deltatransfer.h ../src/syncchannel.cpp ../src/syncchannel.h
        ../src/blepipe.cpp ../src/blepipe.h ../src/wifisynclink.cpp ../src/wifisynclink.h
        ../src/syncrendezvous.cpp ../src/syncrendezvous.h)
    target_link_libraries(wirelesssync_tests PRIVATE quaderno_test_core Qt6::Test)
    if(APPLE)
        target_sources(wirelesssync_tests PRIVATE ../src/blecentral_mac.mm ../src/blecentral.h)
        set_target_properties(wirelesssync_tests PROPERTIES OBJCXX_STANDARD 17 OBJCXX_STANDARD_REQUIRED ON)
        target_link_libraries(wirelesssync_tests PRIVATE ${bluetooth_framework})
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_sources(wirelesssync_tests PRIVATE ../src/bleperipheral.cpp ../src/bleperipheral.h)
    endif()
    add_test(NAME wirelesssync COMMAND wirelesssync_tests)
    set_tests_properties(wirelesssync PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 90)
endif()

qt_add_executable(syncrendezvous_tests syncrendezvous_tests.cpp
    ../src/syncrendezvous.cpp ../src/wifisynclink.cpp ../src/wifisynclink.h)
target_include_directories(syncrendezvous_tests PRIVATE ../src)
target_link_libraries(syncrendezvous_tests PRIVATE Qt6::Network Qt6::Test)
add_test(NAME syncrendezvous COMMAND syncrendezvous_tests)

qt_add_executable(paperprosshaccess_tests paperprosshaccess_tests.cpp
    ../src/paperprosshaccess.cpp ../src/paperprosshaccess.h)
target_include_directories(paperprosshaccess_tests PRIVATE ../src)
target_link_libraries(paperprosshaccess_tests PRIVATE Qt6::Qml Qt6::Test)
add_test(NAME paperprosshaccess COMMAND paperprosshaccess_tests)

foreach(suite ui settingsintegration diagramsintegration workspaceintegration booknotesintegration)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${suite}_tests.cpp")
qt_add_executable(${suite}_tests ${suite}_tests.cpp)
get_target_property(core_sources quaderno_test_core SOURCES)
foreach(source IN LISTS core_sources)
    get_filename_component(name "${source}" NAME)
    list(APPEND core_names "${name}")
endforeach()
get_target_property(app_sources quaderno SOURCES)
foreach(source IN LISTS app_sources)
    if(source MATCHES "^src/" AND NOT source STREQUAL "src/main.cpp")
        get_filename_component(name "${source}" NAME)
        if(NOT name IN_LIST core_names)
            target_sources(${suite}_tests PRIVATE "../${source}")
        endif()
    endif()
endforeach()
if("accessibility" IN_LIST qt_disabled_gui_features)
    # The firmware omits Accessible; test the same portable QML used by the app.
    file(COPY "${PROJECT_BINARY_DIR}/qml-portable/" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/portable-source/qml")
    target_compile_definitions(${suite}_tests PRIVATE QUADERNO_SOURCE_DIR="${CMAKE_CURRENT_BINARY_DIR}/portable-source")
else()
    target_compile_definitions(${suite}_tests PRIVATE QUADERNO_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
endif()
target_link_libraries(${suite}_tests PRIVATE quaderno_test_core Qt6::Test Qt6::QuickControls2)
if(APPLE)
    set_target_properties(${suite}_tests PROPERTIES OBJCXX_STANDARD 17 OBJCXX_STANDARD_REQUIRED ON)
    target_link_libraries(${suite}_tests PRIVATE ${bluetooth_framework})
endif()
add_test(NAME ${suite} COMMAND ${suite}_tests)
set_tests_properties(${suite} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 90)

endif()
endforeach()

find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/scripts/cloud-auth-paper-pro.py")
    add_test(NAME cloud_helper COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/cloud_helper_tests.py")
    set_tests_properties(cloud_helper PROPERTIES TIMEOUT 20 ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
endif()
