#include "pkgmaster/platform/phone_pairing_auth.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
int main() {
    const std::string pairing(64, 'a');
    const std::string session(64, 'b');
    pkgmaster::PhonePairingAuth auth;
    std::string error, issued;
    assert(auth.arm(pairing, session, 1000, 60000, 3600000, error));
    assert(auth.pairing_pending());
    assert(!auth.authenticate(session, 1001));
    assert(!auth.exchange(std::string(64, 'c'), 1002, issued));
    assert(auth.exchange(pairing, 1003, issued));
    assert(issued == session);
    assert(!auth.pairing_pending());
    assert(!auth.exchange(pairing, 1004, issued));
    assert(auth.authenticate(session, 1004));
    assert(!auth.authenticate(std::string(64, 'c'), 1004));
    assert(!auth.authenticate(session, 3601000));
    assert(auth.authenticate(session, 3600999));
    auth.revoke();
    assert(!auth.authenticate(session, 1005));
    assert(!auth.arm(std::string(63, 'a'), session, 0, 1000, 60000, error));
    assert(!auth.arm(pairing, session, 0, 999, 60000, error));
    assert(!auth.arm(pairing, session, 0, 300001, 60000, error));
    assert(!auth.arm(pairing, session, 0, 1000, 59999, error));
    assert(!auth.arm(pairing, pairing, 0, 1000, 60000, error));
    assert(!auth.arm(pairing, session, std::numeric_limits<std::uint64_t>::max()-500,
                     1000, 60000, error));
    assert(auth.arm(pairing, session, 0, 1000, 60000, error));
    assert(!auth.exchange(pairing, 1000, issued)); // expiry is exclusive
    assert(!auth.authenticate(session, 1000));
    std::cout << "phone pairing auth tests passed\n";
}
