#include "app.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>
/* (the NDK's zconf.h defines far and near away, for 16-bit DOS) */
#undef far
#undef near
#include <sys/stat.h>
#include <time.h>
#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/mach_time.h>
#else
#include <time.h>
#endif

#include "anm.h"
#include "car.h"
#include "cardb.h"
#include "carwheel.h"
#include "rivaldb.h"
#include "tendb.h"
#include "storycity.h"
#include "storymovie.h"
#include "storyinfo.h"
#include "gl.h"
#include "hud.h"
#include "lights.h"
#include "spark.h"
#include "mathx.h"
#include "menu.h"
#include "minicourse.h"
#include "gk.h"
#include "net.h"
#include "vs_host.h"
#include "render.h"
#include "sound.h"
#include "title.h"

#define MAX_ARROWS 128

/* The arcade's maps by city (car.h CAR_TOKYO..) and time zone (menu_state.zone): Tokyo's
 * morning (other_stage/c1_daytime.lua) and night (tokyo/night/night.lua), packed by
 * tools/stage/pack_tokyo.sh, the other cities' .lua files in data/<city>/daytime and night by
 * tools/stage/pack_city.py; each with its sky box, junction arrow boards (arrows*.wpk) and
 * street lights (lights*.bin, tools/stage/pack_lights.py) and its minimap (hud_set_city).
 * Only the chosen map is in GL at a time. */
static const char *const CITY_FILE[CAR_CITIES] = { "tokyo", "oosaka", "hakone", "nagoya", "fukuoka" };
/* their FogParam (the global fog: far, near, colour) */
static const struct {
  float far, near, color[3];
} FOG[CAR_CITIES][2] = {
  { { 35000.0f, 100.0f, { 0.6f, 0.75f, 0.8f } }, { 25000.0f, 250.0f, { 0.6f, 0.75f, 0.8f } } },
  { { 25000.0f, 1.0f, { 0.88f, 0.88f, 0.905f } }, { 25000.0f, 250.0f, { 0.3f, 0.3f, 0.3f } } },
  { { 15000.0f, 100.0f, { 0.6f, 0.7f, 0.75f } }, { 2000.0f * 1.8f, 250.0f, { 0.1f, 0.1f, 0.1f } } },
  { { 25000.0f, 1.0f, { 0.88f, 0.88f, 0.905f } }, { 25000.0f, 1.0f, { 0.3f, 0.3f, 0.3f } } },
  { { 16000.0f, 1.0f, { 0.0f, 0.0f, 0.0f } }, { 25000.0f, 1.0f, { 0.3f, 0.3f, 0.3f } } },
};
/* the port's own grade of a map and its sky (render_params.grade; 0 none): Hakone's morning
 * in a brighter, warmer sun, its night darker under a cool moon, the lamps and the cars
 * as they are (Allan, 10-26: one multiply in the scene programs, nothing for the A5X) */
static const float MAP_GRADE[CAR_CITIES][2][3] = {
  [CAR_HAKONE] = { { 1.16f, 1.12f, 1.02f }, { 0.72f, 0.74f, 0.82f } },
};
/* the cars' lights (render_params light_*), data/car_light.lua by course and time zone
 * (clGraphicFrame::initCarLight): the sun and the "headlight", the scene ambient for every
 * car; its material (diffuse 1, ambient 0.9) lights all but the car.cg / car_blend.cg parts */
typedef struct {
  float dir[2][3], diffuse[2], ambient, mat_diffuse, mat_ambient;
} car_light;
static const car_light CAR_LIGHT[CAR_CITIES][2] = {
  { /* tokyo-daytime, tokyo-night */
    { { { -0.5f, -1.0f, -0.3f }, { 0.0f, -1.0f, 0.0f } }, { 0.62f, 0.52f }, 0.35f, 1.0f, 0.9f },
    { { { 0.5f, -0.15f, -0.3f }, { 0.0f, -1.0f, -0.2f } }, { 0.73f, 0.55f }, 0.32f, 1.0f, 0.9f } },
  { /* oosaka */
    { { { -0.5f, -1.0f, -0.3f }, { 0.0f, -1.0f, 0.0f } }, { 0.62f, 0.53f }, 0.35f, 1.0f, 0.9f },
    { { { 0.5f, -0.2f, -0.3f }, { 0.0f, -1.0f, -0.2f } }, { 0.73f, 0.54f }, 0.29f, 1.0f, 0.9f } },
  { /* hakone */
    { { { 0.5f, -1.0f, -0.3f }, { 0.0f, -1.0f, 0.0f } }, { 0.62f, 0.45f }, 0.31f, 1.0f, 0.9f },
    { { { 0.5f, -1.0f, -0.3f }, { 0.0f, -1.0f, 0.0f } }, { 0.79f, 0.66f }, 0.14f, 1.0f, 0.9f } },
  { /* nagoya */
    { { { -0.5f, -1.0f, -0.3f }, { 0.0f, -1.0f, 0.0f } }, { 0.62f, 0.53f }, 0.35f, 1.0f, 0.9f },
    { { { 0.5f, -0.2f, -0.3f }, { 0.0f, -1.0f, -0.2f } }, { 0.73f, 0.54f }, 0.29f, 1.0f, 0.9f } },
  { /* fukuoka */
    { { { -0.5f, -1.0f, -0.3f }, { 0.0f, -1.0f, 0.0f } }, { 0.62f, 0.53f }, 0.35f, 1.0f, 0.9f },
    { { { 0.5f, -0.2f, -0.3f }, { 0.0f, -1.0f, -0.2f } }, { 0.73f, 0.54f }, 0.29f, 1.0f, 0.9f } },
};
/* the menus' (clMenu3dEnvironment): data/menu/menu_4.lua's first two lights (car.cg reads
 * two), the ambient addRender sets (0.1), GL's default material */
static const car_light MENU_LIGHT = { { { 1.0f, -0.8f, -0.9f }, { -1.0f, -0.7f, -0.8f } }, { 0.85f, 0.8f }, 0.1f, 0.8f, 0.2f };
/* the other environments' (clMenu3dEnvironment(n): data/menu/menu_<n>.lua, 0 menu.lua) the
 * dress-up screens and the story tuning work (8) use: their first two lights, the rest as menu_4's */
static const struct {
  int env;
  car_light light;
} MENU_ENV_LIGHT[] = {
  { 0, { { { -0.3f, -0.7f, -1.0f }, { 1.0f, -0.7f, -0.3f } }, { 0.9f, 0.7f }, 0.1f, 0.8f, 0.2f } },
  { 3, { { { 0.2f, -0.2f, -0.9f }, { -0.6f, -0.2f, -0.9f } }, { 0.9f, 0.75f }, 0.1f, 0.8f, 0.2f } },
  { 5, { { { 0.7f, -0.8f, 1.0f }, { -0.3f, -0.2f, 1.0f } }, { 1.0f, 1.0f }, 0.1f, 0.8f, 0.2f } },
  /* the wheel change's (seen from behind: menu_4's lights left it black) */
  { 7, { { { -0.8f, -0.8f, 1.0f }, { 0.7f, -0.2f, 0.9f } }, { 1.0f, 0.8f }, 0.1f, 0.8f, 0.2f } },
  { 8, { { { -0.9f, -0.8f, 0.7f }, { 1.0f, -0.7f, 0.8f } }, { 0.9f, 0.7f }, 0.1f, 0.8f, 0.2f } },
  { 11, { { { 0.4f, -0.2f, -1.0f }, { -0.7f, -0.8f, -1.0f } }, { 1.0f, 0.9f }, 0.1f, 0.8f, 0.2f } },
  { 13, { { { -0.8f, -0.8f, -1.0f }, { 0.0f, -0.2f, 1.0f } }, { 1.0f, 1.0f }, 0.1f, 0.8f, 0.2f } },
  { 14, { { { 0.0f, -1.0f, -1.0f }, { 1.0f, -1.0f, 0.0f } }, { 0.9f, 0.5f }, 0.1f, 0.8f, 0.2f } },
  { 16, { { { 0.4f, -0.3f, 1.0f }, { -0.7f, -0.9f, 0.9f } }, { 1.0f, 0.9f }, 0.1f, 0.8f, 0.2f } },
};

static const car_light *menu_env_light(int env) {
  for (size_t i = 0; i < sizeof MENU_ENV_LIGHT / sizeof MENU_ENV_LIGHT[0]; ++i)
    if (MENU_ENV_LIGHT[i].env == env) return &MENU_ENV_LIGHT[i].light;
  return &MENU_LIGHT;
}

static void set_car_light(render_params *rp, const car_light *l, const mat4 *view) {
  for (int k = 0; k < 3; ++k) {
    for (int i = 0; i < 2; ++i) {
      rp->light_dir[i][k] = l->dir[i][k];
      rp->light_diffuse[i][k] = l->diffuse[i];
    }
    rp->ambient[k] = l->ambient;
    rp->material_diffuse[k] = l->mat_diffuse;
    rp->material_ambient[k] = l->mat_ambient;
  }
  rp->view = *view;
}

/* clMapDisp::initSkyBox puts the sky box 1000 m down */
#define SKY_DROP 10000.0f

/* car models: the car select's (data/cars, CARDB_PACKS), then the story rivals' own
 * (data/chara, RIVALDB_CHARA) */
#define NPACKS (CARDB_NPACKS + RIVALDB_NCHARA)

/* what the menus want loaded, for the loading (load_wants_now) */
typedef struct {
  int order[NPACKS], n; /* car models, most wanted first */
  int first;                  /* how many of them come before the course */
  int cars_before_warm;       /* the rest before warming the course (in the car select) */
  int paints[NPACKS], n_paints; /* car models whose other paints are wanted (the car under the cursor) */
  int paints_first;           /* those come before the course too (in the car select) */
  int hold_track;             /* the last map is still being freed: the next waits */
  int course;                 /* the course chosen: the map warms from its start out (warm_step) */
} load_wants;

/* what is loaded, from the loading to the menus (load_sync) */
typedef struct {
  signed char pack[NPACKS];  /* 1 in GL, -1 missing, 0 not (yet) */
  signed char paint[NPACKS]; /* 1 its other paints are in GL too */
  int track;                       /* 1 all of the map is in GL, 2 and first-drawn (warm_step) */
  int start;                       /* course + 1 whose start the race's first frames find warm (warm_step) */
} load_news;

#define CAR_BEST_MAX 512 /* car types' records kept (47 cars, 6 courses) */
/* the dress-up wheels kept in GL: the change screen's place, those either side, its colours */
#define WHEEL_SLOTS 8
#define MENU_AERO_SLOTS 4
/* a car's dress-up as a versus profile sends it (menu_state's of the car) */
typedef struct {
  int aero, mirror, wing, bonnet, wheel, neon, vinyl;
} vs_dress_t;
enum { WHEEL_FREE, WHEEL_READY, WHEEL_UP, WHEEL_WARM, WHEEL_MISSING };

static struct {
  wpk_scene track; /* the map of track_city and track_zone */
  int track_city, track_zone;
  int extras_city; /* the city whose sky boxes, arrow boards, lights and minimap are loaded (-1 none) */
  /* render_hq up (Android); the OPTION screen's graphics switches last applied (hq_apply),
   * the maps' ASTC packs (*.astc.wpk) in use */
  int hq_on, hq_bloom, hq_ssao, hq_env, hq_ssao_made, hd_maps, hd_reload;
  wpk_scene track_old; /* the other time zone's, freed a step a menu frame (track_open) */
  wpk_scene sky[2];
  /* the car select's models (data/cars, tools/stage/pack_cars.py), loaded as the menus need
   * them; fd3s.wpk stands in for any that is missing */
  wpk_scene fd3s, packs[NPACKS];
  signed char pack_state[NPACKS]; /* 0 not loaded, 2 going up, 3 first drawing, 1 loaded, -1 missing */
  signed char paint_state[NPACKS]; /* its other paints (wpk_upload_paints): 0 not, 2 going up, 3 first drawing, 1 in GL */
  int low_memory; /* iOS warned: keep only the car models near the maker on show */
  wpk_scene *race_model;
  wpk_scene aero;  /* the player's car in its dress-up aero set, on race_model's textures */
  int aero_pack, aero_set; /* what aero holds (aero_set 0: nothing) */
  /* the dress-up screens' cars in their aero sets A..C (menu_aero): a car pack and set each
 * (the versus' cars flying in may be of several) */
  wpk_scene menu_aero[MENU_AERO_SLOTS];
  int menu_aero_pack[MENU_AERO_SLOTS], menu_aero_set[MENU_AERO_SLOTS], menu_aero_in[MENU_AERO_SLOTS];
  /* a versus' other cars dressed up as their profiles say (vs_dress; their aero sets in
   * menu_aero's slots, menu_aero_want) */
  vs_dress_t rival_dress[CAR_RIVALS];
  /* the dress-up wheels in GL (wheel_model, wheel_sync): menu_state.wheel's value each
   * holds, how far it is (WHEEL_*), when last wanted; the one the menus last showed */
  wpk_scene wheel[WHEEL_SLOTS];
  int wheel_id[WHEEL_SLOTS], wheel_state[WHEEL_SLOTS];
  unsigned wheel_used[WHEEL_SLOTS], wheel_tick;
  int wheel_shown;
  /* the wheel GET screen's model (get_wheel_model), the same way */
  wpk_scene get_wheel;
  int get_wheel_id, get_wheel_ok;
  /* the traffic's models (tools/stage/pack_traffic.sh), by CAR_TRAFFIC_* */
  wpk_scene traffic[CAR_TRAFFIC_MODELS];
  char data_dir[1024];
  int options_global; /* the OPTION screen's choices from save_dir/option (options_load) */
  char save_dir[1024]; /* where perf.log, the saves (records<n>, garage<n>, slot) and timezone go (app_save_dir) */
  /* the arcade's junction arrow boards (coursearrow), one object per junction block, by time zone */
  wpk_scene arrows[2];
  char arrow_names[MAX_ARROWS][48];
  int n_arrows;
  unsigned char arrow_visible[MAX_ARROWS];
  unsigned char arrow_switched[MAX_ARROWS]; /* named in the junction block table */
  int hk_arrow; /* Hakone's board shown: 0 none, 1 hk_for, 2 hk_back */
  int arrow_table_ready;
  /* the course selects' 3D miniature of Tokyo (clMiniatureCourse, src/minicourse.h) */
  wpk_scene mini;
  unsigned char mini_visible[MINI_OBJECTS];
  const mini_course *mini_shown; /* the card it shows (NULL off the course selects) */
  float mini_from[2][3];         /* the camera and aim it moves from */
  int mini_move, mini_frame;
  car_state car;
  hud_state hud;
  menu_state menu;
  int in_menu; /* the front end is up; the race starts once a course is chosen */
  int paused;  /* the race is frozen under its pause menu */
  int track_ready; /* as load_news.track: the loading's (the menus see seen.track) */
  int start_ready; /* as load_news.start */
  int pre_wait;    /* frames the race's title page has waited for the loading */
  int pre_frames;  /* and has been up (perf.log) */
  int count_shown; /* the start's countdown as the console last showed it (app_frame) */
  /* the loading (load_unit), by the menu frames or by a thread of its own (app_loader) */
  pthread_mutex_t load_mx;
  pthread_cond_t load_cv;
  int loader_up; /* the thread is there (it says so itself) */
  int loader;    /* the menus know it is: the frames leave the loading to it */
  int load_run;  /* it may load: the menus are up and so is the app */
  int load_busy; /* it is in a load_unit */
  int load_idle; /* it found nothing to do and waits for the menus' next frame */
  double frame_start; /* the menu frame's (app_frame) */
  double load_until;  /* the thread loads between a menu frame's end (app_frame_done) and this */
  int load_hurry;     /* the menus wait for the loading (NOW LOADING, the race title, the boot logos) */
  int load_wait;      /* the car select's NOW LOADING: nothing on show but it */
  int load_units;     /* the thread's steps and their time, for perf.log (under load_mx) */
  double load_ms;
  int away;      /* the app is in the background, where iOS allows no GL */
  load_wants wants;
  load_news pub, seen; /* as the loading has it, and the menus' copy */
  render_params rp;
  int fb_w, fb_h;
  int camera; /* 0 the arcade's view behind the car, 1 its roof view */
  float fps;
  char error[512];
  /* frame timing of the race, for hunting hitches: perf.log in the data folder */
  FILE *perf;
  double race_time, max_phys, max_render;
  int perf_frames, odd_steps, slow_frames;
  /* render_hq's governor: a window's frames, those that missed; the level, the lowest it
   * may go back up to, the frames since it last went up */
  int gov_frames, gov_slow, gov_level, gov_floor, gov_since_up;
  double present_ms; /* the last frame's wait in presentRenderbuffer (app_frame_done) */
  double seg[8];     /* the race frame's render, by part (perf_frame) */
  int race_frames; /* frames since the race started */
  /* the overhead reflection (render_overhead, not the arcade's): off for a test
   * (WMMT_NO_OVERHEAD); this frame's map drawn for the player's car, its batches and ms */
  int no_over, over_drawn, over_draws, over_tris;
  int bm_shown, bm_draws, bm_tris; /* the back mirror this frame (draw_backmirror) */
  double bm_ms;
  double over_ms;
  float tilt; /* the iPad's turn about the screen's normal (app_tilt), radians */
  int no_sharp; /* test: WMMT_NO_SHARPEN */
  int ao_test;  /* test: WMMT_AO=0 or 1 over the OPTION screen's (-1 none) */
  /* an iPhone (app_wide): the screen edge layout px wider than 4:3 on each side, and the
   * A4's cuts (phone): no sharpening, overhead map, radial blur or map gloss, shorter
   * building ranges, the traffic only near, few car models kept */
  float edge;
  int phone;
  int console; /* app_console: a phone of today's (Android), everything on and further */
  int sharp_rest, sharp_rested; /* frames the sharpening sits out yet; of perf_frame's 600 */
  int time_shown;  /* the whole seconds left the console shows, for its ticks */
  int ten_rival_on; /* the 10-in-a-row's rival its approach sound was played for (-1: none yet) */
  int ten_freeze;   /* frames left of a beaten 10-in-a-row rival's still, negative frame */
  int ten_beaten_on; /* that frame's play state 9 has been seen (till it goes) */
  int ten_bgm_off;  /* the last rival is beaten: no race BGM */
  int ten_white;    /* frames since the 10-in-a-row's white went up (0: none) */
  int sign, sign_t; /* the route sign over the car, and frames to its next sound */
  int best_ms[CAR_COURSES]; /* the player's best time on each course (data_dir/records), 0 = none */
  /* the machine's best time of each car type (model pack) on each course, from data_dir/records */
  struct { int course, pack, ms; } car_best[CAR_BEST_MAX];
  int n_car_best;
  int race_pack;   /* the race's car (CARDB pack) */
  /* the titles (src/title.h): the card before the race (judgeTitle's PRE) and its mileage then */
  title_card title_pre;
  double title_odo0;
  /* the CPU rivals: the race's first (RIVALDB, -1 none; the others are the rows after it,
   * rival_rows), and each one's car model and paint (rival_look) */
  int rival, n_rival, rival_pack[CAR_RIVALS], rival_paint[CAR_RIVALS];
  int rival_env;   /* a test's rival (env WMMT_RIVAL), -1 none */
  int story_race;  /* the select's race to run next (0 the first; some selects have two) */
  /* a story select's realtime movie before its race (car_movie_start): frames run and its
   * length (0: none playing), how it fades out, the race to start after it, and the models
   * of its cars 1..3 */
  float movie_frame, movie_len;
  int movie_fade, movie_in;
  /* a select's race after its first: frames left of its fade in (the previous race's
   * STORYRACE_FADE: clRaceThread::run's setFade(0, 1, 1/3)) over its movie, and that fade */
  int period_in, period_fade;
  car_spec race_spec;
  car_rival race_rv[CAR_RIVALS];
  int race_nrv, race_course;
  wpk_scene *movie_model[CAR_RIVALS];
  int movie_paint[CAR_RIVALS];
  wpk_scene *rival_model[CAR_RIVALS];
  /* the 10-in-a-row's rivals in order (its one rival on the road changes each round): their
   * car models and paints */
  int n_ten, ten_pack[CAR_TEN_RIVALS], ten_paint[CAR_TEN_RIVALS];
  float top_kmh;   /* the race's top speed so far */
  /* the online versus (net.c): this app's id at the server, when it asked to connect, the race
   * the host started (net_take_go), and the other cars in the race on the road (0: no versus;
   * they are rivals 0.. in PCB order) */
  uint32_t vs_id;
  double vs_asked;
  /* the area vote, as its host: when it opened, when the result was out; the time zone vote's
  * the same and when the race was asked for */
  double vs_area_at, vs_final_at;
  double vs_day_at, vs_day_final_at, vs_day_started;
  /* the race loaded: frames waited for the others', the machines (by PCB) seen loaded */
  int vs_wait, vs_seen_loaded;
  int vs_gk; /* the Bluetooth session: VS_GK_OFF .. VS_GK_HOST */
  net_go vs_go;
  int vs_race;
  int vs_song;      /* the host's: its song for everyone's race */
  int vs_closed;    /* a race's frames since the host closed the game (or went quiet): its notice */
} G;

static void vs_ip_load(void);

/* iOS 6 has no clock_gettime */
static double now_ms(void) {
#ifdef __APPLE__
  static mach_timebase_info_data_t tb;
  if (!tb.denom) mach_timebase_info(&tb);
  return (double)mach_absolute_time() * tb.numer / tb.denom * 1e-6;
#else
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (double)t.tv_sec * 1e3 + (double)t.tv_nsec * 1e-6;
#endif
}

/* the time a menu frame may take with its loading (the course, car models), from its start,
 * the GPU's part included (load_step); more on the race's title page, which is there for
 * the loading, and in the maker logos the app boots into, which cannot be skipped for it
 * (menu_booting): both then run at an even 30 fps. And the loading's least share, however
 * long the menu's own drawing took. */
#define LOAD_MS 12.0
#define LOAD_PRE_MS 24.0
#define LOAD_MIN_MS 3.0
/* the race's own share for first-drawing the course ahead (wpk_prewarm), and how far ahead */
#define WARM_MS 2.0
#define WARM_MARGIN 6000.0f

/* where each course's race starts: the camera's place at its first frame (car_start, the
 * same on both time zones' maps). The loading first-draws the map from there out. */
static const float START[CAR_COURSES][3] = {
  { 3047, 258, -27446 }, { 822, 124, 5482 }, { 35228, 18, 29159 },
  { 647, 124, 5496 }, { 35371, 18, 29427 }, { -38825, 52, 151331 },
  { -2988, 173, -11315 }, { 7821, 123, -7066 }, { -48658, 123, 1563 },
  { -16265, 103, -48750 }, { -4147, 53, 25205 }, { 10909, -77, 46167 },
};

/* the build, for telling the pushes apart in perf.log */
#define PORT_BUILD "push164"
#define PHONE_TRAFFIC_FAR 3000.0f /* an iPhone draws the traffic this near (0.1 m, as G.rp.range) */

/* the app's resident memory in MB (iOS 6 warns the app around 300 to 400 on the iPad 3) */
static double resident_mb(void) {
#ifdef __APPLE__
  struct task_basic_info info;
  mach_msg_type_number_t n = TASK_BASIC_INFO_COUNT;
  if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&info, &n) == KERN_SUCCESS)
    return (double)info.resident_size / 1048576.0;
#elif defined(__ANDROID__)
  FILE *f = fopen("/proc/self/statm", "r");
  long pages = 0, rss = 0;
  if (f) {
    if (fscanf(f, "%ld %ld", &pages, &rss) != 2) rss = 0;
    fclose(f);
  }
  return (double)rss * 4096.0 / 1048576.0;
#endif
  return 0;
}

static void perf_file(void) {
  char path[1200];
  snprintf(path, sizeof path, "%s/perf.log", G.save_dir);
  if (!G.perf) G.perf = fopen(path, "a");
  if (!G.perf) G.perf = fopen("/tmp/wmmt_perf.log", "a");
}

/* how much of the course the menus left cold (wpk_prewarm) */
static int cold_chunks(void) {
  int cold = 0;
  for (uint32_t c = 0; G.track.warm && c < G.track.n_chunk; ++c) cold += !G.track.warm[c];
  return cold;
}

static void perf_open(void) {
  perf_file();
  if (G.perf)
    fprintf(G.perf, "--- race, course %d, zone %d, %d of %u chunks cold, title page %.1f s, %.0f MB resident (%s)\n",
            G.menu.course, G.track_zone, cold_chunks(), G.track.n_chunk, G.pre_frames / 60.0, resident_mb(), PORT_BUILD);
  G.pre_frames = 0;
  G.race_time = G.max_phys = G.max_render = 0;
  G.perf_frames = G.odd_steps = G.slow_frames = 0;
  G.race_frames = 0;
}

/* how much the overhead map takes over the player's car's environment where it has something */
#define OVER_AMOUNT 0.9f
/* the OPTION screen's FSR sharpening (RCAS's sharpness: 1 most, each stop down halves it) */
#define SHARP_AMOUNT 0.8f
/* after a race frame that missed its vsync the sharpening's pass (the GPU's to spare: some
 * 3 ms of the A5X's) sits out this many frames, so a heavy stretch of the road keeps 60 */
#define SHARP_REST 120

/* the parts of a race frame's render (G.seg) */
enum { SEG_SKY, SEG_OPAQUE, SEG_CUTOUT, SEG_BLEND, SEG_LIGHTS, SEG_WARM, SEG_BLUR, SEG_HUD, SEGS };

/* A line for each frame that missed its vsync or ran slow, and a summary every 10 s. The
 * slow ones say where the render's time went and how long the frame before waited for the
 * GPU in presentRenderbuffer. */
static void perf_frame(float dt, int steps, double phys, double render, int warmed, const render_params *rp) {
  G.race_time += dt;
  if (phys > G.max_phys) G.max_phys = phys;
  if (render > G.max_render) G.max_render = render;
  if (steps != 1) G.odd_steps++;
  int slow = dt > 1.5f / 60.0f || phys + render > 12.0;
  if (slow) G.slow_frames++;
  /* 1080p at 60 before any effect (Allan's): over 3 of 120 frames missing their vsync (a
   * chunk's first draw aside) gives up the next of render_hq's costs; 1200 frames without a
   * miss take one back, unless the level above missed again soon after its last return */
  if (G.console) {
    G.gov_slow += dt > 1.25f / 60.0f && !rp->new_chunks && !warmed;
    if (G.gov_since_up) G.gov_since_up++; /* (0: never went up) */
    int window = G.gov_slow ? 120 : 1200, level = G.gov_level;
    if (++G.gov_frames >= window) {
      if (G.gov_slow > 3) {
        if (G.gov_since_up && G.gov_since_up < 1800) G.gov_floor = level + 1; /* (it went up too soon) */
        level++;
      } else if (!G.gov_slow && level > G.gov_floor) {
        level--, G.gov_since_up = 1;
      }
      G.gov_frames = G.gov_slow = 0;
    }
    if (level != G.gov_level) {
      G.gov_level = render_hq_level(level);
      if (G.perf) fprintf(G.perf, "%7.2fs render_hq level %d (floor %d)\n", G.race_time, G.gov_level, G.gov_floor);
    }
  }
  if (!G.perf) return;
  if (slow || rp->new_chunks || warmed)
    fprintf(G.perf,
            "%7.2fs dt %5.1f ms steps %d phys %5.2f ms draw %5.2f ms first drawn %d warmed %d draws %d tris %d pos %.0f %.0f"
            " kmh %.0f | present %.1f over %.1f/%d/%d bm %.1f/%d/%d sky %.1f opq %.1f cut %.1f bl %.1f lights %.1f warm %.1f blur %.1f hud %.1f\n",
            G.race_time, dt * 1e3f, steps, phys, render, rp->new_chunks, warmed, rp->draws, rp->triangles,
            G.car.pos[0], G.car.pos[2], car_kmh(&G.car), G.present_ms, G.over_ms, G.over_draws, G.over_tris, G.bm_ms, G.bm_draws, G.bm_tris, G.seg[SEG_SKY], G.seg[SEG_OPAQUE],
            G.seg[SEG_CUTOUT], G.seg[SEG_BLEND], G.seg[SEG_LIGHTS], G.seg[SEG_WARM], G.seg[SEG_BLUR], G.seg[SEG_HUD]);
  if (++G.perf_frames == 600) {
    char warm[256];
    wpk_warm_stats(warm, sizeof warm);
    fprintf(G.perf, "%7.2fs 600 frames: %d slow, %d not one step, max phys %.2f ms, max draw %.2f ms, %d unsharpened; %s\n",
            G.race_time, G.slow_frames, G.odd_steps, G.max_phys, G.max_render, G.sharp_rested, warm);
    fflush(G.perf);
    G.perf_frames = G.odd_steps = G.slow_frames = G.sharp_rested = 0;
    G.max_phys = G.max_render = 0;
  }
}

/* The menus' frame times, a line every 10 s: the screen, what is loaded, the frames that
 * missed their vsync and the longest, the menu frames' own time (cpu: their code and GL
 * calls) and the loading thread's steps and time, the app's memory. */
static void perf_menu(float dt, double cpu) {
  static int frames, slow, slow_movie;
  static float longest;
  static double cpu_sum, cpu_max, present_sum, present_max;
  frames++;
  /* the last frame's wait in presentRenderbuffer (the GPU's share) and slow frames under a movie */
  if (dt > 1.5f / 60.0f) slow++, slow_movie += app_movie() >= 0;
  if (dt > longest) longest = dt;
  cpu_sum += cpu;
  if (cpu > cpu_max) cpu_max = cpu;
  present_sum += G.present_ms;
  if (G.present_ms > present_max) present_max = G.present_ms;
  if (frames < 600) return;
  perf_file();
  int cars = 0, paints = 0;
  for (int k = 0; k < CARDB_NPACKS; ++k) {
    cars += G.seen.pack[k] == 1;
    paints += G.seen.paint[k] == 1;
  }
  pthread_mutex_lock(&G.load_mx);
  int units = G.load_units;
  double load_ms = G.load_ms;
  G.load_units = 0;
  G.load_ms = 0;
  pthread_mutex_unlock(&G.load_mx);
  if (G.perf) {
    char warm[256];
    wpk_warm_stats(warm, sizeof warm);
    fprintf(G.perf,
            "menu screen %d: course %s%s, %d of %u chunks cold, %d of %d car models, %d with paints (%s); 600 frames: %d slow, "
            "longest %.1f ms, %d under a movie; menu cpu %.1f avg %.1f max ms; present %.1f avg %.1f max ms; loading %d steps %.0f ms; %s; %.0f MB (%s)\n",
            G.menu.screen, G.seen.track == 2 ? "warm" : G.seen.track ? "up" : "loading",
            G.seen.start == G.menu.course + 1 ? " (start warm)" : "", cold_chunks(), G.track.n_chunk, cars, CARDB_NPACKS,
            paints, G.loader ? "loading thread" : "in the frames", slow, longest * 1e3f, slow_movie, cpu_sum / frames, cpu_max,
            present_sum / frames, present_max, units,
            load_ms, warm, resident_mb(), PORT_BUILD);
    fflush(G.perf);
  }
  frames = slow = slow_movie = 0;
  longest = 0;
  cpu_sum = cpu_max = present_sum = present_max = 0;
}

