cmake_minimum_required(VERSION 3.21)

project(Gazer
    VERSION 0.5.1
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

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets Qml Quick Svg Test)

# AUTOMOC may invoke the compiler by basename. Keep MinGW on PATH even when
# the parent shell did not export it (`cmake --build` / ninja autogen).
if(WIN32 AND CMAKE_CXX_COMPILER)
    get_filename_component(GAZER_CXX_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
    if(EXISTS "${GAZER_CXX_BIN}")
        set(ENV{PATH} "${GAZER_CXX_BIN};$ENV{PATH}")
    endif()
endif()

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
    src/app/AppSettingsIo.cpp
    src/app/AppSettingsTheme.cpp
    src/app/ActiveStateResolver.cpp
    src/app/GazeRouter.cpp
    src/app/GazerServices.cpp
    src/app/SettingsUi.cpp
    src/app/SettingsNumpad.cpp
    src/app/SettingsArrayEditor.cpp
    src/app/SettingsColorPicker.cpp
    src/app/SettingsOpacity.cpp
    src/app/SettingsHexEditor.cpp
    src/app/SettingsSliderGaze.cpp
    src/app/SettingsCommands.cpp
    src/app/CommandRegistry.cpp
    src/core/TrackerMouse.cpp
    src/core/TrackerTobii.cpp
    src/core/StreamEngineLib.cpp
    src/layout/PageDim.cpp
    src/layout/PageActionParse.cpp
    src/layout/PageActionParseFields.cpp
    src/layout/PageLoader.cpp
    src/layout/PageResolve.cpp
    src/layout/PageDetector.cpp
    src/layout/PageHit.cpp
    src/layout/PageSession.cpp
    src/layout/PageSessionAttach.cpp
    src/layout/PageSessionNav.cpp
    src/layout/PageSessionChrome.cpp
    src/layout/PageSessionGaze.cpp
    src/layout/PageWriter.cpp
    src/layout/PageEdit.cpp
    src/layout/PageCatalog.cpp
    src/layout/DwellRegionSpace.cpp
    src/layout/DwellStateMachine.cpp
    src/input/KeyboardInjector.cpp
    src/input/KeyStateManager.cpp
    src/input/KeyGlyphs.cpp
    src/input/MouseInjector.cpp
    src/input/PixelScroller.cpp
    src/input/VirtualGamepad.cpp
    src/input/InputService.cpp
    src/mapping/MappingLoader.cpp
    src/mapping/MappingEngine.cpp
    src/assist/TtsService.cpp
    src/assist/PhraseService.cpp
    src/assist/ScriptHost.cpp
    src/assist/AhkLauncher.cpp
    src/assist/LookToScroll.cpp
    src/assist/ComboMouse.cpp
    src/assist/MouseDwellMove.cpp
    src/assist/MouseDwellMoveMag.cpp
    src/assist/MouseAssistState.cpp
    src/assist/GazeReticle.cpp
    src/assist/GazeMouseFollow.cpp
    src/assist/AssistSession.cpp
    src/assist/AssistCommands.cpp
    src/assist/ActionLoopService.cpp
    src/ui/PreviewWindow.cpp
    src/ui/PreviewGeometry.cpp
    src/ui/StlMesh.cpp
    src/ui/PageHostWindow.cpp
    src/ui/BoardPaint.cpp
    src/ui/SliderTrack.cpp
    src/ui/GlassBackdrop.cpp
    src/ui/TrayIcon.cpp
    src/ui/MagnifierOverlay.cpp
    src/ui/DwellSuspendOverlay.cpp
    src/ui/Theme.cpp
    src/ui/ThemeScheme.cpp
    src/ui/KeySymbols.cpp
    src/ui/PickStyle.cpp
    src/editor/LayoutEditorSession.cpp
    src/editor/LayoutEditorSessionIo.cpp
    src/editor/LayoutEditorSessionEdit.cpp
    src/editor/LayoutEditorKeyboard.cpp
    src/editor/LayoutEditorCanvas.cpp
    src/editor/LayoutEditorCanvasPaint.cpp
    src/editor/LayoutEditorCanvasInteract.cpp
    src/editor/LayoutEditorIcons.cpp
    src/editor/LayoutEditorMenu.cpp
    src/editor/LayoutEditorStyle.cpp
    src/editor/LayoutEditorCodeView.cpp
    src/editor/LayoutEditorToolbox.cpp
    src/editor/LayoutEditorFields.cpp
    src/editor/LayoutEditorFieldsGroups.cpp
    src/editor/LayoutEditorProperties.cpp
    src/editor/LayoutEditorPropertiesFill.cpp
    src/editor/LayoutEditorWindow.cpp
    src/editor/LayoutEditorWindowFile.cpp
    src/editor/LayoutEditorXmlHighlight.cpp
    src/utils/WinOverlay.cpp
)

