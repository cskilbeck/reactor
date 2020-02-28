//////////////////////////////////////////////////////////////////////

#include "main.h"

//////////////////////////////////////////////////////////////////////

#define led_on 32767
#define led_off 0

//////////////////////////////////////////////////////////////////////

typedef struct button
{
    uint32 state;
    bool held;
    bool previous_held;
    bool pressed;
    bool released;
} button;

typedef void (*action)(void);

//////////////////////////////////////////////////////////////////////

enum
{
    led_blue = 3,    // zero score led
    led_snap = 7     // snap led
};

//////////////////////////////////////////////////////////////////////
// state handlers

void do_boot(void);
void do_get_ready(void);
void do_waiting(void);
void do_snap(void);
void do_score(void);
void do_winner(void);

//////////////////////////////////////////////////////////////////////

volatile uint32_t ticks = 0;

button button_a = { 0 };
button button_b = { 0 };

int seed = 0;

action current_action = null;

uint32 state_time = 0;
uint32 wait_time = 0;

int score = 0;
int score_delta = 0;

uint16_t led_values[8] = { led_off };

int const score_masks[7] = { 0x0f, 0x0e, 0x0c, 0x08, 0x18, 0x38, 0x78 };    // score can be -3 ... 3

// clang-format off
volatile uint32_t *const led_registers[8] = {
    &TIM1->CCR1,
    &TIM1->CCR2,
    &TIM1->CCR3,
    &TIM1->CCR4,
    &TIM4->CCR1,
    &TIM4->CCR2,
    &TIM4->CCR3,
    &TIM4->CCR4
};
// clang-format on

//////////////////////////////////////////////////////////////////////
// set the leds for a given bitmap

void set_leds(int x, int value)
{
    for(int i = 0; i < 8; ++i)
    {
        led_values[i] = (x & 1) ? value : led_off;
        x >>= 1;
    }
}

//////////////////////////////////////////////////////////////////////
// apply led states to the actual leds

void update_leds()
{
    for(int i = 0; i < 8; ++i)
    {
        *led_registers[i] = 32767 - led_values[i];    // (32767 - x) because leds sink from vcc into GPIOs (0 = on)
    }
}

//////////////////////////////////////////////////////////////////////
// set leds for a give score

void set_leds_by_score(int s, int x)
{
    set_leds(score_masks[s + 3], x);
}

//////////////////////////////////////////////////////////////////////
// set just one led for a given score

void set_score_led(int score, int x)
{
    led_values[score + 3] = x;
}

//////////////////////////////////////////////////////////////////////
// X^3 good enough

int gamma_correct(int x)
{
    return ((((x * x) >> 15) * x) >> 15);
}

//////////////////////////////////////////////////////////////////////
// shitty prng

void srand(int s)
{
    seed = s;
}

//////////////////////////////////////////////////////////////////////

int rand()
{
    seed = seed * 1103515245 + 12345;
    return (seed >> 16) & 0xffff;
}

//////////////////////////////////////////////////////////////////////
// -1 .. 0 .. 1

int sign(int n)
{
    return (n >> 31) | (n != 0);
}

//////////////////////////////////////////////////////////////////////

int abs(int x)
{
    return (x < 0) ? -x : x;
}

//////////////////////////////////////////////////////////////////////
// change to new state, reset state timer

void set_state(action a)
{
    current_action = a;
    state_time = ticks;
}

//////////////////////////////////////////////////////////////////////
// how long in 10KHz ticks has current state been active

uint32 get_state_time()
{
    return ticks - state_time;
}

//////////////////////////////////////////////////////////////////////
// system reset occurred, wait for any button

void do_boot(void)
{
    if(button_a.pressed || button_b.pressed)
    {
        score = 0;
        srand(ticks);
        button_a.pressed = false;
        button_b.pressed = false;
        set_state(do_get_ready);
    }
    else
    {
        int a = gamma_correct(abs(((get_state_time() << 2) & 65535) - 32768));
        for(int i = 0; i < 7; ++i)
        {
            led_values[i] = a;
        }
        led_values[7] = led_off;
    }
}

//////////////////////////////////////////////////////////////////////
// flash the score, then start turn

void do_get_ready(void)
{
    int a = gamma_correct(((get_state_time() >> 10) & 1) * 16383);
    if(get_state_time() > 9000)    // 10000 is 1 second
    {
        a = led_off;
        set_state(do_waiting);
        wait_time = (rand() >> 1) + 10000;
    }
    for(int i = 0; i < 7; ++i)
    {
        led_values[i] = a;
    }
    led_values[7] = led_off;
}

//////////////////////////////////////////////////////////////////////
// wait for random time
// button

void do_waiting(void)
{
    if(get_state_time() > wait_time)
    {
        set_state(do_snap);
    }
    else
    {
        if(button_a.pressed)
        {
            score_delta = -1;
            set_state(do_score);
        }
        if(button_b.pressed)
        {
            score_delta = 1;
            set_state(do_score);
        }
    }
}

//////////////////////////////////////////////////////////////////////

void do_snap(void)
{
    led_values[7] = gamma_correct(((~get_state_time() >> 9) & 1) * 32767);
    if(button_a.pressed)
    {
        led_values[7] = led_on;
        score_delta = 1;
        set_state(do_score);
    }
    if(button_b.pressed)
    {
        led_values[7] = led_on;
        score_delta = -1;
        set_state(do_score);
    }
}

//////////////////////////////////////////////////////////////////////

void do_score(void)
{
    int s = score + score_delta;
    if(s == 0)
    {
        s += score_delta;
    }

    set_leds_by_score(s, led_on);

    if(get_state_time() < 7500)
    {
        int a = gamma_correct(((~get_state_time() >> 9) & 1) * 32767);
        set_score_led(s, a);
    }
    else
    {
        score = s;
        set_score_led(s, led_on);
        score_delta = 0;
        if(score == 3 || score == -3)
        {
            set_state(do_winner);
        }
        else
        {
            set_state(do_waiting);
            wait_time = (rand() >> 1) + 10000;
        }
    }
}

//////////////////////////////////////////////////////////////////////

void do_winner(void)
{
    int a = gamma_correct(abs(((get_state_time() << 3) & 65535) - 32768));
    set_leds_by_score(score, a);
}

//////////////////////////////////////////////////////////////////////

void loop(void)
{
    (current_action)();
    update_leds();
}

//////////////////////////////////////////////////////////////////////

void update_button(int x, button *b)
{
    b->state = (b->state << 1) | x;
    uint16_t s = b->state & 0xffff;

    if(s == 0x0001)
    {
        b->held = true;
    }
    else if(s == 0xfffe)
    {
        b->held = false;
    }

    bool diff = b->held != b->previous_held;
    b->pressed = diff && b->held;
    b->released = diff && !b->held;
    b->previous_held = b->held;
}

//////////////////////////////////////////////////////////////////////

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim2)
{
    uint32 state = ~GPIOB->IDR;
    update_button(state & 1, &button_a);
    update_button((state >> 1) & 1, &button_b);
    ticks += 1;
}

//////////////////////////////////////////////////////////////////////

void begin(void)
{
    for(uint32 i = TIM_CHANNEL_1; i <= TIM_CHANNEL_4; i += (TIM_CHANNEL_2 - TIM_CHANNEL_1))
    {
        HAL_TIM_PWM_Start(&htim1, i);
        HAL_TIM_PWM_Start(&htim4, i);
    }
    HAL_TIM_Base_Start_IT(&htim2);
    set_state(do_boot);
}
