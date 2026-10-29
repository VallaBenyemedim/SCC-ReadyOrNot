#pragma once
#include <cstdint>
namespace GameplayPolicy {
    inline bool CanRequestSurrender(uint64_t Now,uint64_t LastAttempt,bool PlayingAnimation,bool Surrendered) {
        return !PlayingAnimation && !Surrendered && (LastAttempt==0 || Now-LastAttempt>=2500);
    }
}
