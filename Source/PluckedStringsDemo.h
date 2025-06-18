/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

 name:             TamburaApp
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
 mainClass:        TamburaApp

 useLocalCopy:     1

 END_JUCE_PIP_METADATA

*******************************************************************************/

//#pragma once ?idk

enum Instrument { Bisernica = 1, Brac, Bugarija, Bas };

class StringSynthesiser : private Timer
{
public:

    //konstruktor
    StringSynthesiser (double sampleRate, double frequencyInHz, int pickSpeed, bool trzanje, Instrument instrument)
    {
        doPluckForNextBuffer.set (false);
        prepareSynthesiserState (sampleRate, frequencyInHz, instrument);
        changePickSpeed(pickSpeed);
        changeTrzanje(trzanje);
    }

    void changePickSpeed(int pickSpeed)
    {
        this->pickSpeed = pickSpeed;
    }

    void changePickSpeedRand(int pickSpeedRand)
    {
        this->pickSpeedRand = pickSpeedRand;
    }

    void changeVelocityRand(int velocityRand) {
       this->velocityRand = velocityRand;
    }

    void changeTrzanje(bool trzanje)
    {
        this->trzanje = trzanje;
    }

    //void changeNote(int midiNoteNumber)
    //{
    //   frequencyInHz = MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    //   prepareSynthesiserState (frequencyInHz);
    //}

    void changePlayDecay(float decay) { playDecay = decay; }

    void changeStopDecay(float decay) { stopDecay = decay; }

    void changeVelocity(float velocity) { this->velocity = velocity; }

