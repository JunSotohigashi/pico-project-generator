#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/pwm.h"
#include "buzzer.hpp"
// #include "midi.hpp"
#include "midi-data.h"

//// Pin definitions ////
// on-board LED
const uint LED_PIN = PICO_DEFAULT_LED_PIN;
// 4-digits 7-segments LED
const uint LED_SEG_PIN[8] = {10, 8, 12, 14, 15, 9, 11, 13};
const uint LED_DIG_PIN[4] = {7, 6, 5, 4};
// buttons
const uint BTN_M_PIN = 2;
const uint BTN_S_PIN = 1;
const uint BTN_E_PIN = 0;
// passive buzzers
const uint BUZ_0_PIN = 19;
const uint BUZ_1_PIN = 20;
Buzzer *buz0;
Buzzer *buz1;
// power latch relay
const uint PWR_PIN = 18;

//// Global variables ////
// static uint8_t melody = 0;
static uint8_t led_bin[4] = {0, 0, 0, 0};
volatile uint32_t btn_m_time = 0;
volatile uint32_t btn_s_time = 0;
volatile uint32_t btn_e_time = 0;
static semaphore_t core_sem;
static uint8_t core_song = 0;
static bool core_exit = false;

void gpio_callback(uint gpio, uint32_t event)
{
    irq_set_enabled(IO_IRQ_BANK0, false);
    uint32_t now_time = to_ms_since_boot(get_absolute_time());
    if (gpio == BTN_M_PIN && event & (GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE))
    {
        btn_m_time = !gpio_get(BTN_M_PIN) ? now_time : 0;
    }
    else if (gpio == BTN_S_PIN && event & (GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE))
    {
        btn_s_time = !gpio_get(BTN_S_PIN) ? now_time : 0;
    }
    else if (gpio == BTN_E_PIN && event & (GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE))
    {
        btn_e_time = !gpio_get(BTN_E_PIN) ? now_time : 0;
    }
    irq_set_enabled(IO_IRQ_BANK0, true);
}

bool timer_dynamic(repeating_timer_t *rptm)
{
    static uint8_t phase = 0;
    uint32_t mask = 0;
    uint32_t value = 0;
    for (auto i = 0; i < 8; i++)
    {
        mask |= (1 << LED_SEG_PIN[i]);
        value |= ((led_bin[phase] >> i & 1) << LED_SEG_PIN[i]);
    }
    for (auto i = 0; i < 4; i++)
    {
        mask |= (1 << LED_DIG_PIN[i]);
        value |= ((phase == i) << LED_DIG_PIN[i]);
    }
    gpio_put_masked(mask, value);
    phase = (phase + 1) % 4;
    return true;
}

uint8_t decode_bin(int8_t num, bool dp)
{
    uint8_t result = 0;
    switch (num)
    {
    case 0:
        result = 0b00111111;
        break;
    case 1:
        result = 0b00000110;
        break;
    case 2:
        result = 0b01011011;
        break;
    case 3:
        result = 0b01001111;
        break;
    case 4:
        result = 0b01100110;
        break;
    case 5:
        result = 0b01101101;
        break;
    case 6:
        result = 0b01111101;
        break;
    case 7:
        result = 0b00000111;
        break;
    case 8:
        result = 0b01111111;
        break;
    case 9:
        result = 0b01101111;
        break;
    case 0xa:
        result = 0b01110111;
        break;
    case 0xb:
        result = 0b01111100;
        break;
    case 0xc:
        result = 0b00111001;
        break;
    case 0xd:
        result = 0b01011110;
        break;
    case 0xe:
        result = 0b01111001;
        break;
    case 0xf:
        result = 0b01110001;
        break;
    default:
        break;
    }
    result |= dp << 7;
    return result;
}

