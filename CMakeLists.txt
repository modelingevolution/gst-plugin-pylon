cmake_minimum_required(VERSION 3.20)
project(gst-plugin-pylon-wrapper)

# This is a CMake wrapper for the Meson-based gst-plugin-pylon project
include(ExternalProject)

# Check if PYLON_ROOT is set
if(NOT DEFINED ENV{PYLON_ROOT})
    message(WARNING "PYLON_ROOT environment variable not set. gst-plugin-pylon may fail to build.")
    message(WARNING "Set it with: export PYLON_ROOT=/opt/pylon")
endif()

# Determine the build directory
set(PYLON_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/pylon-build)
set(PYLON_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/pylon-install)

# Configure and build using Meson
ExternalProject_Add(gst-plugin-pylon-meson
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    BINARY_DIR ${PYLON_BUILD_DIR}
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env PYLON_ROOT=$ENV{PYLON_ROOT}
                      meson setup ${PYLON_BUILD_DIR} ${CMAKE_CURRENT_SOURCE_DIR}
                      --prefix=${PYLON_INSTALL_DIR}
                      -Dtests=disabled
                      -Dexamples=disabled
    BUILD_COMMAND ninja -C ${PYLON_BUILD_DIR}
    INSTALL_COMMAND ninja -C ${PYLON_BUILD_DIR} install
    BUILD_ALWAYS FALSE
    BUILD_BYPRODUCTS ${PYLON_INSTALL_DIR}/lib/gstreamer-1.0/gstpylonsrc.so
)

# Create a custom target that other targets can depend on
add_custom_target(gst-plugin-pylon ALL
    DEPENDS gst-plugin-pylon-meson
)

# Export the plugin location for use by other CMakeLists
set(GST_PYLON_PLUGIN_PATH ${PYLON_INSTALL_DIR}/lib/gstreamer-1.0/gstpylonsrc.so PARENT_SCOPE)
set(GST_PYLON_PLUGIN_DIR ${PYLON_INSTALL_DIR}/lib/gstreamer-1.0 PARENT_SCOPE)
