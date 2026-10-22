#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "dsp/Engine.h"
#include "Resampler.h"

// Knob captions per algorithm (P1..P4), following what each control does in the engine.
struct AlgInfo { const char* name; const char* group; const char* p[4]; };
const AlgInfo& algInfo(int i);

class TumanProcessor : public juce::AudioProcessor {
public:
    TumanProcessor();
    ~TumanProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    using AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Tuman"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // metering for the UI
    std::atomic<float> inLevel { 0.0f }, outLevel { 0.0f };
    std::atomic<bool> clipLed { false };
    std::atomic<int> activeAlg { 0 };

private:
    std::unique_ptr<tuman::Engine> engine;
    tuman::SincResampler up, down;
    tuman::DcBlock dc;
    std::vector<float> fifoL, fifoR;   // engine-rate -> host-rate output FIFO
    size_t fifoRead = 0;
    bool bypassResample = false;
    double hostRate = 44100.0;
    int currentPreset = 0;
    int ledHold = 0;

    std::atomic<float>* pAlg = nullptr; std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pP[4] {};
    std::atomic<float>* pIn = nullptr; std::atomic<float>* pOut = nullptr;
    std::atomic<float>* pLong = nullptr; std::atomic<float>* pGate = nullptr;
    juce::LinearSmoothedValue<float> inGain, outGain;

    void runEngine(float l, float r, float& ol, float& orr);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TumanProcessor)
};
