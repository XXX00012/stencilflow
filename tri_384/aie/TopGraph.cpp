#include "TopGraph.h"

TopStencilGraph topStencil("stencil");

#if defined(__AIESIM__) || defined(__X86SIM__)
int main() {
    
    constexpr int kIter = hdiff_cfg::kDefaultIterations;

    topStencil.init();
    topStencil.run(kIter);
    topStencil.end();
    return 0;
}
#endif
