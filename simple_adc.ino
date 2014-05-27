#include "Energia.h"
#include "inc/hw_ints.h"
#include "inc/hw_nvic.h"
#include "inc/hw_sysctl.h"
#include "inc/hw_types.h"
#include "inc/hw_timer.h"
#include "driverlib/rom.h"
#include "driverlib/cpu.h"
#include "driverlib/debug.h"
#include "driverlib/interrupt.h"
#include "driverlib/timer.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"

#define NUMBER_VALUES 48
#define START 0
#define STOP 1
#define RUNNING 2
#define PACKET_START 1
#define PACKET_SIZE 2
#define PACKET_DATA 3
#define PACKET_END 4
#define PACKET_START_MAGIC 0x5c
#define PACKET_END_MAGIC 0xc5
#define START_MAGIC 0xfc
#define STOP_MAGIC 0Xcf

int adc_pin = A1 ; 
unsigned int adc_value = 0 ;
unsigned int state = START;
unsigned int packet_state = PACKET_START ;

struct queue_data_struct
{
  unsigned char value ;
  struct queue_data_struct *next ;
} ;

typedef queue_data_struct queue_data;

queue_data *head ;
queue_data *tail ;
queue_data *queue ;

void queue_add(unsigned char data)
{
  queue_data *p = (queue_data*)malloc(sizeof(*p)) ;
  p->value = data ;
  p->next = NULL ;
  if (head == NULL && tail == NULL)
  {
    head = p ;
    tail = p ;
    queue = p ;
  }
  else
  {
    tail->next = p ;
    tail = p ;
  }
}

unsigned char queue_pop()
{
  queue_data *v = NULL ;
  queue_data *p = NULL ;
  v = head ;
  unsigned char data = head->value ;
  p = v->next ;
  free(v) ;
  head = p ;
  if (head == NULL) tail = head ;
  return data ;
}

unsigned int is_empty()
{
  if (queue == NULL) return 1 ;
  if (head == NULL && tail == NULL) return 1; 
  return 0 ;
}

unsigned int queue_count()
{
  queue_data *p = NULL ;
  unsigned int count = 0 ;
  for (p = head; p != NULL; p = p->next) count++ ;
  return count ;
  
}
void timer_init(unsigned int time)
{
   SysTickPeriodSet(SysCtlClockGet()/time);
   SysTickIntRegister(SysTick_Handler);
   SysTickIntEnable();
   SysTickEnable();
}

void timer_disable()
{
  SysTickIntDisable();
  SysTickDisable();
}

void setup()
{
  queue = NULL ; head = NULL ; tail = NULL ;
  Serial.begin(115200); 
}
void loop()
{
  if (Serial.available())
  {
    unsigned int code = Serial.read() ;
    if (code == START_MAGIC)
    {
      state = RUNNING ;
      timer_init(1000) ;  
    }
    if (code == STOP_MAGIC)
    {
      state = STOP ;
      timer_disable() ;
    }
  } 
  delay(200) ; 
}

unsigned int cnt = 0 ;

void SysTick_Handler(void)
{
  if (state == RUNNING)
  {
    unsigned int adc_value = analogRead(adc_pin) ;
    unsigned char low_byte = (adc_value & 0xFF) ;
    queue_add(low_byte) ;
    unsigned char high_byte = (adc_value & 0xF00) >> 8 ;
    queue_add(high_byte) ;
    if (packet_state == PACKET_START)
    {
      Serial.write(PACKET_START_MAGIC) ;
      packet_state = PACKET_SIZE ;
      return ;
    }
    if (packet_state == PACKET_SIZE)
    {
      unsigned char size_low = ((NUMBER_VALUES) & 0xFF) ;
      unsigned char size_high = NUMBER_VALUES ;
      size_high = (size_high & 0xFF00) >> 8 ;
      Serial.write(size_low) ;
      Serial.write(size_high) ;
      packet_state = PACKET_DATA ;
      return ; 
    }
    if (packet_state == PACKET_DATA)
    {
      if (cnt < NUMBER_VALUES)
      {
        if (!is_empty())
        {
          Serial.write(queue_pop()) ;
          Serial.write(queue_pop()) ;
          cnt++ ;
        }
      }
      else
      {
        packet_state = PACKET_END ;
        cnt = 0 ;
      }
      return ;
    } 
    if (packet_state == PACKET_END)
    {
      packet_state = PACKET_START ;
      Serial.write(PACKET_END_MAGIC) ;
      return ;
    }
  }
}
