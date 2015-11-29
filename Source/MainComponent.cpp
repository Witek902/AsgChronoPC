#include "../JuceLibraryCode/JuceHeader.h"

#include "MeasureComponent.h"
#include "SetupComponent.h"
#include "AudioSetupComponent.h"

class MainContentComponent : public TabbedComponent
{
public:
    MainContentComponent()
        : TabbedComponent(TabbedButtonBar::TabsAtTop)
    {
        sharedAudioDeviceManager = new AudioDeviceManager();
        sharedAudioDeviceManager->initialise(1, 0, 0, true, String(), 0);

        MeasureComponent* mc = new MeasureComponent(sharedAudioDeviceManager);
        addTab("Measure", Colours::whitesmoke, mc, true);
        addTab("Setup", Colours::whitesmoke, new SetupComponent(mc), true);
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