#ifndef APPLICATION_H
#define APPLICATION_H

#include <stdbool.h>

void InitApp(unsigned int width, unsigned int height, const char *title);

bool AppShouldClose(void);

typedef enum {
    KEY_NONE = 0,
    
    // Letters
    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E,
    KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,
    KEY_K, KEY_L, KEY_M, KEY_N, KEY_O,
    KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T,
    KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y,
    KEY_Z,
    
    // Numbers
    KEY_0, KEY_1, KEY_2, KEY_3, KEY_4,
    KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
    
    // Special Keys
    KEY_SPACE,
    KEY_ENTER,
    KEY_ESC,
    KEY_SHIFT,
    KEY_CTRL,
    KEY_ALT,
    
    // Arrow Keys
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT
} KeyboardKey;

typedef enum {
    MOUSE_BUTTON_LEFT,
    MOUSE_BUTTON_RIGHT,
    MOUSE_BUTTON_MIDDLE
} MouseButton;

typedef enum {
    APP_EVENT_KEY_PRESSED,
    APP_EVENT_KEY_RELEASED,
    APP_EVENT_CHARACTER,
    APP_EVENT_MOUSE_BUTTON_PRESSED,
    APP_EVENT_MOUSE_BUTTON_RELEASED,
    APP_EVENT_MOUSE_MOVED,
    APP_EVENT_CLOSE
} AppEventType;

typedef enum {
    KEY_MOD_CTRL     = 1 << 0,
    KEY_MOD_ALT      = 1 << 1,
    KEY_MOD_SHIFT    = 1 << 2,
    KEY_MOD_SUPER    = 1 << 3,
    KEY_MOD_CAPSLOCK = 1 << 4
} KeyModifier;
    
typedef struct {
    AppEventType type;
    KeyModifier mod;
    KeyboardKey code;
} KeyEvent;

typedef struct {
    AppEventType type;
    unsigned int value;
} CharEvent;

typedef struct {
    AppEventType type;
    KeyModifier mod;
    MouseButton but;
    unsigned int x, y;
} MouseEvent;

typedef union {
    AppEventType type;
    KeyEvent key;
    CharEvent chr;
    MouseEvent mouse;
} AppEvent;
    
bool GetAppEvent(AppEvent *event);

void GetAppWindowResolution(int *w, int *h);

void CloseApp(void);

// Graphics related functions

void GrSwapBuffers(void);
void GrSetFontScale(unsigned int scale);
unsigned int GrGetFontScale(void);
unsigned int GrFontSize(void);
void GrText(const char *str, int x, int y);
void GrChar(unsigned char c, int x, int y);

#ifdef APPLICATION_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>

static KeyboardKey keysym_to_app_key[] = {
    [XK_a] = KEY_A, [XK_b] = KEY_B, [XK_c] = KEY_C, [XK_d] = KEY_D, [XK_e] = KEY_E,
    [XK_f] = KEY_F, [XK_g] = KEY_G, [XK_h] = KEY_H, [XK_i] = KEY_I, [XK_j] = KEY_J,
    [XK_k] = KEY_K, [XK_l] = KEY_L, [XK_m] = KEY_M, [XK_n] = KEY_N, [XK_o] = KEY_O,
    [XK_p] = KEY_P, [XK_q] = KEY_Q, [XK_r] = KEY_R, [XK_s] = KEY_S, [XK_t] = KEY_T,
    [XK_u] = KEY_U, [XK_v] = KEY_V, [XK_w] = KEY_W, [XK_x] = KEY_X, [XK_y] = KEY_Y,
    [XK_z] = KEY_Z,
    
    // Numbers
    [XK_0] = KEY_0, [XK_1] = KEY_1, [XK_2] = KEY_2, [XK_3] = KEY_3, [XK_4] = KEY_4,
    [XK_5] = KEY_5, [XK_6] = KEY_6, [XK_7] = KEY_7, [XK_8] = KEY_8, [XK_9] = KEY_9,
    
    [XK_space]  = KEY_SPACE,
    [XK_Return]  = KEY_ENTER,
    [XK_Escape] = KEY_ESC,
    [XK_Up]     = KEY_UP,
    [XK_Down]   = KEY_DOWN,
    [XK_Left]   = KEY_LEFT,
    [XK_Right]  = KEY_RIGHT
};

static MouseButton button_to_app_button[] = {
    [Button1] = MOUSE_BUTTON_LEFT,
    [Button3] = MOUSE_BUTTON_RIGHT,
    [Button2] = MOUSE_BUTTON_MIDDLE
};
    

static Display *display       = NULL;
static Window window          = {0};
static Atom wm_delete_message = 0;
static GLXContext glctx       = NULL;
static bool should_close      = false;
static int cur_width, cur_height;

typedef struct {
    AppEvent *items;
    size_t count;
    size_t capacity;
    size_t head;
    size_t tail;
} EventQueue;

static EventQueue internal_queue = {0};

