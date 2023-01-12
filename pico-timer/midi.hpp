#ifndef _MIDI_HPP_
#define _MIDI_HPP_

#include "pico/stdlib.h"

class Midi
{
private:
    uint8_t *mMidiData;
public:
    Midi();
    ~Midi();
};

#endif // _MIDI_HPP_