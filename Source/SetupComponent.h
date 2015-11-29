#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "../Builds/AsgChronoLib/Counter.h"

class MeasureComponent;

class SetupComponent
    : public Component
    , public Slider::Listener
{
    friend class MeasureComponent;

public:
    SetupComponent(MeasureComponent* measureComponent);
    void resized() override;
    void sliderValueChanged(Slider* slider) override;

private:
    MeasureComponent* measureComponent;

    float bbMass;             // [g]
    float detectorLength;     // [cm]
    float minVelocity;        // [ft/s]
    float maxVelocity;        // [ft/s]
    float detectionTreshold;
    float fireRateTreshold;

    PropertyPanel propertyPanel;
};
