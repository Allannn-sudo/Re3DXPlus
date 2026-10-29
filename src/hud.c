#include "hud.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "anm.h"
#include "gl.h"
#include "mathx.h"
#include "meterdb.h"
#include "sound.h"
#include "storydemo.h"
#include "storyinfo.h"
#include "namedb.h"
#include "title.h"

typedef struct { float x, y, w, h; } rect;

/* layout */
static const rect LEFT = { 16, 604, 120, 150 };
static const rect RIGHT = { 146, 604, 120, 150 };
static const rect THROTTLE = { 918, 590, 96, 164 };
static const rect BRAKE = { 812, 610, 96, 144 };
static const rect SHIFT_UP = { 706, 610, 96, 68 };
static const rect SHIFT_DOWN = { 706, 686, 96, 68 };
/* the top keys, either side of the back mirror's place (app.c draw_backmirror: 352..672
 * across the layout); a screen wider than 4:3 (the iPhone's, without the mirror) keeps them
 * in a row in the middle (hud_wide) */
/* the view and AT/MT left of pause (a story's three rival boxes take the top right) */
static rect PAUSE = { 236, 12, 48, 48 }; /* the pause menu */
static rect RESET = { 296, 12, 48, 48 };
static rect CAMERA = { 116, 12, 48, 48 };
static rect TRANS = { 176, 12, 48, 48 }; /* AT/MT */
/* the gyro layout: the brake where the steer buttons were, the shift buttons where the brake was */
static const rect G_BRAKE = { 16, 604, 250, 150 };
static const rect G_SHIFT_UP = { 812, 610, 96, 68 };
static const rect G_SHIFT_DOWN = { 812, 686, 96, 68 };
/* menu keys: in the two ends of the arcade's bottom bar (menu_g_bottombar_bar, x 25..996,
 * y 642..718 on screen), each taking a tap a little beyond its edges */
static const rect MENU_LEFT = { 36, 648, 176, 64 };
static const rect MENU_RIGHT = { 812, 648, 176, 64 };
#define MENU_KEY_SLOP 14
static const rect MENU_BACK = { 722, 46, 84, 60 }; /* in the top bar, left of the TIME badge */
static const rect MENU_OPTION = { 628, 46, 84, 60 }; /* left of back */
static const rect MENU_HIDDEN = { 328, 652, 368, 56 }; /* between the left and right keys */
/* A screen wider than 4:3 (an iPhone's 3:2, hud_wide): EDGE more layout px on each side of
 * the 1024x768 layout, which stays in the middle; the race's touch keys go out to the
 * screen's sides and grow by KEY_K about their corner there (a phone's are small). */
static float EDGE, KEY_K = 1;
enum { KEY_TOP, KEY_LEFT, KEY_RIGHT }; /* (hud.h's HUD_KEYS_*) */
/* the player's own layout (hud_layout_set): each side's keys scaled about their corner, moved */
static float USER_K[3] = { 1, 1, 1 }, USER_DX[3], USER_DY[3];

/* a rect of a side's keys on screen, the side k times its size and moved (dx, dy) */
static rect place(rect r, int side, float k, float dx, float dy) {
  float ox = side == KEY_LEFT ? 0 : side == KEY_RIGHT ? 1024 : 512, oy = side == KEY_TOP ? 0 : 768;
  float ex = side == KEY_LEFT ? -EDGE : side == KEY_RIGHT ? EDGE : 0;
  k *= KEY_K;
  return (rect){ ox + ex + dx + (r.x - ox) * k, oy + dy + (r.y - oy) * k, r.w * k, r.h * k };
}

/* a touch key's layout rect where it is on screen */
static rect key(rect r, int side) { return place(r, side, USER_K[side], USER_DX[side], USER_DY[side]); }

/* the projection of the layout's 1024x768 on the whole screen */
static mat4 screen_ortho(float w, float h) { return mat4_ortho(-EDGE, w + EDGE, h, 0, -1, 1); }

/* ... and of a side's touch keys, drawn at their layout rects (key()) */
static mat4 key_ortho(float w, float h, int side) {
  rect r = key((rect){ 0, 0, 1, 1 }, side);
  mat4 m = mat4_identity();
  m.m[0] = m.m[5] = KEY_K * USER_K[side];
  m.m[12] = r.x, m.m[13] = r.y;
  return mat4_mul(screen_ortho(w, h), m);
}

/* the gyro's dead zone about level (radians: 1.5 degrees) */
#define GYRO_DEAD 0.026f

/* the 10-in-a-row's rival entrances (the banner, its rolling start's pilot, the beaten one's
 * still frames: while TenDisp2D is clear, see ten_disp_update) and the rivals' lines (the
 * cut-in, which outlasts the banner by a few frames) show only the pause key, the others
 * neither drawn nor taking new touches (a port choice: the arcade has no touch keys) */
static int keys_pause_only(void);
static rect group_rect(int group, int gyro);

static int inside(rect r, float x, float y) {
  return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

/* a slider pedal's travel at the finger's height y on its key r (PEDAL_FOOT px at its foot
 * and its top margin count as 0 and 1) */
#define PEDAL_FOOT 12.0f
static float pedal_travel(rect r, float y) {
  float v = (r.y + r.h - PEDAL_FOOT - y) / (r.h - 2.0f * PEDAL_FOOT);
  return v < 0 ? 0.0f : v > 1 ? 1.0f : v;
}

void hud_init(hud_state *h, float screen_w, float screen_h) {
  memset(h, 0, sizeof *h);
  h->screen_w = screen_w;
  h->screen_h = screen_h;
}

void hud_touch(hud_state *h, void *id, int phase, float x, float y) {
  rect brake = h->gyro ? key(G_BRAKE, KEY_LEFT) : key(BRAKE, KEY_RIGHT);
  rect up = key(h->gyro ? G_SHIFT_UP : SHIFT_UP, KEY_RIGHT), down = key(h->gyro ? G_SHIFT_DOWN : SHIFT_DOWN, KEY_RIGHT);
  rect throttle = key(THROTTLE, KEY_RIGHT), left = key(LEFT, KEY_LEFT), right = key(RIGHT, KEY_LEFT);
  if (phase == TOUCH_BEGAN && keys_pause_only()) {
    if (inside(key(PAUSE, KEY_TOP), x, y)) h->pause_pressed++;
  } else if (phase == TOUCH_BEGAN) {
    if (inside(throttle, x, y)) h->throttle_touch = id;
    else if (inside(brake, x, y)) h->brake_touch = id;
    else if (inside(up, x, y)) h->shift_up++;
    else if (inside(down, x, y)) h->shift_down++;
    else if (inside(key(CAMERA, KEY_TOP), x, y)) h->camera_pressed++;
    else if (inside(key(TRANS, KEY_TOP), x, y)) h->trans_pressed++;
    else if (inside(key(RESET, KEY_TOP), x, y)) h->reset_pressed++;
    else if (inside(key(PAUSE, KEY_TOP), x, y)) h->pause_pressed++;
    else if (!h->gyro && inside(left, x, y)) h->left_touch = id;
    else if (!h->gyro && inside(right, x, y)) h->right_touch = id;
  } else if (phase == TOUCH_MOVED) {
    /* sliding a finger between the pedals, or between left and right, switches button */
    if (id == h->throttle_touch && inside(brake, x, y)) { h->throttle_touch = 0; h->brake_touch = id; }
    else if (id == h->brake_touch && inside(throttle, x, y)) { h->brake_touch = 0; h->throttle_touch = id; }
    else if (id == h->left_touch && inside(right, x, y)) { h->left_touch = 0; h->right_touch = id; }
    else if (id == h->right_touch && inside(left, x, y)) { h->right_touch = 0; h->left_touch = id; }
  }
  /* slider pedals: the finger's height (held on past the key's top or foot) */
  if (phase != TOUCH_ENDED && id == h->throttle_touch) h->throttle_v = pedal_travel(throttle, y);
  if (phase != TOUCH_ENDED && id == h->brake_touch) h->brake_v = pedal_travel(brake, y);
  if (phase != TOUCH_BEGAN && phase != TOUCH_MOVED) {
    if (id == h->left_touch) h->left_touch = 0;
    if (id == h->right_touch) h->right_touch = 0;
    if (id == h->throttle_touch) h->throttle_touch = 0;
    if (id == h->brake_touch) h->brake_touch = 0;
  }
}

void hud_input(hud_state *h, car_input *in) {
  /* digital steering, car_update ramping the wheel towards it; or the iPad's turn past the
   * dead zone, full lock at h->full */
  in->steer = (h->right_touch ? 1.0f : 0.0f) - (h->left_touch ? 1.0f : 0.0f);
  if (h->gyro) {
    float a = fabsf(h->tilt) - GYRO_DEAD, s = h->full > GYRO_DEAD ? a / (h->full - GYRO_DEAD) : 1.0f;
    s = a <= 0 ? 0.0f : s > 1.0f ? 1.0f : s;
    in->steer = h->tilt < 0 ? -s : s;
  }
  in->analog = h->gyro;
  in->throttle = h->throttle_touch ? (h->slide ? h->throttle_v : 1.0f) : 0.0f;
  in->brake = h->brake_touch ? (h->slide ? h->brake_v : 1.0f) : 0.0f;
  in->shift_up = h->shift_up > 0;
  in->shift_down = h->shift_down > 0;
  h->shift_up = h->shift_down = 0;
}

/* ---- drawing ---- */

static int navi_gl_init(void);
static void navi_load(const char *path);
static char navi_dir[1024];

static GLuint prog, tex_prog;
static GLint u_proj, u_color, u_tproj, u_tex, u_mul, u_add;

int hud_gl_init(void) {
  static const char *vs =
    "attribute vec2 aPos; uniform mat4 uProj;\n"
    "void main() { gl_Position = uProj * vec4(aPos, 0.0, 1.0); }\n";
  static const char *fs =
    "precision mediump float; uniform vec4 uColor;\n"
    "void main() { gl_FragColor = uColor; }\n";
  const char *attribs[] = { "aPos" };
  prog = gl_compile_program(vs, fs, attribs, 1);
  if (!prog) return -1;
  u_proj = glGetUniformLocation(prog, "uProj");
  u_color = glGetUniformLocation(prog, "uColor");
  static const char *tvs =
    "attribute vec2 aPos; attribute vec2 aUV; uniform mat4 uProj; varying vec2 vUV;\n"
    "void main() { vUV = aUV; gl_Position = uProj * vec4(aPos, 0.0, 1.0); }\n";
  static const char *tfs =
    "precision mediump float; uniform sampler2D uTex; uniform vec4 uMul, uAdd; varying vec2 vUV;\n"
    "void main() { gl_FragColor = clamp(texture2D(uTex, vUV) * uMul + uAdd, 0.0, 1.0); }\n";
  const char *tattribs[] = { "aPos", "aUV" };
  tex_prog = gl_compile_program(tvs, tfs, tattribs, 2);
  if (!tex_prog) return -1;
  u_tproj = glGetUniformLocation(tex_prog, "uProj");
  u_tex = glGetUniformLocation(tex_prog, "uTex");
  u_mul = glGetUniformLocation(tex_prog, "uMul");
  u_add = glGetUniformLocation(tex_prog, "uAdd");
  return navi_gl_init();
}

static void color(float r, float g, float b, float a) { glUniform4f(u_color, r, g, b, a); }
/* the sprites' colour, as the arcade's anm records give it: texel * mul + add */
static void tint(float r, float g, float b, float a) { glUniform4f(u_mul, r, g, b, a); }
static void tint_add(float r, float g, float b) { glUniform4f(u_add, r, g, b, 0); }

/* ---- the arcade race HUD (sprites from data/sprite, packed by tools/hudpack.py) ---- */
enum {
  SPR_SHIFT4, SPR_SHIFT5, SPR_SHIFT6, SPR_TAG_MT, SPR_TAG_AT,
  SPR_BALL_MT, SPR_BALL_AT = SPR_BALL_MT + 7, /* 7 each: blank, then gears 1..6 */
  SPR_PLATE = SPR_BALL_AT + 7, SPR_PILL, SPR_CLASS_N, SPR_CLASS_KYU, SPR_KMH, SPR_BARIKI,
  SPR_GRADE, /* the handling grades DG, D, B, G, HG (car_tune_grade order) */
  /* cut from the cons_p_01 atlas */
  SPR_BIG = SPR_GRADE + 5, SPR_BIG_SHADOW = SPR_BIG + 10, SPR_SMALL = SPR_BIG_SHADOW + 10,
  /* clSpriteFont text */
  SPR_TITLE = SPR_SMALL + 10, SPR_NAME,
  /* the time attack's console: "till goal" and TIME as sprites of their own, the rest cut
   * from cons_p_01 (hudpack.py TA_CROPS) */
  SPR_TILL_GOAL, SPR_TIME,
  SPR_TA_TOTAL, SPR_TA_QUOTE1, SPR_TA_QUOTE2, SPR_TA_SECTION, SPR_TA_PILL, SPR_MAP_WAKU, SPR_TIME_EXTEND,
  SPR_GOAL_DOT, SPR_GOAL_KM, SPR_GOAL_M,
  SPR_TA_DIGIT, SPR_LTIME = SPR_TA_DIGIT + 10, SPR_LTIME_SHADOW = SPR_LTIME + 10, SPR_GOAL_DIGIT = SPR_LTIME_SHADOW + 10,
  SPR_MAP_NAME = SPR_GOAL_DIGIT + 10,
  SPR_REC_YOUR = SPR_MAP_NAME + CAR_COURSES, SPR_REC_TIME, SPR_REC_DASH, SPR_BUYIN,
  SPR_SIGN, /* E2DSign 1..14 */
  SPR_MAP_TEXT = SPR_SIGN + HUD_SIGNS, /* EMapText 1..19 (clTimeConsole::setMapName) */
  SPR_RANK = SPR_MAP_TEXT + 19, /* the class's letters N A B C S, then its digits 0..9 */
  SPR_RANK_DIGIT = SPR_RANK + 5,
  SPR_COUNT = SPR_RANK_DIGIT + 10
};
typedef struct { GLuint tex; float w, h; } hud_sprite;
static hud_sprite SPR[SPR_COUNT];
static const float HUD_K = 1.1f; /* arcade sprite pixels -> points (1.6 would be the arcade's own size) */
static float CLUSTER_X = 948; /* (+ EDGE: hud_wide) */
static float CLUSTER_Y = 286; /* where the arcade's (560, 228) goes (hud_lift moves it) */

static GLuint upload(const unsigned char *px, unsigned w, unsigned h) {
  GLuint tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)w, (GLsizei)h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
  return tex;
}

/* ---- each car's own meters (src/meterdb.h; the art in meters.bin) ----
 * The arcade's clMeterConsole::drawMeter: the dials and needles of the car and the time
 * of day, each turned a0 + a1 * its input. meters.bin is indexed at hud_load and only the
 * chosen car's sprites are loaded, at hud_set_car. */
#define METER_CARS ((int)(sizeof METERDB / sizeof METERDB[0]))
static char meter_path[1024];
static unsigned short METER_INDEX[METER_CARS][METER_ZONES][METERDB_MAX_PARTS];
static unsigned *meter_table; /* w, h, file offset per sprite */
static unsigned meter_sprites;
static hud_sprite METER[METERDB_MAX_PARTS];
static const meterdb_car *meter_car;
static int meter_night; /* the zone in use: 2 * the card's meter + night */
static int meter_style; /* the card's meter (hud_set_meter): 0 Normal, 1 For Racing, 2 SPECIAL */

void hud_set_meter(int style) { meter_style = style >= 0 && style < METER_STYLES ? style : 0; }

static void meters_index(const char *data_dir) {
  snprintf(meter_path, sizeof meter_path, "%s/meters.bin", data_dir);
  FILE *f = fopen(meter_path, "rb");
  char magic[4];
  unsigned n[2];
  if (!f) return;
  if (fread(magic, 1, 4, f) == 4 && !memcmp(magic, "WMT1", 4) && fread(n, 4, 2, f) == 2 && n[0] == METER_CARS &&
      fread(METER_INDEX, sizeof METER_INDEX, 1, f) == 1 && (meter_table = malloc(12 * (size_t)n[1])) &&
      fread(meter_table, 12, n[1], f) == n[1])
    meter_sprites = n[1];
  fclose(f);
}

static unsigned char spr_warm[SPR_COUNT], meter_warm[METERDB_MAX_PARTS], navi_warm[2];

void hud_set_car(int pack, int night) {
  /* the race title page's already (hud_warm) */
  int zone = 2 * meter_style + (night != 0);
  if (meter_car && pack >= 0 && pack < METER_CARS && meter_car == &METERDB[pack] && meter_night == zone) return;
  memset(meter_warm, 0, sizeof meter_warm);
  for (int i = 0; i < METERDB_MAX_PARTS; ++i)
    if (METER[i].tex) glDeleteTextures(1, &METER[i].tex);
  memset(METER, 0, sizeof METER);
  meter_car = pack >= 0 && pack < METER_CARS ? &METERDB[pack] : NULL;
  meter_night = zone;
  FILE *f = meter_car && meter_sprites ? fopen(meter_path, "rb") : NULL;
  if (!f) return;
  for (int i = 0; i < meter_car->n[meter_night]; ++i) {
    unsigned id = METER_INDEX[pack][meter_night][i];
    if (id >= meter_sprites) continue;
    unsigned w = meter_table[3 * id], h = meter_table[3 * id + 1];
    size_t size = (size_t)w * h * 4;
    unsigned char *px = malloc(size);
    if (px && fseek(f, (long)meter_table[3 * id + 2], SEEK_SET) == 0 && fread(px, 1, size, f) == size) {
      METER[i].tex = upload(px, w, h);
      METER[i].w = (float)w;
      METER[i].h = (float)h;
    }
    free(px);
  }
  fclose(f);
}

int hud_load(const char *data_dir) {
  char path[1024];
  meters_index(data_dir);
  snprintf(navi_dir, sizeof navi_dir, "%s", data_dir);
  snprintf(path, sizeof path, "%s/navi.bin", data_dir);
  navi_load(path);
  snprintf(path, sizeof path, "%s/hud.bin", data_dir);
  FILE *f = fopen(path, "rb");
  if (!f) return -1;
  char magic[4];
  unsigned n = 0;
  if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "WHD1", 4) || fread(&n, 4, 1, f) != 1 || n > SPR_COUNT) {
    fclose(f);
    return -1;
  }
  for (unsigned i = 0; i < n; ++i) {
    char name[32];
    unsigned wh[2];
    if (fread(name, 1, 32, f) != 32 || fread(wh, 4, 2, f) != 2 || wh[0] > 1024 || wh[1] > 1024) break;
    size_t size = (size_t)wh[0] * wh[1] * 4;
    unsigned char *px = malloc(size);
    if (!px || fread(px, 1, size, f) != size) { free(px); break; }
    SPR[i].tex = upload(px, wh[0], wh[1]);
    free(px);
    SPR[i].w = (float)wh[0];
    SPR[i].h = (float)wh[1];
  }
  fclose(f);
  return 0;
}

