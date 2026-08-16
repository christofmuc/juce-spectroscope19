# Integrating JUCE Spectroscope

## Adding the repository

The usual parent-project layout is a pinned git submodule:

```sh
git submodule add https://github.com/christofmuc/juce-spectroscope19.git modules/juce-spectroscope19
git submodule update --init --recursive
```

Add JUCE before this repository. The parent may either provide the `juce-static` umbrella target used by JammerNetz or expose JUCE's normal `juce::` CMake targets:

```cmake
add_subdirectory(third_party/JUCE EXCLUDE_FROM_ALL)
add_subdirectory(modules/juce-spectroscope19)

target_link_libraries(MyApplication PRIVATE juce-spectroscope19)
```

When this project is not the top-level CMake project, demo, tests, and dependency fetching default to `OFF`. The parent therefore remains in control of its JUCE revision, compile definitions, and packaging.

## Choosing a target

- Link `juce-spectroscope-analysis` for FFT and tracked-pitch analysis without GUI or OpenGL code.
- Link `juce-spectroscope-ui` for the visualization; it brings in the analysis target.
- Link `juce-spectroscope19` when the application needs both or for compatibility with older integration code.

## Analyzer lifecycle

Create the analyzer in non-realtime code and retain shared ownership while workers or widgets can access it:

```cpp
auto analyzer = std::make_shared<Spectrogram>();
analyzer->prepare(sampleRate);
```

Call `prepare()` whenever the audio sample rate changes. Stop the producer and analysis worker before calling `reset()` or destroying the analyzer.

`Spectrogram::process()` accepts a `juce::AudioSourceChannelInfo`, downmixes all supplied channels to mono, and publishes complete normalized spectrum rows. In parallel it updates a logarithmic resonator bank, estimates an adaptive signal and noise level, interpolates local peaks, rejects peaks explained as harmonics of lower notes, and publishes an absolute tracked-fundamental confidence row with the same sequence number. It may perform windowing, FFTs, logarithms, pitch tracking, buffer movement, and synchronization, so it belongs on an analysis worker—not an audio callback.

Use `copySpectrumFramesAfter()` when only FFT data is needed. `copyAnalysisFramesAfter()` returns synchronized FFT and pitch rows for consumers that need both. `copyLatestPitchClass()` exposes the newest 256-sample tracked-fundamental field. Despite its compatibility name, the field is absolute rather than folded: position zero is concert A divided by eight and the row spans six octaves logarithmically. `setConcertAHz()` changes both the tuning grid and the visualization reference safely at the next analysis hop.

## Realtime-safe handoff

A host application should:

1. allocate a fixed-capacity queue and all audio blocks before starting audio;
2. copy the desired source into an available queue slot from the callback;
3. immediately drop analysis input when the queue is full;
4. perform `Spectrogram::process()` on a dedicated worker;
5. stop that worker after the audio callback has stopped and before releasing the analyzer.

The standalone demo's `DemoAnalysisWorker` is a compact reference. JammerNetz uses the same architecture with its own block size, counters, and engine lifecycle.

## Creating the widget

Construct `SpectrogramWidget` with a weakly-held analyzer source and keep the analyzer alive independently:

```cpp
class SpectrumPanel final : public juce::Component {
public:
    SpectrumPanel()
        : analyzer(std::make_shared<Spectrogram>()),
          widget(analyzer)
    {
        addAndMakeVisible(widget);
        widget.setContinuousRedrawing(true);
    }

    ~SpectrumPanel() override { widget.shutdownOpenGL(); }

    void resized() override
    {
        widget.setBounds(getLocalBounds());
    }

private:
    std::shared_ptr<Spectrogram> analyzer;
    SpectrogramWidget widget;
};
```

Continuous redrawing follows the OpenGL swap interval and therefore the display refresh rate. The analyzer retains bounded, synchronized histories of overlapping FFT and tracked-pitch frames, and each render drains the available rows into both waterfall textures. This preserves analysis-time resolution when multiple FFT hops complete between display frames. Applications that prefer manual repaint scheduling may leave continuous redrawing disabled and call `refreshData()` from a bounded timer instead.

Use `setXAxis(true)` for logarithmic frequency mapping and `setHorizontalMode(true)` for horizontal history. `setPitchColourMode(true)` keeps the physical FFT energy in greyscale and overlays circle-of-fifths colour only for temporally tracked tonal peaks. `setTrackedNoteOverlayEnabled(true)` adds frequency-aligned note, cents, and confidence diagnostics with a short release fade and confidence-ordered overlap handling. `setPitchTrackingPreset(PitchTracker::Preset::fast)`, `balanced`, or `stable` selects a coordinated response profile; Balanced is the default. `setConcertAHz()` controls the shared pitch-analysis and display reference.

## Ownership and shutdown

Recommended destruction order:

1. stop or detach the audio-device callback;
2. prevent any further queue writes;
3. signal and join the analysis worker;
4. stop continuous redrawing or UI timers;
5. destroy the OpenGL widget;
6. release the analyzer.

Avoid asynchronous callbacks that capture raw widget or analyzer pointers. The supplied widget uses safe component pointers for message-thread status delivery and owns OpenGL resources exclusively on the OpenGL thread.

## Failure behavior

OpenGL context or shader creation failure disables the visualization and reports a status message; it should not affect the host's audio engine. Hosts should likewise treat queue overflow as lost visualization data rather than an audio failure.
