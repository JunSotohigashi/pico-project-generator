#include "buzzer.hpp"
#include <math.h>

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
    double freq = 440.0 * pow(2, (note - 69) / 12.0) + freq_offset;
    this->set_freq(freq);
}