#define QEnqueue(q, item)                            \
    do {                                             \
        if ((q)->count + 1 > (q)->capacity) break;   \
        (q)->items[(q)->tail] = item;                \
        (q)->tail = ((q)->tail + 1) % (q)->capacity; \
        (q)->count++;                                \
    } while (0)

#define INITIAL_QUEUE_SIZE 32

// OpenGL Function Ptrs

typedef GLXContext(*glXCreateContextAttribsARBPfn)(
                    Display *dpy, GLXFBConfig config,
                    GLXContext share_contex, Bool direct,
                    const int *attrib_list);
static glXCreateContextAttribsARBPfn glXCreateContextAttribsARB = 0;

typedef void (*glXSwapIntervalEXTPfn)(Display *dpy, GLXDrawable drawable, int interval);
static glXSwapIntervalEXTPfn glXSwapIntervalEXT = 0;

typedef void (*glGenBuffersPfn)(GLsizei n, GLuint *buffers);
static glGenBuffersPfn glGenBuffers = 0;

typedef void (*glDeleteBuffersPfn)(GLsizei n, GLuint *buffers);
static glDeleteBuffersPfn glDeleteBuffers = 0; 

typedef void (*glBufferDataPfn)(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
static glBufferDataPfn glBufferData = 0;

typedef void (*glBufferSubDataPfn)(GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
static glBufferSubDataPfn glBufferSubData = 0;

typedef void (*glBindBufferPfn)(GLenum target, GLuint buffer);
static glBindBufferPfn glBindBuffer = 0;

typedef void (*glBindBufferBasePfn)(GLenum target, GLuint index, GLuint buffer);
static glBindBufferBasePfn glBindBufferBase = 0;

typedef void (*glDrawArraysInstancedPfn)(GLenum mode, GLint first, GLsizei count, GLsizei instancecount);
static glDrawArraysInstancedPfn glDrawArraysInstanced = 0;

typedef GLuint (*glCreateShaderPfn)(GLenum shaderType);
static glCreateShaderPfn glCreateShader = 0;

typedef void (*glShaderSourcePfn)(GLuint shader, GLsizei count, const GLchar **string, const GLint *length);
static glShaderSourcePfn glShaderSource = 0;

typedef void (*glGetShaderivPfn)(GLuint shader, GLenum pname, GLint *params);
static glGetShaderivPfn glGetShaderiv = 0;

typedef void (*glGetShaderInfoLogPfn)(GLuint shader, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
static glGetShaderInfoLogPfn glGetShaderInfoLog = 0;

typedef void (*glCompileShaderPfn)(GLuint shader);
static glCompileShaderPfn glCompileShader = 0;

typedef void (*glDeleteShaderPfn)(GLuint shader);
static glDeleteShaderPfn glDeleteShader = 0;

typedef GLuint (*glCreateProgramPfn)(void);
static glCreateProgramPfn glCreateProgram = 0;

typedef void (*glAttachShaderPfn)(GLuint program, GLuint shader);
static glAttachShaderPfn glAttachShader = 0;

typedef void (*glLinkProgramPfn)(GLuint program);
static glLinkProgramPfn glLinkProgram = 0;

typedef void (*glGetProgramivPfn)(GLuint program, GLenum pname, GLint *params);
static glGetProgramivPfn glGetProgramiv = 0;

typedef void (*glGetProgramInfoLogPfn)(GLuint shader, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
static glGetProgramInfoLogPfn glGetProgramInfoLog = 0;

typedef void (*glUseProgramPfn)(GLuint program);
static glUseProgramPfn glUseProgram = 0;

typedef void (*glDeleteProgramPfn)(GLuint program);
static glDeleteProgramPfn glDeleteProgram = 0;

typedef GLint (*glGetUniformLocationPfn)(GLuint program, const GLchar *name);
static glGetUniformLocationPfn glGetUniformLocation = 0;

typedef void (*glTexStorage2DPfn)(GLenum target,
                                  GLsizei levels,
                                  GLenum internalformat,
                                  GLsizei width, GLsizei height);
static glTexStorage2DPfn glTexStorage2D = 0;

typedef void (*glDebugMessageCallbackPfn)(GLDEBUGPROC callback, const void * userParam);
static  glDebugMessageCallbackPfn glDebugMessageCallback = 0;

typedef void (*glGenVertexArraysPfn)(GLsizei n, GLuint *arrays);
static glGenVertexArraysPfn glGenVertexArrays = 0;

typedef void (*glBindVertexArrayPfn)(GLuint array);
static glBindVertexArrayPfn glBindVertexArray = 0;

typedef void (*glUniform1fPfn)(GLint location, GLfloat v0);
static glUniform1fPfn glUniform1f = 0;

typedef void (*glUniform1iPfn)(GLint location, GLint v0);
static glUniform1iPfn glUniform1i = 0;

typedef void (*glUniform2iPfn)(GLint location, GLint v0, GLint v1);
static glUniform2iPfn glUniform2i = 0;

// Text drawing related variables and structs
typedef struct {
    int x, y;
    int character;
    char _pad[4];
} CharacterData;

typedef struct {
    CharacterData *items;
    size_t count;
    size_t capacity;
} CharacterDatas;

static CharacterDatas char_buf = {0};

static unsigned int text_shader_program = 0;
static unsigned int char_ssbo           = 0;
static int num_rows_loc        = 0;
static int atlas_width_loc     = 0;
static int aspect_loc          = 0;
static int res_loc             = 0;
static int scale_loc           = 0;
static int texture0_loc        = 0;

static unsigned int glyf_tex = 0;
static unsigned int default_vao = 0;

#define INITIAL_CHAR_BUF_LEN 32
#define GLYF_WIDTH 8
#define GLYF_HEIGHT GLYF_WIDTH
#define DEFAULT_SCALE_FACTOR 2

static unsigned int font_scale = DEFAULT_SCALE_FACTOR;

static void LoadGLPfns(void);
static void GrInit(void);
static void GrShutdown(void);

void InitApp(unsigned int width, unsigned int height, const char *title)
{   
    LoadGLPfns();
    
    int fbc_attribs[] = {
        GLX_BUFFER_SIZE, 32,
        GLX_DOUBLEBUFFER, True,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        GLX_DEPTH_SIZE, 24,
        GLX_STENCIL_SIZE, 8,
        
        GLX_SAMPLE_BUFFERS, 0,
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_X_RENDERABLE, True,
        None
    };

    display = XOpenDisplay(NULL);
    
    int fbc_len = 0;
    GLXFBConfig *fbc = glXChooseFBConfig(display, DefaultScreen(display), fbc_attribs, &fbc_len);

    printf("fbc_len = %d\n", fbc_len);
    GLXFBConfig selected_fbc = fbc[0];

    XFree(fbc);

    internal_queue.items = malloc(sizeof(AppEvent) * INITIAL_QUEUE_SIZE);
    internal_queue.capacity = INITIAL_QUEUE_SIZE;
    
    int window_x = 0;
    int window_y = 0;
    int border_width = 0;
    XVisualInfo *vi = glXGetVisualFromFBConfig(display, selected_fbc);
    int attrib_mask = CWBackPixel | CWEventMask;
    
    XSetWindowAttributes swa = {0};
    swa.event_mask = StructureNotifyMask | KeyPressMask | KeyReleaseMask
                     | ExposureMask | PointerMotionMask | ButtonPressMask
                     | ButtonReleaseMask;

    window = XCreateWindow(display, XDefaultRootWindow(display), 
            window_x, window_y, width, height,
            border_width, vi->depth, InputOutput, vi->visual,
            attrib_mask, &swa);

    XMapWindow(display, window);
    XStoreName(display, window, title);

    int context_attribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
        GLX_CONTEXT_MINOR_VERSION_ARB, 3,
        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_DEBUG_BIT_ARB,
        None
    };
    
    glctx = glXCreateContextAttribsARB(display, selected_fbc, NULL, True, context_attribs);
    
    glXMakeCurrent(display, window, glctx);
    printf("Renderer: %s\n", glGetString(GL_RENDERER));
    printf("Vendor: %s\n",   glGetString(GL_VENDOR));
    printf("Version: %s\n",  glGetString(GL_VERSION));

    glXSwapIntervalEXT(display, window, 0);
    
    wm_delete_message = XInternAtom(display, "WM_DELETE_WINDOW", True);
    XSetWMProtocols(display, window, &wm_delete_message, 1);

    XWindowAttributes attrs;
    XGetWindowAttributes(display, window, &attrs);
    cur_width = attrs.width;
    cur_height = attrs.height;
    
    GrInit();
}

static KeyModifier StateToKeyModifier(unsigned int state)
{
    KeyModifier mod = 0;
    mod = (state & ControlMask) ? mod | KEY_MOD_CTRL     : mod;
    mod = (state & ShiftMask)   ? mod | KEY_MOD_SHIFT    : mod;
    mod = (state & LockMask)    ? mod | KEY_MOD_CAPSLOCK : mod;
    mod = (state & Mod1Mask)    ? mod | KEY_MOD_ALT      : mod;
    return mod;
}

bool GetAppEvent(AppEvent *event)
{
    while (XPending(display) > 0 && internal_queue.count < internal_queue.capacity) {
        XEvent e;
        AppEvent ae;
        int state;
        
        XNextEvent(display, &e);
        
        switch (e.type) {
        case KeyRelease:
        case KeyPress:
            if (e.type == KeyRelease) ae.type = APP_EVENT_KEY_RELEASED;
            if (e.type == KeyPress) ae.type = APP_EVENT_KEY_PRESSED;

            KeySym keysym = XLookupKeysym(&e.xkey, 0);
            ae.key.code = keysym_to_app_key[keysym];
            state = e.xkey.state;

            ae.key.mod = StateToKeyModifier(state);
            
            if (!(IsModifierKey(keysym) || IsCursorKey(keysym) || IsFunctionKey(keysym) || IsPFKey(keysym) || keysym == XK_Escape)) {
                char buf[5];
                int len = XLookupString(&e.xkey, buf, sizeof(buf), NULL, NULL);
                
                if (len <= 4 && !iscntrl(buf[0])) {
                    AppEvent char_event = {0};
                    char_event.type = APP_EVENT_CHARACTER;
                    char_event.chr.value = *(int *)buf;

                    QEnqueue(&internal_queue, char_event);
                }
            }

            QEnqueue(&internal_queue, ae);
            break;
        case ButtonPress:
        case ButtonRelease:
            if (e.type == ButtonRelease) ae.type = APP_EVENT_MOUSE_BUTTON_RELEASED;
            if (e.type == ButtonPress) ae.type = APP_EVENT_MOUSE_BUTTON_PRESSED;
            state = e.xbutton.state;

            ae.mouse.mod = StateToKeyModifier(state);
            ae.mouse.x = e.xbutton.x;
            ae.mouse.y = e.xbutton.y;
            
            ae.mouse.but = button_to_app_button[e.xbutton.button];
            QEnqueue(&internal_queue, ae);
            break;
        case ClientMessage:
            if ((Atom) e.xclient.data.l[0] == wm_delete_message) {
                ae.type = APP_EVENT_CLOSE;
                QEnqueue(&internal_queue, ae);
                should_close = true;
            }
            break;
        case MotionNotify:
            state = e.xmotion.state;
            ae.type = APP_EVENT_MOUSE_MOVED;
            ae.mouse.mod = StateToKeyModifier(state);
            
            ae.mouse.x = e.xmotion.x;
            ae.mouse.y = e.xmotion.y;

            QEnqueue(&internal_queue, ae);
            break;
        case ConfigureNotify: {
            XConfigureEvent xce = e.xconfigure;
            
            // Check if the size actually changed to avoid redundant calculations
            if (xce.width != cur_width || xce.height != cur_height) {
                cur_width = xce.width;
                cur_height = xce.height;
                
                glViewport(0, 0, cur_width, cur_height);
                glUseProgram(text_shader_program);
                glUniform2i(res_loc, cur_width, cur_height);
                glUseProgram(0);
            }
        } break;
        default:
            break;
        };
    }

    if (internal_queue.count == 0) return false;
    
    AppEvent ae = internal_queue.items[internal_queue.head];
    internal_queue.head = (internal_queue.head + 1) % internal_queue.capacity;
    internal_queue.count--;
    *event = ae;
    
    return true;
}

bool AppShouldClose(void) { return should_close; }

void GetAppWindowResolution(int *w, int *h)
{
    *w = cur_width;
    *h = cur_height;
}

void CloseApp(void)
{
    GrShutdown();
    glXDestroyContext(display, glctx);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
}

//#include "font.c"

unsigned long long sys_font_std[256]= {
    0x0000000000000000,0x0000000000000000,
    0x000000FF00000000,0x000000FF00FF0000,//..
    0x1818181818181818,0x6C6C6C6C6C6C6C6C,//..
    0x181818F800000000,0x6C6C6CEC0CFC0000,//..
    0x1818181F00000000,0x6C6C6C6F607F0000,//..
    0x000000F818181818,0x000000FC0CEC6C6C,//..
    0x0000001F18181818,0x0000007F606F6C6C,//..
    0x0000000000000000,0x0000000000000000,
    0x0000000000000000,0x0000000000000000,
    0x0000000000000000,0x0000000000000000,
    0x0000000000000000,0x0000000000000000,
    0x0000000000000000,0x0000000000000000,
    0x0000000000000000,0x0000000000000000,
    0x0000000000000000,0x0000000000000000,
    0x0000000000000000,0x0000000000000000,
    0x0000000000000000,0x0008000000000000,//
    0x0000000000000000,0x00180018183C3C18,// !
    0x0000000000363636,0x006C6CFE6CFE6C6C,//"#
    0x00187ED07C16FC30,0x0060660C18306606,//$%
    0x00DC66B61C36361C,0x0000000000181818,//&'
    0x0030180C0C0C1830,0x000C18303030180C,//()
    0x0000187E3C7E1800,0x000018187E181800,//*+
    0x0C18180000000000,0x000000007E000000,//,-
    0x0018180000000000,0x0000060C18306000,//./
    0x003C666E7E76663C,0x007E181818181C18,//01
    0x007E0C183060663C,0x003C66603860663C,//23
    0x0030307E363C3830,0x003C6660603E067E,//45
    0x003C66663E060C38,0x000C0C0C1830607E,//67
    0x003C66663C66663C,0x001C30607C66663C,//89
    0x0018180018180000,0x0C18180018180000,//:;
    0x0030180C060C1830,0x0000007E007E0000,//<=
    0x000C18306030180C,0x001800181830663C,//>?
    0x003C06765676663C,0x006666667E66663C,//@A
    0x003E66663E66663E,0x003C66060606663C,//BC
    0x001E36666666361E,0x007E06063E06067E,//DE
    0x000606063E06067E,0x003C66667606663C,//FG
    0x006666667E666666,0x007E18181818187E,//HI
    0x001C36303030307C,0x0066361E0E1E3666,//JK
    0x007E060606060606,0x00C6C6D6D6FEEEC6,//LM
    0x006666767E6E6666,0x003C66666666663C,//NO
    0x000606063E66663E,0x006C36566666663C,//PQ
    0x006666363E66663E,0x003C66603C06663C,//RS
    0x001818181818187E,0x003C666666666666,//TU
    0x00183C6666666666,0x00C6EEFED6D6C6C6,//VW
    0x0066663C183C6666,0x001818183C666666,//XY
    0x007E060C1830607E,0x003E06060606063E,//Z[
    0x00006030180C0600,0x007C60606060607C,//\]
    0x000000000000663C,0xFFFF000000000000,//^_
    0x000000000030180C,0x007C667C603C0000,//`a
    0x003E6666663E0606,0x003C6606663C0000,//bc
    0x007C6666667C6060,0x003C067E663C0000,//de
    0x000C0C0C3E0C0C38,0x3C607C66667C0000,//fg
    0x00666666663E0606,0x003C1818181C0018,//hi
    0x0E181818181C0018,0x0066361E36660606,//jk
    0x003C18181818181C,0x00C6D6D6FE6C0000,//lm
    0x00666666663E0000,0x003C6666663C0000,//no
    0x06063E66663E0000,0xE0607C66667C0000,//pq
    0x000606066E360000,0x003E603C067C0000,//rs
    0x00380C0C0C3E0C0C,0x007C666666660000,//tu
    0x00183C6666660000,0x006CFED6D6C60000,//vw
    0x00663C183C660000,0x3C607C6666660000,//xy
    0x007E0C18307E0000,0x003018180E181830,//z{
        0x0018181818181818,0x000C18187018180C,//|}
        0x000000000062D68C,0xFFFFFFFFFFFFFFFF,//~
        0x1E30181E3303331E,0x007E333333003300,//..
        0x001E033F331E0038,0x00FC667C603CC37E,//..
        0x007E333E301E0033,0x007E333E301E0007,//..
        0x007E333E301E0C0C,0x3C603E03033E0000,//..
        0x003C067E663CC37E,0x001E033F331E0033,//..
        0x001E033F331E0007,0x001E0C0C0C0E0033,//..
        0x003C1818181C633E,0x001E0C0C0C0E0007,//..
        0x00333F33331E0C33,0x00333F331E000C0C,//..
        0x003F061E063F0038,0x00FE33FE30FE0000,//..
        0x007333337F33367C,0x001E33331E00331E,//..
        0x001E33331E003300,0x001E33331E000700,//..
        0x007E33333300331E,0x007E333333000700,//..
        0x1F303F3333003300,0x001C3E63633E1C63,//..
        0x001E333333330033,0x18187E03037E1818,//..
        0x003F67060F26361C,0x000C3F0C3F1E3333,//..
        0x70337B332F1B1B0F,0x0E1B18187E18D870,//..
        0x007E333E301E0038,0x001E0C0C0C0E001C,//..
        0x001E33331E003800,0x007E333333003800,//..
        0x003333331F001F00,0x00333B3F3733003F,//..
        0x00007E007C36363C,0x00007E003C66663C,//..
        0x001E3303060C000C,0x000003033F000000,//..
        0x000030303F000000,0xF81973C67C1B3363,//..
        0xC0F9F3E6CF1B3363,0x183C3C1818001800,//..
        0x0000CC663366CC00,0x00003366CC663300,//..
        0x1144114411441144,0x55AA55AA55AA55AA,//..
        0xEEBBEEBBEEBBEEBB,0x1818181818181818,//..
        0x1818181F18181818,0x1818181F181F1818,//..
        0x6C6C6C6F6C6C6C6C,0x6C6C6C7F00000000,//..
        0x1818181F181F0000,0x6C6C6C6F606F6C6C,//..
        0x6C6C6C6C6C6C6C6C,0x6C6C6C6F607F0000,//..
        0x0000007F606F6C6C,0x0000007F6C6C6C6C,//..
        0x0000001F181F1818,0x1818181F00000000,//..
        0x000000F818181818,0x000000FF18181818,//..
        0x181818FF00000000,0x181818F818181818,//..
        0x000000FF00000000,0x181818FF18181818,//..
        0x181818F818F81818,0x6C6C6CEC6C6C6C6C,//..
        0x000000FC0CEC6C6C,0x6C6C6CEC0CFC0000,//..
        0x000000FF00EF6C6C,0x6C6C6CEF00FF0000,//..
        0x6C6C6CEC0CEC6C6C,0x000000FF00FF0000,//..
        0x6C6C6CEF00EF6C6C,0x000000FF00FF1818,//..
        0x000000FF6C6C6C6C,0x181818FF00FF0000,//..
        0x6C6C6CFF00000000,0x000000FC6C6C6C6C,//..
        0x000000F818F81818,0x181818F818F80000,//..
        0x6C6C6CFC00000000,0x6C6C6CEF6C6C6C6C,//..
        0x181818FF00FF1818,0x0000001F18181818,//..
        0x181818F800000000,0xFFFFFFFFFFFFFFFF,//..
        0xFFFFFFFF00000000,0x0F0F0F0F0F0F0F0F,//..
        0xF0F0F0F0F0F0F0F0,0x00000000FFFFFFFF,//..
        0x006E3B133B6E0000,0x03031F331F331E00,//..
        0x0003030303637F00,0x0036363636367F00,//.pi
        0x007F660C180C667F,0x001E3333337E0000,//..
        0x03063E6666666600,0x00181818183B6E00,//u.
        0x3F0C1E33331E0C3F,0x001C36637F63361C,//phitheta
        0x007736366363361C,0x001E33333E180C38,//omega.
        0x00007EDBDB7E0000,0x03067EDBDB7E3060,//inf.
        0x003C06033F03063C,0x003333333333331E,//..
        0x00003F003F003F00,0x003F000C0C3F0C0C,//..
        0x003F00060C180C06,0x003F00180C060C18,//..
        0x1818181818D8D870,0x0E1B1B1818181818,//..
        0x000C0C003F000C0C,0x0000394E00394E00,//..
        0x000000001C36361C,0x0000001818000000,//..
        0x0000001800000000,0x383C3637303030F0,//..
        0x000000363636361E,0x0000003E061C301E,//..
        0x00003C3C3C3C0000,0xFFFFFFFFFFFFFFFF,//..
};

static const char vs_code[] = {
"#version 430 core\n"

"int idx[6] = int[6](\n"
"    2, 1, 0,\n"
"    0, 3, 2\n"
");\n"
"\n"
"vec2 p[4] = vec2[4](\n"
"     vec2(1, 0),\n"
"     vec2(1, 1),\n"
"     vec2(0, 1),\n"
"     vec2(0, 0)\n"
");\n"
"\n"
"vec2 tc[4] = vec2[4](\n"
"     vec2(1, 1),\n"
"     vec2(1, 0),\n"
"     vec2(0, 0),\n"
"     vec2(0, 1)\n"
");\n"
"\n"
"uniform int num_rows;\n"
"uniform int atlas_width;\n"
"uniform int scale;\n"
"\n"
"uniform ivec2 res;\n"
"\n"
"struct CharData {\n"
"    ivec2 pos;\n"
"    int character;\n"
"};\n"
"layout(std430, binding = 0) readonly buffer TextData {\n"
"    CharData characters[];\n"
"};\n"
"\n"
"out vec2 fragTexCoord;\n"
"\n"
"void main()\n"
"{\n"
"    CharData cdata = characters[gl_InstanceID];\n"
"    int c = cdata.character;\n"
"    int x = c % num_rows;\n"
"    int y = c / num_rows;\n"
"    y = num_rows - y;\n"
"\n"
"    vec2 uv_base = tc[idx[gl_VertexID]];\n"
"    uv_base /= float(num_rows);\n"
"\n"
"    vec2 offset = vec2(float(x) / float(num_rows), float(y) / float(num_rows));\n"
"    offset.y -= 1.0 / float(num_rows);\n"
"\n"
"    vec2 uv = uv_base + offset;\n"
"\n"
"    fragTexCoord = uv;\n"
"\n"
"\n"
"    ivec2 pos = cdata.pos;\n"
"    ivec2 vpos = ivec2(p[idx[gl_VertexID]]) * 8 * scale;\n"
"    vpos += pos;\n"
"    float nx = (float(vpos.x) / float(res.x)) * 2.0 - 1.0;\n"
"    float ny = (float(res.y - vpos.y) / float(res.y)) * 2.0 - 1.0;\n"
"\n"
"    vec2 npos = vec2(nx, ny);\n"
"\n"
"    gl_Position = vec4(npos, 0, 1);\n"
"}\n"
};

static const char fs_code[] = {
"#version 430 core\n"
"\n"
"in vec2 fragTexCoord;\n"
"out vec4 finalColor;\n"
"\n"
"uniform sampler2D texture0;\n"
"\n"
"vec3 foreground = vec3(0.5, 0.5, 0.5);\n"
"vec3 background = vec3(0, 0, 0.5);\n"
"\n"
"void main()\n"
"{\n"
"    vec4 texel_color = texture(texture0, fragTexCoord);\n"
"    bool is_fore = texel_color.x == 1.0;\n"
"\n"
"    vec3 color = (float(is_fore) * foreground) + (float(!is_fore) * background);\n"
"    finalColor = vec4(color, 1);\n"
"}\n"
"\n"
};

static bool CompileShader(const char *str, int len, GLenum type, unsigned int *shader)
{
    unsigned int s = glCreateShader(type);
    glShaderSource(s, 1, &str, &len);
    glCompileShader(s);

    int success = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &success);
    if (!success) {
        int length = 0;
        int size = 0;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &length);

        char *mesg = malloc(length + 1);
        
        glGetShaderInfoLog(s, length + 1, &size, mesg);
        mesg[length] = 0;
        
        printf("Shader compilation: %s\n", mesg);

        free(mesg);
        glDeleteShader(s);
        return false;
    }

    *shader = s;
    return true;
}

static bool LinkProgram(int program)
{
    glLinkProgram(program);

    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        int length = 0;
        int size = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        
        char *mesg = malloc(length + 1);
        
        glGetProgramInfoLog(program, length + 1, &size, mesg);
        mesg[length] = 0;
        
        printf("Program linking: %s\n", mesg);

        free(mesg);
        glDeleteProgram(program);
        return false;
    }

    return true;
}

