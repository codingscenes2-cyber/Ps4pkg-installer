#include "pkgmaster/platform/phone_qr_code.hpp"
#include "pkgmaster/platform/phone_web_ui.hpp"
#include <cassert>
#include <string>
int main() {
    std::string url, svg, error;
    const std::string secret(64, 'c');
    assert(pkgmaster::make_phone_pairing_url("192.168.1.20", 8080, secret, url, error));
    assert(pkgmaster::generate_phone_pairing_qr_svg(url, svg, error));
    assert(svg.find("<svg xmlns=\"http://www.w3.org/2000/svg\"") == 0);
    assert(svg.find("<rect") != std::string::npos);
    assert(svg.find("<path d=\"M") != std::string::npos);
    assert(svg.find("</svg>") != std::string::npos);
    assert(svg.find("<script") == std::string::npos);
    assert(!pkgmaster::generate_phone_pairing_qr_svg("", svg, error));
    assert(!pkgmaster::generate_phone_pairing_qr_svg("https://example.com", svg, error));
    assert(!pkgmaster::generate_phone_pairing_qr_svg(url, svg, error, 1, 4));
    assert(!pkgmaster::generate_phone_pairing_qr_svg(url, svg, error, 5, 3));
    assert(!pkgmaster::generate_phone_pairing_qr_svg(std::string(513, 'x'), svg, error));
}
