// Minimal RadLib consumer: opens whichever detectors are attached and prints one
// one-second reading from each. Builds and runs without any hardware attached.
//
//   radlib_minimal [path/to/gef_C12137.csv]

#include <cstdio>

#include "c12137/c12137_driver.hpp"
#include "d3s/d3s_driver.hpp"
#include "gr1/gr1_driver.hpp"
#include "sigma50/sigma50_driver.hpp"

int main(int argc, char * argv[])
{
    const char * ge_file = (argc > 1) ? argv[1] : "gef_C12137.csv";

    gr1::GR1Driver gr1;
    if (gr1.open()) {
        gr1::Reading r;
        if (gr1.read(r, 1.0)) std::printf("GR1:      %.1f cps over %.2f s\n", r.cps, r.elapsed_s);
        gr1.close();
    } else {
        std::printf("GR1:      not found\n");
    }

    sigma50::Sigma50Driver sigma;
    if (sigma.open()) {
        sigma50::Reading r;
        if (sigma.read(r, 1.0)) std::printf("Sigma 50: %.1f cps over %.2f s\n", r.cps, r.elapsed_s);
        sigma.close();
    } else {
        std::printf("Sigma 50: not found\n");
    }

    c12137::C12137Driver hama;
    if (hama.open(ge_file)) {
        c12137::Reading r;
        if (hama.read(r, 1.0)) std::printf("C12137:   %.4f uSv/h, %.1f C\n", r.dose_uSv_h, r.temperature_c);
        hama.close();
    } else {
        std::printf("C12137:   not found (or G(E) file %s missing)\n", ge_file);
    }

    d3s::D3SDriver d3s;
    if (d3s.open()) {
        d3s::Reading r;
        if (d3s.read(r, 1.0)) std::printf("D3S:      %.2f neutron cps, %.0f C\n", r.neutron_cps, r.temperature_c);
        d3s.close();
    } else {
        std::printf("D3S:      not found\n");
    }

    return 0;
}
