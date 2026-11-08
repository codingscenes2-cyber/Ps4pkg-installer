#include "pkgmaster/platform/phone_lan_server.hpp"
#include <cassert>
#include <string>
int main() {
    using pkgmaster::PhoneLanServer;
    assert(PhoneLanServer::is_private_lan_ipv4("192.168.1.20"));
    assert(PhoneLanServer::is_private_lan_ipv4("10.0.0.2"));
    assert(PhoneLanServer::is_private_lan_ipv4("172.16.0.1"));
    assert(PhoneLanServer::is_private_lan_ipv4("172.31.255.254"));
    assert(!PhoneLanServer::is_private_lan_ipv4("172.32.0.1"));
    assert(!PhoneLanServer::is_private_lan_ipv4("192.169.1.1"));
    assert(!PhoneLanServer::is_private_lan_ipv4("127.0.0.1"));
    assert(!PhoneLanServer::is_private_lan_ipv4("0.0.0.0"));
    assert(!PhoneLanServer::is_private_lan_ipv4("169.254.1.1"));
    assert(!PhoneLanServer::is_private_lan_ipv4("8.8.8.8"));
    PhoneLanServer server;
    std::string secret, error;
    assert(!server.start("127.0.0.1", 8080, secret, error));
    assert(!error.empty() && !server.running());
    assert(!server.start("192.168.1.2", 0, secret, error));
    assert(!server.running());
}