const char *app_error(void) { return G.error; }

/* the patched iOS SDK has no plain strcmp for armv7 */
/* (the junction table's Osaka and Hakone names keep their scene prefix, "yazirushi_all:") */
static int same_name(const char *a, const char *b) {
  for (const char *q = b; *q; ++q)
    if (*q == ':') b = q + 1;
  while (*a && *a == *b) a++, b++;
  return *a == *b;
}

static void load_pause(void);
static void track_set(int ready);

/* Make zone's map the one going up to GL, from the next load_unit on. The other's goes to
 * track_old, which the menu frames free a step at a time (its ten thousand buffers and
 * textures at once held the time zone select for a long moment); the new one waits for that,
 * as the iPad has no room for both. */
/* data_dir/<kind>[_<city>][_night].<ext>: Tokyo's files have no city in their names but its
 * map's (tokyo.wpk) */
static void city_path(char *out, size_t n, const char *kind, int city, int zone, const char *ext) {
  snprintf(out, n, "%s/%s%s%s%s.%s", G.data_dir, kind, city && *kind ? "_" : "", city || !*kind ? CITY_FILE[city] : "",
           zone ? "_night" : "", ext);
  /* the OPTION screen's HD TEXTURES (Android): a map's pack AI-upscaled into ASTC
   * (upscale_tools, render.c astc_upload) where there is one, <name>.astc.wpk */
  size_t len = strlen(out);
  if (G.hd_maps && same_name(ext, "wpk") && len > 4 && len + 5 < n) {
    char hd[1200];
    snprintf(hd, sizeof hd, "%.*s.astc.wpk", (int)(len - 4), out);
    if (access(hd, F_OK) == 0) snprintf(out, n, "%s", hd);
  }
}

/* G.error for a pack that would not open: the file, and why (the player's copy of the data is
 * what is usually wrong: not there, not readable by the app, or cut short) */
static void load_error(const char *path) {
  struct stat st;
  char why[200];
  unsigned char m[4];
  int fd;
  if (stat(path, &st) != 0)
    snprintf(why, sizeof why, "%s", errno == ENOENT ? "the file is not there" : strerror(errno));
  else if ((fd = open(path, O_RDONLY)) < 0)
    snprintf(why, sizeof why, "the app may not read it (%s): uninstall, install the app, open it once, then copy the data again", strerror(errno));
  else {
    ssize_t n = read(fd, m, 4);
    close(fd);
    if (n != 4 || memcmp(m, "WPK", 3) != 0)
      snprintf(why, sizeof why, "not a WPK file (%lld bytes): damaged or not fully copied, copy it again", (long long)st.st_size);
    else
      snprintf(why, sizeof why, "damaged or not fully copied (%lld bytes), copy it again", (long long)st.st_size);
  }
  snprintf(G.error, sizeof G.error, "cannot load %s: %s", path, why);
}

/* A city's sky boxes, arrow boards (with their names, the scene file's prefix "...:" left
 * out), lights and minimap, those of the other city freed (small: a step at once) */
static void extras_open(int city) {
  char path[1200];
  if (city == G.extras_city) return;
  G.extras_city = city;
  for (int z = 0; z < 2; ++z) {
    wpk_free(&G.sky[z]);
    wpk_free(&G.arrows[z]);
    memset(&G.sky[z], 0, sizeof G.sky[z]);
    memset(&G.arrows[z], 0, sizeof G.arrows[z]);
    city_path(path, sizeof path, "sky", city, z, "wpk");
    if (wpk_load(&G.sky[z], path) == 0) G.sky[z].two_sided = G.sky[z].solid = 1;
    else fprintf(stderr, "no %s, no sky\n", path);
    /* Hakone's morning dome is only the sun's glow: black where its alpha is 0 (all but the
     * glow), over the sky's colour (the clear, the fog's) blended, not drawn black */
    wpk_blend_texture(&G.sky[z], "hk_sky01_a.tga");
    city_path(path, sizeof path, "arrows", city, z, "wpk");
    if (wpk_load(&G.arrows[z], path) != 0) fprintf(stderr, "no %s, no junction arrow boards\n", path);
  }
  /* the junction table's flags are by board index: a city's own (another city's left the
   * boards of the same indices hidden but at their junctions, push87) */
  G.n_arrows = 0;
  memset(G.arrow_switched, 0, sizeof G.arrow_switched);
  G.arrow_table_ready = 0;
  G.hk_arrow = 0;
  city_path(path, sizeof path, "arrows", city, 0, "names");
  FILE *f = fopen(path, "r");
  char name[256];
  while (f && G.n_arrows < MAX_ARROWS && fscanf(f, "%255s", name) == 1) {
    const char *p = name;
    for (const char *q = name; *q; ++q)
      if (*q == ':') p = q + 1;
    snprintf(G.arrow_names[G.n_arrows++], sizeof G.arrow_names[0], "%s", p);
  }
  if (f) fclose(f);
  city_path(path, sizeof path, "lights", city, 0, "bin");
  if (lights_points(path) != 0) fprintf(stderr, "no %s, no street lights\n", path);
  hud_set_city(city);
}

static int track_open(int city, int zone) {
  char path[1200];
  load_pause();
  if (G.track_old.n_batch || G.track_old.n_tex) wpk_free(&G.track_old); /* a third switch in a row */
  G.track_old = G.track;
  memset(&G.track, 0, sizeof G.track);
  track_set(0);
  G.track_city = city;
  G.track_zone = zone;
  G.hd_reload = 0;
  extras_open(city);
  city_path(path, sizeof path, "", city, zone, "wpk");
  return wpk_open(&G.track, path);
}

/* GL objects of track_old freed a menu frame */
#define TRACK_FREE_STEP 1500
/* frames the time zone card stays at rest before its map starts loading (a quick look at the
 * other card and back costs nothing) */
#define ZONE_REST 40

/* ---- the player's best time on each course, kept in data_dir/records (a player with a
 * card, as the arcade has it): the console's YOUR RECORD (the card's
 * clV386CardData::getTimeAttack) and RECORD TIME (the machine's course record,
 * clTimeAttackRankingDataManager::getCourseRecord, 0 while no run has set one). On the iPad
 * the player is the machine's only one, so both are that best, 0 (dashes) until the course's
 * first goal. The attract loop's ranking still shows its fresh top ten at 9'59"999 (menu.c),
 * which a run places behind when it is slower. ---- */
#define RANKING_TOP_MS 599999

/* save_dir/records<n> (save n's): a line with the player's best on each course (ms, 0 = none), then a line
 * "car <course> <pack> <ms>" for each car type's best on a course */
static void records_load(void) {
  char path[1100];
  snprintf(path, sizeof path, "%s/records%d", G.save_dir, G.menu.save_slot);
  FILE *f = G.menu.save_slot ? fopen(path, "r") : NULL;
  for (int i = 0; i < CAR_COURSES; ++i)
    if (!f || fscanf(f, "%d", &G.best_ms[i]) != 1 || G.best_ms[i] < 0) G.best_ms[i] = 0;
  G.n_car_best = 0;
  int course, pack, ms;
  while (f && G.n_car_best < CAR_BEST_MAX && fscanf(f, " car %d %d %d", &course, &pack, &ms) == 3)
    if (course >= 0 && course < CAR_COURSES && ms > 0) {
      G.car_best[G.n_car_best].course = course;
      G.car_best[G.n_car_best].pack = pack;
      G.car_best[G.n_car_best++].ms = ms;
    }
  if (f) fclose(f);
}

static void records_save(void) {
  char path[1100];
  if (!G.menu.save_slot) return;
  snprintf(path, sizeof path, "%s/records%d", G.save_dir, G.menu.save_slot);
  FILE *f = fopen(path, "w");
  if (!f) return;
  for (int i = 0; i < CAR_COURSES; ++i) fprintf(f, "%d%c", G.best_ms[i], i + 1 < CAR_COURSES ? ' ' : '\n');
  for (int i = 0; i < G.n_car_best; ++i) fprintf(f, "car %d %d %d\n", G.car_best[i].course, G.car_best[i].pack, G.car_best[i].ms);
  fclose(f);
}

static void records_show(int course) {
  float best = (float)G.best_ms[course] * 0.001f;
  hud_set_records(best, best);
}

/* A run reached the goal: clTaResult's figures (the records as they stand after it), and a
 * new best kept. The course's record is the player's best, as on the console; the run's
 * place counts the ranking's fresh top ten (all 9'59"999, menu.c) too. */
static void records_goal(int course, const car_race *race, menu_result *res) {
  int ms = (int)(race->total * 1000.0f); /* as nsTaData::setTime7 shows it */
  if (ms <= 0) ms = 1;
  int best = G.best_ms[course];
  int k = 0;
  while (k < G.n_car_best && (G.car_best[k].course != course || G.car_best[k].pack != G.race_pack)) k++;
  int car = k < G.n_car_best ? G.car_best[k].ms : 0;
  memset(res, 0, sizeof *res);
  res->new_course = !best || ms < best;
  res->new_car = !car || ms < car;
  res->new_personal = !best || ms < best;
  /* (the run's own time as it is, the kept ones a hair over their ms so they show as such) */
  res->course_record = res->new_course ? race->total : ((float)best + 0.5f) * 0.001f;
  res->car_record = res->new_car ? race->total : ((float)car + 0.5f) * 0.001f;
  res->personal = res->new_personal ? race->total : ((float)best + 0.5f) * 0.001f;
  /* the sections the course has (menu_g_finish_ta_section_1 shows four) */
  for (int i = 0; i < race->sections && i < MENU_SECTIONS; ++i)
    res->section[i] = (i + 1 < race->sections ? race->split[i + 1] : race->total) - race->split[i];
  res->total = race->total;
  res->top_speed = (int)G.top_kmh;
  /* its place: behind the player's best if that is faster, behind the default ten past 9'59"999 */
  res->rank = 1 + (best && best < ms) + (ms > RANKING_TOP_MS ? 10 : 0);
  if (res->new_personal) G.best_ms[course] = ms;
  if (res->new_car) {
    if (k == G.n_car_best && k < CAR_BEST_MAX) {
      G.car_best[k].course = course;
      G.car_best[k].pack = G.race_pack;
      G.n_car_best++;
    }
    if (k < G.n_car_best) G.car_best[k].ms = ms;
  }
  if (res->new_personal || res->new_car) records_save();
}

/* ---- the titles (clWorkSpace::judgeTitle, src/title.h): the card as its getters read it,
 * taken as the race starts and again once its results are in; the race's new titles go to
 * the title select on the way back (menu_title_due) ---- */
static double title_odo(void) {
  return G.menu.own_sel >= 0 && G.menu.own_sel < G.menu.n_own ? G.menu.own[G.menu.own_sel].odo : 0.0;
}

static void title_card_now(title_card *c) {
  const menu_state *m = &G.menu;
  int mk = m->maker, car = m->car;
  memset(c, 0, sizeof *c);
  c->power = m->st_power[mk][car];
  c->handling = m->st_handling[mk][car];
  c->st_div = m->st_div;
  c->st_bits = m->st_bits;
  int bits = 0;
  for (int i = 0; i < 20; ++i) bits += m->st_bits >> i & 1;
  c->st_wins = 20 * m->st_div + bits; /* getStoryClearCount: the episodes cleared */
  c->st_streak = m->st_streak_car[mk][car];
  for (int k = 0; k < G.n_car_best; ++k) {
    int slot = car_course_ta_slot(G.car_best[k].course);
    if (G.car_best[k].pack == G.race_pack && slot >= 0) c->ta_ms[slot] = G.car_best[k].ms;
  }
  memcpy(c->ten, m->ten_result, sizeof c->ten);
  c->vs_wins = m->vs_wins_car[mk][car];
}

/* the race over (its results on the card): judge it, the new titles due */
static void title_race_over(int mode, int won, const car_race *race, int course, int ta_top) {
  menu_state *m = &G.menu;
  title_play p;
  memset(&p, 0, sizeof p);
  p.mode = mode;
  p.model = title_model(m->maker, m->car, m->color);
  p.odo_m = title_odo();
  p.race_m = p.odo_m - G.title_odo0;
  time_t now = time(NULL);
  struct tm tm = *localtime(&now);
  p.year = tm.tm_year + 1900, p.month = tm.tm_mon + 1, p.day = tm.tm_mday;
  p.hour = tm.tm_hour, p.minute = tm.tm_min, p.second = tm.tm_sec;
  p.name = m->name;
  p.plays = m->plays++;
  p.won = won;
  p.pre = G.title_pre;
  title_card_now(&p.now);
  p.story_sel = m->story_sel;
  p.ta_slot = car_course_ta_slot(course);
  p.ta_time = race->total;
  p.ta_top = ta_top;
  p.ten_course = menu_ten_course(m);
  p.ten_level = m->ten_level;
  if (mode == TITLE_VS)
    for (int k = 0; k < m->vs.match_n && k < MENU_VS_PLAYERS; ++k) {
      if (k == m->vs.match_me || p.vs_n >= 3) continue;
      p.vs_model[p.vs_n++] = title_model(m->vs.match[k].maker, m->vs.match[k].car, m->vs.match[k].color);
    }
  p.log = race->title_log_on ? race->title_log : NULL;
  int got[3];
  int n = title_judge(&p, menu_title(m), got);
  fprintf(stderr, "titles: mode %d, %d new (%d %d %d)\n", mode, n, got[0], got[1], got[2]);
  menu_title_due(m, got);
}

/* ---- save_dir/garage<n> (save n's): the car setting screen's choices (menu_state), a line "gearbox <0 AT,
 * 1 MT>", then "tune <0 stock, 1 full> <full tune's power> <car>" for each car of the car
 * select (by its label, src/cardb.h), and "sttune <power> <handling> <gauge> <car>", the
 * story's tune and points; "own <colour> <registered (time_t)> <mileage (m)> <car>" for each
 * of the save's cars, with "cstory <divisions> <episode bits> <car>", "cten <course>
 * <medal of each level> <car>", "cclass <class 0..46> <car>" and "ctitle <title> <story wins in
 * a row> <versus wins> <car>", its story and 10-in-a-row
 * results and class (a garage from before the
 * cars kept them once, as "story_div", "story_bits" and "ten <course> <medals>") ---- */
static int car_by_label(const char *label, int *maker, int *car) {
  for (int mk = 0; mk < CARDB_ALL; ++mk)
    for (int c = 0; c < CARDB_NCARS[mk]; ++c)
      if (same_name(CARDB_CARS[mk][c].label, label)) {
        *maker = mk, *car = c;
        return 1;
      }
  return 0;
}

/* A garage from before the cars: its cars are those tuned or dressed up (in their first
 * colour), the one dressed up most (else the first) taking the story and 10-in-a-row
 * results it kept for all of them. */
static void garage_adopt_cars(int legacy) {
  int best = -1, best_score = -1;
  for (int mk = 0; mk < CARDB_ALL; ++mk)
    for (int c = 0; c < CARDB_NCARS[mk]; ++c) {
      const menu_state *m = &G.menu;
      int used = m->tune_full[mk][c] || m->st_power[mk][c] || m->st_handling[mk][c] || m->st_gauge[mk][c] ||
                 m->dress_lv[mk][c] || m->aero[mk][c] || m->mirror[mk][c] || m->wing[mk][c] || m->bonnet[mk][c] ||
                 m->wheel[mk][c] || m->neon[mk][c] || m->vinyl[mk][c];
      if (!used || G.menu.n_own >= MENU_OWN_MAX) continue;
      int score = 4 * m->dress_lv[mk][c] + m->st_power[mk][c] + m->st_handling[mk][c] + m->st_gauge[mk][c];
      if (score > best_score) best_score = score, best = G.menu.n_own;
      G.menu.own[G.menu.n_own++] = (menu_own){ (signed char)mk, (signed char)c, 0, (long long)time(NULL), 0 };
    }
  if (best >= 0 && legacy) {
    int mk = G.menu.own[best].maker, c = G.menu.own[best].car;
    G.menu.st_div_car[mk][c] = G.menu.st_div;
    G.menu.st_bits_car[mk][c] = G.menu.st_bits;
    memcpy(G.menu.ten_car[mk][c], G.menu.ten_result, sizeof G.menu.ten_result);
  }
  fprintf(stderr, "garage from before the cars: %d cars\n", G.menu.n_own);
}

/* The OPTION screen's choices (the SAVE DATA screen's gear key too, before any save is
 * chosen): save_dir/option, the same for every save; a save's garage keeps them only for a
 * port from before (Allan, 10-24). */
/* save_dir/option's value k (0..12, as options_save writes them), or def: what render_hq
 * (the ANTI-ALIASING) and the first map (the HD TEXTURES) take before the menus have the
 * screen's options */
static int options_peek(int k, int def) {
  char path[1100];
  snprintf(path, sizeof path, "%s/option", G.save_dir);
  FILE *f = fopen(path, "r");
  int v[13], n = f ? fscanf(f, "option %d %d %d %d %d %d %d %d %d %d %d %d %d", &v[0], &v[1], &v[2], &v[3], &v[4],
                            &v[5], &v[6], &v[7], &v[8], &v[9], &v[10], &v[11], &v[12]) : 0;
  if (f) fclose(f);
  return n > k ? v[k] : def;
}

/* The OPTION screen's graphics switches to the renderer (Android, render_hq on): the bloom,
 * the SSAO (its targets made the first time it is on), the last frame's reflections (the
 * paint's, the wet road's), the maps' HD packs (the map and its extras opened again at the
 * next menu frame); the wet road's strength is the race's (G.rp.wet) */
static void hq_apply(void) {
  if (!G.hq_on) return;
  int bloom = G.menu.bloom_on, ssao = G.menu.ssao_on, env = (G.menu.paint_on ? 1 : 0) | (G.menu.wet_on ? 2 : 0);
  if (ssao && !G.hq_ssao_made) render_ssao(1.0f), G.hq_ssao_made = 1;
  if (bloom != G.hq_bloom || ssao != G.hq_ssao || env != G.hq_env) {
    render_hq_options(bloom, ssao, env & 1, env >> 1);
    G.hq_bloom = bloom, G.hq_ssao = ssao, G.hq_env = env;
  }
  if (G.menu.hd_tex != G.hd_maps) {
    G.hd_maps = G.menu.hd_tex;
    G.hd_reload = 1, G.extras_city = -1; /* (menu_frame opens them again) */
  }
}

static int options_load(void) {
  char path[1100];
  snprintf(path, sizeof path, "%s/option", G.save_dir);
  FILE *f = fopen(path, "r");
  if (!f) return 0;
  int a, b, c, d = 1, e = 0, ao = 1, bm = 1, hq[6] = { 1, 1, 0, 1, 1, 1 }, hires = 0;
  int n = fscanf(f, "option %d %d %d %d %d %d %d %d %d %d %d %d %d %d", &a, &b, &c, &d, &e, &ao, &bm, &hq[0], &hq[1],
                 &hq[2], &hq[3], &hq[4], &hq[5], &hires);
  /* the KEY LAYOUT's groups (menu.c MENU_LAYOUT): size percent and move, a second line */
  int lay[9];
  if (n >= 3 && fscanf(f, " keys %d %d %d %d %d %d %d %d %d", &lay[0], &lay[1], &lay[2], &lay[3], &lay[4], &lay[5], &lay[6],
                       &lay[7], &lay[8]) == 9)
    for (int g = 0; g < 3; ++g) {
      int k = lay[g * 3];
      G.menu.key_k[g] = k < 60 ? 60 : k > 160 ? 160 : k;
      G.menu.key_dx[g] = lay[g * 3 + 1] < -2000 || lay[g * 3 + 1] > 2000 ? 0 : lay[g * 3 + 1];
      G.menu.key_dy[g] = lay[g * 3 + 2] < -2000 || lay[g * 3 + 2] > 2000 ? 0 : lay[g * 3 + 2];
    }
  fclose(f);
  if (n < 3) return 0;
  G.menu.hires = hires != 0;
  G.menu.hd_tex = hq[0] != 0, G.menu.bloom_on = hq[1] != 0, G.menu.ssao_on = hq[2] != 0;
  G.menu.wet_on = hq[3] != 0, G.menu.paint_on = hq[4] != 0, G.menu.aa4 = hq[5] != 0;
  G.menu.gyro = a != 0;
  G.menu.gyro_sens = b >= 1 && b <= 10 ? b : 5;
  G.menu.over_on = c != 0;
  G.menu.sharp_on = d != 0;
  G.menu.pedal_slide = e != 0;
  G.menu.ao_on = ao != 0;
  G.menu.bm_on = bm != 0;
  return 1;
}

static void options_save(void) {
  static int last[23] = { -1 };
  int now[23] = { G.menu.gyro, G.menu.gyro_sens, G.menu.over_on, G.menu.sharp_on, G.menu.pedal_slide, G.menu.ao_on,
                  G.menu.bm_on, G.menu.hd_tex, G.menu.bloom_on, G.menu.ssao_on, G.menu.wet_on, G.menu.paint_on,
                  G.menu.aa4, G.menu.hires };
  for (int g = 0; g < 3; ++g)
    now[14 + g * 3] = G.menu.key_k[g], now[15 + g * 3] = G.menu.key_dx[g], now[16 + g * 3] = G.menu.key_dy[g];
  if (G.options_global && !memcmp(now, last, sizeof now)) return;
  char path[1100];
  snprintf(path, sizeof path, "%s/option", G.save_dir);
  FILE *f = fopen(path, "w");
  if (!f) return;
  fprintf(f, "option %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n", now[0], now[1], now[2], now[3], now[4], now[5], now[6],
          now[7], now[8], now[9], now[10], now[11], now[12], now[13]);
  fprintf(f, "keys %d %d %d %d %d %d %d %d %d\n", now[14], now[15], now[16], now[17], now[18], now[19], now[20], now[21],
          now[22]);
  fclose(f);
  memcpy(last, now, sizeof last);
  G.options_global = 1;
}

/* a garage's "name" line into name (left as it is for another line) */
static void name_read(const char *line, short name[5]) {
  int c[5];
  if (sscanf(line, "name %d %d %d %d %d", &c[0], &c[1], &c[2], &c[3], &c[4]) != 5) return;
  for (int i = 0, end = 0; i < 5; ++i) {
    end |= c[i] < 0 || c[i] >= 256;
    name[i] = (short)(end ? -1 : c[i]);
  }
}

static void garage_load(void) {
  char path[1100], line[256], label[200];
  snprintf(path, sizeof path, "%s/garage%d", G.save_dir, G.menu.save_slot);
  FILE *f = G.menu.save_slot ? fopen(path, "r") : NULL;
  if (!f) return;
  int a, b, legacy = 0;
  while (fgets(line, sizeof line, f)) {
    if (sscanf(line, "gearbox %d", &a) == 1) G.menu.manual = a != 0;
    name_read(line, G.menu.name);
    int c2, d2 = 1;
    if (!G.options_global && sscanf(line, "option %d %d %d %d", &a, &b, &c2, &d2) >= 3) {
      G.menu.gyro = a != 0;
      G.menu.gyro_sens = b >= 1 && b <= 10 ? b : 5;
      G.menu.over_on = c2 != 0;
      G.menu.sharp_on = d2 != 0;
    }
    /* a garage from before the cars: the story's progress (st_div, st_bits; an older one's
     * "story <next select>" counted the selects one after the other) */
    if (sscanf(line, "story_div %d", &a) == 1 && a >= 0) G.menu.st_div = a, legacy = 1;
    if (sscanf(line, "story_bits %d", &a) == 1) G.menu.st_bits = a & 0xfffff, legacy = 1;
    if (sscanf(line, "story %d", &a) == 1 && a > 0 && a < 100)
      G.menu.st_div = a / 20, G.menu.st_bits = (1 << (a % 20)) - 1, legacy = 1;
    int c3;
    long long born;
    double odo;
    if (sscanf(line, "own %d %lld %lf %199[^\n]", &a, &born, &odo, label) == 4) {
      int mk, c;
      if (car_by_label(label, &mk, &c) && menu_own_find(&G.menu, mk, c) < 0 && G.menu.n_own < MENU_OWN_MAX &&
          a >= 0 && a < CARDB_CARS[mk][c].ncolors)
        G.menu.own[G.menu.n_own++] = (menu_own){ (signed char)mk, (signed char)c, (signed char)a, born, odo };
      continue;
    }
    if (sscanf(line, "cstory %d %d %199[^\n]", &a, &b, label) == 3) {
      int mk, c;
      if (car_by_label(label, &mk, &c) && a >= 0) G.menu.st_div_car[mk][c] = a, G.menu.st_bits_car[mk][c] = b & 0xfffff;
      continue;
    }
    if (sscanf(line, "ccheck %d %d %199[^\n]", &a, &b, label) == 3) {
      int mk, c;
      if (car_by_label(label, &mk, &c) && a >= 0 && a < 3 && b >= 0 && b < 3)
        G.menu.meter_car[mk][c] = (signed char)a, G.menu.sound_car[mk][c] = (signed char)b;
      continue;
    }
    if (sscanf(line, "cbonus %d %199[^\n]", &a, label) == 2) {
      int mk, c;
      if (car_by_label(label, &mk, &c) && a >= 0 && a < 16) G.menu.st_bonus_car[mk][c] = (unsigned char)a;
      continue;
    }
    if (sscanf(line, "cstar %d %d %199[^\n]", &a, &b, label) == 3) {
      int mk, c;
      if (car_by_label(label, &mk, &c) && a >= 0 && a <= 999999 && b >= 0 && b <= 999999)
        G.menu.vs_stars_car[mk][c] = a, G.menu.vs_count_car[mk][c] = b;
      continue;
    }
    if (sscanf(line, "cclass %d %199[^\n]", &a, label) == 2) {
      int mk, c;
      if (car_by_label(label, &mk, &c) && a >= 0 && a < HUD_CLASSES) G.menu.class_car[mk][c] = (signed char)a;
      continue;
    }
    if (sscanf(line, "ctitle %d %d %d %199[^\n]", &a, &b, &c3, label) == 4) {
      int mk, c;
      if (car_by_label(label, &mk, &c)) {
        G.menu.title_car[mk][c] = (short)(a > 0 && a < 0x7fff ? a : 0);
        G.menu.st_streak_car[mk][c] = (short)(b >= 0 && b < 0x7fff ? b : 0);
        G.menu.vs_wins_car[mk][c] = c3 >= 0 ? c3 : 0;
      }
      continue;
    }
    if (sscanf(line, "sttune %d %d %d %199[^\n]", &a, &b, &c3, label) == 4) {
      for (int mk = 0; mk < CARDB_ALL; ++mk)
        for (int c = 0; c < CARDB_NCARS[mk]; ++c)
          if (same_name(CARDB_CARS[mk][c].label, label) && a >= 0 && a <= CAR_TUNE_MAX && b >= 0 && b <= CAR_TUNE_MAX && c3 >= 0 && c3 < 20) {
            G.menu.st_power[mk][c] = (signed char)a;
            G.menu.st_handling[mk][c] = (signed char)b;
            G.menu.st_gauge[mk][c] = (signed char)c3;
          }
      continue;
    }
    if (sscanf(line, "dress %d %d %199[^\n]", &a, &b, label) == 3) {
      for (int mk = 0; mk < CARDB_ALL; ++mk)
        for (int c = 0; c < CARDB_NCARS[mk]; ++c)
          if (same_name(CARDB_CARS[mk][c].label, label) && a >= 0 && a <= 63 && b >= 0 && b <= 3) {
            G.menu.dress_lv[mk][c] = (signed char)a;
            G.menu.aero[mk][c] = (signed char)b;
          }
      continue;
    }
    if (sscanf(line, "wear %d %d %d %199[^\n]", &a, &b, &c3, label) == 4) {
      for (int mk = 0; mk < CARDB_ALL; ++mk)
        for (int c = 0; c < CARDB_NCARS[mk]; ++c)
          if (same_name(CARDB_CARS[mk][c].label, label) && a >= 0 && a <= 1 && b >= 0 && b <= 4 && c3 >= 0 && c3 <= 3) {
            G.menu.mirror[mk][c] = (signed char)a;
            G.menu.wing[mk][c] = (signed char)b;
            G.menu.bonnet[mk][c] = (signed char)c3;
          }
      continue;
    }
    if (sscanf(line, "vinyl %d %199[^\n]", &a, label) == 2) {
      for (int mk = 0; mk < CARDB_ALL; ++mk)
        for (int c = 0; c < CARDB_NCARS[mk]; ++c)
          if (same_name(CARDB_CARS[mk][c].label, label) && a >= 0 && a < 100) G.menu.vinyl[mk][c] = (unsigned char)a;
      continue;
    }
    if (sscanf(line, "neon %d %199[^\n]", &a, label) == 2) {
      for (int mk = 0; mk < CARDB_ALL; ++mk)
        for (int c = 0; c < CARDB_NCARS[mk]; ++c)
          if (same_name(CARDB_CARS[mk][c].label, label) && a >= 0 && a <= 12) G.menu.neon[mk][c] = (signed char)a;
      continue;
    }
    if (sscanf(line, "wheel %d %199[^\n]", &a, label) == 2) {
      for (int mk = 0; mk < CARDB_ALL; ++mk)
        for (int c = 0; c < CARDB_NCARS[mk]; ++c)
          if (same_name(CARDB_CARS[mk][c].label, label) && a >= 0 && a <= 255) G.menu.wheel[mk][c] = (unsigned char)a;
      continue;
    }
    int t[11];
    if (sscanf(line, "ten %d %d %d %d %d %d %d %d %d %d %d %d", &a, &t[0], &t[1], &t[2], &t[3], &t[4], &t[5], &t[6], &t[7],
               &t[8], &t[9], &t[10]) == 12 && a >= 0 && a < 10) {
      for (int i = 0; i < 11; ++i) G.menu.ten_result[a][i] = (signed char)(t[i] >= 0 && t[i] <= 3 ? t[i] : 0);
      legacy |= 2;
      continue;
    }
    if (sscanf(line, "cten %d %d %d %d %d %d %d %d %d %d %d %d %199[^\n]", &a, &t[0], &t[1], &t[2], &t[3], &t[4], &t[5],
               &t[6], &t[7], &t[8], &t[9], &t[10], label) == 13 && a >= 0 && a < 10) {
      int mk, c;
      if (car_by_label(label, &mk, &c))
        for (int i = 0; i < 11; ++i) G.menu.ten_car[mk][c][a][i] = (signed char)(t[i] >= 0 && t[i] <= 3 ? t[i] : 0);
      continue;
    }
    if (sscanf(line, "tune %d %d %199[^\n]", &a, &b, label) != 3) continue;
    for (int mk = 0; mk < CARDB_ALL; ++mk)
      for (int c = 0; c < CARDB_NCARS[mk]; ++c)
        if (same_name(CARDB_CARS[mk][c].label, label) && b >= CAR_TUNE_TOTAL - CAR_TUNE_MAX && b <= CAR_TUNE_MAX) {
          G.menu.tune_full[mk][c] = (signed char)(a != 0);
          G.menu.tune_power[mk][c] = (signed char)b;
        }
  }
  fclose(f);
  if (!G.menu.n_own) garage_adopt_cars(legacy);
  /* (a garage from before the classes: each car's from its results) */
  menu_class_update(&G.menu);
}

