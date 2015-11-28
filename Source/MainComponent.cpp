#ifndef MAINCOMPONENT_H_INCLUDED
#define MAINCOMPONENT_H_INCLUDED

#include <mutex>
#include <string.h>
#include "../JuceLibraryCode/JuceHeader.h"
#include "../Builds/AsgChronoLib/Counter.h"


namespace {

const float METERS_TO_FEET = 3.2808f;

} // namespace

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
    AudioSetupComponent(AudioDeviceManager* audioDeviceManager)
        : audioDeviceManager(audioDeviceManager)
    {
        addAndMakeVisible(audioSetupComp = new AudioDeviceSelectorComponent(
            *(audioDeviceManager), 0, 256, 0, 256, false, false, true, false));
        addAndMakeVisible(liveAudioScroller);
        audioDeviceManager->addAudioCallback(&liveAudioScroller);
    }

    ~AudioSetupComponent()
    {
        audioDeviceManager->removeAudioCallback(&liveAudioScroller);
    }

    void resized() override
    {
        Rectangle<int> area(getLocalBounds());
        liveAudioScroller.setBounds(area.removeFromTop(100));

        if (audioSetupComp)
            audioSetupComp->setBounds(area.reduced(4).removeFromTop(proportionOfHeight(0.65f)));
    }

