# Building and using the standalone demo

`JuceSpectroscopeDemo` is a small desktop application that displays the selected microphone or audio-interface input. It replaces the demo formerly maintained in the archived `juce-spectroscope19-ci` repository.

The application is also a build integration test: it compiles the public analyzer and UI targets, exercises JUCE audio-device APIs, and links a runnable application bundle on every supported desktop platform. An optional Windows lifecycle test additionally creates a real OpenGL component and verifies clean shutdown on an interactive desktop.

## Requirements

- CMake 3.22 or newer;
- a C++17 compiler;
- internet access during the first standalone configuration so CMake can download the checksum-pinned JUCE archive;
- platform audio and OpenGL development libraries.

The demo does not require GLEW, the Steinberg ASIO SDK, WebKit, JACK, `juce-cmake`, or git submodules.

## Windows

Install a current Visual Studio release with the Desktop development with C++ workload, then use the native Visual Studio generator:

```powershell
cmake -S . -B build -A x64
cmake --build build --config RelWithDebInfo --parallel
ctest --test-dir build -C RelWithDebInfo --output-on-failure
```

To register the real-window OpenGL exit test, configure the same build with:

```powershell
cmake -S . -B build -DJUCE_SPECTROSCOPE_BUILD_GUI_TESTS=ON
```

This test requires an interactive Windows desktop and working OpenGL driver. It is intentionally not enabled on headless or virtualized CI runners.

Using an unconfigured MinGW shell is not supported by JUCE. The GitHub Actions workflow deliberately uses Visual Studio on Windows.

## macOS

Install the Xcode command-line tools, then configure a single-configuration Ninja or Unix Makefiles build:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The generated app bundle includes a microphone usage description. macOS will request permission when the demo first opens an input device.

## iPhone and iPad (experimental)

Build the standalone repository on a Mac with full Xcode installed (command-line
tools alone do not include the iOS SDK). The same C++ analysis and JUCE UI are
used on iOS. Configure from this repository's root, not the JammerNetz root:

```sh
cmake -S . -B build-ios -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
  -DJUCE_SPECTROSCOPE_BUILD_TESTS=OFF \
  -DBUILD_TESTING=OFF \
  -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=YOUR_TEAM_ID
open build-ios/juce-spectroscope19.xcodeproj
```

In Xcode, select the `JuceSpectroscopeDemo` scheme, choose your signing team and
enable automatic signing, then select your connected iPhone and Run. Enable
Developer Mode on the phone if Xcode requests it, and allow microphone access.
Keep team identifiers and signing credentials in your local build configuration.
If Xcode reports that the bundle identifier is unavailable, choose a unique one
locally. A free Personal Team can test on a device but requires periodic
reprovisioning.

For an unsigned compile check, replace the team argument with
`-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO` and run:

```sh
cmake --build build-ios --config Release --parallel --target JuceSpectroscopeDemo
```

For an Apple Silicon simulator build, use a separate `build-ios-simulator`
directory and `-DCMAKE_OSX_SYSROOT=iphonesimulator`. Device and simulator binaries
are different even when both have the arm64 architecture.

The `iOS standalone demo` GitHub Actions workflow compiles both SDK variants,
validates the exact generated GLSL ES shaders embedded in the app, and uploads
zipped unsigned `.app` bundles and diagnostics. The iPhone artifact is a compile
check, not an installable signed IPA; build and sign locally for your phone. The
simulator artifact can be extracted and installed on a booted Apple Silicon iOS
simulator using `xcrun simctl install booted "JUCE Spectroscope Demo.app"`.
CI does not yet launch the app or verify live audio or rendered pixels.

The iOS renderer requests OpenGL ES 3.0 and generates GLSL ES 3.00 shaders from
the shared desktop shader sources. Single-channel half-float textures preserve
linear filtering without requiring the optional full-float filtering extension.
OpenGL ES is deprecated by Apple, so this is an experimental reuse of the
existing renderer, not a Metal backend. The demo uses a fullscreen layout with
safe-area insets and wrapping controls, requests a mono microphone input, and
stops capture and continuous repainting while suspended.

Before relying on the port, test microphone permission denial, portrait and
landscape layouts, pitch colours and note labels, locking/unlocking the phone,
audio interruptions and route changes, and sustained CPU/battery usage on a
physical device. Desktop analyzer tests remain in the existing build workflow.