static void DebugCallback(GLenum source,
                          GLenum type, GLuint id, GLenum severity,
                          GLsizei length, const GLchar *message, const void *userParam)
{
    (void)(source);
    (void)(type);
    (void)(id);
    (void)(severity);
    (void)(length);
    (void)(userParam);
    printf("%s\n", message);
}    

static void GrInit(void)
{   
    //da_reserve(&char_buf, INITIAL_CHAR_BUF_LEN);
    char_buf.items = malloc(sizeof(char_buf.items[0]) * INITIAL_CHAR_BUF_LEN);
    char_buf.capacity = INITIAL_CHAR_BUF_LEN;

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(DebugCallback, NULL);

    glGenVertexArrays(1, &default_vao);
    glBindVertexArray(default_vao);
    
    glGenBuffers(1, &char_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, char_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(CharacterData) * INITIAL_CHAR_BUF_LEN, NULL, GL_DYNAMIC_DRAW);
        
    // text shader compilation and linking.
    {
        unsigned int vert;
        CompileShader(vs_code, sizeof(vs_code), GL_VERTEX_SHADER, &vert);

        unsigned int frag;
        CompileShader(fs_code, sizeof(fs_code), GL_FRAGMENT_SHADER, &frag);

        text_shader_program = glCreateProgram();
        glAttachShader(text_shader_program, vert);
        glAttachShader(text_shader_program, frag);
        LinkProgram(text_shader_program);
        
        glDeleteShader(vert);
        glDeleteShader(frag);
    }

    num_rows_loc    = glGetUniformLocation(text_shader_program, "num_rows");
    atlas_width_loc = glGetUniformLocation(text_shader_program, "atlas_width");
    aspect_loc      = glGetUniformLocation(text_shader_program, "aspect");
    res_loc         = glGetUniformLocation(text_shader_program, "res");
    scale_loc         = glGetUniformLocation(text_shader_program, "scale");
    texture0_loc    = glGetUniformLocation(text_shader_program, "texture0");
    
    // load bitmap font as a texture
    {
        unsigned char *texture = NULL;

        int num_glyfs = 256;
        int glyfs_per_row = 16;
        int atlas_width = glyfs_per_row * 8;
        int atlas_height = atlas_width;
        int size = num_glyfs * 64; // 256 = num characters, 64 = squared resolution
        texture = malloc(size);

        for (int gy = 0; gy < glyfs_per_row; gy++) {
            for (int gx = 0; gx < glyfs_per_row; gx++) {
                unsigned long long glyf = sys_font_std[(gy * glyfs_per_row) + gx];
                
                for (int y = 0; y < 8; y++) {
                    for (int x = 0; x < 8; x++) {
                        bool dark = (glyf >> ((y*8)+(x))) & 1;
                        int total = size - atlas_width;
                        int base_y = gy * glyfs_per_row * 64;
                        texture[(total - (base_y + (y*glyfs_per_row*8))) + (gx*8) + x] = dark ? 0xFF : 0x00;
                    }
                }
            }
        }

        glGenTextures(1, &glyf_tex);
        glBindTexture(GL_TEXTURE_2D, glyf_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, atlas_width, atlas_height);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
  	                        atlas_width, atlas_height,
  	                        GL_RED,
  	                        GL_UNSIGNED_BYTE,
  	                        texture);                            

        free(texture);
    }
    
    // set uniforms;
    
    //float aspect = (float)h / (float)w;
    
    glUseProgram(text_shader_program);
    glUniform1i(num_rows_loc, 16);
    glUniform1i(atlas_width_loc, 16*8);
    glUniform2i(res_loc, cur_width, cur_height);
    glUniform1i(scale_loc, font_scale);
    glUniform1i(texture0_loc, 0);
}

