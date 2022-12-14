cmake_minimum_required (VERSION 3.0)

include (CheckFunctionExists)
include (CheckVariableExists)
include (CheckCSourceCompiles)
include (CheckCXXSourceCompiles)
include (CheckIncludeFile)
include (CheckIncludeFileCXX)

if(COMMAND cmake_policy)
   if(POLICY CMP0054)
       cmake_policy(SET CMP0054 NEW)
   endif()
   # Mac RPATH policy
   if(POLICY CMP0042)
       cmake_policy(SET CMP0042 NEW)
   endif()

    # AUTOMOC/AUTOUIC on generated files policy. Since we only
    # generate resource files with fgrcc which don't use Qt,
    # we set those files source property. See the help for this
    # policy for more info
    if(POLICY CMP0071)
        cmake_policy(SET CMP0071 NEW)
    endif()

    # OpenGL VND policy : use the old definition for now, until we can audit this
    if(POLICY CMP0072)
        cmake_policy(SET CMP0072 OLD)
    endif()
endif()

message("CMAKE Build type: ${CMAKE_BUILD_TYPE}")
# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Debug' as none was specified.")
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
    "MinSizeRel" "RelWithDebInfo")
endif()

if(NOT FG_BUILD_TYPE)
  message(STATUS "Setting build type to 'Dev' as none was specified.")
  set(FG_BUILD_TYPE Dev CACHE STRING "Choose the FlightGear build type" FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE FG_BUILD_TYPE PROPERTY STRINGS "Dev" "Nightly" "Release")
endif()

if(APPLE)
	set(CMAKE_INSTALL_RPATH "@loader_path/../Frameworks")
	# when building, don't use the install RPATH already
	# (but later on when installing)
	SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
endif()

# let's use & require C++11 - note these are only functional with CMake 3.1
# we do manual fallbacks for CMake 3.0 in the compilers section
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9" CACHE STRING "Minimum OS X deployment version")

project(FlightGear)

# Define SRC_PARENT_DIR as the parent directory of the project source directory
get_filename_component(SRC_PARENT_DIR ${PROJECT_SOURCE_DIR} DIRECTORY)

# We have some custom .cmake scripts not in the official distribution.
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/CMakeModules;${CMAKE_MODULE_PATH}")
set_property(GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS TRUE)

# Warning when build is not an out-of-source build.
string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}" InSourceBuild)
if(InSourceBuild)
    message(WARNING  "Avoid building inside the source tree!")
    message(WARNING  "Create a separate build directory instead (i.e. 'fgbuild') and call CMake from there: ")
    message(WARNING  "  mkdir ../fgbuild && cd ../fgbuild && cmake ${CMAKE_SOURCE_DIR}")
endif(InSourceBuild)

include(GNUInstallDirs)
# System detection/default settings
include( DetectDistro )
include( DetectBrowser )
include( ExportDebugSymbols )
set(CMAKE_DEBUG_POSTFIX          "d" CACHE STRING "add a postfix, usually d on windows")
set(CMAKE_RELEASE_POSTFIX        ""  CACHE STRING "add a postfix, usually empty on windows")
set(CMAKE_RELWITHDEBINFO_POSTFIX ""  CACHE STRING "add a postfix, usually empty on windows")
set(CMAKE_MINSIZEREL_POSTFIX     ""  CACHE STRING "add a postfix, usually empty on windows")

# read 'version' file into a variable (stripping any newlines or spaces)
file(READ flightgear-version versionFile)
if (NOT versionFile)
    message(FATAL_ERROR "Unable to determine FlightGear version. Version file is missing.")
endif()
string(STRIP "${versionFile}" FLIGHTGEAR_VERSION)
# add a dependency on the version file
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS version)

# FlightGear packaging (to build a source tarball)
include( ConfigureCPack )

# FlightGear base package path
if (FG_DATA_DIR)
    message(STATUS "Using explicit data directory for base package: ${FG_DATA_DIR}")
elseif(IS_DIRECTORY "${SRC_PARENT_DIR}/fgdata")
    set(FG_DATA_DIR "${SRC_PARENT_DIR}/fgdata")
    message(STATUS "Using data directory for base package: ${FG_DATA_DIR}")
else()
    set(FG_DATA_DIR "${CMAKE_INSTALL_PREFIX}/lib/FlightGear" CACHE PATH "Default location where data files are located")
    message(STATUS "Using default data directory for base package:  ${FG_DATA_DIR}")
endif()

