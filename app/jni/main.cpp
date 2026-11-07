// Babuun Hollow — ponto de entrada nativo (NativeActivity + VrApi) para Oculus Go
#include <android/log.h>
#include <android/asset_manager.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <jni.h>
#include <unistd.h>
#include <time.h>
#include <sys/prctl.h>
#include <signal.h>
#include <fcntl.h>
#include "VrApi.h"
#include "VrApi_Helpers.h"
#include "VrApi_Input.h"
#include "VrApi_SystemUtils.h"
#include "gfx.h"
#include "game.h"
#include "audio.h"
#include "recorder.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "BabuunHollow", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "BabuunHollow", __VA_ARGS__)

static AAssetManager *gAssets = nullptr;

// ------------------------------------------------------------------ diagnóstico de falhas
// Guarda a etapa atual; se o jogo quebrar, grava em crash.txt e mostra na próxima abertura.
static volatile const char *gStage = "inicio";
static char gCrashPath[256] = "/sdcard/Android/data/com.babuun.hollow/files/crash.txt";
#define STAGE(s) do { gStage = s; LOGI("etapa: %s", s); } while (0)
static void crashHandler(int sig, siginfo_t *info, void *) {
    int fd = open(gCrashPath, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fd >= 0) {
        char b[256];
        int n = snprintf(b, sizeof b, "sinal %d endereco %p etapa %s\n", sig, info ? info->si_addr : nullptr, (const char *)gStage);
        write(fd, b, n);
        close(fd);
    }
    __android_log_print(ANDROID_LOG_FATAL, "BabuunHollow", "FALHA sinal %d na etapa %s", sig, (const char *)gStage);
    signal(sig, SIG_DFL);
    raise(sig);
}
static void installCrashHandler(const char *dataDir) {
    if (dataDir) snprintf(gCrashPath, sizeof gCrashPath, "%s/crash.txt", dataDir);
    struct sigaction sa = {};
    sa.sa_sigaction = crashHandler;
    sa.sa_flags = SA_SIGINFO;
    for (int s : {SIGSEGV, SIGABRT, SIGBUS, SIGILL, SIGFPE}) sigaction(s, &sa, nullptr);
}

bool readAsset(const char *path, std::vector<unsigned char> &out) {
    if (!gAssets) return false;
    AAsset *a = AAssetManager_open(gAssets, path, AASSET_MODE_BUFFER);
    if (!a) return false;
    size_t n = AAsset_getLength(a);
    out.resize(n);
    AAsset_read(a, out.data(), n);
    AAsset_close(a);
    return true;
}

// ------------------------------------------------------------------ estado da aplicação
struct EyeTarget {
    ovrTextureSwapChain *chain = nullptr;
    int len = 0, w = 0, h = 0;
    std::vector<GLuint> fbos, depths;
};

struct App {
    android_app *app = nullptr;
    ovrJava java;
    ovrMobile *ovr = nullptr;
    EGLDisplay dpy = EGL_NO_DISPLAY;
    EGLContext ctx = EGL_NO_CONTEXT;
    EGLSurface tiny = EGL_NO_SURFACE;
    ANativeWindow *window = nullptr;
    bool resumed = false, destroyed = false;
    EyeTarget eyes[2];
    long long frameIndex = 1;
    Gfx gfx;
    Game game;
    bool ready = false;
    // controle
    ovrDeviceID remoteId = ovrDeviceIdType_Invalid;
    uint32_t prevButtons = 0;
    bool leftHanded = false;
    float padMaxX = 1, padMaxY = 1;
    // câmera do jogo (FBO + PBO para leitura assíncrona)
    GLuint camFbo = 0, camColor = 0, camDepth = 0, pbo[2] = {0, 0};
    int pboIdx = 0; bool pboPending[2] = {false, false}; double pboTime[2] = {0, 0};
    double lastCamFrame = 0;
    std::vector<unsigned char> pix;
    double lastTime = 0;
};
static App A;