static void garage_save(void) {
  char path[1100];
  if (!G.menu.save_slot) return;
  snprintf(path, sizeof path, "%s/garage%d", G.save_dir, G.menu.save_slot);
  FILE *f = fopen(path, "w");
  if (!f) return;
  fprintf(f, "gearbox %d\n", G.menu.manual);
  fprintf(f, "option %d %d %d %d\n", G.menu.gyro, G.menu.gyro_sens, G.menu.over_on, G.menu.sharp_on);
  /* the player's name (namedb.h's characters, -1 past its end) */
  fprintf(f, "name %d %d %d %d %d\n", G.menu.name[0], G.menu.name[1], G.menu.name[2], G.menu.name[3], G.menu.name[4]);
  /* the cars, in the order they came; each one's story and 10-in-a-row results and class
   * (raised to its results' here, as clRaceThread::fini does after each race: never lowered) */
  menu_class_update(&G.menu);
  for (int k = 0; k < G.menu.n_own; ++k) {
    const menu_own *o = &G.menu.own[k];
    const char *label = CARDB_CARS[o->maker][o->car].label;
    fprintf(f, "own %d %lld %.0f %s\n", o->color, o->born, o->odo, label);
    fprintf(f, "cstory %d %d %s\n", G.menu.st_div_car[o->maker][o->car], G.menu.st_bits_car[o->maker][o->car], label);
    fprintf(f, "cclass %d %s\n", G.menu.class_car[o->maker][o->car], label);
    /* the versus stars and count (the card's +0x180, +0x17c) */
    fprintf(f, "cstar %d %d %s\n", G.menu.vs_stars_car[o->maker][o->car], G.menu.vs_count_car[o->maker][o->car], label);
    /* the story bonuses' flags (the card's +0x279, +0x186, +0x278, +0x187) */
    fprintf(f, "cbonus %d %s\n", G.menu.st_bonus_car[o->maker][o->car], label);
    /* the card check's meter (+0x26c) and BGM sound source */
    fprintf(f, "ccheck %d %d %s\n", G.menu.meter_car[o->maker][o->car], G.menu.sound_car[o->maker][o->car], label);
    fprintf(f, "ctitle %d %d %d %s\n", G.menu.title_car[o->maker][o->car], G.menu.st_streak_car[o->maker][o->car],
            G.menu.vs_wins_car[o->maker][o->car], label);
    for (int c = 0; c < 10; ++c) {
      const signed char *t = G.menu.ten_car[o->maker][o->car][c];
      int any = 0;
      for (int i = 0; i < 11; ++i) any |= t[i];
      if (!any) continue;
      fprintf(f, "cten %d", c);
      for (int i = 0; i < 11; ++i) fprintf(f, " %d", t[i]);
      fprintf(f, " %s\n", label);
    }
  }
  for (int mk = 0; mk < CARDB_ALL; ++mk)
    for (int c = 0; c < CARDB_NCARS[mk]; ++c)
      fprintf(f, "tune %d %d %s\n", G.menu.tune_full[mk][c], G.menu.tune_power[mk][c], CARDB_CARS[mk][c].label);
  for (int mk = 0; mk < CARDB_ALL; ++mk)
    for (int c = 0; c < CARDB_NCARS[mk]; ++c)
      fprintf(f, "sttune %d %d %d %s\n", G.menu.st_power[mk][c], G.menu.st_handling[mk][c], G.menu.st_gauge[mk][c],
              CARDB_CARS[mk][c].label);
  for (int mk = 0; mk < CARDB_ALL; ++mk)
    for (int c = 0; c < CARDB_NCARS[mk]; ++c)
      if (G.menu.dress_lv[mk][c] || G.menu.aero[mk][c])
        fprintf(f, "dress %d %d %s\n", G.menu.dress_lv[mk][c], G.menu.aero[mk][c], CARDB_CARS[mk][c].label);
  for (int mk = 0; mk < CARDB_ALL; ++mk)
    for (int c = 0; c < CARDB_NCARS[mk]; ++c)
      if (G.menu.mirror[mk][c] || G.menu.wing[mk][c] || G.menu.bonnet[mk][c])
        fprintf(f, "wear %d %d %d %s\n", G.menu.mirror[mk][c], G.menu.wing[mk][c], G.menu.bonnet[mk][c],
                CARDB_CARS[mk][c].label);
  for (int mk = 0; mk < CARDB_ALL; ++mk)
    for (int c = 0; c < CARDB_NCARS[mk]; ++c)
      if (G.menu.wheel[mk][c]) fprintf(f, "wheel %d %s\n", G.menu.wheel[mk][c], CARDB_CARS[mk][c].label);
  for (int mk = 0; mk < CARDB_ALL; ++mk)
    for (int c = 0; c < CARDB_NCARS[mk]; ++c)
      if (G.menu.neon[mk][c]) fprintf(f, "neon %d %s\n", G.menu.neon[mk][c], CARDB_CARS[mk][c].label);
  for (int mk = 0; mk < CARDB_ALL; ++mk)
    for (int c = 0; c < CARDB_NCARS[mk]; ++c)
      if (G.menu.vinyl[mk][c]) fprintf(f, "vinyl %d %s\n", G.menu.vinyl[mk][c], CARDB_CARS[mk][c].label);
  fclose(f);
}

/* ---- the port's saves (menu.c's SAVE DATA screen): save n (1..MENU_SAVES) is
 * save_dir/garage<n> and records<n>, save_dir/slot names the one in use. A garage and
 * records from before the saves become save 1. ---- */
static void save_slot_write(void) {
  char path[1100];
  snprintf(path, sizeof path, "%s/slot", G.save_dir);
  FILE *f = fopen(path, "w");
  if (!f) return;
  fprintf(f, "%d\n", G.menu.save_slot);
  fclose(f);
}

/* the old garage and records as save n's */
static void save_adopt(int n) {
  static const char *const WHAT[2] = { "garage", "records" };
  for (int i = 0; i < 2; ++i) {
    char from[1100], to[1100];
    snprintf(from, sizeof from, "%s/%s", G.save_dir, WHAT[i]);
    snprintf(to, sizeof to, "%s/%s%d", G.save_dir, WHAT[i], n);
    if (access(from, F_OK) == 0) rename(from, to);
  }
}

static void save_open(void) {
  char path[1100];
  int slot = 0;
  snprintf(path, sizeof path, "%s/slot", G.save_dir);
  FILE *f = fopen(path, "r");
  if (f) {
    if (fscanf(f, "%d", &slot) != 1 || slot < 0 || slot > MENU_SAVES) slot = 0;
    fclose(f);
  } else {
    snprintf(path, sizeof path, "%s/garage", G.save_dir);
    if (access(path, R_OK) == 0) {
      save_adopt(1);
      slot = 1;
    }
  }
#ifdef WMMT_EGL
  /* test: the data dir's garage as this save, START straight to the maker select */
  if (getenv("WMMT_SAVE_SLOT")) {
    slot = atoi(getenv("WMMT_SAVE_SLOT"));
    save_adopt(slot);
    G.menu.save_skip = 1;
  }
#endif
  G.menu.save_slot = slot;
  if (!f && slot) save_slot_write();
}

/* what the save screen shows of each save */
static void save_scan(void) {
  int any = 0;
  for (int k = 0; k < MENU_SAVES; ++k) {
    menu_save_info *s = &G.menu.save[k];
    memset(s, 0, sizeof *s);
    memset(s->name, -1, sizeof s->name);
    char path[1100], line[256];
    snprintf(path, sizeof path, "%s/garage%d", G.save_dir, k + 1);
    FILE *f = fopen(path, "r");
    if (!f) continue;
    s->used = any = 1;
    struct stat st;
    if (fstat(fileno(f), &st) == 0) s->when = (long long)st.st_mtime;
    /* (a garage from before the cars: its one story) */
    int div = 0, bits = 0, a, b, t[11];
    while (fgets(line, sizeof line, f)) {
      if (sscanf(line, "story_div %d", &a) == 1 && a >= 0) div = a;
      if (sscanf(line, "story_bits %d", &a) == 1) bits = a & 0xfffff;
      if (sscanf(line, "cstory %d %d", &a, &b) == 2 && a >= 0) s->story += 20 * a + __builtin_popcount((unsigned)b & 0xfffff);
      if (strncmp(line, "own ", 4) == 0) s->cars++;
      name_read(line, s->name);
      if (sscanf(line, "ten %d %d %d %d %d %d %d %d %d %d %d %d", &a, &t[0], &t[1], &t[2], &t[3], &t[4], &t[5], &t[6],
                 &t[7], &t[8], &t[9], &t[10]) == 12 ||
          sscanf(line, "cten %d %d %d %d %d %d %d %d %d %d %d %d", &a, &t[0], &t[1], &t[2], &t[3], &t[4], &t[5], &t[6],
                 &t[7], &t[8], &t[9], &t[10]) == 12)
        for (int i = 0; i < 11; ++i) s->medals += t[i] == 3;
    }
    fclose(f);
    s->story += 20 * div + __builtin_popcount((unsigned)bits);
  }
  if (!any) G.menu.save_new = 1, G.menu.save_del = 0;
}

/* the save DELETE took off the save screen: its garage and records (and the save in use no
 * more, if it was) */
static void save_erase(int n) {
  static const char *const WHAT[2] = { "garage", "records" };
  for (int i = 0; i < 2; ++i) {
    char path[1100];
    snprintf(path, sizeof path, "%s/%s%d", G.save_dir, WHAT[i], n);
    remove(path);
  }
  if (G.menu.save_slot == n) {
    G.menu.save_slot = 0;
    save_slot_write();
  }
  fprintf(stderr, "save %d deleted\n", n);
}

/* the save chosen: loaded, or started over (the OPTION screen's choices kept) */
static void save_take(void) {
  G.menu.save_slot = G.menu.save_pick;
  G.menu.save_pick = 0;
  menu_garage_reset(&G.menu);
  if (G.menu.save_pick_new) {
    char path[1100];
    snprintf(path, sizeof path, "%s/records%d", G.save_dir, G.menu.save_slot);
    remove(path);
    garage_save();
  } else {
    garage_load();
  }
  records_load();
  save_slot_write();
  fprintf(stderr, "save %d %s\n", G.menu.save_slot, G.menu.save_pick_new ? "started over" : "loaded");
}

void app_save_dir(const char *dir) { snprintf(G.save_dir, sizeof G.save_dir, "%s", dir); }

void app_console(void) {
  G.console = 1;
  car_hits(1);
}

void app_wide(float edge, float key_k) {
  G.edge = edge > 0 ? edge : 0;
  G.phone = G.edge > 0 && !G.console;
  anm_view(G.edge);
  hud_wide(G.edge, key_k);
  menu_wide(G.edge); /* (the menus' backs across a wide screen) */
  if (G.console && G.edge > 0) {
    hud_lift(28, 40); /* (the plate sat on the pedals, the shift gate on ◀) */
  } else if (G.edge > 0) {
    /* an iPhone's keys x1.3: the plate off the pedals, the meters off it with the pill still
     * clear of them and of the records over it, the map off the gate and the gate off ◀ */
    hud_lift_parts(32, 42, 40, 40, 25);
  }
}

int app_init(const char *data_dir, int fb_width, int fb_height, float screen_w, float screen_h) {
  char path[1100];
  G.fb_w = fb_width;
  G.fb_h = fb_height;
  if (render_init() != 0 || hud_gl_init() != 0) {
    snprintf(G.error, sizeof G.error, "shader setup failed");
    return -1;
  }
  /* a phone of today's (app_console) can afford the road ahead sharp: trilinear and 16x
   * anisotropic filtering */
  if (G.console) render_aniso(16);
  snprintf(G.data_dir, sizeof G.data_dir, "%s", data_dir);
  if (!G.save_dir[0]) snprintf(G.save_dir, sizeof G.save_dir, "%s", data_dir);
  /* and the 3D multisampled in half floats, with a bloom over the lights (a file hq_off in
   * the data folder, or WMMT_NO_HQ, for the frame times without, to compare) */
  snprintf(path, sizeof path, "%s/hq_off", G.data_dir);
  if (G.console && !getenv("WMMT_NO_HQ") && access(path, F_OK) != 0) {
    /* (hq_2x: 2x MSAA, to weigh against the 4x) */
    snprintf(path, sizeof path, "%s/hq_2x", G.data_dir);
    /* (and the OPTION screen's ANTI-ALIASING 2X, save_dir/option's 13th: from this start) */
    int ms2 = access(path, F_OK) == 0 || !options_peek(12, 1);
    /* (hq_blits: the multisampled buffers resolved by blits, not rendered into textures) */
    snprintf(path, sizeof path, "%s/hq_blits", G.data_dir);
    if (access(path, F_OK) == 0) render_hq_blits();
    G.hq_on = render_hq(ms2 ? 2 : 4, 0.6f);
    G.hq_bloom = -1; /* (hq_apply hands render_hq the switches whatever they are) */
    /* the race's ambient occlusion only with a file ssao_on or the OPTION screen's SSAO (its
     * 20 ms on the Mi 6 cost the 60 fps; hq_apply) */
    snprintf(path, sizeof path, "%s/ssao_on", G.data_dir);
    if (access(path, F_OK) == 0) render_ssao(1.0f), G.hq_ssao_made = 1;
    if (getenv("WMMT_HQ_LEVEL")) G.gov_level = G.gov_floor = render_hq_level(atoi(getenv("WMMT_HQ_LEVEL")));
  }
  /* what a texture's or a buffer's first draw costs on this GPU, each way, for perf.log */
  char test[1200];
  render_selftest(test, sizeof test);
  perf_file();
  if (G.perf) {
    fprintf(G.perf, "%s (%s)\n", test, PORT_BUILD);
    fflush(G.perf);
  }
  pthread_mutex_init(&G.load_mx, NULL);
  pthread_cond_init(&G.load_cv, NULL);
  /* the time zone of the last race (the map the menus load first) and whether there is a night */
  snprintf(path, sizeof path, "%s/timezone", G.save_dir);
  FILE *tz = fopen(path, "r");
  int zone = tz ? fgetc(tz) == '1' : 0;
  if (tz) fclose(tz);
  G.extras_city = -1;
  city_path(path, sizeof path, "", CAR_TOKYO, 1, "wpk");
  int zones = access(path, R_OK) == 0 ? 2 : 1;
  if (zone >= zones) zone = 0;
  /* the street lights' sprites (their points come by city, extras_open) */
  if (lights_load(data_dir) != 0) fprintf(stderr, "no lights.wpk/lights.bin, no street lights\n");
  /* the course goes up to GL a little each frame while the maker logos play (app_frame) */
  G.hd_maps = G.hq_on && options_peek(7, 1); /* (the HD TEXTURES: hq_apply after the menus' options) */
  if (track_open(CAR_TOKYO, zone) != 0) {
    /* the error names the pack that failed (it named the night one whatever the zone); the
     * other time zone's map, when there is one, stands in */
    city_path(path, sizeof path, "", CAR_TOKYO, zone, "wpk");
    load_error(path);
    fprintf(stderr, "%s\n", G.error);
    if (zones < 2 || track_open(CAR_TOKYO, !zone) != 0) return -1;
    zone = !zone;
    G.error[0] = 0;
  }
  snprintf(path, sizeof path, "%s/fd3s.wpk", data_dir);
  if (wpk_load(&G.fd3s, path) != 0) {
    load_error(path);
    return -1;
  }
  G.fd3s.two_sided = 1;
  snprintf(path, sizeof path, "%s/minicourse.wpk", data_dir);
  if (wpk_load(&G.mini, path) == 0) G.mini.two_sided = 1;
  else fprintf(stderr, "no %s, no 3D map on the course selects\n", path);
  static const char *const TRAFFIC_FILE[CAR_TRAFFIC_MODELS] = { "nze121", "kzh100g", "truck", "rc2" };
  for (int k = 0; k < CAR_TRAFFIC_MODELS; ++k) {
    snprintf(path, sizeof path, "%s/traffic/%s.wpk", data_dir, TRAFFIC_FILE[k]);
    if (wpk_load(&G.traffic[k], path) == 0) G.traffic[k].two_sided = 1;
    else fprintf(stderr, "no %s, that traffic car is not drawn\n", path);
  }
  if (car_init(data_dir, G.error, sizeof G.error) != 0) return -1;
  hud_init(&G.hud, screen_w, screen_h);
  if (hud_load(data_dir) != 0) fprintf(stderr, "hud.bin missing, gauges off\n");
  if (snd_init(data_dir) != 0) fprintf(stderr, "some sound files are missing\n");
  /* GroundRange, ShortBuildRange, LongBuildRange from c1_daytime.lua (ground a little
   * further than the arcade's 4000 so the road ahead never pops in) */
  G.rp.range[WPK_GROUND] = 5000.0f;
  G.rp.range[WPK_SHORT] = 7000.0f;
  G.rp.range[WPK_LONG] = 10000.0f;
  G.rp.lamps_on = 1; /* the race's cars: clCarConstructParam mode 0 (3 the official machine) */
  G.rp.no_gloss = getenv("WMMT_NO_GLOSS") != NULL; /* test: the maps without their reflections */
  G.no_over = getenv("WMMT_NO_OVERHEAD") != NULL;
  G.no_sharp = getenv("WMMT_NO_SHARPEN") != NULL;
  G.ao_test = getenv("WMMT_AO") ? atoi(getenv("WMMT_AO")) != 0 : -1;
  if (G.phone) {
    G.rp.range[WPK_GROUND] = 4000.0f;
    G.rp.range[WPK_SHORT] = 4500.0f;
    G.rp.range[WPK_LONG] = 6500.0f;
    G.rp.no_gloss = G.no_over = G.no_sharp = 1;
    G.low_memory = 1;
    wpk_tex_half(1); /* the maps' textures alone were 153 MB of its 512 */
  }
  if (G.console) {
    /* GPU and memory to spare: the maps drawn much further (the arcade's own long range) */
    G.rp.range[WPK_GROUND] = 9000.0f;
    G.rp.range[WPK_SHORT] = 12000.0f;
    G.rp.range[WPK_LONG] = 18000.0f;
  }
  if (anm_load(data_dir) != 0) {
    snprintf(G.error, sizeof G.error, "cannot load %s/menu.bin", data_dir);
    return -1;
  }
  menu_init(&G.menu);
  save_open();
  G.options_global = options_load();
  G.menu.hq_rows = G.hq_on;
  G.menu.res_row = !G.hq_on && !G.console; /* (an iPad's, an iPhone's: res_phone) */
  G.menu.res_phone = G.edge > 0;
  hq_apply();
  garage_load();
  records_load();
  G.menu.zone = zone;
  G.menu.zones = zones;
  G.race_model = &G.fd3s;
  G.rival = G.rival_env = -1;
  /* the versus: the server last used, this app's id there (fresh each start) */
  vs_ip_load();
  G.menu.vs.ramp = 6; /* WANGAN_OUT_13 */
  G.menu.vs.km = 13;  /* as the arcade's getJoinDist */
  G.vs_id = (uint32_t)time(NULL) * 2654435761u ^ (uint32_t)getpid() << 16 ^ (uint32_t)(now_ms() * 1000.0);
  if (!G.vs_id) G.vs_id = 1;
#ifdef WMMT_EGL
  if (getenv("WMMT_RIVAL")) G.rival = G.rival_env = atoi(getenv("WMMT_RIVAL")); /* test: race this RIVALDB rival */
#endif
  G.in_menu = 1;
  fprintf(stderr, "GL %s | %s | tex %u batches %u chunks %u | menu ready\n",
          (const char *)glGetString(GL_RENDERER), (const char *)glGetString(GL_VERSION),
          G.track.n_tex, G.track.n_batch, G.track.n_chunk);
  return 0;
}

void app_touch(void *id, int phase, float x, float y) {
  if (!G.in_menu && G.menu.ending == 2) { /* the ending: a tap skips to its logo */
    if (phase == TOUCH_BEGAN) G.menu.ending_skip |= G.menu.ending_t < MENU_ENDING_SKIP;
    return;
  }
  if (G.in_menu || G.paused || (G.menu.finish && phase == TOUCH_BEGAN))
    menu_touch(&G.menu, id, phase, x, y);
  else
    hud_touch(&G.hud, id, phase, x, y); /* (over the result, letting go of the pedals) */
}

void app_tilt(float radians) { G.tilt = radians; }
int app_wants_tilt(void) { return G.menu.gyro; }

/* the OPTION screen's steering for the race's keys: the tilt for a full lock, 50 degrees at
 * sensitivity 1 down to 15 at 10 */
static void hud_options(void) {
  G.hud.gyro = G.menu.gyro;
  G.hud.slide = G.menu.pedal_slide;
  G.hud.tilt = G.tilt;
  G.hud.full = (50.0f - (float)(G.menu.gyro_sens - 1) * (35.0f / 9.0f)) * 0.0174533f;
  /* the KEY LAYOUT's (menu.c MENU_LAYOUT) */
  for (int g = 0; g < HUD_KEY_GROUPS; ++g)
    hud_layout_set(g, (float)G.menu.key_k[g] * 0.01f, (float)G.menu.key_dx[g], (float)G.menu.key_dy[g]);
}

void app_press_start(void) {
  if (G.in_menu) menu_start(&G.menu);
}

int app_movie(void) { return G.in_menu ? menu_movie(&G.menu) : -1; }

void app_movie_done(void) {
  if (G.in_menu) menu_movie_done(&G.menu);
}

/* the race's song stops with the last 10-in-a-row rival (clAudio::stopStream); result2 still
 * plays the result's (0x6d) */
float app_bgm_level(void) { return G.in_menu ? 1.0f : menu_bgm_level(&G.menu); }

const char *app_bgm(void) {
  return !G.in_menu && G.ten_bgm_off && !G.menu.finish ? NULL : menu_bgm(&G.menu, !G.in_menu);
}

/* clRaceThread::race's 10-in-a-row white (clFader): white at once as the race starts and after
 * a beaten rival's still (the "ホワイトアウト", while the next rival loads), then 6 frames on
 * the white-in fades it over 0.5 s (setFade(0, 1, 0.5): 30 frames). The alpha of the white. */
#define TEN_WHITE_HOLD 6
#define TEN_WHITE_FADE 30
static float ten_white_alpha(void) {
  if (!G.ten_white) return 0.0f;
  int t = G.ten_white - 1 - TEN_WHITE_HOLD;
  return t <= 0 ? 1.0f : 1.0f - (float)t / TEN_WHITE_FADE;
}

/* ---- Loading: the course and the car models go up to GL, and are first drawn once (the
 * driver finishes a texture's upload on its first use: wpk_prewarm), while the menus run.
 * A thread of its own does it when main.m starts one (app_loader: a GL context sharing the
 * app's, on the A5X's second core), else the menu frames in their spare time (load_step).
 * What it works on (pack_state, the course's upload and warming) belongs to whoever loads:
 * the menus pause the thread (load_pause) before they touch any of it themselves, and see
 * what is loaded through a copy they take each frame (load_sync). */

/* A car model of the car select once it is in GL (NULL while it is still loading; the
 * FD3S stands in for a missing file). */
static wpk_scene *car_pack(int k) {
  if (k < 0 || k >= NPACKS || G.seen.pack[k] < 0) return &G.fd3s;
  return G.seen.pack[k] == 1 ? &G.packs[k] : NULL;
}

/* Tell the menus what is loaded (they see it at their next load_sync): GL is done with it
 * first, as a second context must not use what it has not finished. */
static void pack_set(int k, int state) {
  if (state == 1) glFinish();
  G.pack_state[k] = (signed char)state;
  pthread_mutex_lock(&G.load_mx);
  G.pub.pack[k] = (signed char)(state == 1 || state == -1 ? state : 0);
  pthread_mutex_unlock(&G.load_mx);
}
static void track_set(int state) {
  if (state) glFinish();
  G.track_ready = state;
  if (!state) G.start_ready = 0;
  pthread_mutex_lock(&G.load_mx);
  G.pub.track = state;
  G.pub.start = G.start_ready;
  pthread_mutex_unlock(&G.load_mx);
}
static void start_set(int course1) {
  glFinish();
  G.start_ready = course1;
  pthread_mutex_lock(&G.load_mx);
  G.pub.start = course1;
  pthread_mutex_unlock(&G.load_mx);
}

/* The menus take the loading's news, and hand it what they want next (w, or NULL to keep
 * the last); outside a race and while the app is up the thread may load. */
static void load_sync(const load_wants *w) {
  pthread_mutex_lock(&G.load_mx);
  G.seen = G.pub;
  if (w) G.wants = *w;
  G.loader |= G.loader_up; /* the frames stop loading before the thread starts */
  G.load_run = G.loader && G.in_menu && !G.away;
  G.load_idle = 0;
  pthread_cond_broadcast(&G.load_cv);
  pthread_mutex_unlock(&G.load_mx);
}

/* Stop the thread after the step it is in (a texture or a few first draws): the loading
 * state is the menus' until their next load_sync. */
static void load_pause(void) {
  pthread_mutex_lock(&G.load_mx);
  G.load_run = 0;
  while (G.load_busy) pthread_cond_wait(&G.load_cv, &G.load_mx);
  pthread_mutex_unlock(&G.load_mx);
}

/* The loading's own framebuffer of one pixel, in each GL context (0 the app's, 1 the
 * thread's): it draws there, so the screen's is done with by then (no second pass over it
 * on the tile GPU). */
static GLuint load_fb(int ctx) {
  static GLuint fb[2];
  if (!fb[ctx]) {
    GLuint rb;
    glGenRenderbuffers(1, &rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB565, 1, 1);
    glGenFramebuffers(1, &fb[ctx]);
    glBindFramebuffer(GL_FRAMEBUFFER, fb[ctx]);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb);
  }
  return fb[ctx];
}

/* first draws per step (a car model's, the course's), and the bytes of the course a step
 * hands to GL: a step is a few ms at most, and each ends with a glFlush. A first draw cost
 * up to 20 ms on the iPad (push56, the driver's mipmaps); 12 of them in a step held the
 * menus' frames for 160 ms, the app's GL waiting for the thread's. */
#define WARM_STEP 1
#define WARM_STEP_COURSE 1
#define TRACK_STEP_BYTES (128 * 1024)

/* car model k's file in data_dir, without .wpk */
static const char *pack_name(int k) {
  static char name[2][128];
  static int i;
  i ^= 1;
  if (k < CARDB_NPACKS) snprintf(name[i], sizeof name[i], "cars/%s", CARDB_PACKS[k]);
  else snprintf(name[i], sizeof name[i], "chara/%s", RIVALDB_CHARA[k - CARDB_NPACKS]);
  return name[i];
}

/* One step of loading pack k: opening it, one texture or buffer of it, or first-drawing a
 * few of its batches (so its first turn on the carousel does not stall). */
static void pack_step(int k, int ctx) {
  wpk_scene *s = &G.packs[k];
  if (G.pack_state[k] == 0) {
    char path[1200];
    snprintf(path, sizeof path, "%s/%s.wpk", G.data_dir, pack_name(k));
    if (wpk_open(s, path) != 0) {
      fprintf(stderr, "no %s, the FD3S stands in\n", path);
      pack_set(k, -1);
      return;
    }
    s->two_sided = 1;
    s->skip_paints = 1; /* paint_step, for the car under the cursor */
    pack_set(k, 2);
  } else if (G.pack_state[k] == 2) {
    if (wpk_upload(s, 1)) pack_set(k, 3);
  } else if (G.pack_state[k] == 3) {
    glBindFramebuffer(GL_FRAMEBUFFER, load_fb(ctx));
    render_warm_begin();
    if (!wpk_warm(s, WARM_STEP)) {
      pack_set(k, 1);
      fprintf(stderr, "car model %s ready\n", pack_name(k));
    }
  }
}

static void paint_set(int k, int state) {
  if (state == 1) glFinish();
  G.paint_state[k] = (signed char)state;
  pthread_mutex_lock(&G.load_mx);
  G.pub.paint[k] = (signed char)(state == 1);
  pthread_mutex_unlock(&G.load_mx);
}

/* One step of loading the other paints of pack k (in GL already without them): one texture,
 * or first-drawing a few. A car's model is a third of its pack, its other paints the rest:
 * all 47 with all their paints did not fit with the course (iOS warned, and the car select
 * dropped the far makers' models and loaded them again and again). */
static void paint_step(int k, int ctx) {
  wpk_scene *s = &G.packs[k];
  if (G.paint_state[k] == 0 || G.paint_state[k] == 2) {
    if (G.paint_state[k] == 0) paint_set(k, 2);
    if (wpk_upload_paints(s, 1)) paint_set(k, 3);
  } else if (G.paint_state[k] == 3) {
    glBindFramebuffer(GL_FRAMEBUFFER, load_fb(ctx));
    render_warm_begin();
    if (!wpk_warm_paints(s, WARM_STEP)) paint_set(k, 1);
  }
}

/* In the menus, once the course and the car models are up: first-draw the map, so the race
 * does not stall on a texture's first use when the car comes fast into a new part of it.
 * First what the race's first frames draw (the sky, the traffic's models, the map and its
 * arrow boards in reach of w's course's start with the race's own warming margin: the
 * race's title page waits for that, start_set), then the rest, nearest that start first.
 * 0 once it all is. */
static int warm_step(const load_wants *w, int ctx) {
  glBindFramebuffer(GL_FRAMEBUFFER, load_fb(ctx));
  render_warm_begin();
  const float *at = START[w->course];
  int n = lights_warm();
  if (!n) n = wpk_warm(&G.sky[G.track_zone], WARM_STEP_COURSE);
  for (int k = 0; !n && k < CAR_TRAFFIC_MODELS; ++k) n = wpk_warm(&G.traffic[k], WARM_STEP_COURSE);
  if (!n) n = wpk_warm_near(&G.track, at, WARM_MARGIN, WARM_STEP_COURSE, &G.rp);
  if (!n) n = wpk_warm_near(&G.arrows[G.track_zone], at, WARM_MARGIN, WARM_STEP_COURSE, &G.rp);
  if (!n && G.start_ready != w->course + 1) start_set(w->course + 1);
  if (!n) n = wpk_warm_near(&G.track, at, 0, WARM_STEP_COURSE, NULL);
  if (!n) n = wpk_warm_near(&G.arrows[G.track_zone], at, 0, WARM_STEP_COURSE, NULL);
  return n;
}

static const cardb_color *chosen_color(void) {
  return &CARDB_CARS[G.menu.maker][G.menu.car].colors[G.menu.color];
}