# Change the default build type to something fast
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
        FORCE)
endif(NOT CMAKE_BUILD_TYPE)

#####################################################################################
# Configure library search paths
#####################################################################################

IF(APPLE)
    set(EVENT_INPUT_DEFAULT 1)

    find_library(CORESERVICES_LIBRARY CoreServices)
    find_library(COCOA_LIBRARY Cocoa)
    list(APPEND PLATFORM_LIBS ${COCOA_LIBRARY} ${CORESERVICES_LIBRARY})
elseif(WIN32)
    set(EVENT_INPUT_DEFAULT 1)
    list(APPEND PLATFORM_LIBS "Shlwapi.lib" "Version.lib")

    # disabling for now so we don' bundle an outdatd UCRT
 #   set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION "bin")
 #   set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
 #   include(InstallRequiredSystemLibraries)
  #  MESSAGE("Installing: ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}")

elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR
	${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD" OR
	${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
    find_package(Threads REQUIRED)
    find_package(X11 REQUIRED)

    set(USE_DBUS_DEFAULT 1)

    find_package(UDev QUIET)
    if(UDEV_FOUND)
        set(EVENT_INPUT_DEFAULT 1)
    endif(UDEV_FOUND)

    find_package(Speex)
    find_package(Speexdsp)
    if(SPEEX_FOUND AND SPEEXDSP_FOUND)
        set(SYSTEM_SPEEX_DEFAULT 1)
    endif(SPEEX_FOUND AND SPEEXDSP_FOUND)

    find_package(Gsm)
    if(GSM_FOUND)
        set(SYSTEM_GSM_DEFAULT 1)
    endif(GSM_FOUND)

    find_package(Flite)
    if(FLITE_FOUND)
        set(SYSTEM_FLITE_DEFAULT 1)
    endif()

    find_package(HtsEngine)
    if(HTS_ENGINE_FOUND)
        set(SYSTEM_HTS_ENGINE_DEFAULT 1)
    endif()
endif()

# FlightGear build options
option(LOGGING           "Set to ON to build FlightGear with logging support (default)" ON)
option(JSBSIM_TERRAIN    "Set to ON to build FlightGear with JSBSim terrain handling code" ON)
option(SP_FDMS           "Set to ON to build FlightGear with special-purpose FDMs" ON)
option(ENABLE_UIUC_MODEL "Set to ON to build FlightGear with UIUCModel FDM" ON)
option(ENABLE_LARCSIM    "Set to ON to build FlightGear with LaRCsim FDM" ON)
option(ENABLE_YASIM      "Set to ON to build FlightGear with YASIM FDM (default)" ON)
option(ENABLE_JSBSIM     "Set to ON to build FlightGear with JSBSim FDM (default)" ON)
option(EVENT_INPUT       "Set to ON to build FlightGear with event-based Input support" ${EVENT_INPUT_DEFAULT})
option(ENABLE_RTI        "Set to ON to build FlightGear with RTI support" OFF)
option(ENABLE_PROFILE    "Set to ON to build FlightGear with gperftools profiling support" OFF)
option(SYSTEM_SQLITE     "Set to ON to build FlightGear with the system's SQLite3 library" OFF)
option(ENABLE_IAX        "Set to ON to build FlightGear with IAXClient/fgcom built-in (default)" ON)
option(USE_DBUS          "Set to ON to build FlightGear with DBus screensaver interaction (default on Linux)" ${USE_DBUS_DEFAULT})
option(SYSTEM_SPEEX      "Set to ON to build IAXClient with the system's speex and speexdsp library" ${SYSTEM_SPEEX_DEFAULT})
option(SYSTEM_GSM        "Set to ON to build IAXClient with the system's GSM library" ${SYSTEM_GSM_DEFAULT})
option(SYSTEM_FLITE      "Set to ON to build Flightgear with the system's Flite library" ${SYSTEM_FLITE_DEFAULT})
option(SYSTEM_HTS_ENGINE "Set to ON to build Flightgear with the system's HTS Engine library" ${SYSTEM_HTS_ENGINE_DEFAULT})
option(SYSTEM_CPPUNIT    "Set to ON to build Flightgear with the system's CppUnit library")

# additional utilities
option(ENABLE_FGELEV     "Set to ON to build the fgelev application (default)" ON)
option(WITH_FGPANEL      "Set to ON to build the fgpanel application (default)" ON)
option(ENABLE_FGVIEWER   "Set to ON to build the fgviewer application (default)" ON)
option(ENABLE_GPSSMOOTH  "Set to ON to build the GPSsmooth application (default)" ON)
option(ENABLE_TERRASYNC  "Set to ON to build the terrasync application (default)" ON)
option(ENABLE_FGJS       "Set to ON to build the fgjs application (default)" ON)
option(ENABLE_JS_DEMO    "Set to ON to build the js_demo application (default)" ON)
option(ENABLE_METAR      "Set to ON to build the metar application (default)" ON)
option(ENABLE_STGMERGE   "Set to ON to build the stgmerge application (default)" OFF)
option(ENABLE_FGCOM      "Set to ON to build the FGCom application (default)" ON)
option(ENABLE_QT         "Set to ON to build the internal Qt launcher" ON)
option(ENABLE_TRAFFIC    "Set to ON to build the external traffic generator modules" ON)
option(ENABLE_FGQCANVAS  "Set to ON to build the Qt-based remote canvas application" OFF)
option(ENABLE_DEMCONVERT "Set to ON to build the dem conversion tool (default)" ON)
option(ENABLE_HID_INPUT  "Set to ON to build HID-based input code" ${EVENT_INPUT_DEFAULT})
option(ENABLE_PLIB_JOYSTICK  "Set to ON to enable legacy joystick code (default)" ON)
option(ENABLE_COMPOSITOR "Set to ON to enable the Compositor-based Viewer" OFF)
option(ENABLE_SWIFT      "Set to ON to build the swift module" ON)


# Test-suite options.
option(ENABLE_AUTOTESTING "Set to ON to execute the test suite after building the test_suite target (default)" ON)

include (DetectArch)

# when building an OSG with commit 15ec7e2ae7a8b983ecc44e1ce7363a9a9fa7da95
# applied, we can use better link options
option(OSG_FSTREAM_EXPORT_FIXED "Set to ON if the osgDB fstream export patch is applied" OFF)

if(LOGGING)
    # nothing
else()
    set(FG_NDEBUG 1)
endif()

if(JSBSIM_TERRAIN)
   set(JSBSIM_USE_GROUNDREACTIONS 1)
endif()

if(SP_FDMS)
    set(ENABLE_SP_FDM 1)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
    include_directories(${PROJECT_SOURCE_DIR}/3rdparty/iaxclient/lib/libspeex/include)
endif()

if(ENABLE_FGCOM)
  set(ENABLE_IAX 1)
  include_directories(${PROJECT_SOURCE_DIR}/3rdparty/iaxclient/lib ) # for iaxclient.h
endif()

# Setup MSVC 3rd party directories
include( ConfigureMsvc3rdParty )

if(EVENT_INPUT)
    if(APPLE)
      add_definitions(-DWITH_EVENTINPUT)
      find_library(IOKIT_FRAMEWORK IOKit)
      list(APPEND EVENT_INPUT_LIBRARIES ${IOKIT_FRAMEWORK})
    elseif(CMAKE_SYSTEM_NAME MATCHES "Linux|FreeBSD|OpenBSD")
        if(NOT UDEV_FOUND)
            message(WARNING "UDev not found, event input is disabled!")
            set(EVENT_INPUT 0)

            # HIDraw backend for hidapi also needs UDev, so also force
            # HID-input to off in this scenario
            set(ENABLE_HID_INPUT 0)
        else()
            add_definitions(-DWITH_EVENTINPUT)
            set(EVENT_INPUT_LIBRARIES ${UDEV_LIBRARIES})
            message(STATUS "event-based input enabled. Using ${UDEV_LIBRARIES}")
        endif()
    else()
        add_definitions(-DWITH_EVENTINPUT)
    endif()

    if (ENABLE_HID_INPUT)
        message(STATUS "Enabling HID-API input")
        list(APPEND EVENT_INPUT_LIBRARIES hidapi)
    endif()
endif(EVENT_INPUT)

if (ENABLE_SWIFT)
# DBUS

    # our local FindDBus.cmake file uses pkg-config on non-Windows
    # we want to ensure our local prefixes are searched, so set this
    set(PKG_CONFIG_USE_CMAKE_PREFIX_PATH 1)

    # unfortunately CMAKE_INSTALL_PREFIX is not searched, so add that manually
    list(APPEND CMAKE_PREFIX_PATH ${CMAKE_INSTALL_PREFIX})

    find_package(DBus)

    #libevent
    find_package(LibEvent)

    if (DBUS_FOUND AND LIBEVENT_FOUND)
        message(STATUS "SWIFT support enabled")
        list(APPEND PLATFORM_LIBS ${LIBEVENT_LIB})
        # FIXME, make this target specific, not a global include setting
        include_directories(${LIBEVENT_INCLUDE_DIR})
    else()
        message(WARNING "SWIFT support disabled, dbus and/or LibEvent not found")
        set(ENABLE_SWIFT 0)
    endif()
endif()

if (SYSTEM_CPPUNIT)
    find_package(CppUnit REQUIRED)
endif()

# check required dependencies
find_package(Boost REQUIRED)
find_package(OpenGL  REQUIRED)
find_package(OpenSceneGraph 3.2.0 REQUIRED
  osgText
  osgSim
  osgDB
  osgParticle
  osgFX
  osgUtil
  osgViewer
  osgGA
)

find_package(sentry QUIET)

if (TARGET sentry::sentry)
    set(sentry_api_key "https://c9c7ea97f65347ddab678977afecc7b4@o372956.ingest.sentry.io/5188535")
    message(STATUS "Sentry.io crash reporting enabled")
    set(HAVE_SENTRY 1)
endif()

#if(${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
#    include_directories("${CMAKE_SOURCE_DIR}/3rdparty/iaxclient/lib/libspeex/include")
#endif()

##############################################################################
## Sqlite3 setup

if (SYSTEM_SQLITE)
    find_package(SQLite3 REQUIRED)
    message(STATUS "Using system SQLite3 library")
else()
    set(SQLITE3_INCLUDED_DIR "${CMAKE_SOURCE_DIR}/3rdparty/sqlite3")
    # this target is defined in src/Navaids/CMakeLists.txt
    list(APPEND SQLITE3_LIBRARY fgsqlite3)
endif (SYSTEM_SQLITE)

# Sqlite always depends on the threading lib
list(APPEND SQLITE3_LIBRARY ${CMAKE_THREAD_LIBS_INIT})

##############################################################################
## DBus setup

if (USE_DBUS)
    include(FindPkgConfig)
    if (PKG_CONFIG_FOUND)
        pkg_check_modules(DBUS dbus-1)
    endif (PKG_CONFIG_FOUND) #if we don't have pkg-config, assume we don't have libdbus-1-dev either http://packages.debian.org/sid/libdbus-1-dev
    if (DBUS_FOUND)
        set(HAVE_DBUS 1)
        message(STATUS "Using DBus")
        include_directories( ${DBUS_INCLUDE_DIRS})
    else()
        message(STATUS "DBus not found, screensaver control disabled")
    endif (DBUS_FOUND)
else()
endif (USE_DBUS)

##############################################################################
## Qt5 setup setup
if (ENABLE_QT)
    message(STATUS "Qt launcher enabled, checking for Qt >= 5.4 / qmake")
    find_package(Qt5 5.4 COMPONENTS Widgets Gui Network Qml Quick Svg)
    if (Qt5Widgets_FOUND AND Qt5Quick_FOUND)
        message(STATUS "Will enable Qt launcher GUI")
        set(HAVE_QT 1)
        include (Translations)

        # extract the Qt root direction, since we need it for a few things
        if (TARGET Qt5::qmake)
            get_target_property(_qt5_qmake_path Qt5::qmake LOCATION)
            get_filename_component(FG_QT_BIN_DIR ${_qt5_qmake_path} DIRECTORY)
            get_filename_component(FG_QT_ROOT_DIR ${FG_QT_BIN_DIR} DIRECTORY)
        endif()

    else()
        # don't try to build FGQCanvas if Qt wasn't found correctly
        set(ENABLE_FGQCANVAS OFF)
    endif()

    # we would ideally use TARGET::Qt5GuiPrivate but Fedora broke it
    # perform a manual check instead
    # see https://bugzilla.redhat.com/show_bug.cgi?id=1846613
    set (CMAKE_REQUIRED_INCLUDES ${Qt5Gui_PRIVATE_INCLUDE_DIRS} ${Qt5Gui_INCLUDE_DIRS})
    check_include_file_cxx("private/qopenglcontext_p.h" REALLY_HAVE_QGUIPRIVATE)

    if (REALLY_HAVE_QGUIPRIVATE)
        message(STATUS "Have QtGUI private headers")
        set(ENABLE_QQ_UI 1)
    else()
        message(STATUS "QtGui private headers not available.")
    endif()
else()
    message(STATUS "Qt support disabled")
    set(ENABLE_FGQCANVAS OFF)
endif (ENABLE_QT)
##############################################################################

find_package(PLIB REQUIRED puaux pu)

# FlightGear and SimGear versions need to match major + minor
# split version string into components, note CMAKE_MATCH_0 is the entire regexp match
string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" VERSION_REGEX ${FLIGHTGEAR_VERSION} )
set(FG_VERSION_MAJOR ${CMAKE_MATCH_1})
set(FG_VERSION_MINOR ${CMAKE_MATCH_2})
set(FG_VERSION_PATCH ${CMAKE_MATCH_3})

set(MIN_SIMGEAR_VERSION "${FG_VERSION_MAJOR}.${FG_VERSION_MINOR}.0")
message(STATUS "Min Simgear version is ${MIN_SIMGEAR_VERSION}")

find_package(SimGear ${MIN_SIMGEAR_VERSION} CONFIG REQUIRED)

##############################################################################

check_include_file(unistd.h        HAVE_UNISTD_H)
check_include_file(sys/time.h      HAVE_SYS_TIME_H)
check_include_file(sys/timeb.h     HAVE_SYS_TIMEB_H)
check_include_file(windows.h       HAVE_WINDOWS_H)
check_function_exists(timegm       HAVE_TIMEGM)
check_variable_exists(daylight     HAVE_DAYLIGHT)
check_function_exists(ftime        HAVE_FTIME)
check_function_exists(gettimeofday HAVE_GETTIMEOFDAY)

if(ENABLE_PROFILE)
    find_package(GooglePerfTools REQUIRED)
    set(FG_HAVE_GPERFTOOLS 1)
    message(STATUS "Built-in profiler using gperftools available")
endif()

if(ENABLE_RTI)
    message(STATUS "RTI: ENABLED")
    set(FG_HAVE_HLA 1)
else()
    message(STATUS "RTI: DISABLED")
endif(ENABLE_RTI)

if(ENABLE_GDAL)
    find_package(GDAL 2.0.0 REQUIRED)
endif(ENABLE_GDAL)
include_directories(${GDAL_INCLUDE_DIR})

if (ENABLE_OPENMP)
    find_package(OpenMP)
    if(OPENMP_FOUND)
        message(STATUS "OpenMP: ENABLED")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    else()
        message(STATUS "OpenMP: NOT FOUND")
    endif()
else()
    message(STATUS "OpenMP: DISABLED")
endif()

if (ENABLE_SIMD)
  message(STATUS "SSE/SSE2 support: ENABLED")
else()
  message(STATUS "SSE/SSE2 support: DISABLED")
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
    set(WARNING_FLAGS_CXX "-Wall")
    set(WARNING_FLAGS_C   "-Wall")

    if (CMAKE_VERSION VERSION_LESS 3.1)
       set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    endif()

    if (X86 OR X86_64)
        set(SIMD_COMPILER_FLAGS "-msse2 -mfpmath=sse -ftree-vectorize -ftree-slp-vectorize")
    endif()

    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -DNDEBUG")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO  "-O3 -g -DNDEBUG")
endif(CMAKE_COMPILER_IS_GNUCXX)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" )
   set(WARNING_FLAGS_CXX "-Wall -Wno-overloaded-virtual \
     -Wno-redeclared-class-member \
     -Wno-inconsistent-missing-override \
     -Wno-unused-local-typedef")

     if (CMAKE_VERSION VERSION_LESS 3.1)
       set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
     endif()

    # disabled by James - these cause weridness for multi-config generators
    # since they set thge flags for all configurations
    # need to find a better way to adjust these
     
    # if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    #   set(CMAKE_C_FLAGS
    #     "${CMAKE_C_FLAGS} -O0 -fno-omit-frame-pointer -fno-inline-functions")
    #   set(CMAKE_CXX_FLAGS
    #     "${CMAKE_CXX_FLAGS} -O0 -fno-omit-frame-pointer -fno-inline-functions")
    # endif()

    # override CMake default RelWithDebInfo flags.
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -DNDEBUG")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO  "-O3 -g -DNDEBUG")

    set(SIMD_COMPILER_FLAGS "-msse2 -mfpmath=sse -ftree-vectorize -ftree-slp-vectorize")

  set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
   set(WARNING_FLAGS_C "-Wall")
endif()

if(WIN32)
    if(MSVC)
        # override CMake default RelWithDebInfo flags. This is important to ensure
        # good performance
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/Zi /O2 /Ob2 /D NDEBUG")
        set(CMAKE_C_FLAGS_RELWITHDEBINFO "/Zi /O2 /Ob2 /D NDEBUG")

        set(MSVC_FLAGS "-DNOMINMAX -D_USE_MATH_DEFINES -D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS -D__CRT_NONSTDC_NO_WARNINGS")
        if (X86)
            set(SIMD_COMPILER_FLAGS "/arch:SSE /arch:SSE2")
        endif()

        if (NOT OSG_FSTREAM_EXPORT_FIXED AND ${MSVC_VERSION} GREATER 1599)
          message(STATUS "For better linking performance, use OSG with fixed fstream header")
          # needed to avoid link errors on multiply-defined standard C++
          # symbols. This issue was fixed in OSG commit 15ec7e2ae7a8b983ecc44e1ce7363a9a9fa7da95
          set( MSVC_LD_FLAGS "/FORCE:MULTIPLE" )
        endif ()

        if (${MSVC_VERSION} GREATER 1899)
            # needed for debug builds with VS2015
            set( MSVC_FLAGS "${MSVC_FLAGS} /bigobj" )
        endif()
    endif(MSVC)

    set(NOMINMAX 1)
endif(WIN32)

set (BOOST_CXX_FLAGS "-DBOOST_BIMAP_DISABLE_SERIALIZATION -DBOOST_NO_STDLIB_CONFIG")

# append the SIMD flags if requested
if (ENABLE_SIMD)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SIMD_COMPILER_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SIMD_COMPILER_FLAGS}")

  # set for multi-configuration generators
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${SIMD_COMPILER_FLAGS}")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${SIMD_COMPILER_FLAGS}")

  set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${SIMD_COMPILER_FLAGS}")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${SIMD_COMPILER_FLAGS}")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WARNING_FLAGS_C} ${MSVC_FLAGS} -D_REENTRANT")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNING_FLAGS_CXX} ${MSVC_FLAGS} -D_REENTRANT ${BOOST_CXX_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${MSVC_LD_FLAGS}")

