#include "AudioSetupComponent.h"


LiveScrollingAudioDisplay::LiveScrollingAudioDisplay()
    : AudioVisualiserComponent(1)
{
    setSamplesPerBlock(32);  // adjust speed
    setBufferSize(1024);     // history size
}

void LiveScrollingAudioDisplay::audioDeviceAboutToStart(AudioIODevice*)
{
    clear();
}

void LiveScrollingAudioDisplay::audioDeviceStopped()
{
    clear();
}

void LiveScrollingAudioDisplay::audioDeviceIOCallback(const float** inputChannelData, int numInputChannels,
                                                      float** outputChannelData, int numOutputChannels,
                                                      int numberOfSamples)
{
    for (int i = 0; i < numberOfSamples; ++i)
    {
        float inputSample = 0;
        for (int chan = 0; chan < numInputChannels; ++chan)
            if (const float* inputChannel = inputChannelData[chan])
                inputSample += inputChannel[i];
        inputSample *= 2.0f;
        pushSample(&inputSample, 1);
    }

    for (int j = 0; j < numOutputChannels; ++j)
        if (float* outputChannel = outputChannelData[j])
            zeromem(outputChannel, sizeof(float) * (size_t)numberOfSamples);
}


AudioSetupComponent::AudioSetupComponent(AudioDeviceManager* audioDeviceManager)
    : audioDeviceManager(audioDeviceManager)
{
    addAndMakeVisible(audioSetupComp = new AudioDeviceSelectorComponent(
        *(audioDeviceManager), 0, 256, 0, 256, false, false, true, false));
    addAndMakeVisible(liveAudioScroller);
    audioDeviceManager->addAudioCallback(&liveAudioScroller);
}

AudioSetupComponent::~AudioSetupComponent()
{
    audioDeviceManager->removeAudioCallback(&liveAudioScroller);
}

void AudioSetupComponent::resized()
{
    Rectangle<int> area(getLocalBounds());
    liveAudioScroller.setBounds(area.removeFromTop(100));

    if (audioSetupComp)
        audioSetupComp->setBounds(area.reduced(4).removeFromTop(proportionOfHeight(0.65f)));
}