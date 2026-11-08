#pragma once
#include <cstdint>
#include <string>

namespace pkgmaster {
const std::string& phone_web_ui_html();
const std::string& phone_web_ui_javascript();
bool make_phone_pairing_url(const std::string& private_lan_ipv4,
                            std::uint16_t port,
                            const std::string& pairing_secret,
                            std::string& output,
                            std::string& error);
}
