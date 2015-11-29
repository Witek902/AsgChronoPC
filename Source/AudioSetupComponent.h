#pragma once

#include "../JuceLibraryCode/JuceHeader.h"


class LiveScrollingAudioDisplay
    : public AudioVisualiserComponent
    , public AudioIODeviceCallback
{
public:
    LiveScrollingAudioDisplay();
    void audioDeviceAboutToStart(AudioIODevice*) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallback(const float** inputChannelData, int numInputChannels,
                               float** outputChannelData, int numOutputChannels,
                               int numberOfSamples) override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiveScrollingAudioDisplay)
};


class AudioSetupComponent : public Component
{
public:
    AudioSetupComponent(AudioDeviceManager* audioDeviceManager);
    ~AudioSetupComponent();
    void resized() override;

private:
    AudioDeviceManager* audioDeviceManager;
    ScopedPointer<AudioDeviceSelectorComponent> audioSetupComp;
    LiveScrollingAudioDisplay liveAudioScroller;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSetupComponent)
};