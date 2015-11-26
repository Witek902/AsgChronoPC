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



class SetupComponent
    : public Component
{
public:
    SetupComponent()
        : bbMass(0.2)
        , detectorLength(20.0)
        , minVelocity(100.0)
        , maxVelocity(600.0)
        , detectionTreshold(8.0)
        , fireRateTreshold(1.5)
    {
        {
            /*
            TODO: general settings (language, units)
             */

            {
                Array<PropertyComponent*> comps;
                comps.add(new SliderPropertyComponent(Value(var(bbMass)), "BB mass [g]", 0.0, 10.0, 0.0));
                comps.add(new SliderPropertyComponent(Value(var(detectorLength)), "Detector length [cm]", 0.0, 10.0, 0.0));
                propertyPanel.addSection("Measurement variables", comps);
            }

            {
                Array<PropertyComponent*> comps;
                comps.add(new SliderPropertyComponent(Value(var(minVelocity)), "Min. velocity [ft/s]", 50.0, 1000.0, 1.0));
                comps.add(new SliderPropertyComponent(Value(var(maxVelocity)), "Max. velocity [ft/s]", 50.0, 1000.0, 1.0));
                comps.add(new SliderPropertyComponent(Value(var(detectionTreshold)), "Peak detection treshold", 1.0, 20.0, 0.0));
                comps.add(new SliderPropertyComponent(Value(var(detectionTreshold)), "Fire rate treshold", 1.0, 10.0, 0.0));
                propertyPanel.addSection("Detection options", comps);
            }
        }

        addAndMakeVisible(propertyPanel);
    }

    void resized() override
    {
        Rectangle<int> area(getLocalBounds());
        propertyPanel.setBounds(area);
    }

private:
    double bbMass;
    double detectorLength;

    double minVelocity;
    double maxVelocity;
    double detectionTreshold;
    double fireRateTreshold;

    PropertyPanel propertyPanel;
};


class MeasureComponent
    : public Component
    , public ButtonListener
{
public:
    static const int TITLE_FONT_SIZE = 16;

    MeasureComponent()
    {
        addAndMakeVisible(clearButton = new TextButton("Clear", "Begin a new measurement"));
        clearButton->addListener(this);

    }

    void buttonClicked(Button* button) override
    {
    }

    void resized() override
    {
        Rectangle<int> area(getLocalBounds());
        clearButton->setBounds(area.removeFromTop(40));
    }

private:
    /*

    TODO:
    * Velocity: min, max, average, standrad deviation (ft/s, m/s)
    * Fire rate: min, max, average, standrad deviation (rounds/s, rounds/min)
    * Energy, power
    */

    ScopedPointer<Button> clearButton;
};




class MainContentComponent : public TabbedComponent
{
public:
    MainContentComponent()
        : TabbedComponent(TabbedButtonBar::TabsAtTop)
    {
        addTab("Measure", Colours::whitesmoke, new MeasureComponent(), true);
        addTab("Setup", Colours::whitesmoke, new SetupComponent(), true);
        addTab("Input setup", Colours::whitesmoke, new AudioSetupComponent(), true);

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