/* The player's car in its aero set (cars/<pack>_aeroA..C.wpk, tools/stage/pack_cars.py
 * --aero), on the stock model's textures: loaded whole as a race starts (half a MB of
 * buffers), kept till another car or set is chosen. NULL: the stock model. */
static int aero_load(wpk_scene *s, int pack, int set) {
  wpk_scene *base = car_pack(pack);
  if (!set || !base || pack >= CARDB_NPACKS) return 0;
  char path[1200];
  snprintf(path, sizeof path, "%s/%s_aero%c.wpk", G.data_dir, pack_name(pack), 'A' + set - 1);
  if (wpk_open(s, path) != 0) {
    fprintf(stderr, "no %s\n", path);
    return 0;
  }
  s->two_sided = 1;
  wpk_share(s, base);
  wpk_upload(s, (size_t)-1);
  wpk_share_sync(s);
  wpk_warm(s, 1 << 20);
  return 1;
}

static wpk_scene *aero_model(int pack, int set) {
  if (set != G.aero_set || pack != G.aero_pack) {
    if (G.aero_set) wpk_free(&G.aero);
    G.aero_set = 0;
    if (!aero_load(&G.aero, pack, set)) return NULL;
    G.aero_pack = pack;
    G.aero_set = set;
  }
  return G.aero_set ? &G.aero : NULL;
}

/* the dress-up screens' aero cars: those the scene shows loaded (on the stock model, which
 * the scene keeps in), all freed once no scene shows one; NULL while one is not in */
static int menu_aero_find(int pack, int set) {
  for (int k = 0; k < MENU_AERO_SLOTS; ++k)
    if (G.menu_aero_in[k] && G.menu_aero_pack[k] == pack && G.menu_aero_set[k] == set) return k;
  return -1;
}

/* car pack `pack` in aero set `set` loaded into a free slot (if not in, and one is free) */
static void menu_aero_want(int pack, int set) {
  if (set < 1 || set > 3 || !car_pack(pack) || menu_aero_find(pack, set) >= 0) return;
  for (int k = 0; k < MENU_AERO_SLOTS; ++k)
    if (!G.menu_aero_in[k]) {
      G.menu_aero_pack[k] = pack, G.menu_aero_set[k] = set;
      G.menu_aero_in[k] = aero_load(&G.menu_aero[k], pack, set) ? 1 : -1;
      return;
    }
}

static void menu_aero_sync(const menu_scene *s) {
  int pack[MENU_AERO_SLOTS], set[MENU_AERO_SLOTS], n = 0;
  for (int i = 0; s && i < s->n; ++i) {
    int a = s->cars[i].aero, dup = 0;
    if (a < 1 || a > 3 || !car_pack(s->cars[i].pack)) continue;
    for (int j = 0; j < n; ++j) dup |= pack[j] == s->cars[i].pack && set[j] == a;
    if (!dup && n < MENU_AERO_SLOTS) pack[n] = s->cars[i].pack, set[n++] = a;
  }
  /* those no longer shown let go, then those shown and not in loaded into the free slots */
  for (int k = 0; k < MENU_AERO_SLOTS; ++k) {
    int keep = 0;
    for (int j = 0; j < n; ++j) keep |= G.menu_aero_pack[k] == pack[j] && G.menu_aero_set[k] == set[j];
    if (G.menu_aero_in[k] && !keep) {
      if (G.menu_aero_in[k] == 1) wpk_free(&G.menu_aero[k]);
      G.menu_aero_in[k] = 0;
    }
  }
  for (int j = 0; j < n; ++j) menu_aero_want(pack[j], set[j]);
}

static wpk_scene *menu_aero(int pack, int set) {
  int k = menu_aero_find(pack, set);
  return k >= 0 && G.menu_aero_in[k] == 1 ? &G.menu_aero[k] : NULL;
}

/* Dress-up wheels (menu_state.wheel's value; wheels/<name>_<set>.wpk, tools/stage/
 * pack_cars.py --wheels, a few hundred KB each), WHEEL_SLOTS of them in GL, the one wanted
 * least lately let go for another. wheel_slot finds or opens one's slot (-1 for the car's
 * own, or no slot free of `keep`); wheel_load_step takes it a texture or buffer, then a
 * first draw, further (1 once ready). */
static int wheel_find(int wheel) {
  for (int k = 0; wheel && k < WHEEL_SLOTS; ++k)
    if (G.wheel_state[k] != WHEEL_FREE && G.wheel_id[k] == wheel) return k;
  return -1;
}

static int wheel_slot(int wheel, const int *keep, int n_keep) {
  int k = wheel_find(wheel), set;
  if (k >= 0 || !wheel) return k;
  const char *name = menu_wheel_pack(wheel, &set);
  if (!name) return -1;
  for (int i = 0; i < WHEEL_SLOTS; ++i) {
    int kept = G.wheel_state[i] != WHEEL_FREE && G.wheel_id[i] == G.wheel_shown;
    for (int j = 0; j < n_keep; ++j) kept |= G.wheel_state[i] != WHEEL_FREE && G.wheel_id[i] == keep[j];
    if (!kept && (k < 0 || G.wheel_state[i] == WHEEL_FREE || (G.wheel_state[k] != WHEEL_FREE && G.wheel_used[i] < G.wheel_used[k])))
      k = i;
  }
  if (k < 0) return -1;
  if (G.wheel_state[k] != WHEEL_FREE && G.wheel_state[k] != WHEEL_MISSING) wpk_free(&G.wheel[k]);
  char path[1200];
  snprintf(path, sizeof path, "%s/wheels/%s_%d.wpk", G.data_dir, name, set);
  G.wheel_id[k] = wheel;
  G.wheel_state[k] = wpk_open(&G.wheel[k], path) == 0 ? WHEEL_UP : WHEEL_MISSING;
  if (G.wheel_state[k] == WHEEL_MISSING) fprintf(stderr, "no %s\n", path);
  return k;
}

static int wheel_load_step(int k) {
  if (G.wheel_state[k] == WHEEL_UP && wpk_upload(&G.wheel[k], 1)) G.wheel_state[k] = WHEEL_WARM;
  else if (G.wheel_state[k] == WHEEL_WARM) {
    GLint screen_fb = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &screen_fb);
    glBindFramebuffer(GL_FRAMEBUFFER, load_fb(0));
    render_warm_begin();
    if (!wpk_warm(&G.wheel[k], 1)) G.wheel_state[k] = WHEEL_READY;
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)screen_fb);
  }
  return G.wheel_state[k] == WHEEL_READY;
}

/* the wheel's model in GL, loaded whole now if it is not (the race's start); NULL for the
 * car's own or a missing file */
static wpk_scene *wheel_model(int wheel) {
  int k = wheel_slot(wheel, NULL, 0);
  if (k < 0) return NULL;
  while (G.wheel_state[k] == WHEEL_UP || G.wheel_state[k] == WHEEL_WARM) wheel_load_step(k);
  G.wheel_used[k] = ++G.wheel_tick;
  return G.wheel_state[k] == WHEEL_READY ? &G.wheel[k] : NULL;
}

/* The menus' wheels: those of the cars on show, then those the screen may show next
 * (menu_wheel_next), get a slot; one step of loading a frame goes to the first not ready.
 * On the dress-up screens a car whose wheel is still loading shows the one shown before
 * (the change screen's cursor does not wait), or till there is none its own; elsewhere the
 * car's wheel loads whole at once, as before. */
#define WHEEL_WANTS 12
static void wheel_sync(menu_scene *shown) {
  int want[WHEEL_WANTS], n = 0;
  if (G.menu.screen != MENU_TG_WHEEL && G.menu.screen != MENU_TG_WEAR && G.menu.screen != MENU_TG_GET)
    for (int i = 0; shown && i < shown->n; ++i)
      if (shown->cars[i].wheel) wheel_model(shown->cars[i].wheel);
  for (int i = 0; shown && i < shown->n && n < WHEEL_WANTS; ++i)
    if (shown->cars[i].wheel) want[n++] = shown->cars[i].wheel;
  n += menu_wheel_next(&G.menu, want + n, WHEEL_WANTS - n);
  int stepped = 0, opened = 0;
  for (int i = 0; i < n; ++i) {
    int k = wheel_find(want[i]);
    if (k < 0 && !opened) k = wheel_slot(want[i], want, n), opened = 1; /* (a file a frame) */
    if (k < 0) continue;
    G.wheel_used[k] = ++G.wheel_tick;
    if (!stepped && G.wheel_state[k] != WHEEL_READY && G.wheel_state[k] != WHEEL_MISSING) {
      wheel_load_step(k);
      stepped = 1;
    }
  }
  for (int i = 0; shown && i < shown->n; ++i) {
    int w = shown->cars[i].wheel, k = wheel_find(w);
    if (!w || (k >= 0 && G.wheel_state[k] == WHEEL_READY)) G.wheel_shown = w;
    else if (wheel_find(G.wheel_shown) >= 0 && G.wheel_state[wheel_find(G.wheel_shown)] == WHEEL_READY) shown->cars[i].wheel = G.wheel_shown;
    else shown->cars[i].wheel = 0;
  }
}

/* A wheel's GET screen model (wheels/<menu3d>.wpk, pack_cars.py --wheels), in GL while the
 * screen is up (0: let it go). NULL if missing. */
static wpk_scene *get_wheel_model(int wheel) {
  if (wheel != G.get_wheel_id) {
    if (G.get_wheel_ok) wpk_free(&G.get_wheel);
    G.get_wheel_ok = 0;
    G.get_wheel_id = wheel;
    const char *name = menu_wheel_get(wheel);
    char path[1200];
    if (name) {
      snprintf(path, sizeof path, "%s/wheels/%s.wpk", G.data_dir, name);
      if (wpk_open(&G.get_wheel, path) != 0) {
        fprintf(stderr, "no %s\n", path);
      } else {
        wpk_upload(&G.get_wheel, (size_t)-1);
        wpk_warm(&G.get_wheel, 1 << 20);
        G.get_wheel_ok = 1;
      }
    }
  }
  return G.get_wheel_ok ? &G.get_wheel : NULL;
}

/* that wheel on a car of pack `pack` in aero set `aero` (src/carwheel.h), once in GL */
static void draw_wheels(int wheel, int pack, int aero, const mat4 *vp, const mat4 *model, const float *eye,
                        int pass, render_params *rp) {
  int k = wheel_find(wheel);
  if (k >= 0 && G.wheel_state[k] == WHEEL_READY && pack >= 0 && pack < CARDB_NPACKS && aero >= 0 && aero <= 3)
    wpk_draw_wheels(&G.wheel[k], vp, model, eye, pass, rp, CARWHEEL[pack][aero]);
}

/* the car wears that wheel: its WPK_CTIRE tyres show */
static int wheel_on(int wheel) {
  int k = wheel_find(wheel);
  return k >= 0 && G.wheel_state[k] == WHEEL_READY;
}

/* The car select's model of the rival's ECarType (its first colour): its tyres, and its
 * look when the rival's own model is missing; -1 for no rival. */
/* rival rows: RIVALDB's, then the 10-in-a-row's characters (TENDB_CHAR) after them;
 * TEN_FIRST stands for the chosen level's rivals (rival_rows) */
#define RACE_ROWS CAR_TEN_RIVALS
#define TEN_ROW(ch) (RIVALDB_N + (ch))
#define TEN_FIRST TEN_ROW(TENDB_NCHAR)
/* the 10-in-a-row's courses (clEffTenCourseType) by city, and the time attack's course of
 * each (its map name and city; the race's own start comes from GetTenFightRaceField) */
static const int TEN_CITY[10] = { CAR_TOKYO, CAR_TOKYO, CAR_TOKYO, CAR_TOKYO, CAR_TOKYO, CAR_TOKYO,
                                  CAR_NAGOYA, CAR_OSAKA, CAR_HAKONE, CAR_FUKUOKA };
static const int TEN_TA_COURSE[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 9, 8 };
static int ten_ta_course(void) { return TEN_TA_COURSE[menu_ten_course(&G.menu)]; }

/* the race's rival: the story's chosen episode's (the rival select's story_pick) once on its
 * title page, else a test's (WMMT_RIVAL) or none */
/* The story ending's cars (clRaceThread::initEnding: clEffStoryType 111's getCar, model 247 the
 * 964 RS "Blackbird", ECarType 1, twice; getChara 109 and 108), on its series 145 */
#define ENDING_SERIES 145
static const int ENDING_CHARA[2] = { 109, 108 };
static int ending_row(void) {
  for (int i = 0; i < RIVALDB_N; ++i)
    if (RIVALDB[i].model == 247) return i;
  return -1;
}

static int story_rival(void) {
  if (G.menu.ending == 2 && G.menu.screen == MENU_PRE) return ending_row();
  if (G.menu.game == MENU_GAME_TEN && G.menu.screen == MENU_PRE) return TEN_FIRST;
  if (G.menu.game != MENU_GAME_STORY || G.menu.screen != MENU_PRE || G.menu.story_pick < 0) return G.rival_env;
  int pick = G.menu.story_pick;
#ifdef WMMT_EGL
  if (getenv("WMMT_STORY_SEL")) pick = atoi(getenv("WMMT_STORY_SEL")); /* test: this story select */
#endif
  for (int i = 0; i < RIVALDB_N; ++i)
    if (RIVALDB[i].select == pick && RIVALDB[i].race == G.story_race && RIVALDB[i].slot == 0) return i;
  return G.rival_env;
}

/* The rivals of the race whose first rival is RIVALDB row `first`: it and the rows after it
 * of the same select and race (clEffStoryType: up to 3 in a race). */
static int rival_rows(int first, int rows[RACE_ROWS]) {
  int n = 0;
  if (first == TEN_FIRST) { /* the 10-in-a-row level's (CharacterSet10) */
    int lv = G.menu.ten_level;
    for (int i = 0; i < TENDB_RIVALS[lv]; ++i) rows[n++] = TEN_ROW(TENDB_SET[lv][i]);
    return n;
  }
  for (int i = first; first >= 0 && i < RIVALDB_N && n < CAR_RIVALS; ++i) {
    if (RIVALDB[i].select != RIVALDB[first].select || RIVALDB[i].race != RIVALDB[first].race) break;
    rows[n++] = i;
  }
  return n;
}

/* The select's next race after `race` (-1 none): its first rival's RIVALDB row. */
static int story_next_race(int select, int race) {
  for (int i = 0; i < RIVALDB_N; ++i)
    if (RIVALDB[i].select == select && RIVALDB[i].race == race + 1 && RIVALDB[i].slot == 0) return i;
  return -1;
}

static int rival_type(int rival) {
  return rival >= TEN_ROW(0) ? TENDB_CHAR[rival - TEN_ROW(0)].type : RIVALDB[rival].type;
}

static int rival_base(int rival, int *paint) {
  *paint = 0;
  if (rival < 0 || rival >= TEN_ROW(TENDB_NCHAR)) return -1;
  for (int mk = 0; mk < CARDB_ALL; ++mk)
    for (int i = 0; i < CARDB_NCARS[mk]; ++i)
      for (int j = 0; j < CARDB_CARS[mk][i].ncolors; ++j)
        if (CARDB_CARS[mk][i].colors[j].type == rival_type(rival)) {
          *paint = CARDB_CARS[mk][i].colors[j].paint;
          return CARDB_CARS[mk][i].colors[j].pack;
        }
  return -1;
}

/* The rival's look: its own model (data/chara), else rival_base's; -1 for no rival. */
static int rival_look(int rival, int *paint) {
  int base = rival_base(rival, paint);
  int chara = rival >= TEN_ROW(0) ? TENDB_CHAR[rival - TEN_ROW(0)].chara : RIVALDB[rival].chara;
  if (base < 0 || chara < 0) return base;
  *paint = 0;
  return CARDB_NPACKS + chara;
}

/* What the menus want loaded: the car models, most wanted first. The cars on screen, then
 * the makers up to half the list away from the one on show (or under the maker select's
 * cursor), nearest first, each its carousel (every car's first colour) before its other
 * colours; after a memory warning no further than its neighbours. Up to the car select's
 * end (the attract loop and the boot logos included) the cars on screen and the maker on
 * show come before the course (`first` of them), so that the car select finds them in; in
 * the car select the other models come before warming the course too. */
static void load_wants_now(const menu_scene *shown, load_wants *w) {
  unsigned char listed[NPACKS] = { 0 };
  w->n = 0;
  /* the OPTION screen loads as the screen it came from, the save screens as the maker select ahead */
  int ahead = G.menu.screen == MENU_SAVE || G.menu.screen == MENU_HAVE || G.menu.screen == MENU_NAME ||
              (G.menu.screen == MENU_OPTION && G.menu.opt_from == MENU_SAVE);
  int screen = G.menu.screen == MENU_OPTION || ahead ? MENU_MAKER : G.menu.screen;
  int sel = ahead ? G.menu.maker : G.menu.screen == MENU_OPTION ? G.menu.opt_sel : G.menu.sel;
#define WANT(k)                                                     \
  if ((k) >= 0 && (k) < NPACKS && !listed[k]) {               \
    listed[k] = 1;                                                  \
    w->order[w->n++] = (k);                                         \
  }
  for (int i = 0; shown && i < shown->n; ++i) WANT(shown->cars[i].pack);
  /* a save picked (HAVE, its GARAGE): its cars next, the one on view first, before the
   * course (a blank garage waited for the whole course, Allan 10-25) */
  int owned = G.menu.screen == MENU_HAVE || G.menu.screen == MENU_GARAGE;
  /* (after a memory warning only the one on view: the others were freed again at once) */
  for (int i = 0; owned && i < (G.low_memory ? 1 : G.menu.n_own); ++i) {
    int k = (i + (G.menu.own_view > 0 ? G.menu.own_view : 0)) % G.menu.n_own;
    const menu_own *o = &G.menu.own[k];
    if (o->maker >= 0 && o->maker < CARDB_ALL && o->car >= 0 && o->car < CARDB_NCARS[o->maker])
      WANT(CARDB_CARS[o->maker][o->car].colors[o->color].pack);
  }
  int n_owned = w->n;
  /* past the car select the chosen car comes first, its paint too (the race title waits for it) */
  int chosen = screen > MENU_COLOR ? chosen_color()->pack : -1;
  int before = w->n;
  WANT(chosen);
  int rows[RACE_ROWS], n_rival = chosen >= 0 ? rival_rows(G.rival, rows) : 0;
  int rival_paint[RACE_ROWS], rival[RACE_ROWS];
  for (int i = 0; i < n_rival; ++i) {
    rival[i] = rival_look(rows[i], &rival_paint[i]);
    WANT(rival[i]);
  }
  int race_cars = w->n - before;
  /* the phone: only the maker on show, each car in its first paint */
  int reach = G.phone ? 0 : G.low_memory ? 1 : CARDB_MAKERS / 2, first = 0;
  int on_show = screen == MENU_MAKER ? sel : G.menu.maker;
  /* the hidden cars' page: its cars alone */
  if (on_show == CARDB_HIDDEN) {
    for (int all = 0; all < (G.phone ? 1 : 2); ++all)
      for (int i = 0; i < CARDB_NCARS[CARDB_HIDDEN]; ++i) {
        const cardb_car *c = &CARDB_CARS[CARDB_HIDDEN][i];
        for (int j = 0; j < (all ? c->ncolors : 1); ++j) WANT(c->colors[j].pack);
      }
    first = w->n;
    reach = -1;
  }
  for (int d = 0; d <= reach; ++d) {
    for (int sign = 1; sign >= -1; sign -= 2) {
      int maker = ((on_show + sign * d) % CARDB_MAKERS + CARDB_MAKERS) % CARDB_MAKERS;
      for (int all = 0; all < (G.phone ? 1 : 2); ++all)
        for (int i = 0; i < CARDB_NCARS[maker]; ++i) {
          const cardb_car *c = &CARDB_CARS[maker][i];
          for (int j = 0; j < (all ? c->ncolors : 1); ++j) WANT(c->colors[j].pack);
        }
    }
    if (d == 0) first = w->n;
  }
#undef WANT
  /* past the car select the cars on screen too (the garage's saved car: a count of the
   * race's cars alone left it behind the whole course, the phone's push108, the iPad's push118) */
  w->first = owned ? n_owned : screen <= MENU_COLOR ? first : before + race_cars;
  /* the boot's screens too: warming the course first (a minute of it) left the car select
   * of a maker not yet in waiting on its whole carousel the first time (push116) */
  w->cars_before_warm = screen <= MENU_COLOR;
  /* the other paints of the car under the cursor (the colour select's), or of the car chosen */
  w->n_paints = 0;
  int car = screen == MENU_CAR ? sel : G.menu.car;
  if (screen >= MENU_CAR && car >= 0 && car < CARDB_NCARS[G.menu.maker]) {
    const cardb_car *c = &CARDB_CARS[G.menu.maker][car];
    for (int j = 0; j < c->ncolors; ++j) {
      int k = c->colors[j].pack, dup = 0;
      for (int i = 0; i < w->n_paints; ++i) dup |= w->paints[i] == k;
      if (!dup && k >= 0 && k < NPACKS) w->paints[w->n_paints++] = k;
    }
  }
  if (chosen >= 0) {
    w->n_paints = 0;
    if (chosen_color()->paint) w->paints[w->n_paints++] = chosen;
    for (int i = 0; i < n_rival; ++i) {
      int dup = rival[i] < 0 || !rival_paint[i] || rival[i] == chosen;
      for (int j = 0; j < w->n_paints; ++j) dup |= w->paints[j] == rival[i];
      if (!dup) w->paints[w->n_paints++] = rival[i];
    }
  }
  /* and the paints of the cars on show (the save's car in GARAGE: not the chosen one, so
   * car_paints_trim freed its paint again and again, NOW LOADING for minutes, Allan 10-25) */
  for (int i = 0; shown && i < shown->n; ++i) {
    int k = shown->cars[i].pack, dup = !shown->cars[i].paint || k < 0 || k >= NPACKS;
    for (int j = 0; j < w->n_paints; ++j) dup |= w->paints[j] == k;
    if (!dup && w->n_paints < (int)(sizeof w->paints / sizeof w->paints[0])) w->paints[w->n_paints++] = k;
  }
  w->paints_first = screen <= MENU_COLOR || chosen >= 0;
  w->hold_track = G.track_old.n_batch || G.track_old.n_tex;
  w->course = G.menu.course >= 0 && G.menu.course < CAR_COURSES ? G.menu.course : 0;
}

/* the first of w's paints to load whose model is in GL, or -1 */
static int paint_next(const load_wants *w) {
  for (int i = 0; i < w->n_paints; ++i) {
    int k = w->paints[i];
    if (G.pack_state[k] == 1 && G.paint_state[k] != 1) return k;
  }
  return -1;
}

/* the first of w's car models from..to that is still to load, or -1 */
static int want_next(const load_wants *w, int from, int to) {
  for (int i = from; i < to; ++i) {
    int k = w->order[i];
    if (G.pack_state[k] != 1 && G.pack_state[k] != -1) return k;
  }
  return -1;
}

/* One step of the loading, in GL context ctx: the cars w wants first, then the course (the
 * race needs it), then the other cars before or after warming the course (load_wants_now).
 * 0 when there is nothing left to do. */
static int load_unit(const load_wants *w, int ctx) {
  int k = want_next(w, 0, w->first);
  if (k >= 0) {
    pack_step(k, ctx);
    return 1;
  }
  k = paint_next(w);
  if (k >= 0 && w->paints_first) {
    paint_step(k, ctx);
    return 1;
  }
  if (!G.track_ready && !w->hold_track) {
    if (wpk_upload(&G.track, TRACK_STEP_BYTES)) {
      track_set(1);
      fprintf(stderr, "course ready\n");
    }
    return 1;
  }
  k = want_next(w, 0, w->n);
  if (k >= 0 && w->cars_before_warm) {
    pack_step(k, ctx);
    return 1;
  }
  if (G.track_ready) {
    if (warm_step(w, ctx)) return 1;
    if (G.track_ready == 1) track_set(2); /* the race's title page waits for this */
  }
  if (k >= 0) {
    pack_step(k, ctx);
    return 1;
  }
  k = paint_next(w);
  if (k >= 0) {
    paint_step(k, ctx);
    return 1;
  }
  return 0;
}

/* all of pack k, in the app's context (the race's car, chosen before it was in), and its
 * other paints if `paints` */
static void pack_load_now(int k, int paints) {
  load_pause();
  GLint screen_fb = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &screen_fb);
  while (k >= 0 && k < NPACKS && G.pack_state[k] != 1 && G.pack_state[k] != -1) pack_step(k, 0);
  while (paints && k >= 0 && k < NPACKS && G.pack_state[k] == 1 && G.paint_state[k] != 1) paint_step(k, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)screen_fb);
}

/* whether car model k's paint is in GL as the menus see it (the FD3S standing in has all) */
static int paint_ready(int k, int paint) {
  if (!paint || k < 0 || k >= NPACKS || G.seen.pack[k] < 0) return 1;
  return G.seen.paint[k] == 1;
}

/* whether every car of a car select scene is in GL with its paint (menu_draw3d shows them all or none) */
static int scene_ready(const menu_scene *s) {
  for (int i = 0; s && i < s->n; ++i)
    if (!car_pack(s->cars[i].pack) || !paint_ready(s->cars[i].pack, s->cars[i].paint)) return 0;
  return 1;
}

/* Free car models: all but the chosen car's (the race), or after a memory warning all
 * but the makers load_wants_now still wants and the cars it wants first (w: the cars on
 * show and the save's cars in GARAGE, of any maker; freed, they loaded again and again and
 * the garage stayed on NOW LOADING, Allan 10-25) */
static void car_packs_trim(int keep_near, const load_wants *w) {
  unsigned char want[NPACKS] = { 0 };
  want[chosen_color()->pack] = 1;
  for (int i = 0; w && i < w->first && i < w->n; ++i) want[w->order[i]] = 1;
  int rows[RACE_ROWS], n_rival = rival_rows(G.rival, rows);
  if (G.menu.ending == 2 && G.rival >= 0) rows[0] = rows[1] = G.rival, n_rival = 2;
  for (int i = 0; i < n_rival; ++i) {
    int paint, rival = rival_look(rows[i], &paint);
    if (rival >= 0) want[rival] = 1;
  }
  for (int i = 0; G.vs_race && i < G.n_rival; ++i) want[G.rival_pack[i]] = 1;
  for (int d = G.phone ? 0 : -1; keep_near && d <= (G.phone ? 0 : 1); ++d) {
    int maker = ((G.menu.maker + d) % CARDB_MAKERS + CARDB_MAKERS) % CARDB_MAKERS;
    if (G.menu.maker == CARDB_HIDDEN) {
      if (d) continue;
      maker = CARDB_HIDDEN;
    }
    for (int i = 0; i < CARDB_NCARS[maker]; ++i) {
      const cardb_car *c = &CARDB_CARS[maker][i];
      for (int j = 0; j < c->ncolors; ++j) want[c->colors[j].pack] = 1;
    }
  }
  /* (a look at the thread's state first, so that nothing pauses it when nothing goes) */
  int any = 0;
  for (int k = 0; k < NPACKS; ++k) any |= G.pack_state[k] > 0 && !want[k];
  if (!any) return;
  load_pause();
  for (int k = 0; k < NPACKS; ++k)
    if (G.pack_state[k] > 0 && !want[k]) {
      wpk_free(&G.packs[k]);
      pack_set(k, 0);
      paint_set(k, 0);
    }
  load_sync(NULL);
}

/* Free the other paints of all car models but the chosen car's and those w wants (NULL:
 * none), once more than a few cars have theirs: the car select's cursor goes past many. */
#define PAINTED_MAX 4
static void car_paints_trim(const load_wants *w, int force) {
  unsigned char keep[NPACKS] = { 0 };
  keep[chosen_color()->pack] = 1;
  int rows[RACE_ROWS], n_rival = rival_rows(G.rival, rows);
  for (int i = 0; i < n_rival; ++i) {
    int paint, rival = rival_look(rows[i], &paint);
    if (rival >= 0) keep[rival] = 1;
  }
  for (int i = 0; G.vs_race && i < G.n_rival; ++i) keep[G.rival_pack[i]] = 1;
  for (int i = 0; w && i < w->n_paints; ++i) keep[w->paints[i]] = 1;
  int painted = 0, extra = 0;
  for (int k = 0; k < NPACKS; ++k) {
    painted += G.paint_state[k] != 0;
    extra += G.paint_state[k] != 0 && !keep[k];
  }
  if (!extra || (!force && painted <= PAINTED_MAX)) return;
  load_pause();
  for (int k = 0; k < NPACKS; ++k)
    if (G.paint_state[k] != 0 && !keep[k]) {
      if (G.pack_state[k] > 0) wpk_free_paints(&G.packs[k]);
      paint_set(k, 0);
    }
  load_sync(NULL);
}

void app_memory_warning(void) {
  fprintf(stderr, "memory warning: car models of far makers freed\n");
  perf_file();
  if (G.perf) {
    fprintf(G.perf, "memory warning on screen %d (%s), %.0f MB resident\n", G.menu.screen, G.in_menu ? "menus" : "race",
            resident_mb());
    fflush(G.perf);
  }
  G.low_memory = 1;
  car_packs_trim(G.in_menu, G.in_menu ? &G.wants : NULL);
}

/* Spend what is left of this frame's loading time (no loading thread). More on the race's
 * title page, which is there for the loading, in the boot logos, which cannot be skipped
 * for it, and while the car select waits for its cars (it shows them all at once).
 * LOAD_MIN_MS of it at least, so loading always moves on. The driver does much of an
 * upload's work later (a texture's on its first use, the rest when the frame goes to the
 * GPU), so each step waits for it (glFinish) to count it: the steps then stop in time and
 * the menus keep their 60 fps. */
static void load_step(double frame_start) {
  double budget = G.load_hurry ? LOAD_PRE_MS : LOAD_MS;
  GLint screen_fb = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &screen_fb);
  glBindFramebuffer(GL_FRAMEBUFFER, load_fb(0));
  glFinish(); /* the menu's own frame */
  double t0 = now_ms();
  for (double t = t0; t - frame_start < budget || t - t0 < LOAD_MIN_MS; t = now_ms()) {
    if (!load_unit(&G.wants, 0)) break;
    glFinish();
  }
  glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)screen_fb);
}

/* The thread loads between frames: from the end of a menu frame (app_frame_done, after its
 * presentRenderbuffer) till LOAD_WINDOW_MS after the frame's start, the vsync being at 16.7;
 * more when the menus wait for it. Run alongside the frames, its uploads and first draws went
 * to the GPU and the driver in the middle of theirs, and the menus dropped most of their
 * frames while it warmed the course. */
#define LOAD_WINDOW_MS 13.5
#define LOAD_HURRY_MS 16.0
#define LOAD_LATE_MS 2.0 /* after a late frame */
/* the car select's NOW LOADING: the thread's least time after a frame, the menus at 30 fps
 * meanwhile (after a vsync wait in presentRenderbuffer it had 2 ms of each frame: four car
 * models in ten seconds) */
