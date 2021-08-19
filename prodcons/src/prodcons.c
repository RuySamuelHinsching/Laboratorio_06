#include "system_tm4c1294.h" // CMSIS-Core
#include "driverleds.h" // device drivers
#include "cmsis_os2.h" // CMSIS-RTOS
#include "driverbuttons.h" // Projects/drivers
#include <stdbool.h>
#include "inc/hw_memmap.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "inc/hw_gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/pwm.h"
#include "inc/hw_types.h"
#include <stdint.h>

#define BUFFER_SIZE 16

typedef struct Led
{
  int tempo;
  int led;
  int id;
}Led;

void initLedBotao();

osThreadId_t controladora_id;
osThreadId_t threadLED[4];

osMessageQueueId_t filaControladora_id;
osMessageQueueId_t filaAcionadora_id[4];

struct Led led1, led2, led3, led4;

Led ledAnterior;


osMutexId_t mutexLed;

typedef enum {selecionaLed=0, mudaIntensidade, nada} Mensagem;

void GPIOJ_Handler(void)
{
  int status=0;

  status = GPIOIntStatus(GPIO_PORTJ_BASE, true);
  GPIOIntClear(GPIO_PORTJ_BASE, status);
  
  GPIOIntDisable(GPIO_PORTJ_BASE, GPIO_INT_PIN_0);
  GPIOIntDisable(GPIO_PORTJ_BASE, GPIO_INT_PIN_1);
  
  Mensagem mensagem_id;
  if( (status & GPIO_INT_PIN_0) == GPIO_INT_PIN_0)
  {
    mensagem_id = selecionaLed;
    osMessageQueuePut (filaControladora_id, &mensagem_id, 0, 0);
  }
      
  else if( (status & GPIO_INT_PIN_1) == GPIO_INT_PIN_1)
  {
    mensagem_id = mudaIntensidade;
    osMessageQueuePut (filaControladora_id, &mensagem_id, 0, 0);
  }
    
  GPIOIntEnable(GPIO_PORTJ_BASE, GPIO_INT_PIN_0);
  GPIOIntEnable(GPIO_PORTJ_BASE, GPIO_INT_PIN_1);
}


void controladora(void *arg)
{
  Mensagem mensagem;
  
  int led_id = 0;
  ledAnterior.id = led_id;
  ledAnterior.tempo = 0;
  while (1)
  {
    osMessageQueueGet (filaControladora_id, &mensagem, NULL, osWaitForever); //a mensagem pega eh armazenada em &mensagem
    
    if (mensagem == selecionaLed)
    {
      led_id = ledAnterior.id;
      if (led_id < 3)
        led_id = led_id + 1;
      else
        led_id = 0;
      switch (led_id)
      {
        case 0:
          osMessageQueuePut (filaAcionadora_id[led1.id], &led1.tempo, 0, osWaitForever);
        case 1:
          osMessageQueuePut (filaAcionadora_id[led2.id], &led2.tempo, 0, osWaitForever);
        case 2:
          osMessageQueuePut (filaAcionadora_id[led3.id], &led3.tempo, 0, osWaitForever);
        case 3:
          osMessageQueuePut (filaAcionadora_id[led4.id], &led4.tempo, 0, osWaitForever);
      }    
      ledAnterior.id = led_id;
    }
    else if (mensagem == mudaIntensidade)
    {
      int tempo = 0;
      switch (led_id)
      {
        case 0:
          tempo = led1.tempo;
        case 1:
          tempo = led2.tempo;
        case 2:
          tempo = led3.tempo;
        case 3:
          tempo = led4.tempo;
      } 
      if (tempo <=80)
        tempo = tempo+10;
      else
        tempo = 0;
      switch (led_id)
      {
        case 0:
          led1.tempo = tempo;
          osMessageQueuePut (filaAcionadora_id[led_id], &led1.tempo, 0, osWaitForever);
        case 1:
          led2.tempo = tempo;
          osMessageQueuePut (filaAcionadora_id[led_id], &led2.tempo, 0, osWaitForever);
        case 2:
          led3.tempo = tempo;
          osMessageQueuePut (filaAcionadora_id[led_id], &led3.tempo, 0, osWaitForever);
        case 3:
          led4.tempo = tempo;
          osMessageQueuePut (filaAcionadora_id[led_id], &led4.tempo, 0, osWaitForever);
      }  
    }
    mensagem = nada;
  }
}

