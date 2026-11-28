#ifndef TEST_TASK_H
#define TEST_TASK_H
#include "FreeRTOS.h"
typedef struct FakeTask *TaskHandle_t;
typedef void (*TaskFunction_t)(void *);
typedef enum { eRunning, eReady, eBlocked, eSuspended, eDeleted, eInvalid } eTaskState;
BaseType_t xTaskCreatePinnedToCore(TaskFunction_t entry, const char *name, uint32_t stack,
    void *argument, UBaseType_t priority, TaskHandle_t *handle, BaseType_t core);
TaskHandle_t xTaskGetCurrentTaskHandle(void);
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t task);
eTaskState eTaskGetState(TaskHandle_t task);
void vTaskSuspend(TaskHandle_t task);
void vTaskDelete(TaskHandle_t task);
void vTaskDelay(TickType_t ticks);
#define taskSCHEDULER_SUSPENDED ((BaseType_t)0)
#define taskSCHEDULER_NOT_STARTED ((BaseType_t)1)
#define taskSCHEDULER_RUNNING ((BaseType_t)2)
BaseType_t xTaskGetSchedulerState(void);
uint32_t ulTaskNotifyTake(BaseType_t clear_on_exit, TickType_t timeout);
BaseType_t xTaskNotifyGive(TaskHandle_t task);
#endif