set(GAZER_HEADERS
    src/app/Application.h
    src/app/ActionDispatcher.h
    src/app/AppSettings.h
    src/app/ActiveStateResolver.h
    src/app/GazeRouter.h
    src/app/GazerServices.h
    src/app/SettingsUi.h
    src/app/SettingsPageBuild.h
    src/app/SettingsUiInternal.h
    src/app/CommandRegistry.h
    src/core/GazePoint.h
    src/core/HeadPose.h
    src/core/ITracker.h
    src/core/TrackerMouse.h
    src/core/TrackerTobii.h
    src/core/StreamEngineLib.h
    src/layout/DwellRegionSpace.h
    src/layout/PageTypes.h
    src/layout/PageBox.h
    src/layout/RoundBox.h
    src/layout/ChromeBlur.h
    src/layout/ProgressStyle.h
    src/layout/PageDim.h
    src/layout/PageActionParse.h
    src/layout/PageLoader.h
    src/layout/PageResolve.h
    src/layout/PageNav.h
    src/layout/PageDetector.h
    src/layout/PageHit.h
    src/layout/PageSession.h
    src/layout/PageWriter.h
    src/layout/PageEdit.h
    src/layout/PageCatalog.h
    src/layout/DwellStateMachine.h
    src/layout/InvalidGazeGrace.h
    src/input/InputTypes.h
    src/input/KeyboardInjector.h
    src/input/KeyStateManager.h
    src/input/KeyGlyphs.h
    src/input/MouseInjector.h
    src/input/PixelScroller.h
    src/input/VirtualGamepad.h
    src/input/InputService.h
    src/mapping/MappingTypes.h
    src/mapping/MappingLoader.h
    src/mapping/MappingEngine.h
    src/assist/TtsService.h
    src/assist/PhraseService.h
    src/assist/ScriptHost.h
    src/assist/AhkLauncher.h
    src/assist/LookToScroll.h
    src/assist/ComboMouse.h
    src/assist/ComboMouseHit.h
    src/assist/LtsSpeed.h
    src/assist/LtsIndicator.h
    src/assist/MouseDwellMove.h
    src/assist/MouseAssistState.h
    src/assist/GazeReticle.h
    src/assist/GazeMouseFollow.h
    src/assist/AssistSession.h
    src/assist/AssistCommands.h
    src/assist/ActionLoopService.h
    src/assist/GazeDwellTracker.h
    src/assist/GazeFollowProfile.h
    src/assist/GazeFollowStickiness.h
    src/assist/ForesightMemory.h
    src/assist/MagLayout.h
    src/ui/PreviewWindow.h
    src/ui/PreviewGeometry.h
    src/ui/StlMesh.h
    src/ui/PageHostWindow.h
    src/ui/BoardPaint.h
    src/ui/ProgressVisuals.h
    src/ui/ProgressPaint.h
    src/ui/SliderTrack.h
    src/ui/GlassBackdrop.h
    src/ui/TrayIcon.h
    src/ui/MagnifierOverlay.h
    src/ui/DwellSuspendOverlay.h
    src/ui/OverlaySurface.h
    src/ui/AppIcon.h
    src/ui/KeySymbols.h
    src/ui/Theme.h
    src/ui/PickStyle.h
    src/ui/ThemeScheme.h
    src/editor/LayoutEditorSession.h
    src/editor/LayoutEditorKeyboard.h
    src/editor/LayoutEditorCanvas.h
    src/editor/LayoutEditorIcons.h
    src/editor/LayoutEditorMenu.h
    src/editor/LayoutEditorStyle.h
    src/editor/LayoutEditorCodeView.h
    src/editor/LayoutEditorToolbox.h
    src/editor/LayoutEditorFields.h
    src/editor/LayoutEditorProperties.h
    src/editor/LayoutEditorWindow.h
    src/editor/LayoutEditorXmlHighlight.h
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
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Qml
    Qt6::Quick
    Qt6::Svg
)

if(WIN32)
    target_link_libraries(Gazer PRIVATE user32 ole32 oleaut32 uuid sapi)
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
        # --no-compiler-runtime skips MinGW's libstdc++ / libgcc / winpthread.
        # Without them next to Gazer.exe, a shell that does not have mingw on PATH
        # exits instantly (WIN32 subsystem: no console error).
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            get_filename_component(_mingw_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
            foreach(_dll IN ITEMS libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
                if(EXISTS "${_mingw_bin}/${_dll}")
                    add_custom_command(TARGET Gazer POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${_mingw_bin}/${_dll}"
                            "$<TARGET_FILE_DIR:Gazer>/${_dll}"
                    )
                endif()
            endforeach()
        endif()
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

enable_testing()
set(GAZER_PAGE_TEST_LIB
    src/layout/PageDim.cpp
    src/layout/PageActionParse.cpp
    src/layout/PageActionParseFields.cpp
    src/layout/PageLoader.cpp
    src/layout/PageResolve.cpp
    src/layout/PageDetector.cpp
    src/layout/PageHit.cpp
    src/layout/PageWriter.cpp
    src/layout/PageEdit.cpp
    src/layout/PageCatalog.cpp
    src/layout/DwellRegionSpace.cpp
    src/assist/AhkLauncher.cpp
)
add_executable(GazerPageTests
    tests/page_tests_main.cpp
    tests/PageLoaderTest.cpp
    tests/PageLoaderActionTest.cpp
    tests/PageDimTest.cpp
    tests/PageNavTest.cpp
    tests/PageHitTest.cpp
    tests/PageHitLiveTest.cpp
    tests/SettingsLayoutTest.cpp
    tests/ProgressPaintTest.cpp
    tests/ComboMouseTest.cpp
    tests/AhkLauncherTest.cpp
    tests/KeyStateManagerTest.cpp
    src/input/KeyStateManager.cpp
    src/input/KeyGlyphs.cpp
    ${GAZER_PAGE_TEST_LIB}
)
target_include_directories(GazerPageTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(GazerPageTests PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
target_compile_definitions(GazerPageTests PRIVATE
    "GAZER_TEST_FIXTURES=\"${CMAKE_SOURCE_DIR}/tests/fixtures\""
    "GAZER_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\""
)
add_test(NAME GazerPageTests COMMAND GazerPageTests)

add_executable(GazerDwellTests
    tests/DwellStateMachineTest.cpp
    src/layout/DwellStateMachine.cpp
)
target_include_directories(GazerDwellTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(GazerDwellTests PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME GazerDwellTests COMMAND GazerDwellTests)