#define LOAD_WAIT_MS 20.0

void app_frame_done(double present_ms) {
  double t = now_ms();
  G.present_ms = present_ms;
  pthread_mutex_lock(&G.load_mx);
  double until = G.frame_start + (G.load_hurry ? LOAD_HURRY_MS : LOAD_WINDOW_MS);
  double least = t + (G.load_wait ? LOAD_WAIT_MS : LOAD_LATE_MS);
  G.load_until = until > least ? until : least;
  pthread_cond_broadcast(&G.load_cv);
  pthread_mutex_unlock(&G.load_mx);
}

void app_loader(void) {
  pthread_mutex_lock(&G.load_mx);
  G.loader_up = 1;
  pthread_mutex_unlock(&G.load_mx);
  for (;;) {
    pthread_mutex_lock(&G.load_mx);
    G.load_busy = 0;
    pthread_cond_broadcast(&G.load_cv); /* load_pause waits for this */
    while (!G.load_run || G.load_idle || now_ms() >= G.load_until) pthread_cond_wait(&G.load_cv, &G.load_mx);
    G.load_busy = 1;
    load_wants w = G.wants;
    pthread_mutex_unlock(&G.load_mx);
    double t0 = now_ms();
    int did = load_unit(&w, 1);
    pthread_mutex_lock(&G.load_mx);
    int away = G.away;
    pthread_mutex_unlock(&G.load_mx);
    if (away) glFinish(); /* the app goes to the background: nothing of the thread's left in flight */
    else glFlush();       /* for the app's context, which may use what it made after a pause */
    pthread_mutex_lock(&G.load_mx);
    G.load_units++;
    G.load_ms += now_ms() - t0;
    if (!did) G.load_idle = 1;
    pthread_mutex_unlock(&G.load_mx);
  }
}

void app_suspend(int away) {
  pthread_mutex_lock(&G.load_mx);
  G.away = away;
  pthread_mutex_unlock(&G.load_mx);
  if (!away) return;
  load_pause();
  /* a race comes back under its pause menu (one already open stays the one) */
  if (!G.in_menu && !G.paused) G.hud.pause_pressed++;
  glFinish(); /* none of the app's GL work left in flight in the background */
  if (G.perf) fflush(G.perf);
}

/* ---- the stickers (vinyls): clCarModelData::constructBody blends the car's vinyl onto its
 * paint's page 1 (nsTextureReplace::TextureReplace); here the same, a composite texture made
 * from data/vinyl/<car>.vpk (tools/stage/pack_vinyl.py) the first time a car is drawn with it,
 * the last few kept (the change screen goes back and forth) */
#define VINYL_KEEP 5 /* (a versus: up to four cars at once) */
static struct {
  int pack, body, paint, vinyl, cb, used;
  uint32_t tex; /* 0: the car has no such vinyl */
  char page1[48];
} VIN[VINYL_KEEP];
static int vin_clock;

/* the composite of car pack k (CARDB_PACKS) on body 0..3 in paint `paint` with sticker `vinyl`
 * (menu_state.vinyl's: VinylTexture's entry, 0 none), the bonnet (carbon past 1: page1_cb), and
 * the name of the texture it stands in for; 0 if none (or the car is still loading) */
static uint32_t vinyl_tex(int k, int body, int paint, int vinyl, int bonnet, const char **page1) {
  int cb = bonnet > 1;
  if (vinyl <= 0 || k < 0 || k >= CARDB_NPACKS) return 0;
  body &= 3;
  for (int i = 0; i < VINYL_KEEP; ++i)
    if (VIN[i].used && VIN[i].pack == k && VIN[i].body == body && VIN[i].paint == paint && VIN[i].vinyl == vinyl &&
        VIN[i].cb == cb) {
      VIN[i].used = ++vin_clock;
      *page1 = VIN[i].page1;
      return VIN[i].tex;
    }
  wpk_scene *stock = car_pack(k);
  if (!stock || (stock == &G.fd3s && !same_name("maz_FD3S", CARDB_PACKS[k]))) return 0;
  int slot = 0;
  for (int i = 1; i < VINYL_KEEP; ++i)
    if (VIN[i].used < VIN[slot].used) slot = i;
  if (VIN[slot].tex) glDeleteTextures(1, &VIN[slot].tex);
  memset(&VIN[slot], 0, sizeof VIN[slot]);
  VIN[slot].pack = k, VIN[slot].body = body, VIN[slot].paint = paint, VIN[slot].vinyl = vinyl, VIN[slot].cb = cb;
  VIN[slot].used = ++vin_clock;
  *page1 = VIN[slot].page1;
  char path[1200];
  snprintf(path, sizeof path, "%s/vinyl/%s.vpk", G.data_dir, CARDB_PACKS[k]);
  FILE *f = fopen(path, "rb");
  if (!f) return 0;
  unsigned char head[12 + 192 + 32];
  uint32_t n, np;
  if (fread(head, 1, sizeof head, f) != sizeof head || memcmp(head, "VPK1", 4) != 0) {
    fclose(f);
    return 0;
  }
  memcpy(&n, head + 4, 4);
  memcpy(&np, head + 8, 4);
  memcpy(VIN[slot].page1, head + 12 + 48 * body, 47);
  int works = paint >= 0 && paint < 32 ? head[12 + 192 + paint] : 1;
  /* the entry: the vinyl on this body, the works livery's of the paint's WorksIndex, the
   * carbon bonnet's if it has one */
  unsigned char e[12], best[12];
  int found = 0;
  for (uint32_t i = 0; i < n && fread(e, 1, 12, f) == 12; ++i) {
    if (e[0] != body || e[1] != vinyl || (e[2] && e[2] != works)) continue;
    if (e[3] == cb || !found) memcpy(best, e, 12), found = 1 + (e[3] == cb);
  }
  uint32_t plane, off_size[2];
  uLongf want = 0;
  unsigned char *z = NULL, *px = NULL;
  if (found) {
    memcpy(&plane, best + 8, 4);
    want = (uLongf)512 * 512 * (best[7] ? 1 : 4);
    if (plane < np && fseek(f, (long)(sizeof head + 12 * n + 8 * plane), SEEK_SET) == 0 && fread(off_size, 4, 2, f) == 2 &&
        (z = malloc(off_size[1])) && (px = malloc(want)) && fseek(f, (long)off_size[0], SEEK_SET) == 0 &&
        fread(z, 1, off_size[1], f) == off_size[1]) {
      uLongf got = want;
      if (uncompress(px, &got, z, off_size[1]) != Z_OK || got != want) found = 0;
    } else {
      found = 0;
    }
  }
  fclose(f);
  free(z);
  if (found) {
    char name[64];
    int idx = -1;
    if (paint > 0) {
      snprintf(name, sizeof name, "p%d:%s", paint, VIN[slot].page1);
      idx = wpk_find_texture(stock, name);
    }
    if (idx < 0) idx = wpk_find_texture(stock, VIN[slot].page1);
    double t0 = now_ms();
    VIN[slot].tex = wpk_texture_over(stock, idx, px, 512, 512, best[7] ? best + 4 : NULL);
    fprintf(stderr, "vinyl %d on %s body %d paint %d: %s, %.1f ms\n", vinyl, CARDB_PACKS[k], body, paint,
            VIN[slot].tex ? "made" : "failed", now_ms() - t0);
  }
  free(px);
  return VIN[slot].tex;
}

/* s (the car's stock or aero scene) drawn with composite `tex` for its texture page1: the
 * index swapped, its GL name before in *old (vinyl_off puts it back) */
static int vinyl_on(wpk_scene *s, uint32_t tex, const char *page1, uint32_t *old) {
  int i = tex && s ? wpk_find_texture(s, page1) : -1;
  if (i >= 0) *old = s->textures[i], s->textures[i] = tex;
  return i;
}
static void vinyl_off(wpk_scene *s, int i, uint32_t old) {
  if (i >= 0) s->textures[i] = old;
}

/* the car select's 3D cars, with the arcade's menu camera */
static void eye_in(const mat4 *m, const float *p, float *out);

static void menu_draw3d(void *ctx) {
  const menu_scene *s = ctx;
  float aspect = (float)G.fb_w / (float)G.fb_h;
  float fovy = 2.0f * atanf(tanf(s->fov_x * 0.5f) / (4.0f / 3.0f));
  mat4 proj = mat4_perspective(fovy, aspect, 5.0f, 20000.0f);
  const float up[3] = { 0, 1, 0 };
  mat4 view = mat4_look_at(s->eye, s->at, up);
  mat4 vp = mat4_mul(proj, view);
  render_params rp = { .fog_far = 2e6f, .fog_near = 1e6f, .range = { 1e9f, 1e9f, 1e9f } };
  set_car_light(&rp, menu_env_light(s->env), &view);
  /* all the cars at once, not each as it comes in (load_step hurries them meanwhile) */
  if (!scene_ready(s)) return;
  glClear(GL_DEPTH_BUFFER_BIT);
  wpk_scene *scene[MENU_MAX_CARS];
  for (int i = 0; i < s->n; ++i) {
    scene[i] = car_pack(s->cars[i].pack);
    if (scene[i] && s->cars[i].aero) {
      wpk_paint(scene[i], s->cars[i].paint);
      scene[i] = menu_aero(s->cars[i].pack, s->cars[i].aero);
      if (scene[i]) wpk_share_sync(scene[i]);
    }
  }
  /* clSpriteAnimeCPModel's own scene: the eye at (0, 0, 1000) on the origin, the frustum
   * (60 degrees across the 640-wide screen, 5 to 20000) on the axis through its dot */
  const float cp_eye[3] = { 0, 0, 1000.0f }, cp_at[3] = { 0, 0, 0 };
  mat4 cp_view = mat4_look_at(cp_eye, cp_at, up);
  render_begin(&rp);
  for (int pass = 0; pass < WPK_PASSES; ++pass) {
    render_blend_pass(pass == WPK_BLEND);
    for (int i = 0; i < s->n; ++i) {
      if (!scene[i]) continue;
      if (!s->cars[i].aero) wpk_paint(scene[i], s->cars[i].paint);
      mat4 model;
      memcpy(model.m, s->cars[i].model, sizeof model.m);
      const float *e = s->eye;
      mat4 cvp = vp;
      if (s->cars[i].cp) {
        float h = tanf(0.52359878f) / 64.0f, x = s->cars[i].cp_x, y = s->cars[i].cp_y;
        float ew = G.edge / 1.6f; /* (a wide screen's sides) */
        cvp = mat4_mul(mat4_frustum((-x - ew) * h, (640.0f + ew - x) * h, (y - 480.0f) * h, y * h, 5.0f, 20000.0f), cp_view);
        e = cp_eye;
      }
      /* the eye in the car's frame (the model matrix a rotation, a scale and a translation) */
      const float *m = model.m;
      float k2 = m[0] * m[0] + m[1] * m[1] + m[2] * m[2];
      float d[3] = { e[0] - m[12], e[1] - m[13], e[2] - m[14] };
      float eye[3] = { (d[0] * m[0] + d[1] * m[1] + d[2] * m[2]) / k2, (d[0] * m[4] + d[1] * m[5] + d[2] * m[6]) / k2,
                       (d[0] * m[8] + d[1] * m[9] + d[2] * m[10]) / k2 };
      rp.mirror = s->cars[i].mirror, rp.wing = s->cars[i].wing, rp.bonnet = s->cars[i].bonnet;
      rp.wheel_custom = wheel_on(s->cars[i].wheel);
      rp.wheel_drop = s->cars[i].lift;
      /* the cars flying in: their corners' spin and steer (Battle_anm.igb's) */
      static const float still[4];
      rp.wheels = s->cars[i].turned ? (const float (*)[16])s->cars[i].tyre : NULL;
      rp.wheel_rot = still;
      rp.wheels_hub = s->cars[i].turned;
      const char *page1 = "";
      uint32_t vt = vinyl_tex(s->cars[i].pack, s->cars[i].aero, s->cars[i].paint, s->cars[i].vinyl, s->cars[i].bonnet, &page1), vold = 0;
      int vi = vinyl_on(scene[i], vt, page1, &vold);
      wpk_draw(scene[i], &cvp, &model, eye, pass, &rp);
      vinyl_off(scene[i], vi, vold);
      draw_wheels(s->cars[i].wheel, s->cars[i].pack, s->cars[i].aero, &cvp, &model, eye, pass, &rp);
    }
    rp.wheels = NULL;
    rp.wheels_hub = 0;
    if (s->get_wheel && s->get_wheel == G.get_wheel_id && G.get_wheel_ok) {
      mat4 model;
      memcpy(model.m, s->wheel_model, sizeof model.m);
      float eye[3];
      eye_in(&model, s->eye, eye);
      rp.mirror = rp.wing = rp.bonnet = rp.wheel_custom = 0;
      wpk_draw(&G.get_wheel, &vp, &model, eye, pass, &rp);
    }
  }
  /* the under neons (clMenuCar::setMatrix: clNeon::setPosture with (0, -3.1, 0) under the
   * car's matrix) */
  for (int i = 0; i < s->n; ++i) {
    if (!scene[i] || !s->cars[i].neon) continue;
    mat4 model, nproj = proj, nview = view;
    memcpy(model.m, s->cars[i].model, sizeof model.m);
    for (int j = 0; j < 3; ++j) model.m[12 + j] -= 3.1f * model.m[4 + j];
    if (s->cars[i].cp) {
      float h = tanf(0.52359878f) / 64.0f, x = s->cars[i].cp_x, y = s->cars[i].cp_y;
      float ew = G.edge / 1.6f;
      nproj = mat4_frustum((-x - ew) * h, (640.0f + ew - x) * h, (y - 480.0f) * h, y * h, 5.0f, 20000.0f);
      nview = cp_view;
    }
    lights_draw_neon(&nproj, &nview, model.m, s->cars[i].pack, s->cars[i].aero, s->cars[i].neon, &rp);
  }
  render_blend_pass(0);
}

/* clMiniatureCourse::update, each frame of a course select on one of Tokyo's cards (the
 * others show a picture, mapChange): the map, the roads and the card's marks; on another card
 * the camera and its aim move to the card's in 8 frames, a straight line ((8 - n) * 0.125 of
 * the way, from where the last card's were); the projection 30 degrees across (Tokyo's maps,
 * "sk"), 5 to 20000; the gradation and the run light (named *graShape, *runlightShape) drawn
 * without the depth test (init's node walk), the gradation's texture running by its
 * sequence, the run light's glow at its sequence's place (the shader turns it to the camera).
 * fade: the screen's, which the arcade lays over in black. */
#define MINI_MOVE 8

static void eye_in(const mat4 *m, const float *p, float *out);

static void mini_draw(void *ctx, float fade) {
  const menu_state *m = ctx;
  int card = m->sel;
  int ten = m->screen == MENU_TEN_COURSE, vs = m->screen == MENU_VS_AREA;
  if (vs) card = m->vs.item;
  if (!G.mini.n_batch || card < 0 || card >= (vs ? MINI_VS_CARDS : ten ? MINI_TEN_CARDS : MINI_TA_CARDS)) return;
  const mini_course *mc = vs ? &MINI_VS[card] : ten ? &MINI_TEN[card] : &MINI_TA[card];
  if (!mc->on) return;
  if (mc != G.mini_shown) {
    if (G.mini_shown) {
      memcpy(G.mini_from[0], G.mini_shown->eye, sizeof G.mini_from[0]);
      memcpy(G.mini_from[1], G.mini_shown->at, sizeof G.mini_from[1]);
      G.mini_move = MINI_MOVE;
    }
    G.mini_shown = mc;
  }
  float eye[3], at[3];
  float k = 1.0f;
  if (G.mini_move) k = (float)(MINI_MOVE - --G.mini_move) * 0.125f;
  for (int i = 0; i < 3; ++i) {
    eye[i] = G.mini_from[0][i] + (mc->eye[i] - G.mini_from[0][i]) * k;
    at[i] = G.mini_from[1][i] + (mc->at[i] - G.mini_from[1][i]) * k;
  }
  float aspect = (float)G.fb_w / (float)G.fb_h;
  float fovy = 2.0f * atanf(tanf(0.5236f * 0.5f) / (4.0f / 3.0f));
  mat4 proj = mat4_perspective(fovy, aspect, 5.0f, 20000.0f);
  const float up[3] = { 0, 1, 0 };
  mat4 view = mat4_look_at(eye, at, up);
  mat4 vp = mat4_mul(proj, view);
  mat4 ident = mat4_identity();
  /* the fade as fog: black by 1 - fade wherever it is */
  render_params rp = { .fog_far = 1e7f * fade, .fog_near = 1e7f * fade - 1e7f, .range = { 1e9f, 1e9f, 1e9f },
                       .object_visible = G.mini_visible, .n_objects = MINI_OBJECTS };
  glClear(GL_DEPTH_BUFFER_BIT);
  render_begin(&rp);
  memset(G.mini_visible, 0, sizeof G.mini_visible);
  G.mini_visible[MINI_MAP] = G.mini_visible[MINI_COURSE] = G.mini_visible[mc->macula] = 1;
  for (int pass = 0; pass < WPK_PASSES; ++pass) {
    render_blend_pass(pass == WPK_BLEND);
    wpk_draw(&G.mini, &vp, &ident, eye, pass, &rp);
  }
  int f = G.mini_frame++ % MINI_FRAMES;
  glDisable(GL_DEPTH_TEST);
  render_blend_pass(1);
  memset(G.mini_visible, 0, sizeof G.mini_visible);
  G.mini_visible[mc->gradation] = 1;
  rp.uv_shift[0] = mc->uv[f];
  wpk_draw(&G.mini, &vp, &ident, eye, WPK_BLEND, &rp);
  rp.uv_shift[0] = 0;
  G.mini_visible[mc->gradation] = 0;
  G.mini_visible[mc->runlight] = 1;
  /* facing the camera: the view's rows for its axes */
  mat4 bb = ident;
  for (int i = 0; i < 3; ++i) {
    bb.m[i] = view.m[4 * i];
    bb.m[4 + i] = view.m[4 * i + 1];
    bb.m[8 + i] = view.m[4 * i + 2];
    bb.m[12 + i] = mc->run[f][i];
  }
  float eye_bb[3];
  eye_in(&bb, eye, eye_bb);
  wpk_draw(&G.mini, &vp, &bb, eye_bb, WPK_BLEND, &rp);
  render_blend_pass(0);
  glEnable(GL_DEPTH_TEST);
}

/* frames the race's title page waits for the loading at most (it should never take this
 * long: a way out should something never finish) */
#define PRE_WAIT_MAX (20 * 60)
#define HUD_WARM_STEP 4 /* HUD sprites first-drawn a frame on the title page */

/* clRaceThread::initStory: the select's realtime movie before this race (src/storymovie.h), on
 * the race's course, its cars the race's rivals by their ECarType; the race (set up already)
 * starts over once it is done (movie_close) */
#define MOVIE_FADE 20 /* clRaceThread::race's setFade: 1/3 s */
/* a getFade's colour (clFader::draw: 1 atract_p_black, 2 Full_white) as hud_fade's grey */
static float fade_grey(int fade) { return fade == 2 ? 1.0f : 0.0f; }
static void movie_open(int select, int race, const int *rows, int n_rows) {
  const storymovie *mv = NULL;
  for (int i = 0; i < STORYMOVIE_N && !mv; ++i)
    if (STORYMOVIE[i].select == select && STORYMOVIE[i].race == race) mv = &STORYMOVIE[i];
  if (!mv || getenv("WMMT_NO_MOVIE")) return;
  car_rival cars[CAR_RIVALS];
  int used[RACE_ROWS] = { 0 };
  memset(cars, 0, sizeof cars);
  for (int k = 0; k < CAR_RIVALS; ++k) {
    G.movie_model[k] = NULL;
    G.movie_paint[k] = 0;
    cars[k].type = (unsigned)mv->cars[k];
    cars[k].spokes = 5;
    for (int i = 0; mv->cars[k] && i < n_rows && i < G.n_rival; ++i) {
      if (used[i] || RIVALDB[rows[i]].type != mv->cars[k]) continue;
      used[i] = 1;
      cars[k] = G.race_rv[i];
      G.movie_model[k] = G.rival_model[i];
      G.movie_paint[k] = G.rival_paint[i];
      break;
    }
  }
  float len = car_movie_start(&G.car, mv->movie, &G.race_spec, cars, CAR_RIVALS);
  fprintf(stderr, "story movie %d (select %d race %d): %.0f frames\n", mv->movie, select, race, len);
  if (len <= 1.0f) { /* its trails missing: the race at once */
    car_set_rivals(G.race_rv, G.race_nrv);
    car_start(&G.car, G.race_course, G.menu.manual, &G.race_spec);
    spark_reset();
    return;
  }
  G.movie_len = len;
  G.movie_frame = 0;
  G.movie_fade = mv->fade;
  G.movie_in = 0;
  car_movie_update(&G.car, 0);
  hud_demo_start(mv->demo);
}

/* the movie's end: the race from its start, out of the movie's fade */
static void movie_close(void) {
  car_movie_end();
  hud_demo_end();
  G.movie_len = 0;
  car_set_rivals(G.race_rv, G.race_nrv);
  if (car_start(&G.car, G.race_course, G.menu.manual, &G.race_spec) != 0) fprintf(stderr, "race after the movie failed to start\n");
  spark_reset();
  G.movie_in = G.movie_fade ? MOVIE_FADE : 0;
  G.race_frames = 0;
  G.time_shown = (int)G.car.race.time_left;
  G.top_kmh = 0;
}

/* ---- the versus (the port's: net.c; over a server, tools/vs_server.py; over Bluetooth or
 * the LAN, a device hosting with vs_host.c): the lobby while the menus show MENU_VS, then the
 * race the host starts (car_set_versus) on the arcade's own link code. Each player's profile
 * describes its car: maker, car, colour, power, handling (VS_PROFILE_CAR, all an older build
 * sends); then its name's five characters (namedb.h's, 255 past its end) for the lobby to
 * draw in the arcade's own glyphs (VS_PROFILE_NAME); then its title (src/title.h, u16) for the
 * race's boxes (VS_PROFILE_DRESS); then its dress-up (vs_dress_t's, a byte each) for the
 * others to draw it as it is. */
#define VS_PROFILE_CAR 5
#define VS_PROFILE_NAME 10
#define VS_PROFILE_DRESS 12
#define VS_PROFILE 19
enum { VS_GK_OFF, VS_GK_BROWSE, VS_GK_JOIN, VS_GK_HOST };
#define VS_SILENT 3.0 /* seconds without the server before the screen says so */
#define VS_GONE 10.0  /* seconds without it in a race on: the host is gone */
#define VS_CLOSED_NOTICE 180 /* frames of the race's notice of it */
/* the time attack course whose map and HUD a join ramp's race uses (the ramp's road) */
static const signed char VS_RAMP_COURSE[MENU_VS_RAMPS] = { 0, 1, 0, 1, 2, 3, 4, 4, 4, 4, 5, 5, 5, 5, 7, 9, 9, 6, 8, 8, 8, 8 };

static void vs_link_send(const unsigned char *p, int len, void *ctx) { (void)ctx; net_race_send(p, len); }
static void vs_link_receive(void *ctx) { (void)ctx; net_race_receive(); }

static void vs_profile(unsigned char *p) {
  int power, handling;
  menu_tune(&G.menu, &power, &handling);
  p[0] = (unsigned char)G.menu.maker, p[1] = (unsigned char)G.menu.car, p[2] = (unsigned char)G.menu.color;
  p[3] = (unsigned char)power, p[4] = (unsigned char)handling;
  for (int i = 0; i < 5; ++i) p[VS_PROFILE_CAR + i] = G.menu.name[i] >= 0 ? (unsigned char)G.menu.name[i] : 255;
  int title = menu_title(&G.menu);
  p[VS_PROFILE_NAME] = (unsigned char)(title & 0xff), p[VS_PROFILE_NAME + 1] = (unsigned char)(title >> 8 & 0xff);
  int mk = G.menu.maker, c = G.menu.car;
  unsigned char *d = p + VS_PROFILE_DRESS;
  d[0] = (unsigned char)G.menu.aero[mk][c], d[1] = (unsigned char)G.menu.mirror[mk][c];
  d[2] = (unsigned char)G.menu.wing[mk][c], d[3] = (unsigned char)G.menu.bonnet[mk][c];
  d[4] = G.menu.wheel[mk][c], d[5] = (unsigned char)G.menu.neon[mk][c], d[6] = G.menu.vinyl[mk][c];
}

/* a player's dress-up from its profile (an older build's, or none sent: stock) */
static vs_dress_t vs_dress(const net_player *pl) {
  vs_dress_t v = { 0 };
  if (pl->profile_len < VS_PROFILE) return v;
  const unsigned char *d = pl->profile + VS_PROFILE_DRESS;
  v.aero = d[0] <= 3 ? d[0] : 0;
  v.mirror = d[1] <= 1 ? d[1] : 0;
  v.wing = d[2] <= 4 ? d[2] : 0;
  v.bonnet = d[3] <= 3 ? d[3] : 0;
  v.wheel = d[4];
  v.neon = d[5] <= 12 ? d[5] : 0;
  v.vinyl = d[6] < 100 ? d[6] : 0;
  return v;
}

/* clEffCarType::isR35: the GT-R R35 (its box shows the tune's steps) */
static int vs_r35(int maker, int car) {
  if (maker < 0 || maker >= CARDB_ALL || car < 0 || car >= CARDB_NCARS[maker]) return 0;
  for (const char *s = CARDB_CARS[maker][car].label; *s; ++s)
    if (s[0] == '[' && s[1] == 'R' && s[2] == '3' && s[3] == '5' && s[4] == ']') return 1;
  return 0;
}

/* a player's car from its profile (NULL: none this app knows) */
static const cardb_color *vs_color(const net_player *pl) {
  const unsigned char *p = pl->profile;
  if (pl->profile_len < VS_PROFILE_CAR || p[0] >= CARDB_ALL || p[1] >= CARDB_NCARS[p[0]]) return NULL;
  const cardb_car *c = &CARDB_CARS[p[0]][p[1]];
  return p[2] < c->ncolors ? &c->colors[p[2]] : NULL;
}

/* the save's name as the versus sends it: its characters' (ASCII) or, a save from before
 * the names, PLAYER n as its card shows (nothing asked: Allan 10-26) */
static void vs_name(char *name, int n) {
  menu_name_ascii(&G.menu, name, n);
  if (!name[0]) snprintf(name, n, "PLAYER %d", G.menu.save_slot >= 1 && G.menu.save_slot <= MENU_SAVES ? G.menu.save_slot : 1);
}

/* the screen's font has ASCII only (a car's name has full-width signs) */
static void vs_ascii(char *out, int n, const char *s) {
  int k = 0;
  for (; s && *s && k < n - 1; ++s) {
    unsigned char c = (unsigned char)*s;
    if (c < 0x80) out[k++] = (char)c;
    else if (c == 0xef && (unsigned char)s[1] == 0xbc && (unsigned char)s[2] == 0x8d) out[k++] = '-', s += 2; /* － */
  }
  out[k] = 0;
}

/* save_dir/vs_server: the server's address as last entered */
static void vs_ip_load(void) {
  char path[1200];
  snprintf(path, sizeof path, "%s/vs_server", G.save_dir);
  FILE *f = fopen(path, "r");
  if (!f) return;
  char ip[sizeof G.menu.vs.ip] = { 0 };
  if (fgets(ip, sizeof ip, f)) {
    int k = 0;
    for (int i = 0; ip[i]; ++i)
      if ((ip[i] >= '0' && ip[i] <= '9') || ip[i] == '.') ip[k++] = ip[i];
    ip[k] = 0;
    memcpy(G.menu.vs.ip, ip, sizeof ip);
  }
  fclose(f);
}

static void vs_ip_save(void) {
  char path[1200];
  snprintf(path, sizeof path, "%s/vs_server", G.save_dir);
  FILE *f = fopen(path, "w");
  if (!f) return;
  fprintf(f, "%s\n", G.menu.vs.ip);
  fclose(f);
}

/* no GameKit here (Android): Bluetooth versus is Apple's only, the LAN plays across */
#ifdef __ANDROID__
#define VS_NO_BT "BLUETOOTH: APPLE ONLY. USE LAN"
#else
#define VS_NO_BT "NO BLUETOOTH HERE"
#endif

/* everything of the versus off: the client, the host, the LAN's search, Bluetooth */
static void vs_off(void) {
  if (net_is_open() || vs_host_is_open()) fprintf(stderr, "versus: off (screen %d, state %d)\n", G.menu.screen, G.menu.vs.state);
  net_close();
  vs_host_close();
  net_scan(0, 0);
  if (G.vs_gk != VS_GK_OFF) gk_stop();
  G.vs_gk = VS_GK_OFF;
  G.menu.vs.hosting = 0;
  G.menu.vs.n_found = 0;
}

/* host a game here: the host on UDP (and Bluetooth's peers), this app's client on the loopback */
static void vs_host_start(const char *name) {
  vs_off();
  int bt = G.menu.vs.kind == MENU_VS_BT;
  if (bt && gk_host(name) != 0) {
    snprintf(G.menu.vs.note, sizeof G.menu.vs.note, VS_NO_BT);
    return;
  }
  if (bt) G.vs_gk = VS_GK_HOST;
  if (vs_host_open(bt, G.vs_id) != 0 || net_open("127.0.0.1", NET_PORT, G.vs_id) != 0) {
    vs_off();
    snprintf(G.menu.vs.note, sizeof G.menu.vs.note, "COULD NOT HOST (PORT %d IN USE?)", NET_PORT);
    return;
  }
  G.menu.vs.hosting = 1;
  G.menu.vs.state = MENU_VS_CONNECTING;
  fprintf(stderr, "versus: hosting a %s game as %08x\n", bt ? "Bluetooth" : "LAN", G.vs_id);
}

/* the games nearby, while nothing is joined: the LAN's answers, or the Bluetooth hosts seen */
static void vs_search(double now, const char *name) {
  menu_state *m = &G.menu;
  if (m->vs.kind == MENU_VS_LAN) {
    net_scan(now, 1);
    net_found f[MENU_VS_FOUND];
    m->vs.n_found = net_found_list(f, MENU_VS_FOUND);
    for (int k = 0; k < m->vs.n_found; ++k) {
      char nm[NET_NAME + 1];
      vs_ascii(nm, sizeof nm, f[k].name);
      snprintf(m->vs.found[k], sizeof m->vs.found[k], "%s (%d/%d)", nm, f[k].n, NET_PLAYERS);
    }
  } else if (m->vs.kind == MENU_VS_BT) {
    if (G.vs_gk == VS_GK_OFF) G.vs_gk = gk_browse(name) == 0 ? VS_GK_BROWSE : -1;
    char f[MENU_VS_FOUND][GK_NAME];
    m->vs.n_found = G.vs_gk == VS_GK_BROWSE ? gk_found(f, MENU_VS_FOUND) : 0;
    for (int k = 0; k < m->vs.n_found; ++k) vs_ascii(m->vs.found[k], sizeof m->vs.found[k], f[k]);
    if (G.vs_gk < 0) snprintf(m->vs.note, sizeof m->vs.note, VS_NO_BT);
  }
}

