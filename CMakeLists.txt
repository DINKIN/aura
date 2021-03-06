CMAKE_MINIMUM_REQUIRED(VERSION 2.8.8)

PROJECT(aura)
SET(PROJECT_VERSION   0.1.2)
SET(AURA_API_VERSION  1)

enable_testing()
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)


#TODO: Move out linux-specific stuff

#Safe to enable, pool is stable
SET(AURA_USE_BUFFER_POOL yes CACHE BOOL "Enable buffer pool")

#DEVELOPER HACKS. Enable only when hacking around aura
SET(AURA_USE_LIBEVENT no CACHE BOOL "Use libevent-based eventloop")
SET(AURA_TEST_LEAKS no CACHE BOOL "Valgrind for memory leaks during testing (Runs each test twice)")
SET(AURA_TEST_USB_TRANSPORT no CACHE BOOL "Run usb/susb transport module test suite (requires hardware)")
SET(AURA_TEST_NMC_TRANSPORT no CACHE BOOL "Run Neuromatrix(r) DSP transport module test suite (requires hardware)")
SET(AURA_TEST_DUMMY yes CACHE BOOL "Run basic test suite on a dummy transport (no hw required)")
SET(AURA_TEST_BINDINGS yes CACHE BOOL "Run basic lua bindings test suite (no hw required)")
SET(AURA_COVERALLS no CACHE BOOL "Generate coveralls data")

SET(SRCS
  buffer.c
  slog.c panic.c utils.c
  transport.c eventloop.c aura.c export.c serdes.c
  retparse.c queue.c utils-linux.c
  transport-dummy.c
  transport-bench.c
  transport-sysfs-gpio.c
  packetizer.c
  crc.c
)
SET(TRANSPORTS dummy sysfs-gpio serial)

FIND_PACKAGE(PkgConfig)
PKG_CHECK_MODULES(LIBUSB libusb-1.0)
PKG_CHECK_MODULES(LUA52 lua5.2)
PKG_CHECK_MODULES(LUA51 lua5.1)
PKG_CHECK_MODULES(EASYNMC easynmc-0.1.1)
if (AURA_USE_LIBEVENT)
PKG_CHECK_MODULES(LIBEVENT libevent)
endif()

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fPIC -D_GNU_SOURCE")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")


MACRO (TODAY RESULT)
    IF (WIN32)
        EXECUTE_PROCESS(COMMAND "cmd" " /C date /T" OUTPUT_VARIABLE ${RESULT})
        string(REGEX REPLACE "(..)/(..)/..(..).*" "\\1/\\2/\\3" ${RESULT} ${${RESULT}})
    ELSEIF(UNIX)
        EXECUTE_PROCESS(COMMAND "date" "+%d/%m/%Y" OUTPUT_VARIABLE ${RESULT})
        string(REGEX REPLACE "(..)/(..)/..(..).*" "\\1/\\2/\\3" ${RESULT} ${${RESULT}})
    ELSE (WIN32)
        MESSAGE(SEND_ERROR "date not implemented")
        SET(${RESULT} 000000)
    ENDIF (WIN32)
ENDMACRO (TODAY)


#Set up some version-specific defines
EXECUTE_PROCESS(COMMAND git rev-parse HEAD
    OUTPUT_VARIABLE PROJECT_VERSION_GIT
    )
STRING(REGEX REPLACE "\n" "" PROJECT_VERSION_GIT ${PROJECT_VERSION_GIT})

SITE_NAME(AURA_BUILD_HOSTNAME)
TODAY(AURA_BUILD_DATE)
SET(PROJECT_BUILD_TAG "$ENV{USER}@${AURA_BUILD_HOSTNAME}")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAURA_VERSION='\"${PROJECT_VERSION}\"'")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAURA_VERSION_GIT='\"${PROJECT_VERSION_GIT}\"'")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAURA_BUILD_TAG='\"${PROJECT_BUILD_TAG}\"'")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAURA_BUILD_DATE='\"${AURA_BUILD_DATE}\"'")

if(AURA_USE_BUFFER_POOL)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAURA_USE_BUFFER_POOL")
endif()

#Let's use libevent if we have and should
if(LIBEVENT_FOUND AND AURA_USE_LIBEVENT)
  SET(SRCS ${SRCS} eventsys-libevent.c)
  SET(EVENTSYSTEM libevent)
else()
  SET(SRCS ${SRCS} eventsys-epoll.c)
  SET(EVENTSYSTEM epoll)
endif()

