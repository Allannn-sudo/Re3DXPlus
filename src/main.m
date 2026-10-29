// iOS 6 shell: a full-screen GLES2 view driving app.c. Manual retain/release
// (no ARC: the Xcode 16 toolchain has no libarclite for iOS 6).
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreMotion/CoreMotion.h>
#include <math.h>
#import <OpenGLES/ES2/gl.h>
#import <OpenGLES/ES2/glext.h>
#include <pthread.h>

#include "app.h"
#include "sound_ios.h"

static NSString *DataDir(void) {
  NSFileManager *fm = [NSFileManager defaultManager];
  NSString *bundle = [[NSBundle mainBundle] resourcePath];
  if ([fm fileExistsAtPath:[bundle stringByAppendingPathComponent:@"tokyo.wpk"]]) return bundle;
  return @"/var/mobile/Media/wmmt";
}

/* Where the player's files and the logs go: the app's Documents when the data is in the
 * bundle (an .ipa install, whose bundle is read-only), else the data folder itself. */
static NSString *SaveDir(void) {
  NSString *data = DataDir();
  if (![data isEqualToString:[[NSBundle mainBundle] resourcePath]]) return data;
  NSString *docs = [NSHomeDirectory() stringByAppendingPathComponent:@"Documents"];
  [[NSFileManager defaultManager] createDirectoryAtPath:docs withIntermediateDirectories:YES attributes:nil error:NULL];
  return docs;
}

/* The arcade's attract movies (the .bik files in data/movie with their sound, converted by
 * prepare_movie.sh), indexed as app_movie() returns them. */
static NSString *const MOVIES[3] = { @"intro.mp4", @"playdemo.mp4", @"tutorial.mp4" };

/* One attract movie, played once. It takes no touches itself: GLView sees the tap. */
@interface IntroView : UIView {
  AVPlayer *player;
  id target;
  SEL done;
}
- (id)initWithFrame:(CGRect)f url:(NSURL *)url target:(id)t done:(SEL)d;
- (void)pause:(BOOL)p;
- (void)stop;
@end

@implementation IntroView
+ (Class)layerClass { return [AVPlayerLayer class]; }

- (id)initWithFrame:(CGRect)f url:(NSURL *)url target:(id)t done:(SEL)d {
  if ((self = [super initWithFrame:f])) {
    self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.userInteractionEnabled = NO;
    self.backgroundColor = [UIColor blackColor];
    target = t;
    done = d;
    player = [[AVPlayer alloc] initWithURL:url];
    player.actionAtItemEnd = AVPlayerActionAtItemEndPause;
    AVPlayerLayer *layer = (AVPlayerLayer *)self.layer;
    layer.videoGravity = AVLayerVideoGravityResizeAspect;
    layer.player = player;
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(ended:)
                                                 name:AVPlayerItemDidPlayToEndTimeNotification
                                               object:player.currentItem];
    [player play];
  }
  return self;
}

- (void)ended:(NSNotification *)n {
  [target performSelector:done];
}

- (void)pause:(BOOL)p {
  if (p) [player pause]; else [player play];
}

- (void)stop {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [player pause];
  ((AVPlayerLayer *)self.layer).player = nil;
}

- (void)dealloc {
  [player release];
  [super dealloc];
}
@end

/* The loading's thread (app_loader), with its GL context sharing the view's */
static void *Loader(void *context) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  [NSThread setThreadPriority:0.3]; /* below the frames' */
  if ([EAGLContext setCurrentContext:(EAGLContext *)context]) app_loader();
  [pool release];
  return NULL;
}

@interface GLView : UIView {
  EAGLContext *ctx;
  GLuint fb, color_rb, depth_rb;
  GLint fb_w, fb_h;
  CADisplayLink *link;
  CFTimeInterval last;
  BOOL started, loaded, failed;
  IntroView *intro;
  int movie;      /* index of the movie in `intro`, -1 none */
  BOOL movie_end; /* it ended before the game had loaded */
  const char *bgm; /* the BGM playing (app_bgm's name) */
  CMMotionManager *motion; /* the gravity for steering by tilt, while the OPTION screen asks */
  float touch_k, touch_edge; /* points to the 1024x768 layout's px (app_wide) */
}
@end

@implementation GLView
+ (Class)layerClass { return [CAEAGLLayer class]; }

