#ifndef MAINCOMPONENT_H_INCLUDED
#define MAINCOMPONENT_H_INCLUDED

#include <string.h>
#include "../JuceLibraryCode/JuceHeader.h"

class LiveScrollingAudioDisplay
    : public AudioVisualiserComponent
    , public AudioIODeviceCallback
{
public:
    LiveScrollingAudioDisplay() : AudioVisualiserComponent(1)
    {
        setSamplesPerBlock(32);  // adjust speed
        setBufferSize(1024);     // history size
    }

    void audioDeviceAboutToStart(AudioIODevice*) override
    {
        clear();
    }

    void audioDeviceStopped() override
    {
        clear();
    }

    void audioDeviceIOCallback(const float** inputChannelData, int numInputChannels,
                               float** outputChannelData, int numOutputChannels,
                               int numberOfSamples) override
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiveScrollingAudioDisplay)
};


class AudioSetupComponent : public Component
{
public:
    AudioSetupComponent()
    {
        sharedAudioDeviceManager = new AudioDeviceManager();
        sharedAudioDeviceManager->initialise(1, 0, 0, true, String(), 0);

        addAndMakeVisible(audioSetupComp = new AudioDeviceSelectorComponent(
            *(sharedAudioDeviceManager.get()), 0, 256, 0, 256, false, false, true, false));

        addAndMakeVisible(liveAudioScroller);
        sharedAudioDeviceManager->addAudioCallback(&liveAudioScroller);
    }

    ~AudioSetupComponent()
    {
        sharedAudioDeviceManager->removeAudioCallback(&liveAudioScroller);
    }

    void resized() override
    {
        Rectangle<int> area(getLocalBounds());
        liveAudioScroller.setBounds(area.removeFromTop(100));

        if (audioSetupComp)
            audioSetupComp->setBounds(area.reduced(4).removeFromTop(proportionOfHeight(0.65f)));
    }

private:
    ScopedPointer<AudioDeviceManager> sharedAudioDeviceManager;
    ScopedPointer<AudioDeviceSelectorComponent> audioSetupComp;
    LiveScrollingAudioDisplay liveAudioScroller;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSetupComponent)
};

class MeasureComponent
    : public Component
    , public ButtonListener
{
public:
    static const int TITLE_FONT_SIZE = 16;

    MeasureComponent()
        : bbMass(0.2)
        , minVelocity(100.0)
        , maxVelocity(600.0)
    {
        addAndMakeVisible(clearButton = new TextButton("Clear", "Begin a new measurement"));
        clearButton->addListener(this);

        {
            Array<PropertyComponent*> comps;
            comps.add(new TextPropertyComponent(Value(var("0.0")), "Velocity [ft/s]", 200, false));
            comps.add(new TextPropertyComponent(Value(var("0.0")), "Rate of fire [1/s]", 200, false));
            comps.add(new TextPropertyComponent(Value(var("0.0")), "Bullet energy [J]", 200, false));

            propertyPanel.addSection("Measured values", comps);
        }

        {
            Array<PropertyComponent*> comps;
            comps.add(new SliderPropertyComponent(Value(var(bbMass)), "BB mass [g]", 0.0, 10.0, 0.0));
            comps.add(new SliderPropertyComponent(Value(var(minVelocity)), "Min. velocity", 50.0, 1000.0, 1.0));
            comps.add(new SliderPropertyComponent(Value(var(maxVelocity)), "Max. velocity", 50.0, 1000.0, 1.0));

            propertyPanel.addSection("Options", comps);
        }

        addAndMakeVisible(propertyPanel);
    }

    void buttonClicked(Button* button) override
    {
    }

    void resized() override
    {
        Rectangle<int> area(getLocalBounds());
        clearButton->setBounds(area.removeFromTop(40));
        propertyPanel.setBounds(area);
    }

private:
    double bbMass;
    double minVelocity;
    double maxVelocity;


    ScopedPointer<Button> clearButton;
    PropertyPanel propertyPanel;
};




class MainContentComponent : public TabbedComponent
{
public:
    MainContentComponent()
        : TabbedComponent(TabbedButtonBar::TabsAtTop)
    {
        addTab("Measure", Colours::whitesmoke, new MeasureComponent(), true);
        addTab("Audio setup", Colours::whitesmoke, new AudioSetupComponent(), true);

        setSize(600, 600);
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};

Component* createMainContentComponent()
{
    return new MainContentComponent();
}

#endif  // MAINCOMPONENT_H_INCLUDED