# Let's auto detect a suitable lua version
if(LUA52_FOUND)
  SET(LUA_INCLUDE_DIRS ${LUA52_INCLUDE_DIRS})
  SET(LUA_LIBRARIES ${LUA52_LIBRARIES})
  SET(LUA_FOUND y)
  EXECUTE_PROCESS(COMMAND lua5.2 ${CMAKE_SOURCE_DIR}/lua-guess-lib-install-path.lua cpath ${CMAKE_INSTALL_PREFIX} ${CMAKE_LIBRARY_PATH}
    OUTPUT_VARIABLE LUA_CPATH
    )
  EXECUTE_PROCESS(COMMAND lua5.2 ${CMAKE_SOURCE_DIR}/lua-guess-lib-install-path.lua path ${CMAKE_INSTALL_PREFIX} ${CMAKE_LIBRARY_PATH}
    OUTPUT_VARIABLE LUA_LPATH
    )
  SET(PKG_CONFIG_REQUIRES "${PKG_CONFIG_REQUIRES} lua5.2")
elseif(${LUA51_FOUND})
  SET(LUA_INCLUDE_DIRS ${LUA51_INCLUDE_DIRS})
  SET(LUA_LIBRARIES ${LUA51_LIBRARIES})
  SET(LUA_FOUND y)
  EXECUTE_PROCESS(COMMAND lua5.1 ${CMAKE_SOURCE_DIR}/lua-guess-lib-install-path.lua cpath ${CMAKE_INSTALL_PREFIX} ${CMAKE_LIBRARY_PATH}
    OUTPUT_VARIABLE LUA_CPATH
    )
  EXECUTE_PROCESS(COMMAND lua5.1 ${CMAKE_SOURCE_DIR}/lua-guess-lib-install-path.lua path ${CMAKE_INSTALL_PREFIX} ${CMAKE_LIBRARY_PATH}
    OUTPUT_VARIABLE LUA_LPATH
    )
  SET(PKG_CONFIG_REQUIRES "${PKG_CONFIG_REQUIRES} lua5.1")
endif()

if(LIBUSB_FOUND)
  SET(TRANSPORTS ${TRANSPORTS} usb)
  SET(SRCS ${SRCS} transport-usb.c usb-helpers.c)
  SET(PKG_CONFIG_REQUIRES "${PKG_CONFIG_REQUIRES} libusb-1.0")
endif()

if(LIBUSB_FOUND AND LUA_FOUND)
    SET(TRANSPORTS ${TRANSPORTS} susb)
    SET(SRCS ${SRCS} transport-susb.c)
endif()

IF(LUA_FOUND)
  SET(SRCS ${SRCS} bindings-lua.c utils-lua.c)
  SET(BINDINGS ${BINDINGS} lua)
ENDIF()

IF(EASYNMC_FOUND)
  SET(TRANSPORTS ${TRANSPORTS} nmc)
  SET(SRCS ${SRCS} transport-nmc.c ion.c)
  SET(PKG_CONFIG_REQUIRES "${PKG_CONFIG_REQUIRES} easynmc-0.1.1")
ENDIF()

INCLUDE_DIRECTORIES(
    ./include/
    ${LIBUSB_INCLUDE_DIRS}
    ${LUA_INCLUDE_DIRS}
    ${EASYNMC_INCLUDE_DIRS}
    ${LIBEVENT_INCLUDE_DIRS}
)

ADD_LIBRARY(aura OBJECT ${SRCS})
if (AURA_COVERALLS)
    include(Coveralls)
    coveralls_turn_on_coverage()
    coveralls_setup(
       "${SRCS}" # The source files.
       ON                 # If we should upload.
       )
endif()

ADD_LIBRARY(aurastatic STATIC $<TARGET_OBJECTS:aura>)
SET_TARGET_PROPERTIES(aurastatic PROPERTIES OUTPUT_NAME aura)
TARGET_LINK_LIBRARIES(aurastatic
    ${LIBUSB_LIBRARIES}
    ${LUA_LIBRARIES}
    ${EASYNMC_LIBRARIES}
    ${LIBEVENT_LIBRARIES}
)

ADD_LIBRARY(aurashared SHARED $<TARGET_OBJECTS:aura>)
SET_TARGET_PROPERTIES(aurashared PROPERTIES OUTPUT_NAME aura)
TARGET_LINK_LIBRARIES(aurashared
    ${LIBUSB_LIBRARIES}
    ${LUA_LIBRARIES}
    ${EASYNMC_LIBRARIES}
    ${LIBEVENT_LIBRARIES}
    -lrt -lm
)

message(STATUS "Generarating ${CMAKE_SOURCE_DIR}/.clang_complete")
get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
file(WRITE ${CMAKE_SOURCE_DIR}/.clang_complete "")
foreach(dir ${dirs})
  file(APPEND ${CMAKE_SOURCE_DIR}/.clang_complete "-I${dir}\n")
