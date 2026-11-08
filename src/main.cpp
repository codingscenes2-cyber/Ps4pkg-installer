#include <iostream>
#include "pkgmaster/core/queue.hpp"
#include "pkgmaster/platform/installer_bridge.hpp"

int main() {
    pkgmaster::Queue queue;
    pkgmaster::Job welcome;
    welcome.file_name = "Drop a PKG link from your phone";
    queue.add(welcome);

    std::cout << "PKGMaster portable core " << "0.1.0\n";
    std::cout << "Jobs: " << queue.snapshot().size() << "\n";
    std::cout << "PS4 UI and native installer bridge are platform work still pending.\n";
    return 0;
}