private:
    AudioDeviceManager* audioDeviceManager;
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
        , fireRateTreshold(1.3)
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
    , public AudioIODeviceCallback
    , public Timer
{
public:
    MeasureComponent(AudioDeviceManager* audioDeviceManager)
        : audioDeviceManager(audioDeviceManager)
        , font(Font::getDefaultMonospacedFontName(), 24.0f, Font::bold)
    {
        advancedView = true;

        addAndMakeVisible(historyTextBox);
        historyTextBox.setReadOnly(true);
        historyTextBox.setMultiLine(true);
        historyTextBox.setColour(TextEditor::outlineColourId, Colours::grey);
        historyTextBox.setFont(Font(Font::getDefaultMonospacedFontName(), 14.0f, Font::plain));

        addAndMakeVisible(clearButton = new TextButton("Clear", "Begin a new measurement"));
        clearButton->addListener(this);

        addAndMakeVisible(advancedViewButton);
        advancedViewButton.setButtonText("Advanced view");


        addAndMakeVisible(velocityTitleLabel);
        addAndMakeVisible(velocityLabel);
        velocityTitleLabel.setText("Velocity [ft/s]", dontSendNotification);
        velocityLabel.setFont(font);

        addAndMakeVisible(rofTitleLabel);
        addAndMakeVisible(rofLabel);
        rofTitleLabel.setText("Rate of fire [BB/min]", dontSendNotification);
        rofLabel.setFont(font);

        Reset();

        audioDeviceManager->addAudioCallback(this);
        counter.SetCallback(std::bind(&MeasureComponent::OnAsgEvent, this));
        startTimer(200);
    }

    ~MeasureComponent()
    {
        audioDeviceManager->removeAudioCallback(this);
    }

    // overrides AudioIODeviceCallback ============================================================

    void audioDeviceAboutToStart(AudioIODevice* device) override
    {
        sampleRate = device->getCurrentSampleRate();
        counter.GetConfig().sampleRate = sampleRate;
    }

    void audioDeviceStopped() override
    {
        sampleRate = 0;
    }

    void audioDeviceIOCallback(const float** inputChannelData, int numInputChannels,
                               float** outputChannelData, int numOutputChannels,
                               int numSamples) override
    {
        if (buffer.size() < numSamples)
            buffer.resize(numSamples + 256);

        for (int i = 0; i < numSamples; ++i)
        {
            float inputSample = 0.0f;
            for (int chan = 0; chan < numInputChannels; ++chan)
                if (const float* inputChannel = inputChannelData[chan])
                    inputSample += inputChannel[i];
            buffer[i] = inputSample;
        }

        // TODO: this should be done on a separate thread
        {
            std::unique_lock<std::mutex> lock(asgStatsLock);
            counter.ProcessBuffer(buffer.data(), numSamples);
        }

        // we need to clear the output buffers, in case they're full of junk...
        for (int i = 0; i < numOutputChannels; ++i)
            if (outputChannelData[i] != nullptr)
                FloatVectorOperations::clear(outputChannelData[i], numSamples);
    }

    // overrides ButtonListener ===================================================================

    void buttonClicked(Button* button) override
    {
        if (button == clearButton)
            Reset();
    }

    // overrides Component ========================================================================
    void resized() override
    {
        Rectangle<int> area(getLocalBounds());
        Rectangle<int> tmpArea;

        const int BORDER = 8;
        const int TITLE_LABEL_HEIGHT = 20;
        const int LABEL_HEIGHT = 40;
        const int SPACER_HEIGHT = 10;

        // clear button & history
        clearButton->setBounds(area.removeFromTop(60).reduced(BORDER));
        historyTextBox.setBounds(area.removeFromRight(250).reduced(BORDER));
        area.reduce(BORDER, BORDER);

        // simple view lables

        tmpArea = area;
        velocityTitleLabel.setBounds(tmpArea.removeFromTop(TITLE_LABEL_HEIGHT));
        velocityLabel.setBounds(tmpArea.removeFromTop(LABEL_HEIGHT));
        area.removeFromTop(SPACER_HEIGHT + TITLE_LABEL_HEIGHT + LABEL_HEIGHT);

        rofTitleLabel.setBounds(area.removeFromTop(TITLE_LABEL_HEIGHT));
        rofLabel.setBounds(area.removeFromTop(LABEL_HEIGHT));
        area.removeFromTop(SPACER_HEIGHT);

        // advanced view button
        advancedViewButton.setBounds(area.removeFromBottom(40));
    }

    // overrides Timer ============================================================================

    void timerCallback() override
    {
        UpdateStats();
    }

    // custom methods =============================================================================

    void SwitchToAdvancedView()
    {

    }

    void SwitchToSimpleView()
    {

    }

    void Reset()
    {
        {
            std::unique_lock<std::mutex> lock(asgStatsLock);
            counter.Reset();
        }

        velocityLabel.setText("N/A", dontSendNotification);
        rofLabel.setText("N/A", dontSendNotification);
        historyTextBox.setText("", false);
    }

    void UpdateStats()
    {
        AsgStats stats;
        {
            std::unique_lock<std::mutex> lock(asgStatsLock);
            stats = counter.GetStats();
        }

        stats.Calc();

        juce::String historyStr = "#ID      FPS      RoF\n";
        for (size_t i = 0; i < stats.history.size(); ++i)
        {
            historyStr += juce::String::formatted("#%-2i", i);

            if (stats.history[i].velocity > 0.0f)
                historyStr += juce::String::formatted("   %6.1f", stats.history[i].velocity * METERS_TO_FEET);
            else
                historyStr += "      N/A";

            if (stats.history[i].deltaTime > 0.0f)
                historyStr += juce::String::formatted("   %6.1f", 60.0f / stats.history[i].deltaTime);
            else
                historyStr += "      N/A";

            historyStr += '\n';
        }

        historyTextBox.setText(historyStr, false);

        if (stats.velocityAvg > 0.0f)
            velocityLabel.setText(juce::String::formatted("%.1f", stats.velocityAvg * METERS_TO_FEET),
                                                          dontSendNotification);
        else
            velocityLabel.setText("N/A", dontSendNotification);

        if (stats.fireRateAvg > 0.0f)
            rofLabel.setText(juce::String::formatted("%.1f", stats.fireRateAvg * 60.0f), dontSendNotification);
        else
            rofLabel.setText("N/A", dontSendNotification);
    }

    void OnAsgEvent()
    {

    }

private:
    AudioDeviceManager* audioDeviceManager;

    std::vector<float> buffer;
    double sampleRate;

    std::mutex asgStatsLock;

    AsgCounter counter;

    Font font;

    /*
    TODO:
    * Velocity: min, max, average, standrad deviation (ft/s, m/s)
    * Fire rate: min, max, average, standrad deviation (rounds/s, rounds/min)
    * Energy, power
    */

    Label velocityTitleLabel, velocityLabel;
    Label rofTitleLabel, rofLabel;

    ToggleButton advancedViewButton;
    bool advancedView;
    TextEditor historyTextBox;
    ScopedPointer<Button> clearButton;
};


class MainContentComponent : public TabbedComponent
{
public:
    MainContentComponent()
        : TabbedComponent(TabbedButtonBar::TabsAtTop)
    {
        sharedAudioDeviceManager = new AudioDeviceManager();
        sharedAudioDeviceManager->initialise(1, 0, 0, true, String(), 0);

        addTab("Measure", Colours::whitesmoke, new MeasureComponent(sharedAudioDeviceManager), true);
        addTab("Setup", Colours::whitesmoke, new SetupComponent(), true);
        addTab("Input setup", Colours::whitesmoke, new AudioSetupComponent(sharedAudioDeviceManager), true);

        setSize(600, 600);
    }

    ~MainContentComponent()
    {
        clearTabs();
    }

private:
    ScopedPointer<AudioDeviceManager> sharedAudioDeviceManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};

Component* createMainContentComponent()
{
    return new MainContentComponent();
}

#endif  // MAINCOMPONENT_H_INCLUDED
