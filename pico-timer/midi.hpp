#ifndef _MIDI_HPP_
#define _MIDI_HPP_

#include "pico/stdlib.h"

class Midi
{
private:
    uint8_t *mMidiData;
    struct HeaderChunk
    {
        uint32_t chunkID = 0x4D546864;  // "MThd"
        uint32_t chunkSize = 6;
        uint16_t formatType = 0;    // SMF0
        uint16_t numberOfTracks = 1;
        uint16_t timeDivision = 480;
    } mHeader;
    struct TrackChunk
    {
        uint32_t chunkID = 0x4D54726B;  // "MTrk"
        uint32_t chunkSize = 6;
        uint8_t *data;
    } mTrack;

public:
    Midi();
    ~Midi();
    
};

#endif // _MIDI_HPP_