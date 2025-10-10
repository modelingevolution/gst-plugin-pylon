# Changelog
All notable changes to this project will be documented in this file.

## [Unreleased]
### Fixed
- Fixed critical dual-path sequencer configuration bug in HDR mode
  * Previously, `saveSet()` was called twice per sequencer set - once after Path 0 configuration and once after Path 1 configuration
  * The second call overwrote the first path configuration, resulting in only Path 1 being saved
  * Now both paths are configured completely before calling `saveSet()` once
  * Applies to both Profile 0 and Profile 1 configuration loops
  * See gstpylon.cpp:909-958 (Profile 0) and gstpylon.cpp:960-1009 (Profile 1)
- Implemented fail-fast error handling for critical sequencer operations
  * Replaced `GST_WARNING()` calls with `throw Pylon::GenericException()` for critical failures
  * Ensures proper validation of `SequencerTriggerSource` parameter availability
  * Validates that software signals (SoftwareSignal1/2) can be set before attempting configuration
  * Validates that HDR_SEQUENCER_TRIGGER can be set for ExposureActive path
  * Properly frees memory with `g_strfreev()` before throwing exceptions

### Changed
- Updated HDR dual-path sequencer configuration (gstpylon.cpp:909-1009)
  * ALL sequencer sets now have dual-path configuration (not just the last set of each profile)
  * Path 0: Software signal for profile switching - checked first, takes priority when signal is active
    - Profile 0 sets use SoftwareSignal1 to jump to Profile 1
    - Profile 1 sets use SoftwareSignal2 to jump to Profile 0
  * Path 1: ExposureActive trigger for normal progression - fallback path, always fires after exposure
  * This ensures profile switching is checked first with automatic fallback to continue current profile
  * Frame-synchronized switching with no dropped frames
- Changed exposure time logging level from INFO to DEBUG (gstpylon.cpp:1318-1335)
  * Reduces console noise during normal operation
  * Debug logs still available via GST_DEBUG=pylonsrc:5

## [1.0.0] - 2024-08-14
## Added
- added script to generate release notes
- added python example to show use of pylonmeta from pyth
  * fixes #114

### Fixed
- NVMM image width is in byte not pixels!
  * fixes #100
- removing source while active buffers in pipeline
  * fixes lifetime issue between buffers in pipeline and src
  * fixes #54
- Proper init/terminate of pylon sdk from gst-plugin-scanner
  * avoids output of an exception message after new install
- Removed unused pylonc library dependency

### Changed
- Only export external header files to access PylonMeta


## [0.7.3] - 2024-07-30
### Added
- publish artifacts on release page

### Fixed
- build debian package in case meson/ninja already installed

## [0.7.2] - 2024-07-12
### Changed
- add debian packaging
  * CI builds for std x86/aarch64 ubuntu and debian targets
  * Building deb packates for NVIDIA Jetson is documented in the README

### Fixed
- workaround for failed tests for Ubuntu 22.04
  * fixes #111
- fix nvmm error if no camera connected
  * fixes #88 

## [0.7.1] - 2024-06-06
### Changed
- check for cuda version >= 11 to enable nvmm code
  * nvmm support implementation is only compatible to cuda >= 11
  * fixes #60
- update documentation to build on Windows
  * fixes #68
- latency update
  * src now sends a message to recompute pipeline latency when it becomes known
  * fixed by #104

### Fixed
- float features working again
  * fixes #80
- build now works with the changes in meson >= 1.3.0
  * fixes #72
- build now works with the changes in meson >= 1.4.0
- fixed CI on ubuntu 24.04
- fixed invalid buffer size with enabled chunks ( non NVMM mode ) 
  * fixed by #101
- fixed possible segfault path in NVMM error path
  * fixed by #103

### Added
- Pylon 7.5
  * supports to build the plugin with pylon 7.5 now
- Pylon 7.4
  * supports to build the plugin with pylon 7.4 now

## [0.7.0] - 2023-05-08

### Changed
- offset x/y are cached when pipeline is not playing.
  * setting the offset an ROI configured via caps is possible now
  * fixes #44

### Added
- Restructing of buffer pool management to support platform specific optimal buffer types
- NVMM support
  * This feature is automatically enabled when both the CUDA library and the DeepStream library are installed on the system.
  * If enabled pylonsrc can directly generate output buffers into nvmm, that can be used by other nvidia elements.
  * Current restrictions:
    * only runs on NVIDIA Jetson at the moment

- Pylon 7.3
  * meson supports to build the plugin with pylon 7.3 now


## [0.6.2] - 2023-04-04

### Changed
- automatic rounding of values is default
  * behavioural change
  * if a value set via gstreamer is not valid for the camera
    it will be automaticaylly roundedi
  * this can be disabled by `enable-correction=false`

- Filter gev control features to speed up introspection
  * exclude gev control dependencies

## [0.6.1] - 2023-03-27

### Changed
- Filter event data nodes
  * exclude from registration until supported in gstreamer

