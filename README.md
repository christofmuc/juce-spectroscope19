# Introduction

This is a pretty OpenGL spectroscope for the awesome [JUCE](https://github.com/WeAreROLI/JUCE) cross platform C++ programming framework.

## Usage

This repository is meant to be included as a git submodule in a main project, see for instance [JammerNetz](https://github.com/christofmuc/JammerNetz) for an example how this is used. The parent project must provide a `juce-static` target with JUCE audio basics, core, DSP, GUI basics, graphics, and OpenGL support.

The CMake build exposes three targets:

- `juce-spectroscope-analysis`: headless FFT analysis;
- `juce-spectroscope-ui`: OpenGL visualization;
- `juce-spectroscope19`: compatibility target linking both.

Shader validation is optional. Enable `JUCE_SPECTROSCOPE_VALIDATE_SHADERS` to use an installed `glslangValidator`; configuration never downloads a moving tool archive.

## Example

In order to build this library standalone and get a working example program, there is a [separate little repository called juce-spectroscope19-ci](https://github.com/christofmuc/juce-spectroscope19-ci) that you can use. This also documents the build process.

Here is a screenshot of the program in the example repo action rendering a youtube video of a performance of [Pergolesi's Stabat mater](https://www.youtube.com/watch?v=FjJ02agjjdo):

![A picture of the rendering of the spectrogram](Screenshot.png)

## Third party libraries used

Please understand that this software uses the following third party libraries, and you are implicitly accepting their license terms as well when using this software. Please visit the links and familarize yourself with their conditions. 

For the sake of easy accessibility, the cmake build of this example software automatically downloads and uses the following components:

  1. The awesome [JUCE](https://juce.com/) library for cross-platform C++ development.
  2. CMake for integrating the analyzer and UI targets into a parent project.
  3. An optional installed [glslangValidator](https://github.com/KhronosGroup/glslang) for build-time shader validation.

## Licensing

As some substantial work has gone into the development of this and related software, I decided to offer a dual license - AGPL, see the LICENSE.md file for the details, for everybody interested in how this works and willing to spend some time her- or himself on this, and a commercial MIT license available from me on request. Thus I can help the OpenSource community without blocking possible commercial applications.

## Contributing

All pull requests and issues welcome, I will try to get back to you as soon as I can. Due to the dual licensing please be aware that I will need to request transfer of copyright on accepting a PR. 

## About the author

Christof is a lifelong software developer having worked in various industries, and can't stop his programming hobby anyway. 
