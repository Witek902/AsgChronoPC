#include "MeasureComponent.h"
#include "Common.h"
#include "SetupComponent.h"

MeasureComponent::MeasureComponent(AudioDeviceManager* audioDeviceManager)
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
    advancedViewButton.addListener(this);

    addAndMakeVisible(velocityTitleLabel);
    addAndMakeVisible(velocityLabel);
    velocityTitleLabel.setText("Velocity [ft/s]:", dontSendNotification);
    velocityLabel.setFont(font);

    addAndMakeVisible(rofTitleLabel);
    addAndMakeVisible(rofLabel);
    rofTitleLabel.setText("Rate of fire [BB/min]:", dontSendNotification);
    rofLabel.setFont(font);

    addChildComponent(advancedViewTextBox);
    advancedViewTextBox.setReadOnly(true);
    advancedViewTextBox.setMultiLine(true);
    advancedViewTextBox.setColour(TextEditor::outlineColourId, Colours::grey);
    advancedViewTextBox.setFont(Font(Font::getDefaultMonospacedFontName(), 14.0f, Font::plain));

    Reset();

    audioDeviceManager->addAudioCallback(this);
    counter.SetCallback(std::bind(&MeasureComponent::OnAsgEvent, this));
    startTimer(200);
}

MeasureComponent::~MeasureComponent()
{
    audioDeviceManager->removeAudioCallback(this);
}

// overrides AudioIODeviceCallback ============================================================

void MeasureComponent::audioDeviceAboutToStart(AudioIODevice* device)
{
    sampleRate = device->getCurrentSampleRate();
    counter.GetConfig().sampleRate = sampleRate;
}

void MeasureComponent::audioDeviceStopped()
{
    sampleRate = 0;
}

void MeasureComponent::audioDeviceIOCallback(const float** inputChannelData, int numInputChannels,
                                             float** outputChannelData, int numOutputChannels,
                                             int numSamples)
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

void MeasureComponent::buttonClicked(Button* button)
{
    if (button == clearButton)
    {
        {
            std::unique_lock<std::mutex> lock(asgStatsLock);
            counter.Reset();
        }

        Reset();
    }

    if (button == &advancedViewButton)
    {
        advancedView = advancedViewButton.getToggleState();
        advancedViewTextBox.setVisible(advancedView);
    }
}

// overrides Component ========================================================================
void MeasureComponent::resized()
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

    advancedViewButton.setBounds(area.removeFromBottom(30));
    area.removeFromBottom(SPACER_HEIGHT);

    advancedViewTextBox.setBounds(area);
}

// overrides Timer ============================================================================

void MeasureComponent::timerCallback()
{
    UpdateStats();
}

// custom methods =============================================================================

void MeasureComponent::Reset()
{
    velocityLabel.setText("N/A", dontSendNotification);
    rofLabel.setText("N/A", dontSendNotification);

    advancedViewTextBox.setText("");
    historyTextBox.setText("", false);
}

void MeasureComponent::UpdateStats()
{
    AsgStats stats;
    AsgCounterConfig config;

    {
        std::unique_lock<std::mutex> lock(asgStatsLock);
        stats = counter.GetStats();
        config = counter.GetConfig();
    }

    stats.Calc(config);

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


    juce::String advancedStatsStr;

    if (stats.velocityAvg > 0.0f)
    {
        velocityLabel.setText(juce::String::formatted("%.1f", stats.velocityAvg * METERS_TO_FEET),
                              dontSendNotification);

        advancedStatsStr += juce::String::formatted("Min. velocity:      %.1f ft/s\n", stats.velocityMin * METERS_TO_FEET);
        advancedStatsStr += juce::String::formatted("Max. velocity:      %.1f ft/s\n", stats.velocityMax * METERS_TO_FEET);
        advancedStatsStr += juce::String::formatted("Velocity std. dev.: %.1f ft/s\n", stats.velocityStdDev * METERS_TO_FEET);

        float energy = 0.5f * stats.velocityAvg * stats.velocityAvg * config.mass;
        advancedStatsStr += juce::String::formatted("Energy:             %.3f J\n", energy);
    }
    else
        velocityLabel.setText("N/A", dontSendNotification);

    if (stats.fireRateAvg > 0.0f)
    {
        rofLabel.setText(juce::String::formatted("%.1f", stats.fireRateAvg * 60.0f), dontSendNotification);

        advancedStatsStr += juce::String::formatted("Min. fire rate:     %.1f BB/min\n", stats.fireRateMin* 60.0f);
        advancedStatsStr += juce::String::formatted("Max. fire rate:     %.1f BB/min\n", stats.fireRateMax* 60.0f);
    }
    else
        rofLabel.setText("N/A", dontSendNotification);


    if (stats.velocityAvg > 0.0f && stats.fireRateAvg > 0.0f)
    {
        float power = stats.fireRateAvg * 0.5f * stats.velocityAvg * stats.velocityAvg * config.mass;
        advancedStatsStr += juce::String::formatted("Power:              %.3f W\n", power);
    }

    advancedViewTextBox.setText(advancedStatsStr);
}

void MeasureComponent::UpdateConfig(SetupComponent* setupComponent)
{
    std::unique_lock<std::mutex> lock(asgStatsLock);

    AsgCounterConfig& cfg = counter.GetConfig();

    cfg.mass = setupComponent->bbMass / 1000.0f;
    cfg.length = 0.01f * setupComponent->detectorLength;
    cfg.maxPeakDistance = cfg.length * cfg.sampleRate / setupComponent->minVelocity * METERS_TO_FEET;
    cfg.minPeakDistance = cfg.length * cfg.sampleRate / setupComponent->maxVelocity * METERS_TO_FEET;
    if (cfg.minPeakDistance > cfg.maxPeakDistance)
        cfg.maxPeakDistance = cfg.minPeakDistance;

    cfg.fireRateTreshold = setupComponent->fireRateTreshold;
    cfg.detectionSigma = setupComponent->detectionTreshold;

    counter.Reset();
    Reset();
}

void MeasureComponent::OnAsgEvent()
{

}