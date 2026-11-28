#ifndef TEST_FREERTOS_H
#define TEST_FREERTOS_H
#include <stdint.h>
#include <stdbool.h>
typedef int BaseType_t;
typedef unsigned UBaseType_t;
typedef uint32_t TickType_t;
#define configTICK_RATE_HZ 100U
#define configMINIMAL_STACK_SIZE 1536U
#define configMAX_PRIORITIES 25U
#ifndef configNUMBER_OF_CORES
#define configNUMBER_OF_CORES 2
#endif
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define portMAX_DELAY UINT32_MAX
#define tskNO_AFFINITY (-1)
BaseType_t xPortInIsrContext(void);
bool xPortCanYield(void);
typedef struct { unsigned depth; } portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED {0U}
void fakePortEnterCritical(portMUX_TYPE *lock);
void fakePortExitCritical(portMUX_TYPE *lock);
#define portENTER_CRITICAL(lock) fakePortEnterCritical(lock)
#define portEXIT_CRITICAL(lock) fakePortExitCritical(lock)
#endif