- (id)initWithFrame:(CGRect)f {
  if ((self = [super initWithFrame:f])) {
    self.multipleTouchEnabled = YES;
    self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    /* render at 1024x768 rather than Retina 2048x1536: the SGX543MP4 can't fill 4x the pixels
     * (an iPhone 4's A4 renders at 480x320 so); the OPTION screen's RESOLUTION 2048 X 1536
     * (the options' 14th number, menu_state.hires) has a later iPad's Retina screen's */
    self.contentScaleFactor = 1.0f;
    FILE *opt = fopen([[SaveDir() stringByAppendingPathComponent:@"option"] fileSystemRepresentation], "r");
    if (opt) {
      int v[14], n = fscanf(opt, "option %d %d %d %d %d %d %d %d %d %d %d %d %d %d", &v[0], &v[1], &v[2], &v[3], &v[4],
                            &v[5], &v[6], &v[7], &v[8], &v[9], &v[10], &v[11], &v[12], &v[13]);
      fclose(opt);
      if (n == 14 && v[13] && [[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPad)
        self.contentScaleFactor = [UIScreen mainScreen].scale;
    }
    CAEAGLLayer *layer = (CAEAGLLayer *)self.layer;
    layer.opaque = YES;
    ctx = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    [EAGLContext setCurrentContext:ctx];
  }
  return self;
}

- (void)makeFramebuffer {
  [EAGLContext setCurrentContext:ctx];
  if (fb) {
    glDeleteFramebuffers(1, &fb);
    glDeleteRenderbuffers(1, &color_rb);
    glDeleteRenderbuffers(1, &depth_rb);
  }
  glGenFramebuffers(1, &fb);
  glBindFramebuffer(GL_FRAMEBUFFER, fb);
  glGenRenderbuffers(1, &color_rb);
  glBindRenderbuffer(GL_RENDERBUFFER, color_rb);
  [ctx renderbufferStorage:GL_RENDERBUFFER fromDrawable:(CAEAGLLayer *)self.layer];
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color_rb);
  glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &fb_w);
  glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &fb_h);
  glGenRenderbuffers(1, &depth_rb);
  glBindRenderbuffer(GL_RENDERBUFFER, depth_rb);
  /* depth and stencil in one (the stencil marks the cars' shadows, render_shadows) */
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, fb_w, fb_h);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_rb);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depth_rb);
  glBindRenderbuffer(GL_RENDERBUFFER, color_rb);
  fprintf(stderr, "fb %dx%d status 0x%x\n", fb_w, fb_h, glCheckFramebufferStatus(GL_FRAMEBUFFER));
}

- (void)layoutSubviews {
  fprintf(stderr, "layout %.0fx%.0f\n", self.bounds.size.width, self.bounds.size.height);
  [super layoutSubviews];
  CGSize s = self.bounds.size;
  if (s.width < s.height) return; /* wait for the landscape layout */
  [self makeFramebuffer];
  if (!started) {
    started = YES;
    movie = -1;
    bgm = NULL;
    /* the attract loop opens with the maker logos (app_movie says when a movie is due) */
    [self startGame];
  }
}

- (BOOL)showMovie:(int)k {
  NSString *path = [DataDir() stringByAppendingPathComponent:MOVIES[k]];
  if (![[NSFileManager defaultManager] fileExistsAtPath:path]) return NO;
  /* the intro's picture is letterboxed in its 4:3 (640x326 at y 76): on a wider screen (the
   * iPhone's) it goes big enough to cover the screen, its black bands and a little of its
   * sides off it; playdemo and tutorial fill their 4:3 and stay so */
  CGRect f = self.bounds;
  BOOL cover = k == 0 && f.size.width * 3 > f.size.height * 4;
  if (cover) {
    CGFloat sc = MAX(f.size.width / 640.0f, f.size.height / 326.0f), w = 640.0f * sc, h = 480.0f * sc;
    f = CGRectMake(f.origin.x + (f.size.width - w) / 2, f.origin.y + f.size.height / 2 - (76.0f + 163.0f) * sc, w, h);
  }
  intro = [[IntroView alloc] initWithFrame:f url:[NSURL fileURLWithPath:path]
                                    target:self done:@selector(movieEnded)];
  if (cover) intro.autoresizingMask = UIViewAutoresizingNone;
  [self addSubview:intro];
  movie = k;
  return YES;
}