/* Sprite centred on (cx, cy), scaled by k and turned `deg` clockwise. */
static void draw_k(const hud_sprite *sp, float cx, float cy, float deg, float k) {
  if (!sp->tex) return;
  float a = deg * (float)M_PI / 180.0f, c = cosf(a), s = sinf(a);
  float hw = sp->w * k * 0.5f, hh = sp->h * k * 0.5f;
  static const float corner[4][2] = { { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 } };
  float v[16];
  for (int i = 0; i < 4; ++i) {
    float x = corner[i][0] * hw, y = corner[i][1] * hh;
    v[i * 4 + 0] = cx + x * c - y * s;
    v[i * 4 + 1] = cy + x * s + y * c;
    v[i * 4 + 2] = (corner[i][0] + 1) * 0.5f;
    v[i * 4 + 3] = (corner[i][1] + 1) * 0.5f;
  }
  glBindTexture(GL_TEXTURE_2D, sp->tex);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, v);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, v + 2);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

static void sprite_k(int id, float cx, float cy, float deg, float k) { draw_k(&SPR[id], cx, cy, deg, k); }

/* Unrotated sprite by its top-left corner, in HUD_K-scaled points. */
static void blit(int id, float x, float y) {
  sprite_k(id, x + SPR[id].w * HUD_K * 0.5f, y + SPR[id].h * HUD_K * 0.5f, 0, HUD_K);
}

/* The needles' inputs, as yt::CSeq::Get2DInfoMeter hands them to the meters: km/h over
 * the 320 the dials were probed at; CEngine::GetRevRate shaped so idle sits off the
 * stop; CTurbo's boost (bar) from -0.5. No clamp: the arcade's needles run on past the end. */
static float meter_input(const meterdb_part *p, const car_state *car) {
  float r = car->rev_rate;
  switch (p->input) {
  case METER_SPEED: return car_kmh(car) / 320.0f;
  case METER_REV: return r > 0.1f ? 0.96f * r + 0.04f : (5.0f * r * r + 0.05f) * 0.96f + 0.04f;
  case METER_BOOST: return (car->boost + 0.5f) * 0.4f;
  default: return 0;
  }
}

static void draw_meters(const car_state *car) {
  if (!meter_car) return;
  glEnableVertexAttribArray(1);
  int night = meter_night;
  for (int i = 0; i < meter_car->n[night]; ++i) {
    const meterdb_part *p = &meter_car->parts[night][i];
    float deg = p->a0 + p->a1 * meter_input(p, car);
    if (p->add) glBlendFunc(GL_SRC_ALPHA, GL_ONE); /* the R35's glowing night needles */
    draw_k(&METER[i], CLUSTER_X + (p->x - 560) * HUD_K, CLUSTER_Y + (p->y - 228) * HUD_K, deg, HUD_K);
    if (p->add) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  }
  glDisableVertexAttribArray(1);
}

/* The arcade's corner shift indicator (cons_g_mission.anm, clShiftConsole::setMission):
 * the car's 4, 5 or 6-speed H-pattern with the current gear as a numbered ball over its
 * knob, red in MT and blue in AT, and the MT/AT tag beside the bottom row. Knob centres
 * are from the anm, from the pattern's top-left. Reverse has no knob; it shows the blank
 * ball on the neutral bar. */
static const float SHIFT_K = 1.4f;
static float SHIFT_X = 62, SHIFT_Y = 530; /* pattern centre, above ◀ ▶ (hud_wide moves it) */
static const float KNOB[7][2] = { { 28, 33 }, { 17, 10 }, { 9, 61 }, { 34, 10 }, { 25, 61 }, { 50, 10 }, { 42, 61 } };
static const float TAG[2] = { 75, 63 };

static void draw_shifter(const car_state *car) {
  int gears = meter_car ? meter_car->gears : 6;
  gears = gears < 4 ? 4 : gears > 6 ? 6 : gears;
  int pattern = SPR_SHIFT4 + gears - 4;
  if (!SPR[pattern].tex) return;
  glEnableVertexAttribArray(1);
  float ox = SHIFT_X - 30 * SHIFT_K, oy = SHIFT_Y - 36 * SHIFT_K; /* pattern top-left */
  sprite_k(pattern, SHIFT_X, SHIFT_Y, 0, SHIFT_K);
  int g = car->gear < 0 ? 0 : car->gear > gears ? gears : car->gear;
  sprite_k((car->manual ? SPR_BALL_MT : SPR_BALL_AT) + g, ox + KNOB[g][0] * SHIFT_K, oy + KNOB[g][1] * SHIFT_K, 0, SHIFT_K);
  sprite_k(car->manual ? SPR_TAG_MT : SPR_TAG_AT, ox + TAG[0] * SHIFT_K, oy + TAG[1] * SHIFT_K, 0, SHIFT_K);
  glDisableVertexAttribArray(1);
}

/* The player's own plate under the meters (cons_g_speed.anm): class pill, title, name,
 * speed in big digits over their shadows, km/h, power and handling. Offsets are from the
 * plate's top-left at (436,406) on the arcade's 640x480 screen; the plate sits flush with
 * the right edge like the arcade's. A player without a card: N級, 湾岸の新人, GUEST. The
 * power and the handling grade are the car's at its tune (hud_set_tune). */
static float INFO_Y = 505; /* (hud_lift moves it) */
static float PILL_UP; /* the challenger pill over the meters, up with them (hud_lift) */
static float MAP_UP; /* the map, its name and the seconds left, up with the shift gate (hud_lift) */
static int plate_hp = 825, plate_grade = CAR_GRADE_DG, plate_class;

void hud_set_class(int cls) { plate_class = cls < 0 ? 0 : cls > HUD_CLASSES - 1 ? HUD_CLASSES - 1 : cls; }

/* the card's title and name on the plate (clMeterConsole's clSpriteFonts: the title in font
 * 0, from the plate's (71, 3.5); the name in font 1, centred on (119, 32)), drawn from the
 * menus' glyphs: the titles' in font 1 (menu.c TITLE_GLYPH1) taken down to font 0's 15-pixel
 * cells and advances (7 half, 11 full), the name's in font 5 (NAME_GLYPH5) down to font 1's
 * (cell 20, advances 8 and 16). Without them (a menu.bin from before), hud.bin's 湾岸の新人 and
 * GUEST. */
#define PLATE_TITLE_GLYPH 110000
#define PLATE_NAME_GLYPH 106600
static int plate_title = TITLE_FIRST, plate_name_n;
static short plate_name[5];

void hud_set_card(int title, const short *name) {
  plate_title = title > 0 ? title : TITLE_FIRST;
  plate_name_n = 0;
  for (int i = 0; name && i < 5 && name[i] >= 0 && name[i] < NAMEDB_N; ++i) plate_name[plate_name_n++] = name[i];
}

/* glyphs of base (cells of `cell` px, drawn `out` px) from x, y (the HUD's points): their
 * advances half or full (in out's px), each glyph half if its character is ASCII */
static void plate_glyphs(int base, const unsigned short *g, const unsigned char *half, int n, float x, float y,
                         float cell, float out, float adv_half, float adv_full) {
  const float k = HUD_K / 1.6f; /* the HUD's points over the menus' 640x480 */
  for (int i = 0; i < n; ++i) {
    int s = anm_sprite(base + g[i]);
    float src[4] = { 0, 0, cell, cell }, dst[4] = { x / 1.6f, y / 1.6f, x / 1.6f + out * k, y / 1.6f + out * k };
    if (s >= 0) anm_draw_part(s, src, dst, 1);
    x += (half[i] ? adv_half : adv_full) * HUD_K;
  }
}

/* whether the menus' glyphs are in (else hud.bin's own title and name) */
static int plate_text_ok(void) {
  const unsigned short *g;
  return title_glyphs(TITLE_FIRST, &g) > 0 && anm_sprite(PLATE_TITLE_GLYPH + g[0]) >= 0;
}

static void draw_plate_text(void) {
  if (!SPR[SPR_PLATE].tex || !plate_text_ok()) return;
  float k = HUD_K, x0 = 1024 + EDGE - SPR[SPR_PLATE].w * k, y0 = INFO_Y;
  const unsigned short *tg;
  unsigned short g[32];
  unsigned char half[32];
  int n = title_glyphs(plate_title, &tg);
  if (n > 32) n = 32;
  for (int i = 0; i < n; ++i) g[i] = tg[i], half[i] = (unsigned char)title_glyph_half(tg[i]);
  plate_glyphs(PLATE_TITLE_GLYPH, g, half, n, x0 + 71 * k, y0 + 3.5f * k, 20, 15, 7, 11);
  if (plate_name_n) {
    float w = 0;
    for (int i = 0; i < plate_name_n; ++i) {
      g[i] = (unsigned short)plate_name[i];
      half[i] = NAMEDB_WCHAR[plate_name[i]] <= 0x7f;
      w += half[i] ? 8 : 16;
    }
    w += 20 - (half[plate_name_n - 1] ? 8 : 16); /* the last one's cell, not its advance */
    plate_glyphs(PLATE_NAME_GLYPH, g, half, plate_name_n, x0 + (119 - w / 2) * k, y0 + (32 - 10) * k, 32, 20, 8, 16);
  }
}

/* clMeterConsole::setClass: the class's row of the table at 0x93ce860 (cons_g_speed.anm's
 * frame, then the UVs for its templates 0x1115, 0x1109 and 0x1108; 1 blank). Frame 0 (C, B,
 * A and S 9..1) has the letter at x 27 and the digit at 37 from the plate, 級 at 47; frame 1
 * (N, SS 9..1, SSS) 0x1115 at 22, 0x1108 at 32, 0x1109 at 42, 級 at 52. Glyphs here: 'N' 'A'
 * 'B' 'C' 'S', '0'..'9', 0 blank. */
static int class_glyphs(int cls, char g[3]) {
  if (cls == 0) return g[0] = 0, g[1] = 'N', g[2] = 0, 1;
  if (cls <= 36) return g[0] = "CBAS"[(cls - 1) / 9], g[1] = (char)('9' - (cls - 1) % 9), g[2] = 0, 0;
  if (cls <= 45) return g[0] = g[1] = 'S', g[2] = (char)('9' - (cls - 37)), 1;
  return g[0] = g[1] = g[2] = 'S', 1;
}

static void blit_rank(char c, float x, float y) {
  const char *l = strchr("NABCS", c);
  int s = c >= '0' && c <= '9' ? SPR_RANK_DIGIT + c - '0' : l && c ? SPR_RANK + (int)(l - "NABCS") : -1;
  if (s < 0) return;
  if (SPR[s].tex) blit(s, x, y);
  else if (c == 'N') blit(SPR_CLASS_N, x, y); /* a hud.bin from before the classes */
}

void hud_set_tune(int hp, int grade) {
  plate_hp = hp < 0 ? 0 : hp > 999 ? 999 : hp;
  plate_grade = grade < CAR_GRADE_DG ? CAR_GRADE_DG : grade > CAR_GRADE_HG ? CAR_GRADE_HG : grade;
}

static void draw_plate(const car_state *car) {
  if (!SPR[SPR_PLATE].tex) return;
  glEnableVertexAttribArray(1);
  float k = HUD_K, x0 = 1024 + EDGE - SPR[SPR_PLATE].w * k, y0 = INFO_Y;
  blit(SPR_PLATE, x0, y0);
  blit(SPR_PILL, x0 + 20 * k, y0 + 3 * k);
  char g[3];
  int wide = class_glyphs(plate_class, g);
  float gx = wide ? 22 : 27; /* frame 1: 0x1115, 0x1108, 0x1109; frame 0: 0x1115, 0x1109 */
  for (int i = 0; i < (wide ? 3 : 2); ++i) blit_rank(g[i], x0 + (gx + 10 * i) * k, y0 + 2 * k);
  blit(SPR_CLASS_KYU, x0 + (wide ? 52 : 47) * k, y0 + 3 * k);
  if (!plate_text_ok()) { /* (draw_plate_text draws the card's) */
    blit(SPR_TITLE, x0 + 71 * k, y0 + 3.5f * k);
    sprite_k(SPR_NAME, x0 + 119 * k, y0 + 32 * k, 0, k); /* the font draws the name centred */
  } else if (!plate_name_n) {
    sprite_k(SPR_NAME, x0 + 119 * k, y0 + 32 * k, 0, k);
  }
  int kmh = (int)(fminf(car_kmh(car), 999.0f) + 0.5f);
  for (int pass = 0; pass < 2; ++pass) {
    int base = pass ? SPR_BIG : SPR_BIG_SHADOW;
    for (int i = 0, div = 100; i < 3; ++i, div /= 10)
      if (kmh >= div || i == 2) blit(base + kmh / div % 10, x0 + (7 + 19 * i) * k, y0 + 24 * k);
  }
  blit(SPR_KMH, x0 + 72 * k, y0 + 40 * k);
  for (int i = 0, div = 100; i < 3; ++i, div /= 10)
    blit(SPR_SMALL + plate_hp / div % 10, x0 + (110 + 10 * i) * k, y0 + 40 * k);
  blit(SPR_BARIKI, x0 + 140 * k, y0 + 40 * k);
  blit(SPR_GRADE + plate_grade, x0 + 166 * k, y0 + 40 * k);
  glDisableVertexAttribArray(1);
}

/* ---- the time attack's console, the column on the left above the shifter: till goal
 * (cons_g_finish), TOTAL TIME (cons_g_totaltime_ta), SECTION TIME (cons_g_sectiontime),
 * the route map and TIME (cons_g_map_split), TIME EXTEND (cons_g_ta_time_extend).
 * Positions are the anms' on the arcade's 640x480 screen, at HUD_K from (TA_X, TA_Y). ---- */
static float TA_X = 0; /* (- EDGE: hud_wide) */
static const float TA_Y = 26;
static float tx(float ax) { return TA_X + ax * HUD_K; }
static float ty(float ay) { return TA_Y + ay * HUD_K; }
static void ta_blit(int id, float ax, float ay) { blit(id, tx(ax), ty(ay)); }
static void map_blit(int id, float ax, float ay) { blit(id, tx(ax), ty(ay) - MAP_UP); }

static int ta_course;
static float extend_at = -1; /* the race's total when TIME EXTEND popped up, or < 0 */

/* m to the goal (setAnime by its digits): metres up to 999, then km with one decimal,
 * rounded (+50 m) */
static void draw_till_goal(float remain) {
  ta_blit(SPR_TILL_GOAL, 9, 12);
  int d = (int)remain;
  if (d <= 999) {
    int n = d <= 9 ? 1 : d <= 99 ? 2 : 3;
    for (int i = 0, div = n == 3 ? 100 : n == 2 ? 10 : 1; i < n; ++i, div /= 10)
      ta_blit(SPR_GOAL_DIGIT + d / div % 10, 10 + 40 * i, 30);
    ta_blit(SPR_GOAL_M, 10 + 40 * n, 30);
    return;
  }
  int u = (d + 50) / 100; /* 100 m */
  if (u > 999) u = 999;
  if (u <= 99) {
    ta_blit(SPR_GOAL_DIGIT + u / 10, 10, 30);
    ta_blit(SPR_GOAL_DOT, 43, 30);
    ta_blit(SPR_GOAL_DIGIT + u % 10, 70, 30);
    ta_blit(SPR_GOAL_KM, 110, 30);
  } else {
    ta_blit(SPR_GOAL_DIGIT + u / 100, 10, 30);
    ta_blit(SPR_GOAL_DIGIT + u / 10 % 10, 50, 30);
    ta_blit(SPR_GOAL_DOT, 83, 30);
    ta_blit(SPR_GOAL_DIGIT + u % 10, 110, 30);
    ta_blit(SPR_GOAL_KM, 155, 30);
  }
}

/* m'ss"mmm in the 20x20 digits: x of the minute, ', the seconds, " and the ms */
static void draw_laptime(float t, float y, const float *x) {
  int ms = (int)(t * 1000.0f + 0.5f);
  if (ms < 0) ms = 0;
  if (ms > 9 * 60000 + 59999) ms = 9 * 60000 + 59999;
  int m = ms / 60000, sec = ms / 1000 % 60, f = ms % 1000;
  ta_blit(SPR_TA_DIGIT + m, x[0], y);
  ta_blit(SPR_TA_QUOTE1, x[1], y);
  ta_blit(SPR_TA_DIGIT + sec / 10, x[2], y);
  ta_blit(SPR_TA_DIGIT + sec % 10, x[3], y);
  ta_blit(SPR_TA_QUOTE2, x[4], y + 1);
  ta_blit(SPR_TA_DIGIT + f / 100, x[5], y);
  ta_blit(SPR_TA_DIGIT + f / 10 % 10, x[6], y);
  ta_blit(SPR_TA_DIGIT + f % 10, x[7], y);
}

static void draw_times(const car_race *r) {
  static const float TOTAL_X[8] = { 14, 27, 37, 51, 65, 75, 89, 103 };
  static const float SECTION_X[8] = { 14, 26, 36, 49, 62, 72, 85, 98 };
  ta_blit(SPR_TA_PILL, -28, 87);
  draw_laptime(r->total, 100, TOTAL_X);
  ta_blit(SPR_TA_TOTAL, 13, 85);
  ta_blit(SPR_TA_SECTION, 16, 121);
  /* the sections driven, and the one being driven so far */
  int rows = r->sections < 4 ? r->sections : 4;
  for (int k = 0; k < rows && k <= r->section; ++k) {
    float t = k < r->section ? r->split[k + 1] - r->split[k] : r->total - r->split[k];
    draw_laptime(t, 136 + 18 * k, SECTION_X);
  }
}

/* The seconds left: shadows, the label, then the faces; red for the last 9 */
static void draw_time_left(float left) {
  int v = (int)left;
  v = v < 0 ? 0 : v > 999 ? 999 : v;
  static const float X[3] = { 53, 68, 83 };
  for (int i = 0, div = 100; i < 3; ++i, div /= 10) map_blit(SPR_LTIME_SHADOW + v / div % 10, X[i], 364);
  if (v <= 9) tint(1, 0.30f, 0.30f, 1);
  map_blit(SPR_TIME, 15, 366);
  for (int i = 0, div = 100; i < 3; ++i, div /= 10) map_blit(SPR_LTIME + v / div % 10, X[i], 364);
  tint(1, 1, 1, 1);
}

