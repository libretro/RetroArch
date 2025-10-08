//
// Created by ash on 5/9/25.
//

#ifndef RETROARCH_AC_H
#define RETROARCH_AC_H

#include <nn/ac/ac_c.h>

// Missing from wut headers
// Prefered [sic] typo is really present in CafeOS
NNResult
ACGetAssignedPreferedDns(uint32_t *ip);

NNResult
ACGetAssignedAlternativeDns(uint32_t *ip);

NNResult
ACGetAssignedGateway(uint32_t *ip);

NNResult
ACGetAssignedSubnet(uint32_t *ip);

#endif //RETROARCH_AC_H
