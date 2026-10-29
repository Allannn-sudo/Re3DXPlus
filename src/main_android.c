/* The Android side of the port (main.m's, for a phone like the Xiaomi Mi 6): MainActivity.java
 * hands over the SurfaceView's surface, touches, the gravity and the lifecycle; the game runs
 * on a thread of its own here with an OpenGL ES 3 context (4x MSAA) on that surface, the
 * loading thread with a context sharing it, and the sound through AAudio. Built with -DWMMT_VK
 * the game's GL goes through vkgl (Vulkan) instead: a swapchain on the surface, the loading
 * thread vkgl's second. */
#ifdef WMMT_VK
#define VK_USE_PLATFORM_ANDROID_KHR
#endif
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <aaudio/AAudio.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <jni.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "app.h"
#include "gl.h"
#include "sound.h"

/* the attract movies (prepare_movie.sh), indexed as app_movie() returns them */
static const char *const MOVIES[3] = { "intro.mp4", "playdemo.mp4", "tutorial.mp4" };

#define TOUCH_MAX 256
typedef struct { int id, phase; float x, y; } touch_ev;

static struct {
  JavaVM *vm;
  jobject activity;
  jmethodID show_movie, play_bgm, bgm_level, fail;
  char data_dir[512], save_dir[512];
  pthread_mutex_t mx;
  pthread_cond_t cv;
  ANativeWindow *window; /* the surface to draw on (NULL while there is none) */
  int window_changed, paused, started;
  touch_ev touches[TOUCH_MAX];
  int n_touch;
  int press_start, movie_done_ev, memory_warning;
  float tilt;
  int tilt_new, wants_tilt;
  /* the game thread's own */
#ifdef WMMT_VK
  int vk_up, have_surface;
  VkSurfaceKHR vk_surface;
#endif
  EGLDisplay dpy;
  EGLConfig config;
  EGLContext ctx, load_ctx;
  EGLSurface surface;
  int fb_w, fb_h;
  float touch_k, touch_edge;
  int movie; /* shown, -1 none */
  const char *bgm;
  AAudioStream *audio;
} A = { .movie = -1 };

static double now_s(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (double)t.tv_sec + t.tv_nsec * 1e-9;
}

/* ---- sound: sound.c's mix, 44.1 kHz stereo int16, low latency ---- */

static aaudio_data_callback_result_t audio_cb(AAudioStream *s, void *user, void *data, int32_t frames) {
  snd_mix((short *)data, frames);
  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static void audio_error(AAudioStream *s, void *user, aaudio_result_t err);

static void audio_open(void) {
  AAudioStreamBuilder *b;
  if (AAudio_createStreamBuilder(&b) != AAUDIO_OK) return;
  AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_I16);
  AAudioStreamBuilder_setChannelCount(b, 2);
  AAudioStreamBuilder_setSampleRate(b, SND_RATE);
  AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
  AAudioStreamBuilder_setUsage(b, AAUDIO_USAGE_GAME);
  AAudioStreamBuilder_setDataCallback(b, audio_cb, NULL);
  AAudioStreamBuilder_setErrorCallback(b, audio_error, NULL);
  if (AAudioStreamBuilder_openStream(b, &A.audio) == AAUDIO_OK) AAudioStream_requestStart(A.audio);
  else A.audio = NULL;
  AAudioStreamBuilder_delete(b);
}

/* the output went away (headphones in or out): a new stream, from a thread of its own as
 * AAudio asks */
static void *audio_reopen(void *arg) {
  if (A.audio) AAudioStream_close(A.audio);
  A.audio = NULL;
  audio_open();
  return NULL;
}

static void audio_error(AAudioStream *s, void *user, aaudio_result_t err) {
  if (err != AAUDIO_ERROR_DISCONNECTED) return;
  pthread_t t;
  if (pthread_create(&t, NULL, audio_reopen, NULL) == 0) pthread_detach(t);
}

/* ---- calls up to MainActivity (the game thread is attached to the VM) ---- */

