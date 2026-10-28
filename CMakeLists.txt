cmake_minimum_required(VERSION 3.21)
project(BankingSystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Point this at a complete Qt kit, for example:
# cmake -S . -B build -DOOPBANK_QT_ROOT=C:/Qt/6.12.0/msvc2022_64
set(OOPBANK_QT_ROOT "" CACHE PATH "Qt kit directory containing lib/cmake/Qt6")
if(NOT OOPBANK_QT_ROOT AND DEFINED ENV{QT_ROOT_DIR})
    set(OOPBANK_QT_ROOT "$ENV{QT_ROOT_DIR}")
endif()
if(OOPBANK_QT_ROOT)
    list(PREPEND CMAKE_PREFIX_PATH "${OOPBANK_QT_ROOT}")
endif()
if(NOT Qt6_DIR AND DEFINED ENV{Qt6_DIR})
    set(Qt6_DIR "$ENV{Qt6_DIR}")
endif()

if(WIN32 AND MINGW)
    message(FATAL_ERROR
        "This application embeds the UI with Qt WebEngine, which is not supported by the Windows MinGW kit. "
        "Install and select a Qt MSVC 64-bit kit that includes WebEngineWidgets and WebChannel.")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 CONFIG REQUIRED COMPONENTS
    Core
    Widgets
    Network
    Concurrent
    WebChannel
    WebEngineWidgets
)
message(STATUS "Using Qt ${Qt6_VERSION} from ${Qt6_DIR}")

# Keep all current backend, model, repository, service and bridge implementation
# files in the executable. CONFIGURE_DEPENDS makes CMake refresh this list when
# a source or header is added or removed.
file(GLOB_RECURSE OOPBANK_IMPLEMENTATION_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
file(GLOB_RECURSE OOPBANK_PROJECT_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

qt_add_executable(BankingSystem
    ${OOPBANK_IMPLEMENTATION_SOURCES}
    ${OOPBANK_PROJECT_HEADERS}
)

# Give the desktop application a stable output filename.
set_target_properties(BankingSystem PROPERTIES OUTPUT_NAME "OOPBANK-APP_DESKTOP")
if(WIN32)
    # Build as a Windows GUI application so launching the EXE doesn't open a console window.
    set_target_properties(BankingSystem PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_include_directories(BankingSystem PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

set(OOPBANK_PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH
    "Runtime project directory containing .env and UI_Frontend")
file(TO_CMAKE_PATH "${OOPBANK_PROJECT_ROOT}" OOPBANK_PROJECT_DIR)
set(OOPBANK_UI_DIR "${OOPBANK_PROJECT_DIR}/docs")
string(REPLACE "#" "\\x23" OOPBANK_PROJECT_DIR_DEFINE "${OOPBANK_PROJECT_DIR}")
string(REPLACE "#" "\\x23" OOPBANK_UI_DIR_DEFINE "${OOPBANK_UI_DIR}")
target_compile_definitions(BankingSystem PRIVATE
    "OOPBANK_PROJECT_DIR=\"${OOPBANK_PROJECT_DIR_DEFINE}\""
    "OOPBANK_UI_DIR=\"${OOPBANK_UI_DIR_DEFINE}\""
)

if(MSVC)
    target_compile_options(BankingSystem PRIVATE /Zc:__cplusplus)
endif()

target_link_libraries(BankingSystem PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent
    Qt6::WebChannel
    Qt6::WebEngineWidgets
)

# Make the desktop build relocatable: keep the Web UI and schema beside the EXE.
add_custom_command(TARGET BankingSystem POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/docs" "$<TARGET_FILE_DIR:BankingSystem>/docs"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/schema.sql" "$<TARGET_FILE_DIR:BankingSystem>/schema.sql"
    VERBATIM
)

# Keep migrations beside the executable when the project includes them.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/migrations")
    add_custom_command(TARGET BankingSystem POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/migrations" "$<TARGET_FILE_DIR:BankingSystem>/migrations"
        VERBATIM
    )
endif()

if(WIN32)
    get_target_property(OOPBANK_QT_CORE_DEBUG Qt6::Core IMPORTED_LOCATION_DEBUG)
    get_filename_component(OOPBANK_QT_BIN "${OOPBANK_QT_CORE_DEBUG}" DIRECTORY)
    if(EXISTS "${OOPBANK_QT_BIN}/windeployqt.exe")
        add_custom_command(TARGET BankingSystem POST_BUILD
            COMMAND "${OOPBANK_QT_BIN}/windeployqt.exe"
                --$<IF:$<CONFIG:Debug>,debug,release>
                --compiler-runtime
                "$<TARGET_FILE:BankingSystem>"
            VERBATIM
        )
    endif()

    # The app reads .env from its executable directory in a deployed folder.
    # Keep this local credential file out of source control, but copy it when present.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.env")
        add_custom_command(TARGET BankingSystem POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/.env" "$<TARGET_FILE_DIR:BankingSystem>/.env"
            VERBATIM
        )
    endif()
endif()