static void GrShutdown(void)
{
    glDeleteTextures(1, &glyf_tex);
    
    free(char_buf.items);
    glDeleteProgram(text_shader_program);
    glDeleteBuffers(1, &char_ssbo);
}

static void DrawChars(void)
{
    if (char_buf.count < 1) return;
    
    glUseProgram(text_shader_program);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, char_ssbo);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, char_buf.count * sizeof(CharacterData), char_buf.items);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, char_ssbo);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, glyf_tex);
    
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, char_buf.count);
    char_buf.count = 0;
}

static void CharBufferAppend(CharacterData cd)
{
    if (char_buf.count == char_buf.capacity) {
        DrawChars();
    }

    char_buf.items[char_buf.count] = cd;
    ++char_buf.count;
}

void GrText(const char *str, int x, int y)
{
    for (const char *c = str; *c != 0; ++c) {
        CharacterData cd = {
            .character = *c,
            .x = x + (c - str) * GLYF_WIDTH * font_scale,
            .y = y
        };
        CharBufferAppend(cd);
    }
}

void GrChar(unsigned char c, int x, int y)
{
    CharacterData cd = {
        .character = c,
        .x = x,
        .y = y
    };
    CharBufferAppend(cd);
}

void GrSetFontScale(unsigned int scale)
{
    font_scale = scale;
    glUseProgram(text_shader_program);
    glUniform1i(scale_loc, font_scale);
    glUseProgram(0);
}

