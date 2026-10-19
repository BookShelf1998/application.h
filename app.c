#include <stdio.h>

#define APPLICATION_IMPLEMENTATION
#include "application.h"

void DrawBox(int x, int y, int w, int h)
{
    int rx = x*GrFontSize();
    int ry = y*GrFontSize();
    int rw = w*GrFontSize();
    int rh = h*GrFontSize();
    
    for (int i = 0; i < w; ++i) {
        GrChar(0xC4, rx + i * GrFontSize(), ry); // top
        GrChar(0xC4, rx + i * GrFontSize(), ry + h * GrFontSize()); // bottom
    }

    for (int i = 0; i < h; ++i) {
        GrChar(0xb3, rx, ry + i * GrFontSize());
        GrChar(0xb3, rx + rw, ry + i * GrFontSize());
    }

    GrChar(0xDA, rx, ry);
    GrChar(0xD9, rx+rw, ry+rh);
    GrChar(0xC0, rx, ry+rh);
    GrChar(0xBF, rx+rw, ry);
}

int main()
{
    bool show_name = false;
    bool show_scale = true;
    unsigned char c = 0;
    const char mesg[] = "Hello, My name is Marcus!";
    const char diag[] = "<Hide name>";
    
    InitApp(640, 480, "Hello, World!");
    
    while (!AppShouldClose()) {
        AppEvent e = {0};
        while (GetAppEvent(&e)) {
            if (e.type == APP_EVENT_KEY_PRESSED) {
                if (e.key.code == KEY_ESC) goto end;
                if (e.key.code == KEY_Q && e.key.mod == (KEY_MOD_CTRL | KEY_MOD_SHIFT | KEY_MOD_ALT)) {
                    goto end;
                }
                if (e.key.code == KEY_I && e.key.mod == KEY_MOD_CTRL) GrSetFontScale(GrGetFontScale() + 1);
                if (e.key.code == KEY_J && e.key.mod == KEY_MOD_CTRL) GrSetFontScale(GrGetFontScale() - 1);
                
                if (e.key.code == KEY_LEFT && e.key.mod == 0) --c;
                if (e.key.code == KEY_RIGHT && e.key.mod == 0) ++c;
                
                if (e.key.code == KEY_P) show_scale = !show_scale;
            }
            if (e.type == APP_EVENT_CLOSE) {
                printf("Window is closing!\n");
            }
            if (e.type == APP_EVENT_MOUSE_MOVED) {
                // if (e.mouse.x < 100 && e.mouse.y < 100 && e.key.mod == KEY_MOD_CTRL) goto end;`
            }
            if (e.type == APP_EVENT_MOUSE_BUTTON_PRESSED) {
                if (e.mouse.but == MOUSE_BUTTON_LEFT) {
                    if (e.mouse.x < (sizeof(diag) - 1) * 8 * GrGetFontScale() && e.mouse.y < 8 * GrGetFontScale()) show_name = !show_name;
                }
            }
        }

        int w, h;
        GetAppWindowResolution(&w, &h);

        if (show_name) {
            GrText(mesg, 0, 8*GrGetFontScale());
            GrText("<Hide name>", 0, 0);
        } else {
            GrText("<Show name>", 0, 0);
        }

        if (show_scale) {
            char buf[128] = "";
            snprintf(buf, sizeof(buf) - 1, "Scale: %d, Char: 0x%X", GrGetFontScale(), c);

            GrText(buf, 0, h - GrFontSize());
        }


        DrawBox(4, 5, 4, 5);
        

        GrSwapBuffers();
    }
end:
    CloseApp();
    return 0;
}