- (void)closeMovie {
  [intro stop];
  [intro removeFromSuperview];
  [intro release];
  intro = nil;
  movie = -1;
}

- (void)movieEnded {
  if (!loaded) {
    movie_end = YES;
    return;
  }
  [self closeMovie];
  app_movie_done();
  [self syncMovie];
}

/* Put up or take down the movie the attract loop wants, and the BGM with it. */
- (void)syncMovie {
  int want = app_movie();
  if (want != movie) {
    if (intro) [self closeMovie];
    if (want >= 0 && ![self showMovie:want]) app_movie_done();
  }
  /* app_bgm's names are constants: a new pointer is a new track */
  const char *track = app_bgm();
  if (track != bgm) {
    bgm = track;
    snd_ios_bgm(track);
  }
  /* (the story ending's fade out) */
  static float level = 1.0f;
  float l = app_bgm_level();
  if (l != level) {
    level = l;
    snd_ios_bgm_level(l);
  }
}

- (void)startGame {
  CGSize s = self.bounds.size;
  app_save_dir([SaveDir() fileSystemRepresentation]);
  /* the game's 1024x768 layout, 768 high on screen; an iPhone's 3:2 is wider (its touch
   * keys bigger: its screen is small) */
  touch_k = 768.0f / (float)s.height;
  touch_edge = ((float)s.width * touch_k - 1024.0f) * 0.5f;
  if (touch_edge < 1.0f) touch_edge = 0;
  app_wide(touch_edge, touch_edge > 0 ? 1.3f : 1.0f);
  if (app_init([DataDir() fileSystemRepresentation], fb_w, fb_h, 1024, 768) != 0) {
    failed = YES;
    UIAlertView *a = [[UIAlertView alloc] initWithTitle:@"WMMT3DX"
                                                message:[NSString stringWithUTF8String:app_error()]
                                               delegate:nil cancelButtonTitle:@"OK" otherButtonTitles:nil];
    [a show];
    [a release];
    return;
  }
  snd_ios_start(DataDir());
  /* the course and the car models load on the A5X's other core (it keeps the context) */
  EAGLContext *load_ctx = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2 sharegroup:ctx.sharegroup];
  pthread_t loader;
  if (load_ctx && pthread_create(&loader, NULL, Loader, load_ctx) == 0) pthread_detach(loader);
  else fprintf(stderr, "no loading thread: the menu frames load\n");
  link = [CADisplayLink displayLinkWithTarget:self selector:@selector(frame:)];
  link.frameInterval = 1; /* 60 fps target */
  [link addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
  loaded = YES;
  if (movie_end) [self movieEnded];
  [self syncMovie];
}

/* the iPad turned like a wheel: gravity's angle in the screen's plane from the screen's down,
 * clockwise positive; nothing when it lies (near) flat */
- (void)tilt {
  if (!app_wants_tilt()) {
    if (motion.deviceMotionActive) [motion stopDeviceMotionUpdates];
    return;
  }
  if (!motion) motion = [[CMMotionManager alloc] init];
  if (!motion.deviceMotionAvailable) return;
  if (!motion.deviceMotionActive) {
    motion.deviceMotionUpdateInterval = 1.0 / 60.0;
    [motion startDeviceMotionUpdates];
  }
  CMDeviceMotion *m = motion.deviceMotion;
  if (!m) return;
  /* the screen's right and up in the device's axes: home on the right, its +x is up and -y
   * right; home on the left the other way */
  double gx = m.gravity.x, gy = m.gravity.y;
  if ([UIApplication sharedApplication].statusBarOrientation == UIInterfaceOrientationLandscapeLeft) gx = -gx, gy = -gy;
  app_tilt(gx * gx + gy * gy < 0.04 ? 0.0f : (float)atan2(-gy, -gx));
}