/* TIME EXTEND (cons_g_ta_time_extend) in the middle of the screen: it zooms in (frames
 * 81-90), glows in its loop (1-59) for two rounds, and zooms away (61-69) */
#define EXTEND_IN 10
#define EXTEND_HOLD 120
#define EXTEND_OUT 9
static void draw_time_extend(float total) {
  if (extend_at < 0) return;
  float t = (total - extend_at) * 60.0f;
  if (t < 0 || t >= EXTEND_IN + EXTEND_HOLD + EXTEND_OUT) { extend_at = -1; return; }
  float k = 1, a = 1, glow = 0;
  if (t < EXTEND_IN) {
    float u = t / EXTEND_IN;
    k = u < 0.7f ? 2.0f - 1.14f * u / 0.7f : 0.86f + 0.14f * (u - 0.7f) / 0.3f;
    a = u;
    glow = 1 - u;
  } else if (t < EXTEND_IN + EXTEND_HOLD) {
    glow = 64.0f / 255.0f * (0.5f - 0.5f * cosf((t - EXTEND_IN) * 2.0f * (float)M_PI / 30.0f));
  } else {
    float u = (t - EXTEND_IN - EXTEND_HOLD) / EXTEND_OUT;
    k = 1 + u;
    a = 1 - u;
  }
  tint(1, 1, 1, a);
  tint_add(glow, glow, glow);
  sprite_k(SPR_TIME_EXTEND, 512, 250, 0, HUD_K * k); /* the arcade's (320, 156) */
  tint(1, 1, 1, 1);
  tint_add(0, 0, 0);
}

void hud_time_extend(float total) { extend_at = total; }

/* ---- top right, over the meters: RECORD TIME and YOUR RECORD (cons_g_ta_recordtime and
 * cons_g_ta_yourrecord, clTimeAttackConsoleInfos::setRecord) and the challenger pill
 * (cons_g_buyin of clMeterConsole). Nobody can join a race on the iPad, so the pill says
 * what the arcade's says then, "Refusing challengers" (setNoJoinNo(1)). Flush with the
 * screen's right edge like the arcade's. ---- */
static float rec_time, rec_yours; /* s; 0 = none */

void hud_set_records(float record, float yours) {
  rec_time = record;
  rec_yours = yours;
}

static float rx(float ax) { return 1024 + EDGE - (640 - ax) * HUD_K; }

/* m'ss"mmm in the 20x20 digits at the template's places; a dash in each without a record */
static void draw_record(int label, float label_y, float y, float t) {
  static const float X[8] = { 522, 534, 544, 557, 570, 580, 593, 606 };
  int ms = (int)(t * 1000.0f);
  if (ms > 99 * 60000 + 59999) ms = 99 * 60000 + 59999;
  int d[7] = { ms / 60000 % 10, ms / 10000 % 6, ms / 1000 % 10, ms / 100 % 10, ms / 10 % 10, ms % 10, ms / 600000 };
  static const int SLOT[6] = { 0, 2, 3, 5, 6, 7 };
  for (int i = 0; i < 6; ++i) blit(ms ? SPR_TA_DIGIT + d[i] : SPR_REC_DASH, rx(X[SLOT[i]]), ty(y));
  if (ms && d[6]) blit(SPR_TA_DIGIT + d[6], rx(509), ty(y)); /* ten minutes and more */
  blit(SPR_TA_QUOTE1, rx(X[1]), ty(y));
  blit(SPR_TA_QUOTE2, rx(X[4]), ty(y + 1));
  blit(label, rx(522), ty(label_y));
}

/* ---- the route sign over the car (clSignConsole, cons_g_caution): the sign SAreaData::GetSign
 * gives for the road ahead, at the arcade's (289, 81) 64x64, as big against the screen as
 * there. The red ones and the caution blink in (frames 99..158), the rest stand (0..59). ---- */
static int sign_shown, sign_t;

int hud_sign_blinks(int sign) { return (sign >= 1 && sign <= 4) || sign == 11; }

static void draw_sign(const car_race *r) {
  int sign = r->state == CAR_RACE_RUN && r->sign >= 1 && r->sign <= HUD_SIGNS ? r->sign : 0;
  if (sign != sign_shown) {
    sign_shown = sign;
    sign_t = 0;
  }
  if (!sign) return;
  /* the blink loop's alpha (of 256): each 15 frames it comes back in over 5 or 6 */
  static const unsigned char BLINK[60] = {
    0, 0, 64, 128, 192, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    0, 51, 102, 154, 205, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    0, 51, 102, 154, 205, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    0, 43, 85, 128, 171, 213, 255, 255, 255, 255, 255, 255, 255, 255, 255 };
  float a = hud_sign_blinks(sign) ? BLINK[sign_t % 60] / 255.0f : 1.0f;
  sign_t++;
  const float k = 1.6f; /* the arcade's 640x480 to the 1024x768 screen */
  tint(1, 1, 1, a);
  sprite_k(SPR_SIGN + sign - 1, (289 + 32) * k, (81 + 32) * k, 0, k);
  tint(1, 1, 1, 1);
}

static void draw_records(void) {
  if (!SPR[SPR_BUYIN].tex) return;
  draw_record(SPR_REC_TIME, 15, 29, rec_time);
  draw_record(SPR_REC_YOUR, 55, 69, rec_yours);
  blit(SPR_BUYIN, rx(536), ty(130) - PILL_UP);
}

/* ---- the route map (clMiniMap): data/minimap/SkNavi.igb from above, turned with the car,
 * in a window of the console (packed by tools/navipack.py into navi.bin). Each area's road
 * is there twice: orange (on) for the area the car is in, the one it came from and the one
 * it goes to next, blue grey (off) for the others. The car is car_icon's red arrow. ---- */
#define NAVI_VERT 24 /* f32 pos[3], f32 uv[2], u8 rgba[4] */
typedef struct { short area; unsigned char on, blend; unsigned vert, index, n_index; } navi_mesh;
static struct {
  GLuint prog, tex[2], vbo, ibo;
  GLint u_mvp, u_tex;
  int n;
  navi_mesh *mesh;
  int lit[3], next; /* the areas shown orange, and the next area they were set for */
} NAVI;
static const float NAVI_ICON = 400; /* the arrow's half size (display units) */
static const float NAVI_ICON_UV[4][2] = { { 0.0f, 0.5f }, { 0.34f, 0.5f }, { 0.0f, 1.0f }, { 0.34f, 1.0f } };
#define NAVI_UNSET (-1000)

static int navi_gl_init(void) {
  static const char *vs =
    "attribute vec3 aPos; attribute vec2 aUV; attribute vec4 aColor; uniform mat4 uMVP;\n"
    "varying vec2 vUV; varying vec4 vColor;\n"
    "void main() { vUV = aUV; vColor = aColor; gl_Position = uMVP * vec4(aPos, 1.0); }\n";
  static const char *fs =
    "precision mediump float; uniform sampler2D uTex; varying vec2 vUV; varying vec4 vColor;\n"
    "void main() { gl_FragColor = texture2D(uTex, vUV) * vColor; }\n";
  const char *attribs[] = { "aPos", "aUV", "aColor" };
  NAVI.prog = gl_compile_program(vs, fs, attribs, 3);
  if (!NAVI.prog) return -1;
  NAVI.u_mvp = glGetUniformLocation(NAVI.prog, "uMVP");
  NAVI.u_tex = glGetUniformLocation(NAVI.prog, "uTex");
  return 0;
}

static void navi_load(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) return;
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  unsigned char *d = size > 24 ? malloc((size_t)size) : NULL;
  int ok = d && fread(d, 1, (size_t)size, f) == (size_t)size && !memcmp(d, "WNV1", 4);
  fclose(f);
  unsigned h[5] = { 0 };
  if (ok) memcpy(h, d + 4, sizeof h);
  size_t o = 24 + 4 * ((size_t)h[0] * h[1] + (size_t)h[2] * h[3]);
  if (!ok || o > (size_t)size || !(NAVI.mesh = calloc(h[4], sizeof *NAVI.mesh))) { free(d); return; }
  /* first the sizes, then everything into one vertex and one index buffer */
  size_t verts = 0, indices = 0, p = o;
  int n = 0;
  for (; n < (int)h[4] && p + 12 <= (size_t)size; ++n) {
    navi_mesh *m = &NAVI.mesh[n];
    unsigned nv, ni;
    memcpy(&m->area, d + p, 2);
    m->on = d[p + 2];
    m->blend = d[p + 3];
    memcpy(&nv, d + p + 4, 4);
    memcpy(&ni, d + p + 8, 4);
    p += 12 + (size_t)nv * NAVI_VERT + (size_t)ni * 2;
    if (p > (size_t)size) break;
    m->vert = (unsigned)verts;
    m->index = (unsigned)indices;
    m->n_index = ni;
    verts += nv;
    indices += ni;
  }
  unsigned char *vb = malloc(verts * NAVI_VERT), *ib = malloc(indices * 2);
  if (vb && ib) {
    p = o;
    for (int i = 0; i < n; ++i) {
      unsigned nv, ni;
      memcpy(&nv, d + p + 4, 4);
      memcpy(&ni, d + p + 8, 4);
      memcpy(vb + (size_t)NAVI.mesh[i].vert * NAVI_VERT, d + p + 12, (size_t)nv * NAVI_VERT);
      memcpy(ib + (size_t)NAVI.mesh[i].index * 2, d + p + 12 + (size_t)nv * NAVI_VERT, (size_t)ni * 2);
      p += 12 + (size_t)nv * NAVI_VERT + (size_t)ni * 2;
    }
    glGenBuffers(1, &NAVI.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, NAVI.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts * NAVI_VERT), vb, GL_STATIC_DRAW);
    glGenBuffers(1, &NAVI.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, NAVI.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(indices * 2), ib, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    NAVI.tex[0] = upload(d + 24, h[0], h[1]);
    NAVI.tex[1] = upload(d + 24 + 4 * (size_t)h[0] * h[1], h[2], h[3]);
    NAVI.n = n;
  }
  free(vb);
  free(ib);
  free(d);
}

/* PowerVR leaves a texture's real upload to its first draw: the race's first frame drew every
 * HUD sprite for the first time and took some 100 ms on the iPad. The race title page draws
 * them first instead, n a frame, each into one pixel unseen. */
static int warm_one(GLuint tex, unsigned char *done, int *n) {
  if (!tex || *done) return 0;
  if (*n <= 0) return 1;
  static const float v[16] = { 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 1, 1, 1, 1 };
  glBindTexture(GL_TEXTURE_2D, tex);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, v);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, v + 2);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  *done = 1;
  --*n;
  return 0;
}

int hud_warm(float screen_w, float screen_h, int n) {
  if (!tex_prog) return 0;
  glUseProgram(tex_prog);
  mat4 proj = screen_ortho(screen_w, screen_h);
  glUniformMatrix4fv(u_tproj, 1, GL_FALSE, proj.m);
  glUniform1i(u_tex, 0);
  tint(1, 1, 1, 0);
  tint_add(0, 0, 0);
  glActiveTexture(GL_TEXTURE0);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glDisableVertexAttribArray(2);
  int left = 0;
  for (int i = 0; i < METERDB_MAX_PARTS; ++i) left += warm_one(METER[i].tex, &meter_warm[i], &n);
  for (int i = 0; i < SPR_COUNT; ++i) left += warm_one(SPR[i].tex, &spr_warm[i], &n);
  for (int i = 0; i < 2; ++i) left += warm_one(NAVI.tex[i], &navi_warm[i], &n);
  glDisableVertexAttribArray(1);
  return left;
}

void hud_set_course(int course) {
  ta_course = course;
  extend_at = -1;
  NAVI.next = NAVI_UNSET;
}

/* Hakone's, Nagoya's and Fukuoka's areas each share one road (the minimap's mesh names,
 * kept under the city's first area by navipack.py) */
static int navi_area(int area) {
  return area >= 58 && area <= 61 ? 58 : area >= 62 && area <= 69 ? 62 : area >= 70 && area <= 73 ? 70 : area;
}

static int navi_lit(int area) {
  return area == navi_area(NAVI.lit[0]) || area == navi_area(NAVI.lit[1]) || area == navi_area(NAVI.lit[2]);
}

void hud_set_city(int city) {
  static const char *const FILE_NAME[CAR_CITIES] = { "navi.bin", "navi_oosaka.bin", "navi_hakone.bin", "navi_nagoya.bin",
                                                     "navi_fukuoka.bin" };
  static int now; /* hud_load's Tokyo */
  if (city < 0 || city >= CAR_CITIES || city == now) return;
  now = city;
  if (NAVI.vbo) glDeleteBuffers(1, &NAVI.vbo);
  if (NAVI.ibo) glDeleteBuffers(1, &NAVI.ibo);
  glDeleteTextures(2, NAVI.tex);
  NAVI.vbo = NAVI.ibo = NAVI.tex[0] = NAVI.tex[1] = 0;
  free(NAVI.mesh);
  NAVI.mesh = NULL;
  NAVI.n = 0;
  navi_warm[0] = navi_warm[1] = 0;
  char path[1100];
  snprintf(path, sizeof path, "%s/%s", navi_dir, FILE_NAME[city]);
  navi_load(path);
}

/* The map in its window at (17, 113, 88, 82) of the arcade's GL viewport (y up), looking
 * down from 2000 over the car with its heading up, ortho -2000..2000 x -1200..2800. */
static void draw_navi(const hud_state *h, const car_race *r) {
  if (!NAVI.n) return;
  if (NAVI.next != r->next_area) { /* clMiniMap::update */
    NAVI.lit[0] = NAVI.next == NAVI_UNSET ? NAVI_UNSET : NAVI.lit[1];
    NAVI.lit[1] = r->area;
    NAVI.lit[2] = r->next_area;
    NAVI.next = r->next_area;
  }
  GLint vp[4];
  glGetIntegerv(GL_VIEWPORT, vp);
  float sx = (float)vp[2] / (h->screen_w + 2 * EDGE), sy = (float)vp[3] / h->screen_h;
  float x0 = tx(17), y0 = ty(285) - MAP_UP, w = 88 * HUD_K, hh = 82 * HUD_K;
  GLint wx = vp[0] + (GLint)((x0 + EDGE) * sx + 0.5f), wy = vp[1] + (GLint)((h->screen_h - y0 - hh) * sy + 0.5f);
  GLsizei ww = (GLsizei)(w * sx + 0.5f), wh = (GLsizei)(hh * sy + 0.5f);
  glViewport(wx, wy, ww, wh);
  glEnable(GL_SCISSOR_TEST);
  glScissor(wx, wy, ww, wh);
  glDepthMask(GL_TRUE);
  glClear(GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

  /* the car's heading: Get2DInfoMap's angle is its yaw turned half round */
  float fx = -sinf(r->map_angle), fz = -cosf(r->map_angle);
  const float eye[3] = { r->map_x, 2000, r->map_z }, at[3] = { r->map_x, 0, r->map_z }, up[3] = { fx, 0, fz };
  mat4 mvp = mat4_mul(mat4_ortho(-2000, 2000, -1200, 2800, 5, 10000), mat4_look_at(eye, at, up));
  glUseProgram(NAVI.prog);
  glUniformMatrix4fv(NAVI.u_mvp, 1, GL_FALSE, mvp.m);
  glUniform1i(NAVI.u_tex, 0);
  glBindTexture(GL_TEXTURE_2D, NAVI.tex[0]);
  glBindBuffer(GL_ARRAY_BUFFER, NAVI.vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, NAVI.ibo);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
  for (int pass = 0; pass < 2; ++pass) { /* the solid roads, then the blended ones */
    glDepthMask(pass ? GL_FALSE : GL_TRUE);
    for (int i = 0; i < NAVI.n; ++i) {
      const navi_mesh *m = &NAVI.mesh[i];
      if (m->blend != pass || (m->area >= 0 && navi_lit(m->area) != m->on)) continue;
      const char *base = (const char *)((size_t)m->vert * NAVI_VERT);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, NAVI_VERT, base);
      glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, NAVI_VERT, base + 12);
      glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, NAVI_VERT, base + 20);
      glDrawElements(GL_TRIANGLES, (GLsizei)m->n_index, GL_UNSIGNED_SHORT, (const void *)((size_t)m->index * 2));
    }
  }
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glDisableVertexAttribArray(2);
  glVertexAttrib4f(2, 1, 1, 1, 1);

  /* the arrow over the roads, its tip (+z) along the heading */
  float v[4][5];
  for (int i = 0; i < 4; ++i) {
    float lx = i & 1 ? -NAVI_ICON : NAVI_ICON, lz = i & 2 ? NAVI_ICON : -NAVI_ICON;
    v[i][0] = r->map_x + lx * fz + lz * fx;
    v[i][1] = 1900;
    v[i][2] = r->map_z - lx * fx + lz * fz;
    v[i][3] = NAVI_ICON_UV[i][0];
    v[i][4] = NAVI_ICON_UV[i][1];
  }
  glBindTexture(GL_TEXTURE_2D, NAVI.tex[1]);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 20, v[0]);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 20, &v[0][3]);
  glDisable(GL_DEPTH_TEST);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  glDepthMask(GL_TRUE);
  glDisable(GL_SCISSOR_TEST);
  glViewport(vp[0], vp[1], vp[2], vp[3]);
}

/* a story race (hud_set_story): no time attack records or course name */
static int ta_story;
/* each rival mark's frames: cons_g_R's shown on the screen, the loops' off it and the leader's
 * place in its cons_g_R_tg (draw_rival_marks) */
static struct { int pop, loop[3]; float phase; } mark_state[4];

static int story_chara[3], story_n, story_sel = -1;
static unsigned char story_shown[3];

void hud_set_story_rivals(const int *chara, const unsigned char *shown, int n, int sel) {
  story_sel = sel;
  story_n = n < 0 ? 0 : n > 3 ? 3 : n;
  for (int i = 0; i < 3; ++i) {
    story_chara[i] = i < story_n ? chara[i] : 0;
    story_shown[i] = i < story_n ? shown[i] : 0;
  }
}

void hud_set_story(int story) {
  ta_story = story != 0;
  memset(mark_state, 0, sizeof mark_state);
}

