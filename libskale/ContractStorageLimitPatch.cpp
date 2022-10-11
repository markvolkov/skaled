#include "ContractStorageLimitPatch.h"

time_t ContractStorageLimitPatch::introduceChangesTimestamp;
time_t ContractStorageLimitPatch::lastBlockTimestamp;

bool ContractStorageLimitPatch::isEnabled() {
    return introduceChangesTimestamp <= lastBlockTimestamp;
}