endforeach()
string(REPLACE "'" "" CMAKE_C_FLAGS_SPLIT ${CMAKE_C_FLAGS})
string(REPLACE " " ";" CMAKE_C_FLAGS_SPLIT ${CMAKE_C_FLAGS_SPLIT})
foreach(flag ${CMAKE_C_FLAGS_SPLIT})
# FixMe: Ugly hack
  if (NOT "${flag}" MATCHES "-Wall")
    if (NOT "${flag}" MATCHES "-g")
      if (NOT "${flag}" MATCHES "-fPIC")
        file(APPEND ${CMAKE_SOURCE_DIR}/.clang_complete "${flag}\n")
      endif()
    endif()
  endif()
endforeach()

file(WRITE ${CMAKE_SOURCE_DIR}/.core_sources "")
foreach(f ${SRCS})
  file(APPEND ${CMAKE_SOURCE_DIR}/.core_sources "${f} ")
endforeach()

include(${CMAKE_SOURCE_DIR}/tests/tests.cmake)

ADD_C_TEST_DIRECTORY(dummy dummy ${AURA_TEST_DUMMY} ${AURA_TEST_LEAKS})
ADD_SCRIPT_TEST_DIRECTORY(lua bindings lua ${AURA_TEST_BINDINGS} ${AURA_TEST_LEAKS})
ADD_C_TEST_DIRECTORY(xUSB xusb ${AURA_TEST_USB_TRANSPORT} ${AURA_TEST_LEAKS})
ADD_C_TEST_DIRECTORY(nmc nmc ${AURA_TEST_NMC_TRANSPORT} ${AURA_TEST_LEAKS})

SET_TARGET_PROPERTIES(aurashared PROPERTIES SOVERSION ${PROJECT_VERSION}
  VERSION ${AURA_API_VERSION})

message("libaura version:           ${PROJECT_VERSION}")
message("libaura git commit:        ${PROJECT_VERSION_GIT}")
message("Enabled transports:        ${TRANSPORTS}")
message("Enabled language bindings: ${BINDINGS}")
message("Install prefix:            ${CMAKE_INSTALL_PREFIX}")
message("Event system:              ${EVENTSYSTEM}")
message("Test suites to run:        ${AURA_TEST_SUITES_TO_RUN}")
message("Test coverage upload:      ${AURA_COVERALLS}")

if (AURA_USE_BUFFER_POOL)
message("Buffer pooling:            enabled")
else()
message("Buffer pooling:            disabled")
endif()

if(LUA_FOUND)
  message("lua .so dir:               ${CMAKE_INSTALL_PREFIX}/${LUA_CPATH}")
  message("lua lib dir:               ${CMAKE_INSTALL_PREFIX}/${LUA_LPATH}")
endif()

INSTALL(TARGETS aurashared LIBRARY
        DESTINATION lib/${CMAKE_LIBRARY_PATH})

INSTALL(TARGETS aurastatic ARCHIVE
        DESTINATION lib/${CMAKE_LIBRARY_PATH})

file(GLOB LUA_SCRIPTS
    "${CMAKE_SOURCE_DIR}/lua/aura/*"
)

# If we have lua - let's install all the lua stuff
if (LUA_FOUND)
INSTALL(FILES ${LUA_SCRIPTS}
       DESTINATION ${LUA_LPATH}/aura)

INSTALL(FILES lua/aura.lua
       DESTINATION ${LUA_LPATH})

#FixMe: Hack. CMake symlink handling is shitty and obscure
add_custom_command(TARGET aurashared POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:aurashared> auracore.so
)

INSTALL(FILES ${CMAKE_BINARY_DIR}/auracore.so DESTINATION ${LUA_CPATH})
endif()

# Now, let's install headers
file(GLOB AURA_HEADERS
    "${CMAKE_SOURCE_DIR}/include/aura/*"
)

file(GLOB ION_HEADERS
    "${CMAKE_SOURCE_DIR}/include/ion/*"
)

INSTALL(FILES ${AURA_HEADERS}
       DESTINATION include/${CMAKE_LIBRARY_PATH}/aura-${PROJECT_VERSION}/aura)

INSTALL(FILES ${ION_HEADERS}
       DESTINATION include/${CMAKE_LIBRARY_PATH}/ion)

#Finally, the pkg-config file
SET(PKG_CONFIG_LIBDIR
    "\${prefix}/lib/\${deb_host_multiarch}"
)
SET(PKG_CONFIG_INCLUDEDIR
    "\${prefix}/include/\${deb_host_multiarch}/aura-${PROJECT_VERSION}"
)
SET(PKG_CONFIG_LIBS
    "-L\${libdir} -l:libaura.so.${PROJECT_VERSION}"
)
SET(PKG_CONFIG_CFLAGS
    "-I\${includedir} -D_GNU_SOURCE"
)

CONFIGURE_FILE(
  "${CMAKE_CURRENT_SOURCE_DIR}/pkg-config.pc.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc"
)

INSTALL(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc"
       DESTINATION lib/${CMAKE_LIBRARY_PATH}/pkgconfig/)
