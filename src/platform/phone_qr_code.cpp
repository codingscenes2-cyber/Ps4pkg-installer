#include "pkgmaster/platform/phone_qr_code.hpp"
#include "qrcodegen.hpp"
#include <sstream>
#include <utility>

namespace pkgmaster {
bool generate_phone_pairing_qr_svg(const std::string& pairing_url,
                                   std::string& svg,
                                   std::string& error,
                                   int module_scale,
                                   int quiet_zone) {
    error.clear();
    svg.clear();
    if (pairing_url.empty() || pairing_url.size() > 512 ||
        pairing_url.compare(0, 7, "http://") != 0 ||
        module_scale < 2 || module_scale > 16 ||
        quiet_zone < 4 || quiet_zone > 8) {
        error = "Invalid pairing URL or QR rendering dimensions";
        return false;
    }
    try {
        const auto qr = qrcodegen::QrCode::encodeText(
            pairing_url.c_str(), qrcodegen::QrCode::Ecc::QUARTILE);
        const int size = qr.getSize();
        const int extent = (size + quiet_zone * 2) * module_scale;
        std::ostringstream path;
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                if (qr.getModule(x, y)) {
                    const int px = x + quiet_zone;
                    const int py = y + quiet_zone;
                    path << 'M' << px << ',' << py << "h1v1h-1z";
                }
            }
        }
        std::ostringstream out;
        out << "<svg xmlns=\"http://www.w3.org/2000/svg\""
            << " width=\"" << extent << "\" height=\"" << extent << "\""
            << " viewBox=\"0 0 " << size + quiet_zone * 2 << ' '
            << size + quiet_zone * 2 << "\" shape-rendering=\"crispEdges\">"
            << "<rect width=\"100%\" height=\"100%\" fill=\"#fff\"/>"
            << "<path d=\"" << path.str() << "\" fill=\"#000\"/>"
            << "</svg>";
        svg = out.str();
        return true;
    } catch (const std::exception&) {
        error = "Could not encode pairing URL as a QR code";
        return false;
    }
}
}