void note_control(bool on, uint8_t note, uint8_t velocity)
{
    static uint8_t ch0_note = 0;
    static uint8_t ch1_note = 0;
    static bool ch0_state = false;
    static bool ch1_state = false;
    static bool oldest_is_ch0 = true;
    if (velocity == 0)
    {
        on = false;
    }
    // printf("%d, %3d, %3d\n", on, note, velocity);

    if (on)
    {
        // buz0->set_freq_by_note(note);
        // buz0->set_duty(0.5 * (velocity / 127.0));
        // buz0->set_gate(true);
        if (oldest_is_ch0)
        {
            ch0_note = note;
            buz0->set_freq_by_note(note);
            buz0->set_duty(0.5 * (velocity / 127.0));
            buz0->set_gate(true);
            ch0_state = true;
            oldest_is_ch0 = false;
        }
        else
        {
            ch1_note = note;
            buz1->set_freq_by_note(note);
            buz1->set_duty(0.5 * (velocity / 127.0));
            buz1->set_gate(true);
            ch1_state = true;
            oldest_is_ch0 = true;
        }
    }
    else
    {
        if (note == ch0_note)
        {
            buz0->set_gate(false);
            ch0_state = false;
        }
        else if (note == ch1_note)
        {
            buz1->set_gate(false);
            ch1_state = false;
        }
        // buz0->set_gate(false);
    }
}