void acionadora(void *arg)
{
  struct Led* led = arg;
  struct Led ledAux = *led;
  ledAux.id = led->id;
  ledAux.led = led->led;
  ledAux.tempo = led->tempo;
  int tick, i, flag = 0;

  while (1)
  {
    flag = 0;
    osMessageQueueGet (filaAcionadora_id[ledAux.id], &ledAux.tempo , NULL, osWaitForever); //a mensagem pega eh armazenada em &mensagem
    
    ledAux.id = led->id;
    ledAux.led = led->led;
    ledAux.tempo = led->tempo;
    
    if (ledAux.tempo >= 0)
    {
      osMutexAcquire(mutexLed, 0);
      LEDOn(ledAux.led);
      osMutexRelease(mutexLed);
    }
    tick = osKernelGetTickCount(); 
    osDelayUntil(ledAux.tempo + tick);
    
    osMutexAcquire(mutexLed, 0);
    LEDOff(ledAux.led);
    osMutexRelease(mutexLed);
    tick = osKernelGetTickCount();  
    osDelayUntil((100-ledAux.tempo) + tick);
    
    for(i=0; i<4; i++)
    {
      if(osMessageQueueGetCount(filaAcionadora_id[i]) > 0)
      {
        flag++;
      }
    }
    
    if(!flag)
    {
      if (ledAux.id == 0)
      {
        osMessageQueuePut (filaAcionadora_id[led1.id], &led1.tempo, 0, osWaitForever);
      }
      else if (ledAux.id == 1)
      {
        osMessageQueuePut (filaAcionadora_id[led2.id], &led2.tempo, 0, osWaitForever);
      }
      else if (ledAux.id == 2)
      {
        osMessageQueuePut (filaAcionadora_id[led3.id], &led3.tempo, 0, osWaitForever);
      }
      else if (ledAux.id == 3)
      {
        osMessageQueuePut (filaAcionadora_id[led4.id], &led4.tempo, 0, osWaitForever);
      }
    }  
  }
}

void main(void)
{
  SystemInit();
  
  initLedBotao();
  const osMutexAttr_t Mutex_attr = {"piscaLed",osMutexRecursive | osMutexPrioInherit,NULL, 0};
  mutexLed = osMutexNew(&Mutex_attr);
  
  osKernelInitialize();

  controladora_id = osThreadNew(controladora, NULL, NULL);
  filaControladora_id = osMessageQueueNew (BUFFER_SIZE, sizeof(int), NULL);
  
  led1.led = LED1;
  led1.tempo = 0;
  led1.id = 0;

  led2.led = LED2;
  led2.tempo = 0;
  led2.id = 1;  

  led3.led = LED3;
  led3.tempo = 0;
  led3.id = 2;

  led4.led = LED4;
  led4.tempo = 0;
  led4.id = 3; 
  
  threadLED[0] = osThreadNew(acionadora, &led1, NULL);
  threadLED[1] = osThreadNew(acionadora, &led2, NULL);
  threadLED[2] = osThreadNew(acionadora, &led3, NULL);
  threadLED[3] = osThreadNew(acionadora, &led4, NULL);
  
  filaAcionadora_id[0] = osMessageQueueNew (BUFFER_SIZE, sizeof(int), NULL);
  filaAcionadora_id[1] = osMessageQueueNew (BUFFER_SIZE, sizeof(int), NULL);
  filaAcionadora_id[2] = osMessageQueueNew (BUFFER_SIZE, sizeof(int), NULL);
  filaAcionadora_id[3] = osMessageQueueNew (BUFFER_SIZE, sizeof(int), NULL);
  
  if(osKernelGetState() == osKernelReady)
    osKernelStart();

  while(1){}
} // main

void initLedBotao()
{
  ButtonInit(USW1 | USW2);
  ButtonIntDisable(USW1 | USW2);
  ButtonIntEnable(USW1 | USW2);
  GPIOIntRegister(GPIO_PORTJ_BASE, GPIOJ_Handler);
  
  LEDInit(LED4 | LED3 | LED2 | LED1);
}
