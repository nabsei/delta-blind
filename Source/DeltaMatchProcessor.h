#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

// Delta Match: a loudness-matched A/B comparison tool -- the second piece
// of the Delta line (audio engineer verification tools), after Delta
// itself (phase-cancellation null-test). Same audience, same sidechain/
// file-load/test-signal architecture, different DSP core.
//
// Main input  = source A (e.g. your current mix)
// Sidechain   = source B (e.g. a previous version, a reference master)
// Output      = whichever of A/B is selected, crossfaded click-free on
//               switch. When MATCH is on, B is continuously gain-trimmed
//               (slow-adapting running RMS comparison, not chasing
//               short-term dynamics) so A and B play back at the same
//               perceived level -- a preference between them then
//               reflects a real difference, not just "louder sounds
//               better", the classic bias this kind of tool exists to
//               remove.
class DeltaMatchProcessor : public juce::AudioProcessor
{
public:
    DeltaMatchProcessor();
    ~DeltaMatchProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Delta Blind"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // For the UI: long-term (meter, ~300ms-ish) level readouts and the
    // gain currently being applied to B to match A.
    float getLevelADb() const { return levelADb.load(); }
    float getLevelBDb() const { return levelBDb.load(); }
    float getMatchGainDb() const { return matchGainDb.load(); }
    bool hasSidechainSignal() const { return sidechainActive.load(); }

    void setTestSignalEnabled(bool shouldEnable) { testSignalEnabled.store(shouldEnable); }
    bool isTestSignalEnabled() const { return testSignalEnabled.load(); }

    void loadFileIntoA(const juce::File& file);
    void loadFileIntoB(const juce::File& file);
    void clearFiles();
    bool isFileModeEnabled() const { return fileModeEnabled.load(); }
    juce::String getFileNameA() const { const juce::SpinLock::ScopedLockType sl(fileLock); return fileNameA; }
    juce::String getFileNameB() const { const juce::SpinLock::ScopedLockType sl(fileLock); return fileNameB; }
    juce::String getFilePathA() const { const juce::SpinLock::ScopedLockType sl(fileLock); return filePathA; }
    juce::String getFilePathB() const { const juce::SpinLock::ScopedLockType sl(fileLock); return filePathB; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void loadFileInto(const juce::File& file, juce::AudioBuffer<float>& destBuffer,
                       bool& loadedFlag, juce::String& nameOut, juce::String& pathOut, int& playheadOut);

    double currentSampleRate = 44100.0;

    std::atomic<float> levelADb { -100.0f };
    std::atomic<float> levelBDb { -100.0f };
    std::atomic<float> matchGainDb { 0.0f };
    std::atomic<bool> sidechainActive { false };

    // Meter envelope: fast enough to read as live (~block-rate one-pole,
    // same 0.15 coefficient convention Delta's own readout uses).
    float meterEnvA = -100.0f, meterEnvB = -100.0f;
    // Match envelope: deliberately much slower (~2-3s) so the correction
    // tracks overall loudness, not transients -- chasing short-term
    // dynamics would hide real level differences instead of just
    // correcting for them.
    float matchEnvA = -100.0f, matchEnvB = -100.0f;

    juce::SmoothedValue<float> mixSmoothed;        // 0 = A, 1 = B; ramped on toggle to avoid a click
    juce::SmoothedValue<float> matchGainSmoothed;  // linear gain applied to B; ramped so MATCH on/off doesn't jump

    std::atomic<bool> testSignalEnabled { false };
    juce::int64 testSampleCounter = 0;

    juce::AudioFormatManager formatManager;
    juce::SpinLock fileLock;
    juce::AudioBuffer<float> fileBufferA, fileBufferB;
    int filePlayheadA = 0, filePlayheadB = 0;
    bool fileALoaded = false, fileBLoaded = false;
    juce::String fileNameA, fileNameB;
    juce::String filePathA, filePathB;
    std::atomic<bool> fileModeEnabled { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeltaMatchProcessor)
};