static double nowSec() { timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

// ------------------------------------------------------------------ JNI: permissões, galeria, caminhos
static JNIEnv *jenv() { return A.java.Env; }

static void requestPermissions() {
    JNIEnv *env = jenv();
    jclass act = env->GetObjectClass(A.java.ActivityObject);
    jmethodID req = env->GetMethodID(act, "requestPermissions", "([Ljava/lang/String;I)V");
    if (!req) { env->ExceptionClear(); return; }
    const char *perms[] = {"android.permission.RECORD_AUDIO", "android.permission.WRITE_EXTERNAL_STORAGE", "android.permission.READ_EXTERNAL_STORAGE"};
    jclass strC = env->FindClass("java/lang/String");
    jobjectArray arr = env->NewObjectArray(3, strC, nullptr);
    for (int i = 0; i < 3; i++) env->SetObjectArrayElement(arr, i, env->NewStringUTF(perms[i]));
    env->CallVoidMethod(A.java.ActivityObject, req, arr, 1);
    if (env->ExceptionCheck()) env->ExceptionClear();
}

static void scanFile(const std::string &path) {
    JNIEnv *env = jenv();
    jclass msc = env->FindClass("android/media/MediaScannerConnection");
    if (!msc) { env->ExceptionClear(); return; }
    jmethodID scan = env->GetStaticMethodID(msc, "scanFile", "(Landroid/content/Context;[Ljava/lang/String;[Ljava/lang/String;Landroid/media/MediaScannerConnection$OnScanCompletedListener;)V");
    if (!scan) { env->ExceptionClear(); return; }
    jclass strC = env->FindClass("java/lang/String");
    jobjectArray arr = env->NewObjectArray(1, strC, env->NewStringUTF(path.c_str()));
    env->CallStaticVoidMethod(msc, scan, A.java.ActivityObject, arr, nullptr, nullptr);
    if (env->ExceptionCheck()) env->ExceptionClear();
}

// trava de multicast/broadcast do Wi-Fi: sem ela o Go pode descartar os anúncios de servidores
static void wifiMulticastLock() {
    JNIEnv *env = jenv();
    jclass act = env->GetObjectClass(A.java.ActivityObject);
    jmethodID gss = env->GetMethodID(act, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jobject wm = gss ? env->CallObjectMethod(A.java.ActivityObject, gss, env->NewStringUTF("wifi")) : nullptr;
    if (env->ExceptionCheck() || !wm) { env->ExceptionClear(); return; }
    jclass wmc = env->GetObjectClass(wm);
    jmethodID cml = env->GetMethodID(wmc, "createMulticastLock", "(Ljava/lang/String;)Landroid/net/wifi/WifiManager$MulticastLock;");
    jobject lock = cml ? env->CallObjectMethod(wm, cml, env->NewStringUTF("babuunhollow")) : nullptr;
    if (env->ExceptionCheck() || !lock) { env->ExceptionClear(); return; }
    jclass lc = env->GetObjectClass(lock);
    jmethodID acq = env->GetMethodID(lc, "acquire", "()V");
    if (acq) env->CallVoidMethod(lock, acq);
    if (env->ExceptionCheck()) env->ExceptionClear();
    else { env->NewGlobalRef(lock); LOGI("multicast lock ok"); }
}

// ------------------------------------------------------------------ EGL
static bool initEgl() {
    A.dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(A.dpy, nullptr, nullptr);
    EGLConfig cfgs[512]; EGLint n = 0;
    eglGetConfigs(A.dpy, cfgs, 512, &n);
    const EGLint want[] = {EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0, EGL_SAMPLES, 0, EGL_NONE};
    EGLConfig cfg = 0;
    for (int i = 0; i < n && !cfg; i++) {
        EGLint v = 0;
        eglGetConfigAttrib(A.dpy, cfgs[i], EGL_RENDERABLE_TYPE, &v);
        if ((v & EGL_OPENGL_ES3_BIT_KHR) != EGL_OPENGL_ES3_BIT_KHR) continue;
        eglGetConfigAttrib(A.dpy, cfgs[i], EGL_SURFACE_TYPE, &v);
        if ((v & (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) != (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) continue;
        bool ok = true;
        for (int j = 0; want[j] != EGL_NONE; j += 2) {
            eglGetConfigAttrib(A.dpy, cfgs[i], want[j], &v);
            if (v != want[j + 1]) { ok = false; break; }
        }
        if (ok) cfg = cfgs[i];
    }
    if (!cfg) { LOGE("sem config EGL"); return false; }
    const EGLint ca[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    A.ctx = eglCreateContext(A.dpy, cfg, EGL_NO_CONTEXT, ca);
    const EGLint sa[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
    A.tiny = eglCreatePbufferSurface(A.dpy, cfg, sa);
    eglMakeCurrent(A.dpy, A.tiny, A.tiny, A.ctx);
    return true;
}

static void createEyes() {
    int w = vrapi_GetSystemPropertyInt(&A.java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH);
    int h = vrapi_GetSystemPropertyInt(&A.java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_HEIGHT);
    for (int e = 0; e < 2; e++) {
        EyeTarget &t = A.eyes[e];
        t.w = w; t.h = h;
        t.chain = vrapi_CreateTextureSwapChain(VRAPI_TEXTURE_TYPE_2D, VRAPI_TEXTURE_FORMAT_8888, w, h, 1, true);
        t.len = vrapi_GetTextureSwapChainLength(t.chain);
        t.fbos.resize(t.len); t.depths.resize(t.len);
        glGenFramebuffers(t.len, t.fbos.data());
        glGenRenderbuffers(t.len, t.depths.data());
        for (int i = 0; i < t.len; i++) {
            GLuint tex = vrapi_GetTextureSwapChainHandle(t.chain, i);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindRenderbuffer(GL_RENDERBUFFER, t.depths[i]);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
            glBindFramebuffer(GL_FRAMEBUFFER, t.fbos[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, t.depths[i]);
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // câmera do jogo
    glGenTextures(1, &A.camColor);
    glBindTexture(GL_TEXTURE_2D, A.camColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, REC_W, REC_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenRenderbuffers(1, &A.camDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, A.camDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, REC_W, REC_H);
    glGenFramebuffers(1, &A.camFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, A.camFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, A.camColor, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, A.camDepth);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glGenBuffers(2, A.pbo);
    for (int i = 0; i < 2; i++) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, A.pbo[i]);
        glBufferData(GL_PIXEL_PACK_BUFFER, REC_W * REC_H * 4, nullptr, GL_STREAM_READ);
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    A.pix.resize(REC_W * REC_H * 4);
    A.game.camTex = A.camColor;
}

// ------------------------------------------------------------------ ciclo de vida do VR
static void handleVrMode() {
    if (A.resumed && A.window && !A.ovr) {
        ovrModeParms p = vrapi_DefaultModeParms(&A.java);
        p.Flags |= VRAPI_MODE_FLAG_NATIVE_WINDOW;
        p.Flags &= ~VRAPI_MODE_FLAG_RESET_WINDOW_FULLSCREEN;
        p.Display = (size_t)A.dpy;
        p.WindowSurface = (size_t)A.window;
        p.ShareContext = (size_t)A.ctx;
        A.ovr = vrapi_EnterVrMode(&p);
        if (!A.ovr) { LOGE("falha ao entrar no modo VR"); return; }
        vrapi_SetClockLevels(A.ovr, 3, 3);
        vrapi_SetPerfThread(A.ovr, VRAPI_PERF_THREAD_TYPE_MAIN, gettid());
        vrapi_SetDisplayRefreshRate(A.ovr, 72.0f);
        vrapi_SetPropertyInt(&A.java, VRAPI_FOVEATION_LEVEL, 2);
        LOGI("modo VR ativo");
    } else if ((!A.resumed || !A.window) && A.ovr) {
        vrapi_LeaveVrMode(A.ovr);
        A.ovr = nullptr;
        eglMakeCurrent(A.dpy, A.tiny, A.tiny, A.ctx);
        if (A.ready) A.game.onPause();
    }
}

static void onCmd(android_app *app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_RESUME: A.resumed = true; break;
        case APP_CMD_PAUSE: A.resumed = false; break;
        case APP_CMD_INIT_WINDOW: A.window = app->window; break;
        case APP_CMD_TERM_WINDOW: A.window = nullptr; break;
        case APP_CMD_DESTROY: A.destroyed = true; break;
    }
    handleVrMode();
}

// ------------------------------------------------------------------ entrada do controle do Go
static Input readInput(double displayTime) {
    Input in;
    // procura o controle (Go tem apenas um)
    if (A.remoteId == ovrDeviceIdType_Invalid) {
        for (uint32_t i = 0;; i++) {
            ovrInputCapabilityHeader h;
            if (vrapi_EnumerateInputDevices(A.ovr, i, &h) < 0) break;
            if (h.Type == ovrControllerType_TrackedRemote) {
                ovrInputTrackedRemoteCapabilities caps;
                caps.Header = h;
                if (vrapi_GetInputDeviceCapabilities(A.ovr, &caps.Header) >= 0) {
                    A.remoteId = h.DeviceID;
                    A.leftHanded = (caps.ControllerCapabilities & ovrControllerCaps_LeftHand) != 0;
                    A.padMaxX = caps.TrackpadMaxX > 0 ? caps.TrackpadMaxX : 1;
                    A.padMaxY = caps.TrackpadMaxY > 0 ? caps.TrackpadMaxY : 1;
                }
                break;
            }
        }
    }
    if (A.remoteId != ovrDeviceIdType_Invalid) {
        ovrInputStateTrackedRemote st;
        st.Header.ControllerType = ovrControllerType_TrackedRemote;
        if (vrapi_GetCurrentInputState(A.ovr, A.remoteId, &st.Header) >= 0) {
            ovrTracking tr;
            if (vrapi_GetInputTrackingState(A.ovr, A.remoteId, displayTime, &tr) >= 0) {
                in.hasCtrl = true;
                const ovrQuatf &q = tr.HeadPose.Pose.Orientation;
                in.ctrl = Q(q.x, q.y, q.z, q.w);
            }
            uint32_t b = st.Buttons, pb = A.prevButtons;
            in.trigger = b & ovrButton_A;
            in.triggerDown = (b & ovrButton_A) && !(pb & ovrButton_A);
            in.padClick = b & ovrButton_Enter;
            in.padClickDown = (b & ovrButton_Enter) && !(pb & ovrButton_Enter);
            in.backDown = (b & ovrButton_Back) && !(pb & ovrButton_Back);
            in.padTouch = st.TrackpadStatus != 0;
            if (in.padTouch) {
                in.padX = clampf(st.TrackpadPosition.x / A.padMaxX * 2 - 1, -1, 1);
                in.padY = clampf(-(st.TrackpadPosition.y / A.padMaxY * 2 - 1), -1, 1);
            }
            A.prevButtons = b;
        } else A.remoteId = ovrDeviceIdType_Invalid;
    }
    in.leftHanded = A.leftHanded;
    return in;
}

// ------------------------------------------------------------------ quadro
static void submitLoadingFrame() {
    A.frameIndex++;
    double disp = vrapi_GetPredictedDisplayTime(A.ovr, A.frameIndex);
    ovrTracking2 tr = vrapi_GetPredictedTracking2(A.ovr, disp);
    ovrLayerProjection2 black = vrapi_DefaultLayerBlackProjection2();
    black.HeadPose = tr.HeadPose;
    black.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;
    ovrLayerLoadingIcon2 icon = vrapi_DefaultLayerLoadingIcon2();
    icon.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;
    const ovrLayerHeader2 *layers[] = {&black.Header, &icon.Header};
    ovrSubmitFrameDescription2 fd = {};
    fd.Flags |= VRAPI_FRAME_FLAG_FLUSH;
    fd.SwapInterval = 1;
    fd.FrameIndex = A.frameIndex;
    fd.DisplayTime = disp;
    fd.LayerCount = 2;
    fd.Layers = layers;
    vrapi_SubmitFrame2(A.ovr, &fd);
}

static void renderCamera(double t) {
    // renderiza a visão da câmera segurável (~30 fps)
    if (!A.game.camWantsFrame() || t - A.lastCamFrame < 1.0 / REC_FPS) return;
    A.lastCamFrame = t;
    glBindFramebuffer(GL_FRAMEBUFFER, A.camFbo);
    glViewport(0, 0, REC_W, REC_H);
    Scene sc = A.game.scene();
    glClearColor(sc.clear.x, sc.clear.y, sc.clear.z, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    DrawCtx c;
    M4 cw = A.game.cameraWorld();
    c.view = mInvRigid(cw);
    c.proj = mPersp(70 * PI / 180, (float)REC_W / REC_H, 0.05f, 120);
    c.camPos = cw.pos();
    c.camFwd = cw.dir(V3(0, 0, -1));
    c.cullDist = 60;
    A.game.render(c, true);
    // leitura assíncrona via PBO (pega o quadro anterior para não travar a GPU)
    if (A.game.recording()) {
        int cur = A.pboIdx, prev = A.pboIdx ^ 1;
        glBindBuffer(GL_PIXEL_PACK_BUFFER, A.pbo[cur]);
        glReadPixels(0, 0, REC_W, REC_H, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        A.pboPending[cur] = true; A.pboTime[cur] = t;
        if (A.pboPending[prev]) {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, A.pbo[prev]);
            void *p = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, REC_W * REC_H * 4, GL_MAP_READ_BIT);
            if (p) { memcpy(A.pix.data(), p, A.pix.size()); glUnmapBuffer(GL_PIXEL_PACK_BUFFER); A.game.onCameraFrame(A.pix.data(), A.pboTime[prev]); }
            A.pboPending[prev] = false;
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        A.pboIdx ^= 1;
    } else A.pboPending[0] = A.pboPending[1] = false;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void frame() {
    A.frameIndex++;
    double disp = vrapi_GetPredictedDisplayTime(A.ovr, A.frameIndex);
    ovrTracking2 tr = vrapi_GetPredictedTracking2(A.ovr, disp);
    Input in = readInput(disp);
    const ovrQuatf &hq = tr.HeadPose.Pose.Orientation;
    in.head = Q(hq.x, hq.y, hq.z, hq.w);
    in.headPos = V3(tr.HeadPose.Pose.Position.x, tr.HeadPose.Pose.Position.y, tr.HeadPose.Pose.Position.z);
    double t = nowSec();
    in.dt = A.lastTime > 0 ? (float)(t - A.lastTime) : 1 / 72.f;
    in.time = t;
    A.lastTime = t;
    if (!in.hasCtrl) {  // sem controle: usa a cabeça como "mão"
        in.ctrl = in.head;
    }
    gStage = "update";
    A.game.update(in);
    gStage = "camera";
    renderCamera(t);
    gStage = "render";

    M4 rig = A.game.rigMatrix();
    M4 rigInv = mInvRigid(rig);
    Scene sc = A.game.scene();
    ovrLayerProjection2 layer = vrapi_DefaultLayerProjection2();
    layer.HeadPose = tr.HeadPose;
    for (int e = 0; e < 2; e++) {
        EyeTarget &et = A.eyes[e];
        int idx = (int)(A.frameIndex % et.len);
        glBindFramebuffer(GL_FRAMEBUFFER, et.fbos[idx]);
        glViewport(0, 0, et.w, et.h);
        glScissor(0, 0, et.w, et.h);
        glClearColor(sc.clear.x, sc.clear.y, sc.clear.z, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        DrawCtx c;
        M4 ev; memcpy(ev.m, &tr.Eye[e].ViewMatrix, 64);
        M4 ep; memcpy(ep.m, &tr.Eye[e].ProjectionMatrix, 64);
        // VrApi usa matrizes linha-major: transpõe
        M4 evT, epT;
        for (int r = 0; r < 4; r++) for (int cc = 0; cc < 4; cc++) { evT.at(r, cc) = ev.m[r * 4 + cc]; epT.at(r, cc) = ep.m[r * 4 + cc]; }
        c.view = evT * rigInv;
        c.proj = epT;
        M4 camW = mInvRigid(c.view);
        c.camPos = camW.pos();
        c.camFwd = camW.dir(V3(0, 0, -1));
        c.cullDist = 70;
        A.game.render(c, false);
        // limpa a borda (evita esticar pixels nas laterais do time-warp)
        glEnable(GL_SCISSOR_TEST);
        glClearColor(0, 0, 0, 1);
        glScissor(0, 0, 1, et.h); glClear(GL_COLOR_BUFFER_BIT);
        glScissor(et.w - 1, 0, 1, et.h); glClear(GL_COLOR_BUFFER_BIT);
        glScissor(0, 0, et.w, 1); glClear(GL_COLOR_BUFFER_BIT);
        glScissor(0, et.h - 1, et.w, 1); glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);
        const GLenum att[] = {GL_DEPTH_ATTACHMENT};
        glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, att);
        glFlush();
        layer.Textures[e].ColorSwapChain = et.chain;
        layer.Textures[e].SwapChainIndex = idx;
        layer.Textures[e].TexCoordsFromTanAngles = ovrMatrix4f_TanAngleMatrixFromProjection(&tr.Eye[e].ProjectionMatrix);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    const ovrLayerHeader2 *layers[] = {&layer.Header};
    ovrSubmitFrameDescription2 fd = {};
    fd.SwapInterval = 1;
    fd.FrameIndex = A.frameIndex;
    fd.DisplayTime = disp;
    fd.LayerCount = 1;
    fd.Layers = layers;
    vrapi_SubmitFrame2(A.ovr, &fd);
}

// ------------------------------------------------------------------ main
void android_main(android_app *app) {
    A.app = app;
    gAssets = app->activity->assetManager;
    A.java.Vm = app->activity->vm;
    A.java.Vm->AttachCurrentThread(&A.java.Env, nullptr);
    A.java.ActivityObject = app->activity->clazz;
    prctl(PR_SET_NAME, (long)"BabuunMain", 0, 0, 0);
    installCrashHandler(app->activity->internalDataPath);
    std::string lastCrash;
    {
        FILE *cf = fopen(gCrashPath, "r");
        if (cf) { char b[256] = {0}; fgets(b, sizeof b, cf); fclose(cf); lastCrash = b; remove(gCrashPath); }
    }
    // Responde aos comandos do Android IMEDIATAMENTE (senão o sistema acusa "não está respondendo")
    app->onAppCmd = onCmd;
    app->onInputEvent = [](android_app *, AInputEvent *) -> int32_t { return 0; };

    STAGE("vrapi_Initialize");
    ovrInitParms ip = vrapi_DefaultInitParms(&A.java);
    if (vrapi_Initialize(&ip) != VRAPI_INITIALIZE_SUCCESS) { LOGE("vrapi_Initialize falhou"); exit(0); }
    STAGE("EGL");
    if (!initEgl()) { LOGE("EGL falhou"); vrapi_Shutdown(); exit(0); }

    std::string save = app->activity->internalDataPath ? app->activity->internalDataPath : "/sdcard";
    std::string vid1 = "/sdcard/Movies/BabuunHollow";
    std::string vid2 = std::string("/sdcard/Android/data/com.babuun.hollow/files/Movies");
    A.game.scanFile = scanFile;
    A.game.askPermissions = requestPermissions;
    int loadStep = 0;

    while (!A.destroyed) {
        int events;
        android_poll_source *src;
        // sem VR ainda: espera eventos com timeout curto para nunca travar
        while (ALooper_pollAll(A.ovr ? 0 : 100, nullptr, &events, (void **)&src) >= 0) {
            if (src) src->process(app, src);
            if (app->destroyRequested) { A.destroyed = true; break; }
            handleVrMode();
            if (A.ovr) break;
        }
        if (A.destroyed) break;
        if (!A.ovr) continue;
        if (!A.ready) {
            // tela de carregamento do próprio VrApi (ícone girando) enquanto carrega em etapas
            submitLoadingFrame();
            switch (loadStep++) {
                case 0: STAGE("swapchains"); createEyes(); break;
                case 1: STAGE("shaders"); A.gfx.init(); break;
                case 2: STAGE("audio"); gAudio.init(); break;
                case 3: STAGE("wifi"); wifiMulticastLock(); break;
                case 4:
                    STAGE("carregando jogo");
                    A.game.init(&A.gfx, save, vid1, vid2);
                    if (!lastCrash.empty()) A.game.showError("Falha anterior: " + lastCrash);
                    A.ready = true;
                    A.lastTime = 0;
                    STAGE("loop");
                    break;
            }
            continue;
        }
        gStage = "quadro";
        frame();
    }
    if (A.ready) {
        A.game.onPause();
        A.game.onDestroy();  // se for host: passa o servidor para o próximo jogador
        gRecorder.stop();
        gAudio.shutdown();
    }
    if (A.ovr) vrapi_LeaveVrMode(A.ovr);
    vrapi_Shutdown();
    A.java.Vm->DetachCurrentThread();
}