include_directories(${OPENSCENEGRAPH_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
    ${SIMGEAR_INCLUDE_DIRS}
    ${PLIB_INCLUDE_DIR}
    ${SQLITE3_INCLUDED_DIR} )

if (USE_AEONWAVE)
    find_package(AAX REQUIRED)
    include_directories(${OPENSCENEGRAPH_INCLUDE_DIRS}
        ${AAX_INCLUDE_DIR}
    )
else()
    include_directories(${OPENSCENEGRAPH_INCLUDE_DIRS}
        ${OPENAL_INCLUDE_DIR}
    )
endif()

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/src)
# following is needed, because config.h is include 'bare', whereas
# version.h is included as <Include/version.h> - this should be cleaned up
include_directories(${PROJECT_BINARY_DIR}/src)
include_directories(${PROJECT_BINARY_DIR}/src/Include)

include_directories(${PROJECT_SOURCE_DIR}/3rdparty/hts_engine_API/include )
include_directories(${PROJECT_SOURCE_DIR}/3rdparty/flite_hts_engine/include )

add_definitions(-DHAVE_CONFIG_H)

set(FG_TEST_SUITE_DATA "${CMAKE_SOURCE_DIR}/test_suite/test_data")

check_function_exists(mkfifo HAVE_MKFIFO)

# configure a header file to pass some of the CMake settings
# to the source code
configure_file (
    "${PROJECT_SOURCE_DIR}/src/Include/config_cmake.h.in"
    "${PROJECT_BINARY_DIR}/src/Include/config.h"
)

#and the same for the version header
configure_file (
    "${PROJECT_SOURCE_DIR}/src/Include/version.h.cmake-in"
    "${PROJECT_BINARY_DIR}/src/Include/version.h"
)

# Setup build.h header, each build
include( GenerateBuildInfo )

add_subdirectory(3rdparty)
add_subdirectory(utils)

add_subdirectory(src)
add_subdirectory(icons)
add_subdirectory(man)
add_subdirectory(package)
add_subdirectory(scripts)

#----------------------------------------------------------------------------
### MSVC startup project - ensure you can just hit build & run in MSVC
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT fgfs)

#-----------------------------------------------------------------------------
### uninstall target
#-----------------------------------------------------------------------------
CONFIGURE_FILE(
    "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)
ADD_CUSTOM_TARGET(uninstall
    "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")

# The test suite.
enable_testing()
add_subdirectory(test_suite EXCLUDE_FROM_ALL)