/* ---- the 10-in-a-row's console, clTenConsole (ctor 0x85c96f8, update 0x85c9154, draw
 * 0x85c9018): the standard console's map and time, and in place of the time attack's column
 * - cons_g_or_men (key 0x40) at the top: LV and the level (templates 0x143c, 0x143b: one
 *   digit in the first, 10 as "1" "0", the extend's "EX"), and the rival on the road counted
 *   from 1 in the big place digits (0x1430 the ones, 0x142f the tens, UV 0x93cf220) with
 *   人目 after them, on frame 4 below 10 and frame 9 from 10;
 * - cons_g_splittime (clSectionTimeConsole, mode 7): SPLIT TIME and the time each rival took,
 *   the one on the road's running, the last four rows (setTime, digits UV 0x93cf140);
 * - TIME EXTEND as each rival is beaten (clTimeExtendConsole::set).
 * The round shown changes 6 frames after CTenSeq's (the update's counter at +0xa88). Drawn
 * at the time attack column's scale. Images from the UV table: menupack.py UV_BASE + n. */
#define UV_BASE 102000
enum { T_TTIME0 = 0x143b, T_TTIME1, T_TTIME2, T_TTIME3, T_TTIME4, T_TTIME5, T_PLACE3 = 0x142f, T_PLACE4 = 0x1430,
       T_OR_MEN = 0xcc, T_OR_LV = 0x39, UV_BLANK = 1, UV_TIME0 = 22, UV_PLACE0 = 140, UV_MEN = 166, UV_LV = 167,
       UV_EX = 274 };
static int ten_level = -1, ten_round = -1, ten_round_wait, ten_disp;
static int banner_rival, banner_frame, banner_on, banner_round, banner_chara;

static void cut_reset(void);

void hud_set_ten(int level) {
  ten_level = level;
  ten_round = -1;
  ten_round_wait = 0;
  ten_disp = 0;
  banner_rival = -1;
  cut_reset();
}

static int uv(int n) { return n == UV_BLANK ? -1 : anm_sprite(UV_BASE + n); }

/* setTime's digits: m ' ss " mmm, capped at 9'59"999 */
static int ten_time_swaps(float t, anm_swap *sw) {
  int ms = (int)(t * 1000.0f);
  if (ms < 0) ms = 0;
  if (ms > 599999) ms = 599999;
  int d[6] = { ms / 60000, ms / 10000 % 6, ms / 1000 % 10, ms / 100 % 10, ms / 10 % 10, ms % 10 };
  for (int i = 0; i < 6; ++i) sw[i] = (anm_swap){ anm_sprite(T_TTIME0 + i), uv(UV_TIME0 + d[i]) };
  sw[6] = (anm_swap){ anm_sprite(0xc14), uv(32) }; /* ' and " as the atlas has them */
  sw[7] = (anm_swap){ anm_sprite(0xc13), uv(33) };
  return 8;
}

/* ---- the rival's box (clRivalInfoConsole, cons_g_cpu at the arcade's (584, 76)): its face,
 * name and car name (the 10-in-a-row's from getTenChara*Image: the character of the round,
 * TEN_CHARA[level][round], or the round itself in the extend mode; the table at 0x9333660),
 * the frames' templates blanked (UV 183) and the advantage: |m| in the digits left aligned
 * ("%d     "), frames 5..9 ahead (cyan) by its 1..5 digits, 4..0 behind (red, a minus).
 * Drawn right aligned at the meters' scale. */
static const unsigned char TEN_CHARA[10][10] = {
  { 0, 1, 2, 3, 4, 5, 6, 10, 8, 30 },       { 1, 3, 4, 10, 9, 20, 21, 28, 25, 31 },
  { 4, 22, 8, 10, 11, 20, 21, 23, 29, 31 }, { 22, 5, 4, 6, 7, 26, 21, 27, 30, 31 },
  { 16, 17, 18, 19, 15, 7, 5, 12, 13, 9 },  { 3, 7, 20, 8, 24, 29, 14, 27, 15, 31 },
  { 8, 14, 27, 16, 15, 7, 12, 5, 13, 9 },   { 14, 17, 18, 19, 13, 15, 23, 24, 27, 29 },
  { 10, 20, 22, 23, 24, 28, 25, 26, 27, 30 }, { 22, 21, 23, 28, 25, 26, 27, 29, 30, 31 } };
static const unsigned short TEN_FACE[32][3] = { /* +8 face, +0xc name, +0x10 car */
  { 3052, 4353, 7446 }, { 2615, 3457, 7627 }, { 3428, 5164, 7790 }, { 4298, 5985, 7446 },
  { 2616, 3458, 7628 }, { 3424, 5162, 7801 }, { 4309, 5989, 7440 }, { 6388, 6949, 7447 },
  { 4320, 5992, 7444 }, { 3066, 4358, 7628 }, { 3069, 4359, 7898 }, { 3063, 4356, 7901 },
  { 5112, 6425, 7627 }, { 5961, 6710, 7791 }, { 5962, 6711, 7898 }, { 3426, 4355, 7791 },
  { 3065, 4357, 7898 }, { 5125, 6426, 7898 }, { 4325, 5993, 7898 }, { 4315, 5991, 7898 },
  { 4309, 5989, 7447 }, { 2612, 3456, 7800 }, { 5120, 5983, 7628 }, { 4327, 5994, 7790 },
  { 5954, 6709, 7790 }, { 5115, 5990, 7628 }, { 3061, 4354, 7900 }, { 4308, 5988, 7898 },
  { 4304, 5986, 7627 }, { 3432, 5164, 7790 }, { 1968, 3077, 7179 }, { 3049, 4353, 7906 } };
enum { T_CPU_WAKU = 0xa3f, T_CPU_CHARA_WAKU = 0x1a27, T_CPU_ADVANTAGE = 0x1c0a, T_CPU_FACE = 0x1f54,
       T_CPU_NAME = 0x1b19, T_CPU_CAR = 0x1d1a, T_CPU_DIGIT0 = 0x1d02, UV_CPU_EMPTY = 183,
       UV_CPU_ADVANTAGE = 181, UV_CPU_DIGIT0 = 168 };

/* one box: cons_g_cpu at the arcade's (x, 76), its pictures, the advantage (m, > 0 ahead) */
static void draw_info_box(int cpu, float x, int face, int name, int car_img, float gap) {
  int adv = (int)(gap * 1000.0f), m = abs(adv / 1000), neg = adv < 0;
  if (m > 99999) m = 99999;
  int digits = m > 9999 ? 5 : m > 999 ? 4 : m > 99 ? 3 : m > 9 ? 2 : 1;
  anm_swap sw[14];
  int n = 0;
  sw[n++] = (anm_swap){ anm_sprite(T_CPU_WAKU), uv(UV_CPU_EMPTY) };
  sw[n++] = (anm_swap){ anm_sprite(T_CPU_CHARA_WAKU), uv(UV_CPU_EMPTY) };
  sw[n++] = (anm_swap){ anm_sprite(T_CPU_ADVANTAGE), uv(UV_CPU_ADVANTAGE) };
  sw[n++] = (anm_swap){ anm_sprite(T_CPU_FACE), anm_sprite(face) };
  sw[n++] = (anm_swap){ anm_sprite(T_CPU_NAME), anm_sprite(name) };
  sw[n++] = (anm_swap){ anm_sprite(T_CPU_CAR), anm_sprite(car_img) };
  char text[16];
  snprintf(text, sizeof text, "%d     ", m);
  for (int i = 0; i < 5; ++i)
    sw[n++] = (anm_swap){ anm_sprite(T_CPU_DIGIT0 + i), text[i] == ' ' ? -1 : uv(UV_CPU_DIGIT0 + text[i] - '0') };
  const float zoom = HUD_K / 1.6f;
  anm_draw_xf(cpu, neg ? 5 - digits : 4 + digits, 1, (1024 + EDGE - (640 - x) * HUD_K) / 1.6f, (TA_Y + 76 * HUD_K) / 1.6f,
              zoom, sw, n);
}

static void draw_rival_info(const car_state *car, int round) {
  static int cpu = -2;
  if (cpu == -2) cpu = anm_find("cons_g_cpu");
  if (cpu < 0) return;
  int extend = ten_level >= 10, lv = ten_level > 9 ? 9 : ten_level;
  int chara = extend ? (round > 31 ? 31 : round) : TEN_CHARA[lv][round > 9 ? 9 : round];
  /* setAdvantage: getInfoTenAdvantage (mm) / 1000 */
  draw_info_box(cpu, 584, TEN_FACE[chara][0], TEN_FACE[chara][1], TEN_FACE[chara][2], car->race.ten_gap);
}

/* The story race's boxes (clStoryConsole::update, src/storyinfo.h): box k for each rival i in
 * place order that is shown and on (getInfoStoryCharacterEnable), its pictures
 * getStCharData(chara)'s (the first entry when not there), x by which boxes are wide */
static void draw_story_info(const car_state *car) {
  static int cpu = -2;
  if (cpu == -2) cpu = anm_find("cons_g_cpu");
  if (cpu < 0) return;
  const storyinfo_char *box[3];
  float gap[3];
  int n = 0, wide = 0;
  for (int i = 0; i < story_n; ++i) {
    if (!story_shown[i] || !car->race.story_on[i]) continue;
    const storyinfo_char *c = &STORYINFO_CHAR[0];
    for (int k = 0; k < STORYINFO_NCHAR; ++k)
      if (STORYINFO_CHAR[k].chara == story_chara[i]) {
        c = &STORYINFO_CHAR[k];
        break;
      }
    if (c->wide == 1) wide |= 4 >> n;
    gap[n] = car->race.story_gap[i];
    box[n++] = c;
  }
  for (int k = 0; k < n; ++k) draw_info_box(cpu, STORYINFO_X[wide][k], box[k]->face, box[k]->name, box[k]->car, gap[k]);
}

/* ---- the versus' console (clJoinConsole: ctor 0x85c04d0, update 0x85c010c, draw 0x85c02f0),
 * drawn in play states 1 and 2 (while Disp2D) in place of the time attack's column:
 * - cons_g_place (clJoinConsoleInfos +0x82c, at its own place: the screen's top left): the
 *   place (getInfoJoinRank: S2DInfoBattle +0, 0 first) in 0x1430 as UV 0x8c + it, "/n" in
 *   0x176e (UV 0x96 + the machines - 2), and the distance left (setRemainDist, its m: over
 *   999 m +50 and in 100 m) by its frame: 4, 3, 2 one to three digits of m, 1 x.x km, 0 xx.x
 *   km, the digits UV 0x99 + d in 0x191d (the first), 0x191c, 0x191b; the dot (0xc06) UV
 *   0xa3, m (0x55b) 0xa4, km (0x699) 0xa5;
 * - clRivalInfoConsole1: a box per other machine i (the join list's order, this one left out)
 *   at (575, 463, 361)[i], 60 (0x93cf120, 0x93cf114): cons_g_rival, or cons_g_rival_ph with
 *   an R35 in the race (changeToStepDisp). The card's name (clEffNameType::getStrip, font 1
 *   on template 0x37c) and title (font 0 on 0x56d), datrums 9: centred; the car's name
 *   (getCarLogoSprite(model, 0): get_car_model_data +0x2c) in 0xc12; the horsepower
 *   (clCarStyle::getHorsePower) in 0x1735 (hundreds, blank under 100), 0x1734, 0x1733 as UV
 *   0x4d + d and the grade (getHandling: DG, D, B, G, HG UV 0x59, 0x58, 0x57, 0x5a, 0x5b, the
 *   table at 0x93cf12c) in 0x173e, or the steps' power (getTunePower) in 0x1735 / 0x1734 and
 *   handling in 0x1733 / 0x1736 (tens blank under 10); ADVANTAGE 0x1c0a UV 0xb5, the minus
 *   0x1770 0xb3, the comma 0x176f 0xb2, m 0xc11 0xb4, HP/ 0xbe7 0x4c. setAdvantage
 *   (getInfoJoinAdvantage, mm, > 0 the player ahead) as the cpu box's: |m| as "%d     " in
 *   0x1d02.. (UV 0x78 + the character), frames 5..9 ahead by its digits, 4..0 behind.
 * The font glyphs: the plate's (draw_plate_text). ---- */
enum { T_PLACE_SLASH = 0x176e, T_PLACE_D0 = 0x191b, T_PLACE_DOT = 0xc06, T_PLACE_M = 0x55b, T_PLACE_KM = 0x699,
       UV_PLACE_SLASH = 0x96, UV_PLACE_D0 = 0x99, UV_PLACE_DOT = 0xa3, UV_PLACE_M = 0xa4, UV_PLACE_KM = 0xa5,
       T_VS_NAME = 0x37c, T_VS_TITLE = 0x56d, T_VS_CAR = 0xc12, T_VS_MINUS = 0x1770, T_VS_COMMA = 0x176f,
       T_VS_M = 0xc11, T_VS_HP = 0xbe7, T_VS_D0 = 0x1733, T_VS_GRADE = 0x173e, T_VS_D3 = 0x1736, UV_VS_ADVANTAGE = 0xb5,
       UV_VS_MINUS = 0xb3, UV_VS_COMMA = 0xb2, UV_VS_M = 0xb4, UV_VS_HP = 0x4c, UV_VS_D0 = 0x4d };
static const short VS_BOX_X[CAR_RIVALS] = { 575, 463, 361 };
static const unsigned char VS_GRADE_UV[5] = { 0x59, 0x58, 0x57, 0x5a, 0x5b };
static hud_vs_box vs_box[CAR_RIVALS];
static int vs_n;

void hud_set_vs(const hud_vs_box *box, int n) {
  vs_n = n < 0 ? 0 : n > CAR_RIVALS ? CAR_RIVALS : n;
  for (int i = 0; i < vs_n; ++i) vs_box[i] = box[i];
}

static void draw_vs_place(const car_race *r) {
  static int anim = -2;
  if (anim == -2) anim = anm_find("cons_g_place");
  if (anim < 0) return;
  int rank = r->vs_rank < 0 ? 0 : r->vs_rank > 9 ? 9 : r->vs_rank, d = (int)r->remain, frame, dig[3] = { -1, -1, -1 };
  if (d < 0) d = 0;
  if (d > 999) d += 50;
  if (d <= 9) frame = 4, dig[0] = d;
  else if (d <= 99) frame = 3, dig[0] = d / 10 % 10, dig[1] = d % 10;
  else if (d <= 999) frame = 2, dig[0] = d / 100 % 10, dig[1] = d / 10 % 10, dig[2] = d % 10;
  else if (d <= 9999) frame = 1, dig[0] = d / 1000 % 10, dig[1] = d / 100 % 10;
  else if (d <= 99999) frame = 0, dig[0] = d / 10000 % 10, dig[1] = d / 1000 % 10, dig[2] = d / 100 % 10;
  else return;
  anm_swap sw[8];
  int n = 0;
  sw[n++] = (anm_swap){ anm_sprite(T_PLACE4), uv(UV_PLACE0 + rank) };
  sw[n++] = (anm_swap){ anm_sprite(T_PLACE_SLASH), uv(UV_PLACE_SLASH + (vs_n + 1 < 2 ? 0 : vs_n - 1)) };
  for (int i = 0; i < 3; ++i) sw[n++] = (anm_swap){ anm_sprite(T_PLACE_D0 + 2 - i), dig[i] < 0 ? -1 : uv(UV_PLACE_D0 + dig[i]) };
  sw[n++] = (anm_swap){ anm_sprite(T_PLACE_DOT), uv(UV_PLACE_DOT) };
  sw[n++] = (anm_swap){ anm_sprite(T_PLACE_M), uv(UV_PLACE_M) };
  sw[n++] = (anm_swap){ anm_sprite(T_PLACE_KM), uv(UV_PLACE_KM) };
  anm_draw_xf(anim, frame, 1, TA_X / 1.6f, TA_Y / 1.6f, HUD_K / 1.6f, sw, n);
}

/* a font's glyphs centred (datrums 9) on a template of a frame drawn at (dx, dy) * zoom: cells
 * of `cell` px drawn `out` px, advances half or full */
static void vs_text(int anim, int frame, int tmpl, int base, const unsigned short *g, const unsigned char *half, int n,
                    float cell, float out, float adv_half, float adv_full, float dx, float dy, float zoom) {
  float r[4], w = 0;
  if (n <= 0 || !anm_sprite_rect(anim, frame, anm_sprite(tmpl), r)) return;
  for (int i = 0; i < n; ++i) w += half[i] ? adv_half : adv_full;
  float x = r[0] + (adv_full - w) / 2.0f;
  for (int i = 0; i < n; ++i) {
    int s = anm_sprite(base + g[i]);
    float src[4] = { 0, 0, cell, cell };
    float dst[4] = { dx + zoom * x, dy + zoom * r[1], dx + zoom * (x + out), dy + zoom * (r[1] + out) };
    if (s >= 0) anm_draw_part(s, src, dst, 1);
    x += half[i] ? adv_half : adv_full;
  }
}