/* A menu frame's part of the versus: hosting, finding and joining (Bluetooth, LAN) or connecting
 * (a server), the lobby as the host has it, the host's START, and the GO that sends this app to
 * the race's loading (its course's map: the race starts once that is in, menu_frame) */
/* The area vote (MENU_VS_AREA, the arcade's seqJoCourseSelect) over the lobby's statuses
 * (menu_vs_status). Each player's card and vote come from its status; this device opens the
 * vote when the host's does. The host counts it (clNet::getJoinArea): once everyone in it has
 * voted, or VS_AREA_LATE s after its own 20 s (a vote not in: the card it was on, omakase's a
 * random area), the most voted card wins, a tie one of them at random; the result shows for
 * VS_REVEAL s, then the race starts on a random start ramp of the area
 * (clEffAreaType::getRandomRamp, the area table 0x97391a0's ramps). */
#define VS_AREA_LATE 3.0
#define VS_REVEAL 1.6
static const signed char AREA_RAMP[8][2] = { { 0, 4 }, { 4, 2 }, { 6, 4 }, { 10, 4 }, { 17, 1 }, { 14, 1 }, { 15, 2 }, { 18, 4 } };

static void vs_day_open(menu_state *m);

static void vs_area(menu_state *m, const net_lobby *l, double now) {
  int host = l->me == l->host;
  if (m->screen == MENU_VS && m->vs.state == MENU_VS_LOBBY && !m->vs.area_go && !host && l->host < m->vs.n) {
    int st = l->player[l->host].status;
    if (st >= MENU_VS_CHOOSING && st < MENU_VS_FINAL) m->vs.area_go = 1;
  }
  if (m->screen != MENU_VS_AREA) {
    G.vs_area_at = 0;
    return;
  }
  if (!G.vs_area_at) G.vs_area_at = now;
  if (!host) {
    int st = l->host < m->vs.n ? l->player[l->host].status : 0;
    if (m->vs.area_final < 0 && st >= MENU_VS_FINAL && st < MENU_VS_FINAL + MENU_VS_CARDS) m->vs.area_final = st - MENU_VS_FINAL;
    /* the host on to the time zone vote: this one with it */
    if (MENU_VS_IN_DAY(st)) vs_day_open(m);
    return;
  }
  if (m->vs.area_final < 0) {
    int votes[MENU_VS_CARDS] = { 0 }, all = 1, late = m->vs.timer == 0 && now - G.vs_area_at > 20.0 + VS_AREA_LATE;
    for (int i = 0; i < m->vs.n; ++i) {
      int v = i == m->vs.me ? m->vs.voted : m->vs.other_vote[i];
      int card = i == m->vs.me ? (m->vs.voted ? m->vs.vote : m->vs.item) : m->vs.other[i];
      if (v < 0) continue; /* not in the vote (back from a race late, say) */
      if (v == 2) continue; /* omakase: the others' choice */
      if (!v) {
        all = 0;
        if (!late || card <= 0 || card >= MENU_VS_CARDS) continue;
      }
      if (card > 0 && card < MENU_VS_CARDS) votes[card]++;
    }
    if (!all && !late) return;
    int best = 0, n_best = 0;
    for (int k = 1; k < MENU_VS_CARDS; ++k) best = votes[k] > best ? votes[k] : best;
    int tie[MENU_VS_CARDS];
    for (int k = 1; k < MENU_VS_CARDS; ++k)
      if (votes[k] == best && best) tie[n_best++] = k;
    /* every vote omakase: the arcade's random area */
    m->vs.area_final = n_best ? tie[rand() % n_best] : menu_vs_random_card();
    G.vs_final_at = now;
    fprintf(stderr, "versus: area vote: card %d (%d votes, %d tied)\n", m->vs.area_final, best, n_best);
    return;
  }
  /* the result shown: the start ramp (clEffAreaType::getRandomRamp), the time zone vote */
  if (now - G.vs_final_at >= VS_REVEAL) {
    int a = menu_vs_card_area(m->vs.area_final);
    m->vs.ramp = AREA_RAMP[a < 0 ? 0 : a][0] + rand() % AREA_RAMP[a < 0 ? 0 : a][1];
    vs_day_open(m);
  }
}

/* The time zone vote (MENU_BGM's vs.day, seqJoinDayTimeSelect after seqJoStartRampDecide)
 * over the statuses again: the host counts it (clNet::getJoinDayTime) once everyone in it has
 * voted, or VS_AREA_LATE s after its own 20 s (a vote not in: the card it was on); the most
 * voted zone wins, a tie one at random; the result shows VS_REVEAL s, then the race starts with
 * it (the GO's ramp byte's top bit). The map of the area's city loads under it. */
static void vs_day_open(menu_state *m) {
  int a = menu_vs_card_area(m->vs.area_final);
  if (m->vs.me == m->vs.host) m->course = VS_RAMP_COURSE[m->vs.ramp];
  else if (a >= 0) m->course = VS_RAMP_COURSE[AREA_RAMP[a][0]];
  menu_vs_day_open(m);
}

static void vs_day(menu_state *m, const net_lobby *l, double now) {
  int host = l->me == l->host;
  if (m->screen != MENU_BGM || !m->vs.day) {
    G.vs_day_at = 0;
    return;
  }
  if (!G.vs_day_at) G.vs_day_at = now, G.vs_day_started = 0;
  if (!host) {
    int st = l->host < m->vs.n ? l->player[l->host].status : 0;
    if (m->vs.day_final < 0 && st >= MENU_VS_DAY_FINAL && st < MENU_VS_LOADED) m->vs.day_final = st - MENU_VS_DAY_FINAL;
    return;
  }
  if (m->vs.day_final < 0) {
    int votes[2] = { 0 }, all = 1, late = m->vs.day_timer == 0 && now - G.vs_day_at > 20.0 + VS_AREA_LATE;
    for (int i = 0; i < m->vs.n; ++i) {
      int v = i == m->vs.me ? m->vs.day_voted : m->vs.other_day[i];
      int zone = i == m->vs.me ? m->zone : m->vs.other_zone[i];
      if (v < 0) continue;
      if (v == 2) continue; /* omakase: the others' choice */
      if (!v) {
        all = 0;
        if (!late) continue;
      }
      votes[zone & 1]++;
    }
    if (!all && !late) return;
    m->vs.day_final = m->zones < 2 ? 0 : votes[0] == votes[1] ? rand() % 2 : votes[1] > votes[0];
    /* everyone's race has the host's song (Allan 10-27: one BGM for all), sent with the GO */
    G.vs_song = m->song;
    G.vs_day_final_at = now;
    fprintf(stderr, "versus: time zone vote: %s (%d morning, %d night)\n", m->vs.day_final ? "night" : "morning", votes[0], votes[1]);
    return;
  }
  /* START (again each 2 s should it or its GO be lost: the GO moves this screen on) */
  if (now - G.vs_day_final_at >= VS_REVEAL && now - G.vs_day_started >= 2.0) {
    G.vs_day_started = now;
    net_start(m->vs.ramp | m->vs.day_final << 7, m->vs.km, G.vs_song + 2);
  }
}

static void vs_menu(void) {
  menu_state *m = &G.menu;
  double now = now_ms() * 1e-3;
  char name[NET_NAME + 1];
  vs_name(name, sizeof name);
  if (m->screen != MENU_VS && m->screen != MENU_VS_AREA && m->screen != MENU_VS_MATCH && m->screen != MENU_VS_STAR &&
      !(m->screen == MENU_BGM && m->vs.day)) { /* left the screen */
    if (net_is_open() || vs_host_is_open() || G.vs_gk != VS_GK_OFF || m->vs.n_found) vs_off();
    m->vs.state = MENU_VS_OFF, m->vs.n = 0;
    return;
  }
  if (m->vs.connect) {
    int c = m->vs.connect;
    m->vs.connect = 0;
    m->vs.n = 0;
    m->vs.note[0] = 0;
    G.vs_asked = now;
    if (c < 0) {
      vs_off();
      m->vs.state = MENU_VS_OFF;
    } else if (m->vs.kind != MENU_VS_NET) {
      vs_host_start(name);
    } else {
      vs_ip_save();
      if (net_open(m->vs.ip, NET_PORT, G.vs_id) == 0) {
        m->vs.state = MENU_VS_CONNECTING;
        fprintf(stderr, "versus: connecting to %s as %08x\n", m->vs.ip, G.vs_id);
      } else {
        snprintf(m->vs.note, sizeof m->vs.note, "NOT AN ADDRESS (LIKE 192.168.1.5)");
      }
    }
  }
  if (m->vs.join) {
    int k = m->vs.join - 1;
    m->vs.join = 0;
    m->vs.note[0] = 0;
    G.vs_asked = now;
    if (m->vs.kind == MENU_VS_LAN) {
      net_found f[MENU_VS_FOUND];
      int n = net_found_list(f, MENU_VS_FOUND);
      if (k < n && net_open(f[k].ip, NET_PORT, G.vs_id) == 0) {
        net_scan(now, 0);
        m->vs.state = MENU_VS_CONNECTING;
        fprintf(stderr, "versus: joining %s at %s as %08x\n", f[k].name, f[k].ip, G.vs_id);
      }
    } else if (m->vs.kind == MENU_VS_BT && G.vs_gk == VS_GK_BROWSE && gk_join(k) == 0) {
      G.vs_gk = VS_GK_JOIN;
      m->vs.state = MENU_VS_CONNECTING;
    }
  }
  /* Bluetooth's join: the client once the session has its host, back to the search if not */
  if (G.vs_gk == VS_GK_JOIN && !net_is_open()) {
    int j = gk_joined();
    if (j > 0) {
      net_open_gk(G.vs_id);
      G.vs_asked = now;
    } else if (j < 0 || now - G.vs_asked > 12.0) {
      gk_stop();
      G.vs_gk = VS_GK_OFF;
      m->vs.state = MENU_VS_OFF;
      snprintf(m->vs.note, sizeof m->vs.note, "COULD NOT JOIN");
    }
  }
  if (G.vs_gk == VS_GK_JOIN && net_is_open() && gk_joined() < 0) {
    vs_off();
    m->vs.state = MENU_VS_OFF;
    snprintf(m->vs.note, sizeof m->vs.note, "THE HOST WENT AWAY");
  }
  if (vs_host_is_open()) vs_host_poll(now, name);
  if (m->vs.state == MENU_VS_OFF) vs_search(now, name);
  if (!net_is_open()) {
    if (m->vs.state != MENU_VS_OFF && G.vs_gk != VS_GK_JOIN) m->vs.state = MENU_VS_OFF, m->vs.n = 0;
    return;
  }
  unsigned char profile[VS_PROFILE];
  vs_profile(profile);
  net_poll(now, menu_vs_status(m), name, profile, VS_PROFILE);
  if (net_host_closed()) {
    vs_off();
    m->vs.state = MENU_VS_OFF, m->vs.n = 0;
    snprintf(m->vs.note, sizeof m->vs.note, "THE HOST CLOSED THE GAME");
    return;
  }
  const net_lobby *l = net_lobby_now();
  if (l->connected) {
    if (m->vs.state == MENU_VS_CONNECTING) m->vs.state = MENU_VS_LOBBY;
    m->vs.n = l->n < MENU_VS_PLAYERS ? l->n : MENU_VS_PLAYERS;
    m->vs.me = l->me;
    m->vs.host = l->host;
    for (int i = 0; i < m->vs.n; ++i) {
      const net_player *pl = &l->player[i];
      vs_ascii(m->vs.name[i], sizeof m->vs.name[i], pl->name);
      for (int j = 0, end = pl->profile_len < VS_PROFILE_NAME; j < 5; ++j) {
        end |= pl->profile[VS_PROFILE_CAR + j] == 255;
        m->vs.glyph[i][j] = (short)(end ? -1 : pl->profile[VS_PROFILE_CAR + j]);
      }
      const cardb_color *col = vs_color(pl);
      const cardb_car *car = col ? &CARDB_CARS[pl->profile[0]][pl->profile[1]] : NULL;
      vs_ascii(m->vs.car[i], sizeof m->vs.car[i], car ? car->label : "?");
      m->vs.racing[i] = pl->status == 1 || pl->status == MENU_VS_LOADED;
      /* the area vote's (menu_vs_status) */
      int st = pl->status;
      m->vs.other_vote[i] = st >= MENU_VS_CHOOSING && st < MENU_VS_VOTED ? 0 : st >= MENU_VS_VOTED && st < MENU_VS_OMAKASE ? 1
                            : st >= MENU_VS_OMAKASE && st < MENU_VS_FINAL ? 2 : st >= MENU_VS_FINAL && st < MENU_VS_DAY ? 1 : -1;
      m->vs.other[i] = st >= MENU_VS_FINAL ? st - MENU_VS_FINAL : st >= MENU_VS_OMAKASE ? st - MENU_VS_OMAKASE
                       : st >= MENU_VS_VOTED ? st - MENU_VS_VOTED : st - MENU_VS_CHOOSING;
      /* and the time zone vote's */
      m->vs.other_day[i] = st >= MENU_VS_DAY && st < MENU_VS_DAY_VOTED ? 0 : st >= MENU_VS_DAY_VOTED && st < MENU_VS_LOADED ? 1
                           : st >= MENU_VS_DAY_OMAKASE && st < MENU_VS_DAY_OMAKASE + 2 ? 2 : -1;
      m->vs.other_zone[i] = (st - MENU_VS_DAY) & 1;
    }
    vs_area(m, l, now);
    vs_day(m, l, now);
  }
  double silent = l->connected ? net_silence(now) : now - G.vs_asked;
  /* a host gone without a word (its app closed, its device asleep): back to the search, where
   * it shows again once hosting again (Allan 10-27: a way back in) */
  if (l->connected && !vs_host_is_open() && m->vs.state == MENU_VS_LOBBY && silent > VS_GONE) {
    fprintf(stderr, "versus: the host went away\n");
    vs_off();
    m->vs.state = MENU_VS_OFF, m->vs.n = 0;
    snprintf(m->vs.note, sizeof m->vs.note, "THE HOST WENT AWAY");
    return;
  }
  const char *quiet = m->vs.kind == MENU_VS_NET ? "SERVER NOT ANSWERING" : "HOST NOT ANSWERING";
  if (m->vs.state == MENU_VS_CONNECTING)
    snprintf(m->vs.note, sizeof m->vs.note, "%s", silent > VS_SILENT ? quiet : "CONNECTING...");
  else if (silent > VS_SILENT)
    snprintf(m->vs.note, sizeof m->vs.note, "%s", quiet);
  else
    m->vs.note[0] = 0;
  if (m->vs.start) {
    /* the host's START opens the area vote (the others follow its status) */
    m->vs.start = 0;
    if (m->vs.state == MENU_VS_LOBBY && l->me == l->host && m->screen == MENU_VS) m->vs.area_go = 1;
  }
  net_go go;
  if (m->vs.state == MENU_VS_LOBBY && net_take_go(&go)) {
    /* the time zone vote's result rides the ramp's top bit */
    int zone = go.ramp >> 7 & 1;
    go.ramp &= 0x7f;
    m->zone = m->zones > 1 ? zone : 0;
    /* and the host's song (a host from before it: each its own) */
    if (go.song) menu_vs_song(m, go.song - 2);
    G.vs_go = go;
    G.vs_wait = G.vs_seen_loaded = 0;
    menu_vs_loading(m);
    /* the cars flying in once it is loaded (MENU_VS_MATCH): the machines' cars and plates */
    m->vs.match_n = go.n < MENU_VS_PLAYERS ? go.n : MENU_VS_PLAYERS;
    m->vs.match_me = go.pcb;
    for (int k = 0; k < m->vs.match_n; ++k) {
      const net_player *pl = &go.player[k];
      const unsigned char *p = pl->profile;
      int car = vs_color(pl) != NULL;
      m->vs.match[k].maker = car ? p[0] : -1;
      m->vs.match[k].car = car ? p[1] : -1;
      m->vs.match[k].color = car ? p[2] : -1;
      m->vs.match[k].power = car && p[3] <= CAR_TUNE_MAX ? p[3] : 0;
      m->vs.match[k].handling = car && p[4] <= CAR_TUNE_MAX ? p[4] : 0;
      for (int j = 0, end = pl->profile_len < VS_PROFILE_NAME; j < 5; ++j) {
        end |= p[VS_PROFILE_CAR + j] == 255;
        m->vs.match[k].glyph[j] = (short)(end ? -1 : p[VS_PROFILE_CAR + j]);
      }
      /* no card name sent (PLAYER n): its ASCII name in the glyphs (the plate had none) */
      if (m->vs.match[k].glyph[0] < 0) {
        char nm[NET_NAME + 1];
        vs_ascii(nm, sizeof nm, pl->name);
        menu_name_glyphs(m->vs.match[k].glyph, nm);
      }
      /* (an older build's: none, the box shows the first) */
      vs_dress_t d = vs_dress(pl);
      m->vs.match[k].aero = d.aero, m->vs.match[k].mirror = d.mirror, m->vs.match[k].wing = d.wing;
      m->vs.match[k].bonnet = d.bonnet, m->vs.match[k].wheel = d.wheel, m->vs.match[k].neon = d.neon;
      m->vs.match[k].vinyl = d.vinyl;
      m->vs.match[k].title = pl->profile_len >= VS_PROFILE_DRESS ? p[VS_PROFILE_NAME] | p[VS_PROFILE_NAME + 1] << 8 : 0;
    }
    m->vs.ramp = go.ramp < MENU_VS_RAMPS ? go.ramp : 0;
    m->vs.km = go.km;
    m->course = VS_RAMP_COURSE[m->vs.ramp];
    G.pre_wait = 0;
    fprintf(stderr, "versus: race %u, ramp %d (%s), %s, %d km, %d machines, this one PCB %d\n", go.race, go.ramp,
            menu_vs_ramp_name(m->vs.ramp), m->zone ? "night" : "morning", go.km, go.n, go.pcb);
  }
}

/* A phone's wide menu (app_wide): the 1024x768 layout's 4:3 middle, its outermost columns
 * drawn out over the sides, darkening toward the screen's edges, for the black bars (and the
 * sprites' bits past the arcade's 640). Android's window is multisampled and so can't be
 * copied from: the two columns go by GLES3's blit, to the same place in a texture the screen's
 * size; the iPhone's (GLES2, its own framebuffer) copies them into a texture two wide. */
#ifdef __ANDROID__
#include <EGL/egl.h>
#endif
typedef void (*blit_fn)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);
static struct {
  blit_fn blit;
  GLuint prog, tex, fb;
  GLint u_tex, u_dy;
  int w, h, fmt, off;
} SIDES;

#ifdef __ANDROID__
/* the texture (and its framebuffer) for the blit, in sized format fmt (0: RGB8, 1: RGBA8) */
static void sides_target(int fmt) {
  if (SIDES.fb) glDeleteFramebuffers(1, &SIDES.fb);
  if (SIDES.tex) glDeleteTextures(1, &SIDES.tex);
  glGenTextures(1, &SIDES.tex);
  glBindTexture(GL_TEXTURE_2D, SIDES.tex);
  glTexImage2D(GL_TEXTURE_2D, 0, fmt ? 0x8058 /* RGBA8 */ : 0x8051 /* RGB8 */, G.fb_w, G.fb_h, 0, fmt ? GL_RGBA : GL_RGB,
               GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glGenFramebuffers(1, &SIDES.fb);
  SIDES.w = G.fb_w, SIDES.h = G.fb_h, SIDES.fmt = fmt;
}

/* the two columns into the texture; 0 if the blit was refused */
static int sides_blit(GLint screen, int xl, int xr) {
  while (glGetError() != GL_NO_ERROR) {}
  glBindFramebuffer(GL_FRAMEBUFFER, SIDES.fb);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, SIDES.tex, 0);
  glBindFramebuffer(0x8CA8 /* READ_FRAMEBUFFER */, (GLuint)screen);
#ifdef WMMT_VK
  /* vkgl reads the screen upside down (Vulkan's rows from the top) and so asks Adreno 540's
   * Vulkan for a flipped copy, which gave the column's first row alone: the source flipped
   * here instead, vkgl's copy straight (the texture's rows then from the top: wide_sides'
   * v flipped) */
  SIDES.blit(xl, G.fb_h, xl + 1, 0, xl, 0, xl + 1, G.fb_h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
  SIDES.blit(xr, G.fb_h, xr + 1, 0, xr, 0, xr + 1, G.fb_h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
#else
  SIDES.blit(xl, 0, xl + 1, G.fb_h, xl, 0, xl + 1, G.fb_h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
  SIDES.blit(xr, 0, xr + 1, G.fb_h, xr, 0, xr + 1, G.fb_h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
#endif
  glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)screen);
  return glGetError() == GL_NO_ERROR;
}
#else
/* the texture: the two columns side by side */
static void sides_target(int fmt) {
  (void)fmt;
  if (!SIDES.tex) glGenTextures(1, &SIDES.tex);
  glBindTexture(GL_TEXTURE_2D, SIDES.tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, G.fb_h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  SIDES.w = G.fb_w, SIDES.h = G.fb_h;
}

/* the two columns into the texture's 0 and 1; 0 if the copy was refused */
static int sides_blit(GLint screen, int xl, int xr) {
  (void)screen;
  while (glGetError() != GL_NO_ERROR) {}
  glBindTexture(GL_TEXTURE_2D, SIDES.tex);
  glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, xl, 0, 1, G.fb_h);
  glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 1, 0, xr, 0, 1, G.fb_h);
  return glGetError() == GL_NO_ERROR;
}
#endif

static void wide_sides(void) {
  int x0 = (int)(G.edge * (float)G.fb_h / 768.0f + 0.5f); /* the layout's x 0 on the screen */
  if (SIDES.off || x0 < 2 || 2 * x0 + 4 > G.fb_w) return;
  if (!SIDES.prog) {
    static const char *vs =
      "attribute vec2 aPos; attribute vec2 aUV; attribute float aShade; varying vec2 vUV; varying float vShade;\n"
      "void main() { vUV = aUV; vShade = aShade; gl_Position = vec4(aPos, 0.0, 1.0); }\n";
    /* a little soft up and down, so a line of text across the column doesn't streak */
    static const char *fs =
      "precision highp float; uniform sampler2D uTex; uniform float uDy; varying vec2 vUV; varying float vShade;\n"
      "void main() {\n"
      "  vec2 d = vec2(0.0, uDy);\n"
      "  vec3 c = texture2D(uTex, vUV).rgb * 0.4 + (texture2D(uTex, vUV + d).rgb + texture2D(uTex, vUV - d).rgb) * 0.2\n"
      "         + (texture2D(uTex, vUV + 2.0 * d).rgb + texture2D(uTex, vUV - 2.0 * d).rgb) * 0.1;\n"
      "  gl_FragColor = vec4(c * vShade, 1.0);\n"
      "}\n";
    const char *attribs[] = { "aPos", "aUV", "aShade" };
#ifdef __ANDROID__
    SIDES.blit = (blit_fn)eglGetProcAddress("glBlitFramebuffer");
    SIDES.prog = SIDES.blit ? gl_compile_program(vs, fs, attribs, 3) : 0;
#else
    SIDES.prog = gl_compile_program(vs, fs, attribs, 3);
#endif
    if (!SIDES.prog) {
      fprintf(stderr, "wide menu sides: no blit or program, black bars\n");
      SIDES.off = 1;
      return;
    }
    SIDES.u_tex = glGetUniformLocation(SIDES.prog, "uTex");
    SIDES.u_dy = glGetUniformLocation(SIDES.prog, "uDy");
  }
  GLint screen = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &screen);
  int xl = x0 + 1, xr = G.fb_w - x0 - 2; /* a pixel in from the 4:3's edges */
  if (SIDES.w != G.fb_w || SIDES.h != G.fb_h) sides_target(SIDES.fmt);
  if (!sides_blit(screen, xl, xr)) {
    /* the window's format: RGB8 asked, an RGBX surface may count as RGBA8 */
    sides_target(!SIDES.fmt);
    if (!sides_blit(screen, xl, xr)) {
      fprintf(stderr, "wide menu sides: blit refused, black bars\n");
      SIDES.off = 1;
      return;
    }
    fprintf(stderr, "wide menu sides: %s\n", SIDES.fmt ? "RGBA8" : "RGB8");
  }
  glViewport(0, 0, G.fb_w, G.fb_h);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_BLEND);
  glUseProgram(SIDES.prog);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, SIDES.tex);
  glUniform1i(SIDES.u_tex, 0);
  glUniform1f(SIDES.u_dy, 1.5f / (float)G.fb_h);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
  const float out = 0.55f; /* the shade at the screen's edge (1 at the 4:3's) */
#ifdef __ANDROID__
  float e = 2.0f * (float)x0 / (float)G.fb_w, ul = ((float)xl + 0.5f) / (float)G.fb_w,
        ur = ((float)xr + 0.5f) / (float)G.fb_w;
#else
  float e = 2.0f * (float)x0 / (float)G.fb_w, ul = 0.25f, ur = 0.75f;
#endif
#ifdef WMMT_VK
  const float v0 = 1, v1 = 0; /* (the copy's rows from the top: sides_blit) */
#else
  const float v0 = 0, v1 = 1;
#endif
  const float v[2][20] = {
    { -1, -1, ul, v0, out, -1 + e, -1, ul, v0, 1, -1, 1, ul, v1, out, -1 + e, 1, ul, v1, 1 },
    { 1 - e, -1, ur, v0, 1, 1, -1, ur, v0, out, 1 - e, 1, ur, v1, 1, 1, 1, ur, v1, out },
  };
  for (int s = 0; s < 2; ++s) {
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 20, v[s]);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 20, v[s] + 2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 20, v[s] + 4);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }
  glDisableVertexAttribArray(2);
}

/* Whether the race's other machines have loaded it too: each says so in its lobby status
 * (MENU_VS_LOADED, then 1 again in the race), a machine not in the lobby (gone) not waited for */
#define VS_WAIT_MAX (60 * 60)
#define VS_LOADING_MIN 30 /* frames of the loading page at least, after the cars' flying in */
static int vs_others_loaded(void) {
  const net_lobby *l = net_lobby_now();
  if (!net_is_open() || !l->connected) return 1;
  int all = 1;
  for (int k = 0; k < G.vs_go.n && k < NET_PLAYERS; ++k) {
    if (k == G.vs_go.pcb || (G.vs_seen_loaded >> k & 1)) continue;
    for (int i = 0; i < l->n; ++i) {
      if (l->player[i].id != G.vs_go.player[k].id) continue;
      if (l->player[i].status == MENU_VS_LOADED) G.vs_seen_loaded |= 1 << k;
      else all = 0;
    }
  }
  return all;
}

/* the race's notice that the host closed the game */
static void vs_closed_draw(void) {
  static const char S[] = "THE HOST CLOSED THE GAME";
  float w = G.hud.screen_w, h = G.hud.screen_h, px = 4.0f, tw = hud_ui_text_width(S, px);
  float a = G.vs_closed < 10 ? (float)G.vs_closed / 10.0f : 1.0f;
  hud_ui_begin(w, h);
  hud_ui_rect(0, 0.5f * h - 60.0f, w, 120.0f, 0, 0, 0, 0.7f * a);
  hud_ui_text(S, 0.5f * (w - tw), 0.5f * h - 3.5f * px, px, 1.0f, 0.55f, 0.1f, a);
  hud_ui_end();
}

