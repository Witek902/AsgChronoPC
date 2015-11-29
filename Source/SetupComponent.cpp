#include "SetupComponent.h"
#include "Common.h"
#include "MeasureComponent.h"

class SetupFloatProperty
    : public SliderPropertyComponent
    , public SliderListener
{
public:
    SetupFloatProperty(Slider::Listener* listener,
                       float* valuePtr, const String& propertyName,
                       float rangeMin, float rangeMax, float interval, float initialValue)
        : SliderPropertyComponent(propertyName, rangeMin, rangeMax, interval, 1.0)
        , valuePtr(valuePtr)
    {
        slider.addListener(this);
        slider.addListener(listener);

        *valuePtr = initialValue;
        slider.setValue(initialValue);
    }

private:
    float* valuePtr;

    void sliderValueChanged(Slider* slider) override
    {
        if (slider == &this->slider)
        {
            *valuePtr = slider->getValue();
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SetupFloatProperty)
};


SetupComponent::SetupComponent(MeasureComponent* measureComponent)
    : measureComponent(measureComponent)
{
    {
        /*
        TODO: general settings (language, units)
        */

        {
            Array<PropertyComponent*> comps;
            comps.add(new SetupFloatProperty(this, &bbMass, "BB mass [g]", 0.0f, 10.0f, 0.0f, 0.2f));
            comps.add(new SetupFloatProperty(this, &detectorLength, "Detector length [cm]", 1.0f, 100.0f, 0.1f, 20.0f));
            propertyPanel.addSection("Measurement variables", comps);
        }

        {
            Array<PropertyComponent*> comps;
            comps.add(new SetupFloatProperty(this, &minVelocity, "Min. velocity [ft/s]", 50.0f, 1000.0f, 1.0f, 100.0f));
            comps.add(new SetupFloatProperty(this, &maxVelocity, "Max. velocity [ft/s]", 50.0f, 1000.0f, 1.0f, 600.0f));
            comps.add(new SetupFloatProperty(this, &detectionTreshold, "Peak detection treshold", 1.0f, 20.0f, 0.01f, 7.0f));
            comps.add(new SetupFloatProperty(this, &fireRateTreshold, "Fire rate treshold", 1.0f, 3.0f, 0.01f, 1.25f));
            propertyPanel.addSection("Detection options", comps);
        }
    }



    addAndMakeVisible(propertyPanel);
}

void SetupComponent::resized()
{
    Rectangle<int> area(getLocalBounds());
    propertyPanel.setBounds(area.reduced(8));
}

void SetupComponent::sliderValueChanged(Slider* slider)
{
    measureComponent->UpdateConfig(this);
}