static void draw_vs_box(int i, float gap) {
  static int anim[2] = { -2, -2 };
  if (anim[0] == -2) {
    anim[0] = anm_find("cons_g_rival");
    anim[1] = anm_find("cons_g_rival_ph");
  }
  const hud_vs_box *b = &vs_box[i];
  int a = anim[b->steps != 0];
  if (a < 0) return;
  int adv = (int)(gap * 1000.0f), m = abs(adv / 1000), neg = adv < 0;
  if (m > 99999) m = 99999;
  int digits = m > 9999 ? 5 : m > 999 ? 4 : m > 99 ? 3 : m > 9 ? 2 : 1;
  anm_swap sw[24];
  int n = 0;
  sw[n++] = (anm_swap){ anm_sprite(T_CPU_ADVANTAGE), uv(UV_VS_ADVANTAGE) };
  sw[n++] = (anm_swap){ anm_sprite(T_VS_MINUS), uv(UV_VS_MINUS) };
  sw[n++] = (anm_swap){ anm_sprite(T_VS_COMMA), uv(UV_VS_COMMA) };
  sw[n++] = (anm_swap){ anm_sprite(T_VS_M), uv(UV_VS_M) };
  sw[n++] = (anm_swap){ anm_sprite(T_VS_HP), uv(UV_VS_HP) };
  sw[n++] = (anm_swap){ anm_sprite(T_VS_NAME), -1 };
  sw[n++] = (anm_swap){ anm_sprite(T_VS_TITLE), -1 };
  sw[n++] = (anm_swap){ anm_sprite(T_VS_CAR), anm_sprite(b->plate) };
  if (b->steps) {
    int p = b->power < 0 ? 0 : b->power > 99 ? 99 : b->power, h = b->handling < 0 ? 0 : b->handling > 99 ? 99 : b->handling;
    sw[n++] = (anm_swap){ anm_sprite(T_VS_D0 + 2), p <= 9 ? -1 : uv(UV_VS_D0 + p / 10) };
    sw[n++] = (anm_swap){ anm_sprite(T_VS_D0 + 1), uv(UV_VS_D0 + p % 10) };
    sw[n++] = (anm_swap){ anm_sprite(T_VS_D0), h <= 9 ? -1 : uv(UV_VS_D0 + h / 10) };
    sw[n++] = (anm_swap){ anm_sprite(T_VS_D3), uv(UV_VS_D0 + h % 10) };
  } else {
    int hp = b->hp < 0 ? 0 : b->hp > 999 ? 999 : b->hp, g = b->grade < 0 ? 0 : b->grade > 4 ? 4 : b->grade;
    sw[n++] = (anm_swap){ anm_sprite(T_VS_D0 + 2), hp <= 99 ? -1 : uv(UV_VS_D0 + hp / 100) };
    sw[n++] = (anm_swap){ anm_sprite(T_VS_D0 + 1), uv(UV_VS_D0 + hp / 10 % 10) };
    sw[n++] = (anm_swap){ anm_sprite(T_VS_D0), uv(UV_VS_D0 + hp % 10) };
    sw[n++] = (anm_swap){ anm_sprite(T_VS_GRADE), uv(VS_GRADE_UV[g]) };
  }
  char text[16];
  snprintf(text, sizeof text, "%d     ", m);
  for (int k = 0; k < 5; ++k)
    sw[n++] = (anm_swap){ anm_sprite(T_CPU_DIGIT0 + k), text[k] == ' ' ? -1 : uv(UV_CPU_DIGIT0 + text[k] - '0') };
  int frame = neg ? 5 - digits : 4 + digits;
  const float zoom = HUD_K / 1.6f, dx = (1024 + EDGE - (640 - VS_BOX_X[i]) * HUD_K) / 1.6f, dy = (TA_Y + 60 * HUD_K) / 1.6f;
  anm_draw_xf(a, frame, 1, dx, dy, zoom, sw, n);
  /* the name (font 1: the menus' font 5 glyphs down to its 20-pixel cells, advances 8 and 16)
   * and the title (font 0: font 1's down to 15, advances 7 and 11), as the plate has them */
  unsigned short g[32];
  unsigned char half[32];
  int len = 0;
  for (int k = 0; k < 5 && b->name[k] >= 0 && b->name[k] < NAMEDB_N; ++k) {
    g[len] = (unsigned short)b->name[k];
    half[len++] = NAMEDB_WCHAR[b->name[k]] <= 0x7f;
  }
  vs_text(a, frame, T_VS_NAME, PLATE_NAME_GLYPH, g, half, len, 32, 20, 8, 16, dx, dy, zoom);
  const unsigned short *tg;
  len = title_glyphs(b->title > 0 ? b->title : TITLE_FIRST, &tg);
  if (len > 32) len = 32;
  for (int k = 0; k < len; ++k) g[k] = tg[k], half[k] = (unsigned char)title_glyph_half(tg[k]);
  vs_text(a, frame, T_VS_TITLE, PLATE_TITLE_GLYPH, g, half, len, 20, 15, 7, 11, dx, dy, zoom);
}

static void draw_vs(const car_state *car) {
  const car_race *r = &car->race;
  if (!vs_n || (r->play_state != 1 && r->play_state != 2)) return;
  draw_vs_place(r);
  for (int i = 0; i < vs_n; ++i)
    if (r->vs_on[i]) draw_vs_box(i, r->vs_gap[i]);
}

/* ---- the rivals' marks (clRivalMarkConsole: ctor 0x85cc9f8, update 0x85cdc90, draw 0x85ce998;
 * clRivalMark: ctor 0x85ef8a0, update 0x85f0650, draw 0x85f0ea0), one over each rival's car.
 * clRaceThread::setRivalPositionData feeds them: the story's rivals 1..n (mark k = rival k, its
 * advantage getInfoStoryAdvantage(k - 1)), the 10-in-a-row's rival 1, a versus' other machines
 * (setRivalCarDispId: the mark of the machine's PCB id, getInfoJoinAdvantage) whose cars show.
 * The advantage: mm, the player ahead > 0. update: the car's place (GetInfoRivalCarDisp) 14 up
 * (setPosOffset), through the camera; band = |advantage| per 20 m (0..2).
 * - The story's and the 10-in-a-row's: cons_g_R with the R by mark (0x93d85cc: cons_p_R,
 *   cons_p_R_2, cons_p_R_3 for template 1674). On the screen (x, y and z within -1..1): frames
 *   0..60 once, then the R, at the point, at 100 times the width half a unit either side of it
 *   makes (at most 1.8), drawn while the advantage is -200 m..3 m; off it (behind the camera:
 *   the point mirrored) looping 30, 50 or 120 frames by band (0x93d85ac) at x within the R's
 *   half width of the edges, y 450, scaled (2.5 - |adv| m / 150) * 0.4, drawn while the player
 *   is 6 m..150 m ahead.
 * - A story's irregular rival (checkSrotyIrregularRival: the select's partner, 0x93ddd80) and
 *   a versus' machines: cons_g_R_tg, 120 frames per mark (0x93d859c: from 120 by 120), its R and
 *   crown (1674, 1671) left out (setImageReplace 1; setCrown's ranking crowns not here): the
 *   yellow arrow and, in a versus, the machine's name (setRivalname: font 4, datrums 9, on
 *   templates 0x49 and 0x59f, in the frames' colour by mark). On the screen: the mark's frames
 *   60 on (0x93d85bc, the arrow still) 20 (story) or 15 (versus) times the scale below the
 *   point; in a versus the machine furthest ahead of the player (the console's sort: the least
 *   advantage) runs through all 120 at 10/3, 20/9 or 5/3 a frame by band (0x9770be0). Off it:
 *   the band's loop from the mark's first frame, at y 470. The console draws the furthest
 *   (|advantage|) first. ---- */
static const float MARK_LOOP[3] = { 30, 50, 120 };                          /* 0x93d85ac */
static const int MARK_R[4] = { 43, 43, 201, 202 };                          /* 0x93d85cc */
static const int MARK_TG[4] = { 120, 240, 360, 480 };                       /* 0x93d859c */
static const float MARK_SPEED[3] = { 10.0f / 3.0f, 20.0f / 9.0f, 5.0f / 3.0f }; /* 0x9770be0 */
static const unsigned char STORY_IRREGULAR[11][2] = {                       /* 0x93ddd80 */
  { 0, 2 }, { 30, 2 }, { 46, 2 }, { 70, 2 }, { 72, 1 }, { 80, 2 }, { 81, 2 }, { 85, 2 }, { 90, 2 }, { 91, 2 }, { 94, 2 }
};
enum { T_R_TEMPLATE = 1674, T_R_CROWN = 0x687, T_R_FONT = 0x49, T_R_FONT2 = 0x59f,
       R_HALF_W = 32 /* cons_p_R 64 wide (getSize(0x2b) * 0.5) */ };
static float mark_view[16], mark_proj[16];
static int mark_on;

void hud_rival_view(const float view[16], const float proj[16]) {
  memcpy(mark_view, view, sizeof mark_view);
  memcpy(mark_proj, proj, sizeof mark_proj);
  mark_on = 1;
}

static void xform(const float *m, const float v[4], float out[4]) {
  for (int r = 0; r < 4; ++r) out[r] = m[r] * v[0] + m[4 + r] * v[1] + m[8 + r] * v[2] + m[12 + r] * v[3];
}

/* a versus mark's name, as the fonts on its templates draw it: font 4's 32-pixel cells
 * (advances 28 and 14) at each template's scale and colour, centred on it */
static void mark_name(int anim, int frame, const short *name, float dx, float dy, float zoom) {
  unsigned short g[5];
  unsigned char half[5];
  int len = 0;
  for (int k = 0; k < 5 && name[k] >= 0 && name[k] < NAMEDB_N; ++k) {
    g[len] = (unsigned short)name[k];
    half[len++] = NAMEDB_WCHAR[name[k]] <= 0x7f;
  }
  static const int T[2] = { T_R_FONT, T_R_FONT2 };
  for (int t = 0; t < 2 && len; ++t) {
    float xf[6], mul[4], add[4];
    if (!anm_sprite_xf(anim, frame, anm_sprite(T[t]), xf, mul, add)) continue;
    float s = xf[2], w = 0;
    for (int i = 0; i < len; ++i) w += half[i] ? 14 : 28;
    float x = xf[0] + s * (28 - w) / 2.0f, y = xf[1];
    for (int i = 0; i < len; ++i) {
      static const float src[4] = { 0, 0, 32, 32 };
      float dst[4] = { dx + zoom * x, dy + zoom * y, dx + zoom * (x + 32 * s), dy + zoom * (y + 32 * s) };
      anm_draw_part_col(anm_sprite(PLATE_NAME_GLYPH + g[i]), src, dst, mul, add);
      x += s * (half[i] ? 14 : 28);
    }
  }
}

typedef struct { int mark, slot, tg; float adv; const short *name; } rival_mark;

static void draw_mark(const car_state *car, const rival_mark *m, int lead) {
  static int anim[2] = { -2, -2 };
  if (anim[0] == -2) {
    anim[0] = anm_find("cons_g_R");
    anim[1] = anm_find("cons_g_R_tg");
  }
  int a = anim[m->tg != 0];
  if (a < 0) return;
  const float *b = car->rival_body[m->slot];
  float pw[4] = { b[12], b[13] + 14.0f, b[14], 1 }, pc[4], q[3][4];
  xform(mark_view, pw, pc);
  for (int k = 0; k < 3; ++k) {
    float p[4] = { pc[0] + 0.5f * (float)(k - 1), pc[1], pc[2], 1 };
    xform(mark_proj, p, q[k]);
    for (int i = 0; i < 3; ++i) q[k][i] /= q[k][3];
  }
  float x = q[1][0], y = q[1][1], z = q[1][2], adv = m->adv;
  float sx = 320.0f + 320.0f * x, sy = 240.0f - 240.0f * y;
  int band = (int)(fabsf(adv) * 5e-5f);
  if (band < 0) band = 0;
  if (band > 2) band = 2;
  anm_swap sw[4];
  int nsw;
  if (m->tg) {
    sw[0] = (anm_swap){ anm_sprite(T_R_TEMPLATE), -1 };
    sw[1] = (anm_swap){ anm_sprite(T_R_CROWN), -1 };
    sw[2] = (anm_swap){ anm_sprite(T_R_FONT), -1 };
    sw[3] = (anm_swap){ anm_sprite(T_R_FONT2), -1 };
    nsw = 4;
  } else {
    sw[0] = (anm_swap){ anm_sprite(T_R_TEMPLATE), anm_sprite(MARK_R[m->mark]) };
    nsw = 1;
  }
  typeof(mark_state[0]) *st = &mark_state[m->mark];
  if (x > -1 && x < 1 && y > -1 && y < 1 && z > -1 && z < 1) {
    float scale = (q[2][0] - q[0][0]) * 100.0f;
    scale = scale < 0 ? 0 : scale > 1.8f ? 1.8f : scale;
    int f;
    if (!m->tg) {
      if (st->pop < 60) ++st->pop;
      f = st->pop;
    } else if (m->name && lead) {
      f = MARK_TG[m->mark] + (int)st->phase;
      st->phase += MARK_SPEED[band];
      if (st->phase >= 120.0f) st->phase = 0;
      sy += 15.0f * scale;
    } else {
      f = MARK_TG[m->mark] + 60;
      sy += (m->name ? 15.0f : 20.0f) * scale;
    }
    if (adv > -200000.0f && adv < 3000.0f) {
      anm_draw_xf(a, f, 1, sx, sy, scale, sw, nsw);
      if (m->name) mark_name(a, f, m->name, sx, sy, scale);
    }
  } else {
    float mx = 640.0f - sx;
    mx = mx < R_HALF_W ? R_HALF_W : mx > 640 - R_HALF_W ? 640 - R_HALF_W : mx;
    float scale = (1.5f + 1.0f - fabsf(adv) / 150000.0f) * 0.4f;
    int f = st->loop[band]++ % (int)MARK_LOOP[band];
    if (m->tg) f += MARK_TG[m->mark];
    if (adv > 6000.0f && adv < 150000.0f) {
      anm_draw_xf(a, f, 1, mx, m->tg ? 470.0f : 450.0f, scale, sw, nsw);
      if (m->name) mark_name(a, f, m->name, mx, 470.0f, scale);
    }
  }
}

static void draw_rival_marks(const car_state *car) {
  if (!mark_on || (!ta_story && ten_level < 0)) return;
  const car_race *r = &car->race;
  rival_mark m[4];
  int n = 0;
  if (ten_level >= 0) {
    if (car->rival_shown[0]) m[n++] = (rival_mark){ 1, 0, 0, r->ten_gap * 1000.0f, NULL };
  } else if (vs_n) {
    for (int i = 0; i < vs_n && i < CAR_RIVALS && n < 4; ++i)
      if (car->rival_shown[i] && r->vs_on[i])
        m[n++] = (rival_mark){ vs_box[i].seat & 3, i, 1, r->vs_gap[i] * 1000.0f, vs_box[i].name };
  } else {
    int count = story_n ? story_n : 1;
    for (int i = 0; i < count && i < CAR_RIVALS && n < 4; ++i) {
      if (!car->rival_shown[i]) continue;
      int irregular = 0;
      for (int k = 0; k < 11; ++k) irregular |= STORY_IRREGULAR[k][0] == story_sel && STORY_IRREGULAR[k][1] == i + 1;
      m[n++] = (rival_mark){ i + 1, i, irregular, (story_n ? r->story_gap[i] : r->gap) * 1000.0f, NULL };
    }
  }
  int lead = -1; /* the console's first on its sort by advantage */
  for (int i = 0; i < n; ++i)
    if (lead < 0 || m[i].adv < m[lead].adv) lead = i;
  int order[4];
  for (int i = 0; i < n; ++i) order[i] = i;
  for (int i = 1; i < n; ++i) /* the furthest first */
    for (int j = i; j > 0 && fabsf(m[order[j]].adv) > fabsf(m[order[j - 1]].adv); --j) {
      int t = order[j];
      order[j] = order[j - 1];
      order[j - 1] = t;
    }
  for (int i = 0; i < n; ++i) draw_mark(car, &m[order[i]], order[i] == lead);
}

/* ---- the banner as each rival comes on (clTenStartRivalNameDisp: ctor 0x85e7a2c, update
 * 0x85e7b90, reset 0x85e7cd0). clRaceThread::race and changeTenRival reset it as the round's
 * rival is set up: menu_layout_or_demo_0705b from frame 0 (115 frames, once; nothing shows
 * before 1), its round (font 4, "%s人目", template 0x59f), the rival's "vs " name (font 4,
 * datrums 12: right aligned, template 0x5a0) and its car's name (getTenCharaCarImage, 0x1d2).
 * update holds it at frame 0 until getInfoTenSwRound (S2DInfoTenFight +0x90, SwName: up as
 * the rival rolls in beside the player) and then plays it; clRaceThread::yield draws it
 * while it is not over, over the consoles. clSpriteFont::draw puts a datrums 0 string's
 * 32-pixel cells (font 4 advances 28) at the template's place less (32 - 28) / 2, a datrums 12
 * one's its width (the advances' sum) left of it. Pre-rendered: menupack.py
 * TEN_BANNER_TEXT_BASE. ---- */
enum { TEN_BANNER_TEXT = 101700, T_BANNER_ROUND = 0x59f, T_BANNER_NAME = 0x5a0, T_BANNER_CAR = 0x1d2,
       BANNER_FRAMES = 115 };

static void draw_ten_banner(const car_state *car) {
  static int anim = -2;
  if (anim == -2) anim = anm_find("menu_layout_or_demo_0705b");
  const car_race *r = &car->race;
  if (anim < 0 || ten_level < 0) return;
  int extend = ten_level >= 10, max = extend ? 31 : 9;
  if (r->ten_rival != banner_rival) { /* reset(false) */
    banner_rival = r->ten_rival;
    banner_round = r->ten_beaten < 0 ? 0 : r->ten_beaten > max ? max : r->ten_beaten;
    int lv = ten_level > 9 ? 9 : ten_level;
    banner_chara = extend ? banner_round : TEN_CHARA[lv][banner_round > 9 ? 9 : banner_round];
    banner_frame = 0;
    banner_on = 0;
  }
  if (r->ten_sw_name) banner_on = 1;
  if (banner_frame >= BANNER_FRAMES) return;
  int f = banner_frame;
  if (banner_on) ++banner_frame;
  int round = anm_sprite(TEN_BANNER_TEXT + banner_round), name = anm_sprite(TEN_BANNER_TEXT + 32 + banner_chara);
  int t_round = anm_sprite(T_BANNER_ROUND), t_name = anm_sprite(T_BANNER_NAME), t_car = anm_sprite(T_BANNER_CAR);
  anm_swap sw[3] = { { t_round, -1 }, { t_name, -1 }, { t_car, anm_sprite(TEN_FACE[banner_chara][2]) } };
  anm_draw_ex(anim, f, 1, 0, 0, sw, 3);
  /* the strings, drawn after the frame as the fonts are: each alone where its template is */
  int others[3] = { t_round, t_name, t_car };
  for (int k = 0; k < 2; ++k) {
    anm_swap only[4];
    int n = 0;
    for (int i = 0; i < 3; ++i) only[n++] = (anm_swap){ others[i], i == k ? (k ? name : round) : -1 };
    only[n++] = (anm_swap){ anm_sprite(2946), -1 }; /* the tuning bar */
    float dx = k ? -(anm_sprite_width(name) - 4.0f) : -2.0f, dy = k ? 0.0f : -2.0f;
    anm_draw_ex(anim, f, 1, dx, dy, only, n);
  }
}

/* clWorkSpace::setTenDisp2D: clRaceThread::initTen and the race's beaten rival (play state 9)
 * clear it, clRaceThread::yield sets it once the banner is over (clTenStartRivalNameDisp::
 * isAction) in play state 1 with under 3.4 s left of the rolling start. While clear the
 * 10-in-a-row shows no console (clStandardConsole::draw only the countdown, in states 1
 * and 2; clGraphicFrame no mini map; clTenConsole nothing). */
