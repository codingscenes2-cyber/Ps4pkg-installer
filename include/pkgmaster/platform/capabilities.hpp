#pragma once
#include <cstdint>
#include <string>
namespace pkgmaster {
enum class Capability : std::uint32_t { None=0, FileSystem=1u<<0, NetworkClient=1u<<1, NetworkServer=1u<<2, ControllerInput=1u<<3, NativeUi=1u<<4, PackageInstallHandoff=1u<<5, UsbStorage=1u<<6, ArchiveExtraction=1u<<7, QrGeneration=1u<<8 };
constexpr Capability operator|(Capability a, Capability b) { return static_cast<Capability>(static_cast<std::uint32_t>(a)|static_cast<std::uint32_t>(b)); }
struct RuntimeInfo { int firmware_major=0, firmware_minor=0, firmware_patch=0; std::string goldhen_version, console_model; std::uint32_t available=0; bool firmware_detected=false, goldhen_detected=false; };
struct FeatureDecision { bool available=false; std::string reason; };
bool has_capability(const RuntimeInfo&, Capability);
FeatureDecision evaluate_feature(const RuntimeInfo&, Capability);
std::string describe_runtime(const RuntimeInfo&);
bool parse_firmware_version(const std::string&, int&, int&, int&);
}
