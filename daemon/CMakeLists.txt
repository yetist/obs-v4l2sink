find_package(PkgConfig REQUIRED)

pkg_check_modules(
  GLIB REQUIRED
  gio-unix-2.0
  gobject-2.0
  libkmod
)

include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(SOURCES
  v4l2sink-daemon.c
  dbus-generated.c
  main.c
)

add_gdbus_codegen(
  SOURCES
  dbus-generated
  com.obsproject
  com.obsproject.v4l2sink.xml
  )

set_property(SOURCE dbus-generated.h PROPERTY SKIP_AUTOGEN ON)
set_property(SOURCE dbus-generated.c PROPERTY SKIP_AUTOGEN ON)

add_executable(
  v4l2sink-daemon
  ${SOURCES}
  )

target_link_libraries(
  v4l2sink-daemon
  ${GLIB_LDFLAGS}
)

install(TARGETS v4l2sink-daemon
    DESTINATION lib/obs-v4l2sink
)