static void ten_disp_update(const car_race *r) {
  if (r->play_state == 9) ten_disp = 0;
  else if (r->play_state == 1 && r->countdown < 3.4f && !(banner_on && banner_frame < BANNER_FRAMES)) ten_disp = 1;
}

/* ---- the 10-in-a-row rivals' lines (Cut::clCutIn::Update's ten branch, 0x85b3d75): yt::
 * CSeq::Exec's cut-in info gives the rival's character (+0x4c), CTenSeq::SwCutIn (+0x50) and
 * the beaten flag (+0x51). As one comes up the line is set (clCutinDisp::setSpeak at place 0,
 * with the flash), as it goes down it ends (setEnd); the entrance one is ECutInStart line
 * 2 serial + ((int)timer & 1) of data/cutin/jp/CutInTenRival.txt, the beaten one's ECutInLose
 * line serial (serial: the table at 0x9768e84 over the 10-in-a-row characters; timer: the
 * frames the cut-in has run). clCutinDisp per place: setSpeak with the flash plays both
 * cons_g_cutin_effect and cons_g_cutin_serifu frames 91..109 at (0, 160) (place 0's
 * 0x93cfc80, facing left's 0x93cfc78 0), state 1; once serifu is over (isBound) the flash
 * stops, serifu holds frame 0, SE turning3_cutin, state 2; setEnd plays serifu 61..69, state
 * 3, then nothing. serifu's template 0x18fc is the face (the cell of the expression), 0x37d
 * the line, font 3 datrums 9: centred, its widest line's advances W, at the template's x +
 * (30 - W) / 2 + (20 - 30) / 2 (the font's cell 30, full width 20). Pre-rendered: menupack.py
 * CUTIN_TEXT_BASE / CUTIN_FACE_BASE + the file's line. ---- */
enum { CUT_TEXT = 103000, CUT_FACE = 103200, CUT_LOSE = 64, T_CUT_FACE = 0x18fc, T_CUT_TEXT = 0x37d,
       CUT_Y = 160, CUT_CELL = 30, CUT_FULL = 20 };
static const unsigned char CUT_SERIAL[32] = { 0, 1, 3, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                                              16, 17, 18, 19, 20, 21, 22, 23, 24, 26, 27, 28, 25, 29, 30, 31 };
typedef struct { int anim, start, len, t; } cut_anime; /* anim -1: stopped (not drawn) */
static struct {
  int state, line, timer, said_start, said_lose;
  cut_anime flash, serifu;
} cut;

static int keys_pause_only(void) { return ten_level >= 0 && (!ten_disp || cut.state); }

static void cut_reset(void) {
  memset(&cut, 0, sizeof cut);
  cut.flash.anim = cut.serifu.anim = -1;
}

static void cut_play(cut_anime *a, int anim, int start, int len) { *a = (cut_anime){ anim, start, len, 0 }; }
static int cut_bound(const cut_anime *a) { return a->anim >= 0 && a->t >= a->len; }
static int cut_frame(const cut_anime *a) { return a->start + (a->t < a->len ? a->t : a->len - 1); }

static void cut_speak(int line) {
  static int effect = -2, serifu;
  if (effect == -2) {
    effect = anm_find("cons_g_cutin_effect");
    serifu = anm_find("cons_g_cutin_serifu");
  }
  cut_play(&cut.flash, effect, 91, 19);
  cut_play(&cut.serifu, serifu, 91, 19);
  cut.line = line;
  cut.state = 1;
}

static void cut_end(void) {
  if (!cut.state) return;
  cut.flash.anim = -1;
  cut_play(&cut.serifu, cut.serifu.anim, 61, 9);
  cut.state = 3;
}

/* One arcade frame of it (clRaceThread::yield, or the beaten one's still frames) */
static void cut_step(const car_race *r) {
  int serial = r->cut_char >= 0 && r->cut_char < 32 ? CUT_SERIAL[r->cut_char] : -1;
  if (r->cut_start && !cut.said_start) {
    if (serial >= 0) cut_speak(2 * serial + (cut.timer & 1));
    cut.said_start = 1;
  } else if (!r->cut_start && cut.said_start) {
    cut_end();
    cut.said_start = 0;
  }
  if (r->cut_lose && !cut.said_lose) {
    if (serial >= 0) cut_speak(CUT_LOSE + serial);
    cut.said_lose = 1;
  } else if (!r->cut_lose && cut.said_lose) {
    cut_end();
    cut.said_lose = 0;
  }
  /* clCutinDisp::update: the states, then the animes */
  if (cut.state == 1 && cut_bound(&cut.serifu)) {
    cut.flash.anim = -1;
    cut_play(&cut.serifu, cut.serifu.anim, 0, 1);
    cut.state = 2;
    snd_menu(SND_TURN3_CUTIN);
  } else if (cut.state == 3 && cut_bound(&cut.serifu)) {
    cut.flash.anim = cut.serifu.anim = -1;
    cut.state = 0;
  }
  if (cut.flash.anim >= 0) cut.flash.t++;
  if (cut.serifu.anim >= 0) cut.serifu.t++;
  cut.timer++;
}

void hud_ten_cutin(const car_race *r, int frames) {
  if (ten_level < 0) return;
  for (int i = 0; i < frames; ++i) cut_step(r);
}

static void draw_cutin(void) {
  if (ten_level < 0) return;
  if (cut.flash.anim >= 0) anm_draw_ex(cut.flash.anim, cut_frame(&cut.flash), 1, 0, CUT_Y, NULL, 0);
  if (cut.serifu.anim < 0) return;
  int f = cut_frame(&cut.serifu), text = anm_sprite(CUT_TEXT + cut.line);
  int t_face = anm_sprite(T_CUT_FACE), t_text = anm_sprite(T_CUT_TEXT);
  anm_swap sw[2] = { { t_face, anm_sprite(CUT_FACE + cut.line) }, { t_text, -1 } };
  anm_draw_ex(cut.serifu.anim, f, 1, 0, CUT_Y, sw, 2);
  /* the line, after the anime as clCutinDisp::draw has its font cluster */
  int w = (int)anm_sprite_width(text) - CUT_CELL;
  anm_swap only[2] = { { t_face, -1 }, { t_text, text } };
  anm_draw_ex(cut.serifu.anim, f, 1, (float)((CUT_CELL - w) / 2 + (CUT_FULL - CUT_CELL) / 2), CUT_Y, only, 2);
}

/* ---- the story's realtime movies' shows (clEffectTable, src/storydemo.h) ----
 * clRaceThread::yield runs clEffectTable::update and draw each frame of a movie with a show:
 * its frame counter one up, the events whose frame it has reached (types in storydemo.py),
 * then its clCutinDisp (3 places, each as the 10-in-a-row's one: setSpeak's flash and line
 * frames 91..109, the line held on frame 0 with turning3_cutin, setEnd's 61..69) and its
 * anime clusters. draw: clMenuFade (+4, black or white over 1/6 s), the clusters, the cut-ins.
 * ePos gives the place (0x93cfc58) and y (0x93cfc80); way the line's anime (0x93cfc70), flash
 * the flash's (0x93cfc98), the row its x. Lines: menu.bin's DEMO_TEXT / DEMO_FACE + row. */
enum { DEMO_TEXT = 104000, DEMO_FACE = 104400, DEMO_FADE = 10, DEMO_WARM = 96 };
static const unsigned char DEMO_PLACE[6] = { 0, 0, 1, 0, 1, 2 };
static const short DEMO_Y[6] = { 160, 120, 304, 120, 224, 340 };
typedef struct { int state, row, y, x; cut_anime flash, serifu; } demo_place;
static struct {
  int table, frame, next, held, negaposi;
  struct { int anim, t, x, y; } cl[3]; /* anim -1: stopped */
  demo_place cut[3];
  float fade_from, fade_to; int fade_t, fade_black;
  int warm[DEMO_WARM], n_warm;
} demo = { .table = -1 };
static int demo_anim[STORYDEMO_ANIMES]; /* anm_find + 1 of each anime row's (0 not looked up) */

static int demo_row_anim(int row) {
  if (!demo_anim[row]) demo_anim[row] = anm_find(STORYDEMO_ANIME[row].anim) + 1;
  return demo_anim[row] - 1;
}

static int demo_cut_anim(int which, int kind) {
  static int a[2][2] = { { -2, -2 }, { -2, -2 } };
  static const char *const NAME[2][2] = { { "cons_g_cutin_serifu", "cons_g_cutin2_serifu" },
                                          { "cons_g_cutin_effect", "cons_g_cutin2_effect" } };
  kind &= 1;
  if (a[which][kind] == -2) a[which][kind] = anm_find(NAME[which][kind]);
  return a[which][kind];
}

static void demo_warm_add(int sprite) {
  if (sprite < 0 || demo.n_warm >= DEMO_WARM) return;
  for (int i = 0; i < demo.n_warm; ++i)
    if (demo.warm[i] == sprite) return;
  demo.warm[demo.n_warm++] = sprite;
}

void hud_demo_start(int table) {
  memset(&demo, 0, sizeof demo);
  demo.table = table >= 0 && table < STORYDEMO_TABLES ? table : -1;
  for (int i = 0; i < 3; ++i) {
    demo.cl[i].anim = -1;
    demo.cut[i].flash.anim = demo.cut[i].serifu.anim = -1;
  }
  if (demo.table < 0) return;
  /* what it draws up in GL now (the movie's start, still loading), kept up till it is drawn */
  const storydemo_table *t = &STORYDEMO_TABLE[demo.table];
  for (int i = 0; i < t->n; ++i) {
    const storydemo_event *e = &STORYDEMO_EVENT[t->first + i];
    int list[32], n = 0;
    if (e->type == 8 && e->arg < STORYDEMO_ANIMES) n = anm_sprites(demo_row_anim(e->arg), list, 32);
    if (e->type == 10 && e->arg < STORYDEMO_CUTINS) {
      list[n++] = anm_sprite(DEMO_TEXT + e->arg);
      list[n++] = anm_sprite(DEMO_FACE + e->arg);
      n += anm_sprites(demo_cut_anim(0, STORYDEMO_CUTIN[e->arg].way), list + n, 8);
      n += anm_sprites(demo_cut_anim(1, STORYDEMO_CUTIN[e->arg].flash), list + n, 8);
    }
    for (int k = 0; k < n; ++k) demo_warm_add(list[k]);
  }
  for (int i = 0; i < demo.n_warm; ++i) anm_warm_sprite(demo.warm[i], 1);
}

void hud_demo_end(void) { hud_demo_start(-1); }

static void demo_speak(int row) {
  const storydemo_cutin *c = &STORYDEMO_CUTIN[row];
  int pos = c->pos < 6 ? c->pos : 0;
  demo_place *k = &demo.cut[DEMO_PLACE[pos]];
  cut_play(&k->flash, demo_cut_anim(1, c->flash), 91, 19);
  cut_play(&k->serifu, demo_cut_anim(0, c->way), 91, 19);
  k->row = row;
  k->x = c->x;
  k->y = DEMO_Y[pos];
  k->state = 1;
}

static void demo_end_speech(int row) {
  int pos = STORYDEMO_CUTIN[row].pos < 6 ? STORYDEMO_CUTIN[row].pos : 0;
  demo_place *k = &demo.cut[DEMO_PLACE[pos]];
  k->flash.anim = -1;
  if (k->serifu.anim >= 0) cut_play(&k->serifu, k->serifu.anim, 61, 9);
  k->state = 3;
}

int hud_demo_step(void) {
  if (demo.table < 0) return 0;
  const storydemo_table *t = &STORYDEMO_TABLE[demo.table];
  demo.frame++;
  for (; demo.next < t->n && STORYDEMO_EVENT[t->first + demo.next].frame <= demo.frame; ++demo.next) {
    const storydemo_event *e = &STORYDEMO_EVENT[t->first + demo.next];
    switch (e->type) {
    case 0: case 1: demo.negaposi = e->type == 0; break;
    case 4: case 5: case 6: case 7:
      demo.fade_from = e->type & 1 ? 0.0f : 1.0f;
      demo.fade_to = 1.0f - demo.fade_from;
      demo.fade_t = 0;
      demo.fade_black = e->type < 6;
      break;
    case 8:
      if (e->arg < STORYDEMO_ANIMES) {
        const storydemo_anime *a = &STORYDEMO_ANIME[e->arg];
        int c = a->cluster >= 0 && a->cluster < 3 ? a->cluster : 0;
        demo.cl[c].anim = demo_row_anim(e->arg);
        demo.cl[c].t = 0;
        demo.cl[c].x = a->x;
        demo.cl[c].y = a->y;
      }
      break;
    case 9:
      if (e->arg < STORYDEMO_ANIMES) {
        int c = STORYDEMO_ANIME[e->arg].cluster;
        demo.cl[c >= 0 && c < 3 ? c : 0].anim = -1;
      }
      break;
    case 10: if (e->arg < STORYDEMO_CUTINS) demo_speak(e->arg); break;
    case 11: if (e->arg < STORYDEMO_CUTINS) demo_end_speech(e->arg); break;
    case 15: if (e->arg < STORYDEMO_SES) snd_effect(STORYDEMO_SE[e->arg].wav, STORYDEMO_SE[e->arg].vol); break;
    case 20: case 21: demo.held = e->type == 20; break;
    }
  }
  for (int i = 0; i < 3; ++i) {
    demo_place *k = &demo.cut[i];
    if (k->state == 1 && cut_bound(&k->serifu)) {
      k->flash.anim = -1;
      cut_play(&k->serifu, k->serifu.anim, 0, 1);
      k->state = 2;
      snd_menu(SND_TURN3_CUTIN);
    } else if (k->state == 3 && cut_bound(&k->serifu)) {
      k->flash.anim = k->serifu.anim = -1;
      k->state = 0;
    }
    if (k->flash.anim >= 0) k->flash.t++;
    if (k->serifu.anim >= 0) k->serifu.t++;
    if (demo.cl[i].anim >= 0) demo.cl[i].t++;
  }
  if (demo.fade_t < DEMO_FADE) demo.fade_t++;
  for (int i = 0; i < demo.n_warm; ++i) anm_warm_sprite(demo.warm[i], 0);
  return demo.held;
}

void hud_demo_draw(const hud_state *h) {
  if (demo.table < 0) return;
  if (demo.negaposi) hud_negate(h);
  float a = demo.fade_from + (demo.fade_to - demo.fade_from) * (float)demo.fade_t / DEMO_FADE;
  if (a > 0.0f) hud_fade(h, demo.fade_black ? 0.0f : 1.0f, a);
  for (int i = 0; i < 3; ++i) {
    int an = demo.cl[i].anim;
    if (an < 0) continue;
    int n = anm_frames(an), f = demo.cl[i].t < n ? demo.cl[i].t : n - 1;
    anm_draw_ex(an, f, 1, (float)demo.cl[i].x, (float)demo.cl[i].y, NULL, 0);
  }
  for (int i = 0; i < 3; ++i) {
    demo_place *k = &demo.cut[i];
    if (k->flash.anim >= 0) anm_draw_ex(k->flash.anim, cut_frame(&k->flash), 1, (float)k->x, (float)k->y, NULL, 0);
    if (k->serifu.anim < 0) continue;
    int f = cut_frame(&k->serifu), text = anm_sprite(DEMO_TEXT + k->row);
    int t_face = anm_sprite(T_CUT_FACE), t_text = anm_sprite(T_CUT_TEXT);
    anm_swap sw[2] = { { t_face, anm_sprite(DEMO_FACE + k->row) }, { t_text, -1 } };
    anm_draw_ex(k->serifu.anim, f, 1, (float)k->x, (float)k->y, sw, 2);
    int w = (int)anm_sprite_width(text) - CUT_CELL;
    anm_swap only[2] = { { t_face, -1 }, { t_text, text } };
    anm_draw_ex(k->serifu.anim, f, 1, (float)(k->x + (CUT_CELL - w) / 2 + (CUT_FULL - CUT_CELL) / 2), (float)k->y, only, 2);
  }
}

static void draw_ten(const car_state *car) {
  static int men = -2, split;
  if (men == -2) {
    men = anm_find("cons_g_or_men");
    split = anm_find("cons_g_splittime");
  }
  const car_race *r = &car->race;
  if (ten_level < 0 || men < 0 || split < 0 || r->state != CAR_RACE_RUN) return;
  if (!ten_disp) return;
  const float zoom = HUD_K / 1.6f, dx = TA_X / 1.6f, dy = TA_Y / 1.6f;
  int extend = ten_level >= 10, max = extend ? 31 : 9;
  int round = r->ten_beaten < 0 ? 0 : r->ten_beaten > max ? max : r->ten_beaten;
  if (round != ten_round) {
    if (ten_round < 0 || ++ten_round_wait > 5) {
      if (ten_round >= 0 && round > 0) extend_at = r->total; /* clTimeExtendConsole::set */
      ten_round = round;
      ten_round_wait = 0;
    }
  }
  /* cons_g_or_men */
  anm_swap sw[9];
  int n = 0, lv = ten_level + 1, shown = ten_round + 1;
  sw[n++] = (anm_swap){ anm_sprite(T_OR_MEN), uv(UV_MEN) };
  sw[n++] = (anm_swap){ anm_sprite(T_OR_LV), uv(UV_LV) };
  if (extend) {
    sw[n++] = (anm_swap){ anm_sprite(T_TTIME1), uv(UV_EX) };
    sw[n++] = (anm_swap){ anm_sprite(T_TTIME0), -1 };
  } else if (lv <= 9) {
    sw[n++] = (anm_swap){ anm_sprite(T_TTIME1), uv(UV_TIME0 + lv) };
    sw[n++] = (anm_swap){ anm_sprite(T_TTIME0), -1 };
  } else {
    sw[n++] = (anm_swap){ anm_sprite(T_TTIME1), uv(UV_TIME0 + 1) };
    sw[n++] = (anm_swap){ anm_sprite(T_TTIME0), uv(UV_TIME0) };
  }
  sw[n++] = (anm_swap){ anm_sprite(T_PLACE4), uv(UV_PLACE0 + (shown % 10 == 0 ? 9 : shown % 10 - 1)) };
  sw[n++] = (anm_swap){ anm_sprite(T_PLACE3), uv(UV_PLACE0 + (shown / 10 % 10 == 0 ? 9 : shown / 10 % 10 - 1)) };
  anm_draw_xf(men, shown <= 9 ? 4 : 9, 1, dx, dy, zoom, sw, n);
  /* cons_g_splittime: rows 0..3, the last four of the rivals so far */
  int first = ten_round > 3 ? ten_round - 3 : 0;
  for (int k = first; k <= ten_round; ++k) { /* getInfoTenClearTime: the one on the road's runs */
    anm_swap ts[8];
    anm_draw_xf(split, k - first, 1, dx, dy, zoom, ts, ten_time_swaps(r->ten_time[k], ts));
  }
  draw_rival_info(car, ten_round);
}