void play_midi(uint8_t *midi_data)
{
    uint32_t pointer = 0;
    uint32_t tempo = 500000;

    // Header data
    uint32_t header_len;
    uint16_t smf_format;
    uint16_t number_of_tracks;
    uint16_t time_div;

    // Track data
    uint32_t track_len;

    // Header chunk
    if (midi_data[pointer++] != 0x4D) // "M"
        return;
    if (midi_data[pointer++] != 0x54) // "T"
        return;
    if (midi_data[pointer++] != 0x68) // "h"
        return;
    if (midi_data[pointer++] != 0x64) // "d"
        return;
    header_len = midi_data[pointer++] << 24 | midi_data[pointer++] << 16 | midi_data[pointer++] << 8 | midi_data[pointer++];
    smf_format = midi_data[pointer++] << 8 | midi_data[pointer++];
    number_of_tracks = midi_data[pointer++] << 8 | midi_data[pointer++];
    time_div = midi_data[pointer++] << 8 | midi_data[pointer];
    // printf("header_len=%d, smf_format=%d, number_of_tracks=%d, time_division=%d\n", header_len, smf_format, number_of_tracks, time_div);
    if (smf_format != 0)
        return;

    // Track chunk
    pointer = 8 + header_len;
    if (midi_data[pointer++] != 0x4D) // "M"
        return;
    if (midi_data[pointer++] != 0x54) // "T"
        return;
    if (midi_data[pointer++] != 0x72) // "r"
        return;
    if (midi_data[pointer++] != 0x6B) // "k"
        return;
    track_len = midi_data[pointer++] << 24 | midi_data[pointer++] << 16 | midi_data[pointer++] << 8 | midi_data[pointer++];
    while (pointer < 16 + header_len + track_len)
    {
        uint32_t delta_tick = 0;
        // printf("pointer=%04x ", pointer);
        do
        {
            delta_tick = (delta_tick << 7) | (midi_data[pointer] & 0b1111111);
        } while (midi_data[pointer++] & 0b10000000);
        // printf("delta=%d ", delta_tick);
        if (delta_tick != 0)
        {
            uint64_t delta_us = (tempo / time_div) * delta_tick;
            sleep_us(delta_us);
        }

        uint8_t status_byte = midi_data[pointer++];
        // printf("status=%02x ", status_byte);
        if ((status_byte & 0xF0) == 0x80) // Note-off
        {
            uint8_t note_number = midi_data[pointer++];
            uint8_t velocity = midi_data[pointer++];
            note_control(false, note_number, velocity);
            // printf("Note-off note=%d velocity=%d\n", note_number, velocity);
        }
        else if ((status_byte & 0xF0) == 0x90) // Note-on
        {
            uint8_t note_number = midi_data[pointer++];
            uint8_t velocity = midi_data[pointer++];
            note_control(true, note_number, velocity);
            // printf("Note-on note=%d velocity=%d\n", note_number, velocity);
        }
        else if ((status_byte & 0xF0) == 0xA0) // Polyphonic-key-pressure
        {
            uint8_t note_number = midi_data[pointer++];
            uint8_t pressure = midi_data[pointer++];
            // printf("Polyphonic-key-pressure note=%d pressure=%d\n", note_number, pressure);
        }
        else if ((status_byte & 0xF0) == 0xB0) // Control-Change
        {
            uint8_t control_number = midi_data[pointer++];
            uint8_t control_data = midi_data[pointer++];
            // printf("Control-change control_number=%d control_data=%d\n", control_number, control_data);
        }
        else if ((status_byte & 0xF0) == 0xC0) // Program-change
        {
            uint8_t program_number = midi_data[pointer++];
            // printf("Program-change program_number=%d\n", program_number);
        }
        else if ((status_byte & 0xF0) == 0xD0) // Channel-pressure
        {
            uint8_t pressure = midi_data[pointer++];
            // printf("Channel-pressure pressure=%d\n", pressure);
        }
        else if ((status_byte & 0xF0) == 0xE0) // Pitch-bend-change
        {
            uint16_t pitch_bend = midi_data[pointer++] | (midi_data[pointer++] << 8);
            // printf("Pitch-bend-change pitch_bend=%d\n", pitch_bend);
        }
        else if (status_byte == 0xF0) // SysEx
        {
            while (midi_data[pointer++] != 0xF7)
            {
            }
            // printf("SysEx\n");
        }
        else if (status_byte == 0xFF) // Meta Event
        {
            uint8_t event_type = midi_data[pointer++];
            uint32_t event_length = 0;
            if (event_type == 0x00) // Sequence-number
            {
                pointer++;
                uint16_t sequence = (midi_data[pointer++] << 8) | midi_data[pointer++];
                // printf("Sequence-number sequence=%d\n", sequence);
            }
            else if (event_type == 0x01) // Text-event
            {
                do
                {
                    event_length = (delta_tick << 7) | (midi_data[pointer] & 0b1111111);
                } while (midi_data[pointer++] & 0b10000000);
                char text[event_length + 1];
                for (auto i = 0; i < event_length; i++)
                {
                    text[i] = midi_data[pointer++];
                }
                text[event_length] = 0;
                // printf("Text-event text=%s\n", text);
            }
            else if (event_type == 0x02) // Copyright
            {
                do
                {
                    event_length = (delta_tick << 7) | (midi_data[pointer] & 0b1111111);
                } while (midi_data[pointer++] & 0b10000000);
                char text[event_length + 1];
                for (auto i = 0; i < event_length; i++)
                {
                    text[i] = midi_data[pointer++];
                }
                text[event_length] = 0;
                // printf("Copyright text=%s\n", text);
            }
            else if (event_type == 0x03) // Sequence-name
            {
                do
                {
                    event_length = (delta_tick << 7) | (midi_data[pointer] & 0b1111111);
                } while (midi_data[pointer++] & 0b10000000);
                char text[event_length + 1];
                for (auto i = 0; i < event_length; i++)
                {
                    text[i] = midi_data[pointer++];
                }
                text[event_length] = 0;
                // printf("Sequence-name text=%s\n", text);
            }
            else if (event_type == 0x04) // Instrument-name
            {
                do
                {
                    event_length = (delta_tick << 7) | (midi_data[pointer] & 0b1111111);
                } while (midi_data[pointer++] & 0b10000000);
                char text[event_length + 1];
                for (auto i = 0; i < event_length; i++)
                {
                    text[i] = midi_data[pointer++];
                }
                text[event_length] = 0;
                // printf("Instrument-name text=%s\n", text);
            }
            else if (event_type == 0x05) // Lyrics
            {
                do
                {
                    event_length = (delta_tick << 7) | (midi_data[pointer] & 0b1111111);
                } while (midi_data[pointer++] & 0b10000000);
                char text[event_length + 1];
                for (auto i = 0; i < event_length; i++)
                {
                    text[i] = midi_data[pointer++];
                }
                text[event_length] = 0;
                // printf("Lyrics text=%s\n", text);
            }
            else if (event_type == 0x06) // Marker
            {
                do
                {
                    event_length = (delta_tick << 7) | (midi_data[pointer] & 0b1111111);
                } while (midi_data[pointer++] & 0b10000000);
                char text[event_length + 1];
                for (auto i = 0; i < event_length; i++)
                {
                    text[i] = midi_data[pointer++];
                }
                text[event_length] = 0;
                // printf("Marker text=%s\n", text);
            }
            else if (event_type == 0x07) // Cue-point
            {
                do
                {
                    event_length = (delta_tick << 7) | (midi_data[pointer] & 0b1111111);
                } while (midi_data[pointer++] & 0b10000000);
                char text[event_length + 1];
                for (auto i = 0; i < event_length; i++)
                {
                    text[i] = midi_data[pointer++];
                }
                text[event_length] = 0;
                // printf("Cue-point text=%s\n", text);
            }
            else if (event_type == 0x20) // Channel-prefix
            {
                pointer++;
                uint8_t channel_prefix = midi_data[pointer++];
                // printf("Channel-prefix channel=%d\n", channel_prefix);
            }
            else if (event_type == 0x2F) // End-of-track
            {
                // printf("End-of-track\n");
                return;
            }
            else if (event_type == 0x51) // Set-tempo
            {
                pointer++;
                tempo = (midi_data[pointer++] << 16) | (midi_data[pointer++] << 8) | midi_data[pointer++];
                // printf("Set-tempo tempo=%d\n", tempo);
            }
            else if (event_type == 0x54) // SMPTE-offset
            {
                pointer += 6;
                // printf("SMTPE-offset\n");
            }
            else if (event_type == 0x58) // Beat
            {
                pointer += 5;
                // printf("Beat\n");
            }
            else if (event_type == 0x59) // Key
            {
                pointer += 3;
                // printf("Key\n");
            }
            else if (event_type == 0x7F) // Special-meta-event
            {
                do
                {
                    event_length = (delta_tick << 7) | (midi_data[pointer] & 0b1111111);
                } while (midi_data[pointer++] & 0b10000000);
                char data[event_length + 1];
                for (auto i = 0; i < event_length; i++)
                {
                    data[i] = midi_data[pointer++];
                }
                data[event_length] = 0;
                // printf("Special-meta-event data=%s\n", data);
            }
        }
    }
}