unsigned int GrGetFontScale(void)
{
    return font_scale;
}

unsigned int GrFontSize(void)
{
    return font_scale * GLYF_WIDTH;
}

void GrSwapBuffers(void)
{
    DrawChars(); // Flush character buffer
    glXSwapBuffers(display, window);
    glClear(GL_COLOR_BUFFER_BIT);
}

void LoadGLPfns(void)
{
    glXCreateContextAttribsARB = (glXCreateContextAttribsARBPfn)glXGetProcAddress((GLubyte*)"glXCreateContextAttribsARB");
    glXSwapIntervalEXT = (glXSwapIntervalEXTPfn)glXGetProcAddress((GLubyte*)"glXSwapIntervalEXT");
    glGenBuffers = (glGenBuffersPfn)glXGetProcAddress((GLubyte*)"glGenBuffers");
    glBindBuffer = (glBindBufferPfn)glXGetProcAddress((GLubyte*)"glBindBuffer");
    glDeleteBuffers = (glDeleteBuffersPfn)glXGetProcAddress((GLubyte*)"glDeleteBuffers");
    glBufferData = (glBufferDataPfn)glXGetProcAddress((GLubyte*)"glBufferData");
    glBufferSubData = (glBufferSubDataPfn)glXGetProcAddress((GLubyte*)"glBufferSubData");
    glBindBufferBase = (glBindBufferBasePfn)glXGetProcAddress((GLubyte*)"glBindBufferBase");
    glDrawArraysInstanced = (glDrawArraysInstancedPfn)glXGetProcAddress((GLubyte*)"glDrawArraysInstanced");
    glCreateShader = (glCreateShaderPfn)glXGetProcAddress((GLubyte*)"glCreateShader");
    glShaderSource = (glShaderSourcePfn)glXGetProcAddress((GLubyte*)"glShaderSource");
    glGetShaderiv = (glGetShaderivPfn)glXGetProcAddress((GLubyte*)"glGetShaderiv");
    glGetShaderInfoLog = (glGetShaderInfoLogPfn)glXGetProcAddress((GLubyte*)"glGetShaderInfoLog");
    glCompileShader = (glCompileShaderPfn)glXGetProcAddress((GLubyte*)"glCompileShader");
    glDeleteShader = (glDeleteShaderPfn)glXGetProcAddress((GLubyte*)"glDeleteShader");
    glCreateProgram = (glCreateProgramPfn)glXGetProcAddress((GLubyte*)"glCreateProgram");
    glAttachShader = (glAttachShaderPfn)glXGetProcAddress((GLubyte*)"glAttachShader");
    glLinkProgram = (glLinkProgramPfn)glXGetProcAddress((GLubyte*)"glLinkProgram");
    glGetProgramiv = (glGetProgramivPfn)glXGetProcAddress((GLubyte*)"glGetProgramiv");
    glGetProgramInfoLog = (glGetProgramInfoLogPfn)glXGetProcAddress((GLubyte*)"glGetProgramInfoLog");
    glUseProgram = (glUseProgramPfn)glXGetProcAddress((GLubyte*)"glUseProgram");
    glDeleteProgram = (glDeleteProgramPfn)glXGetProcAddress((GLubyte*)"glDeleteProgram");
    glGetUniformLocation = (glGetUniformLocationPfn)glXGetProcAddress((GLubyte*)"glGetUniformLocation");
    glTexStorage2D = (glTexStorage2DPfn)glXGetProcAddress((GLubyte*)"glTexStorage2D");
    glDebugMessageCallback = (glDebugMessageCallbackPfn)glXGetProcAddress((GLubyte*)"glDebugMessageCallback");
    glGenVertexArrays = (glGenVertexArraysPfn)glXGetProcAddress((GLubyte*)"glGenVertexArrays");
    glBindVertexArray = (glBindVertexArrayPfn)glXGetProcAddress((GLubyte*)"glBindVertexArray");
    glUniform1f = (glUniform1fPfn)glXGetProcAddress((GLubyte*)"glUniform1f");
    glUniform2i = (glUniform2iPfn)glXGetProcAddress((GLubyte*)"glUniform2i");
    glUniform1i = (glUniform1iPfn)glXGetProcAddress((GLubyte*)"glUniform1i");
}

#endif // APPLICATION_IMPLEMENTATION

#endif // APPLICATION_H
