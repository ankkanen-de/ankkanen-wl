cmake_minimum_required(VERSION 3.6)
project("ankkanen-wl" C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(WLROOTS REQUIRED wlroots)
pkg_check_modules(WAYLAND_SERVER REQUIRED wayland-server)
pkg_check_modules(XKBCOMMON REQUIRED xkbcommon)
pkg_get_variable(WAYLAND_PROTOCOLS wayland-protocols pkgdatadir)
pkg_get_variable(WAYLAND_SCANNER wayland-scanner wayland_scanner)

add_executable(
    ankkanen-wl
    src/cursor.c
    src/keyboard.c
    src/layer-shell-v1.c
    src/main.c
    src/output.c
    src/seat.c
    src/xdg-shell.c
    xdg-shell-protocol.h
    wlr-layer-shell-unstable-v1-protocol.h
)

add_custom_command(
    OUTPUT xdg-shell-protocol.h
    COMMAND ${WAYLAND_SCANNER} server-header
        ${WAYLAND_PROTOCOLS}/stable/xdg-shell/xdg-shell.xml
        xdg-shell-protocol.h
)

add_custom_command(
    OUTPUT wlr-layer-shell-unstable-v1-protocol.h
    COMMAND ${WAYLAND_SCANNER} server-header
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/wlr-layer-shell-unstable-v1.xml
        wlr-layer-shell-unstable-v1-protocol.h
)

add_compile_definitions(WLR_USE_UNSTABLE)

target_link_libraries(
    ankkanen-wl
    ${WLROOTS_LIBRARIES}
    ${WAYLAND_SERVER_LIBRARIES}
    ${XKBCOMMON_LIBRARIES})

target_include_directories(
    ankkanen-wl PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${WLROOTS_INCLUDE_DIRS}
    ${WAYLAND_SERVER_INCLUDE_DIRS}
    ${XKBCOMMON_INCLUDE_DIRS})

include(GNUInstallDirs)
install(
    TARGETS ankkanen-wl
    EXPORT ankkanen-wl
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
