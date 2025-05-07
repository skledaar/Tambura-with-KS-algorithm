/*
  ==============================================================================

   This file is part of the JUCE framework examples.
   Copyright (c) Raw Material Software Limited

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   to use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
   REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
   AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
   INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
   LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
   OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
   PERFORMANCE OF THIS SOFTWARE.

  ==============================================================================
*/

/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

 name:             PluckedStringsDemo
 version:          1.0.0
 vendor:           JUCE
 website:          http://juce.com
 description:      Simulation of a plucked string sound.

 dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats,
                   juce_audio_processors, juce_audio_utils, juce_core,
                   juce_data_structures, juce_events, juce_graphics,
                   juce_gui_basics, juce_gui_extra
 exporters:        xcode_mac, vs2022, linux_make, androidstudio, xcode_iphone

 moduleFlags:      JUCE_STRICT_REFCOUNTEDPOINTER=1

 type:             Component
 mainClass:        PluckedStringsDemo

 useLocalCopy:     1

 END_JUCE_PIP_METADATA

*******************************************************************************/

#pragma once


//==============================================================================
/**
    A very basic generator of a simulated plucked string sound, implementing
    the Karplus-Strong algorithm.

    Not performance-optimised!
*/
class StringSynthesiser : private Timer
{
public:
    //==============================================================================
    /** Constructor.

        @param sampleRate      The audio sample rate to use.
        @param frequencyInHz   The fundamental frequency of the simulated string in
                               Hertz.
        @param pickSpeed       Picking speed in seconds.
    */
    StringSynthesiser (double sampleRate, double frequencyInHz, int pickSpeed)
    {
        doPluckForNextBuffer.set (false);
        prepareSynthesiserState (sampleRate, frequencyInHz);
        changePickSpeed(pickSpeed);
    }

    //==============================================================================
    /** Excite the simulated string by plucking it at a given position.

        @param pluckPosition The position of the plucking, relative to the length
                             of the string. Must be between 0 and 1.
    */

    void changePickSpeed(int newPickSpeed) {
        pickSpeed = newPickSpeed;
    }

    void changeNote(int midiNoteNumber) {
       frequencyInHz = MidiMessage::getMidiNoteInHertz(midiNoteNumber);
       prepareSynthesiserState (frequencyInHz);
    }

    void stringPlucked ()
    {

        // we choose a very simple approach to communicate with the audio thread:
        // simply tell the synth to perform the plucking excitation at the beginning
        // of the next buffer (= when generateAndAddData is called the next time).

        if (doPluckForNextBuffer.compareAndSetBool (1, 0))
        {
            // plucking in the middle gives the largest amplitude;
            // plucking at the very ends will do nothing.
            amplitude = std::sin (MathConstants<float>::pi * 0.5);
            startTimer(pickSpeed);
        }
    }

    void timerCallback()
    {
        stringPlucked();
    }

    void stringMuted()
    {
        stopTimer();
    }

    //==============================================================================
    /** Generate next chunk of mono audio output and add it into a buffer.

        @param outBuffer  Buffer to fill (one channel only). New sound will be
                          added to existing content of the buffer (instead of
                          replacing it).
        @param numSamples Number of samples to generate (make sure that outBuffer
                          has enough space).
    */
    void generateAndAddData (float* outBuffer, int numSamples)
    {
        if (doPluckForNextBuffer.compareAndSetBool (0, 1))
            exciteInternalBuffer();

        // cycle through the delay line and apply a simple averaging filter
        for (auto i = 0; i < numSamples; ++i)
        {
            auto nextPos = (pos + 1) % delayLine.size();

            delayLine[nextPos] = (float) (decay * 0.5 * (delayLine[nextPos] + delayLine[pos]));
            outBuffer[i] += delayLine[pos];

            pos = nextPos;
        }
    }

private:
    //==============================================================================
    void prepareSynthesiserState (double sampleRate, double frequencyInHz)
    {
        this->savedSampleRate = sampleRate;
        this->frequencyInHz = frequencyInHz;
        auto delayLineLength = (size_t) roundToInt (sampleRate / frequencyInHz);

        // we need a minimum delay line length to get a reasonable synthesis.
        // if you hit this assert, increase sample rate or decrease frequency!
        jassert (delayLineLength > 50);

        delayLine.resize (delayLineLength);
        std::fill (delayLine.begin(), delayLine.end(), 0.0f);

        excitationSample.resize (delayLineLength);

        // as the excitation sample we use random noise between -1 and 1
        // (as a simple approximation to a plucking excitation)

        std::generate (excitationSample.begin(),
                       excitationSample.end(),
                       [] { return (Random::getSystemRandom().nextFloat() * 2.0f) - 1.0f; } );
    }

