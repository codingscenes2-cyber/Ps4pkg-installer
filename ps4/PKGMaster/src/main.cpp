#include <cstdio>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include <orbis/Pad.h>
#include <orbis/UserService.h>

#include "graphics.h"
#include "log.h"
#include "pkg_scanner.hpp"
#include "pkg_inspector.hpp"

static constexpr int kWidth = 1920;
static constexpr int kHeight = 1080;
static constexpr int kDepth = 4;

std::stringstream debugLogStream;

struct Glyph {
    char character;
    uint8_t rows[7];
};

static const Glyph kFont[] = {
    {'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}},
    {'C',{14,17,16,16,16,17,14}}, {'D',{30,17,17,17,17,17,30}},
    {'E',{31,16,16,30,16,16,31}}, {'F',{31,16,16,30,16,16,16}},
    {'G',{14,17,16,23,17,17,15}}, {'H',{17,17,17,31,17,17,17}},
    {'I',{31,4,4,4,4,4,31}}, {'J',{7,2,2,2,18,18,12}},
    {'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
    {'M',{17,27,21,21,17,17,17}}, {'N',{17,25,21,19,17,17,17}},
    {'O',{14,17,17,17,17,17,14}}, {'P',{30,17,17,30,16,16,16}},
    {'Q',{14,17,17,17,21,18,13}}, {'R',{30,17,17,30,20,18,17}},
    {'S',{15,16,16,14,1,1,30}}, {'T',{31,4,4,4,4,4,4}},
    {'U',{17,17,17,17,17,17,14}}, {'V',{17,17,17,17,17,10,4}},
    {'W',{17,17,17,21,21,21,10}}, {'X',{17,17,10,4,10,17,17}},
    {'Y',{17,17,10,4,4,4,4}}, {'Z',{31,1,2,4,8,16,31}},
    {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}},
    {'2',{14,17,1,2,4,8,31}}, {'3',{30,1,1,14,1,1,30}},
    {'4',{2,6,10,18,31,2,2}}, {'5',{31,16,16,30,1,1,30}},
    {'6',{14,16,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}},
    {'8',{14,17,17,14,17,17,14}}, {'9',{14,17,17,15,1,1,14}},
    {'-',{0,0,0,31,0,0,0}}, {':',{0,4,4,0,4,4,0}},
    {'/',{1,2,2,4,8,8,16}}, {'.',{0,0,0,0,0,12,12}},
    {'>',{16,8,4,2,4,8,16}}, {'<',{1,2,4,8,4,2,1}},
    {'!',{4,4,4,4,4,0,4}}, {'?',{14,17,1,2,4,0,4}},
    {'_',{0,0,0,0,0,0,31}}, {' ',{0,0,0,0,0,0,0}}
};

static const Glyph* findGlyph(char c) {
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    for (const auto& glyph : kFont) {
        if (glyph.character == c) return &glyph;
    }
    return &kFont[sizeof(kFont) / sizeof(kFont[0]) - 1];
}

static void drawText(Scene2D* scene, int x, int y, const char* text,
                     Color color, int scale) {
    for (const char* p = text; *p; ++p) {
        const Glyph* glyph = findGlyph(*p);
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (glyph->rows[row] & (1u << (4 - col))) {
                    scene->DrawRectangle(x + col * scale, y + row * scale,
                                         scale, scale, color);
                }
            }
        }
        x += 6 * scale;
    }
}