static void draw_ta(const hud_state *h, const car_state *car, const mat4 *proj) {
  const car_race *r = &car->race;
  if (!SPR[SPR_TILL_GOAL].tex) return;
  glEnableVertexAttribArray(1);
  if (ten_level < 0 && !vs_n) {
    draw_till_goal(r->remain);
    draw_times(r);
  }
  map_blit(SPR_MAP_WAKU, 15, 262);
  draw_navi(h, r);
  glUseProgram(tex_prog);
  glUniformMatrix4fv(u_tproj, 1, GL_FALSE, proj->m);
  glEnableVertexAttribArray(1);
  /* the black box over the map: the course's name, in a story race the road's (S2DInfoMap +0x14) */
  if (!ta_story && ta_course >= 0 && ta_course < CAR_COURSES) map_blit(SPR_MAP_NAME + ta_course, 15, 266);
  if (ta_story && r->map_text >= 1 && r->map_text <= 19) map_blit(SPR_MAP_TEXT + r->map_text - 1, 15, 266);
  draw_time_left(r->time_left);
  if (!ta_story && ten_level < 0) draw_records();
  draw_sign(r);
  if (r->state == CAR_RACE_RUN) draw_time_extend(r->total);
  glDisableVertexAttribArray(1);
}

static void fill_rect(float x, float y, float w, float h) {
  float v[8] = { x, y, x + w, y, x, y + h, x + w, y + h };
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, v);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

static void frame_rect(rect r, float t) {
  fill_rect(r.x, r.y, r.w, t);
  fill_rect(r.x, r.y + r.h - t, r.w, t);
  fill_rect(r.x, r.y, t, r.h);
  fill_rect(r.x + r.w - t, r.y, t, r.h);
}

static void ring(float cx, float cy, float r0, float r1) {
  float v[66 * 2];
  int n = 0;
  for (int i = 0; i <= 32; ++i) {
    float a = (float)i / 32.0f * 2.0f * (float)M_PI;
    v[n++] = cx + cosf(a) * r1; v[n++] = cy + sinf(a) * r1;
    v[n++] = cx + cosf(a) * r0; v[n++] = cy + sinf(a) * r0;
  }
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, v);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, n / 2);
}

static void disc(float cx, float cy, float r) { ring(cx, cy, 0, r); }

static void triangle(float x0, float y0, float x1, float y1, float x2, float y2) {
  float v[6] = { x0, y0, x1, y1, x2, y2 };
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, v);
  glDrawArrays(GL_TRIANGLES, 0, 3);
}

/* Rounded rectangle outline, inset by `in`; 4 corners x 6 steps. */
static int rrect_path(rect r, float rad, float in, float *v) {
  static const float cx[4] = { 1, 0, 0, 1 }, cy[4] = { 1, 1, 0, 0 }; /* BR, BL, TL, TR */
  int n = 0;
  float rr = rad - in;
  for (int c = 0; c < 4; ++c) {
    float ox = r.x + in + rr + cx[c] * (r.w - 2 * rad), oy = r.y + in + rr + cy[c] * (r.h - 2 * rad);
    for (int i = 0; i <= 6; ++i) {
      float a = ((float)c + (float)i / 6.0f) * (float)M_PI / 2.0f;
      v[n++] = ox + cosf(a) * rr;
      v[n++] = oy + sinf(a) * rr;
    }
  }
  return n / 2;
}

static void fill_rrect(rect r, float rad) {
  float v[2 + 28 * 2 + 2];
  v[0] = r.x + r.w / 2;
  v[1] = r.y + r.h / 2;
  int n = rrect_path(r, rad, 0, v + 2);
  v[2 + n * 2] = v[2];
  v[3 + n * 2] = v[3];
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, v);
  glDrawArrays(GL_TRIANGLE_FAN, 0, n + 2);
}

static void stroke_rrect(rect r, float rad, float t) {
  float a[28 * 2], b[28 * 2], v[29 * 4];
  int n = rrect_path(r, rad, 0, a);
  rrect_path(r, rad, t, b);
  for (int i = 0; i <= n; ++i) {
    int k = i % n;
    v[i * 4 + 0] = a[k * 2]; v[i * 4 + 1] = a[k * 2 + 1];
    v[i * 4 + 2] = b[k * 2]; v[i * 4 + 3] = b[k * 2 + 1];
  }
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, v);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, (n + 1) * 2);
}

/* One look for every control: smoked glass panel, thin silver rim, orange when held. */
static void button(rect r, int held) {
  if (held) color(1.0f, 0.42f, 0.05f, 0.70f);
  else color(0.04f, 0.05f, 0.08f, 0.50f);
  fill_rrect(r, 12);
  color(1, 1, 1, 0.07f); /* top sheen */
  fill_rrect((rect){ r.x + 4, r.y + 4, r.w - 8, fminf(r.h * 0.22f, 26) }, 8);
  color(0.85f, 0.88f, 0.92f, held ? 0.95f : 0.60f);
  stroke_rrect(r, 12, 2);
}

/* a slider pedal's travel v (0..1): a bar up the key's right edge, its foot to its top */
static void pedal_bar(rect r, float v) {
  float x = r.x + r.w - 12, y0 = r.y + PEDAL_FOOT, y1 = r.y + r.h - PEDAL_FOOT;
  color(0, 0, 0, 0.5f);
  fill_rect(x, y0, 6, y1 - y0);
  color(1.0f, 0.42f, 0.05f, 0.95f);
  fill_rect(x, y1 - v * (y1 - y0), 6, v * (y1 - y0));
}

static void chevron(float cx, float cy, float s, float dx, float dy) {
  /* thick arrow head pointing along (dx, dy) */
  float px = -dy, py = dx;
  float v[12] = {
    cx + dx * s, cy + dy * s, cx - dx * s * 0.2f + px * s, cy - dy * s * 0.2f + py * s,
    cx - dx * s * 0.2f - px * s, cy - dy * s * 0.2f - py * s,
  };
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, v);
  glDrawArrays(GL_TRIANGLES, 0, 3);
}

/* Seven-segment digit; segments a..g as bits 0..6. */
static void digit(int d, float x, float y, float w, float h) {
  static const unsigned char SEG[11] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F,
                                         0x50 /* 'r' */ };
  float t = w * 0.18f, hh = h * 0.5f;
  unsigned char s = SEG[d];
  if (s & 1) fill_rect(x, y, w, t);
  if (s & 2) fill_rect(x + w - t, y, t, hh);
  if (s & 4) fill_rect(x + w - t, y + hh, t, hh);
  if (s & 8) fill_rect(x, y + h - t, w, t);
  if (s & 16) fill_rect(x, y + hh, t, hh);
  if (s & 32) fill_rect(x, y, t, hh);
  if (s & 64) fill_rect(x, y + hh - t * 0.5f, w, t);
}

static void number(int v, int digits, float x, float y, float w, float h) {
  for (int i = digits - 1; i >= 0; --i) {
    int d = v % 10;
    v /= 10;
    if (v == 0 && d == 0 && i < digits - 1) continue; /* no leading zeros */
    digit(d, x + (float)i * (w * 1.35f), y, w, h);
  }
}

/* ---- the start's countdown, clCountDownConsole: cons_g_countdown at the top and, for the
 * manual gearbox, cons_g_count_mis_irero ("Put the gear in 2") in the middle, from the menu
 * animations (menu.bin). clStandardConsole::update hands it n = (int)(the seconds left + 1/12)
 * as that changes; setCountDown(n) plays 60 frames of the animation from (3 - n) * 60 + 16
 * and holds for 2 and 1, GO (0) hides after them, 4 and 3 show nothing. The gear prompt
 * (setKeepShift(2), with the template 0x1b18 showing cons_p_mis_irero_2) blinks, frames 0..29
 * looped, till GO. */
#define COUNT_FROM 4.0f /* CState::Prep_CountDown */
#define COUNT_LEN 60
#define COUNT_GO_FRAMES 59 /* GO's 60-frame timer runs down from the frame it is set */
enum { T_KEEP_SHIFT = 0x1b18, KEEP_SHIFT_2 = 0x7ca };
#define KEEP_SHIFT_X 419.0f /* the template's (190, 276) moved past the text */
#define KEEP_SHIFT_Y 276.0f

static void draw_countdown(const car_state *car) {
  static int anim = -2, keep;
  if (anim == -2) {
    anim = anm_find("cons_g_countdown");
    keep = anm_find("cons_g_count_mis_irero");
  }
  const car_race *r = &car->race;
  if (anim < 0 || r->state != CAR_RACE_RUN) return;
  /* seconds since the countdown began, the race's own time going on from its end */
  float t = r->countdown > 0.0f ? COUNT_FROM - r->countdown : COUNT_FROM + r->total;
  int n = r->countdown > 0.0f ? (int)(r->countdown + 1.0f / 12.0f) : 0;
  if (n == 0 && t >= COUNT_FROM - 11.0f / 12.0f + COUNT_GO_FRAMES / 60.0f) return;
  /* the sprites up before they show (GO's are two 512 x 512) */
  anm_preload(anim);
  if (n > 0 && car->manual && keep >= 0) {
    /* the template sits at the start of the line, where the Japanese text leaves the digit
     * its place ("2速に入れろ"); these files' English "Put the gear in" fills it, so the digit
     * goes after the text instead (its right edge, 416 on the screen, and a space) */
    int f = (int)(t * 60.0f) % 30;
    anm_swap none = { anm_sprite(T_KEEP_SHIFT), -1 };
    anm_draw_ex(keep, f, 1, 0, 0, &none, 1);
    if (f < 20) anm_draw_sprite(anm_sprite(KEEP_SHIFT_2), KEEP_SHIFT_X, KEEP_SHIFT_Y, 1);
  }
  if (n > 2) return;
  float from = COUNT_FROM - (float)n - 11.0f / 12.0f; /* when n came */
  int f = (int)((t - from) * 60.0f);
  anm_draw(anim, (3 - n) * COUNT_LEN + 16 + (f < COUNT_LEN - 1 ? f : COUNT_LEN - 1), 1);
}

/* the race's touch keys (each side's in its own projection, key_ortho); steer the wheel's
 * turn for the bar between the steer keys, all_keys 0 for pause only */
static void draw_keys(const hud_state *h, float steer, int all_keys) {
  /* steering (the gyro's: none); each side's keys in its own projection (key_ortho) */
  if (all_keys) {
    rect brake = h->gyro ? G_BRAKE : BRAKE, up = h->gyro ? G_SHIFT_UP : SHIFT_UP, down = h->gyro ? G_SHIFT_DOWN : SHIFT_DOWN;
    mat4 side_proj = key_ortho(h->screen_w, h->screen_h, KEY_LEFT);
    glUniformMatrix4fv(u_proj, 1, GL_FALSE, side_proj.m);
    if (!h->gyro) {
      button(LEFT, h->left_touch != 0);
      button(RIGHT, h->right_touch != 0);
      color(1, 1, 1, 0.92f);
      chevron(LEFT.x + LEFT.w / 2 - 6, LEFT.y + LEFT.h / 2, 30, -1, 0);
      chevron(RIGHT.x + RIGHT.w / 2 + 6, RIGHT.y + RIGHT.h / 2, 30, 1, 0);
    }
    /* wheel angle bar between the two */
    float mid = (LEFT.x + RIGHT.x + RIGHT.w) / 2;
    color(0, 0, 0, 0.4f);
    fill_rect(LEFT.x, LEFT.y - 14, RIGHT.x + RIGHT.w - LEFT.x, 6);
    color(1.0f, 0.42f, 0.05f, 0.95f);
    if (steer > 0) fill_rect(mid, LEFT.y - 14, steer * (mid - LEFT.x), 6);
    else fill_rect(mid + steer * (mid - LEFT.x), LEFT.y - 14, -steer * (mid - LEFT.x), 6);

    /* pedals: ribbed pads, throttle tall and narrow like the cabinet's (the gyro's brake on the left) */
    if (h->gyro) {
      button(brake, h->brake_touch != 0);
      color(1, 1, 1, 0.55f);
      for (int i = 0; i < 5; ++i) fill_rect(brake.x + 18, brake.y + 28 + i * 20, brake.w - 36, 6);
      if (h->slide) pedal_bar(brake, h->brake_touch ? h->brake_v : 0);
    }
    side_proj = key_ortho(h->screen_w, h->screen_h, KEY_RIGHT);
    glUniformMatrix4fv(u_proj, 1, GL_FALSE, side_proj.m);
    button(THROTTLE, h->throttle_touch != 0);
    if (!h->gyro) button(brake, h->brake_touch != 0);
    color(1, 1, 1, 0.55f);
    for (int i = 0; i < 6; ++i) fill_rect(THROTTLE.x + 28, THROTTLE.y + 30 + i * 20, THROTTLE.w - 56, 6);
    if (!h->gyro)
      for (int i = 0; i < 5; ++i) fill_rect(brake.x + 18, brake.y + 28 + i * 20, brake.w - 36, 6);
    /* slider pedals: each one's travel, a bar up its right edge */
    if (h->slide) {
      pedal_bar(THROTTLE, h->throttle_touch ? h->throttle_v : 0);
      if (!h->gyro) pedal_bar(brake, h->brake_touch ? h->brake_v : 0);
    }

    /* shift up / down */
    button(up, 0);
    button(down, 0);
    color(1, 1, 1, 0.92f);
    chevron(up.x + up.w / 2, up.y + up.h / 2 - 4, 22, 0, -1);
    chevron(down.x + down.w / 2, down.y + down.h / 2 + 4, 22, 0, 1);
  }

  /* pause, reset and camera, small, top centre */
  mat4 top_proj = key_ortho(h->screen_w, h->screen_h, KEY_TOP);
  glUniformMatrix4fv(u_proj, 1, GL_FALSE, top_proj.m);
  button(PAUSE, 0);
  if (all_keys) {
    button(RESET, 0);
    button(CAMERA, 0);
    button(TRANS, 0);
    color(1, 1, 1, 0.9f);
    ring(RESET.x + 24, RESET.y + 24, 10, 14);
    triangle(RESET.x + 32, RESET.y + 6, RESET.x + 42, RESET.y + 17, RESET.x + 29, RESET.y + 19);
    ring(CAMERA.x + 24, CAMERA.y + 24, 7, 12);
  }
  color(1, 1, 1, 0.9f);
  fill_rect(PAUSE.x + 15, PAUSE.y + 13, 6, 22);
  fill_rect(PAUSE.x + 27, PAUSE.y + 13, 6, 22);
}

void hud_draw(const hud_state *h, const car_state *car, const car_input *in, float fps) {
  glUseProgram(prog);
  mat4 proj = screen_ortho(h->screen_w, h->screen_h);
  glUniformMatrix4fv(u_proj, 1, GL_FALSE, proj.m);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);
  glEnableVertexAttribArray(0);
  /* the 10-in-a-row's consoles only while TenDisp2D is set, its keys but pause too */
  ten_disp_update(&car->race);
  int consoles = ten_level < 0 || ten_disp, all_keys = !keys_pause_only();

  draw_keys(h, car->steer, all_keys);
  glUniformMatrix4fv(u_proj, 1, GL_FALSE, proj.m);

  /* small fps counter, top-left: at the screen's own edge (a phone's 16:9 has its view key,
   * bigger, where the layout's 16 is: Allan 10-29) */
  color(1, 1, 1, 0.6f);
  {
    /* and clear of the top keys wherever the KEY LAYOUT put them: under them if they cover it */
    rect top = key(group_rect(KEY_TOP, h->gyro), KEY_TOP);
    float fx = 16 - EDGE, fy = 16;
    if (fx < top.x + top.w + 4 && fx + 2 * 12 * 1.35f > top.x - 4 && fy < top.y + top.h + 4) fy = top.y + top.h + 8;
    number((int)(fps + 0.5f), 2, fx, fy, 12, 20);
  }

  if (tex_prog) {
    glUseProgram(tex_prog);
    glUniformMatrix4fv(u_tproj, 1, GL_FALSE, proj.m);
    glUniform1i(u_tex, 0);
    tint(1, 1, 1, 1);
    tint_add(0, 0, 0);
    glActiveTexture(GL_TEXTURE0);
  }
  if (tex_prog && consoles) {
    draw_meters(car);
    draw_shifter(car);
    draw_plate(car);
    draw_ta(h, car, &proj);
    draw_plate_text();
  }
  /* the AT/MT switch (a touch key of the port, not a console: shown while the 10-in-a-row
   * hides the consoles too) shows the gearbox in use with the shifter's own tag */
  int tag = car->manual ? SPR_TAG_MT : SPR_TAG_AT;
  if (tex_prog && SPR[tag].tex && all_keys) {
    glUseProgram(tex_prog);
    mat4 top = key_ortho(h->screen_w, h->screen_h, KEY_TOP);
    glUniformMatrix4fv(u_tproj, 1, GL_FALSE, top.m);
    tint(1, 1, 1, 1);
    tint_add(0, 0, 0);
    glEnableVertexAttribArray(1);
    float k = fminf(36.0f / SPR[tag].w, 30.0f / SPR[tag].h);
    sprite_k(tag, TRANS.x + TRANS.w / 2, TRANS.y + TRANS.h / 2, 0, k);
    glUniformMatrix4fv(u_tproj, 1, GL_FALSE, proj.m);
    glDisableVertexAttribArray(1);
  }
  if (consoles) draw_rival_marks(car);
  draw_ten(car);
  if (consoles && ta_story && ten_level < 0) draw_story_info(car);
  if (consoles) draw_vs(car);
  draw_ten_banner(car);
  draw_countdown(car);
  draw_cutin();
  glDisable(GL_BLEND);
}

void hud_negate(const hud_state *h) {
  glUseProgram(prog);
  mat4 proj = screen_ortho(h->screen_w, h->screen_h);
  glUniformMatrix4fv(u_proj, 1, GL_FALSE, proj.m);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);
  glEnableVertexAttribArray(0);
  color(1, 1, 1, 1);
  fill_rect(-EDGE, 0, h->screen_w + 2 * EDGE, h->screen_h);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_BLEND);
}

