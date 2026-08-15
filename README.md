# Introduction

This is a pretty OpenGL spectroscope for the awesome [JUCE](https://github.com/WeAreROLI/JUCE) cross platform C++ programming framework.

## Usage

This repository can be built directly or included as a git submodule in a parent project such as [JammerNetz](https://github.com/christofmuc/JammerNetz). A parent build may provide its own `juce-static` target; in that mode this project neither fetches nor compiles a second JUCE checkout.

The CMake build exposes three targets:

- `juce-spectroscope-analysis`: headless FFT analysis;
- `juce-spectroscope-ui`: OpenGL visualization;
- `juce-spectroscope19`: compatibility target linking both.

Shader validation is optional. Enable `JUCE_SPECTROSCOPE_VALIDATE_SHADERS` to use an installed `glslangValidator`; configuration never downloads a moving tool archive.

## Standalone build and demo

Top-level builds fetch the pinned JUCE revision used by JammerNetz and enable both the analyzer tests and microphone-input demo by default:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The `JuceSpectroscopeDemo` executable displays the selected audio input without doing FFT or OpenGL work on the audio callback. Use `JUCE_SPECTROSCOPE_BUILD_DEMO`, `JUCE_SPECTROSCOPE_BUILD_TESTS`, and `JUCE_SPECTROSCOPE_FETCH_JUCE` to control standalone configuration explicitly.

The former [juce-spectroscope19-ci](https://github.com/christofmuc/juce-spectroscope19-ci) super-project is retained only as historical reference. Its demo and AppVeyor responsibilities now live here.

## Example

Here is a screenshot from the original demo rendering a YouTube performance of [Pergolesi's Stabat mater](https://www.youtube.com/watch?v=FjJ02agjjdo):

![A picture of the rendering of the spectrogram](Screenshot.png)

## Third party libraries used

Please understand that this software uses the following third party libraries, and you are implicitly accepting their license terms as well when using this software. Please visit the links and familarize yourself with their conditions. 

For the sake of easy accessibility, a standalone CMake build downloads and uses the following components:

  1. The awesome [JUCE](https://juce.com/) library for cross-platform C++ development.
  2. CMake for integrating the analyzer and UI targets into a parent project.
  3. An optional installed [glslangValidator](https://github.com/KhronosGroup/glslang) for build-time shader validation.

## Licensing

As some substantial work has gone into the development of this and related software, I decided to offer a dual license - AGPL, see the LICENSE.md file for the details, for everybody interested in how this works and willing to spend some time her- or himself on this, and a commercial MIT license available from me on request. Thus I can help the OpenSource community without blocking possible commercial applications.

## Contributing

All pull requests and issues welcome, I will try to get back to you as soon as I can. Due to the dual licensing please be aware that I will need to request transfer of copyright on accepting a PR. 

## About the author

Christof is a lifelong software developer having worked in various industries, and can't stop his programming hobby anyway. 
