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

    void changeDecay(double decay) { this->decay = decay; }

    //e ovo je grdo, velocity treba hendlat
    void stringPlucked()//(float velocity)
    {

        // we choose a very simple approach to communicate with the audio thread:
        // simply tell the synth to perform the plucking excitation at the beginning
        // of the next buffer (= when generateAndAddData is called the next time).

        if (doPluckForNextBuffer.compareAndSetBool (1, 0))
        {
            // plucking in the middle gives the largest amplitude;
            // plucking at the very ends will do nothing.
            amplitude = std::sin(MathConstants<float>::pi * 0.5); //* velocity);
            startTimer(pickSpeed);
        }
    }

    void timerCallback()
    {   
        //TODO: imati u dictionaryju velocityje ili stalno dohvaæat zbog aftertoucha?
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
    //ovo je public funkcija jer... ju poziva juce?
    //ovo je float jer...?
    void generateAndAddData (float* outBuffer, int numSamples)
    {
        if (doPluckForNextBuffer.compareAndSetBool (0, 1))
            exciteInternalBuffer();
        //delayLineWhole = N
        //delayLineDecimal = alpha

        //numSamples == delayLine.size() kaj ne? ili outBuffer.size()?
        //nebitno, ne smiješ koristit. u inspiracijskom kodu se koristi samo za for petlju, a "i" se koristi za outbuffer samo!
        for (auto i = 0; i < numSamples; ++i)
        {
            //bedastoæe iz matlaba
            //moramo dodat numSamples da ne dobijemo nešto negativno
            //int i1 = (pos - delayLineWhole + numSamples) % numSamples;
            //int i2 = (pos - delayLineWhole + numSamples - 1) % numSamples;
            auto nextPos = (pos + 1) % delayLine.size();
            //TODO: e tu uglavit linear ono govno alfa plus blabla
            //TODO: decay je malo èudan, eh, ak imam decay ne trebam ovih 0.5
            //float interpolatedSample = (1.f - alpha) * delayLine[nextPos] + alpha * delayLine[pos];
            //s ovom obiènom malo pucketa kad kreæe... hm al dobro
            //float interpolatedSample = decay * 0.5f * (delayLine[nextPos] + delayLine[pos]);
            //pa ak radiš interpolaciju makni 0.5f jooooj
            // e krckanje nije do sampla... nego do ove interpolacije hmmmm
            //float interpolatedSample = decay * 0.5f * ((1.0f - delayLineDecimal) * delayLine[nextPos] + (1.0f + delayLineDecimal) * delayLine[pos]);
            float interpolatedSample = decay * 0.5f * ((1.0f - delayLineDecimal) * delayLine[nextPos] + (1.0f + delayLineDecimal) * delayLine[pos]);
            //delayLine[nextPos] = (float)(decay * 0.5 * (delayLine[nextPos] + delayLine[pos]));
            //ZNAÈI SAMO JE OVO TREBALO ZAMIJENIT, pisao sam unaprijed i onda se popikavao... ajde kvragu
            delayLine[pos] = interpolatedSample;

            //outBuffer[i] += delayLine[pos];
            //TODO: zaš nemrem pretvorit += u =? onda radi samo za najviši ton... možda ga ja skratim za sve ko za najviši???
            outBuffer[i] += delayLine[nextPos];
            //ovo radi ok DBG(1.0 - alpha);
            pos = nextPos;
        }
    }

private:
    //==============================================================================
    //TODO: ovo mora primati enum ovisno o instrumentu... also jel moramo imat drugaèije samplerate samplove? jel ovisi išta o tome, ako prebrzo pustimo snimljeni sample? probaj promijenit samplerate na kompu
    void prepareSynthesiserState (double sampleRate, double frequencyInHz)
    {
        //jel moguæe da se ovo samo jednom vrti???
        //DBG("prepare za frekvenciju " << frequencyInHz);
        savedSampleRate = sampleRate;
        this->frequencyInHz = frequencyInHz;
        //aha, ovo raèuna svaki put kad promijenim visinu tona i nekad pukne
        //ako je sample rate 44100Hz, frequencyInHz maksimalno smije biti 882 (a2 samo)
        //ova varijabla odreðuje visinu tona
        delayLineWhole = (int) std::floor(sampleRate / frequencyInHz);
        delayLineDecimal = (float) (sampleRate / frequencyInHz) - (float) delayLineWhole;
        //DBG(delayLineLength);
        //DBG(this->delayLineDecimal);

        // we need a minimum delay line length to get a reasonable synthesis.
        // if you hit this assert, increase sample rate or decrease frequency!
        // 44100 / 882 (max dopušteno) = 50
        // a da jbt krene se bunit...
        // jassert (delayLineWhole > 50);

        // ovo nam smeta da dobijemo sintezu zvuka s impulsom tambure
        delayLine.resize(delayLineWhole + 1);    //+1 zbog decimalnog dijela, da interpolacija radi
        std::fill (delayLine.begin(), delayLine.end(), 0.0f);

        //ovo nam smeta da dobijemo sintezu zvuka s impulsom tambure
        excitationSample.resize(delayLineWhole + 1);

        // as the excitation sample we use random noise between -1 and 1
        // (as a simple approximation to a plucking excitation)
        // ovo mijenjamo našom funkcijom za unos samplea

        //std::generate (excitationSample.begin(),
        //               excitationSample.end(),
        //               [] { return (Random::getSystemRandom().nextFloat() * 2.0f) - 1.0f; } );

        loadKontraToVector();
        //delayLine.resize(excitationSample.size());
        //std::fill(delayLine.begin(), delayLine.end(), 0.0f);
    }

    //ummmmmmm tu šaljemo savedSampleRate i tamo ga upisujemo u savedSampleRate... vjv zato jer tamo može doæ od negdje drugdje?
    void prepareSynthesiserState (double frequencyInHz)
    {
        prepareSynthesiserState(savedSampleRate, frequencyInHz);
    }


    void exciteInternalBuffer()
    {
        // fill the buffer with the precomputed excitation sound (scaled with amplitude)
        //mislim ovo ga samo metne nutra, nema smisla da je ovo vezano za visinu/frekvenciju tona
        //TODO: ovo ublažit??? kao prvih 5 fejdat

        //DBG(delayLine.size());
        //DBG("   ");
        //DBG(excitationSample.size());
        jassert (delayLine.size() >= excitationSample.size());

        std::transform (excitationSample.begin(),
                        excitationSample.end(),
                        delayLine.begin(),
                        [this] (double sample) { return static_cast<float> (amplitude * sample); } );
    }

    // std::vector<float> loadWavToVector(const juce::File& file)
    void loadWavToVector(const juce::File& file)
    {
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();
        juce::AudioFormatReader* reader = formatManager.createReaderFor(file);
        
        juce::AudioBuffer<float> audioBuffer;
        audioBuffer.setSize((int)reader->numChannels, (int)reader->lengthInSamples);

        //excitationSample.resize((int)reader->lengthInSamples);

        //nemremo direktno jer mora bit AudioBuffer tip destinacije
        //reader->read(&audioBuffer, 0, (int)reader->lengthInSamples, 0, true, true);
        reader->read(&audioBuffer, 0, excitationSample.size(), 0, true, true);
        delete reader;

        const float *channelData = audioBuffer.getReadPointer(0);
        excitationSample.assign(channelData,
                                channelData + excitationSample.size());
        //DBG("excitationSample.size() = " << excitationSample.size());
        //auto maxElement = std::max_element(
        //    excitationSample.begin(), excitationSample.end(),
        //    [](float a, float b) { return std::abs(a) < std::abs(b); });

        //if (maxElement != excitationSample.end())
        //   DBG("Max amplitude in excitationSample = "
        //       << *maxElement << " (abs: " << std::abs(*maxElement) << ")");

    }

    void loadKontraToVector() {
       juce::File sample("C:/Users/josep/FER/treca/6sem/zavrsni/PluckedStringsDemo/Samples/kontraPluck1impuls.wav");
       loadWavToVector(sample);
    }

    void dampInternalBuffer() {
       // fill the buffer with the precomputed excitation sound (scaled with
       // amplitude)

       std::fill (delayLine.begin(), delayLine.end(), 0.0f);
    }

    //==============================================================================
    float decay = 0.998;
    double amplitude = 0.0;
    int pickSpeed = 110;    //milisekunde, TODO: jel treba ovo bit? i jel treba onaj dolje di definira rotary bit?
    double frequencyInHz;
    double savedSampleRate;
    int delayLineWhole;     //N
    float delayLineDecimal; //alpha

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
        pickSpeedRotary.setRange(80, 140);
        pickSpeedRotary.setValue(110);

        //double to int... ok joža
        pickSpeedRotary.onValueChange = [this] { setPickSpeed(pickSpeedRotary.getValue()); };

        

        addAndMakeVisible (midiInputListLabel);
        midiInputListLabel.setText ("MIDI ulaz:", juce::dontSendNotification);
        midiInputListLabel.attachToComponent (&midiInputList, true);

        addAndMakeVisible (midiInputList);
        midiInputList.setTextWhenNoChoicesAvailable ("Nema dostupnih MIDI uredaja");
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
    
    //pa ovo mi niš ne znaèi, nemrem to pozvat jer to automatski framework nekako pri bootu
    //void prepareToPlay (int /*samplesPerBlockExpected*/, double sampleRate, int pickSpeed)
    //{
     //   generateStringSynths (sampleRate, pickSpeed);
    //}

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
    //void paint (Graphics&) override {}

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
    
    //TODO: unordered_set bi trebao biti stack da dobio pravi mono playing
    std::unordered_set<int> pressedNotes;

    struct StringParameters
    {
        StringParameters (int midiNote)
            : frequencyInHz (MidiMessage::getMidiNoteInHertz (midiNote))
        {}

        double frequencyInHz;
        int lengthInPixels;
    };

    static Array<StringParameters> getDefaultStringParameters() {
       // mislim da je 60 = c1
       //return Array<StringParameters>(66, 71, 76, 81); // primica? ne, oktava gore od braèa
        //return Array<StringParameters>(54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75,
        //                               76, 77, 78, 79, 80, 81, 82, 83, 84, 85,
        //                               86, 87, 88, 89, 90, 91, 92, 93);
       return Array<StringParameters>(52,53,54,55,56,57,58,59,60,61,62,63,64);
    }

    static int getMinMidiNote() { return 52;
    }

    static int getMaxMidiNote() { return 64;
    }

    void generateStringSynths (double sampleRate, int pickSpeed)
    {
        stringSynths.clear();
        //stringSynths.add(new StringSynthesiser (sampleRate, StringParameters(66).frequencyInHz, pickSpeed));
        for (auto stringParams : getDefaultStringParameters())
        {
            stringSynths.add (new StringSynthesiser (sampleRate, stringParams.frequencyInHz, pickSpeed));
        }
    }

    void setPickSpeed(int pickSpeed)
    {
        for (auto stringSynth : stringSynths) {
            stringSynth->changePickSpeed(pickSpeed);
        }
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

    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override
    {
        const juce::ScopedValueSetter<bool> scopedInputFlag (isAddingFromMidiInput, true);
        keyboardState.processNextMidiEvent (message);
    }

    void handleNoteOn (juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override
    {
        //ovo mi ne treba??
        //auto m = juce::MidiMessage::noteOn (midiChannel, midiNoteNumber, velocity);
        //ni ovo
        //pressedNotes.insert(midiNoteNumber);
        //stringSynths.getUnchecked(0)->changeNote(midiNoteNumber);
        if (midiNoteNumber >= getMinMidiNote() && midiNoteNumber <= getMaxMidiNote()) { //ovo je za sad hardcoded ali mijenjaj za instrumente jel
            stringSynths.getUnchecked(midiNoteNumber - getMinMidiNote())->changeDecay(0.998);
            stringSynths.getUnchecked(midiNoteNumber - getMinMidiNote())->stringPlucked(); // velocity);
        }
    }

    void handleNoteOff (juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float /*velocity*/) override
    {
        //auto m = juce::MidiMessage::noteOff (midiChannel, midiNoteNumber);
        //pressedNotes.erase(midiNoteNumber);
        //if (pressedNotes.empty())
        //    stringSynths.getUnchecked(0)->stringMuted();
        if (midiNoteNumber >= getMinMidiNote() && midiNoteNumber <= getMaxMidiNote()) {
            stringSynths.getUnchecked(midiNoteNumber - getMinMidiNote())->changeDecay(0.9);
            stringSynths.getUnchecked(midiNoteNumber - getMinMidiNote())->stringMuted();
        }
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
//kako maknuti synth nakon kaj završi playanje? freeati samo jednog
//DONE: promijenit da nije samo mute note nego se i damping promijeni na žici da realistiènije utihne, ok DONE ali slabije
//DONE: ubacit naš sample
//DONE: stavit da je prvi trz malo duži tajmer
//TODO: fix intonacije, ubaci onu interpolaciju linearnu
//TODO: staccato mode
//TODO: kolko mora bit minimalno dug sample da može generirat sve tonove? što ako nije, jel treba to implementirat u kodu i na koji naèin
//TODO: panic gumb koji cleara sintove i radi nove
//TODO: prebacit SVE u mono
//TODO: mislim da moramo zaboravit na ovaj circular buffer i napravit takav da stane cijeli sample, tak æemo jedino dobit dovoljno lijepu sintezu...