/*******************************************************************************

 BEGIN_JUCE_PIP_METADATA

 name:             TamburaApp
 version:          1.0.0
 description:      Sinteza zvuka tambure pomoću Karplus-Strong algoritma.

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

    void changePlayDecay(float decay) { playDecay = decay; }

    void changeStopDecay(float decay) { stopDecay = decay; }

    void changeVelocity(float velocity) { this->velocity = velocity; }

    void stringPlucked()
    {
        //na početku idućeg buffera (kad se pozove generateAndAddData) sviraj ton
        if (doPluckForNextBuffer.compareAndSetBool (1, 0))
        {
            amplitude = std::sin(MathConstants<float>::pi * 0.5 * velocity) 
                * (0.5 + (juce::Random::getSystemRandom().nextDouble() - 0.5f) * ((float)velocityRand * 0.01));
            decay = playDecay;
            if (trzanje == true)
                startTimer(pickSpeed + (float)pickSpeedRand * (juce::Random::getSystemRandom().nextDouble() - 0.5f));
        }
    }

    void timerCallback()
    {   
        if (trzanje)    //ako se promijeni usred sviranja
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
            auto nextPos = (pos + 1) % delayLine.size();
            float interpolatedSample = decay * 0.5f * ((1.0f - delayLineDecimal) * delayLine[nextPos] + (1.0f + delayLineDecimal) * delayLine[pos]);
            delayLine[pos] = interpolatedSample;
            outBuffer[i] += delayLine[nextPos];
            pos = nextPos;
        }
    }

private:
    void prepareSynthesiserState (double sampleRate, double frequencyInHz, Instrument instrument)
    {
        savedSampleRate = sampleRate;
        this->frequencyInHz = frequencyInHz;
        this->instrument = instrument;
        delayLineWhole = (int) std::floor(sampleRate / frequencyInHz);
        delayLineDecimal = (float) (sampleRate / frequencyInHz) - (float) delayLineWhole;

        delayLine.resize(delayLineWhole + 1);    //+1 zbog decimalnog dijela, da interpolacija radi
        std::fill (delayLine.begin(), delayLine.end(), 0.0f);
        excitationSample.resize(delayLineWhole + 1);

        loadInstrumentToVector(instrument);
    }

    //ubacuje excitationSample u delayLine, pokreće trzaj
    void exciteInternalBuffer()
    {
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

    void loadWavToVector(const juce::File& file)
    {
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();
        juce::AudioFormatReader* reader = formatManager.createReaderFor(file);
        
        juce::AudioBuffer<float> audioBuffer;
        audioBuffer.setSize((int)reader->numChannels, (int)reader->lengthInSamples);
        audioBuffer.clear();

        reader->read(&audioBuffer, 0, excitationSample.size(), 0, true, true);
        delete reader;

        const float *channelData = audioBuffer.getReadPointer(0);
        excitationSample.assign(channelData,
                                channelData + excitationSample.size());
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
    float decay;            //za manipulaciju odzvanjanja
    float playDecay = 0.995;
    float stopDecay = 0.9;
    float velocity = 127.0f;
    double amplitude = 0.0;
    int pickSpeed = 110;    //milisekunde
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
                         private juce::MidiInputCallback,
                         private juce::MidiKeyboardStateListener {
public:
   TamburaApp() : keyboardComponent(keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
       #ifdef JUCE_DEMO_RUNNER
        , AudioAppComponent (getSharedAudioDeviceManager (0, 2))
       #endif
    {

        addAndMakeVisible(pickSpeedRotaryLabel);
        pickSpeedRotaryLabel.setText("Brzina trzanja", juce::dontSendNotification);
        pickSpeedRotaryLabel.attachToComponent(&pickSpeedRotary, false);

        addAndMakeVisible(pickSpeedRotary);
        pickSpeedRotary.setSliderStyle(Slider::Rotary);
        pickSpeedRotary.setTextBoxStyle(Slider::TextBoxBelow, false, 30, 15);
        pickSpeedRotary.setRange(70, 140, 1);
        pickSpeedRotary.setValue(95);

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

        setStopDecay(getDefaultStopDecay());
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
        instrumentList.addItem("Brac",      Brac);
        instrumentList.addItem("Bugarija",  Bugarija);
        instrumentList.addItem("Bas",       Bas);

        instrumentList.setSelectedId(Bisernica);

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
        keyboardState.addListener (this);

        setSize (600, 460);

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
       auto inputAndInstrumentArea = area.removeFromTop(60).removeFromBottom(36); //36 da stane label iznad
       midiInputList.setBounds(inputAndInstrumentArea.removeFromLeft(400));
       instrumentList.setBounds(inputAndInstrumentArea);

       //klavijatura skroz dolje
       keyboardComponent.setBounds(area.removeFromBottom(80));

       auto topRowArea = area.removeFromTop(180);
       auto bottomRowArea = area;

       int widgetWidth = 120;
       int widgetHeight = 120;
       int widgetSpacing = 60;
       int labelHeight = 30;

       //gornji red
       pickSpeedRotary.setBounds(widgetSpacing, topRowArea.getY() + labelHeight,
                                 widgetWidth, widgetHeight);
       pickSpeedRandRotary.setBounds(widgetSpacing * 2 + widgetWidth,
                                     topRowArea.getY() + labelHeight,
                                     widgetWidth,
                                     widgetHeight);
       velocityRandRotary.setBounds(widgetSpacing * 3 + widgetWidth * 2,
                                    topRowArea.getY() + labelHeight,
                                    widgetWidth,
                                    widgetHeight);

       //donji red
       playDecayRotary.setBounds(widgetSpacing, bottomRowArea.getY()+10,
                                 widgetWidth, widgetHeight);
       stopDecayRotary.setBounds(widgetSpacing * 2 + widgetWidth, bottomRowArea.getY()+10,
                                 widgetWidth, widgetHeight);
       tremoloPickingButton.setBounds(widgetSpacing * 3 + widgetWidth * 2 + 30,
           bottomRowArea.getY() + (widgetHeight / 2) - 10 +10, widgetHeight,
           20);
    }



private:
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
    //poziva generateStringSynths koji čisti sve stare i pali nove
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
        if (midiNoteNumber >= getMinMidiNote() && midiNoteNumber <= getMaxMidiNote()) {
            stringSynths.getUnchecked(midiNoteNumber - getMinMidiNote())->changeVelocity(velocity);
            stringSynths.getUnchecked(midiNoteNumber - getMinMidiNote())->stringPlucked();
        }
    }

    void handleNoteOff (juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float /*velocity*/) override
    {
        if (midiNoteNumber >= getMinMidiNote() && midiNoteNumber <= getMaxMidiNote()) {
            stringSynths.getUnchecked(midiNoteNumber - getMinMidiNote())->stringMuted();
        }
    }

    //==============================================================================
    OwnedArray<StringSynthesiser> stringSynths;
    Instrument instrument = Bisernica;  //zadana (default) vrijednost
    float savedSampleRate;              //spremljena frekvencija uzorkovanja

    juce::Slider pickSpeedRotary;       //potenciometar za brzinu trzanja
    juce::Label pickSpeedRotaryLabel;   //labela za isti

    juce::Slider pickSpeedRandRotary;   //pot. za varijaciju brzine trzanja
    juce::Label pickSpeedRandRotaryLabel;

    juce::Slider velocityRandRotary;    //pot. za varijaciju jačine rzanja 
    juce::Label velocityRandRotaryLabel;

    juce::Slider playDecayRotary;       //pot. za prigušenje povratne veze
    juce::Label playDecayRotaryLabel;   //tijekom sviranja

    juce::Slider stopDecayRotary;       //pot. za prigušenje povratne veze
    juce::Label stopDecayRotaryLabel;   //prilikom prestanka sviranja

    juce::ToggleButton tremoloPickingButton;    //gumb za trzanje ili lupanje

    juce::AudioDeviceManager deviceManager;     //upravitelj spojenim uređajima
    juce::ComboBox midiInputList;               //lista povezanih MIDI uređaja
    juce::Label midiInputListLabel;
    int lastInputIndex = 0;
    bool isAddingFromMidiInput = false;

    juce::ComboBox instrumentList;              //lista ponuđenih instrumenata
    juce::Label instrumentListLabel;

    juce::MidiKeyboardState keyboardState;      //stanje klavijature
    juce::MidiKeyboardComponent keyboardComponent;  //klavijatura na zaslonu

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TamburaApp)
};