static JNIEnv *env_here(void) {
  JNIEnv *env = NULL;
  if ((*A.vm)->GetEnv(A.vm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) (*A.vm)->AttachCurrentThread(A.vm, &env, NULL);
  return env;
}

static void call_str(jmethodID m, const char *s) {
  JNIEnv *env = env_here();
  jstring js = s ? (*env)->NewStringUTF(env, s) : NULL;
  (*env)->CallVoidMethod(env, A.activity, m, js);
  if (js) (*env)->DeleteLocalRef(env, js);
}

/* the movie the attract loop wants and the BGM (main.m syncMovie) */
static void sync_movie(void) {
  int want = app_movie();
  if (want != A.movie) {
    A.movie = want;
    call_str(A.show_movie, want >= 0 ? MOVIES[want] : NULL);
  }
  const char *track = app_bgm();
  if (track != A.bgm) {
    A.bgm = track;
    call_str(A.play_bgm, track);
  }
  /* (the story ending's fade out) */
  static float level = 1.0f;
  float l = app_bgm_level();
  if (l != level) {
    level = l;
    JNIEnv *env = env_here();
    (*env)->CallVoidMethod(env, A.activity, A.bgm_level, (jfloat)l);
  }
}

/* ---- GL ---- */

#ifdef WMMT_VK
/* Vulkan (vkgl): the pipelines it made last time from save_dir; the programs it has no SPIR-V
 * for written there too, for the next build */
static int egl_init(void) {
  static const char *const ext[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME };
  if (vkgl_create(ext, 2) != 0) return -1;
  char path[600];
  snprintf(path, sizeof path, "%s/vk_pipelines.bin", A.save_dir);
  vkgl_pipeline_cache_file(path);
  setenv("VKGL_MISSING_DIR", A.save_dir, 1);
  A.vk_up = 1;
  A.load_ctx = (EGLContext)1; /* (a loading thread: vkgl's second) */
  return 0;
}

static void *loader(void *arg) {
  vkgl_thread_loader();
  app_loader();
  return NULL;
}

/* the surface MainActivity has now (under A.mx): its swapchain, or none */
static void take_window(void) {
  A.window_changed = 0;
  if (A.have_surface) {
    vkgl_release_surface(); /* (the surface with it) */
    A.vk_surface = VK_NULL_HANDLE;
    A.have_surface = 0;
  }
  if (!A.window) return;
  VkAndroidSurfaceCreateInfoKHR ci = { VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR };
  ci.window = A.window;
  if (vkCreateAndroidSurfaceKHR(vkgl_instance(), &ci, NULL, &A.vk_surface) != VK_SUCCESS ||
      vkgl_surface(A.vk_surface, ANativeWindow_getWidth(A.window), ANativeWindow_getHeight(A.window)) != 0) {
    fprintf(stderr, "no Vulkan surface\n");
    return;
  }
  A.have_surface = 1;
  vkgl_screen_size(&A.fb_w, &A.fb_h);
  fprintf(stderr, "surface %dx%d (Vulkan)\n", A.fb_w, A.fb_h);
}
#define SURFACE_UP A.have_surface
#else
#define SURFACE_UP (A.surface != EGL_NO_SURFACE)

static int egl_init(void) {
  A.dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (!eglInitialize(A.dpy, NULL, NULL)) return -1;
  /* 8-bit colour, depth 24 + stencil 8 (the cars' shadows), 4x MSAA where there is (an
   * Adreno resolves it on chip, nearly free) */
  for (int samples = 4; samples >= 0; samples -= 4) {
    const EGLint attr[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR, EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
                            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 0,
                            EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8,
                            EGL_SAMPLE_BUFFERS, samples ? 1 : 0, EGL_SAMPLES, samples, EGL_NONE };
    EGLint n = 0;
    if (eglChooseConfig(A.dpy, attr, &A.config, 1, &n) && n == 1) {
      fprintf(stderr, "EGL config: %d samples\n", samples);
      break;
    }
    if (!samples) return -1;
  }
  const EGLint ca[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
  A.ctx = eglCreateContext(A.dpy, A.config, EGL_NO_CONTEXT, ca);
  if (A.ctx == EGL_NO_CONTEXT) return -1;
  A.load_ctx = eglCreateContext(A.dpy, A.config, A.ctx, ca);
  return 0;
}

static void *loader(void *arg) {
  const EGLint pa[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
  EGLSurface pb = eglCreatePbufferSurface(A.dpy, A.config, pa);
  if (!eglMakeCurrent(A.dpy, pb, pb, A.load_ctx)) {
    fprintf(stderr, "loading thread: no context\n");
    return NULL;
  }
  app_loader();
  return NULL;
}

/* the surface MainActivity has now (under A.mx): made current, or none */
static void take_window(void) {
  if (A.surface != EGL_NO_SURFACE) {
    eglMakeCurrent(A.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(A.dpy, A.surface);
    A.surface = EGL_NO_SURFACE;
  }
  A.window_changed = 0;
  if (!A.window) return;
  A.surface = eglCreateWindowSurface(A.dpy, A.config, A.window, NULL);
  if (A.surface == EGL_NO_SURFACE || !eglMakeCurrent(A.dpy, A.surface, A.surface, A.ctx)) {
    fprintf(stderr, "no window surface: 0x%x\n", eglGetError());
    A.surface = EGL_NO_SURFACE;
    return;
  }
  eglQuerySurface(A.dpy, A.surface, EGL_WIDTH, &A.fb_w);
  eglQuerySurface(A.dpy, A.surface, EGL_HEIGHT, &A.fb_h);
  eglSwapInterval(A.dpy, 1);
  fprintf(stderr, "surface %dx%d\n", A.fb_w, A.fb_h);
}
#endif

static int start_game(void) {
  app_save_dir(A.save_dir);
  /* the game's 1024x768 layout, 768 high on screen; a phone's 16:9 is wider (main.m) */
  A.touch_k = 768.0f / (float)A.fb_h;
  A.touch_edge = ((float)A.fb_w * A.touch_k - 1024.0f) * 0.5f;
  if (A.touch_edge < 1.0f) A.touch_edge = 0;
  app_console();
  /* the keys a little smaller than the iPhone's 1.3, under the meters' plate and the shift gate */
  app_wide(A.touch_edge, A.touch_edge > 0 ? 1.2f : 1.0f);
  if (app_init(A.data_dir, A.fb_w, A.fb_h, 1024, 768) != 0) {
    fprintf(stderr, "%s\n", app_error());
    call_str(A.fail, app_error());
    return -1;
  }
  audio_open();
  pthread_t t;
  if (A.load_ctx != EGL_NO_CONTEXT && pthread_create(&t, NULL, loader, NULL) == 0) pthread_detach(t);
  else fprintf(stderr, "no loading thread: the menu frames load\n");
  return 0;
}

static void *game(void *arg) {
  env_here();
  if (egl_init() != 0) {
    call_str(A.fail, "no OpenGL ES 3 on this phone");
    return NULL;
  }
  double last = 0;
  int failed = 0, away = 0;
  for (;;) {
    touch_ev tv[TOUCH_MAX];
    int nt, press, done, warn, tilt_new;
    float tilt;
    pthread_mutex_lock(&A.mx);
    while (A.paused || (!A.window && !A.window_changed)) {
      if (A.started && !away && !failed) {
        app_suspend(1); /* the loading thread stops too */
#ifdef WMMT_VK
        vkgl_pipeline_cache_save();
#endif
        if (A.audio) AAudioStream_requestPause(A.audio);
        away = 1;
      }
      last = 0;
      pthread_cond_wait(&A.cv, &A.mx);
    }
    if (A.window_changed) take_window();
    nt = A.n_touch;
    memcpy(tv, A.touches, nt * sizeof *tv);
    A.n_touch = 0;
    press = A.press_start, done = A.movie_done_ev, warn = A.memory_warning;
    A.press_start = A.movie_done_ev = A.memory_warning = 0;
    tilt = A.tilt, tilt_new = A.tilt_new;
    A.tilt_new = 0;
    pthread_mutex_unlock(&A.mx);
    if (!SURFACE_UP) continue;
    if (!A.started) {
      A.started = 1;
      failed = start_game() != 0;
    }
    if (failed) {
      usleep(100000);
      continue;
    }
    if (away) {
      app_suspend(0);
      if (A.audio) AAudioStream_requestStart(A.audio);
      away = 0;
    }
    if (warn) app_memory_warning();
    if (done) {
      A.movie = -1;
      app_movie_done();
    }
    if (press) app_press_start();
    for (int i = 0; i < nt; ++i)
      app_touch((void *)(intptr_t)(tv[i].id + 1), tv[i].phase, tv[i].x * A.touch_k - A.touch_edge, tv[i].y * A.touch_k);
    A.wants_tilt = app_wants_tilt();
    if (tilt_new) app_tilt(tilt);
    double now = now_s();
    float dt = last > 0 ? (float)(now - last) : 1.0f / 30.0f;
    last = now;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    app_frame(dt);
    sync_movie();
    /* depth and stencil need not go out to memory */
    static PFNGLDISCARDFRAMEBUFFEREXTPROC discard_fb;
    if (!discard_fb) discard_fb = (PFNGLDISCARDFRAMEBUFFEREXTPROC)eglGetProcAddress("glDiscardFramebufferEXT");
    const GLenum discard[] = { GL_DEPTH_EXT, GL_STENCIL_EXT };
    if (discard_fb) discard_fb(GL_FRAMEBUFFER, 2, discard);
    double p0 = now_s();
#ifdef WMMT_VK
    /* (the swapchain made again for a new size or rotation) */
    if (vkgl_present() != 0 && A.window) {
      pthread_mutex_lock(&A.mx);
      A.window_changed = 1;
      pthread_mutex_unlock(&A.mx);
    }
#else
    eglSwapBuffers(A.dpy, A.surface);
#endif
    app_frame_done((now_s() - p0) * 1000.0);
  }
  return NULL;
}

/* ---- JNI ---- */

#define JFN(name) Java_com_wmmt_port_MainActivity_##name

JNIEXPORT void JNICALL JFN(nativeStart)(JNIEnv *env, jclass cls, jobject activity, jstring data, jstring save) {
  if (A.activity) return; /* a second onCreate (the activity remade): the game goes on */
  (*env)->GetJavaVM(env, &A.vm);
  A.activity = (*env)->NewGlobalRef(env, activity);
  A.show_movie = (*env)->GetMethodID(env, cls, "showMovie", "(Ljava/lang/String;)V");
  A.play_bgm = (*env)->GetMethodID(env, cls, "playBgm", "(Ljava/lang/String;)V");
  A.bgm_level = (*env)->GetMethodID(env, cls, "bgmLevel", "(F)V");
  A.fail = (*env)->GetMethodID(env, cls, "fail", "(Ljava/lang/String;)V");
  const char *d = (*env)->GetStringUTFChars(env, data, NULL), *s = (*env)->GetStringUTFChars(env, save, NULL);
  snprintf(A.data_dir, sizeof A.data_dir, "%s", d);
  snprintf(A.save_dir, sizeof A.save_dir, "%s", s);
  (*env)->ReleaseStringUTFChars(env, data, d);
  (*env)->ReleaseStringUTFChars(env, save, s);
  char log[600];
  snprintf(log, sizeof log, "%s/log.txt", A.save_dir);
  freopen(log, "w", stderr);
  setvbuf(stderr, NULL, _IONBF, 0);
  fprintf(stderr, "start (android)\n");
  pthread_mutex_init(&A.mx, NULL);
  pthread_cond_init(&A.cv, NULL);
  pthread_t t;
  pthread_attr_t at;
  pthread_attr_init(&at);
  pthread_attr_setstacksize(&at, 8 << 20); /* the recompiled physics has deep frames */
  pthread_create(&t, &at, game, NULL);
  pthread_detach(t);
}

JNIEXPORT void JNICALL JFN(nativeSurface)(JNIEnv *env, jclass cls, jobject surface, jint w, jint h) {
  ANativeWindow *win = surface ? ANativeWindow_fromSurface(env, surface) : NULL;
  pthread_mutex_lock(&A.mx);
  if (A.window) ANativeWindow_release(A.window);
  A.window = win;
  A.window_changed = 1;
  pthread_cond_broadcast(&A.cv);
  pthread_mutex_unlock(&A.mx);
  /* the old surface must be let go of before surfaceDestroyed returns */
  if (!surface) {
    for (int i = 0; i < 200; ++i) {
      pthread_mutex_lock(&A.mx);
      int pending = A.window_changed && A.started;
      pthread_mutex_unlock(&A.mx);
      if (!pending) break;
      usleep(5000);
    }
  }
}

JNIEXPORT void JNICALL JFN(nativeTouch)(JNIEnv *env, jclass cls, jint id, jint phase, jfloat x, jfloat y) {
  pthread_mutex_lock(&A.mx);
  if (A.n_touch < TOUCH_MAX) A.touches[A.n_touch++] = (touch_ev){ id, phase, x, y };
  pthread_mutex_unlock(&A.mx);
}

JNIEXPORT void JNICALL JFN(nativePause)(JNIEnv *env, jclass cls, jboolean p) {
  pthread_mutex_lock(&A.mx);
  A.paused = p;
  pthread_cond_broadcast(&A.cv);
  pthread_mutex_unlock(&A.mx);
}

JNIEXPORT void JNICALL JFN(nativeTilt)(JNIEnv *env, jclass cls, jfloat r) {
  pthread_mutex_lock(&A.mx);
  A.tilt = r;
  A.tilt_new = 1;
  pthread_mutex_unlock(&A.mx);
}

JNIEXPORT jboolean JNICALL JFN(nativeWantsTilt)(JNIEnv *env, jclass cls) { return A.wants_tilt ? JNI_TRUE : JNI_FALSE; }

JNIEXPORT void JNICALL JFN(nativeMovieDone)(JNIEnv *env, jclass cls) {
  pthread_mutex_lock(&A.mx);
  A.movie_done_ev = 1;
  pthread_mutex_unlock(&A.mx);
}

JNIEXPORT void JNICALL JFN(nativePressStart)(JNIEnv *env, jclass cls) {
  pthread_mutex_lock(&A.mx);
  A.press_start = 1;
  pthread_mutex_unlock(&A.mx);
}

JNIEXPORT void JNICALL JFN(nativeMemoryWarning)(JNIEnv *env, jclass cls) {
  pthread_mutex_lock(&A.mx);
  A.memory_warning = 1;
  pthread_mutex_unlock(&A.mx);
}
