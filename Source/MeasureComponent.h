#pragma once

#include <mutex>
#include <string.h>
#include "../JuceLibraryCode/JuceHeader.h"
#include "../Builds/AsgChronoLib/Counter.h"

class SetupComponent;

class MeasureComponent
    : public Component
    , public ButtonListener
    , public AudioIODeviceCallback
    , public Timer
{
public:
    MeasureComponent(AudioDeviceManager* audioDeviceManager);
    ~MeasureComponent();

    // overrides AudioIODeviceCallback ============================================================
    void audioDeviceAboutToStart(AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallback(const float** inputChannelData, int numInputChannels,
                               float** outputChannelData, int numOutputChannels,
                               int numSamples) override;

    // overrides ButtonListener ===================================================================
    void buttonClicked(Button* button) override;

    // overrides Component ========================================================================
    void resized() override;

    // overrides Timer ============================================================================
    void timerCallback() override;

    // custom methods =============================================================================
    void Reset();
    void UpdateStats();
    void OnAsgEvent();
    void UpdateConfig(SetupComponent* setupComponent);

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

    TextEditor advancedViewTextBox;

    ToggleButton advancedViewButton;
    bool advancedView;
    TextEditor historyTextBox;
    ScopedPointer<Button> clearButton;
};
