cmake_minimum_required(VERSION 3.19)
project(Gluttony LANGUAGES CXX)

include(CTest)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Sql Network)
find_package(OpenCV 4.12.0 REQUIRED COMPONENTS core imgproc highgui)

qt_standard_project_setup()

if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
endif()

qt_add_executable(Gluttony
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    resources/resources.qrc

    DataBase/DataBasePaths.h
    DataBase/DataBasePaths.cpp
    DataBase/DataBaseConnection.h
    DataBase/DataBaseConnection.cpp
    DataBase/MigrationCatalog.h
    DataBase/MigrationCatalog.cpp
    DataBase/MigrationStatementSplitter.h
    DataBase/MigrationStatementSplitter.cpp
    DataBase/MigrationCheckSum.h
    DataBase/MigrationCheckSum.cpp
    DataBase/MigrationPlanner.h
    DataBase/MigrationPlanner.cpp
    DataBase/MigrationExecutor.h
    DataBase/MigrationExecutor.cpp
    DataBase/DatabaseBootstrapper.h
    DataBase/DatabaseBootstrapper.cpp
    DataBase/PersistenceRecords.h
    DataBase/SqliteTransaction.h
    DataBase/SqliteTransaction.cpp
    DataBase/InspectionRunRepository.h
    DataBase/InspectionRunRepository.cpp
    DataBase/InspectionItemRepository.h
    DataBase/InspectionItemRepository.cpp
    DataBase/PersistenceWorker.h
    DataBase/PersistenceWorker.cpp

    DetectionResult/DetectionResult.h
    DetectionResult/DetectionResult.cpp
    DetectionResult/DetectionResultUI.h
    DetectionResult/DetectionResultModel.h
    DetectionResult/DetectionResultModel.cpp

    LogWidget/LogWidget.h
    LogWidget/LogWidget.cpp
    LogWidget/LogWidgetUI.h
    LogWidget/LogMessage.h
    LogWidget/LogMessage.cpp

    ModelWidget/ModelWidget.h
    ModelWidget/ModelWidget.cpp
    ModelWidget/ModelWidgetUI.h

    ParamWidget/ParamWidget.h
    ParamWidget/ParamWidget.cpp
    ParamWidget/ParamWidgetUI.h

    QuickResWidget/QuickResWidget.h
    QuickResWidget/QuickResWidget.cpp
    QuickResWidget/QuickResWidgetUI.h

    ToolBar/ToolBar.h
    ToolBar/ToolBar.cpp
    ToolBar/ToolBarUI.h

    ViewWidget/ViewWidget.h
    ViewWidget/ViewWidget.cpp
    ViewWidget/ViewWidgetUI.h
    ViewWidget/GraphicsView.h
    ViewWidget/GraphicsView.cpp
    ViewWidget/ImageItem.h
    ViewWidget/ImageItem.cpp

    statusBar/StatusBar.h
    statusBar/StatusBar.cpp
    statusBar/StatusBarUI.h

    Inference/InferenceEngine.h
    Inference/InferenceEngine.cpp

    Workers/ImageWorker.h
    Workers/ImageWorker.cpp
    Workers/CameraInferenceWorker.h
    Workers/CameraInferenceWorker.cpp
    Workers/LatestCameraFrameBuffer.h
    Workers/LatestCameraFrameBuffer.cpp
    Workers/CameraInferenceController.h
    Workers/CameraInferenceController.cpp

    Transport/TcpImageServer.h
    Transport/TcpImageServer.cpp
    Transport/FrameProtocol.h
    Transport/FrameProtocol.cpp
    Transport/FrameStreamDecoder.h
    Transport/FrameStreamDecoder.cpp
    Transport/CameraStreamReceiver.h
    Transport/CameraStreamReceiver.cpp
    Transport/ReceivedFrameImageDecoder.h
    Transport/ReceivedFrameImageDecoder.cpp
    Transport/CameraFrameSource.h
    Transport/CameraFrameSource.cpp
    Transport/InferenceResponseCodec.h
    Transport/InferenceResponseCodec.cpp
)

get_filename_component(
    GLUTTONY_MIGRATION_SOURCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../../database/migrations"
    ABSOLUTE
)

set(
    GLUTTONY_INITIAL_MIGRATION
    "${GLUTTONY_MIGRATION_SOURCE_DIR}/001_initial_schema.sql"
)