void core1()
{
    sem_acquire_blocking(&core_sem);
    uint8_t song = core_song;
    sem_release(&core_sem);

    switch (song)
    {
    case 0:
        // song_flog();
        play_midi(midi_test);
        break;

    default:
        buz0->set_freq(1000);
        buz1->set_freq(1001);
        buz0->set_gate(true);
        buz1->set_gate(true);
        sleep_ms(3000);
        buz0->set_gate(false);
        buz1->set_gate(false);
        break;
    }
    sem_acquire_blocking(&core_sem);
    core_exit = true;
    sem_release(&core_sem);
}

void core0()
{
    repeating_timer_t timer_dynamic_handle;
    add_repeating_timer_us(1000, timer_dynamic, NULL, &timer_dynamic_handle);
    irq_set_enabled(IO_IRQ_BANK0, true);

    uint8_t state = 0;
    uint32_t time_set_ms = 0;
    uint32_t time_start_ms = 0;
    uint32_t time_add_old = 0;
    uint32_t time_lasttouch_ms = 0;

    while (true)
    {
        uint32_t time_now_ms = to_ms_since_boot(get_absolute_time());
        uint32_t time_countup_ms = 0;
        uint32_t time_countdown_ms = 0;
        uint8_t phase = 0;
        bool core1_exit = false;

        switch (state)
        {
        // 時間設定モード
        case 0:
            if (btn_m_time != 0 && btn_s_time != 0)
            {
                // reset to 0
                time_set_ms = 0;
                led_bin[0] = 0b01000000;
                led_bin[1] = 0b01000000;
                led_bin[2] = 0b01000000;
                led_bin[3] = 0b01000000;
                while (btn_m_time != 0 || btn_s_time != 0)
                {
                    sleep_ms(10);
                }
            }
            else if (btn_m_time == 0 && btn_s_time != 0)
            {
                uint32_t time_add = (time_now_ms - btn_s_time) / 75 + 1; // 75ms周期で数値を増やす
                if (time_add == 1 || time_add > 5)                       // 初回と5カウント以降で増加
                {
                    time_set_ms = time_set_ms - (time_set_ms % 60000) + ((time_add - time_add_old) * 1000 + time_set_ms) % 60000;
                }
                time_add_old = time_add;
            }
            else if (btn_m_time != 0 && btn_s_time == 0)
            {
                uint32_t time_add = (time_now_ms - btn_m_time) / 75 + 1;
                if (time_add == 1 || time_add > 5)
                {
                    time_set_ms = ((time_add - time_add_old) * 60000 + time_set_ms) % 6000000;
                }
                time_add_old = time_add;
            }
            else if (btn_m_time == 0 && btn_s_time == 0 && btn_e_time != 0 && time_set_ms != 0)
            {
                time_add_old = 0;
                state = 1;
            }
            else
            {
                time_add_old = 0;
            }
            if (btn_m_time != 0 || btn_s_time != 0 || btn_e_time != 0)
            {
                time_lasttouch_ms = time_now_ms;
            }
            if (time_now_ms - time_lasttouch_ms > 20000)
            {
                gpio_put(PWR_PIN, 0);
            }
            led_bin[0] = decode_bin(time_set_ms / 1000 / 60 / 10 % 10, false);
            led_bin[1] = decode_bin(time_set_ms / 1000 / 60 % 10, true);
            led_bin[2] = decode_bin(time_set_ms / 1000 % 60 / 10 % 10, false);
            led_bin[3] = decode_bin(time_set_ms / 1000 % 60 % 10, false);
            break;

        // カウントダウン・開始待機
        case 1:
            if (btn_m_time == 0 && btn_s_time == 0 && btn_e_time == 0)
            {
                time_start_ms = time_now_ms;
                time_add_old = 0;
                state = 2;
            }
            break;

        // カウントダウン
        case 2:
            if (time_set_ms > time_now_ms - time_start_ms)
            {
                time_countdown_ms = time_set_ms - (time_now_ms - time_start_ms);
                phase = time_countdown_ms % 1000 / 167;
                led_bin[0] = decode_bin(time_countdown_ms / 1000 / 60 / 10 % 10, phase == 0);
                led_bin[1] = decode_bin(time_countdown_ms / 1000 / 60 % 10, phase == 1 || phase == 5);
                led_bin[2] = decode_bin(time_countdown_ms / 1000 % 60 / 10 % 10, phase == 2 || phase == 4);
                led_bin[3] = decode_bin(time_countdown_ms / 1000 % 60 % 10, phase == 3);
                if (btn_m_time == 0 && btn_s_time == 0 && btn_e_time != 0)
                {
                    state = 3;
                }
            }
            else
            {
                state = 4;
            }
            break;

        // カウントダウン・停止待機
        case 3:
            if (time_set_ms > time_now_ms - time_start_ms)
            {
                time_countdown_ms = time_set_ms - (time_now_ms - time_start_ms);
                phase = time_countdown_ms % 1000 / 167;
                led_bin[0] = decode_bin(time_countdown_ms / 1000 / 60 / 10 % 10, phase == 0);
                led_bin[1] = decode_bin(time_countdown_ms / 1000 / 60 % 10, phase == 1 || phase == 5);
                led_bin[2] = decode_bin(time_countdown_ms / 1000 % 60 / 10 % 10, phase == 2 || phase == 4);
                led_bin[3] = decode_bin(time_countdown_ms / 1000 % 60 % 10, phase == 3);
                if (btn_m_time == 0 && btn_s_time == 0 && btn_e_time == 0)
                {
                    time_set_ms = time_countdown_ms - time_countdown_ms % 1000;
                    state = 0;
                }
            }
            else
            {
                state = 4;
            }
            break;

        // ブザー鳴動
        case 4:
            buz0 = new Buzzer(BUZ_0_PIN);
            buz1 = new Buzzer(BUZ_1_PIN);
            sem_init(&core_sem, 1, 1);
            core_song = time_set_ms >= 2000 ? 0 : 1;
            core_exit = false;
            sem_release(&core_sem);
            multicore_launch_core1(core1);
            state = 5;

        // タイムアップ
        case 5:
            time_countup_ms = time_now_ms - time_start_ms - time_set_ms;
            if (time_countup_ms % 500 < 250)
            {
                led_bin[0] = decode_bin(time_countup_ms / 1000 / 60 / 10 % 10, false);
                led_bin[1] = decode_bin(time_countup_ms / 1000 / 60 % 10, true);
                led_bin[2] = decode_bin(time_countup_ms / 1000 % 60 / 10 % 10, false);
                led_bin[3] = decode_bin(time_countup_ms / 1000 % 60 % 10, false);
            }
            else
            {
                led_bin[0] = 0;
                led_bin[1] = 0b10000000;
                led_bin[2] = 0;
                led_bin[3] = 0;
            }
            sem_acquire_blocking(&core_sem);
            core1_exit = core_exit;
            sem_release(&core_sem);
            if (btn_m_time != 0 || btn_s_time != 0 || btn_e_time != 0)
            {
                state = 6;
            }
            else if (core1_exit)
            {
                multicore_reset_core1();
                delete buz0;
                delete buz1;
                time_lasttouch_ms = time_now_ms;
                state = 0;
            }
            break;

        // タイムアップ・リセット待機
        case 6:
            time_countup_ms = time_now_ms - time_start_ms - time_set_ms;
            if (time_countup_ms % 500 < 250)
            {
                led_bin[0] = decode_bin(time_countup_ms / 1000 / 60 / 10 % 10, false);
                led_bin[1] = decode_bin(time_countup_ms / 1000 / 60 % 10, true);
                led_bin[2] = decode_bin(time_countup_ms / 1000 % 60 / 10 % 10, false);
                led_bin[3] = decode_bin(time_countup_ms / 1000 % 60 % 10, false);
            }
            else
            {
                led_bin[0] = 0;
                led_bin[1] = 0b10000000;
                led_bin[2] = 0;
                led_bin[3] = 0;
            }
            if (btn_m_time == 0 && btn_s_time == 0 && btn_e_time == 0)
            {
                multicore_reset_core1();
                delete buz0;
                delete buz1;
                time_lasttouch_ms = time_now_ms;
                state = 0;
            }
            break;

        default:
            break;
        }

        sleep_ms(1);
    }
}