static void drawInterface(Scene2D* scene, int selected, const std::string& status,
                          bool controllerReady) {
    const Color background{12, 18, 30};
    const Color panel{22, 32, 49};
    const Color panelAlt{28, 42, 62};
    const Color cyan{56, 218, 205};
    const Color white{235, 242, 250};
    const Color muted{139, 158, 180};
    const Color selectedText{8, 20, 30};

    scene->FrameBufferFill(background);
    scene->DrawRectangle(0, 0, kWidth, 132, panel);
    scene->DrawRectangle(0, 126, kWidth, 7, cyan);
    scene->DrawRectangle(110, 206, 1700, 2, panelAlt);

    drawText(scene, 112, 42, "PKGMASTER", cyan, 9);
    drawText(scene, 1160, 68, "PS4 DEVELOPMENT BUILD", muted, 3);
    drawText(scene, 116, 166, "OPENORBIS NATIVE UI FOUNDATION", white, 3);

    scene->DrawRectangle(110, 235, 1700, 620, panel);
    drawText(scene, 160, 270, "MAIN MENU", muted, 3);

    static const char* menuItems[] = {
        "PACKAGE LIBRARY",
        "DOWNLOAD QUEUE",
        "PHONE TRANSFER",
        "SETTINGS"
    };

    for (int i = 0; i < 4; ++i) {
        const int y = 330 + i * 118;
        const bool active = i == selected;
        scene->DrawRectangle(145, y, 1630, 94, active ? cyan : panelAlt);
        drawText(scene, 195, y + 27, menuItems[i],
                 active ? selectedText : white, 5);
        if (active) drawText(scene, 1570, y + 34, "SELECTED", selectedText, 2);
    }

    scene->DrawRectangle(110, 885, 1700, 105, panel);
    const std::string shortStatus = status.substr(0, 86);
    drawText(scene, 150, 908, shortStatus.c_str(), white, 3);
    drawText(scene, 110, 1015,
             controllerReady ? "D-PAD NAVIGATE   X SELECT   O BACK"
                              : "CONTROLLER NOT READY - CHECK USB OR BLUETOOTH",
             muted, 3);
    drawText(scene, 110, 1055, "USB PACKAGE SCAN AVAILABLE IN PACKAGE LIBRARY",
             muted, 2);
}

static void drawLibrary(Scene2D* scene, const std::vector<PackageEntry>& packages,
                        int selected, const std::string& status, bool controllerReady) {
    const Color background{12, 18, 30};
    const Color panel{22, 32, 49};
    const Color panelAlt{28, 42, 62};
    const Color cyan{56, 218, 205};
    const Color white{235, 242, 250};
    const Color muted{139, 158, 180};
    const Color selectedText{8, 20, 30};

    scene->FrameBufferFill(background);
    scene->DrawRectangle(0, 0, kWidth, 132, panel);
    scene->DrawRectangle(0, 126, kWidth, 7, cyan);
    drawText(scene, 112, 42, "PKGMASTER", cyan, 9);
    drawText(scene, 1160, 68, "USB PACKAGE LIBRARY", muted, 3);
    drawText(scene, 116, 166, "SCANNED ROOTS: USB0 USB1 USB2 USB3", white, 3);

    scene->DrawRectangle(110, 235, 1700, 620, panel);
    char countText[64];
    std::snprintf(countText, sizeof(countText), "FOUND %u PKG FILES",
                  static_cast<unsigned>(packages.size()));
    drawText(scene, 155, 270, countText, muted, 3);

    if (packages.empty()) {
        drawText(scene, 210, 460, "NO PKG FILES FOUND ON USB", white, 5);
        drawText(scene, 210, 535, "CONNECT A USB DRIVE AND PRESS TRIANGLE TO RESCAN",
                 muted, 3);
    } else {
        const int first = selected > 7 ? selected - 7 : 0;
        for (int row = 0; row < 8; ++row) {
            const int index = first + row;
            if (index >= static_cast<int>(packages.size())) break;

            const int y = 315 + row * 65;
            const bool active = index == selected;
            scene->DrawRectangle(145, y, 1630, 54, active ? cyan : panelAlt);

            std::string displayName = packages[index].name.substr(0, 40);
            drawText(scene, 175, y + 16, displayName.c_str(),
                     active ? selectedText : white, 3);

            char sizeText[32];
            if (packages[index].sizeBytes >= 1024ULL * 1024ULL) {
                std::snprintf(sizeText, sizeof(sizeText), "%llu MB",
                    static_cast<unsigned long long>(
                        packages[index].sizeBytes / (1024ULL * 1024ULL)));
            } else {
                std::snprintf(sizeText, sizeof(sizeText), "%llu KB",
                    static_cast<unsigned long long>(
                        packages[index].sizeBytes / 1024ULL));
            }
            drawText(scene, 1510, y + 19, sizeText,
                     active ? selectedText : muted, 2);
        }
    }

    scene->DrawRectangle(110, 885, 1700, 105, panel);
    const std::string shortStatus = status.substr(0, 86);
    drawText(scene, 150, 908, shortStatus.c_str(), white, 3);
    drawText(scene, 110, 1015,
             controllerReady ? "D-PAD SELECT   X MARK PKG   O BACK   TRIANGLE RESCAN"
                              : "CONTROLLER NOT READY",
             muted, 2);
    drawText(scene, 110, 1055, "INSTALL HANDOFF IS NOT ENABLED IN THIS BUILD",
             muted, 2);
}