if(NOT EXISTS "${GLUTTONY_INITIAL_MIGRATION}")
    message(
        FATAL_ERROR
        "Required database migration was not found: "
        "${GLUTTONY_INITIAL_MIGRATION}"
    )
endif()

qt_add_resources(
    Gluttony
    "gluttony_database_migrations"
    PREFIX
        "/database/migrations"
    BASE
        "${GLUTTONY_MIGRATION_SOURCE_DIR}"
    FILES
        "${GLUTTONY_INITIAL_MIGRATION}"
)

if(BUILD_TESTING)
    qt_add_executable(GluttonyTcpImageServerTest
        tests/TcpImageServerTest.cpp

        Transport/TcpImageServer.h
        Transport/TcpImageServer.cpp
    )

    target_include_directories(
        GluttonyTcpImageServerTest
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    target_link_libraries(
        GluttonyTcpImageServerTest
        PRIVATE
            Qt::Core
            Qt::Network
            Qt::Test
    )

    add_test(
        NAME GluttonyTcpImageServerTest
        COMMAND GluttonyTcpImageServerTest
    )

    qt_add_executable(GluttonyFrameProtocolTest
        tests/FrameProtocolTest.cpp

        Transport/FrameProtocol.h
        Transport/FrameProtocol.cpp
    )

    target_include_directories(
        GluttonyFrameProtocolTest
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    target_link_libraries(
        GluttonyFrameProtocolTest
        PRIVATE
            Qt::Core
            Qt::Test
    )

    add_test(
        NAME GluttonyFrameProtocolTest
        COMMAND GluttonyFrameProtocolTest
    )

    qt_add_executable(GluttonyInferenceResponseCodecTest
        tests/InferenceResponseCodecTest.cpp

        Transport/InferenceResponseCodec.h
        Transport/InferenceResponseCodec.cpp
    )

    target_include_directories(
        GluttonyInferenceResponseCodecTest
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    target_link_libraries(
        GluttonyInferenceResponseCodecTest
        PRIVATE
            Qt::Core
            Qt::Test
    )

    add_test(
        NAME GluttonyInferenceResponseCodecTest
        COMMAND GluttonyInferenceResponseCodecTest
    )

    qt_add_executable(GluttonyFrameStreamDecoderTest
        tests/FrameStreamDecoderTest.cpp

        Transport/FrameProtocol.h
        Transport/FrameProtocol.cpp
        Transport/FrameStreamDecoder.h
        Transport/FrameStreamDecoder.cpp
    )

    target_include_directories(
        GluttonyFrameStreamDecoderTest
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    target_link_libraries(
        GluttonyFrameStreamDecoderTest
        PRIVATE
            Qt::Core
            Qt::Test
    )

    add_test(
        NAME GluttonyFrameStreamDecoderTest
        COMMAND GluttonyFrameStreamDecoderTest
    )

    qt_add_executable(GluttonyCameraStreamReceiverTest
        tests/CameraStreamReceiverTest.cpp

        Transport/TcpImageServer.h
        Transport/TcpImageServer.cpp
        Transport/FrameProtocol.h
        Transport/FrameProtocol.cpp
        Transport/FrameStreamDecoder.h
        Transport/FrameStreamDecoder.cpp
        Transport/CameraStreamReceiver.h
        Transport/CameraStreamReceiver.cpp
    )

    target_include_directories(
        GluttonyCameraStreamReceiverTest
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    target_link_libraries(
        GluttonyCameraStreamReceiverTest
        PRIVATE
            Qt::Core
            Qt::Network
            Qt::Test
    )

    add_test(
        NAME GluttonyCameraStreamReceiverTest
        COMMAND GluttonyCameraStreamReceiverTest
    )

    qt_add_executable(GluttonyReceivedFrameImageDecoderTest
        tests/ReceivedFrameImageDecoderTest.cpp

        Transport/FrameProtocol.h
        Transport/FrameProtocol.cpp
        Transport/FrameStreamDecoder.h
        Transport/FrameStreamDecoder.cpp
        Transport/ReceivedFrameImageDecoder.h
        Transport/ReceivedFrameImageDecoder.cpp
    )

    target_include_directories(
        GluttonyReceivedFrameImageDecoderTest
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    target_link_libraries(
        GluttonyReceivedFrameImageDecoderTest
        PRIVATE
            Qt::Core
            Qt::Gui
            Qt::Test
    )

    add_test(
        NAME GluttonyReceivedFrameImageDecoderTest
        COMMAND GluttonyReceivedFrameImageDecoderTest
    )

    qt_add_executable(GluttonyCameraFrameSourceTest
        tests/CameraFrameSourceTest.cpp

        Transport/TcpImageServer.h
        Transport/TcpImageServer.cpp
        Transport/FrameProtocol.h
        Transport/FrameProtocol.cpp
        Transport/InferenceResponseCodec.h
        Transport/InferenceResponseCodec.cpp
        Transport/FrameStreamDecoder.h
        Transport/FrameStreamDecoder.cpp
        Transport/CameraStreamReceiver.h
        Transport/CameraStreamReceiver.cpp
        Transport/ReceivedFrameImageDecoder.h
        Transport/ReceivedFrameImageDecoder.cpp
        Transport/CameraFrameSource.h
        Transport/CameraFrameSource.cpp
    )

    target_include_directories(
        GluttonyCameraFrameSourceTest
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    target_link_libraries(
        GluttonyCameraFrameSourceTest
        PRIVATE
            Qt::Core
            Qt::Gui
            Qt::Network
            Qt::Test
    )

    add_test(
        NAME GluttonyCameraFrameSourceTest
        COMMAND GluttonyCameraFrameSourceTest
    )

    qt_add_executable(GluttonyLatestCameraFrameBufferTest
        tests/LatestCameraFrameBufferTest.cpp

        Workers/LatestCameraFrameBuffer.h
        Workers/LatestCameraFrameBuffer.cpp
    )

    target_include_directories(
        GluttonyLatestCameraFrameBufferTest
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    target_link_libraries(
        GluttonyLatestCameraFrameBufferTest
        PRIVATE
            Qt::Core
            Qt::Gui
            Qt::Test
    )

    add_test(
        NAME GluttonyLatestCameraFrameBufferTest
        COMMAND GluttonyLatestCameraFrameBufferTest
    )

    qt_add_executable(GluttonyPersistenceSmoke
        tests/PersistenceSmoke.cpp
        DataBase/DataBaseConnection.h
        DataBase/DataBaseConnection.cpp
        DataBase/MigrationCatalog.h
        DataBase/MigrationCatalog.cpp
        DataBase/MigrationStatementSplitter.h
        DataBase/MigrationStatementSplitter.cpp
        DataBase/MigrationCheckSum.h
        DataBase/MigrationCheckSum.cpp
        DataBase/MigrationPlanner.h
        DataBase/MigrationPlanner.cpp
        DataBase/MigrationExecutor.h
        DataBase/MigrationExecutor.cpp
        DataBase/DatabaseBootstrapper.h
        DataBase/DatabaseBootstrapper.cpp
        DataBase/PersistenceRecords.h
        DataBase/SqliteTransaction.h
        DataBase/SqliteTransaction.cpp
        DataBase/InspectionRunRepository.h
        DataBase/InspectionRunRepository.cpp
        DataBase/InspectionItemRepository.h
        DataBase/InspectionItemRepository.cpp
    )

    qt_add_resources(
        GluttonyPersistenceSmoke
        "gluttony_database_migrations_smoke"
        PREFIX
            "/database/migrations"
        BASE
            "${GLUTTONY_MIGRATION_SOURCE_DIR}"
        FILES
            "${GLUTTONY_INITIAL_MIGRATION}"
    )

    target_link_libraries(
        GluttonyPersistenceSmoke
        PRIVATE
            Qt::Core
            Qt::Sql
    )

    target_include_directories(
        GluttonyPersistenceSmoke
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    add_test(
        NAME GluttonyPersistenceSmoke
        COMMAND GluttonyPersistenceSmoke
    )
endif()

set(RESOURCE resources/resources.qrc)
set(OpenVINO_DIR "D:/Anaconda/Lib/site-packages/openvino/cmake")

find_package(OpenVINO REQUIRED COMPONENTS Runtime)

if(BUILD_TESTING)
    qt_add_executable(GluttonyCameraInferenceWorkerTest
        tests/CameraInferenceWorkerTest.cpp

        Inference/InferenceEngine.h
        Inference/InferenceEngine.cpp
        Workers/CameraInferenceWorker.h
        Workers/CameraInferenceWorker.cpp
        Transport/FrameProtocol.h
        Transport/FrameProtocol.cpp
        Transport/FrameStreamDecoder.h
        Transport/FrameStreamDecoder.cpp
        Transport/ReceivedFrameImageDecoder.h
    )

    target_include_directories(
        GluttonyCameraInferenceWorkerTest
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
            ${OpenCV_INCLUDE_DIRS}
    )

    target_link_libraries(
        GluttonyCameraInferenceWorkerTest
        PRIVATE
            Qt::Core
            Qt::Gui
            Qt::Widgets
            Qt::Test
            openvino::runtime
            ${OpenCV_LIBS}
    )

    add_test(
        NAME GluttonyCameraInferenceWorkerTest
        COMMAND GluttonyCameraInferenceWorkerTest
    )

    qt_add_executable(GluttonyCameraInferenceControllerTest
        tests/CameraInferenceControllerTest.cpp

        Inference/InferenceEngine.h
        Inference/InferenceEngine.cpp
        Workers/CameraInferenceWorker.h
        Workers/CameraInferenceWorker.cpp
        Workers/CameraInferenceController.h
        Workers/CameraInferenceController.cpp
        Workers/LatestCameraFrameBuffer.h
        Workers/LatestCameraFrameBuffer.cpp
        Transport/FrameProtocol.h
        Transport/FrameProtocol.cpp
        Transport/FrameStreamDecoder.h
        Transport/FrameStreamDecoder.cpp
        Transport/ReceivedFrameImageDecoder.h
    )

    target_include_directories(
        GluttonyCameraInferenceControllerTest
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
            ${OpenCV_INCLUDE_DIRS}
    )

    target_link_libraries(
        GluttonyCameraInferenceControllerTest
        PRIVATE
            Qt::Core
            Qt::Gui
            Qt::Widgets
            Qt::Test
            openvino::runtime
            ${OpenCV_LIBS}
    )

    add_test(
        NAME GluttonyCameraInferenceControllerTest
        COMMAND GluttonyCameraInferenceControllerTest
    )

    qt_add_executable(GluttonyPersistenceWorkerSmoke
        tests/PersistenceWorkerSmoke.cpp
        DataBase/DataBaseConnection.h
        DataBase/DataBaseConnection.cpp
        DataBase/MigrationCatalog.h
        DataBase/MigrationCatalog.cpp
        DataBase/MigrationStatementSplitter.h
        DataBase/MigrationStatementSplitter.cpp
        DataBase/MigrationCheckSum.h
        DataBase/MigrationCheckSum.cpp
        DataBase/MigrationPlanner.h
        DataBase/MigrationPlanner.cpp
        DataBase/MigrationExecutor.h
        DataBase/MigrationExecutor.cpp
        DataBase/DatabaseBootstrapper.h
        DataBase/DatabaseBootstrapper.cpp
        DataBase/PersistenceRecords.h
        DataBase/SqliteTransaction.h
        DataBase/SqliteTransaction.cpp
        DataBase/InspectionRunRepository.h
        DataBase/InspectionRunRepository.cpp
        DataBase/InspectionItemRepository.h
        DataBase/InspectionItemRepository.cpp
        DataBase/PersistenceWorker.h
        DataBase/PersistenceWorker.cpp
    )

    qt_add_resources(
        GluttonyPersistenceWorkerSmoke
        "gluttony_database_migrations_worker_smoke"
        PREFIX
            "/database/migrations"
        BASE
            "${GLUTTONY_MIGRATION_SOURCE_DIR}"
        FILES
            "${GLUTTONY_INITIAL_MIGRATION}"
    )

    target_include_directories(
        GluttonyPersistenceWorkerSmoke
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
            ${OpenCV_INCLUDE_DIRS}
    )

    target_link_libraries(
        GluttonyPersistenceWorkerSmoke
        PRIVATE
            Qt::Core
            Qt::Gui
            Qt::Sql
            Qt::Widgets
            openvino::runtime
    )

    add_test(
        NAME GluttonyPersistenceWorkerSmoke
        COMMAND GluttonyPersistenceWorkerSmoke
    )
endif()

qt_add_resources(APP_RESOURCE ${RESOURCE})
target_sources(Gluttony PRIVATE ${APP_RESOURCE})

target_link_libraries(Gluttony
    PRIVATE
        Qt::Core
        Qt::Widgets
)

target_include_directories(Gluttony PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(Gluttony PRIVATE ${OpenCV_LIBS})

target_link_libraries(Gluttony
    PRIVATE
    Qt::Core
    Qt::Widgets
    Qt::Sql
    Qt::Network
    openvino::runtime
)

include(GNUInstallDirs)

install(TARGETS Gluttony
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET Gluttony
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