void hud_white(const hud_state *h, float alpha) { hud_fade(h, 1.0f, alpha); }

void hud_fade(const hud_state *h, float grey, float alpha) {
  if (alpha <= 0.0f) return;
  glUseProgram(prog);
  mat4 proj = screen_ortho(h->screen_w, h->screen_h);
  glUniformMatrix4fv(u_proj, 1, GL_FALSE, proj.m);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);
  glEnableVertexAttribArray(0);
  color(grey, grey, grey, alpha > 1.0f ? 1.0f : alpha);
  fill_rect(-EDGE, 0, h->screen_w + 2 * EDGE, h->screen_h);
  glDisable(GL_BLEND);
}

int hud_menu_key(float x, float y) {
  const float k = MENU_KEY_SLOP;
  if (inside((rect){ MENU_LEFT.x - k, MENU_LEFT.y - k, MENU_LEFT.w + 2 * k, MENU_LEFT.h + 2 * k }, x, y)) return -1;
  if (inside((rect){ MENU_RIGHT.x - k, MENU_RIGHT.y - k, MENU_RIGHT.w + 2 * k, MENU_RIGHT.h + 2 * k }, x, y)) return 1;
  if (inside(MENU_BACK, x, y)) return HUD_MENU_BACK;
  if (inside(MENU_OPTION, x, y)) return HUD_MENU_OPTION;
  if (inside(MENU_HIDDEN, x, y)) return HUD_MENU_HIDDEN;
  return 0;
}

void hud_draw_menu_option(float screen_w, float screen_h, int held) {
  glUseProgram(prog);
  mat4 proj = screen_ortho(screen_w, screen_h);
  glUniformMatrix4fv(u_proj, 1, GL_FALSE, proj.m);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);
  glEnableVertexAttribArray(0);
  button(MENU_OPTION, held);
  /* a gear: eight teeth round a ring */
  color(1, 1, 1, 0.92f);
  float cx = MENU_OPTION.x + MENU_OPTION.w / 2, cy = MENU_OPTION.y + MENU_OPTION.h / 2;
  ring(cx, cy, 5, 13);
  for (int k = 0; k < 8; ++k) {
    float a = (float)k * 0.785398f, c = cosf(a), s = sinf(a), w = 4.0f;
    float x0 = cx + c * 11, y0 = cy + s * 11, x1 = cx + c * 18, y1 = cy + s * 18;
    triangle(x0 - s * w, y0 + c * w, x1 - s * w, y1 + c * w, x1 + s * w, y1 - c * w);
    triangle(x0 - s * w, y0 + c * w, x1 + s * w, y1 - c * w, x0 + s * w, y0 - c * w);
  }
  glDisable(GL_BLEND);
}

void hud_draw_menu_dress(float screen_w, float screen_h, int held) {
  glUseProgram(prog);
  mat4 proj = screen_ortho(screen_w, screen_h);
  glUniformMatrix4fv(u_proj, 1, GL_FALSE, proj.m);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);
  glEnableVertexAttribArray(0);
  button(MENU_OPTION, held);
  /* a wheel: the rim, the hub and five spokes */
  color(1, 1, 1, 0.92f);
  float cx = MENU_OPTION.x + MENU_OPTION.w / 2, cy = MENU_OPTION.y + MENU_OPTION.h / 2;
  ring(cx, cy, 15, 19);
  ring(cx, cy, 2, 5);
  for (int k = 0; k < 5; ++k) {
    float a = (float)k * 1.256637f - 1.570796f, c = cosf(a), s = sinf(a), w = 1.8f;
    float x0 = cx + c * 4, y0 = cy + s * 4, x1 = cx + c * 16, y1 = cy + s * 16;
    triangle(x0 - s * w, y0 + c * w, x1 - s * w, y1 + c * w, x1 + s * w, y1 - c * w);
    triangle(x0 - s * w, y0 + c * w, x1 + s * w, y1 - c * w, x0 + s * w, y0 - c * w);
  }
  glDisable(GL_BLEND);
}

void hud_draw_menu_hidden(float screen_w, float screen_h, int held) {
  glUseProgram(prog);
  mat4 proj = screen_ortho(screen_w, screen_h);
  glUniformMatrix4fv(u_proj, 1, GL_FALSE, proj.m);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);
  glEnableVertexAttribArray(0);
  button(MENU_HIDDEN, held);
  glDisable(GL_BLEND);
}

void hud_draw_menu_erase(float screen_w, float screen_h, int held, int armed) {
  glUseProgram(prog);
  mat4 proj = screen_ortho(screen_w, screen_h);
  glUniformMatrix4fv(u_proj, 1, GL_FALSE, proj.m);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);
  glEnableVertexAttribArray(0);
  button(MENU_OPTION, held);
  /* a bin: the lid and its handle, the body with three slots (red once armed) */
  if (armed) color(1, 0.25f, 0.2f, 0.95f);
  else color(1, 1, 1, 0.92f);
  float cx = MENU_OPTION.x + MENU_OPTION.w / 2, cy = MENU_OPTION.y + MENU_OPTION.h / 2;
  fill_rect(cx - 6, cy - 21, 12, 3);
  fill_rect(cx - 17, cy - 18, 34, 4);
  fill_rect(cx - 13, cy - 11, 3, 29);
  fill_rect(cx + 10, cy - 11, 3, 29);
  fill_rect(cx - 13, cy + 15, 26, 3);
  for (int k = -1; k <= 1; ++k) fill_rect(cx + (float)k * 6 - 1, cy - 7, 2.5f, 19);
  glDisable(GL_BLEND);
}

void hud_draw_menu_keys(float screen_w, float screen_h, int held, int keys, int back) {
  glUseProgram(prog);
  mat4 proj = screen_ortho(screen_w, screen_h);
  glUniformMatrix4fv(u_proj, 1, GL_FALSE, proj.m);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);
  glEnableVertexAttribArray(0);
  if (keys) {
    button(MENU_LEFT, held < 0);
    button(MENU_RIGHT, held == 1);
    color(1, 1, 1, 0.92f);
    chevron(MENU_LEFT.x + MENU_LEFT.w / 2 - 6, MENU_LEFT.y + MENU_LEFT.h / 2, 22, -1, 0);
    chevron(MENU_RIGHT.x + MENU_RIGHT.w / 2 + 6, MENU_RIGHT.y + MENU_RIGHT.h / 2, 22, 1, 0);
  }
  if (back) {
    /* a return arrow: shaft and hook, with a chevron head pointing left */
    button(MENU_BACK, held == HUD_MENU_BACK);
    color(1, 1, 1, 0.92f);
    float cx = MENU_BACK.x + MENU_BACK.w / 2, cy = MENU_BACK.y + MENU_BACK.h / 2;
    chevron(cx - 12, cy + 6, 16, -1, 0);
    fill_rect(cx - 14, cy + 4, 26, 4);
    fill_rect(cx + 10, cy - 12, 4, 20);
    fill_rect(cx - 2, cy - 12, 16, 4);
  }
  glDisable(GL_BLEND);
}

/* 5x7 glyphs, a row a byte (bit 4 the left column), for ' ' .. '_' */
static const unsigned char FONT5X7[64][7] = {
  { 0 }, { 4, 4, 4, 4, 4, 0, 4 }, { 10, 10, 0, 0, 0, 0, 0 }, { 10, 31, 10, 10, 10, 31, 10 },     /*  !"# */
  { 4, 15, 20, 14, 5, 30, 4 }, { 24, 25, 2, 4, 8, 19, 3 }, { 8, 20, 20, 8, 21, 18, 13 }, { 4, 4, 0, 0, 0, 0, 0 }, /* $%&' */
  { 2, 4, 8, 8, 8, 4, 2 }, { 8, 4, 2, 2, 2, 4, 8 }, { 0, 21, 14, 31, 14, 21, 0 }, { 0, 4, 4, 31, 4, 4, 0 },     /* ()*+ */
  { 0, 0, 0, 0, 12, 4, 8 }, { 0, 0, 0, 31, 0, 0, 0 }, { 0, 0, 0, 0, 0, 12, 12 }, { 1, 1, 2, 4, 8, 16, 16 },     /* ,-./ */
  { 14, 17, 19, 21, 25, 17, 14 }, { 4, 12, 4, 4, 4, 4, 14 }, { 14, 17, 1, 2, 4, 8, 31 }, { 31, 2, 4, 2, 1, 17, 14 }, /* 0123 */
  { 2, 6, 10, 18, 31, 2, 2 }, { 31, 16, 30, 1, 1, 17, 14 }, { 6, 8, 16, 30, 17, 17, 14 }, { 31, 1, 2, 4, 8, 8, 8 }, /* 4567 */
  { 14, 17, 17, 14, 17, 17, 14 }, { 14, 17, 17, 15, 1, 2, 12 }, { 0, 12, 12, 0, 12, 12, 0 }, { 0, 12, 12, 0, 12, 4, 8 }, /* 89:; */
  { 2, 4, 8, 16, 8, 4, 2 }, { 0, 0, 31, 0, 31, 0, 0 }, { 8, 4, 2, 1, 2, 4, 8 }, { 14, 17, 1, 2, 4, 0, 4 },       /* <=>? */
  { 14, 17, 1, 13, 21, 21, 14 }, { 14, 17, 17, 31, 17, 17, 17 }, { 30, 17, 17, 30, 17, 17, 30 }, { 14, 17, 16, 16, 16, 17, 14 }, /* @ABC */
  { 28, 18, 17, 17, 17, 18, 28 }, { 31, 16, 16, 30, 16, 16, 31 }, { 31, 16, 16, 30, 16, 16, 16 }, { 14, 17, 16, 23, 17, 17, 15 }, /* DEFG */
  { 17, 17, 17, 31, 17, 17, 17 }, { 14, 4, 4, 4, 4, 4, 14 }, { 7, 2, 2, 2, 2, 18, 12 }, { 17, 18, 20, 24, 20, 18, 17 }, /* HIJK */
  { 16, 16, 16, 16, 16, 16, 31 }, { 17, 27, 21, 21, 17, 17, 17 }, { 17, 17, 25, 21, 19, 17, 17 }, { 14, 17, 17, 17, 17, 17, 14 }, /* LMNO */
  { 30, 17, 17, 30, 16, 16, 16 }, { 14, 17, 17, 17, 21, 18, 13 }, { 30, 17, 17, 30, 20, 18, 17 }, { 15, 16, 16, 14, 1, 1, 30 }, /* PQRS */
  { 31, 4, 4, 4, 4, 4, 4 }, { 17, 17, 17, 17, 17, 17, 14 }, { 17, 17, 17, 17, 17, 10, 4 }, { 17, 17, 17, 21, 21, 21, 10 }, /* TUVW */
  { 17, 17, 10, 4, 10, 17, 17 }, { 17, 17, 10, 4, 4, 4, 4 }, { 31, 1, 2, 4, 8, 16, 31 }, { 14, 8, 8, 8, 8, 8, 14 }, /* XYZ[ */
  { 16, 16, 8, 4, 2, 1, 1 }, { 14, 2, 2, 2, 2, 2, 14 }, { 4, 10, 17, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 31 },     /* \]^_ */
};

/* the UI's quads, batched: two triangles each */
enum { UI_QUADS = 2048 };
static float ui_v[UI_QUADS * 12];
static int ui_n;
static float ui_rgba[4];

static void ui_flush(void) {
  if (!ui_n) return;
  color(ui_rgba[0], ui_rgba[1], ui_rgba[2], ui_rgba[3]);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, ui_v);
  glDrawArrays(GL_TRIANGLES, 0, ui_n * 6);
  ui_n = 0;
}

static void ui_color(float r, float g, float b, float a) {
  if (ui_n && (ui_rgba[0] != r || ui_rgba[1] != g || ui_rgba[2] != b || ui_rgba[3] != a)) ui_flush();
  ui_rgba[0] = r, ui_rgba[1] = g, ui_rgba[2] = b, ui_rgba[3] = a;
}

static void ui_quad(float x, float y, float w, float h) {
  if (ui_n == UI_QUADS) ui_flush();
  float *v = ui_v + ui_n++ * 12;
  v[0] = x, v[1] = y, v[2] = x + w, v[3] = y, v[4] = x, v[5] = y + h;
  v[6] = x + w, v[7] = y, v[8] = x + w, v[9] = y + h, v[10] = x, v[11] = y + h;
}

void hud_ui_begin(float screen_w, float screen_h) {
  glUseProgram(prog);
  mat4 proj = screen_ortho(screen_w, screen_h);
  glUniformMatrix4fv(u_proj, 1, GL_FALSE, proj.m);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);
  glEnableVertexAttribArray(0);
  ui_n = 0;
}

void hud_ui_end(void) {
  ui_flush();
  glDisable(GL_BLEND);
}

void hud_ui_rect(float x, float y, float w, float h, float r, float g, float b, float a) {
  ui_color(r, g, b, a);
  ui_quad(x, y, w, h);
}

void hud_ui_button(float x, float y, float w, float h, int held) {
  ui_flush();
  button((rect){ x, y, w, h }, held);
}

static int glyph(char c) {
  if (c >= 'a' && c <= 'z') c = (char)(c - 32);
  return c >= ' ' && c <= '_' ? c - ' ' : '?' - ' ';
}

float hud_ui_text_width(const char *s, float px) {
  int n = 0;
  for (; *s; ++s) n++;
  return n ? (float)(6 * n - 1) * px : 0.0f;
}

float hud_ui_text(const char *s, float x, float y, float px, float r, float g, float b, float a) {
  /* a shadow a pixel down and right, then the glyphs */
  for (int pass = 0; pass < 2; ++pass) {
    float o = pass ? 0.0f : fmaxf(1.0f, px * 0.35f), cx = x + o;
    if (pass) ui_color(r, g, b, a);
    else ui_color(0, 0, 0, a * 0.6f);
    for (const char *c = s; *c; ++c, cx += 6 * px) {
      const unsigned char *gl = FONT5X7[glyph(*c)];
      for (int row = 0; row < 7; ++row)
        for (int col = 0; col < 5; ++col)
          if (gl[row] & (16 >> col)) ui_quad(cx + (float)col * px, y + o + (float)row * px, px, px);
    }
  }
  return x + hud_ui_text_width(s, px) + px;
}

void hud_lift_parts(float cluster, float plate, float pill, float map, float gate) {
  CLUSTER_Y = 286 - cluster;
  INFO_Y = 505 - plate;
  PILL_UP = pill;
  MAP_UP = map;
  SHIFT_Y -= gate;
}

void hud_lift(float dy, float left) {
  hud_lift_parts(dy, dy, dy, left, left);
  /* the back mirror is on here: the top keys either side of it again, as on the iPad */
  PAUSE.x = 236, RESET.x = 296, CAMERA.x = 116, TRANS.x = 176;
}

/* ---- the player's own key layout (hud.h hud_layout_set) ---- */
/* a group's keys' box in the layout before key() (the steer bar over the left side's) */
static rect group_rect(int group, int gyro) {
  if (group == KEY_LEFT) return (rect){ LEFT.x, LEFT.y - 14, RIGHT.x + RIGHT.w - LEFT.x, LEFT.h + 14 };
  if (group == KEY_RIGHT) {
    float x = gyro ? G_SHIFT_UP.x : SHIFT_UP.x;
    return (rect){ x, THROTTLE.y, THROTTLE.x + THROTTLE.w - x, BRAKE.y + BRAKE.h - THROTTLE.y };
  }
  return (rect){ CAMERA.x, CAMERA.y, RESET.x + RESET.w - CAMERA.x, CAMERA.h };
}

void hud_layout_set(int group, float k, float dx, float dy) {
  if (group < 0 || group >= HUD_KEY_GROUPS) return;
  USER_K[group] = k < HUD_KEY_K_MIN ? HUD_KEY_K_MIN : k > HUD_KEY_K_MAX ? HUD_KEY_K_MAX : k;
  USER_DX[group] = dx, USER_DY[group] = dy;
}

int hud_layout_hit(int gyro, float x, float y) {
  for (int g = 0; g < HUD_KEY_GROUPS; ++g) {
    rect r = key(group_rect(g, gyro), g);
    if (x >= r.x - 10 && x <= r.x + r.w + 10 && y >= r.y - 10 && y <= r.y + r.h + 10) return g;
  }
  return -1;
}

void hud_layout_clamp(int group, int gyro, float k, float *dx, float *dy) {
  if (group < 0 || group >= HUD_KEY_GROUPS) return;
  k = k < HUD_KEY_K_MIN ? HUD_KEY_K_MIN : k > HUD_KEY_K_MAX ? HUD_KEY_K_MAX : k;
  rect r = place(group_rect(group, gyro), group, k, *dx, *dy);
  if (r.x + r.w > 1024 + EDGE) *dx -= r.x + r.w - (1024 + EDGE);
  if (r.x < -EDGE) *dx += -EDGE - r.x;
  if (r.y + r.h > 768) *dy -= r.y + r.h - 768;
  if (r.y < 0) *dy -= r.y;
}

void hud_draw_layout(float screen_w, float screen_h, int gyro, int sel) {
  hud_state h;
  hud_init(&h, screen_w, screen_h);
  h.gyro = gyro;
  glUseProgram(prog);
  mat4 proj = screen_ortho(screen_w, screen_h);
  glUniformMatrix4fv(u_proj, 1, GL_FALSE, proj.m);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);
  glEnableVertexAttribArray(0);
  draw_keys(&h, 0, 1);
  glUniformMatrix4fv(u_proj, 1, GL_FALSE, proj.m);
  /* each group's box, the chosen one's orange */
  for (int g = 0; g < HUD_KEY_GROUPS; ++g) {
    rect r = key(group_rect(g, gyro), g);
    r = (rect){ r.x - 6, r.y - 6, r.w + 12, r.h + 12 };
    if (g == sel) color(1.0f, 0.42f, 0.05f, 0.95f);
    else color(1, 1, 1, 0.35f);
    frame_rect(r, g == sel ? 4 : 2);
  }
  glDisable(GL_BLEND);
}

void hud_wide(float edge, float key_k) {
  EDGE = edge;
  KEY_K = key_k;
  CLUSTER_X = 948 + edge;
  SHIFT_X = 62 - edge;
  SHIFT_Y = 530 - 0.5f * (768 - 604) * (key_k - 1); /* between the map and the bigger ◀ ▶ */
  TA_X = -edge;
  if (edge > 0) {
    PAUSE.x = 386, RESET.x = 454, CAMERA.x = 250, TRANS.x = 318;
  }
}
