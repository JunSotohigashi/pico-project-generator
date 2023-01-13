#include "buzzer.hpp"
#include <math.h>

float freq_table[128] = {8.2, 8.7, 9.2, 9.7, 10.3, 10.9, 11.6, 12.2, 13.0, 13.8, 14.6, 15.4, 16.4, 17.3, 18.4, 19.4, 20.6, 21.8, 23.1, 24.5, 26.0, 27.5, 29.1, 30.9, 32.7, 34.6, 36.7, 38.9, 41.2, 43.7, 46.2, 49.0, 51.9, 55.0, 58.3, 61.7, 65.4, 69.3, 73.4, 77.8, 82.4, 87.3, 92.5, 98.0, 103.8, 110.0, 116.5, 123.5, 130.8, 138.6, 146.8, 155.6, 164.8, 174.6, 185.0, 196.0, 207.7, 220.0, 233.1, 246.9, 261.6, 277.2, 293.7, 311.1, 329.6, 349.2, 370.0, 392.0, 415.3, 440.0, 466.2, 493.9, 523.3, 554.4, 587.3, 622.3, 659.3, 698.5, 740.0, 784.0, 830.6, 880.0, 932.3, 987.8, 1046.5, 1108.7, 1174.7, 1244.5, 1318.5, 1396.9, 1480.0, 1568.0, 1661.2, 1760.0, 1864.7, 1975.5, 2093.0, 2217.5, 2349.3, 2489.0, 2637.0, 2793.8, 2960.0, 3136.0, 3322.4, 3520.0, 3729.3, 3951.1, 4186.0, 4434.9, 4698.6, 4978.0, 5274.0, 5587.7, 5919.9, 6271.9, 6644.9, 7040.0, 7458.6, 7902.1, 8372.0, 8869.8, 9397.3, 9956.1, 10548.1, 11175.3, 11839.8, 12543.9};

Buzzer::Buzzer(uint pin)
{
    mPin = pin;
    mSlice = pwm_gpio_to_slice_num(mPin);
    mChannel = pwm_gpio_to_channel(mPin);
    gpio_set_function(mPin, GPIO_FUNC_PWM);
    gpio_set_dir(mPin, GPIO_OUT);
    pwm_set_clkdiv_int_frac(mSlice, 0xFF, 0xE);
    pwm_set_enabled(pwm_gpio_to_slice_num(mPin), false);
}

Buzzer::~Buzzer()
{
    pwm_set_enabled(pwm_gpio_to_slice_num(mPin), false);
    gpio_deinit(mPin);
}

void Buzzer::set_freq(double freq)
{
    if (freq >= 100 && freq <= 20000)
    {
        if (freq != mFreq)
        {
            mFreq = freq;
            mWrap = 488400.4884 / mFreq - 1;
            pwm_set_wrap(mSlice, mWrap);
            pwm_set_chan_level(mSlice, mChannel, mWrap * mDuty);
        }
    }
}

void Buzzer::set_duty(double duty)
{
    if (duty >= 0 && duty <= 1)
    {
        if (duty != mDuty)
        {
            mDuty = duty;
            pwm_set_chan_level(mSlice, mChannel, mWrap * mDuty);
        }
    }
}

void Buzzer::set_gate(bool enable)
{
    mGate = enable;
    pwm_set_enabled(pwm_gpio_to_slice_num(mPin), mGate);
}

void Buzzer::set_freq_by_note(uint8_t note, double freq_offset)
{
    //double freq = 440.0 * pow(2, (note - 69) / 12.0) + freq_offset;
    double freq = freq_table[note];
    this->set_freq(freq);
}