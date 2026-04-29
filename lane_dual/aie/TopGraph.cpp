#include "TopGraph.h"

TopStencilGraph topStencil("stencil");

#if defined(__AIESIM__) || defined(__X86SIM__) || defined(__ADF_FRONTEND__)
int main() {
    constexpr int ITER = 8;

    topStencil.init();
    topStencil.run(ITER);
    topStencil.end();

    return 0;
}
#endif