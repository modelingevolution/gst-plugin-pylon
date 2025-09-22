# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a fork of the GStreamer source plugin for Basler cameras (`gst-plugin-pylon`), allowing use of any Basler 2D camera as a source element in GStreamer pipelines. The plugin uses the Basler pylon Camera Software Suite and dynamically maps all camera features to GStreamer properties.

**Note:** This is a fork and may contain modifications or diverge from the upstream repository at https://github.com/basler/gst-plugin-pylon

## Build System

The project uses Meson build system (>= 0.61) with Ninja. Key build commands:

```bash
# Setup (requires PYLON_ROOT environment variable)
export PYLON_ROOT=/opt/pylon
meson setup builddir --prefix /usr/

# Build
ninja -C builddir

# Run tests
ninja -C builddir test

# Install
sudo ninja -C builddir install
```

### Build Options

Important meson options (see `meson_options.txt`):
- `-Dpython-bindings=enabled` - Enable Python bindings for metadata access
- `-Dtests=enabled/disabled` - Enable/disable tests
- `-Dexamples=enabled/disabled` - Enable/disable examples
- `-Dprototypes=enabled/disabled` - Enable/disable prototypes

## Testing

Tests are located in `tests/` directory:
- Unit tests: `tests/check/`
- Examples: `tests/examples/`
- Run tests: `ninja -C builddir test`

## Code Formatting

The project uses clang-format for C/C++ code formatting:
- Format tool: `hooks/cpp-format <file>`
- Pre-commit hook automatically installed from `hooks/pre-commit.hook`
- Configuration in `.clang-format`

## Architecture

### Core Components

1. **Plugin Source** (`ext/pylon/`):
   - `gstpylonsrc.c/h` - Main GStreamer source element implementation
   - `gstpylonfeaturewalker.cpp` - Dynamic camera feature discovery and mapping
   - `gstpylonmeta.c/h` - Metadata handling for chunks and capture information

2. **Libraries** (`gst-libs/gst/pylon/`):
   - Public API for metadata access
   - Python bindings support (when enabled)

3. **Python Bindings** (`bindings/`):
   - PyGstPylon module for accessing metadata from Python

### Key Features

- Dynamic property mapping: Camera features prefixed with `cam::`, stream grabber parameters with `stream::`
- Selected features pattern: `cam::<featurename>-<selectorvalue>`
- Chunk and metadata support via GstMetaPylon
- NVMM support for NVIDIA GPU memory (when DeepStream is available)
- **IP Camera Support**: Connect to GigE Vision cameras via `device-ip-address` property (fork enhancement)

## Packaging

### Debian/Ubuntu Packages

Build debian packages:
```bash
ln -sfn packaging/debian
tools/patch_deb_changelog.sh
PYLON_ROOT=/opt/pylon dpkg-buildpackage -us -uc -rfakeroot
```

For NVIDIA platforms with DeepStream:
```bash
DEB_BUILD_PROFILES=nvidia PYLON_ROOT=/opt/pylon dpkg-buildpackage -us -uc -rfakeroot
```

## CI/CD

GitHub Actions workflows (`.github/workflows/`):
- `ci.yml` - Main CI pipeline for Linux (x86_64, aarch64), Windows
- `deb.yml` - Debian package building
- Configuration in `build_config.json`

## Dependencies

- GStreamer >= 1.0.0
- GLib >= 2.56.0
- Basler pylon SDK (7.5, 7.4, or 6.2 depending on platform)
- CMake (for pylon integration)
- Python3 + pybind11 (for Python bindings)