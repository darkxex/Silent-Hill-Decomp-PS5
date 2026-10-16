#include "ps5_unlock.h"

int ps5_elevate_lapy(void);

int ps5_elevate(void)
{
    if (ps5_data_listable())
        return 1;
    if (ps5_unlock_quick() == UNLOCK_OK)
        return 1;
    UnlockResult hen = ps5_unlock_etahen();
    if (hen == UNLOCK_OK || hen == UNLOCK_ALREADY)
        return 1;
    return ps5_elevate_lapy() == 0 && ps5_data_listable();
}
