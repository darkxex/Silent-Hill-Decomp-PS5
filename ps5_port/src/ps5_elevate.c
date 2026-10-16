#include "ps5_unlock.h"

int ps5_elevate_lapy(void);

int ps5_elevate(void)
{
    UnlockResult quick;
    UnlockResult hen;
    int          lapy;

    if (ps5_data_listable())
    {
        ps5_unlock_trace_add("listable at start; ");
        return 1;
    }
    /* Lapy first: it needs /app0/lapy.elf, which stops being visible once a HEN has taken the
     * process out of the sandbox, and a grant that leaves /data unlistable is no grant. */
    lapy = ps5_elevate_lapy();
    ps5_unlock_trace_add("lapy=%d listable=%d; ", lapy, (int)ps5_data_listable());
    if (lapy == 0 && ps5_data_listable())
        return 1;
    quick = ps5_unlock_quick();
    ps5_unlock_trace_add("quick=%d; ", (int)quick);
    if (quick == UNLOCK_OK)
        return 1;
    hen = ps5_unlock_etahen();
    ps5_unlock_trace_add("etahen=%d listable=%d", (int)hen, (int)ps5_data_listable());
    if (hen == UNLOCK_OK || hen == UNLOCK_ALREADY)
        return 1;
    return ps5_data_listable();
}