static void menu_frame(float dt, double frame_start) {
  glViewport(0, 0, G.fb_w, G.fb_h);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  G.vs_race = 0;
  G.vs_closed = 0;
  vs_menu();
  /* a versus on its way: the race's course loads under the versus screen's NOW LOADING */
  int vs_load = (G.menu.screen == MENU_VS || G.menu.screen == MENU_VS_MATCH) && G.menu.vs.state == MENU_VS_LOADING;
  /* the time zone select's card, once it stays at rest a moment (or is chosen), picks the map
   * to load; the other's goes a step a frame */
  int rest = G.menu.screen != MENU_BGM || G.menu.deciding || G.menu.card_t >= ZONE_REST;
  /* and the city: the chosen course's (car_course_city), a story race's its series' start
   * (STORYCITY, once its title page names the rival) */
  int city = G.track_city;
  if (G.menu.screen == MENU_VS || G.menu.screen == MENU_VS_MATCH) {
    if (vs_load) city = car_course_city(G.menu.course);
  } else if (G.menu.screen == MENU_BGM && G.menu.vs.day) { /* the area's city, once known */
    if (G.menu.vs.area_final >= 0 || G.menu.vs.me == G.menu.vs.host) city = car_course_city(G.menu.course);
  } else if (G.menu.game == MENU_GAME_STORY) {
    int r = story_rival();
    if (G.menu.screen == MENU_PRE && G.menu.ending == 2) city = STORYCITY[ENDING_SERIES];
    else if (G.menu.screen == MENU_PRE && r >= 0 && RIVALDB[r].series >= 0 && RIVALDB[r].series < STORYCITY_N)
      city = STORYCITY[RIVALDB[r].series];
  } else if (G.menu.game == MENU_GAME_TEN) {
    if (G.menu.screen == MENU_PRE) city = TEN_CITY[menu_ten_course(&G.menu)];
  } else if (G.menu.screen == MENU_BGM || G.menu.screen == MENU_PRE) /* decided, not while browsing */
    city = car_course_city(G.menu.course);
  if ((G.menu.zone != G.track_zone || city != G.track_city || G.hd_reload) && !G.menu.slide && rest &&
      track_open(city, G.menu.zone) != 0) {
    fprintf(stderr, "no map of city %d zone %d, back to Tokyo's morning\n", city, G.menu.zone);
    G.menu.zone = 0;
    G.menu.zones = 1;
    track_open(CAR_TOKYO, 0);
  }
  if (G.track_old.n_batch || G.track_old.n_tex) wpk_free_step(&G.track_old, TRACK_FREE_STEP);
  /* the race waits on its title page till all it draws is in GL and first-drawn (a cold part
   * cost the race a frame and more on the iPad): the course, its sky and arrow boards, the
   * traffic (warm_step), the chosen car with its paint, and the HUD with the car's meters
   * (hud_warm: the race's first frame took some 100 ms first-drawing them) */
  G.rival = story_rival();
  G.menu.story_sel = G.rival >= 0 && G.menu.game == MENU_GAME_STORY ? RIVALDB[G.rival].select : -1;
  const cardb_color *chosen = chosen_color();
  int start_in = G.seen.track == 2 || (G.seen.track == 1 && G.seen.start == G.menu.course + 1);
  if (vs_load) start_in = G.seen.track == 2 && city == G.track_city; /* the race starts off the course's start */
  int pre = G.menu.screen == MENU_PRE || vs_load;
  int hud_in = 1;
  if (pre) {
    hud_set_meter(menu_meter(&G.menu));
    hud_set_car(chosen->pack, G.track_zone == 1);
    hud_in = hud_warm(G.hud.screen_w, G.hud.screen_h, HUD_WARM_STEP) == 0;
  }
  int all_in = start_in && hud_in && car_pack(chosen->pack) && paint_ready(chosen->pack, chosen->paint);
  int rows[RACE_ROWS], n_rival = rival_rows(G.rival, rows);
  for (int i = 0; i < n_rival; ++i) {
    int paint, rival = rival_look(rows[i], &paint);
    all_in = all_in && (rival < 0 || (car_pack(rival) && paint_ready(rival, paint)));
  }
  /* a versus': the machines' cars, for their flying in (MENU_VS_MATCH) */
  int match_in = 1;
  for (int k = 0; vs_load && k < G.menu.vs.match_n; ++k) {
    const cardb_color *oc = vs_color(&G.vs_go.player[k]);
    match_in = match_in && (!oc || (car_pack(oc->pack) && paint_ready(oc->pack, oc->paint)));
  }
  all_in = all_in && match_in;
  G.pre_wait = pre && !all_in ? G.pre_wait + 1 : 0;
  if (G.menu.screen == MENU_PRE) G.pre_frames++;
  if (G.pre_wait == PRE_WAIT_MAX) fprintf(stderr, "race title: loading still not done (track %d), starting\n", G.seen.track);
  G.menu.ready = all_in || G.pre_wait >= PRE_WAIT_MAX;
  int go = menu_update(&G.menu, dt);
  int vs = G.menu.screen == MENU_VS && G.menu.vs.state == MENU_VS_LOADING;
  /* the cars fly in first (MENU_VS_MATCH), once theirs are in, the course loading on under it
   * (Allan 10-27); then the loading page */
  int match_first = vs && G.menu.vs.match_n >= 2 && !G.menu.vs.match_played;
  if (match_first && (match_in || G.menu.ready)) menu_vs_match(&G.menu);
  if (menu_vs_match_done(&G.menu)) menu_vs_match_back(&G.menu);
  if (match_first) vs = 0;
  /* loaded: it says so (menu_vs_status) and waits for the race's other machines' (one gone from
   * the lobby does not hold it, nor longer than VS_WAIT_MAX); then the race (the loading page
   * shown a moment at least) */
  if (vs && G.menu.ready && !G.menu.vs.loaded) {
    G.menu.vs.loaded = 1;
    fprintf(stderr, "versus: race loaded (%d frames)\n", G.menu.t);
  }
  int vs_waiting = vs && G.menu.vs.loaded && !vs_others_loaded();
  if (G.menu.vs.wait_others && !vs_waiting) fprintf(stderr, "versus: the others loaded after %.1f s\n", (float)G.vs_wait / 60.0f);
  G.vs_wait = vs_waiting ? G.vs_wait + 1 : 0;
  if (G.vs_wait == VS_WAIT_MAX) fprintf(stderr, "versus: the others still loading after %d s, starting\n", VS_WAIT_MAX / 60);
  G.menu.vs.wait_others = vs_waiting && G.vs_wait < VS_WAIT_MAX;
  int vs_ready = vs && G.menu.ready && !G.menu.vs.wait_others && G.menu.t >= VS_LOADING_MIN;
  if (vs_ready) go = 1;
  if (G.menu.save_erase) {
    save_erase(G.menu.save_erase);
    G.menu.save_erase = 0;
    G.menu.save_scan = 1;
  }
  if (G.menu.save_scan) {
    save_scan();
    G.menu.save_scan = 0;
  }
  if (G.menu.save_pick) save_take();
  if (G.menu.garage_changed) {
    hq_apply();
    options_save();
    garage_save();
    G.menu.garage_changed = 0;
  }
  if (go) {
    const cardb_color *col = chosen_color();
    int power, handling;
    /* a story race runs on the story's tune (the card's), the time attack on the setting's */
    if (G.menu.game == MENU_GAME_STORY && !vs) menu_story_tune(&G.menu, &power, &handling);
    else menu_tune(&G.menu, &power, &handling);
    car_spec spec = { (unsigned)col->type, col->gears, { 0 }, (int)CARDB_TYRES[col->pack][6], power, handling };
    for (int i = 0; i < 6; ++i) spec.tyres[i] = CARDB_TYRES[col->pack][i];
    /* a race chosen before the course finished loading waits for the rest */
    load_pause();
    while (G.track_old.n_batch || G.track_old.n_tex) wpk_free_step(&G.track_old, 1u << 30);
    if (!G.track_ready) track_set(wpk_upload(&G.track, (size_t)-1));
    pack_load_now(col->pack, col->paint != 0);
    car_rival rv[CAR_RIVALS];
    G.n_rival = 0;
    memset(G.rival_dress, 0, sizeof G.rival_dress);
    G.n_ten = 0;
    int ten = G.rival == TEN_FIRST, course = G.menu.course;
    car_set_ten(0, 0, NULL, 0);
    if (ten) {
      /* the 10-in-a-row: all the level's rivals' models now (one on the road, a new one each
       * round: CSeq::Exec), their tyres for its CRivalSetting::GetSettingPt */
      car_rival tv[CAR_TEN_RIVALS];
      for (int i = 0; i < n_rival && i < CAR_TEN_RIVALS; ++i) {
        memset(&tv[i], 0, sizeof tv[i]);
        G.ten_pack[i] = rival_look(rows[i], &G.ten_paint[i]);
        if (G.ten_pack[i] >= 0) pack_load_now(G.ten_pack[i], G.ten_paint[i] != 0);
        int base_paint, base = rival_base(rows[i], &base_paint);
        tv[i].type = (unsigned)rival_type(rows[i]);
        tv[i].character = rows[i] - TEN_ROW(0);
        tv[i].level = G.menu.ten_level;
        for (int j = 0; base >= 0 && j < 6; ++j) tv[i].tyres[j] = CARDB_TYRES[base][j];
        tv[i].spokes = base >= 0 ? (int)CARDB_TYRES[base][6] : 5;
        G.n_ten++;
      }
      car_set_ten(menu_ten_stage(&G.menu), G.menu.ten_level, tv, G.n_ten);
      course = ten_ta_course();
      if (G.n_ten && G.ten_pack[0] >= 0) {
        G.rival_pack[0] = G.ten_pack[0];
        G.rival_paint[0] = G.ten_paint[0];
        G.n_rival = 1;
      }
      n_rival = 0;
    }
    if (vs) {
      /* the versus: the other machines' cars (their profiles), rivals 0.. in PCB order */
      car_link_car others[CAR_LINK_MACHINES - 1];
      for (int k = 0; k < G.vs_go.n && G.n_rival < CAR_LINK_MACHINES - 1 && G.n_rival < CAR_RIVALS; ++k) {
        if (k == G.vs_go.pcb) continue;
        const net_player *pl = &G.vs_go.player[k];
        const cardb_color *oc = vs_color(pl);
        if (!oc) oc = col;
        car_link_car *o = &others[G.n_rival];
        memset(o, 0, sizeof *o);
        o->pcb = k;
        o->type = (unsigned)oc->type;
        for (int j = 0; j < 6; ++j) o->tyres[j] = CARDB_TYRES[oc->pack][j];
        o->spokes = (int)CARDB_TYRES[oc->pack][6];
        o->power = pl->profile_len >= VS_PROFILE_CAR && pl->profile[3] <= CAR_TUNE_MAX ? pl->profile[3] : 0;
        o->handling = pl->profile_len >= VS_PROFILE_CAR && pl->profile[4] <= CAR_TUNE_MAX ? pl->profile[4] : 0;
        G.rival_pack[G.n_rival] = oc->pack;
        G.rival_paint[G.n_rival] = oc->paint;
        pack_load_now(oc->pack, oc->paint != 0);
        /* dressed up as its player's (the profile's; its aero and wheels in now) */
        G.rival_dress[G.n_rival] = vs_dress(pl);
        menu_aero_want(oc->pack, G.rival_dress[G.n_rival].aero);
        if (G.rival_dress[G.n_rival].wheel) wheel_model(G.rival_dress[G.n_rival].wheel);
        G.n_rival++;
      }
      car_link_io io = { vs_link_send, vs_link_receive, NULL };
      car_set_versus(G.vs_go.pcb, others, G.n_rival, G.vs_go.ramp, (float)G.vs_go.km, G.vs_go.seed, &io);
      n_rival = 0;
    }
    for (int i = 0; i < n_rival; ++i) {
      int k = G.n_rival, pack = rival_look(rows[i], &G.rival_paint[k]);
      if (pack < 0) continue;
      G.rival_pack[k] = pack;
      pack_load_now(pack, G.rival_paint[k] != 0);
      const rivaldb_rival *r = &RIVALDB[rows[i]];
      memset(&rv[k], 0, sizeof rv[k]);
      rv[k].type = (unsigned)r->type;
      rv[k].character = r->character;
      rv[k].level = menu_story_loop(&G.menu);
      rv[k].losses = menu_story_losses(&G.menu);
      rv[k].series = r->series;
      /* the tyres of the car select's model of its type (a rival's own car is that body, dressed) */
      int base_paint, base = rival_base(rows[i], &base_paint);
      for (int j = 0; j < 6; ++j) rv[k].tyres[j] = CARDB_TYRES[base][j];
      rv[k].spokes = (int)CARDB_TYRES[base][6];
      if (G.menu.ending == 2) rv[k].series = ENDING_SERIES, rv[k].character = ENDING_CHARA[k & 1];
      G.n_rival++;
    }
    load_sync(NULL);
    car_set_rivals(rv, ten || vs ? 0 : G.n_rival);
    if (car_start(&G.car, course, G.menu.manual, &spec) != 0) {
      fprintf(stderr, "course %d failed to start\n", G.menu.course);
      if (vs) menu_vs_loading(&G.menu), G.menu.vs.state = MENU_VS_LOBBY;
      else menu_open(&G.menu);
    } else {
      spark_reset();
      G.vs_race = vs ? G.n_rival : 0;
      if (vs) G.menu.vs.state = MENU_VS_RACE;
      fprintf(stderr, "race: course %d (%s), %s colour %d, power %d handling %d, %s\n", G.menu.course,
              G.menu.official ? "official machine" : "personal best", CARDB_CARS[G.menu.maker][G.menu.car].label,
              G.menu.color, power, handling, G.menu.manual ? "MT" : "AT");
      G.in_menu = 0;
      load_pause(); /* the race loads nothing more (its own first draws aside) */
      /* the car models stay for the car select after the race (unless iOS warned), only the
       * other cars' paints go */
      if (G.low_memory) car_packs_trim(0, NULL);
      else car_paints_trim(NULL, 1);
      /* the map's fog, and its time zone for the next start */
      int z = G.track_zone;
      G.rp.fog_far = FOG[G.track_city][z].far;
      G.rp.fog_near = FOG[G.track_city][z].near;
      memcpy(G.rp.fog_color, FOG[G.track_city][z].color, sizeof G.rp.fog_color);
      memcpy(G.rp.grade, MAP_GRADE[G.track_city][z], sizeof G.rp.grade);
      G.rp.wet = G.console && z && G.menu.wet_on ? 0.75f : 0.0f; /* (a night's road shines with its lights) */
      char path[1200];
      snprintf(path, sizeof path, "%s/timezone", G.save_dir);
      FILE *tz = fopen(path, "w");
      if (tz) {
        fputc('0' + z, tz);
        fclose(tz);
      }
      G.race_model = car_pack(col->pack);
      wpk_paint(G.race_model, col->paint);
      int set = G.menu.aero[G.menu.maker][G.menu.car];
#ifdef WMMT_EGL
      if (getenv("WMMT_AERO")) set = atoi(getenv("WMMT_AERO")); /* test: this aero set */
#endif
      aero_model(col->pack, set);
      wheel_model(G.menu.wheel[G.menu.maker][G.menu.car]);
      for (int i = 0; i < CAR_RIVALS; ++i) {
        G.rival_model[i] = i < G.n_rival ? car_pack(G.rival_pack[i]) : NULL;
        if (G.rival_model[i] && !ten && !vs)
          fprintf(stderr, "rival %d: %s (%s), select %d race %d\n", i + 1, RIVALDB[rows[i]].name,
                  pack_name(G.rival_pack[i]), RIVALDB[rows[i]].select, RIVALDB[rows[i]].race);
      }
      /* the car's own engine sound, meters (morning or night) and gear pattern */
      snd_set_car((unsigned)col->type, power);
      hud_set_tune(car_tune_hp(CARDB_CARS[G.menu.maker][G.menu.car].hp, power), car_tune_grade(power, handling));
      hud_set_class(G.menu.class_car[G.menu.maker][G.menu.car]);
      hud_set_card(menu_title(&G.menu), G.menu.name);
      hud_set_meter(menu_meter(&G.menu));
      hud_set_car(col->pack, z == 1);
      hud_set_course(course);
      hud_set_story(G.menu.story_sel >= 0 || vs); /* a versus: the story's console (no records) */
      {
        int chara[CAR_RIVALS] = { 0 };
        unsigned char shown[CAR_RIVALS] = { 0 };
        int n = 0;
        for (int i = 0; !ten && G.menu.story_sel >= 0 && i < G.n_rival && i < CAR_RIVALS; ++i) {
          int slot = RIVALDB[rows[i]].slot;
          if (slot < 0 || slot >= CAR_RIVALS) continue;
          chara[slot] = RIVALDB[rows[i]].character;
          shown[slot] = STORYINFO_SHOWN[rows[i]];
          if (slot + 1 > n) n = slot + 1;
        }
        hud_set_story_rivals(chara, shown, n, G.menu.story_sel);
      }
      hud_set_ten(ten ? G.menu.ten_level : -1);
      {
        /* a versus: the other machines' boxes (clJoinConsole), in PCB order as car_set_versus has
         * them (a car this app does not know: its own, as the race has it); the tune's steps
         * with an R35 in the race (its or this one) */
        hud_vs_box box[CAR_RIVALS];
        int n = 0, r35 = vs_r35(G.menu.maker, G.menu.car);
        for (int k = 0; vs && k < G.menu.vs.match_n && n < CAR_RIVALS; ++k) {
          if (k == G.menu.vs.match_me) continue;
          const typeof(G.menu.vs.match[0]) *o = &G.menu.vs.match[k];
          int known = o->maker >= 0, mk = known ? o->maker : G.menu.maker, c = known ? o->car : G.menu.car;
          int power = known ? o->power : 0, handling = known ? o->handling : 0;
          hud_vs_box *b = &box[n++];
          for (int j = 0; j < 5; ++j) b->name[j] = o->glyph[j];
          b->title = o->title;
          b->plate = CARDB_CARS[mk][c].plate;
          b->hp = car_tune_hp(CARDB_CARS[mk][c].hp, power);
          b->grade = car_tune_grade(power, handling);
          b->power = power, b->handling = handling;
          b->steps = r35 || vs_r35(mk, c);
          b->seat = k;
        }
        hud_set_vs(box, n);
      }
      records_show(course);
      G.race_pack = col->pack;
      if (G.story_race == 0) { /* (a select's second race: the card as the first started) */
        title_card_now(&G.title_pre);
        G.title_odo0 = title_odo();
      }
      G.top_kmh = 0;
      G.time_shown = (int)G.car.race.time_left;
      G.ten_rival_on = -1;
      G.ten_freeze = G.ten_beaten_on = G.ten_bgm_off = 0;
      G.ten_white = G.n_ten ? 1 : 0;
      G.race_spec = spec;
      memcpy(G.race_rv, rv, sizeof rv);
      G.race_nrv = ten || vs ? 0 : G.n_rival;
      G.race_course = course;
      G.period_fade = G.menu.pre_fade;
      G.period_in = G.period_fade ? MOVIE_FADE : 0;
      G.menu.pre_fade = 0;
      if (G.menu.ending == 2) {
        movie_open(111, 0, rows, n_rival);
        G.menu.ending_t = G.menu.ending_skip = 0;
        if (G.movie_len <= 0) fprintf(stderr, "story ending: no movie, the staff roll alone\n");
      } else if (!ten && G.menu.game == MENU_GAME_STORY && G.rival >= 0)
        movie_open(RIVALDB[G.rival].select, G.story_race, rows, n_rival);
      perf_open();
      return;
    }
  }
  menu_scene scene;
  int has3d = menu_scene3d(&G.menu, &scene);
  menu_aero_sync(has3d ? &scene : NULL);
  wheel_sync(has3d ? &scene : NULL);
  get_wheel_model(has3d ? scene.get_wheel : 0);
  load_wants want;
  load_wants_now(has3d ? &scene : NULL, &want);
  if (G.low_memory) car_packs_trim(1, &want);
  car_paints_trim(&want, 0);
  load_sync(&want);
  /* NOW LOADING: the car select's cars on show, or the race title's course */
  int waiting = (has3d && !scene_ready(&scene)) || (G.menu.screen == MENU_PRE && !G.menu.ready);
  G.menu.loading = waiting ? G.menu.loading + 1 : 0;
  G.load_hurry = waiting || pre || menu_booting(&G.menu);
  G.load_wait = has3d && waiting;
  if (G.menu.screen != MENU_COURSE && G.menu.screen != MENU_TEN_COURSE && G.menu.screen != MENU_VS_AREA) G.mini_shown = NULL;
  menu_draw(&G.menu, G.hud.screen_w, G.hud.screen_h, has3d ? menu_draw3d : NULL, &scene, mini_draw);
  if (G.edge > 0 && !menu_photo_bg() && !anm_wide_drawn()) wide_sides();
  if (!G.loader) load_step(frame_start);
}

/* clCarDispManager::setupCar gives every car its shadow (clRenderShadow::createShadow) at
 * scale 1, 1.5 units below the car in world space */
#define SHADOW_DROP 1.5f

static void shadow_of(const mat4 *vp, const float *body, mat4 *out) {
  mat4 m;
  memcpy(m.m, body, sizeof m.m);
  m.m[13] -= SHADOW_DROP;
  *out = mat4_mul(*vp, m);
}

/* within far of the eye (a car's origin) */
static int near_eye(const float *m, const float *eye, float far) {
  float d[3] = { m[12] - eye[0], m[13] - eye[1], m[14] - eye[2] };
  return d[0] * d[0] + d[1] * d[1] + d[2] * d[2] <= far * far;
}

/* one pass of the race's cars, seen from eye_pos: the player's own (own: not from its roof),
 * the rivals' and the traffic's (those within far, 0 all) */
static void draw_race_cars(const car_state *c, const mat4 *vp, const float *eye_pos, int pass, int movie, int own,
                           float far, render_params *rp) {
  mat4 car_m;
  memcpy(car_m.m, c->body, sizeof car_m.m);
  float eye_car[3];
  eye_in(&car_m, eye_pos, eye_car);
  if (own) {
    float range = rp->range[WPK_GROUND];
    rp->range[WPK_GROUND] = 1e9f;
    wpk_paint(G.race_model, chosen_color()->paint); /* a rival may share the model */
    rp->wheels = (const float (*)[16])c->tyre;
    rp->wheel_rot = c->tyre_rot;
    rp->braking = c->brake;
    if (G.aero_set) wpk_share_sync(&G.aero);
    rp->mirror = G.menu.mirror[G.menu.maker][G.menu.car];
    rp->wing = G.menu.wing[G.menu.maker][G.menu.car];
    rp->bonnet = G.menu.bonnet[G.menu.maker][G.menu.car];
    int wheel = G.menu.wheel[G.menu.maker][G.menu.car];
    rp->wheel_custom = wheel_on(wheel);
    rp->over = G.over_drawn ? OVER_AMOUNT : 0.0f;
    {
      const char *page1 = "";
      wpk_scene *cs = G.aero_set ? &G.aero : G.race_model;
      uint32_t vt = vinyl_tex(G.race_pack, G.aero_set, chosen_color()->paint, G.menu.vinyl[G.menu.maker][G.menu.car],
                              rp->bonnet, &page1), vold = 0;
      int vi = vinyl_on(cs, vt, page1, &vold);
      wpk_draw(cs, vp, &car_m, eye_car, pass, rp);
      vinyl_off(cs, vi, vold);
    }
    draw_wheels(wheel, G.race_pack, G.aero_set, vp, &car_m, eye_car, pass, rp);
    rp->over = 0;
    /* the rivals' and the traffic's cars: stock */
    rp->mirror = rp->wing = rp->bonnet = rp->wheel_custom = 0;
    rp->wheels = NULL;
    rp->range[WPK_GROUND] = range;
  }
  for (int i = 0; i < CAR_RIVALS; ++i) {
    wpk_scene *model = movie ? G.movie_model[i] : G.rival_model[i];
    if (!c->rival_shown[i] || !model) continue;
    mat4 m;
    memcpy(m.m, c->rival_body[i], sizeof m.m);
    float eye[3];
    eye_in(&m, eye_pos, eye);
    wpk_paint(model, movie ? G.movie_paint[i] : G.rival_paint[i]);
    rp->wheels = (const float (*)[16])c->rival_tyre[i];
    rp->wheel_rot = c->rival_tyre_rot[i];
    rp->braking = c->rival_brake[i];
    if (!movie && G.vs_race) {
      /* a versus' other car: dressed up as its player's */
      const vs_dress_t *d = &G.rival_dress[i];
      int pack = G.rival_pack[i];
      wpk_scene *aero = d->aero ? menu_aero(pack, d->aero) : NULL;
      if (aero) wpk_share_sync(aero), model = aero;
      int body = aero ? d->aero : 0;
      rp->mirror = d->mirror, rp->wing = d->wing, rp->bonnet = d->bonnet;
      rp->wheel_custom = wheel_on(d->wheel);
      const char *page1 = "";
      uint32_t vt = vinyl_tex(pack, body, G.rival_paint[i], d->vinyl, d->bonnet, &page1), vold = 0;
      int vi = vinyl_on(model, vt, page1, &vold);
      wpk_draw(model, vp, &m, eye, pass, rp);
      vinyl_off(model, vi, vold);
      draw_wheels(d->wheel, pack, body, vp, &m, eye, pass, rp);
      rp->mirror = rp->wing = rp->bonnet = rp->wheel_custom = 0;
    } else {
      wpk_draw(model, vp, &m, eye, pass, rp);
    }
    rp->wheels = NULL;
  }
  for (int i = 0; i < c->n_traffic; ++i) {
    const car_traffic *t = &c->traffic[i];
    mat4 m;
    memcpy(m.m, t->body, sizeof m.m);
    if (far > 0.0f && !near_eye(m.m, eye_pos, far)) continue;
    float eye[3];
    eye_in(&m, eye_pos, eye);
    if (G.traffic[t->model].n_batch) wpk_draw(&G.traffic[t->model], vp, &m, eye, pass, rp);
  }
}

/* The back mirror (clBackMirror, clGraphicFrame's traverse and drawHigh): clRaceThread::yield
 * shows it while the camera looks from the roof (the play view 0: CSeq::GetInfoMirrorCamera),
 * outside the 10-in-a-row and before the goal (play state 8), its own scene drawn into a
 * 200x72 target at the camera's fov across it, then put at x 220..420, 15..87 down the
 * arcade's 640x480, flipped left to right. The OPTION screen turns it off (the port's).
 * The arcade's scene is the lua's BackmirrorModel (the ground only) to 50 m (far 500) on a
 * pale clear colour, which showed as holes where the tunnels' roofs and the walls past 50 m
 * were (Allan, 10-26): the port's draws the sky box, the ground to BM_FAR and the buildings
 * beside it to BM_SHORT (the harness's morning C1: 67 draws, 20k triangles; the far ones 80,
 * 24k), fading into the fog's colour from BM_FOG on. */
#define BM_X 220.0f
#define BM_Y 15.0f
#define BM_W 200.0f
#define BM_H 72.0f
#define BM_FAR 1000.0f
#define BM_SHORT 600.0f
#define BM_FOG 300.0f
static void draw_backmirror(const car_state *c, int movie) {
  G.bm_shown = G.bm_draws = G.bm_tris = 0;
  G.bm_ms = 0;
  if (movie || !c->mirror || G.n_ten || !G.menu.bm_on || G.phone || c->race.play_state >= 7 || getenv("WMMT_NO_BM")) return;
  double t0 = now_ms();
  float k = (float)G.fb_h / 480.0f;
  int w = (int)(BM_W * k + 0.5f), h = (int)(BM_H * k + 0.5f);
  if (render_backmirror_begin(w, h, G.rp.fog_color)) return;
  float aspect = BM_W / BM_H;
  float fovy = 2.0f * atanf(tanf(c->mirror_fov * 0.5f) / aspect);
  mat4 view;
  memcpy(view.m, c->mirror_view, sizeof view.m);
  mat4 vp = mat4_mul(mat4_perspective(fovy, aspect, c->mirror_near, BM_FAR), view);
  float eye[3];
  const float *V = c->mirror_view;
  for (int j = 0; j < 3; j++) eye[j] = -(V[12] * V[4 * j] + V[13] * V[4 * j + 1] + V[14] * V[4 * j + 2]);
  render_params rp = G.rp;
  rp.range[WPK_GROUND] = BM_FAR;
  rp.range[WPK_SHORT] = BM_SHORT;
  /* the big structures too (a bridge's deck and towers are the map's long batches: the car on
   * a bridge saw itself over nothing, Allan 10-29), to the buildings' range */
  rp.range[WPK_LONG] = BM_SHORT;
  rp.fog_near = BM_FOG;
  rp.fog_far = BM_FAR;
  rp.object_visible = NULL;
  rp.n_objects = 0;
  rp.draws = rp.triangles = rp.new_chunks = 0;
  /* the sky box, as the race's (no fog, behind everything) */
  wpk_scene *sky = &G.sky[G.track_zone];
  if (sky->n_batch) {
    mat4 sky_vp = mat4_mul(mat4_perspective(fovy, aspect, 100.0f, 600000.0f), view);
    mat4 sky_m = mat4_identity();
    sky_m.m[12] = eye[0];
    sky_m.m[13] = eye[1] - SKY_DROP;
    sky_m.m[14] = eye[2];
    const float sky_eye[3] = { 0, SKY_DROP, 0 };
    render_params sky_rp = { .fog_far = 2e9f, .fog_near = 1e9f, .range = { 1e9f, 1e9f, 1e9f } };
    render_begin(&sky_rp);
    for (int pass = 0; pass < WPK_PASSES; ++pass) {
      render_blend_pass(pass == WPK_BLEND);
      wpk_draw(sky, &sky_vp, &sky_m, sky_eye, pass, &sky_rp);
    }
    render_blend_pass(0);
    glClear(GL_DEPTH_BUFFER_BIT);
    rp.draws += sky_rp.draws;
    rp.triangles += sky_rp.triangles;
  }
  set_car_light(&rp, &CAR_LIGHT[G.track_city][G.track_zone], &view);
  render_begin(&rp);
  mat4 ident = mat4_identity();
  for (int pass = 0; pass < WPK_PASSES; ++pass) {
    render_blend_pass(pass == WPK_BLEND);
    wpk_draw(&G.track, &vp, &ident, eye, pass, &rp);
    /* not the player's own car: its body, wing or roof parts filled the view from the arcade's
     * mirror camera (a HIACE, an aero kit: Allan, 10-26) */
    draw_race_cars(c, &vp, eye, pass, 0, 0, BM_FAR, &rp);
  }
  render_blend_pass(0);
  render_backmirror_end();
  G.bm_shown = 1;
  G.bm_draws = rp.draws;
  G.bm_tris = rp.triangles;
  G.bm_ms = now_ms() - t0;
}

/* the player's car's and the traffic's shadows, onto what the opaque passes drew */
static void draw_shadows(const mat4 *vp, const mat4 *car_m) {
  const wpk_scene *s[2 + CAR_TRAFFIC];
  mat4 mvp[1 + CAR_RIVALS + CAR_TRAFFIC];
  int n = 0;
  const car_state *c = &G.car;
  if (G.camera == 0 && G.race_model) {
    s[n] = G.aero_set ? &G.aero : G.race_model;
    shadow_of(vp, car_m->m, &mvp[n++]);
  }
  for (int i = 0; i < CAR_RIVALS; ++i)
    if (c->rival_shown[i] && G.rival_model[i]) {
      s[n] = G.rival_model[i];
      shadow_of(vp, c->rival_body[i], &mvp[n++]);
    }
  for (int i = 0; i < c->n_traffic && n < 1 + CAR_RIVALS + CAR_TRAFFIC; ++i) {
    const car_traffic *t = &c->traffic[i];
    if (!G.traffic[t->model].n_batch) continue;
    s[n] = &G.traffic[t->model];
    shadow_of(vp, t->body, &mvp[n++]);
  }
  render_shadows(s, mvp, n, &G.rp);
}

/* a point in a model's local frame (its matrix a rotation and a translation) */
static void eye_in(const mat4 *m, const float *p, float *out) {
  float r[3] = { p[0] - m->m[12], p[1] - m->m[13], p[2] - m->m[14] };
  for (int j = 0; j < 3; ++j) out[j] = r[0] * m->m[4 * j] + r[1] * m->m[4 * j + 1] + r[2] * m->m[4 * j + 2];
}