### Fixed
- Process feature limits for ace gige
  * extend heuristics
  * fixes #37

## [0.6.0] - 2023-03-24

### Added
- Caching infrastructure to reduce runtime of property introspection process
  * ranges and access behaviour of features are saved during introspection
  * cache data is saved in [glib_user_cache_dir](https://docs.gtk.org/glib/func.get_user_cache_dir.html)/gstpylon/
- Python bindings to access pylon image metadata from python-gstreamer scripts
  * can be enabled during configuration using meson option `-Dpython-bindings=enabled`
- gstreamer property added to automatically round properties to the nearest valid camera values
  * `enable-correction=<true/false>`  activates automatic correction of values.
- git version is reflected in the plugin version string


### Changed
- Changed codebase from mixed c/c++ to c++
  * symbol default visibility 'hidden' on all platforms
- Exclude the following feature groups from introspection until properly supported
  * SequencerControl
  * FileAccessControl
  * MultiROI
  * Events
  * Commmands

### Fixed
- Disable the `DeviceLinkSelector` on all devices
  * fixes an issue with specific dart1 models
- Concurrent start of pylonsrc from multiple processes
  * opening a device is now retried for 30s in case of multiprocess collision
  * fixes #25
- ace2/dart2/boost feature dependency introspection fixed
  * fixes #32


## [0.5.1] - 2022-12-28

### Fixed
- Properly handle cameras, where OffsetX/Y is readonly after applying startup settings ( Fixes #26)


## [0.5.0] - 2022-12-09

### Added
- Access to all pylon image metadata
  * GrabResult
    * BlockID
    * ImageNumber
    * SkippedImages
    * OffsetX/Y
    * Camera Timestamp
  * Chunkdata
    * all enabled chunks are added as key/value GstStructure elements
  * for sample user code see the [show_meta](tests/examples/pylon/show_meta.c) example
- Generation of includes, library and pkg-config files to access the GstMeta data of the plugin
- Camera properties accessible by integer based selectors are now accessible as gstreamer properties
  * some properties e.g. ActionGroupMask are selected by an integer index. Support for these properties is now integrated.

### Changed
- Breaking change for width/height fixation:
  * old: prefer min(1080P, camera.max)
  * new: prefer current camera value after user-set and pfs-file
- Startup time on some camera models extended up to ~5s
  * This is a side effect of the fixes to properly capture the absolute min/max values of a property.
  * The first gst-inspect-1.0 after compilation/installation will block twice as long.
  * A caching infrastructure to skip this time on subsequent usages of pylonsrc is scheduled for the next release.

### Fixed
- Properties have now proper flags to allow changing in PLAYING state if valid for pylon.
- Plugin uses only a single 'pylonsrc' debug channel (Fixes #22)
  * usage of 'default' and 'pylonsrc' channel was root cause of stability issues with extensive logging
- Detect the absolute min/max values of properties
  * the feature limits of GenICam based cameras can change depending on the operating point. The plugin now explores the min/max values possible with the current device.
- Allow generic introspection of plugin properties ( Fixes #18)
  * internal restructuring of property type system
  * for sample user code see the [list_properties](tests/examples/pylon/list_properties.c) example
- Update readme to cover exact steps to build the plugin (Fixes #23, #19, #20)
- Upstream gstreamer fix to properly detect typos in gst-launch-1.0 pipeline definitions [!2908](https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/2908)
  * Fix is available in gstreamer >= 1.21




## [0.4.0] - 2022-10-06

### Added
- Add property to control behaviour in case of capture errors. Allow to abort capture ( current behaviour ) / skip / keep bad buffer
- Add support for proper handling of a device disconnect

### Changed
- Dynamically adjust a feature's access mode. This will make a feature writable, if it's access mode depends on another features state. ( Fixes issue #12 )

### Fixed
- Fix building gst-plugin-pylon with pylon 6 in monorepo configuration ( Fixes issue #15 )


## [0.3.0] - 2022-09-13

### Added
- Support to access stream grabber parameters via `stream::` prefix
- Bayer 8bit video data support
- Add property to configure camera from a pylon feature stream file ( `pfs-location=<filename>` )
- Support to build gst-plugin-pylon as monorepo subproject to test against upcoming changes in gstreamer monorepo
- Add pygobject example to access camera and stream grabber features in the folder [examples](https://github.com/basler/gst-plugin-pylon/tree/main/tests/examples/pylon)

### Changed
- Remove fixed grab timeout of 5s. pylonsrc will now wait forever. If required timeout/underrun detection can be handled in the gstreamer pipeline. ( e.g. via queue underrun )
- Remove limititation to only map features that are streamable
  Effect is that now all accessible features of a camera are mapped to gstreamer properties
- Output more information in case of missing selection of a camera. Now: serial number, model name and user defined name.
- Run all tests in github actions

### Fixed
- Allow loading of default user set on some early Basler USB3Vision cameras


## [0.2.0] - 2022-08-09
### Added
- Initial public release

