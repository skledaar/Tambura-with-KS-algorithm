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

//#pragma once ?idk

class StringSynthesiser : private Timer
{
public:

    //konstruktor
    StringSynthesiser (double sampleRate, double frequencyInHz, int pickSpeed)
    {
        doPluckForNextBuffer.set (false);
        prepareSynthesiserState (sampleRate, frequencyInHz);
        changePickSpeed(pickSpeed);
    }

    void changePickSpeed(int pickSpeed) {
        this->pickSpeed = pickSpeed;
    }

    void changeTrzanje(bool trzanje) {
        this->trzanje = trzanje;
    }

    void changeNote(int midiNoteNumber) {
       frequencyInHz = MidiMessage::getMidiNoteInHertz(midiNoteNumber);
       prepareSynthesiserState (frequencyInHz);
    }

    void changeDecay(double decay) { this->decay = decay; }

    //e ovo je grdo, velocity treba hendlat
    //TODO: velocity (spremat? èitat iz aftertoucha? dictionary? kako u tajmer koji poziva stringPlucked?)
    //TODO: pozicija trzalice, 0.5 = sredina, 0/1 krajevi žica
    void stringPlucked()
    {
        //na poèetku iduæeg buffera (kad se pozove generateAndAddData) sviraj ton

        if (doPluckForNextBuffer.compareAndSetBool (1, 0))
        {
            amplitude = std::sin(MathConstants<float>::pi * 0.5); //* velocity);
            if (trzanje == true)
                startTimer(pickSpeed);
        }
    }

    void timerCallback()
    {   
        if (trzanje)    //ako se promijeni usred playanja
            stringPlucked();
    }

    void stringMuted()
    {
        stopTimer();
    }

    // delayLineWhole = N
    // delayLineDecimal = alpha
    void generateAndAddData (float* outBuffer, int numSamples)
    {
        if (doPluckForNextBuffer.compareAndSetBool (0, 1))
            exciteInternalBuffer();
        for (auto i = 0; i < numSamples; ++i)
        {
            auto nextPos = (pos + 1) % delayLine.size();
            float interpolatedSample = decay * 0.5f * ((1.0f - delayLineDecimal) * delayLine[nextPos] + (1.0f + delayLineDecimal) * delayLine[pos]);
            delayLine[pos] = interpolatedSample;
            outBuffer[i] += delayLine[nextPos];
            pos = nextPos;
        }
    }

private:
    //==============================================================================
    //TODO: ovo mora primati enum ovisno o instrumentu... also jel moramo imat drugaèije samplerate samplove? jel ovisi išta o tome, ako prebrzo pustimo snimljeni sample? probaj promijenit samplerate na kompu
    void prepareSynthesiserState (double sampleRate, double frequencyInHz)
    {
        savedSampleRate = sampleRate;
        this->frequencyInHz = frequencyInHz;
        delayLineWhole = (int) std::floor(sampleRate / frequencyInHz);
        delayLineDecimal = (float) (sampleRate / frequencyInHz) - (float) delayLineWhole;

        // we need a minimum delay line length to get a reasonable synthesis.
        // if you hit this assert, increase sample rate or decrease frequency!
        // 44100 / 882 (max dopušteno) = 50
        // a da jbt krene se bunit...
        // jassert (delayLineWhole > 50);

        delayLine.resize(delayLineWhole + 1);    //+1 zbog decimalnog dijela, da interpolacija radi
        std::fill (delayLine.begin(), delayLine.end(), 0.0f);
        excitationSample.resize(delayLineWhole + 1);

        loadKontraToVector();
    }

    //ummmmmmm tu šaljemo savedSampleRate i tamo ga upisujemo u savedSampleRate... vjv zato jer tamo može doæ od negdje drugdje?
    void prepareSynthesiserState (double frequencyInHz)
    {
        prepareSynthesiserState(savedSampleRate, frequencyInHz);
    }


    //ubacuje excitationSample u delayLine, pokreæe trzaj
    void exciteInternalBuffer()
    {
        jassert (delayLine.size() >= excitationSample.size());

        std::transform (excitationSample.begin(),
                        excitationSample.end(),
                        delayLine.begin(),
                        [this] (double sample) { return static_cast<float> (amplitude * sample); } );
    }

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