    void prepareSynthesiserState (double frequencyInHz)
    {
        prepareSynthesiserState(savedSampleRate, frequencyInHz);
    }


    void exciteInternalBuffer()
    {
        // fill the buffer with the precomputed excitation sound (scaled with amplitude)

        jassert (delayLine.size() >= excitationSample.size());

        std::transform (excitationSample.begin(),
                        excitationSample.end(),
                        delayLine.begin(),
                        [this] (double sample) { return static_cast<float> (amplitude * sample); } );
    }

    void dampInternalBuffer() {
       // fill the buffer with the precomputed excitation sound (scaled with
       // amplitude)

       std::fill (delayLine.begin(), delayLine.end(), 0.0f);
    }

    //==============================================================================
    const double decay = 0.998;
    double amplitude = 0.0;
    int pickSpeed = 100;    //milisekunde
    double frequencyInHz;
    double savedSampleRate;

    Atomic<int> doPluckForNextBuffer;

    std::vector<float> excitationSample, delayLine;
    size_t pos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StringSynthesiser)
};

//==============================================================================
class PluckedStringsDemo final : public AudioAppComponent,
                                 //public juce::Component,
                                 private juce::MidiInputCallback,
                                 private juce::MidiKeyboardStateListener {
public:
   PluckedStringsDemo() : keyboardComponent(keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
       #ifdef JUCE_DEMO_RUNNER
        , AudioAppComponent (getSharedAudioDeviceManager (0, 2))
       #endif
    {
       
        addAndMakeVisible(pickSpeedRotaryLabel);
        pickSpeedRotaryLabel.setText("Brzina trzanja:", juce::dontSendNotification);
        pickSpeedRotaryLabel.attachToComponent (&pickSpeedRotary, true);


        addAndMakeVisible(pickSpeedRotary);
        pickSpeedRotary.setSliderStyle(Slider::Rotary);
        pickSpeedRotary.setRange(20, 400);

        pickSpeedRotary.onValueChange = [this] { setPickSpeed(pickSpeedRotary.getValue()); };

        

        addAndMakeVisible (midiInputListLabel);
        midiInputListLabel.setText ("MIDI Input:", juce::dontSendNotification);
        midiInputListLabel.attachToComponent (&midiInputList, true);

        addAndMakeVisible (midiInputList);
        midiInputList.setTextWhenNoChoicesAvailable ("No MIDI Inputs Enabled");
        auto midiInputs = juce::MidiInput::getAvailableDevices();

        juce::StringArray midiInputNames;

        for (auto input : midiInputs)
            midiInputNames.add (input.name);

        midiInputList.addItemList (midiInputNames, 1);
        midiInputList.onChange = [this] { setMidiInput (midiInputList.getSelectedItemIndex()); };

        // find the first enabled device and use that by default
        for (auto input : midiInputs)
        {
            if (deviceManager.isMidiInputDeviceEnabled (input.identifier))
            {
                setMidiInput (midiInputs.indexOf (input));
                break;
            }
        }

        // if no enabled devices were found just use the first one in the list
        if (midiInputList.getSelectedId() == 0)
            setMidiInput (0);

        addAndMakeVisible (keyboardComponent);
        keyboardState.addListener (this);

        setSize (600, 400);

        // specify the number of input and output channels that we want to open
        auto audioDevice = deviceManager.getCurrentAudioDevice();
        auto numInputChannels  = (audioDevice != nullptr ? audioDevice->getActiveInputChannels() .countNumberOfSetBits() : 0);
        auto numOutputChannels = jmax (audioDevice != nullptr ? audioDevice->getActiveOutputChannels().countNumberOfSetBits() : 2, 2);

        setAudioChannels (numInputChannels, numOutputChannels);
    }

    ~PluckedStringsDemo() override
    {
        keyboardState.removeListener (this);
        deviceManager.removeMidiInputDeviceCallback (juce::MidiInput::getAvailableDevices()[midiInputList.getSelectedItemIndex()].identifier, this);
        shutdownAudio();
    }

    //==============================================================================
    void prepareToPlay (int /*samplesPerBlockExpected*/, double sampleRate) override
    {
        generateStringSynths (sampleRate, pickSpeedRotary.getValue());
    }
    
    void prepareToPlay (int /*samplesPerBlockExpected*/, double sampleRate, int pickSpeed)
    {
        generateStringSynths (sampleRate, pickSpeed);
    }

    void getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill) override
    {
        bufferToFill.clearActiveBufferRegion();

        for (auto channel = 0; channel < bufferToFill.buffer->getNumChannels(); ++channel)
        {
            auto* channelData = bufferToFill.buffer->getWritePointer (channel, bufferToFill.startSample);

            if (channel == 0)
            {
                for (auto synth : stringSynths)
                    synth->generateAndAddData (channelData, bufferToFill.numSamples);
            }
            else
            {
                memcpy (channelData,
                        bufferToFill.buffer->getReadPointer (0),
                        ((size_t) bufferToFill.numSamples) * sizeof (float));
            }
        }
    }

    void releaseResources() override
    {
        stringSynths.clear();
    }

    //==============================================================================
    void paint (Graphics&) override {}

    void resized() override
    {
        //auto xPos = 20;
        //auto yPos = 20;
        //auto yDistance = 50;

        auto area = getLocalBounds();

        midiInputList    .setBounds (area.removeFromTop (36).removeFromRight (getWidth() - 150).reduced (8));
        keyboardComponent.setBounds (area.removeFromTop (80).reduced(8));
        pickSpeedRotary.setBounds(area.removeFromTop(120));
    }

private:
    
    //TODO: ovo bi trebao biti ordered set da dobio pravi mono playing
    std::unordered_set<int> pressedNotes;

    struct StringParameters
    {
        StringParameters (int midiNote)
            : frequencyInHz (MidiMessage::getMidiNoteInHertz (midiNote)),
              lengthInPixels ((int) (760 / (frequencyInHz / MidiMessage::getMidiNoteInHertz (42))))
        {}

        double frequencyInHz;
        int lengthInPixels;
    };

    static Array<StringParameters> getDefaultStringParameters()
    {   
        //ovo je array neke Fis dur pentatonike, mislim da je 60 = c1
        //return Array<StringParameters> (42, 44, 46, 49, 51, 54, 56, 58, 61, 63, 66, 68, 70);
       return Array<StringParameters>(66, 71, 76, 81);
    }

    void generateStringSynths (double sampleRate, int pickSpeed)
    {
        stringSynths.clear();
        stringSynths.add(new StringSynthesiser (sampleRate, StringParameters(66).frequencyInHz, pickSpeed));
        //for (auto stringParams : getDefaultStringParameters())
        //{
        //    stringSynths.add (new StringSynthesiser (sampleRate, stringParams.frequencyInHz, pickSpeed));
        //}
    }

    void setPickSpeed(int pickSpeed)
    {
       stringSynths.getUnchecked(0)->changePickSpeed(pickSpeed);
    }

    void setMidiInput(int index) {
       auto list = juce::MidiInput::getAvailableDevices();

       deviceManager.removeMidiInputDeviceCallback(
           list[lastInputIndex].identifier, this);

       auto newInput = list[index];

       if (!deviceManager.isMidiInputDeviceEnabled(newInput.identifier))
          deviceManager.setMidiInputDeviceEnabled(newInput.identifier, true);

       deviceManager.addMidiInputDeviceCallback(newInput.identifier, this);
       midiInputList.setSelectedId(index + 1, juce::dontSendNotification);

       lastInputIndex = index;
    }

    // These methods handle callbacks from the midi device + on-screen keyboard..

    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override
    {
        const juce::ScopedValueSetter<bool> scopedInputFlag (isAddingFromMidiInput, true);
        keyboardState.processNextMidiEvent (message);
    }

    void handleNoteOn (juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override
    {
        auto m = juce::MidiMessage::noteOn (midiChannel, midiNoteNumber, velocity);
        pressedNotes.insert(midiNoteNumber);
        //OVO NEK OSTANE ZA SAD, ne gledamo ništa samo nek svira isti ton
        stringSynths.getUnchecked(0)->changeNote(midiNoteNumber);
        stringSynths.getUnchecked(0)->stringPlucked();
    }

    void handleNoteOff (juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float /*velocity*/) override
    {
        auto m = juce::MidiMessage::noteOff (midiChannel, midiNoteNumber);
        pressedNotes.erase(midiNoteNumber);
        if (pressedNotes.empty())
            stringSynths.getUnchecked(0)->stringMuted();
    }

    //==============================================================================
    OwnedArray<StringSynthesiser> stringSynths;

    juce::Slider pickSpeedRotary;
    juce::Label pickSpeedRotaryLabel;

    juce::AudioDeviceManager deviceManager;
    juce::ComboBox midiInputList;
    juce::Label midiInputListLabel;
    int lastInputIndex = 0;
    bool isAddingFromMidiInput = false;

    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboardComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluckedStringsDemo)
};

//TODO: ne znam, jebe ga sad to kaj sam mu dodao mijenjanje note. to u biti ne bi smio radit ja nego tamo neki prepareToPlay jer on ima pristup sampleRateu, ja nemambui