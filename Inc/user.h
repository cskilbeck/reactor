#pragma once

#define null 0

#define true 1
#define false 0
    
typedef int bool;
typedef uint32_t uint32;

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim4;

void begin(void);
void loop(void);

int rand(void);         // 0 .. 65535
void srand(int);