        //TODO: normalizirati samplove prije ili ovdje?

    }

    void loadKontraToVector() {
       //TODO: oèito, fajlove ne hardkodirat
       juce::File sample("C:/Users/josep/FER/treca/6sem/zavrsni/PluckedStringsDemo/Samples/kontraPluck1impuls.wav");
       loadWavToVector(sample);
    }

    void dampInternalBuffer() {
       std::fill (delayLine.begin(), delayLine.end(), 0.0f);
    }

    //==============================================================================
    float decay = 0.998;
    double amplitude = 0.0;
    int pickSpeed = 110;    //milisekunde, TODO: jel treba ovo bit? i jel treba onaj dolje di definira rotary bit?
    bool trzanje = true;
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
        pickSpeedRotaryLabel.setText("Brzina trzanja", juce::dontSendNotification); //zasto dontsent? nije implicitno?
        pickSpeedRotaryLabel.attachToComponent(&pickSpeedRotary, true);

        addAndMakeVisible(pickSpeedRotary);
        pickSpeedRotary.setSliderStyle(Slider::Rotary);
        pickSpeedRotary.setRange(80, 140);
        pickSpeedRotary.setValue(110);

        //TODO: double to int... ok joža, može proæ
        pickSpeedRotary.onValueChange = [this] { setPickSpeed(pickSpeedRotary.getValue()); };

        addAndMakeVisible(tremoloPickingButton);
        tremoloPickingButton.setButtonText("Trzanje");
        tremoloPickingButton.setToggleState(true, juce::dontSendNotification);
        
        tremoloPickingButton.onClick = [this] { setTrzanje(tremoloPickingButton.getToggleState());
           DBG("radi");
        };

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

        //po defaultu koristi prvi enabled midiInput
        for (auto input : midiInputs)
        {
            if (deviceManager.isMidiInputDeviceEnabled (input.identifier))
            {
                setMidiInput (midiInputs.indexOf (input));
                break;
            }
        }

        //ako ni jedan nije enablean, uzmi prvi
        if (midiInputList.getSelectedId() == 0)
            setMidiInput (0);

        addAndMakeVisible (keyboardComponent);
        keyboardState.addListener (this);

        setSize (600, 400);

        //TODO: huh? prouèi, nauèi. specify the number of input and output channels that we want to open
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
        generateStringSynths (sampleRate, pickSpeedRotary.getValue(), instrument);
        savedSampleRate = sampleRate;
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

    void resized() override
    {
        //auto xPos = 20;
        //auto yPos = 20;
        //auto yDistance = 50;

        auto area = getLocalBounds();

        midiInputList       .setBounds (area.removeFromTop (36).removeFromRight (getWidth() - 150).reduced (8));
        keyboardComponent   .setBounds (area.removeFromTop (80).reduced(8));
        pickSpeedRotary     .setBounds(area.removeFromTop(120));
        tremoloPickingButton.setBounds(area.removeFromTop(120).removeFromRight(20));
    }

private:
    
    std::unordered_set<int> pressedNotes;

    enum Instrument {
        Bisernica,
        Brac,
        Bugarija,
        Bas
    };

    //vraca sve MIDI tonove koje instrument može proizvesti
    static Array<int> getMidiRange(Instrument instrument) {
        int najnizi;
        int najvisi;

        switch (instrument) {
            case Bisernica:
                najnizi = 61;
                najvisi = 96;
                break;
            case Brac:
                najnizi = 54;
                najvisi = 85;
                break;
            case Bugarija:
                najnizi = 52;
                najvisi = 76;
                break;
            case Bas:
                najnizi = 30; //A štim, F#
                najvisi = 57; //oktava od 1. žice
                break;
        }
            
        Array<int> notes;
        for (int i = najnizi; i <= najvisi; ++i) {
           notes.add(i);
        }
        return notes;
    }

    //TODO: static? i saznat kak riješit ove magic numbers pametnije
    int getMinMidiNote() {
        int min;
        switch (instrument) {
            case Bisernica:
                min = 61;
                break;
            case Brac:
                min = 54;
                break;
            case Bugarija:
                min = 52;
                break;
            case Bas:
                min = 30;
                break;
        }
        return min;
    }

    int getMaxMidiNote() {
        int max;
        switch (instrument) {
            case Bisernica:
                max = 96;
                break;
            case Brac:
                max = 85;
                break;
            case Bugarija:
                max = 76;
                break;
            case Bas:
                max = 57;
                break;
        }
        return max;
    }

    void generateStringSynths (double sampleRate, int pickSpeed, Instrument instrument)
    {
        stringSynths.clear();
        for (int midiNote : getMidiRange(instrument))
        {
           stringSynths.add(new StringSynthesiser(
               sampleRate,
               juce::MidiMessage::getMidiNoteInHertz(midiNote),
               pickSpeed));
        }
    }

    //ovo kad korisnik promijeni instrument tijekom rada
    //poziva generateStringSynths koji èisti sve stare i pali nove
    //TODO: ovo možemo koristit i za panic button
    void setInstrument(Instrument instrument) {
        this->instrument = instrument;
        generateStringSynths(savedSampleRate, pickSpeedRotary.getValue(), instrument);
    }

    void setPickSpeed(int pickSpeed)
    {
        for (auto stringSynth : stringSynths) {
            stringSynth->changePickSpeed(pickSpeed);
        }
    }

    void setTrzanje(bool trzanje)
    {
        for (auto stringSynth : stringSynths) {
            stringSynth->changeTrzanje(trzanje);
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
        //ovo mi ne treba?? hm mozda za velocity dobit
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
    Instrument instrument = Bugarija;  //default
    float savedSampleRate;

    juce::Slider pickSpeedRotary;
    juce::Label pickSpeedRotaryLabel;

    juce::ToggleButton tremoloPickingButton;  //button ne treba label

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
//ZAPUSTENO i NEPOTREBNO: unordered_set bi trebao biti stack da dobio pravi mono playing
//DONE: promijenit da nije samo mute note nego se i damping promijeni na žici da realistiènije utihne, ok DONE ali slabije
//DONE: ubacit naš sample
//DONE: stavit da je prvi trz malo duži tajmer
//TODO: fix intonacije, ubaci onu interpolaciju linearnu
//TODO: staccato mode
//TODO: kolko mora bit minimalno dug sample da može generirat sve tonove? što ako nije, jel treba to implementirat u kodu i na koji naèin
//TODO: panic gumb koji cleara sintove i radi nove
//TODO: prebacit SVE u mono
//WRONG: mislim da moramo zaboravit na ovaj circular buffer i napravit takav da stane cijeli sample, tak æemo jedino dobit dovoljno lijepu sintezu...
//TODO: kako da pamti pickspeed i instrument pri gašenju? treba imat neku memoriju? pamti automatski?
//DONE: izbaèen StringParameters
//TODO: u generateStringSynths kako ubacit odabir instrumenta
//set je u PluckedStringsDemo klasi, change je u StringSynthesiser