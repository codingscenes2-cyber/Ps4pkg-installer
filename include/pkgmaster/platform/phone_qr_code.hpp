#pragma once
#include <string>

namespace pkgmaster {
// Produces a self-contained black-on-white SVG QR code suitable for the native
// console UI. The caller should pass the URL returned by make_phone_pairing_url.
bool generate_phone_pairing_qr_svg(const std::string& pairing_url,
                                   std::string& svg,
                                   std::string& error,
                                   int module_scale = 5,
                                   int quiet_zone = 4);
}
