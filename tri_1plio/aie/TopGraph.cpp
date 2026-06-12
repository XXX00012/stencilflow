#include "TopGraph.h"

TopStencilGraph topStencil("stencil");

#if defined(__AIESIM__) || defined(__X86SIM__)
int main() {

    // kIter is a RUN count; each run processes kRowsPerCall rows.
    // Rows processed = kIter * kRowsPerCall. For a 256x20 test set kIter=10.
    constexpr int kIter = 10;

    topStencil.init();
    topStencil.run(kIter);
    topStencil.end();
    return 0;
}
#endif
