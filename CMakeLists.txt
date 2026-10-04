# CMakeList.txt: CMake-Projekt für "WxWidgestTemplate". Schließen Sie die Quelle ein, und definieren Sie
# projektspezifische Logik hier.
#
cmake_minimum_required (VERSION 3.8)

# Aktivieren Sie Hot Reload für MSVC-Compiler, sofern unterstützt.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("launcher")


#! ! ! ! ! ! !
#set this to ON instead of OFF to ship the game!
#basically this will change the RESOURCES_PATH to be the local path
#! ! ! ! ! ! !
set(PRODUCTION_BUILD OFF CACHE BOOL "Make this a production build" FORCE)
#delete the out folder after changing if visual studio doesn recognize the change!


# wxWidgets static build in DEBUG MODE
if(NOT PRODUCTION_BUILD)
    set(wxBUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
endif()


#may give problems on linux
if(PRODUCTION_BUILD)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE) #link time optimization
else()
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)
endif()


# add the wxwidgets library
add_subdirectory(thirdparty/wxwidgets)		


# credit to
#https://github.com/meemknight/raylibCmakeSetup

# MY_SOURCES is defined to be a list of all the source files for my game 
# DON'T ADD THE SOURCES BY HAND, they are already added with this macro
file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")


if(MSVC)
    add_executable("${CMAKE_PROJECT_NAME}" WIN32)
else()
    add_executable("${CMAKE_PROJECT_NAME}")
endif()


target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/")


if(MSVC)
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/GL>

        # only for this cpu
        $<$<CONFIG:Release>:/arch:AVX2>
    )

    target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
        $<$<CONFIG:Release>:/LTCG>
    )
endif()


# ON MSCV
if(MSVC)
    # No Console on Windows
    target_link_options("${CMAKE_PROJECT_NAME}" PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/SUBSYSTEM:WINDOWS>"
    )
endif()

# Fügen Sie der ausführbaren Datei dieses Projekts eine Quelle hinzu.
target_sources("${CMAKE_PROJECT_NAME}" PRIVATE ${MY_SOURCES} "app.rc")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET "${CMAKE_PROJECT_NAME}" PROPERTY CXX_STANDARD 20)
endif()

target_link_libraries("${CMAKE_PROJECT_NAME}" PRIVATE wx::core wx::base)

# TODO: Fügen Sie bei Bedarf Tests hinzu, und installieren Sie Ziele.