## Ubuntu Linux

Install the compiler, Ninja, ALSA, X11, font, and OpenGL development packages used by JUCE:

```sh
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build \
  libasound2-dev libfreetype-dev libfontconfig1-dev \
  libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev \
  libxrandr-dev libxrender-dev libxi-dev \
  libglu1-mesa-dev mesa-common-dev libegl-dev
```

Then configure, build, and test:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Demo controls

- **Audio input** toggles between the spectrogram and JUCE's input-device selector.
- **Log frequency** switches between a true logarithmic frequency axis and linear mapping. The logarithmic axis runs from the first usable FFT bin to Nyquist, giving every octave equal screen space.
- **Horizontal history** changes the waterfall orientation.
- **Pitch colours** keeps the full FFT in greyscale and overlays stable tracked notes using their position on the circle of fifths. Colour saturation still falls continuously from a note centre to neutral grey at the midpoint between neighbouring notes.
- **Tracked notes** annotates each inferred fundamental on the frequency axis. The three rows show note name, signed cents relative to the current A4 reference, and confidence; for example `F2`, `+14.0 ct`, and `70%`. Released annotations fade smoothly, and overlapping annotations are painted from least to most confident so the stable note remains readable. In horizontal-history mode only compact note names are shown: an active annotation stays at the newest right-edge FFT frame, then scrolls left with the same analysis-row timeline as the waterfall. The strongest observation is retained, and guesses that never reach 15% confidence are not archived.
- **Fast / Balanced / Stable** selects a coordinated pitch-tracking preset. Fast favours short arpeggios and quick releases, Balanced is the general-purpose default, and Stable favours sustained notes and stronger rejection of uncertain peaks.
- **A4 reference** sets the tuning reference from 415 Hz to 466 Hz. For example, set it to 444 Hz when analysing an ensemble tuned to A4 = 444 Hz.

Pitch analysis runs beside the FFT on the analysis worker. A bank of 144 logarithmic resonators covers six octaves at 24 bins per octave. The selected preset coordinates resonator integration time, peak acceptance, track attack/release, frequency following, and rendered bandwidth rather than exposing a collection of interdependent expert knobs. Local maxima are measured against adaptive signal and noise levels, interpolated between bins, and associated over time. Peaks explained as integer harmonics of a lower tracked fundamental are removed from the colour mask. Every FFT history row has a synchronized 256-sample absolute log-frequency confidence row.

The OpenGL shader uses that second data source as a colour mask: broadband energy, attacks, noise, overtones, and non-pitched detail remain greyscale, while a stable inferred fundamental receives circle-of-fifths colour. Local spectral salience and the continuous in-tune-to-grey gradient remain in the mask, so pitch colour does not erase transient or intonation information.

The status area reports microphone permission or audio-device initialization errors. If no input device is available, the window and renderer remain usable rather than terminating the application.

## Threading model

The demo intentionally illustrates the safe integration pattern:

```text
audio-device callback
    -> bounded preallocated audio queue
    -> analysis worker (downmix, FFT, logarithmic pitch tracking)
    -> bounded synchronized histories of spectrum and pitch frames
    -> VSync-driven OpenGL renderer, draining all available frames
```

The audio callback only copies samples and signals the worker. When the queue is full or an audio block exceeds the documented demo capacity, analysis input is dropped; audio processing never waits for visualization.

## Optional configuration

To build only the reusable targets and tests:

```sh
cmake -S . -B build \
  -DJUCE_SPECTROSCOPE_BUILD_DEMO=OFF \
  -DJUCE_SPECTROSCOPE_BUILD_TESTS=ON
```

To use an existing JUCE checkout or installed JUCE package, make its `juce::` CMake targets available before adding this directory and set `JUCE_SPECTROSCOPE_FETCH_JUCE=OFF`.

## Continuous integration

The repository's `Build and test` workflow performs a RelWithDebInfo build of the library and standalone demo, and runs headless analyzer tests on Windows, Ubuntu, and macOS. The module uses JUCE's recommended warning flags and treats warnings in project sources as errors, so platform-specific compilation failures are caught without building JammerNetz. The real-window OpenGL exit test remains opt-in because hosted Windows runners do not provide a dependable interactive OpenGL session.
