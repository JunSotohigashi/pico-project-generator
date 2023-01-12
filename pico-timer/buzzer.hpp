#ifndef _BUZZER_HPP_
#define _BUZZER_HPP_

#include "pico/stdlib.h"
#include "hardware/pwm.h"

class Buzzer
{
private:
    uint mPin;
    uint mSlice;
    uint mChannel;
    double mFreq = 0;
    double mDuty = 0.5;
    bool mGate = false;
    uint16_t mWrap;

public:
    Buzzer(uint pin);
    ~Buzzer();
    void set_freq(double freq);
    void set_duty(double duty);
    void set_gate(bool enable);
    void set_freq_by_note(uint8_t note, double freq_offset = 0.0);
};

#endif // _BUZZER_HPP_