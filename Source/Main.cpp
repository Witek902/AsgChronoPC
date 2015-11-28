#include "../JuceLibraryCode/JuceHeader.h"

Component* createMainContentComponent();

class AsgChronoApplication  : public JUCEApplication
{
public:
    AsgChronoApplication() {}

    const String getApplicationName() override       { return ProjectInfo::projectName; }
    const String getApplicationVersion() override    { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override       { return true; }

    void initialise (const String& commandLine) override
    {
        mainWindow = new MainWindow (getApplicationName());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const String& commandLine) override
    {
    }

    class MainWindow : public DocumentWindow
    {
    public:
        MainWindow(String name)  : DocumentWindow (name,
                                                    Colours::lightgrey,
                                                    DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(createMainContentComponent(), true);
            setResizable(true, true);

            centreWithSize (getWidth(), getHeight());
            setVisible(true);

            constrainer.setMinimumSize(400, 400);
            setConstrainer(&constrainer);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        ComponentBoundsConstrainer constrainer;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    ScopedPointer<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (AsgChronoApplication)