    //e ovo je grdo, velocity treba hendlat
    //TODO: velocity (spremat? èitat iz aftertoucha? dictionary? kako u tajmer koji poziva stringPlucked?)
    //TODO: pozicija trzalice, 0.5 = sredina, 0/1 krajevi žica
    void stringPlucked()
    {
        //na poèetku iduæeg buffera (kad se pozove generateAndAddData) sviraj ton

        if (doPluckForNextBuffer.compareAndSetBool (1, 0))
        {
            //da, ovo se poziva svaki put jer se velocity može promijeniti, a i evo sad je randomness dodan
            amplitude = std::sin(MathConstants<float>::pi * 0.5 * velocity) 
                * (0.5 + (juce::Random::getSystemRandom().nextDouble() - 0.5f) * ((float)velocityRand * 0.01));
            decay = playDecay;
            if (trzanje == true)
                startTimer(pickSpeed + (float)pickSpeedRand * (juce::Random::getSystemRandom().nextDouble() - 0.5f));
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
        decay = stopDecay;
    }

    // delayLineWhole = N
    // delayLineDecimal = alpha
    void generateAndAddData (float* outBuffer, int numSamples)
    {
        if (doPluckForNextBuffer.compareAndSetBool (0, 1))
            exciteInternalBuffer();
        for (auto i = 0; i < numSamples; ++i)
        {
            auto nextPos = (pos + 1) % delayLine.size();    //u jbt ovo mi puca, integer division by zero... vjv u trenu kad se rade synthovi pa su svi na nula... TODO:
            //TODO: objasnit zaš ovo nije (1-alfa) ... (alfa)
            //TODO: objasnit u principu cijeli ovaj chunk, to je stranica ziher
            float interpolatedSample = decay * 0.5f * ((1.0f - delayLineDecimal) * delayLine[nextPos] + (1.0f + delayLineDecimal) * delayLine[pos]);
            delayLine[pos] = interpolatedSample;
            outBuffer[i] += delayLine[nextPos];
            pos = nextPos;
        }
    }

private:
    //==============================================================================
    //TODO: ovo mora primati enum ovisno o instrumentu... also jel moramo imat drugaèije samplerate samplove? jel ovisi išta o tome, ako prebrzo pustimo snimljeni sample? probaj promijenit samplerate na kompu
    void prepareSynthesiserState (double sampleRate, double frequencyInHz, Instrument instrument)
    {
        savedSampleRate = sampleRate;
        this->frequencyInHz = frequencyInHz;
        this->instrument = instrument;
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

        //loadKontraToVector();
        loadInstrumentToVector(instrument);
    }

    //ummmmmmm tu šaljemo savedSampleRate i tamo ga upisujemo u savedSampleRate... vjv zato jer tamo može doæ od negdje drugdje?
    //ovo koristimo samo u changeNote
    //void prepareSynthesiserState (double frequencyInHz)
    //{
    //    prepareSynthesiserState(savedSampleRate, frequencyInHz, instrument);
    //}


    //ubacuje excitationSample u delayLine, pokreæe trzaj
    //TODO: ak je manje od dovoljno semplova
    void exciteInternalBuffer()
    {
        //jassert (delayLine.size() >= excitationSample.size());
        //DBG("delayLine.size() " << delayLine.size());
        //DBG("excitationSample.size() " << excitationSample.size());
        
        /*
        for (size_t i = 0; i < delayLine.size(); ++i)
        {
           delayLine[i % delayLine.size()] +=
               static_cast<float>(amplitude * excitationSample[i]);
        }
        */
        
        
        for (size_t i = 0; i < delayLine.size(); ++i)
        {
           delayLine[i % delayLine.size()] =
               static_cast<float>(amplitude * excitationSample[i] *
                                  ((delayLine.size() - i) < (delayLine.size() * 0.25f)
                                       ? (delayLine.size() - i) / (delayLine.size() * 0.25f)
                                       : 1)
                                   * (i < 10 ? i / 10.0f : 1));
        }
        

        /*
        std::transform (excitationSample.begin(),
                        excitationSample.end(),
                        delayLine.begin(),
                        [this] (double sample) { return static_cast<float> (amplitude * sample); } );
        */
    }

    //TODO: ako sample nije dovoljno dugaèak (vjv nemoguæe al stavi... also stranica u zavrsnom kolko moraju bit dugacki za kvalitetan sampling
    void loadWavToVector(const juce::File& file)
    {
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();
        juce::AudioFormatReader* reader = formatManager.createReaderFor(file);
        
        juce::AudioBuffer<float> audioBuffer;
        audioBuffer.setSize((int)reader->numChannels, (int)reader->lengthInSamples);
        audioBuffer.clear();

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

    void loadInstrumentToVector(Instrument instrument)
    {
        String path;
        switch (instrument)
        {
            case Bisernica:
                path = "C:/Users/josep/FER/treca/6sem/zavrsni/PluckedStringsDemo/Samples/primPluck2.wav";
                break;
            case Brac:
                path = "C:/Users/josep/FER/treca/6sem/zavrsni/PluckedStringsDemo/Samples/bracPluck1.wav";
                break;
            case Bugarija:
                path = "C:/Users/josep/FER/treca/6sem/zavrsni/PluckedStringsDemo/Samples/kontraPluck1impuls.wav";
                break;
            case Bas:
                path = "C:/Users/josep/FER/treca/6sem/zavrsni/PluckedStringsDemo/Samples/basPluck2a.wav";
                break;
        }
        juce::File sample(path);
        loadWavToVector(sample);
    }

    void dampInternalBuffer() 
    {
       std::fill (delayLine.begin(), delayLine.end(), 0.0f);
    }

    //==============================================================================
    float decay;    //za manipulaciju odzvanjanja
    float playDecay = 0.995;
    float stopDecay = 0.9;
    float velocity = 127.0f;
    double amplitude = 0.0;
    int pickSpeed = 110;    //milisekunde, TODO: jel treba ovo bit? i jel treba onaj dolje di definira rotary bit?
    int pickSpeedRand = 0;
    int velocityRand = 0;
    bool trzanje = true;
    Instrument instrument;
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
class TamburaApp final : public AudioAppComponent,
                                 //public juce::Component,
                                 private juce::MidiInputCallback,
                                 private juce::MidiKeyboardStateListener {
public:
   TamburaApp() : keyboardComponent(keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
       #ifdef JUCE_DEMO_RUNNER
        , AudioAppComponent (getSharedAudioDeviceManager (0, 2))
       #endif
    {

        addAndMakeVisible(pickSpeedRotaryLabel);
        pickSpeedRotaryLabel.setText("Brzina trzanja", juce::dontSendNotification); //zasto dontsent? nije implicitno?
        pickSpeedRotaryLabel.attachToComponent(&pickSpeedRotary, false);

        addAndMakeVisible(pickSpeedRotary);
        pickSpeedRotary.setSliderStyle(Slider::Rotary);
        pickSpeedRotary.setTextBoxStyle(Slider::TextBoxBelow, false, 30, 15);
        pickSpeedRotary.setRange(70, 140, 1);
        pickSpeedRotary.setValue(95);

        //TODO: double to int... ok joža, može proæ
        pickSpeedRotary.onValueChange = [this] { setPickSpeed(pickSpeedRotary.getValue()); };



        addAndMakeVisible(pickSpeedRandRotaryLabel);
        pickSpeedRandRotaryLabel.setText("Varijacija brzine trzanja", juce::dontSendNotification);
        pickSpeedRandRotaryLabel.attachToComponent(&pickSpeedRandRotary, false);

        addAndMakeVisible(pickSpeedRandRotary);
        pickSpeedRandRotary.setSliderStyle(Slider::Rotary);
        pickSpeedRandRotary.setTextBoxStyle(Slider::TextBoxBelow, false, 30, 15);
        pickSpeedRandRotary.setRange(0, 100, 1);
        pickSpeedRandRotary.setValue(0);

        pickSpeedRandRotary.onValueChange = [this] { setPickSpeedRand(pickSpeedRandRotary.getValue()); };



        addAndMakeVisible(velocityRandRotaryLabel);
        velocityRandRotaryLabel.setText("Varijacija jacine trzanja", juce::dontSendNotification);
        velocityRandRotaryLabel.attachToComponent(&velocityRandRotary, false);

        addAndMakeVisible(velocityRandRotary);
        velocityRandRotary.setSliderStyle(Slider::Rotary);
        velocityRandRotary.setTextBoxStyle(Slider::TextBoxBelow, false, 30, 15);
        velocityRandRotary.setRange(0, 100, 1);
        velocityRandRotary.setValue(0);

        velocityRandRotary.onValueChange = [this] {
           setVelocityRand(velocityRandRotary.getValue());
        };



        addAndMakeVisible(playDecayRotaryLabel);
        playDecayRotaryLabel.setText("Decay sviranja", juce::dontSendNotification);
        playDecayRotaryLabel.attachToComponent(&playDecayRotary, false);

        addAndMakeVisible(playDecayRotary);
        playDecayRotary.setSliderStyle(Slider::Rotary);
        playDecayRotary.setTextBoxStyle(Slider::TextBoxBelow, false, 100, 15);
        playDecayRotary.setRange(0.8, 1);
        playDecayRotary.setSkewFactor(7);
        playDecayRotary.setValue(getDefaultPlayDecay());

        setPlayDecay(getDefaultPlayDecay());
        playDecayRotary.onValueChange = [this] { setPlayDecay(playDecayRotary.getValue()); };



        addAndMakeVisible(stopDecayRotaryLabel);
        stopDecayRotaryLabel.setText("Decay zaustavljanja", juce::dontSendNotification);
        stopDecayRotaryLabel.attachToComponent(&stopDecayRotary, false);

        addAndMakeVisible(stopDecayRotary);
        stopDecayRotary.setSliderStyle(Slider::Rotary);
        stopDecayRotary.setTextBoxStyle(Slider::TextBoxBelow, false, 100, 15);
        stopDecayRotary.setRange(0, 1);
        stopDecayRotary.setSkewFactor(3);
        stopDecayRotary.setValue(getDefaultStopDecay());

        setStopDecay(getDefaultStopDecay());    //malo glupo jer ga tu ne poziva neg već stavlja default od bisernice, dolje ga u instrument list change
        stopDecayRotary.onValueChange = [this] { setStopDecay(stopDecayRotary.getValue()); };



        addAndMakeVisible(tremoloPickingButton);
        tremoloPickingButton.setButtonText("Trzanje");
        tremoloPickingButton.setToggleState(getDefaultTrzanje(), juce::dontSendNotification);
        setTrzanje(getDefaultTrzanje());
        
        tremoloPickingButton.onClick = [this] { setTrzanje(tremoloPickingButton.getToggleState()); };

        addAndMakeVisible (midiInputListLabel);
        midiInputListLabel.setText ("MIDI ulaz:", juce::dontSendNotification);
        midiInputListLabel.attachToComponent (&midiInputList, false);

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

        addAndMakeVisible(instrumentListLabel);
        instrumentListLabel.setText("Instrument:", juce::dontSendNotification);
        instrumentListLabel.attachToComponent(&instrumentList, false);

        addAndMakeVisible(instrumentList);
        instrumentList.addItem("Bisernica", Bisernica);
        instrumentList.addItem("Brac",      Brac);  //TODO: podrška za "č"
        instrumentList.addItem("Bugarija",  Bugarija);
        instrumentList.addItem("Bas",       Bas);

        instrumentList.setSelectedId(Bisernica);

        //pazi ovo castanje inta u instrument
        instrumentList.onChange = [this] {
            setInstrument((Instrument)instrumentList.getSelectedId());
            playDecayRotary.setValue(getDefaultPlayDecay());
            setPlayDecay(getDefaultPlayDecay());
            stopDecayRotary.setValue(getDefaultStopDecay());
            setStopDecay(getDefaultStopDecay());
            tremoloPickingButton.setToggleState(getDefaultTrzanje(), juce::dontSendNotification);
            setTrzanje(getDefaultTrzanje());
            pickSpeedRandRotary.setValue(0);
            setPickSpeedRand(0);
            velocityRandRotary.setValue(0);
            setVelocityRand(0);
        };

        addAndMakeVisible (keyboardComponent);
        //keyboardComponent.setAvailableRange(getMinMidiNote(), getMaxMidiNote());
        keyboardState.addListener (this);

        setSize (600, 460);

        //TODO: huh? prouèi, nauèi. specify the number of input and output channels that we want to open
        auto audioDevice = deviceManager.getCurrentAudioDevice();
        auto numInputChannels  = (audioDevice != nullptr ? audioDevice->getActiveInputChannels() .countNumberOfSetBits() : 0);
        auto numOutputChannels = jmax (audioDevice != nullptr ? audioDevice->getActiveOutputChannels().countNumberOfSetBits() : 2, 2);

        setAudioChannels (numInputChannels, numOutputChannels);
    }

    ~TamburaApp() override
    {
        keyboardState.removeListener (this);
        deviceManager.removeMidiInputDeviceCallback (juce::MidiInput::getAvailableDevices()[midiInputList.getSelectedItemIndex()].identifier, this);
        shutdownAudio();
    }

    //==============================================================================
    void prepareToPlay (int /*samplesPerBlockExpected*/, double sampleRate) override
    {
        generateStringSynths (sampleRate, pickSpeedRotary.getValue(), tremoloPickingButton.getToggleState(), instrument);
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

    void resized() override {
       auto area = getLocalBounds();

       //izbornik midi inputa i liste instrumenata
       auto inputControlArea = area.removeFromTop(60).removeFromBottom(36); //da stane label iznad
       midiInputList.setBounds(inputControlArea.removeFromLeft(400));
       instrumentList.setBounds(inputControlArea);

       //klavijatura skroz dolje
       keyboardComponent.setBounds(area.removeFromBottom(80));

       auto topControlArea =
           area.removeFromTop(180);   // Gornji red rotary kontrola
       auto bottomControlArea = area; // Donji red: decay + tremolo

       //gornji red
       int rotaryWidth = 120;
       int rotaryHeight = 120;
       int rotarySpacing = 60;
       int labelHeight = 30;
       int topStartX = (getWidth() - (2 * rotaryWidth + rotarySpacing)) / 2;

       pickSpeedRotary.setBounds(rotarySpacing, topControlArea.getY() + labelHeight,
                                 rotaryWidth, rotaryHeight);
       pickSpeedRandRotary.setBounds(rotarySpacing * 2 + rotaryWidth,
                                     topControlArea.getY() + labelHeight,
                                     rotaryWidth,
                                     rotaryHeight);
       velocityRandRotary.setBounds(rotarySpacing * 3 + rotaryWidth * 2,
                                    topControlArea.getY() + labelHeight,
                                    rotaryWidth,
                                    rotaryHeight);

       //donji red
       int decayWidth = 120;
       int buttonWidth = 120;
       int spacing = 60;

       playDecayRotary.setBounds(spacing, bottomControlArea.getY()+10, decayWidth,
                             rotaryHeight);
       stopDecayRotary.setBounds(spacing * 2 + decayWidth, bottomControlArea.getY()+10,
                                 decayWidth, rotaryHeight);
       tremoloPickingButton.setBounds(
           spacing * 3 + decayWidth * 2 + 30,
           bottomControlArea.getY() + (rotaryHeight / 2) - 10 +10, buttonWidth,
           20); // manji gumb, centriran po visini
    }



private:
    
    //TODO: jel mi ovo treba? sve zakomentirano... vrijeme æe pokazati
    //std::unordered_set<int> pressedNotes;

    //vraca sve MIDI tonove koje instrument može proizvesti
    static Array<int> getMidiRange(Instrument instrument)
    {
        int najnizi;
        int najvisi;

        switch (instrument)
        {
            case Bisernica:
                najnizi = 61;
                najvisi = 96;
                break;
            case Brac:
                najnizi = 54;
                najvisi = 84;
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
    int getMinMidiNote()
    {
        int min;
        switch (instrument)
        {
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

    int getMaxMidiNote()
    {
        int max;
        switch (instrument)
        {
            case Bisernica:
                max = 96;
                break;
            case Brac:
                max = 84;
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

    bool getDefaultTrzanje()
    {
        bool rez;
        switch (instrument)
        {
            case Bisernica:
                rez = true;
                break;
            case Brac:
                rez = true;
                break;
            case Bugarija:
                rez = false;
                break;
            case Bas:
                rez = false;
                break;
        }
        return rez;
    }

    float getDefaultPlayDecay()
    {
        float rez;
        switch (instrument)
        {
            case Bisernica:
                rez = 0.997;
                break;
            case Brac:
                rez = 0.995;
                break;
            case Bugarija:
                rez = 0.991;
                break;
            case Bas:
                rez = 0.971;
                break;
        }
        return rez;
    }

    float getDefaultStopDecay() {
       float rez;
       switch (instrument) {
       case Bisernica:
          rez = 0.925;
          break;
       case Brac:
          rez = 0.862;
          break;
       case Bugarija:
          rez = 0.854;
          break;
       case Bas:
          rez = 0.7;
          break;
       }
       return rez;
    }

    //TODO: sejvat instrument ili ga vuè veæ spremljenog?
    void generateStringSynths (double sampleRate, int pickSpeed, bool tremoloPicking, Instrument instrument)
    {
        stringSynths.clear();
        for (int midiNote : getMidiRange(instrument))
        {
           stringSynths.add(new StringSynthesiser(
               sampleRate,
               juce::MidiMessage::getMidiNoteInHertz(midiNote),
               pickSpeed,
               tremoloPicking,
               instrument));
        }
    }

    //ovo kad korisnik promijeni instrument tijekom rada
    //poziva generateStringSynths koji èisti sve stare i pali nove
    //TODO: ovo možemo koristit i za panic button
    void setInstrument(Instrument instrument)
    {
        this->instrument = instrument;
        generateStringSynths(savedSampleRate, pickSpeedRotary.getValue(), tremoloPickingButton.getToggleState(), instrument);
    }

    void setPickSpeed(int pickSpeed)
    {
        for (auto stringSynth : stringSynths) {
            stringSynth->changePickSpeed(pickSpeed);
        }
    }

    void setPickSpeedRand(int pickSpeedRand)
    {
        for (auto stringSynth : stringSynths) {
          stringSynth->changePickSpeedRand(pickSpeedRand);
        }
    }

    void setVelocityRand(int velocityRand) {
       for (auto stringSynth : stringSynths) {
          stringSynth->changeVelocityRand(velocityRand);
       }
    }

    void setTrzanje(bool trzanje)
    {
        for (auto stringSynth : stringSynths) {
            stringSynth->changeTrzanje(trzanje);
        }
    }

    void setPlayDecay(float decay)
    {
        for (auto stringSynth : stringSynths) {
            stringSynth->changePlayDecay(decay);
        }
    }

    void setStopDecay(float decay) {
       for (auto stringSynth : stringSynths) {
          stringSynth->changeStopDecay(decay);
       }
    }

    void setMidiInput(int index)
    {
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
            //stringSynths.getUnchecked(midiNoteNumber - getMinMidiNote())->changeDecay(0.998);
            stringSynths.getUnchecked(midiNoteNumber - getMinMidiNote())->changeVelocity(velocity);
            stringSynths.getUnchecked(midiNoteNumber - getMinMidiNote())->stringPlucked();
        }
    }

    void handleNoteOff (juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float /*velocity*/) override
    {
        //auto m = juce::MidiMessage::noteOff (midiChannel, midiNoteNumber);
        //pressedNotes.erase(midiNoteNumber);
        //if (pressedNotes.empty())
        //    stringSynths.getUnchecked(0)->stringMuted();
        if (midiNoteNumber >= getMinMidiNote() && midiNoteNumber <= getMaxMidiNote()) {
            //stringSynths.getUnchecked(midiNoteNumber - getMinMidiNote())->changeDecay(0.9);
            stringSynths.getUnchecked(midiNoteNumber - getMinMidiNote())->stringMuted();
        }
    }

    //==============================================================================
    OwnedArray<StringSynthesiser> stringSynths;
    Instrument instrument = Bisernica;  //default
    float savedSampleRate;

    juce::Slider pickSpeedRotary;
    juce::Label pickSpeedRotaryLabel;

    juce::Slider pickSpeedRandRotary;
    juce::Label pickSpeedRandRotaryLabel;

    juce::Slider velocityRandRotary;
    juce::Label velocityRandRotaryLabel;

    juce::Slider playDecayRotary;
    juce::Label playDecayRotaryLabel;

    juce::Slider stopDecayRotary;
    juce::Label stopDecayRotaryLabel;

    juce::ToggleButton tremoloPickingButton;  //button ne treba label

    juce::AudioDeviceManager deviceManager;
    juce::ComboBox midiInputList;
    juce::Label midiInputListLabel;
    int lastInputIndex = 0;
    bool isAddingFromMidiInput = false;

    juce::ComboBox instrumentList;
    juce::Label instrumentListLabel;

    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboardComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TamburaApp)
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
//TODO: dodaj randomness u trzanje (ma rotary drugi) i dodaj decay rotary i pick position...
//TODO: dinamika/velocity
//TODO: sustain pedala
//TODO: zatamniti note izvan opsega (ili samo postavi koje su visible...)
//TODO: formatiranje... po defaultu imaš 3 razmaka od prije, a razmisli oš ostavit tak zagrade
//TODO: dropdown "instrumenata" ili sampleova samo? jel uopæe trebamo rang gledat? a valjda da, svaki put uèitaj novi sample i radi nove synthove
//TODO: razlièit decay factor po instrumentu
//TODO: stranica o tome što se desi kada ton "strši", pokazati sample i gdje ga odreže
//TODO: ili dva decaya? jedan za dok svira, jedan kad se demfa
//TODO: dynamic decay? manji za niže tonove... formula
//TODO: da ostanu postavke tijekom mijenjanja instrumenta