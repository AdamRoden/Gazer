cmake_minimum_required(VERSION 3.21)

project(Gazer
    VERSION 0.4.0
    DESCRIPTION "Gaze input mapper: Tobii Eye Tracker 5 → AAC layouts + virtual controllers"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(GAZER_USE_TOBII "Build Tobii Stream Engine integration (runtime DLL load)" ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Qml Quick)

set(GAZER_TOBII_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party/tobii/include")
set(GAZER_TOBII_EFFECTIVE OFF)

if(GAZER_USE_TOBII)
    if(EXISTS "${GAZER_TOBII_INCLUDE_DIR}/tobii/tobii.h")
        set(GAZER_TOBII_EFFECTIVE ON)
        message(STATUS "Tobii Stream Engine headers: ${GAZER_TOBII_INCLUDE_DIR}")
    endif()
endif()

set(GAZER_SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/app/ActionDispatcher.cpp
    src/app/AppSettings.cpp
    src/app/ActiveStateResolver.cpp
    src/app/GazeRouter.cpp
    src/app/GazerServices.cpp
    src/app/SettingsUi.cpp
    src/app/SettingsEditors.cpp
    src/app/SettingsCommands.cpp
    src/app/CommandRegistry.cpp
    src/core/TrackerMouse.cpp
    src/core/TrackerTobii.cpp
    src/core/StreamEngineLib.cpp
    src/layout/LayoutLoader.cpp
    src/layout/LayoutManager.cpp
    src/layout/LayoutInstance.cpp
    src/layout/LayoutInstanceManager.cpp
    src/layout/RootChrome.cpp
    src/layout/DwellRegionSpace.cpp
    src/layout/DwellStateMachine.cpp
    src/input/KeyboardInjector.cpp
    src/input/MouseInjector.cpp
    src/input/VirtualGamepad.cpp
    src/input/InputService.cpp
    src/mapping/MappingLoader.cpp
    src/mapping/MappingEngine.cpp
    src/assist/TtsService.cpp
    src/assist/PhraseService.cpp
    src/assist/ScriptHost.cpp
    src/assist/LookToScroll.cpp
    src/assist/MouseDwellMove.cpp
    src/assist/MouseAssistState.cpp
    src/assist/GazeReticle.cpp
    src/assist/GazeMouseFollow.cpp
    src/assist/AssistSession.cpp
    src/assist/AssistCommands.cpp
    src/assist/ActionLoopService.cpp
    src/ui/PreviewWindow.cpp
    src/ui/StlMesh.cpp
    src/ui/LayoutQuickWindow.cpp
    src/ui/GlassBackdrop.cpp
    src/ui/TrayIcon.cpp
    src/ui/DockRevealOverlay.cpp
    src/ui/EdgeBubbleOverlay.cpp
    src/ui/MagnifierOverlay.cpp
    src/ui/DwellSuspendOverlay.cpp
    src/ui/Theme.cpp
    src/ui/ThemeScheme.cpp
    src/ui/PickStyle.cpp
)

set(GAZER_HEADERS
    src/app/Application.h
    src/app/ActionDispatcher.h
    src/app/AppSettings.h
    src/app/ActiveStateResolver.h
    src/app/GazeRouter.h
    src/app/GazerServices.h
    src/app/SettingsUi.h
    src/app/CommandRegistry.h
    src/core/GazePoint.h
    src/core/HeadPose.h
    src/core/ITracker.h
    src/core/TrackerMouse.h
    src/core/TrackerTobii.h
    src/core/StreamEngineLib.h
    src/layout/LayoutTypes.h
    src/layout/LayoutVisibility.h
    src/layout/LayoutGeometry.h
    src/layout/DwellRegionSpace.h
    src/layout/InstanceTarget.h
    src/layout/LayoutLoader.h
    src/layout/LayoutManager.h
    src/layout/LayoutInstance.h
    src/layout/LayoutInstanceManager.h
    src/layout/DwellStateMachine.h
    src/layout/InvalidGazeGrace.h
    src/input/InputTypes.h
    src/input/KeyboardInjector.h
    src/input/MouseInjector.h
    src/input/VirtualGamepad.h
    src/input/InputService.h
    src/mapping/MappingTypes.h
    src/mapping/MappingLoader.h
    src/mapping/MappingEngine.h
    src/assist/TtsService.h
    src/assist/PhraseService.h
    src/assist/ScriptHost.h
    src/assist/LookToScroll.h
    src/assist/MouseDwellMove.h
    src/assist/MouseAssistState.h
    src/assist/GazeReticle.h
    src/assist/GazeMouseFollow.h
    src/assist/AssistSession.h
    src/assist/AssistCommands.h
    src/assist/ActionLoopService.h
    src/assist/GazeDwellTracker.h
    src/assist/GazeFollowStickiness.h
    src/assist/ForesightMemory.h
    src/assist/MagLayout.h
    src/ui/PreviewWindow.h
    src/ui/StlMesh.h
    src/ui/LayoutQuickWindow.h
    src/ui/GlassBackdrop.h
    src/ui/TrayIcon.h
    src/ui/DockRevealOverlay.h
    src/ui/EdgeBubbleOverlay.h
    src/ui/MagnifierOverlay.h
    src/ui/DwellSuspendOverlay.h
    src/ui/OverlaySurface.h
    src/ui/AppIcon.h
    src/ui/MouseIcons.h
    src/ui/Theme.h
    src/ui/PickStyle.h
    src/ui/ThemeScheme.h
    src/utils/Log.h
    src/utils/WinOverlay.h
    src/utils/ScreenGrab.h
)

add_executable(Gazer WIN32
    ${GAZER_SOURCES}
    ${GAZER_HEADERS}
)

# Embed multi-size gazer.ico into the PE so Explorer / taskbar / shortcuts show the brand icon.
if(WIN32)
    set(GAZER_ICON_PATH "${CMAKE_SOURCE_DIR}/resources/icons/gazer.ico")
    file(TO_CMAKE_PATH "${GAZER_ICON_PATH}" GAZER_ICON_PATH)
    string(REPLACE "\\" "/" GAZER_ICON_PATH "${GAZER_ICON_PATH}")
    configure_file(
        "${CMAKE_SOURCE_DIR}/resources/icons/gazer.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/gazer_icon.rc"
        @ONLY
    )
    target_sources(Gazer PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/gazer_icon.rc")
endif()

target_include_directories(Gazer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GAZER_TOBII_INCLUDE_DIR}
)

if(GAZER_TOBII_EFFECTIVE)
    target_compile_definitions(Gazer PRIVATE GAZER_USE_TOBII=1)
else()
    target_compile_definitions(Gazer PRIVATE GAZER_USE_TOBII=0)
endif()

target_link_libraries(Gazer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Qml
    Qt6::Quick
)

if(WIN32)
    target_link_libraries(Gazer PRIVATE user32 ole32 sapi)
endif()

add_custom_command(TARGET Gazer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/resources"
        "$<TARGET_FILE_DIR:Gazer>/resources"
)

set(_tobii_dll_candidates
    "$ENV{ProgramFiles}/Tobii/Tobii EyeX/tobii_stream_engine.dll"
    "C:/Program Files/Tobii/Tobii EyeX/tobii_stream_engine.dll"
)
foreach(_dll IN LISTS _tobii_dll_candidates)
    if(EXISTS "${_dll}")
        add_custom_command(TARGET Gazer POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_dll}"
                "$<TARGET_FILE_DIR:Gazer>/tobii_stream_engine.dll"
        )
        break()
    endif()
endforeach()

if(WIN32)
    # Keep runtime deps next to Gazer.exe (Qt6Qml is required for QJSEngine / ScriptHost).
    # Without this, a fresh build silently fails at process start with STATUS_DLL_NOT_FOUND.
    find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET Gazer POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --no-compiler-runtime
                "$<TARGET_FILE:Gazer>"
            COMMENT "Deploying Qt runtime DLLs beside Gazer.exe"
        )
        add_custom_target(deploy
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations "$<TARGET_FILE:Gazer>"
            DEPENDS Gazer
        )
    else()
        message(WARNING "windeployqt not found — run it manually after build or Gazer may fail to start")
    endif()
endif()

install(TARGETS Gazer RUNTIME DESTINATION bin)
install(DIRECTORY resources/ DESTINATION share/gazer/resources)