int main()
{
    //// Initialize ////
    stdio_init_all();
    // on-board LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    // 7-segments 4-digits LED
    for (auto i = 0; i < 8; i++)
    {
        gpio_init(LED_SEG_PIN[i]);
        gpio_set_dir(LED_SEG_PIN[i], GPIO_OUT);
    }
    for (auto i = 0; i < 4; i++)
    {
        gpio_init(LED_DIG_PIN[i]);
        gpio_set_dir(LED_DIG_PIN[i], GPIO_OUT);
    }
    // 分設定ボタン
    gpio_init(BTN_M_PIN);
    gpio_pull_up(BTN_M_PIN);
    gpio_set_dir(BTN_M_PIN, GPIO_IN);
    gpio_set_irq_enabled(BTN_M_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    // 秒設定ボタン
    gpio_init(BTN_S_PIN);
    gpio_pull_up(BTN_S_PIN);
    gpio_set_dir(BTN_S_PIN, GPIO_IN);
    gpio_set_irq_enabled(BTN_S_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    // スタートボタン
    gpio_init(BTN_E_PIN);
    gpio_pull_up(BTN_E_PIN);
    gpio_set_dir(BTN_E_PIN, GPIO_IN);
    gpio_set_irq_enabled(BTN_E_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    // ボタン割り込み処理設定
    gpio_set_irq_callback(gpio_callback);
    // 電源リレー
    gpio_init(PWR_PIN);
    gpio_set_dir(PWR_PIN, GPIO_OUT);
    gpio_put(PWR_PIN, 1);

    //// Main prosess ////
    core0();

    return 0;
}