static void drawDetails(Scene2D* scene, const PackageEntry& package,
                        const pkgmaster::PackageMetadata& metadata,
                        const std::string& error, const std::string& status,
                        bool controllerReady) {
    const Color background{12, 18, 30};
    const Color panel{22, 32, 49};
    const Color cyan{56, 218, 205};
    const Color white{235, 242, 250};
    const Color muted{139, 158, 180};

    scene->FrameBufferFill(background);
    scene->DrawRectangle(0, 0, kWidth, 132, panel);
    scene->DrawRectangle(0, 126, kWidth, 7, cyan);
    drawText(scene, 112, 42, "PKGMASTER", cyan, 9);
    drawText(scene, 1160, 68, "PACKAGE DETAILS", muted, 3);
    drawText(scene, 116, 166, "PARAM SFO INSPECTION", white, 3);

    scene->DrawRectangle(110, 235, 1700, 620, panel);
    drawText(scene, 155, 270, "FILE", muted, 3);
    const std::string filename = package.name.substr(0, 54);
    drawText(scene, 155, 315, filename.c_str(), white, 4);

    drawText(scene, 155, 405, "TITLE", muted, 3);
    const std::string title = metadata.title.empty() ? "UNKNOWN" : metadata.title;
    drawText(scene, 155, 445, title.substr(0, 54).c_str(), cyan, 4);

    drawText(scene, 155, 525, "TITLE ID", muted, 2);
    drawText(scene, 155, 558,
             metadata.title_id.empty() ? "UNKNOWN" : metadata.title_id.c_str(),
             white, 3);

    drawText(scene, 650, 525, "VERSION", muted, 2);
    drawText(scene, 650, 558,
             metadata.app_version.empty() ? "UNKNOWN" : metadata.app_version.c_str(),
             white, 3);

    drawText(scene, 1050, 525, "CATEGORY", muted, 2);
    drawText(scene, 1050, 558,
             metadata.category.empty() ? "UNKNOWN" : metadata.category.c_str(),
             white, 3);

    drawText(scene, 155, 650, "MINIMUM SYSTEM VERSION", muted, 2);
    drawText(scene, 155, 683,
             metadata.system_version.empty() ? "UNKNOWN" : metadata.system_version.c_str(),
             white, 3);

    if (!error.empty()) {
        drawText(scene, 155, 765, "METADATA ERROR", cyan, 2);
        drawText(scene, 155, 795, error.substr(0, 80).c_str(), white, 2);
    }

    scene->DrawRectangle(110, 885, 1700, 105, panel);
    const std::string shortStatus = status.substr(0, 86);
    drawText(scene, 150, 908, shortStatus.c_str(), white, 3);
    drawText(scene, 110, 1015,
             controllerReady ? "CIRCLE BACK TO USB PACKAGE LIST"
                              : "CONTROLLER NOT READY",
             muted, 3);
    drawText(scene, 110, 1055, "PACKAGE INSTALLER HANDOFF IS NOT ENABLED",
             muted, 2);
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    DEBUGLOG << "Starting PKGMaster native UI";

    // The upstream sample declares a destructor but does not provide its body.
    // Keep the graphics context alive for the full application process.
    Scene2D* scene = new Scene2D(kWidth, kHeight, kDepth);
    if (!scene->Init(0xC000000, 2)) {
        DEBUGLOG << "Video output initialization failed";
        for (;;) sceKernelUsleep(1000000);
    }
    scene->SetActiveFrameBuffer(0);

    bool controllerReady = false;
    int padHandle = -1;
    int previousButtons = 0;

    if (scePadInit() == 0) {
        OrbisUserServiceInitializeParams userParams{};
        userParams.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
        sceUserServiceInitialize(&userParams);

        int userId = -1;
        if (sceUserServiceGetInitialUser(&userId) == 0) {
            padHandle = scePadOpen(userId, 0, 0, nullptr);
            controllerReady = padHandle >= 0;
        }
    }

    static const char* menuItems[] = {
        "PACKAGE LIBRARY", "DOWNLOAD QUEUE", "PHONE TRANSFER", "SETTINGS"
    };
    int page = 0;
    int selected = 0;
    int packageSelected = 0;
    std::vector<PackageEntry> packages;
    pkgmaster::PackageMetadata selectedMetadata;
    std::string metadataError;
    std::string status = "BOOTED - SELECT PACKAGE LIBRARY TO SCAN USB STORAGE";
    int frameId = 0;

    for (;;) {
        if (controllerReady) {
            OrbisPadData padData{};
            if (scePadReadState(padHandle, &padData) >= 0) {
                const int pressed = padData.buttons & ~previousButtons;
                previousButtons = padData.buttons;

                if (pressed & ORBIS_PAD_BUTTON_UP) {
                    if (page == 0) selected = (selected + 3) % 4;
                    else if (page == 1 && !packages.empty()) packageSelected =
                        (packageSelected + static_cast<int>(packages.size()) - 1) %
                        static_cast<int>(packages.size());
                }
                if (pressed & ORBIS_PAD_BUTTON_DOWN) {
                    if (page == 0) selected = (selected + 1) % 4;
                    else if (page == 1 && !packages.empty()) packageSelected =
                        (packageSelected + 1) % static_cast<int>(packages.size());
                }
                if (pressed & ORBIS_PAD_BUTTON_CROSS) {
                    if (page == 0 && selected == 0) {
                        packages = scanUsbPackages();
                        packageSelected = 0;
                        page = 1;
                        status = "SCAN COMPLETE - " + std::to_string(packages.size()) +
                                 " PKG FILES FOUND";
                    } else if (page == 0) {
                        status = std::string(menuItems[selected]) +
                                 " - MODULE NOT CONNECTED YET";
                    } else if (page == 1 && !packages.empty()) {
                        metadataError.clear();
                        selectedMetadata = pkgmaster::PackageMetadata{};
                        if (inspect_pkg_param_sfo(packages[packageSelected].path,
                                                  selectedMetadata, metadataError)) {
                            status = "PACKAGE METADATA READ SUCCESSFULLY";
                        } else {
                            status = "PACKAGE METADATA COULD NOT BE READ";
                        }
                        page = 2;
                    }
                }
                if (pressed & ORBIS_PAD_BUTTON_CIRCLE) {
                    if (page == 2) {
                        page = 1;
                        status = "RETURNED TO USB PACKAGE LIST";
                    } else if (page == 1) {
                        page = 0;
                        status = "RETURNED TO MAIN MENU";
                    } else {
                        status = "PKGMASTER HOME - SELECT A SECTION";
                    }
                }
                if (pressed & ORBIS_PAD_BUTTON_TRIANGLE && page == 1) {
                    packages = scanUsbPackages();
                    packageSelected = 0;
                    status = "RESCAN COMPLETE - " + std::to_string(packages.size()) +
                             " PKG FILES FOUND";
                }
            }
        }

        if (page == 0) {
            drawInterface(scene, selected, status, controllerReady);
        } else if (page == 1) {
            drawLibrary(scene, packages, packageSelected, status, controllerReady);
        } else if (page == 2 && !packages.empty()) {
            drawDetails(scene, packages[packageSelected], selectedMetadata,
                        metadataError, status, controllerReady);
        }

        scene->SubmitFlip(frameId);
        scene->FrameWait(frameId);
        scene->FrameBufferSwap();
        ++frameId;
    }

    return 0;
}
