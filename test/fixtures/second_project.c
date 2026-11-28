/* An unrelated product binds its own entry and runtime settings. */
#include "EbOs.h"

static void screen_entry(void *argument)
{
    uint32 *frames = argument;
    ++*frames;
}

const EbOs_TaskConfigType SecondProject_TaskConfig = {
    .name = "ScreenRefresh",
    .stack_bytes = 3072U,
    .priority = 3U,
    .core_affinity = EBOS_CORE_ANY,
    .entry = screen_entry
};
