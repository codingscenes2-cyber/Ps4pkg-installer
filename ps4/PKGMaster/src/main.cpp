#include <cstdio>
#include <cstdint>
#include <sstream>
#include <string>

#include <orbis/Pad.h>
#include <orbis/UserService.h>

#include "graphics.h"
#include "log.h"

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
    {' ',{0,0,0,0,0,0,0}}
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
    drawText(scene, 1160, 68, "PS4 BOOT TEST BUILD", muted, 3);
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
        if (active) {
            drawText(scene, 1570, y + 34, "SELECTED", selectedText, 2);
        }
    }

    scene->DrawRectangle(110, 885, 1700, 105, panel);
    drawText(scene, 150, 908, status.c_str(), white, 3);
    drawText(scene, 110, 1015,
             controllerReady ? "D-PAD NAVIGATE   X SELECT   O BACK"
                              : "CONTROLLER NOT READY - CHECK USB OR BLUETOOTH",
             muted, 3);
    drawText(scene, 110, 1055, "NATIVE INSTALLER AND STORAGE FEATURES NOT ENABLED",
             muted, 2);
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    DEBUGLOG << "Starting PKGMaster native UI";

    Scene2D scene(kWidth, kHeight, kDepth);
    if (!scene.Init(0xC000000, 2)) {
        DEBUGLOG << "Video output initialization failed";
        for (;;) sceKernelUsleep(1000000);
    }
    scene.SetActiveFrameBuffer(0);

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
    int selected = 0;
    std::string status = "BOOTED - SELECT A SECTION TO VIEW ITS CURRENT STATUS";
    int frameId = 0;

    for (;;) {
        if (controllerReady) {
            OrbisPadData padData{};
            if (scePadReadState(padHandle, &padData) >= 0) {
                const int pressed = padData.buttons & ~previousButtons;
                previousButtons = padData.buttons;

                if (pressed & ORBIS_PAD_BUTTON_UP) {
                    selected = (selected + 3) % 4;
                }
                if (pressed & ORBIS_PAD_BUTTON_DOWN) {
                    selected = (selected + 1) % 4;
                }
                if (pressed & ORBIS_PAD_BUTTON_CROSS) {
                    status = std::string(menuItems[selected]) + " - MODULE NOT CONNECTED YET";
                }
                if (pressed & ORBIS_PAD_BUTTON_CIRCLE) {
                    status = "PKGMASTER HOME - SELECT A SECTION";
                }
            }
        }

        drawInterface(&scene, selected, status, controllerReady);
        scene.SubmitFlip(frameId);
        scene.FrameWait(frameId);
        scene.FrameBufferSwap();
        ++frameId;
    }

    return 0;
}