void app_frame(float dt) {
  double frame_start = now_ms();
  G.frame_start = frame_start;
  anm_trim();
  if (dt > 0.1f) dt = 0.1f;
  if (dt > 0) G.fps += (1.0f / dt - G.fps) * 0.05f;
  wpk_ao(G.ao_test >= 0 ? G.ao_test : G.menu.ao_on);
#ifdef WMMT_EGL
  /* test: the versus stars screen for "stars change [wins]" once the car or game mode select is up */
  static int star_test;
  if (getenv("WMMT_STAR") && !star_test && G.in_menu && (G.menu.screen == MENU_MAKER || G.menu.screen == MENU_GAME)) {
    star_test = 1;
    int from = 0, d = 0, w = 0;
    sscanf(getenv("WMMT_STAR"), "%d %d %d", &from, &d, &w);
    G.menu.star_from = from, G.menu.star_delta = d;
    G.menu.vs_stars_car[G.menu.maker][G.menu.car] = from + d;
    G.menu.vs_wins_car[G.menu.maker][G.menu.car] = w;
    menu_vs_star_open(&G.menu);
  }
#endif
  if (G.in_menu) {
    menu_frame(dt, frame_start);
    perf_menu(dt, now_ms() - frame_start);
    return;
  }

  /* a versus: the server hears from this app all through the race (and its end) */
  if (G.vs_race && net_is_open()) {
    char name[NET_NAME + 1];
    unsigned char profile[VS_PROFILE];
    vs_name(name, sizeof name);
    vs_profile(profile);
    /* (a host relays the others' packets all through it too) */
    if (vs_host_is_open()) vs_host_poll(now_ms() * 1e-3, name);
    net_poll(now_ms() * 1e-3, 1, name, profile, VS_PROFILE);
    /* the host closed its game (or went quiet once the race is on: its app gone): the notice,
     * then back to the versus screen (Allan 10-27: the other one drove on alone) */
    if (!G.vs_closed && !G.menu.finish &&
        (net_host_closed() ||
         (G.car.race.state == CAR_RACE_RUN && G.car.race.countdown <= 0.0f && net_silence(now_ms() * 1e-3) > VS_GONE))) {
      G.vs_closed = 1;
      fprintf(stderr, "versus: the host %s\n", net_host_closed() ? "closed the game" : "went quiet");
    }
  }
  if (G.vs_closed && ++G.vs_closed > VS_CLOSED_NOTICE) {
    vs_off();
    G.vs_closed = 0;
    G.paused = 0;
    snd_quiet();
    hud_init(&G.hud, G.hud.screen_w, G.hud.screen_h);
    menu_vs_closed(&G.menu, "THE HOST CLOSED THE GAME");
    G.in_menu = 1;
    if (G.perf) fflush(G.perf);
    menu_frame(0, frame_start);
    return;
  }
  /* the story ending (menu.c): its frames over the movie, the movie held at its end; a tap to
   * the logo; then the menus */
  int ending = G.menu.ending == 2;
  if (ending) {
    int steps = (int)(dt * 60.0f + 0.5f);
    if (steps < 1) steps = 1;
    if (G.menu.ending_skip == 1 && G.menu.ending_t < MENU_ENDING_SKIP) {
      fprintf(stderr, "story ending: skipped at frame %d\n", G.menu.ending_t);
      G.menu.ending_t = MENU_ENDING_SKIP;
      G.menu.ending_skip = 2;
    }
    G.menu.ending_t += steps;
    if (G.menu.ending_t >= MENU_ENDING_LEN) {
      fprintf(stderr, "story ending: over\n");
      if (G.movie_len > 0) car_movie_end(), hud_demo_end();
      G.movie_len = 0;
      snd_quiet();
      hud_init(&G.hud, G.hud.screen_w, G.hud.screen_h);
      G.story_race = 0;
      menu_ending_done(&G.menu);
      G.in_menu = 1;
      if (G.perf) fflush(G.perf);
      menu_frame(0, frame_start);
      return;
    }
  }
  int movie = G.movie_len > 0 || ending;
  /* the pause menu: the race stands still under it (not once the race has stopped running) */
  if (!movie && G.hud.pause_pressed && !G.paused && !G.menu.finish && G.car.race.state == CAR_RACE_RUN) {
    G.paused = 1;
    hud_init(&G.hud, G.hud.screen_w, G.hud.screen_h); /* let go of the pedals and the wheel */
    menu_pause_open(&G.menu);
  }
  if (G.paused) {
    snd_quiet();
    int r = menu_pause_update(&G.menu, dt);
    if (r == MENU_LEAVE) {
      /* the screen is white: on to the menus, which come out of it (a story select starts
       * over from its first race) */
      G.paused = 0;
      G.story_race = 0;
      hud_init(&G.hud, G.hud.screen_w, G.hud.screen_h);
      G.in_menu = 1;
      if (G.perf) fflush(G.perf);
      menu_frame(0, frame_start);
      return;
    }
    if (r == MENU_RESUME) G.paused = 0, G.hud.pause_pressed = 0;
  }
  /* the race's end, FINISH or TIME UP over the race running on, then white: the menus */
  if (G.menu.finish && menu_finish_update(&G.menu, dt) == MENU_LEAVE) {
    snd_quiet();
    hud_init(&G.hud, G.hud.screen_w, G.hud.screen_h);
    G.in_menu = 1;
    if (G.perf) fflush(G.perf);
    menu_frame(0, frame_start);
    return;
  }

  /* input and simulation (the arcade's fixed 60 Hz frames) */
  car_input in;
  hud_options();
  hud_input(&G.hud, &in);
  if (G.hud.camera_pressed) {
    G.camera ^= 1;
    car_view(G.camera ? CAR_VIEW_ROOF : CAR_VIEW_BEHIND);
    G.hud.camera_pressed = 0;
  }
  /* the reset key: the car back on the road once it is in the player's hands (not over the
   * rolling start's pilot, a beaten rival's still frames or the finish) */
  if (G.hud.reset_pressed) {
    const car_race *r = &G.car.race;
    if (r->state == CAR_RACE_RUN && r->countdown <= 0.0f && r->play_state == 2) car_reset(&G.car);
    G.hud.reset_pressed = 0;
  }
  if (G.hud.trans_pressed) {
    /* the arcade fixes the gearbox for a race; here it switches on the move */
    G.menu.manual ^= 1;
    car_set_manual(&G.car, G.menu.manual);
    garage_save();
    G.hud.trans_pressed = 0;
  }
  /* One arcade frame per display frame: dt comes from vsync-aligned timestamps, so
   * rounding it counts the vsyncs that went by. (An accumulator drifting around 1/60 s now
   * and then ran no step one frame and two the next, a visible hitch.) */
  double t0 = now_ms();
  int steps = (int)(dt * 60.0f + 0.5f);
  /* the first frames' dt holds the race start and its first draw, not driving time */
  if (steps < 1 || G.race_frames < 2) steps = 1;
  if (G.paused && !G.vs_race) steps = 0; /* a versus runs on with the others under it */
  if (G.period_in > 0) G.period_in -= steps;
  if (movie) { /* clRaceThread::race: the movie's frames, then the race */
    for (int i = 0; i < steps && G.movie_len > 0; ++i) {
      /* its show's frame first: while that holds the movie (clEffectTable +0x1e0), the movie's
       * time stands and its picture with it (clGraphicFreezeCallback) */
      if (hud_demo_step()) continue;
      if (ending && G.movie_frame + 1.0f >= G.movie_len) break; /* (held at its last frame) */
      G.movie_frame += 1.0f;
      if (G.movie_frame >= G.movie_len) movie_close();
      else car_movie_update(&G.car, G.movie_frame);
    }
    steps = 0;
  } else if (G.movie_in > 0) {
    G.movie_in -= steps;
  }
  int frozen = G.ten_freeze > 0;
  if (frozen) {
    G.ten_freeze -= steps;
    hud_ten_cutin(&G.car.race, steps);
    steps = 0;
    if (G.ten_freeze <= 0 && !G.ten_bgm_off) G.ten_white = 1; /* more rivals: white out */
  }
  for (int i = 0; i < steps; ++i) {
    if (G.ten_white && ++G.ten_white > 1 + TEN_WHITE_HOLD + TEN_WHITE_FADE) G.ten_white = 0;
    car_update(&G.car, &in);
    in.shift_up = in.shift_down = 0;
    /* clCarEffectManager::update's sparks, each car's */
    spark_step(0, G.car.body, &G.car.effect);
    for (int k = 0; k < CAR_RIVALS; ++k)
      spark_step(1 + k, G.car.rival_body[k], G.car.rival_shown[k] ? &G.car.rival_effect[k] : NULL);
    hud_ten_cutin(&G.car.race, 1);
    /* clRaceThread::race on a 10-in-a-row rival beaten (play state 9, one arcade frame): the
     * picture stands still 60 frames (the graphics on clGraphicFreezeCallback, nothing run),
     * colours inverted (clGraphicFrame +0x174, neg.cg), with cutin2 (SE 0x4c) and the beaten
     * one's line coming up (Cut::clCutIn::Update each frame); after the last rival the BGM
     * stops (clAudio::stopStream) */
    if (G.n_ten && G.car.race.play_state == 9 && !G.ten_beaten_on) {
      G.ten_beaten_on = 1;
      G.ten_freeze = 60;
      snd_menu(SND_BEATEN);
      if (G.car.race.ten_beaten >= G.n_ten) G.ten_bgm_off = 1;
      break;
    }
    if (G.car.race.play_state != 9) G.ten_beaten_on = 0;
  }
  if (!G.paused && !frozen && !movie) snd_update(&G.car, &in, dt);
  /* the time attack console's sounds: TIME EXTEND at a checkpoint, the last seconds' ticks */
  car_race *race = &G.car.race;
  /* the 10-in-a-row's rival of the round */
  if (G.n_ten && race->ten_rival >= 0 && race->ten_rival < G.n_ten && G.ten_pack[race->ten_rival] >= 0) {
    G.rival_model[0] = car_pack(G.ten_pack[race->ten_rival]);
    G.rival_paint[0] = G.ten_paint[race->ten_rival];
  }
  /* clRaceThread::race and changeTenRival as each rival is set up: approach2, the last's approach3 */
  if (G.n_ten && race->ten_rival >= 0 && race->ten_rival != G.ten_rival_on) {
    G.ten_rival_on = race->ten_rival;
    snd_menu(race->ten_rival == G.n_ten - 1 ? SND_APPROACH3 : SND_APPROACH2);
  }
  if (race->extended > 0) {
    snd_menu(SND_TIME_EXTEND);
    hud_time_extend(race->total);
  }
  race->extended = 0;
  int left = (int)race->time_left;
  if (left < G.time_shown && left <= 9 && race->state != CAR_RACE_GOAL)
    snd_menu(left >= 5 ? SND_TIME_COUNT : SND_TIME_COUNT_LAST);
  G.time_shown = left;
  /* the start's countdown (clStandardConsole::update -> clCountDownConsole::setCountDown):
   * (int)(the seconds left + 1/12) as it changes; 2 and 1 with start321, 0 (GO) with start_go */
  int count = race->countdown > 0.0f ? (int)(race->countdown + 1.0f / 12.0f) : 0;
  if (count != G.count_shown && count <= 2) snd_menu(count ? SND_START321 : SND_START_GO);
  G.count_shown = count;
  /* the route sign's sound (clSignConsole::draw): as it comes, and the blinking ones' every
   * 20 frames while they stay; turning2 for those, turning3 for the green arrows */
  int sign = race->state == CAR_RACE_RUN ? race->sign : 0;
  if (sign != G.sign) {
    G.sign = sign;
    G.sign_t = 0;
  }
  if (G.sign > 0 && G.sign <= HUD_SIGNS && G.sign_t <= 0) {
    snd_menu(hud_sign_blinks(G.sign) ? SND_TURN2 : G.sign >= 12 ? SND_TURN3 : SND_TURN);
    G.sign_t = hud_sign_blinks(G.sign) ? 20 : 1 << 30;
  }
  G.sign_t -= steps;
  if (race->state == CAR_RACE_RUN && race->countdown <= 0.0f) {
    G.top_kmh = fmaxf(G.top_kmh, car_kmh(&G.car));
    /* the card's mileage */
    if (G.menu.own_sel >= 0 && G.menu.own_sel < G.menu.n_own) G.menu.own[G.menu.own_sel].odo += car_kmh(&G.car) / 3.6 * steps / 60.0;
  }
#ifdef WMMT_EGL
  if (getenv("WMMT_ST_END") && (G.rival >= 0 || G.vs_race) && !movie && G.race_frames > atoi(getenv("WMMT_ST_END")))
    race->over = race->won = 1; /* test: the story race won after this many frames */
#endif
  if (race->over && !G.menu.finish && !movie) {
    int goal = race->state == CAR_RACE_GOAL;
    garage_save(); /* the mileage */
    menu_result res;
    if (G.vs_race) {
      /* the versus: its place (GetPlayResult) as the story's WIN or LOSE */
      int place = race->vs_place > 0 ? race->vs_place : G.vs_race + 1;
      menu_finish_open(&G.menu, goal, NULL);
      menu_finish_vs(&G.menu, place, G.vs_race + 1);
      if (place == 1) G.menu.vs_wins_car[G.menu.maker][G.menu.car]++;
      int stars = menu_vs_stars(&G.menu, place, G.vs_race + 1);
      fprintf(stderr, "versus: stars %d -> %d (%+d)\n", G.menu.star_from, G.menu.star_from + stars, stars);
      title_race_over(TITLE_VS, place == 1, race, G.race_course, 0);
      garage_save();
      fprintf(stderr, "versus: place %d of %d, %.2f s\n", place, G.vs_race + 1, race->total);
    } else if (G.rival >= 0 && G.menu.game == MENU_GAME_STORY) {
      /* a story race: WIN moves the story on, no time attack records */
      /* a select with a second race (clEffStoryType's periods: select 0 runs series 24, then 25
       * against the Z31 and the Blackbird) goes on to it; the last race's WIN moves the story on */
      int pick = G.menu.story_pick;
      int next = race->won && RIVALDB[G.rival].select == pick ? story_next_race(pick, G.story_race) : -1;
      int fade = next >= 0 && pick >= 0 && pick < STORYRACE_SELECTS && G.story_race < 5 ? STORYRACE_FADE[pick][G.story_race] : 0;
      menu_finish_open(&G.menu, goal, NULL);
      menu_finish_story(&G.menu, race->won, next >= 0, race->gap, fade);
      if (next >= 0) {
        G.story_race++;
      } else {
        G.story_race = 0;
        /* the points first: whether the episode was cleared before counts */
        int units = menu_story_points(&G.menu, race->won && RIVALDB[G.rival].select == pick);
        menu_story_lost(&G.menu, race->won && RIVALDB[G.rival].select == pick);
        fprintf(stderr, "story points: +%d units, gauge %d, tune due %d\n", units,
                G.menu.st_gauge[G.menu.maker][G.menu.car], G.menu.st_tune_due);
        /* no dress-up parts: the arcade's come from ghost battle only (Allan, 10-24) */
        int cleared = race->won && RIVALDB[G.rival].select == pick;
        if (cleared) {
          menu_story_clear(&G.menu, G.menu.story_n);
          menu_lvup_check(&G.menu);
        }
        /* getStorySuccessiveVictoryCount: the episodes won in a row */
        short *streak = &G.menu.st_streak_car[G.menu.maker][G.menu.car];
        *streak = cleared ? (short)(*streak < 30000 ? *streak + 1 : *streak) : 0;
        title_race_over(TITLE_STORY, cleared, race, G.race_course, 0);
        garage_save();
        G.menu.garage_changed = 0;
      }
      fprintf(stderr, "story race: %s, select %d race %d, story div %d bits %05x\n", race->won ? "WIN" : "LOSE", pick,
              G.story_race, G.menu.st_div, G.menu.st_bits);
    } else if (G.n_ten) {
      /* the 10-in-a-row's end: clTenResult */
      fprintf(stderr, "ten race: %s, %d rivals beaten, %.2f s, medal %d\n", race->won ? "CLEAR" : "OVER", race->ten_beaten,
              race->ten_now, race->ten_medal);
      /* clRaceThread::finiTen: all ten beaten outside the extend mode, the medal onto the card
       * when it beats the one there */
      signed char *best = &G.menu.ten_result[menu_ten_course(&G.menu)][G.menu.ten_level];
      if (G.menu.ten_level < 10 && race->ten_medal > *best) {
        *best = (signed char)race->ten_medal;
        menu_lvup_check(&G.menu);
      }
      title_race_over(TITLE_TEN, race->won, race, G.race_course, 0);
      garage_save();
      menu_finish_open(&G.menu, goal, NULL);
      int beaten = race->ten_beaten, medal = race->ten_medal;
      if (getenv("WMMT_TEN_RES")) sscanf(getenv("WMMT_TEN_RES"), "%d %d", &beaten, &medal); /* test: this end */
      menu_finish_ten(&G.menu, beaten, race->ten_now, medal);
      menu_dress_win(&G.menu, getenv("WMMT_DRESS_WIN") ? atoi(getenv("WMMT_DRESS_WIN")) : beaten);
    } else {
      if (goal) records_goal(G.menu.course, race, &res);
      menu_finish_open(&G.menu, goal, goal ? &res : NULL);
      title_race_over(TITLE_TA, goal, race, G.menu.course, goal && res.rank == 1);
      garage_save();
    }
  }
  double t1 = now_ms();

  /* the overhead map for the player's car first, before anything goes to the frame's own
   * target (a tile renderer would store the frame and load it again to come back to it) */
  {
    double o0 = now_ms();
    G.over_draws = G.over_tris = 0;
    if (G.no_over || !G.menu.over_on || !(G.camera == 0 || movie)) {
      G.over_drawn = 0;
    } else if (!(G.race_frames & 1) || !G.over_drawn) {
      /* every other frame: a reflection 1/30 s old does not show, half its GPU time does */
      mat4 body;
      memcpy(body.m, G.car.body, sizeof body.m);
      const wpk_scene *over[2] = { &G.track, &G.arrows[G.track_zone] };
      int t0 = G.rp.triangles;
      G.over_draws = render_overhead(over, 2, &body, &G.rp);
      G.over_tris = G.rp.triangles - t0;
      G.over_drawn = G.over_draws >= 0;
    }
    G.over_ms = now_ms() - o0;
  }

  draw_backmirror(&G.car, movie);

  /* render: into the radial blur's texture while the camera asks for it (or the sharpening) */
  int sharp = G.menu.sharp_on && !G.no_sharp;
  if (sharp && dt > 1.5f / 60.0f && G.race_frames > 2) G.sharp_rest = SHARP_REST;
  else if (G.sharp_rest > 0) G.sharp_rest--;
  if (sharp && G.sharp_rest > 0) G.sharp_rested++;
  render_sharpen(sharp && !G.sharp_rest ? SHARP_AMOUNT : 0.0f, sharp);
  int blurred = render_blur_begin(G.phone ? 0.0f : G.car.cam_blur, G.fb_w, G.fb_h);
  int scene_w = G.fb_w, scene_h = G.fb_h;
  if (blurred) render_blur_size(&scene_w, &scene_h);
  glViewport(0, 0, scene_w, scene_h);
  glClearColor(G.rp.fog_color[0], G.rp.fog_color[1], G.rp.fog_color[2], 1);
  glDepthMask(GL_TRUE);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  /* the arcade's own camera: fixed to the car, with its sway */
  const car_state *c = &G.car;
  float aspect = (float)G.fb_w / (float)G.fb_h;
  /* the arcade's view across 4:3; a wider screen sees more at the sides */
  float fovy = 2.0f * atanf(tanf(c->cam_fov * 0.5f) / (4.0f / 3.0f));
  mat4 proj = mat4_perspective(fovy, aspect, c->cam_near, 20000.0f);
  render_hq_view(c->cam_near, 20000.0f, tanf(fovy * 0.5f) * aspect, tanf(fovy * 0.5f));
  mat4 view;
  memcpy(view.m, c->cam_view, sizeof view.m);
  mat4 vp = mat4_mul(proj, view);
  mat4 ident = mat4_identity();
  mat4 car_m;
  memcpy(car_m.m, c->body, sizeof car_m.m);

  /* clCourseArrow::setup hides only the groups the junction block table names; the rest
   * (the boards along a route, e.g. TkC1_In) always show */
  if (!G.arrow_table_ready) {
    for (int i = 0; i < G.n_arrows; ++i)
      for (int id = 1; id < CAR_JCT_BLOCKS; ++id) {
        const char *name = car_jct_block_name(id);
        if (name && same_name(G.arrow_names[i], name)) G.arrow_switched[i] = 1;
      }
    G.arrow_table_ready = 1;
  }
  for (int i = 0; i < G.n_arrows; ++i) G.arrow_visible[i] = !G.arrow_switched[i];
  /* plus the junction blocks the arcade shows for where the car is now */
  for (int k = 0; k < 2; ++k) {
    const char *name = car_jct_block_name(c->jct_block[k]);
    for (int i = 0; name && i < G.n_arrows; ++i)
      if (same_name(G.arrow_names[i], name)) G.arrow_visible[i] = 1;
  }
  /* clCourseArrow::update's Hakone: the board pointing up the pass (hk_for) once the car is
   * on its way out (place 0x3a, or 0x3d 100 m in), back down (hk_back) on the way home (0x3c,
   * or 0x3b 100 m in); kept till the next, both hidden at place 0 */
  if (G.track_city == CAR_HAKONE) {
    int pl = c->race.place;
    if (pl == 0) G.hk_arrow = 0;
    else if (pl == 0x3a || (pl == 0x3d && c->race.place_at * 0.001f >= 100.0f)) G.hk_arrow = 1;
    else if (pl == 0x3c || (pl == 0x3b && c->race.place_at * 0.001f >= 100.0f)) G.hk_arrow = 2;
    for (int i = 0; i < G.n_arrows; ++i) {
      if (same_name(G.arrow_names[i], "hk_for")) G.arrow_visible[i] = G.hk_arrow == 1;
      if (same_name(G.arrow_names[i], "hk_back")) G.arrow_visible[i] = G.hk_arrow == 2;
    }
  }
  G.rp.object_visible = G.arrow_visible;
  G.rp.n_objects = G.n_arrows;

  G.rp.draws = G.rp.triangles = G.rp.new_chunks = 0;
  G.rp.time += dt;
  double seg_t = now_ms();
#define SEG_END(k)                    \
  do {                                \
    double seg_now = now_ms();        \
    G.seg[k] = seg_now - seg_t;       \
    seg_t = seg_now;                  \
  } while (0)
  /* the sky box (SkyBoxModel), centred on the camera: no fog, behind everything */
  wpk_scene *sky = &G.sky[G.track_zone];
  if (sky->n_batch) {
    /* clGraphicFrame's far plane for the sky box's view: 600000 (0x49127c00, the views of
     * mask 1 and 8). Hakone's and Osaka's domes reach 555000 out: a nearer far plane cuts the
     * horizon straight ahead out of the sky, and the clear colour shows there, moving with the car */
    mat4 sky_vp = mat4_mul(mat4_perspective(fovy, aspect, 100.0f, 600000.0f), view);
    mat4 sky_m = mat4_identity();
    sky_m.m[12] = c->cam_pos[0];
    sky_m.m[13] = c->cam_pos[1] - SKY_DROP;
    sky_m.m[14] = c->cam_pos[2];
    const float sky_eye[3] = { 0, SKY_DROP, 0 };
    render_params sky_rp = { .fog_far = 2e9f, .fog_near = 1e9f, .range = { 1e9f, 1e9f, 1e9f } };
    memcpy(sky_rp.grade, G.rp.grade, sizeof sky_rp.grade);
    render_begin(&sky_rp);
    for (int pass = 0; pass < WPK_PASSES; ++pass) {
      render_blend_pass(pass == WPK_BLEND);
      wpk_draw(sky, &sky_vp, &sky_m, sky_eye, pass, &sky_rp);
    }
    render_blend_pass(0);
    glClear(GL_DEPTH_BUFFER_BIT);
  }
  SEG_END(SEG_SKY);
  set_car_light(&G.rp, &CAR_LIGHT[G.track_city][G.track_zone], &view);
  render_begin(&G.rp);
  for (int pass = 0; pass < WPK_PASSES; ++pass) {
    render_blend_pass(pass == WPK_BLEND);
    wpk_draw(&G.track, &vp, &ident, c->cam_pos, pass, &G.rp);
    wpk_draw(&G.arrows[G.track_zone], &vp, &ident, c->cam_pos, pass, &G.rp);
    draw_race_cars(c, &vp, c->cam_pos, pass, movie, G.camera == 0 || movie, G.phone ? PHONE_TRAFFIC_FAR : 0.0f, &G.rp);
    if (pass == WPK_CUTOUT) draw_shadows(&vp, &car_m);
    SEG_END(SEG_OPAQUE + pass);
  }
  /* the headlights' pools on the road (clCarSpotLight), then the street lights' and building
   * lights' glows, and the rows of lights at the goal and the next checkpoint */
  {
    const float *spot[1 + CAR_RIVALS];
    int n = 0;
    /* (the roof view too: the player's car is not drawn there, its lamps' pools still are) */
    if (c->spot) spot[n++] = c->body;
    for (int i = 0; i < CAR_RIVALS; ++i)
      if (c->rival_shown[i] && c->rival_spot[i] && (movie ? G.movie_model[i] : G.rival_model[i])) spot[n++] = c->rival_body[i];
    if (!getenv("WMMT_NO_SPOT")) lights_draw_spots(&proj, &view, spot, n, G.track_zone == 1, &G.rp);
  }
  /* the car's under neon (clCarDisp::update: clNeon::setPosture with the car's frame at its
   * hubs' mean height) */
  if ((G.camera == 0 || movie) && G.menu.neon[G.menu.maker][G.menu.car]) {
    mat4 f = car_m;
    float hub = 0.25f * (c->tyre[0][13] + c->tyre[1][13] + c->tyre[2][13] + c->tyre[3][13]);
    for (int j = 0; j < 3; ++j) f.m[12 + j] += hub * f.m[4 + j];
    lights_draw_neon(&proj, &view, f.m, G.race_pack, G.menu.aero[G.menu.maker][G.menu.car], G.menu.neon[G.menu.maker][G.menu.car], &G.rp);
  }
  /* a versus' other cars' */
  for (int i = 0; !movie && G.vs_race && i < CAR_RIVALS; ++i) {
    const vs_dress_t *d = &G.rival_dress[i];
    if (!d->neon || !c->rival_shown[i] || !G.rival_model[i]) continue;
    mat4 f;
    memcpy(f.m, c->rival_body[i], sizeof f.m);
    float hub = 0.25f * (c->rival_tyre[i][0][13] + c->rival_tyre[i][1][13] + c->rival_tyre[i][2][13] + c->rival_tyre[i][3][13]);
    for (int j = 0; j < 3; ++j) f.m[12 + j] += hub * f.m[4 + j];
    lights_draw_neon(&proj, &view, f.m, G.rival_pack[i], d->aero, d->neon, &G.rp);
  }
  lights_draw(&proj, &view, G.track_zone == 1, G.rp.time, &G.rp);
  lights_draw_goal(&proj, &view, &c->race, G.rp.time, &G.rp);
  lights_draw_ten_line(&proj, &view, &c->race, &G.rp);
  if (!movie && !getenv("WMMT_NO_SPARK")) spark_draw(&proj, &view, &G.rp);
  render_blend_pass(0);
  SEG_END(SEG_LIGHTS);
  /* the first race frame draws all it can see; after it, whatever the menus left cold that
   * comes into reach, for WARM_MS a frame */
  int warmed = 0;
  if (!G.race_frames) {
    warmed = wpk_prewarm(&G.track, &vp, c->cam_pos, 3000.0f, 1 << 30, &G.rp);
    /* and the traffic's models, before the first car comes into view */
    const float origin[3] = { 0, 0, 0 };
    for (int k = 0; k < CAR_TRAFFIC_MODELS; ++k) warmed += wpk_prewarm(&G.traffic[k], &vp, origin, 1e9f, 1 << 30, &G.rp);
  }
  for (double w0 = now_ms(); G.race_frames && now_ms() - w0 < WARM_MS;) {
    int k = wpk_prewarm(&G.track, &vp, c->cam_pos, WARM_MARGIN, 1, &G.rp);
    if (!k) k = wpk_prewarm(&G.arrows[G.track_zone], &vp, c->cam_pos, WARM_MARGIN, 1, &G.rp);
    if (!k) break;
    warmed += k;
  }
  G.race_frames++;
  SEG_END(SEG_WARM);
  if (blurred) render_blur_end();
  SEG_END(SEG_BLUR);
  if (G.bm_shown) {
    float k = (float)G.fb_h / 480.0f, x0 = 0.5f * ((float)G.fb_w - 640.0f * k);
    mat4 px = mat4_ortho(0, (float)G.fb_w, (float)G.fb_h, 0, -1, 1);
    render_backmirror_draw(&px, x0 + BM_X * k, BM_Y * k, BM_W * k, BM_H * k);
  }
  /* the consoles (and the touch controls with them) only while the race runs: yt::CState 1
   * and 2; then the end's sprites go up */
  hud_rival_view(view.m, proj.m);
  if (G.ten_freeze > 0) hud_negate(&G.hud);
  if (movie && G.menu.ending == 2) {
    /* the staff roll; a roll's band: the movie there only, the rest black */
    int band = menu_ending_band(G.menu.ending_t);
    if (band) {
      /* clSubScreen (0x85c9ba4): the staff roll's layers drawn off screen, then only the band
       * of them (0, 288 or 0, 0; 640 x 192: its UVs the band's own) over the movie */
      float k = (float)G.fb_h / 480.0f;
      glEnable(GL_SCISSOR_TEST);
      glScissor(0, band == 1 ? 0 : G.fb_h - (int)(192.0f * k + 0.5f), G.fb_w, (int)(192.0f * k + 0.5f));
    }
    menu_ending_draw(&G.menu);
    if (band) glDisable(GL_SCISSOR_TEST);
  } else if (movie) {
    hud_demo_draw(&G.hud);
    /* its last 20 frames fade out (clRaceThread::race: getFade), the race fades in from it */
    float left = G.movie_len - G.movie_frame;
    if (G.movie_fade && left < MOVIE_FADE) hud_fade(&G.hud, fade_grey(G.movie_fade), 1.0f - left / MOVIE_FADE);
  } else if (c->race.state == CAR_RACE_RUN) hud_draw(&G.hud, c, &in, G.fps);
  else menu_finish_warm(G.n_ten ? 3 : G.vs_race || (G.rival >= 0 && G.menu.game == MENU_GAME_STORY) ? 2 : c->race.state == CAR_RACE_GOAL);
  menu_finish_draw(&G.menu);
  if (!movie && G.movie_in > 0) hud_fade(&G.hud, fade_grey(G.movie_fade), (float)G.movie_in / MOVIE_FADE);
  if (G.period_in > 0) hud_fade(&G.hud, fade_grey(G.period_fade), (float)G.period_in / MOVIE_FADE);
  hud_white(&G.hud, ten_white_alpha());
  if (G.vs_closed) vs_closed_draw();
  SEG_END(SEG_HUD);
#undef SEG_END
  /* now and then, in a frame that is slow anyway (its render, or the frame before it, which
   * a GPU behind leaves at two vsyncs with little CPU), how long the GPU takes over its part
   * (glFinish waits for all of it): is the race short of CPU or of GPU? */
  static int gpu_sampled = -1000;
  if (G.perf && (now_ms() - t1 > 10.0 || dt > 1.5f / 60.0f) && G.race_frames - gpu_sampled >= 300) {
    gpu_sampled = G.race_frames;
    double g0 = now_ms();
    glFinish();
    fprintf(G.perf, "%7.2fs gpu sample: render %.2f ms, then the GPU %.2f ms more; draws %d tris %d kmh %.0f blur %.2f"
            " over %d sharp %d\n", G.race_time, g0 - t1, now_ms() - g0, G.rp.draws, G.rp.triangles, car_kmh(c),
            c->cam_blur, G.over_draws, G.menu.sharp_on && !G.sharp_rest);
  }
  if (G.paused) {
    menu_pause_draw(&G.menu);
    if (G.vs_closed) vs_closed_draw();
    return;
  }
  perf_frame(dt, steps, t1 - t0, now_ms() - t1, warmed, &G.rp);
  static int frame;
  if (frame < 10 || frame % 150 == 0)
    fprintf(stderr, "frame %d dt %.3f draws %d tris %d car %.1f %.1f %.1f cam %.1f %.1f %.1f traffic %d kmh %.0f blur %.3f err 0x%x\n",
            frame, dt, G.rp.draws, G.rp.triangles, c->pos[0], c->pos[1], c->pos[2], c->cam_pos[0], c->cam_pos[1],
            c->cam_pos[2], c->n_traffic, car_kmh(c), c->cam_blur, glGetError());
  frame++;
}
