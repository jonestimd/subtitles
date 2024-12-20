cmake_minimum_required(VERSION 3.19)
project(subtitles LANGUAGES CXX)

# set(CMAKE_AUTORCC on)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

# set(PROJECT_RESOURCES
#     resources/subtitles.qrc
#     resources/MaterialSymbolsRounded_Filled-Regular.ttf
# )

qt_add_executable(subtitles
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    ${PROJECT_RESOURCES}
    subtitle.h subtitle.cpp
)

target_link_libraries(subtitles
    PRIVATE
        tesseract
        Qt::Core
        Qt::Widgets
)

include(GNUInstallDirs)

install(TARGETS subtitles
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET subtitles
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
