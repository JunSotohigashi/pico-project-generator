#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/pwm.h"
#include "buzzer.hpp"
#include "midi.hpp"
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
const uint BUZ_1_PIN = 19;
const uint BUZ_2_PIN = 20;
Buzzer *buz1;
Buzzer *buz2;
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

void song_flog()
{
    buz1->set_freq_by_note(72);
    buz1->set_gate(true);
    for (auto i = 0; i < 100; i++)
    {
        double d = 0.1 - (0.001 * i);
        buz1->set_duty(d);
        buz2->set_duty(d);
        sleep_ms(4);
    }
    buz1->set_freq_by_note(74);
    for (auto i = 0; i < 100; i++)
    {
        double d = 0.1 - (0.001 * i);
        buz1->set_duty(d);
        buz2->set_duty(d);
        sleep_ms(4);
    }
    buz1->set_freq_by_note(76);
    for (auto i = 0; i < 100; i++)
    {
        double d = 0.1 - (0.001 * i);
        buz1->set_duty(d);
        buz2->set_duty(d);
        sleep_ms(4);
    }
    buz1->set_freq_by_note(77);
    for (auto i = 0; i < 100; i++)
    {
        double d = 0.1 - (0.001 * i);
        buz1->set_duty(d);
        buz2->set_duty(d);
        sleep_ms(4);
    }
    buz1->set_freq_by_note(76);
    for (auto i = 0; i < 100; i++)
    {
        double d = 0.1 - (0.001 * i);
        buz1->set_duty(d);
        buz2->set_duty(d);
        sleep_ms(4);
    }
    buz1->set_freq_by_note(74);
    for (auto i = 0; i < 100; i++)
    {
        double d = 0.1 - (0.001 * i);
        buz1->set_duty(d);
        buz2->set_duty(d);
        sleep_ms(4);
    }
    buz1->set_freq_by_note(72);
    for (auto i = 0; i < 100; i++)
    {
        double d = 0.1 - (0.001 * i);
        buz1->set_duty(d);
        buz2->set_duty(d);
        sleep_ms(4);
    }
    buz1->set_gate(false);
    buz2->set_gate(false);
    sleep_ms(400);

    buz1->set_freq_by_note(72);
    buz2->set_freq_by_note(76);
    buz1->set_gate(true);
    buz2->set_gate(true);
    for (auto i = 0; i < 100; i++)
    {
        double d = 0.1 - (0.001 * i);
        buz1->set_duty(d);
        buz2->set_duty(d);
        sleep_ms(4);
    }
    buz1->set_freq_by_note(74);
    buz2->set_freq_by_note(77);
    for (auto i = 0; i < 100; i++)
    {
        double d = 0.1 - (0.001 * i);
        buz1->set_duty(d);
        buz2->set_duty(d);
        sleep_ms(4);
    }
    buz1->set_freq_by_note(76);
    buz2->set_freq_by_note(79);
    for (auto i = 0; i < 100; i++)
    {
        double d = 0.1 - (0.001 * i);
        buz1->set_duty(d);
        buz2->set_duty(d);
        sleep_ms(4);
    }
    buz1->set_freq_by_note(77);
    buz2->set_freq_by_note(81);
    for (auto i = 0; i < 100; i++)
    {
        double d = 0.1 - (0.001 * i);
        buz1->set_duty(d);
        buz2->set_duty(d);
        sleep_ms(4);
    }
    buz1->set_freq_by_note(76);
    buz2->set_freq_by_note(79);
    for (auto i = 0; i < 100; i++)
    {
        double d = 0.1 - (0.001 * i);
        buz1->set_duty(d);
        buz2->set_duty(d);
        sleep_ms(4);
    }
    buz1->set_freq_by_note(74);
    buz2->set_freq_by_note(77);
    for (auto i = 0; i < 100; i++)
    {
        double d = 0.1 - (0.001 * i);
        buz1->set_duty(d);
        buz2->set_duty(d);
        sleep_ms(4);
    }
    buz1->set_freq_by_note(72);
    buz2->set_freq_by_note(76);
    for (auto i = 0; i < 100; i++)
    {
        double d = 0.1 - (0.001 * i);
        buz1->set_duty(d);
        buz2->set_duty(d);
        sleep_ms(4);
    }
    buz1->set_gate(false);
    buz2->set_gate(false);
    sleep_ms(400);
}

// void midi(uint8_t *midi_data)
// {
//     uint32_t pointer = 0;
//     uint32_t tempo = 500000;
//     // Header chunk
// }

void core1()
{
    sem_acquire_blocking(&core_sem);
    uint8_t song = core_song;
    sem_release(&core_sem);

    switch (song)
    {
    case 0:
        song_flog();
        break;

    default:
        buz1->set_freq(1000);
        buz2->set_freq(1001);
        buz1->set_gate(true);
        buz2->set_gate(true);
        sleep_ms(3000);
        buz1->set_gate(false);
        buz2->set_gate(false);
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
            buz1 = new Buzzer(BUZ_1_PIN);
            buz2 = new Buzzer(BUZ_2_PIN);
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
                delete buz1;
                delete buz2;
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
                delete buz1;
                delete buz2;
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
