#ifndef EBOS_H
#define EBOS_H

#include "Neo_Types.h"

#define EBOS_WAIT_FOREVER UINT32_MAX

typedef struct EbOs_Mutex *EbOs_MutexType;
typedef struct EbOs_Task *EbOs_TaskType;

#define EBOS_CORE_ANY (-1)

typedef struct {
    const char *name;
    uint32 stack_bytes;
    uint32 priority;
    sint32 core_affinity;
    void (*entry)(void *argument);
} EbOs_TaskConfigType;

/* Creation/deletion require quiescent users; lifecycle calls are serialized. */
Neo_StatusType EbOs_MutexCreate(EbOs_MutexType *out);
Neo_StatusType EbOs_MutexLock(EbOs_MutexType mutex, uint32 timeout_ms);
void EbOs_MutexUnlock(EbOs_MutexType mutex);
void EbOs_MutexDelete(EbOs_MutexType mutex);
void EbOs_Delay(uint32 ms);
/* Yield for one actual scheduler tick, independently of the tick frequency. */
void EbOs_DelayOneTick(void);

/* Task context only. Configuration and argument must remain valid until the
 * entry returns. Serialize create, join/delete and stack sampling for a handle.
 * Entry returns cooperatively; it must not self-delete or suspend itself.
 * Native handles are private: callers must never resume a completed task. */
Neo_StatusType EbOs_TaskCreate(const EbOs_TaskConfigType *config, void *argument, EbOs_TaskType *out);
/* Success deletes the completed task and clears *task. A null *task is a no-op.
 * Timeout preserves it for retry; this function never cancels running work. */
Neo_StatusType EbOs_TaskJoinAndDelete(EbOs_TaskType *task, uint32 timeout_ms);
Neo_StatusType EbOs_TaskGetStackMinFree(EbOs_TaskType task, uint32 *out_bytes);

#endif
