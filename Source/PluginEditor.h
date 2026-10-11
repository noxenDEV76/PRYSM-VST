#pragma once
#include "PluginProcessor.h"

/** PRYSM editor. The interface is laid out at a fixed logical size and scaled uniformly,
    so it stays crisp and readable at any window size. */
class PrysmEditor : public juce::AudioProcessorEditor,
                    public juce::FileDragAndDropTarget,
                    private juce::Timer,
                    private juce::ChangeListener
{
public:
    explicit PrysmEditor (PrysmProcessor&);
    ~PrysmEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray& files, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    class Impl;
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    PrysmProcessor& proc;
    std::unique_ptr<Impl> impl;
    juce::ComponentBoundsConstrainer constrainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrysmEditor)
};
