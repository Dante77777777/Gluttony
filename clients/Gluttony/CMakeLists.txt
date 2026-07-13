cmake_minimum_required(VERSION 3.19)
project(Gluttony LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)
find_package(OpenCV 4.12.0 REQUIRED COMPONENTS core imgproc highgui)

qt_standard_project_setup()

qt_add_executable(Gluttony
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    resources/resources.qrc

    DetectionResult/DetectionResult.h
    DetectionResult/DetectionResult.cpp
    DetectionResult/DetectionResultUI.h

    LogWidget/LogWidget.h
    LogWidget/LogWidget.cpp
    LogWidget/LogWidgetUI.h

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
)

set(RESOURCE resources/resources.qrc)

qt_add_resources(APP_RESOURCE ${RESOURCE})
target_sources(Gluttony PRIVATE ${APP_RESOURCE})

target_link_libraries(Gluttony
    PRIVATE
        Qt::Core
        Qt::Widgets
)

target_include_directories(Gluttony PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(Gluttony PRIVATE ${OpenCV_LIBS})

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
