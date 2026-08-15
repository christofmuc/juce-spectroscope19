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
- **A4 reference** sets the tuning reference from 415 Hz to 466 Hz. For example, set it to 444 Hz when analysing an ensemble tuned to A4 = 444 Hz.

Pitch analysis runs beside the FFT on the analysis worker. A bank of 144 logarithmic resonators covers six octaves at 24 bins per octave. Each resonator integrates approximately six cycles, balancing semitone separation against arpeggio response; onset time therefore decreases with frequency. Local maxima are measured against adaptive signal and noise levels, interpolated between bins, and associated over time. Peaks explained as integer harmonics of a lower tracked fundamental are removed from the colour mask. Every FFT history row has a synchronized 256-sample absolute log-frequency confidence row.

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
