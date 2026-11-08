#include "pkgmaster/platform/phone_web_ui.hpp"
#include <cassert>
#include <string>
int main() {
    const auto& html = pkgmaster::phone_web_ui_html();
    const auto& js = pkgmaster::phone_web_ui_javascript();
    assert(html.find("PKGMaster Phone Link") != std::string::npos);
    assert(html.find("/app.js") != std::string::npos);
    assert(js.find("/api/v1/pair") != std::string::npos);
    assert(js.find("sessionStorage") != std::string::npos);
    assert(js.find("createElement") != std::string::npos);
    std::string url, error;
    const std::string secret(64, 'a');
    assert(pkgmaster::make_phone_pairing_url("192.168.1.20", 8080, secret, url, error));
    assert(url == "http://192.168.1.20:8080/pair#" + secret);
    assert(!pkgmaster::make_phone_pairing_url("127.0.0.1", 8080, secret, url, error));
    assert(!pkgmaster::make_phone_pairing_url("192.168.1.20", 0, secret, url, error));
    assert(!pkgmaster::make_phone_pairing_url("192.168.1.20", 8080, std::string(63, 'a'), url, error));
    assert(!pkgmaster::make_phone_pairing_url("192.168.1.20", 8080, std::string(64, 'z'), url, error));
}