- (void)frame:(CADisplayLink *)l {
  CFTimeInterval now = l.timestamp;
  float dt = last > 0 ? (float)(now - last) : 1.0f / 30.0f;
  last = now;
  [self tilt];
  [EAGLContext setCurrentContext:ctx];
  glBindFramebuffer(GL_FRAMEBUFFER, fb);
  app_frame(dt);
  [self syncMovie];
  const GLenum discard[] = { GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT };
  glDiscardFramebufferEXT(GL_FRAMEBUFFER, 2, discard);
  glBindRenderbuffer(GL_RENDERBUFFER, color_rb);
  CFTimeInterval p0 = CACurrentMediaTime();
  [ctx presentRenderbuffer:GL_RENDERBUFFER];
  app_frame_done((CACurrentMediaTime() - p0) * 1000.0);
}

- (void)forward:(NSSet *)touches phase:(int)phase {
  for (UITouch *t in touches) {
    CGPoint p = [t locationInView:self];
    app_touch(t, phase, (float)p.x * touch_k - touch_edge, (float)p.y * touch_k);
  }
}
- (void)touchesBegan:(NSSet *)t withEvent:(UIEvent *)e {
  if (intro) {
    /* a tap on the movie is the arcade's "press the gas" */
    if (loaded && !failed) {
      app_press_start();
      [self syncMovie];
    }
    return;
  }
  [self forward:t phase:0];
}
- (void)touchesMoved:(NSSet *)t withEvent:(UIEvent *)e { [self forward:t phase:1]; }
- (void)touchesEnded:(NSSet *)t withEvent:(UIEvent *)e { [self forward:t phase:2]; }
- (void)touchesCancelled:(NSSet *)t withEvent:(UIEvent *)e { [self forward:t phase:2]; }

- (void)pause:(BOOL)p {
  [intro pause:p];
  link.paused = p;
  last = 0;
  if (started && !failed) {
    snd_ios_pause(p);
    app_suspend(p); /* no GL in the background, the loading thread's neither */
  }
}
@end

@interface GameController : UIViewController
@end
@implementation GameController
- (void)loadView {
  GLView *v = [[GLView alloc] initWithFrame:[UIScreen mainScreen].bounds];
  self.view = v;
  [v release];
}
- (BOOL)shouldAutorotate { return YES; }
- (UIInterfaceOrientationMask)supportedInterfaceOrientations { return UIInterfaceOrientationMaskLandscape; }
- (BOOL)prefersStatusBarHidden { return YES; }
@end

@interface AppDelegate : UIResponder <UIApplicationDelegate> {
  UIWindow *window;
  GameController *game;
  UIBackgroundTaskIdentifier away;
}
@end
@implementation AppDelegate
- (BOOL)application:(UIApplication *)a didFinishLaunchingWithOptions:(NSDictionary *)o {
  [a setStatusBarHidden:YES];
  [a setIdleTimerDisabled:YES];
  window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
  game = [[GameController alloc] init];
  window.rootViewController = game;
  [window makeKeyAndVisible];
  away = UIBackgroundTaskInvalid;
  return YES;
}
- (void)applicationWillResignActive:(UIApplication *)a { [(GLView *)game.view pause:YES]; }
- (void)applicationDidBecomeActive:(UIApplication *)a { [(GLView *)game.view pause:NO]; }
/* Run from /Applications, the app has its GL shader cache outside a sandbox
 * (/var/mobile/Library/Caches/com.wmmt.port/com.apple.opengl/shaders.maps), held locked,
 * and iOS kills an app it suspends holding such a lock (0xdead10cc): every trip to the
 * home screen ended the game. So in the background it is not suspended but stays up, frozen
 * (no frames, no GL, no loading), as long as iOS lets it, some ten minutes; then it quits. */
- (void)applicationDidEnterBackground:(UIApplication *)a {
  if (away == UIBackgroundTaskInvalid) away = [a beginBackgroundTaskWithExpirationHandler:^{ exit(0); }];
}
- (void)applicationWillEnterForeground:(UIApplication *)a {
  if (away != UIBackgroundTaskInvalid) [a endBackgroundTask:away];
  away = UIBackgroundTaskInvalid;
}
- (void)applicationDidReceiveMemoryWarning:(UIApplication *)a { app_memory_warning(); }
@end

int main(int argc, char *argv[]) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  freopen([[SaveDir() stringByAppendingPathComponent:@"log.txt"] fileSystemRepresentation], "w", stderr);
  setvbuf(stderr, NULL, _IONBF, 0);
  fprintf(stderr, "start\n");
  int r = UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
  [pool release];
  return r;
}
