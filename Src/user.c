//////////////////////////////////////////////////////////////////////

#include "main.h"

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

void do_boot(void);
void do_get_ready(void);
void do_waiting(void);
void do_ready(void);
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

void reset_score()
{
    score = 0;
}

//////////////////////////////////////////////////////////////////////

int gamma(int x)
{
    return 32767 - ((((x * x) >> 15) * x) >> 15);
}

//////////////////////////////////////////////////////////////////////

void set_state(action a)
{
    current_action = a;
    state_time = ticks;
}

//////////////////////////////////////////////////////////////////////

uint32 get_state_time()
{
    return ticks - state_time;
}

//////////////////////////////////////////////////////////////////////

void do_boot(void)
{
    if(button_a.pressed || button_b.pressed)
    {
        reset_score();
        set_state(do_get_ready);
        srand(ticks);
        button_a.pressed = false;
        button_b.pressed = false;
    }
    int a = ((ticks << 2) & 65535) - 32768;
    if(a < 0) {
        a = -a;
    }

    a = gamma(a);
    TIM1->CCR1 = a;     // R3
    TIM1->CCR2 = a;     // R2
    TIM1->CCR3 = a;     // R1
    TIM1->CCR4 = a;     // MIDDLE
    TIM4->CCR1 = a;     // L1
    TIM4->CCR2 = a;     // L2
    TIM4->CCR3 = a;     // L3
    TIM4->CCR4 = gamma(0);     // SNAP
}

//////////////////////////////////////////////////////////////////////
// flash the score, then start to wait

void do_get_ready(void)
{
    int a = gamma(((get_state_time() >> 10) & 1) * 16383);
    if(get_state_time() > 9000) {   // 10000 is 1 second
        a = gamma(0);
        wait_time = (rand() >> 1) + 10000;
        set_state(do_waiting);
    }
    TIM1->CCR1 = a;
    TIM1->CCR2 = a;
    TIM1->CCR3 = a;
    TIM1->CCR4 = a;
    TIM4->CCR1 = a;
    TIM4->CCR2 = a;
    TIM4->CCR3 = a;
    TIM4->CCR4 = gamma(0);
}

//////////////////////////////////////////////////////////////////////

void do_waiting(void)
{
    if(get_state_time() > wait_time) {
        set_state(do_ready);
    } else {
        if(button_a.pressed) {
            score += 1;
            set_state(do_get_ready);
        }
        if(button_b.pressed) {
            score -= 1;
            set_state(do_get_ready);
        }
    }
}

//////////////////////////////////////////////////////////////////////

void do_ready(void)
{
    int a = gamma(((~get_state_time() >> 9) & 1) * 32767);
    TIM4->CCR4 = a;
    if(button_a.pressed) {
        score -= 1;
        set_state(do_get_ready);
    }
    if(button_b.pressed) {
        score += 1;
        set_state(do_get_ready);
    }
}

//////////////////////////////////////////////////////////////////////

void do_score(void)
{
}

//////////////////////////////////////////////////////////////////////

void do_winner(void)
{
}

//////////////////////////////////////////////////////////////////////

void loop(void)
{
    (current_action)();
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
    for(uint32 i = TIM_CHANNEL_1; i <= TIM_CHANNEL_4; i += (TIM_CHANNEL_2 - TIM_CHANNEL_1)) {
        HAL_TIM_PWM_Start(&htim1, i);
        HAL_TIM_PWM_Start(&htim4, i);
    }
    HAL_TIM_Base_Start_IT(&htim2);
    current_action = do_boot;
}
