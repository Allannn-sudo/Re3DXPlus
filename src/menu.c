#include "menu.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "anm.h"
#include "battle.h"
#include "car.h"
#include "cardb.h"
#include "gl.h"
#include "hud.h"
#include "storypre.h"
#include "storysel.h"
#include "tendb.h"
#include "sttune.h"
#include "steff.h"
#include "dressdb.h"
#include "namedb.h"
#include "mathx.h"
#include "sound.h"
#include "title.h"

/* Frame layout of the arcade's animations (found by stepping through them):
 * - menu_g_topbar_bar / menu_g_bottombar_bar: 0..159 idle, 160..170 slide out, 189..199 slide in.
 * - menu_layout_ta_carsel (clSeqTaModeSelectThread's clMenuSpinBoxStd, its defaults): 100
 *   frames an item k: k*100 + 0..59 idle (looped), k*100 + 60..69 decided (held), k*100 + 90..94
 *   the slide on to k + 1 (backwards going back). menu_layout_ta_carsel_ring: the ring's pulse
 *   k*100 + 0..59, once each time the selection comes to rest (then hidden).
 * - menu_layout_mode_sel (clSeqModeSelectThread's spin box with a card: story, ghost versus
 *   battle, 10 out run, time attack), menu_layout_mode_image (the mode's picture and line) and
 *   menu_layout_mode_ring: as menu_layout_ta_carsel.
 * - menu_layout_ta_course: 100 frames per course k: k*100 idle, k*100+61..75 decided,
 *   k*100+90..99 selection moves on to k+1.
 * - menu_layout_ta_course_arrow: k*100+10..49 the arrows' pulse for course k. */
static struct {
  int title, bg, line, logo, top, bottom, mode, mode_ring, course, arrow, course_map;
  int ten_timeup, ten_face, ten_x, ten_finish, ten_summary; /* clTenResult */
  int game, game_image, game_ring; /* the game mode select */
  int vs_sel, vs_sel_image, vs_sel_ring; /* the versus select */
  int vs_course, vs_icon, vs_icon_omakase, vs_icon_omakase2, vs_ex_player, timer; /* the area vote */
  int shogo_bg, pre_shadow[3], pre_cars[3], pre_vs[3], plate, plate_ph; /* the cars flying in (MENU_VS_MATCH) */
  int maker[3], pre_bg, ranking_bg, ranking;
  int prologue[2];
  /* the car select */
  int bg_carsel, maker_layout, maker_car, car_info, car_arrow, car_new, mes_r35, mes_jzs161;
  int col_strip[4], col_arrow[4], col_name; /* strips for 4, 5 and 6 colours, and one (the hidden taxis') */
  int mes_maker, mes_car, mes_color;
  /* the car setting's wheel prompt */
  int set_handle;
  /* the time zone and BGM select */
  int day_cards, day_ring, day_sound, day_vs, day_vs_ring;
  /* the race's title page */
  int pre_ta, pre_sound;
  int pre_st, pre_sound_st; /* a story race's title */
  /* the story's level select (its layout, arrows, CLEARED marks, ring, the line under the
   * episodes, the episodes by level) and rival select (its layout, arrows, the chapter's and
   * start ramp's names, the episodes by level, the odd and even title glyphs) */
  int st_level, st_level_arrow, st_level_clear, st_level_ring, st_lv_back, st_lv[4];
  int st_rival, st_rival_arrow, st_rival_ramp, st_ep[4][2];
  /* the story's tuning screen: its layout, the tuner's lines, the background, the gauges'
   * cells (lit, the frames), the P and H markers */
  int st_tune, st_tune_serifu, bg_st_tune, gage, gage_waku, gage_waku2, ph;
  /* dress-up: the backgrounds, the bar without time, the GET screen's title and part, the aero
   * change's layouts by its number of choices */
  int tg_bg1, tg_bg3, top_notime, tg_get_title, tg_get_parts, tg_aero, tg_aero2, tg_aero3, tg_aero5;
  int tg_get_image, tg_get_wheel;
  /* the bonnet's, wing's (5 and 6 places) and mirror's change layouts and their arrows */
  int tg_bonnet, tg_bonnet_arrow, tg_wing5, tg_wing5_arrow, tg_wing6, tg_wing6_arrow, tg_mirror, tg_mirror_arrow;
  /* the neons' change layout and its arrows */
  int tg_neon, tg_neon_arrow;
  /* the stickers' change: the bar, the colour column and the arrows; the gem cars' bar */
  int tg_vinyl, tg_vinyl_col, tg_vinyl_arrow, tg_vinyl_gem;
  /* the part worn's title, its name's banner, the white flash */
  int tg_wear_title, tg_wear_logo, whiteout;
  /* the wheel's change: the bar and its arrows by maker (wheel_maker_to_cursor_anim,
   * wheel_maker_to_arrow_anim), the colour column's swatches, cursor, name logo and frame;
   * the level 63 part's COMPLETE */
  int tg_wheel_bar[5], tg_wheel_arrow[5], tg_wheel_col, tg_wheel_cursor, tg_wheel_logo, tg_wheel_waku, tg_complete;
  /* the dress-up menu's places (the gem cars' fewer) and the wheel maker select's
   * pictures and places */
  int tg_all[2], wmaker_bg, wmaker_sel;
  /* the card's first page: its frame and its fields */
  int card_waku, card_layout;
  int loading;
  int finish[2]; /* the race's end: TIME UP, FINISH */
  int win, lose;  /* a story race's end */
  /* its result's advantage row and tuning points: the gauge's frame, the gauge, the title / message */
  int st_adv, st_waku, st_bar, st_moji;
  /* clTaResult's rows: the course, the course's, the car type's and the player's best, the
   * sections, TOTAL TIME, the top speed, the ranking */
  int res_course, res_best[3], res_congrats, res_sections, res_sections2, res_total, res_speed, res_rank;
  /* the 10-in-a-row's course cards, level boxes, their ring, arrows and faces */
  int or_course, or_lv, or_lv_ring, or_lv_arrow, or_lv_face, pre_or;
  /* the class-up screen: its backgrounds and layout; the story's tuning work: its top bar,
   * text boxes (power, handling), complete pictures and install animes */
  int shogo_bg1, shogo_bg2, lvup;
  /* the title select: the back's letters, TITLE, the new titles' frames (one, two, three),
   * the rewrite and the spin box */
  int shogo_moji, shogo_title, shogo_n[3], shogo_rw, yn_rank;
  int eff_top, eff_result, eff_handle, eff_complete, eff_complete2, eff_install[13];
  /* the player's name: the grid's frame, the name's band, its lines (each, the next one's
   * blink), the page tabs and END */
  int name_select, name_back, name_use, name_line, name_tab[5];
  /* the versus stars: background, logo, car, title row, wins, the stars held; the stars won
   * (1..3), their carry (keta_a, keta_b by the ones left), the big stars' finish (1, 100,
   * 1000, 10000) and the stars lost (1, 2) */
  int star_bg, star_logo, star_car, star_title, star_wins, star_use;
  int star_add[3], star_keta_a[3], star_keta_b[3], star_finish[4], star_minus[2];
} ID;

#define N_COURSES CAR_COURSES /* the course cards (car.h; car_course_id their course ids) */
#define MOVE_MODE 4   /* clMenuSpinBoxStd: 1/5 of the way a frame, at rest on the fifth */
#define DECIDE_MODE 10
#define MOVE_COURSE 10
#define DECIDE_COURSE 15
#define FADE 20
/* NOW LOADING from the VS title page's bottom right to the middle of the car select's carousel */
#define LOADING_DX -205.0f
#define LOADING_DY -205.0f
/* and on the race title (clSeqNowLoading::run: setAnime(pre_vs_loading, 4, 1, 20), its
 * pop-in, then held) to the bottom left, as far in from the edge as the arcade's is from the
 * right; it breathes while it waits and fades out once the loading is done */
#define LOADING_PRE_DX -409.0f
#define LOADING_POP 20
#define LOADING_OUT 15

/* mseqDemo: clSeqTitleThread runs 600 frames, clRanking 600 (its clArg), each maker logo of
 * seqAtructCi 240 */
#define TITLE_LEN 600
#define RANKING_LEN 600
#define LOGO_LEN 240
/* seqPrologue after the logos: atract_g_02 ("This game is a work of fiction..."), then with
 * the Japanese locale (0x9735384 == 1) atract_g_01, the story; each plays its 120 frames */
#define PROLOGUE_LEN 120
/* pressing start: white in, then the mode select shows through the fading white */
#define START_LEN 20
#define FLASH_LEN 30

/* ---- the title screen's clock (atract_p_title_time_waku) ----
 * Its digits are templates showing '89.01.23 23:45:67: year, month, day, hour, minute and
 * second, two each. The arcade puts its own clock in them; here the iPad's, with the
 * title's serial number digits (atract_p_title_sn_0..9). */
static const int CLOCK_TEMPLATE[12] = { 7415, 7416, 7617, 7618, 7619, 7620, 7406, 7410, 7411, 7412, 7413, 7414 };
#define T_TITLE_DIGIT 2594

static void draw_title(int frame, float alpha) {
  time_t now = time(NULL);
  const struct tm *tm = localtime(&now);
  int v[6] = { tm->tm_year % 100, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec };
  anm_swap clock[12];
  for (int i = 0; i < 12; ++i) {
    clock[i].from = anm_sprite(CLOCK_TEMPLATE[i]);
    clock[i].to = anm_sprite(T_TITLE_DIGIT + (i & 1 ? v[i / 2] % 10 : v[i / 2] / 10));
  }
  anm_draw_ex(ID.title, frame, alpha, 0, 0, clock, 12);
}

/* ---- the time attack ranking (clRanking) ---- */

/* nsMenuCheck::taCourseIDFromMenuOrder; clRanking::run skips course ids 5, 7 and 11 */
static const int ORDER_TO_COURSE[15] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 14, 10, 11, 12, 13 };
/* nsTaData::getBgSprite and getCourceNameSprite, by course id */
static const int COURSE_BG[15] = { 2863, 2866, 2869, 2865, 2870, 2873, 2880, 2880, 2357, 2355, 2885, 2885, 3930, 6244, 2359 };
static const int COURSE_NAME[15] = { 3931, 3932, 3933, 3934, 5609, 3936, 5610, 3937, 3944, 3939, 5611, 3941, 3942, 3943, 5612 };

/* ---- the course cards' distances (clThread_6_2_CourseSelect's constructor) ---- */

/* clEffTaCourseType::getTaDist by course id: the length in mm yt::TARaceFieldAry gives each
 * time attack stage (0x9778900); ids 5, 7 and 11 have no card */
static const float COURSE_LEN[15] = { 13771708.0f, 13815960.0f, 17464274.0f, 17643834.0f, 22491096.0f, 0, 15103232.0f, 0,
                                      14938780.0f, 18659480.0f, 15790000.0f, 0, 59798504.0f, 38178028.0f, 16810000.0f };
/* menu_p_course_no_N_template by menu order: tens, ones and tenths of a km (0x93e3a40) */
static const int KM_TEMPLATE[15][3] = {
  { 7031, 7032, 7033 }, { 7034, 7035, 7036 }, { 7037, 7038, 7039 }, { 7040, 7226, 7227 }, { 7228, 7229, 7230 },
  { 7231, 7232, 7233 }, { 7234, 7235, 7236 }, { 7237, 7238, 7239 }, { 7256, 7257, 7258 }, { 7240, 7241, 7242 },
  { 7259, 7260, 7261 }, { 7243, 7244, 7245 }, { 7246, 7247, 7248 }, { 7249, 7250, 7251 }, { 7252, 7253, 7254 },
};
#define KM_DIGIT 2239 /* menu_p_course_no_0..9 (0x93e3b00) */
static anm_swap km_swaps[15 * 3];
static int km_nswaps;

/* setImageReplace of each card's digits, with the arcade's float maths: a zero tens digit
 * is blanked */
static void km_init(void) {
  km_nswaps = 0;
  for (int order = 0; order < 15; ++order) {
    float len = COURSE_LEN[ORDER_TO_COURSE[order]];
    if (len <= 0) continue;
    int km = (int)(len * 1e-6f), tenths = (int)(len * 1e-5f);
    int digit[3] = { km / 10, km % 10, tenths % 10 };
    for (int k = 0; k < 3; ++k) {
      anm_swap *sw = &km_swaps[km_nswaps++];
      sw->from = anm_sprite(KM_TEMPLATE[order][k]);
      sw->to = k == 0 && digit[0] == 0 ? -1 : anm_sprite(KM_DIGIT + digit[k]);
    }
  }
}

/* A fresh machine's top ten: clTimeAttackRankingDataManager::load fills every course from
 * clRankingDefault::data_ (records 5, 4, 3, 2, 1, 0, 46, 45, 44, 43 in insertion order),
 * all ＧＵＥＳＴ, title 1 (湾岸の新人), class N, untuned (handling B) at 9'59"999.
 * Car name sprite = get_car_model_data(model)+0x2c, horsepower = GetCarData(car)+0x20. */
static const struct { int car_sprite, hp; } DEFAULT_ROWS[10] = {
  { 7627, 205 }, /* FC3S */
  { 7801, 260 }, /* UCF10 */
  { 7439, 240 }, /* CXD */
  { 7448, 280 }, /* Z33 */
  { 7636, 280 }, /* Z16A */
  { 7798, 280 }, /* JCESE */
  { 7952, 120 }, /* KZH100G */
  { 7905, 120 }, /* NZE121 */
  { 7907, 280 }, /* JZS161 */
  { 7801, 260 }, /* UCF10 */
};
#define DEFAULT_TIME_MS 599999

/* clRankingPlate::InitForHorsePowerDisp: the template sprites of menu_layout_ranking it replaces */
enum {
  T_WAKU = 7308, T_RANK10 = 3353, T_RANK1 = 7527, T_NAME = 2656,
  T_TIME = 7517, /* 7517..7523: M M ' S S " m m m digits */
  T_CAR = 7450, T_HP = 7524, /* 7524..7526 */
  T_HANDLING = 7481, T_CLASS = 7528, /* 7528..7530 */
  T_TITLE = 1408,
};
#define TEXT_GUEST 100000 /* menupack.py TEXTS */
#define TEXT_TITLE 100001

static struct {
  int digit[10]; /* menu_p_ranking_time0..9 */
  int waku, handling_b, class_n, name_waku, guest, title;
  int car[10];
  int t_waku, t_rank10, t_rank1, t_name, t_time[7], t_car, t_hp[3], t_handling, t_class[3], t_title;
  int bg_template, name_template, waku_template;
} R;

/* The arcade ranks a new course each round (seqRanking(round % 15)), skipping ids 5, 7 and
 * 11 (no card). */
static int course_for(int demo) {
  for (int i = 0; i < 15; ++i) {
    int id = ORDER_TO_COURSE[(demo + i) % 15];
    if (id != 5 && id != 7 && id != 11) return id;
  }
  return 0;
}

static void ranking_init(void) {
  for (int i = 0; i < 10; ++i) {
    R.digit[i] = anm_sprite(3352 + i);
    R.car[i] = anm_sprite(DEFAULT_ROWS[i].car_sprite);
  }
  R.waku = anm_sprite(4062);
  R.handling_b = anm_sprite(2718);
  R.class_n = anm_sprite(3349);
  R.name_waku = anm_sprite(0x1ba8); /* title_waku_2: a course with over 99 records */
  R.guest = anm_sprite(TEXT_GUEST);
  R.title = anm_sprite(TEXT_TITLE);
  R.t_waku = anm_sprite(T_WAKU);
  R.t_rank10 = anm_sprite(T_RANK10);
  R.t_rank1 = anm_sprite(T_RANK1);
  R.t_name = anm_sprite(T_NAME);
  for (int i = 0; i < 7; ++i) R.t_time[i] = anm_sprite(T_TIME + i);
  R.t_car = anm_sprite(T_CAR);
  for (int i = 0; i < 3; ++i) {
    R.t_hp[i] = anm_sprite(T_HP + i);
    R.t_class[i] = anm_sprite(T_CLASS + i);
  }
  R.t_handling = anm_sprite(T_HANDLING);
  R.t_title = anm_sprite(T_TITLE);
  R.bg_template = anm_sprite(0xb2f);
  R.name_template = anm_sprite(0x1b9a);
  R.waku_template = anm_sprite(0x1f45);
}

/* one plate of menu_layout_ranking, as InitForHorsePowerDisp fills it */
static void draw_plate(int rank, int frame, float alpha) {
  anm_swap sw[24];
  int n = 0, ms = DEFAULT_TIME_MS, hp = DEFAULT_ROWS[rank - 1].hp;
  sw[n++] = (anm_swap){ R.t_waku, R.waku };
  sw[n++] = (anm_swap){ R.t_rank10, rank > 9 ? R.digit[rank / 10 % 10] : -1 };
  sw[n++] = (anm_swap){ R.t_rank1, R.digit[rank % 10] };
  sw[n++] = (anm_swap){ R.t_name, R.guest };
  const int t_digits[7] = { ms / 600000, ms % 600000 / 60000, ms % 60000 / 10000, ms % 10000 / 1000,
                            ms % 1000 / 100, ms % 100 / 10, ms % 10 };
  for (int i = 0; i < 7; ++i)
    sw[n++] = (anm_swap){ R.t_time[i], i == 0 && ms < 600000 ? -1 : R.digit[t_digits[i]] };
  sw[n++] = (anm_swap){ R.t_car, R.car[rank - 1] };
  sw[n++] = (anm_swap){ R.t_hp[0], hp > 99 ? R.digit[hp / 100 % 10] : -1 };
  sw[n++] = (anm_swap){ R.t_hp[1], hp > 9 ? R.digit[hp / 10 % 10] : -1 };
  sw[n++] = (anm_swap){ R.t_hp[2], R.digit[hp % 10] };
  sw[n++] = (anm_swap){ R.t_handling, R.handling_b };
  /* class N: (none, none, N) from the table at 0x93f2300 */
  sw[n++] = (anm_swap){ R.t_class[0], -1 };
  sw[n++] = (anm_swap){ R.t_class[1], -1 };
  sw[n++] = (anm_swap){ R.t_class[2], R.class_n };
  sw[n++] = (anm_swap){ R.t_title, R.title };
  /* plate k sits 36 px under plate k-1 (clSpriteAnime::setPosition) */
  anm_draw_ex(ID.ranking, frame, alpha, 0, 36.0f * (float)(rank - 1), sw, n);
}

/* A phone's 16:9 (menu_wide): a course's photo behind a screen, menu_g_pre_bg, goes the
 * screen's width about its middle, its top and bottom off the screen, in place of app.c's
 * stretched edges (menu_photo_bg) */
static float WIDE_ZOOM = 1.0f;
static int photo_bg;

void menu_wide(float edge) { WIDE_ZOOM = (1024.0f + 2.0f * edge) / 1024.0f; }
int menu_photo_bg(void) { return photo_bg; }

static void draw_photo_bg(const anm_swap *bg, float fade) {
  float z = WIDE_ZOOM;
  anm_draw_xf(ID.pre_bg, 0, fade, 320.0f * (1.0f - z), 240.0f * (1.0f - z), z, bg, 1);
  photo_bg = 1;
}

/* a screen's back (menu_g_bg_image, the white one most screens have; the car select's, the
 * story tune's) stretched across a 16:9 screen's width, rows kept so its band stays behind the
 * cards, in place of app.c's stretched edges */
static void draw_back(int anim, int frame, float fade) {
  float z = WIDE_ZOOM;
  anm_draw_xy(anim, frame, fade, 320.0f * (1.0f - z), 0, z, 1, NULL, 0);
  if (z > 1.0f) photo_bg = 1;
}

static void draw_ranking(const menu_state *m) {
  int t = m->t;
  float fade = 1.0f;
  if (t < FADE) fade = (float)t / FADE;
  else if (t > RANKING_LEN) fade = 1.0f - (float)(t - RANKING_LEN) / FADE;
  int course = course_for(m->demo);
  anm_swap bg = { R.bg_template, anm_sprite(COURSE_BG[course]) };
  draw_photo_bg(&bg, fade);
  anm_swap head[2] = { { R.name_template, anm_sprite(COURSE_NAME[course]) }, { R.waku_template, R.name_waku } };
  anm_draw_ex(ID.ranking_bg, 0, fade, 0, 0, head, 2);
  /* clRanking::run starts plate k's slide-in (layout frames 11..49) at frame 13 + 2(k-1) */
  for (int k = 1; k <= 10; ++k) {
    int start = 13 + 2 * (k - 1);
    if (t < start) continue;
    int f = 11 + t - start;
    draw_plate(k, f < 49 ? f : 49, fade);
  }
}

/* ---- the car select (clSeqCardMakerSelectThread, clseqCardCarSelectThread,
 * clseqCardColorSelectThread) ----
 * - menu_layout_carsel_maker_ / menu_g_carsel_maker_car_image: 100 frames per maker m,
 *   idle from 100m (and 1 + 100m) looping 59 frames; 80 + 100m .. 83 moves on to m + 1 (the
 *   car image loops 80 + 100m .. 84 meanwhile), played backwards to move back. Input waits
 *   20 frames after a move.
 * - menu_g_carsel_col_0n (+ _arrow): the same for n colours, 6 frames per move.
 * - menu_g_carsel_car: the car's logo, drive and gears, by setImageReplace of templates. */
#define MAKER_MOVE 4
#define MAKER_LOCK 20
#define COLOR_MOVE 6
enum {
  T_CAR_LOGO = 7911, T_CAR_DRIVE = 7910, T_CAR_SPEED = 7683, T_BG_LOGO = 6958,
  T_SWATCH = 7650, /* 7650 + n - i holds colour i of n (the table at 0x93f7a80) */
  T_COL_FONT = 890, /* the colour name's place in menu_g_carsel_col_name */
};
/* maker buttons on menu_layout_carsel_maker_: 128 px sprites from x = 26, 92 apart, at y 286,
 * the button itself the middle 84 px */
#define MAKER_X0 48.0f
#define MAKER_DX 92.0f
#define MAKER_W 84.0f
#define MAKER_Y0 312.0f
#define MAKER_Y1 390.0f

/* The carousel of clseqCardCarSelectThread::yield, by maker (DB_XYZ tables at 0x9772fc0):
 * look-at height, centre x, eye x, field of view, radius, -, eye z (offsets on its constants) */
static const float CAROUSEL[CARDB_ALL][7] = {
  { -15.0f, 53.0f, -18.5f, -7.0f, -2.0f, -7.0f, -19.0f },
  { -13.5f, 45.0f, -18.5f, -10.5f, -11.0f, -14.5f, -19.0f },
  { -13.0f, 47.5f, -18.5f, -7.0f, -9.0f, -7.0f, -19.0f },
  { -17.5f, 56.25f, -28.5f, -1.5f, 5.0f, -7.0f, -22.0f },
  { -12.5f, 43.0f, -25.0f, -10.5f, -11.0f, -9.5f, -19.0f },
  { -11.5f, 48.25f, -13.0f, -10.0f, -10.75f, -28.0f, -12.25f },
  { -17.5f, 56.25f, -28.5f, 4.0f, 5.0f, -7.0f, -22.0f }, /* the hidden cars (the port's): TOYOTA's ring (its 12 sedans), wider for the vans (MITSUBISHI's 7 overlapped) */
};
#define TWO_PI 6.2831853f
#define DEG 0.017453292f

static void go(menu_state *m, int screen);
static int vs_hit(const menu_state *m, float x, float y);

static int has(const char *s, const char *word) {
  for (; *s; ++s) {
    const char *a = s, *b = word;
    while (*b && *a == *b) a++, b++;
    if (!*b) return 1;
  }
  return 0;
}

static const cardb_car *chosen_car(const menu_state *m, int car) { return &CARDB_CARS[m->maker][car]; }

static int ncars(const menu_state *m) { return CARDB_NCARS[m->maker]; }

/* the car info bubble (menu_g_carsel_car) and the background logo of `car` */
static int car_swaps(const cardb_car *c, anm_swap *sw) {
  sw[0] = (anm_swap){ anm_sprite(T_CAR_LOGO), anm_sprite(c->logo) };
  sw[1] = (anm_swap){ anm_sprite(T_CAR_DRIVE), anm_sprite(c->drive) };
  sw[2] = (anm_swap){ anm_sprite(T_CAR_SPEED), anm_sprite(c->speed) };
  return 3;
}

static void draw_bg_logo(const cardb_car *c, int frame, float alpha) {
  anm_swap sw = { anm_sprite(T_BG_LOGO), anm_sprite(c->bglogo) };
  anm_draw_ex(ID.logo, frame, alpha, 0, 0, &sw, 1);
}

static int strip_index(int n) { return n == 1 ? 3 : n <= 4 ? 0 : n == 5 ? 1 : 2; }

/* the colour strip's frame for the current state */
static int color_frame(const menu_state *m) {
  if (m->from >= 0) {
    int t = m->t < COLOR_MOVE ? m->t : COLOR_MOVE - 1;
    return m->from < m->sel ? 80 + 100 * m->from + t : 80 + 100 * m->sel + COLOR_MOVE - 1 - t;
  }
  return 100 * m->sel + m->t % 59;
}

/* Which colour is drawn at (x, y): its template's place in the strip's current frame. */
static int color_at(const menu_state *m, float x, float y) {
  const cardb_car *c = chosen_car(m, m->car);
  int strip = ID.col_strip[strip_index(c->ncolors)];
  for (int i = 0; i < c->ncolors; ++i) {
    float r[4];
    if (anm_sprite_rect(strip, color_frame(m), anm_sprite(T_SWATCH + c->ncolors - i), r) &&
        x >= r[0] && x < r[2] && y >= r[1] && y < r[3])
      return i;
  }
  return -1;
}

/* The game mode select's discs (seqModeSelect with a card: setMax 4, modes 0x38..0x3b):
 * story, the ghost versus battle (0x39, only with a card; the port's online versus in its
 * place), 10 out run, time attack */
#define GAME_SLOTS 4
static const int GAME_AT[GAME_SLOTS] = { MENU_GAME_STORY, MENU_GAME_VS, MENU_GAME_TEN, MENU_GAME_TA };

static int game_slot(int game) {
  for (int k = 0; k < GAME_SLOTS; ++k)
    if (GAME_AT[k] == game) return k;
  return 0;
}

static void car_select_enter(menu_state *m, int screen) {
  go(m, screen);
  /* the car screens have the entry music (mseqEntry), the mode select on the select music:
   * back from the mode select, the car's music comes back */
  if (screen <= MENU_SETTING) m->music = 0;
  /* (back from the hidden cars: the maker their key was pressed on) */
  if (screen == MENU_MAKER) m->sel = m->maker < CARDB_MAKERS ? m->maker : m->hidden_from;
  if (screen == MENU_CAR) {
    m->sel = m->car;
    m->rot = (float)m->sel * TWO_PI / (float)ncars(m);
    m->rot_speed = 0;
  }
  if (screen == MENU_COLOR) {
    m->sel = m->color;
    m->spin = -24.0f; /* car_common(true) */
  }
  if (screen == MENU_GAME) {
    m->sel = game_slot(m->game);
    m->music = 1;
  }
  if (screen == MENU_VS_SEL) {
    m->sel = m->vs.kind;
    m->music = 1;
  }
  if (screen == MENU_MODE) {
    m->sel = m->official;
    m->music = 1;
  }
  if (screen == MENU_COURSE) {
    m->sel = m->course;
    m->course_pos = m->sel <= 3 ? m->sel : m->sel >= 9 ? m->sel - 6 : 3;
  }
}

/* one step of the maker, car or colour select; returns 1 when the screen decides */
static int car_select_step(menu_state *m, int tap, int key) {
  if (m->lock) m->lock--;
  int move = m->screen == MENU_MAKER ? MAKER_MOVE : COLOR_MOVE;
  if (m->from >= 0 && m->t >= move) { m->from = -1; m->t = 0; }
  if (m->screen == MENU_COLOR) m->spin -= 0.3f;
  int count = m->screen == MENU_MAKER ? CARDB_MAKERS : m->screen == MENU_CAR ? ncars(m) : chosen_car(m, m->car)->ncolors;
  if (m->screen == MENU_CAR && m->rot_speed != 0.0f) {
    /* the carousel turns 0.3 / n radians a frame until the selected car is in front */
    float target = (float)m->sel * TWO_PI / (float)count;
    m->rot += m->rot_speed * 0.3f / (float)count;
    if ((m->rot_speed > 0 && m->rot >= target) || (m->rot_speed < 0 && m->rot <= target)) {
      m->rot = target;
      m->rot_speed = 0;
    }
    return 0;
  }
  if (m->lock || m->from >= 0 || (!tap && !key)) return 0;
  /* NOW LOADING: the cars on show are not in yet; no other until they are */
  if (m->loading && m->screen != MENU_MAKER) return 0;
  int to = m->sel;
  if (key) {
    to = m->sel + key;
  } else if (m->screen == MENU_MAKER) {
    int k = (int)floorf((m->tap_x - MAKER_X0) / MAKER_DX);
    float in = m->tap_x - MAKER_X0 - MAKER_DX * (float)k;
    if (k < 0 || k >= CARDB_MAKERS || in > MAKER_W || m->tap_y < MAKER_Y0 || m->tap_y > MAKER_Y1) return 0;
    to = k;
  } else if (m->screen == MENU_CAR) {
    to = m->tap_x < 200.0f ? m->sel - 1 : m->tap_x > 440.0f ? m->sel + 1 : m->sel;
  } else {
    to = color_at(m, m->tap_x, m->tap_y);
    if (to < 0) return 0;
  }
  if (to == m->sel && !key) {
    snd_menu(SND_SELECT);
    return 1;
  }
  if (m->screen == MENU_CAR) {
    /* the carousel goes round: past the last car is the first */
    int d = to > m->sel ? 1 : -1;
    if (to >= count) { to = 0; m->rot -= TWO_PI; }
    if (to < 0) { to = count - 1; m->rot += TWO_PI; }
    snd_menu(SND_CURSOR);
    m->rot_speed = (float)d;
    m->sel = to;
    return 0;
  }
  if (to < 0 || to >= count) return 0;
  snd_menu(SND_CURSOR);
  /* a tap further along jumps there; the arcade's buttons only move one step */
  m->from = to == m->sel + 1 || to == m->sel - 1 ? m->sel : -1;
  m->sel = to;
  m->t = 0;
  m->lock = m->screen == MENU_MAKER ? MAKER_LOCK : COLOR_MOVE;
  return 0;
}

/* dress-up's screens: see menu_dress_win */
static int tg_due(menu_state *m);
#define TG_FADE 10
#define TG_GET_END -185.0f
#define TG_GET_SKIP (TG_FADE + 192) /* 3.2 s after the fade in */
#define TG_GET_HOLD 192           /* the picture GET screens' 3.2 s (0x93fba58) after the fade in */
#define TG_IMAGE_FROM 80          /* get_parts_image: frames 80..99, then held */
#define TG_IMAGE_LEN 20
#define TG_WHEEL_FROM 101         /* get_wheel: from frame 101, then held */
#define TG_WHEEL_START -60.0f     /* the wheel's turn: from -60 degrees, 0.3 a frame, to -120 */
#define TG_WHEEL_END -120.0f
/* seqTgPartsGet's GET threads by the part's kind: the wheels' (0), the bonnet's, wing's and
 * mirror's (2..4, clSeqTgPartsGetWingThread) and the car's (the rest, the aero thread) */
enum { TG_GET_CAR, TG_GET_WHEEL, TG_GET_PICTURE };
static int tg_get_type(const menu_state *m) {
  return m->tg.kind == 0 ? TG_GET_WHEEL : m->tg.kind >= 2 && m->tg.kind <= 4 ? TG_GET_PICTURE : TG_GET_CAR;
}
#define TG_MOVE 29
#define TG_DECIDE 9
enum {
  T_TG_TENS = 0x1efe, T_TG_ONES = 0x1efc, T_TG_KIND = 0x1f96, T_TG_PART = 0x1f7c,
  T_TG_PLATE = 0x1f48, S_TG_PLATE = 0x1bc8, T_TG_NAME = 0x1344, T_TG_NAME2 = 0x1ec2, T_TG_NEW = 0xb83,
  T_TG_DOT = 0x6b4, S_TG_STOCK = 0x1bc6, S_TG_AERO_GEM = 0xd2a, S_TG_REFUSE = 0x1e2e, S_TG_TOPBAR = 0x1ecf,
  T_TG_BG = 0x5af, S_TG_BG_BLACK = 0x111f, T_TG_IMAGE = 0xd3c, T_TG_WHEEL_NAME = 0x1e5f,
};

static int tg_place(const menu_state *m, int i) { return ((m->sel - i) % m->tg.n + m->tg.n) % m->tg.n; }

static int tg_layout(const menu_state *m) {
  return m->tg.n <= 2 ? ID.tg_aero2 : m->tg.n == 3 ? ID.tg_aero3 : m->tg.n == 4 ? ID.tg_aero : ID.tg_aero5;
}

/* choice i's frame of the layout */
static int tg_frame(const menu_state *m, int i) {
  if (m->tg.decided) {
    int t = m->t < TG_DECIDE - 1 ? m->t : TG_DECIDE - 1;
    /* the player shows a segment's first frame after one step: 91 on (90 is still the
     * move's end, which for the place right of the front is the front) */
    return 100 * tg_place(m, i) + 91 + t;
  }
  if (m->tg.move) {
    /* place r's slide (100r+60 .. 100r+88) takes it to place r+1: on from the place before
     * the move going right, back from the place after it going left */
    int r = ((m->tg.move > 0 ? m->from - i : m->sel - i) % m->tg.n + m->tg.n) % m->tg.n, t = m->tg.move_t;
    return 100 * r + 60 + (m->tg.move > 0 ? t : TG_MOVE - 1 - t);
  }
  return 100 * tg_place(m, i) + m->card_t % 60;
}

/* a dress-up screen's car: the parts the car wears */
static void tg_wear(const menu_state *m, menu_car *c) {
  c->mirror = m->mirror[m->maker][m->car];
  c->wing = m->wing[m->maker][m->car];
  c->bonnet = m->bonnet[m->maker][m->car];
  c->wheel = m->wheel[m->maker][m->car];
  c->neon = m->neon[m->maker][m->car];
  c->vinyl = m->vinyl[m->maker][m->car];
}

/* the colours' set (src/dressdb.h DRESS_WHEEL_SETS) a wheel (menu_state.wheel's value) is
 * one of, and which colour it is there; NULL for the car's own */
static const dress_wheel_set *wheel_set(int wheel, int *col) {
  for (size_t i = 0; wheel > 0 && i < sizeof DRESS_WHEEL_SETS / sizeof DRESS_WHEEL_SETS[0]; ++i)
    for (int c = 0; c < DRESS_WHEEL_SETS[i].n; ++c)
      if (DRESS_WHEEL_SETS[i].model[c] == wheel - 1) {
        if (col) *col = c;
        return &DRESS_WHEEL_SETS[i];
      }
  return NULL;
}

/* the wheel change's place i in colour c: stock, the one worn (No change), else the won
 * one's colour (get_parts_data(0, wheel, colour, gem) + 8) */
static int tg_wheel_value(const menu_state *m, int i, int c) {
  if (i <= 1) return m->tg.wl[i];
  const dress_wheel_set *w = wheel_set(m->tg.wl[i], NULL);
  return w ? w->model[c < w->n ? c : 0] + 1 : m->tg.wl[i];
}

/* the bonnet's, wing's and mirror's change screens (clSeqTgChangePartsBonnet/Wing/MirrorThread,
 * their yield()s' 0x9776aa0, 0x9776b60, 0x9776bc0): by the part (0 bonnet, 1 wing, 2 mirror)
 * the car's turn about X, Y, Z (degrees) and place, and the eye; it looks at the origin, 60
 * degrees across */
static const float TG_PART_ROT[3][3] = { { 21.6f, 150.3f, 11.5f }, { -8.0f, 27.0f, -4.0f }, { 24.5f, 146.6f, 16.5f } };
static const float TG_PART_POS[3][3] = { { -3.3f, 4.4f, -935.5f }, { 0.5f, 0.5f, 11.5f }, { 2.0f, 0.5f, -3.5f } };
static const float TG_PART_EYE[3][3] = { { 0, 0, -1000.0f }, { 0, 3.0f, -50.0f }, { 0, -6.0f, -50.0f } };
static int tg_part(const menu_state *m) { return m->tg.kind == 2 ? 0 : m->tg.kind == 3 ? 1 : 2; }

/* the part worn (clSeqTgWearPartsAeroThread::yield's DB_XYZ 0x97ff560, by the kind: table
 * 0x9409f24): the car's turn about X, Y, Z (degrees) and place; the aero set's turns on 0.3
 * degrees a frame about y. The eye (0, 10, 50), the mirror's (0, 10, -50), at the origin,
 * 60 degrees across. */
static const float TG_WEAR_POSE[6][6] = {
  { 0, 0, 0, 0, 0, 0 },
  { 0, -6.0f, 0, 0, -7.5f, -3.0f },
  { 0.5f, -42.0f, 0, 3.8f, -5.7f, -0.5f },
  { 18.8f, -1591.5f, -8.5f, -5.2f, -10.0f, 0.5f },
  { -8.0f, 145.6f, -2.0f, -4.0f, -7.5f, 0 },
  { 0, -56.0f, 0, 0, -7.5f, -3.0f }, /* the sticker's and neons' (wk_s 5, 6, 8) */
};
/* TG_WEAR_POSE's row of a kind: the aero set's for the rest (the gem cars' gull wing) */
static int tg_wear_k(int kind) { return kind >= 1 && kind <= 4 ? kind : kind == 5 || kind == 6 || kind == 8 ? 5 : 1; }

/* igMatrix44f::makeRotationRadians(x, y, z) (degrees here) then the translation: turned about
 * the world's X, then Y, then Z; its rows (row vectors) are the model's axes */
static int tg_vinyl_value(const menu_state *m);
static mat4 tg_rotation(const float *pos, float x, float y, float z) {
  float cx = cosf(x * DEG), sx = sinf(x * DEG), cy = cosf(y * DEG), sy = sinf(y * DEG);
  float cz = cosf(z * DEG), sz = sinf(z * DEG);
  /* Rx Ry */
  const float a[3][3] = { { cy, 0, -sy }, { sx * sy, cx, sx * cy }, { cx * sy, -sx, cx * cy } };
  mat4 r = mat4_identity();
  for (int i = 0; i < 3; ++i) {
    r.m[4 * i + 0] = a[i][0] * cz - a[i][1] * sz;
    r.m[4 * i + 1] = a[i][0] * sz + a[i][1] * cz;
    r.m[4 * i + 2] = a[i][2];
  }
  r.m[12] = pos[0], r.m[13] = pos[1], r.m[14] = pos[2];
  return r;
}

static int match_scene3d(const menu_state *m, menu_scene *s);

int menu_scene3d(const menu_state *m, menu_scene *s) {
  /* all of it: a car's aero and cp left unset made the car select's cars aero cars not in
   * (push86, nothing drawn) */
  memset(s, 0, sizeof *s);
  s->env = 4;
  /* the cars flying in (and while the race loads before them: their packs in) */
  if (m->screen == MENU_VS_MATCH || (m->screen == MENU_VS && m->vs.state == MENU_VS_LOADING && m->vs.match_n))
    return match_scene3d(m, s);
  if (m->screen == MENU_CAR) {
    const float *k = CAROUSEL[m->maker];
    int n = ncars(m);
    /* makeProjection(fov + 60, -1, 4/3, 5, 20000); setInterest(eye, at) */
    s->fov_x = (k[3] + 60.0f) * DEG;
    s->eye[0] = k[2];
    s->eye[1] = 1.0f + 5.0f;
    s->eye[2] = -7.0f + k[6] + 121.0f;
    s->at[0] = 0;
    s->at[1] = k[0] + 9.5f;
    s->at[2] = 0;
    float r = 48.0f + k[4];
    for (int i = 0; i < n && i < MENU_MAX_CARS; ++i) {
      float j = (float)(n - i);
      float a = 2.61f + m->rot + TWO_PI * j / (float)n;
      float yaw = (-360.0f * j / (float)n - 61.1f - m->rot * 57.32484f) * DEG;
      float pos[3] = { cosf(a) * r - 15.0f + k[1], -7.0f, sinf(a) * r + 15.0f };
      mat4 mm = mat4_body(pos, yaw, 0, 0);
      const cardb_color *c = &CARDB_CARS[m->maker][i].colors[0];
      s->cars[s->n].pack = c->pack;
      s->cars[s->n].paint = c->paint;
      for (int e = 0; e < 16; ++e) s->cars[s->n].model[e] = mm.m[e];
      s->n++;
    }
    return 1;
  }
  if (m->screen == MENU_VS_STAR) {
    /* clSeqJoStarGetThread: the card's car turning about y at the origin, environment 0, the
     * default projection, setInterest((0, 11, 50), (0, 4, 0)) */
    s->env = 0;
    s->fov_x = 73.0f * DEG;
    s->eye[0] = 0, s->eye[1] = 11.0f, s->eye[2] = 50.0f;
    s->at[0] = 0, s->at[1] = 4.0f, s->at[2] = 0;
    const float pos[3] = { 0, 0, 0 };
    mat4 mm = mat4_body(pos, m->star_spin * DEG, 0, 0);
    const cardb_color *c = &chosen_car(m, m->car)->colors[m->color];
    s->cars[0].pack = c->pack;
    s->cars[0].paint = c->paint;
    s->cars[0].aero = m->aero[m->maker][m->car];
    tg_wear(m, &s->cars[0]);
    for (int e = 0; e < 16; ++e) s->cars[0].model[e] = mm.m[e];
    s->n = 1;
    return 1;
  }
  if (m->screen == MENU_GARAGE) {
    /* the turntable of the colour select, the car in its parts */
    s->fov_x = 73.0f * DEG;
    s->eye[0] = 0, s->eye[1] = 10.0f, s->eye[2] = 50.0f;
    s->at[0] = 0, s->at[1] = 3.0f, s->at[2] = 0;
    const float pos[3] = { 1.6f, -0.8f, 0.8f };
    mat4 mm = mat4_body(pos, m->spin * DEG, 2.2f * DEG, 0);
    const cardb_color *c = &chosen_car(m, m->car)->colors[m->color];
    s->cars[0].pack = c->pack;
    s->cars[0].paint = c->paint;
    s->cars[0].aero = m->aero[m->maker][m->car];
    tg_wear(m, &s->cars[0]);
    for (int e = 0; e < 16; ++e) s->cars[0].model[e] = mm.m[e];
    s->n = 1;
    return 1;
  }
  if (m->screen == MENU_COLOR || m->screen == MENU_SETTING) {
    /* clMenu3dEnvironment's default projection (73 degrees), the turntable of car_common
     * (the setting screen's too, with the colour chosen) */
    s->fov_x = 73.0f * DEG;
    s->eye[0] = 0, s->eye[1] = 10.0f, s->eye[2] = 50.0f;
    s->at[0] = 0, s->at[1] = 3.0f, s->at[2] = 0;
    const float pos[3] = { 1.6f, -0.8f, 0.8f };
    mat4 mm = mat4_body(pos, m->spin * DEG, 2.2f * DEG, 0);
    const cardb_color *c = &chosen_car(m, m->car)->colors[m->screen == MENU_COLOR ? m->sel : m->color];
    s->cars[0].pack = c->pack;
    s->cars[0].paint = c->paint;
    /* the setting's car in its parts, as the garage showed it (push116) */
    if (m->screen == MENU_SETTING) {
      s->cars[0].aero = m->aero[m->maker][m->car];
      tg_wear(m, &s->cars[0]);
    }
    for (int e = 0; e < 16; ++e) s->cars[0].model[e] = mm.m[e];
    s->n = 1;
    return 1;
  }
  if (m->screen == MENU_ST_EFF) {
    /* clSeqStTuneEffThread: the card's car scaled 0.9, turned and placed by its pose (the
     * second set once rebuilt, turning), environment 8, makeProjection(20, -1, 4/3, 5,
     * 20000), setInterest((0, 28.6, -143), (0, 5, 0)) */
    const cardb_color *c = &chosen_car(m, m->car)->colors[m->color];
    const float *p = STEFF_POSE[m->eff.after][m->eff.pose & 7];
    s->env = 8;
    s->fov_x = 20.0f * DEG;
    s->eye[0] = 0, s->eye[1] = 28.6f, s->eye[2] = -143.0f;
    s->at[0] = 0, s->at[1] = 5.0f, s->at[2] = 0;
    mat4 mm = tg_rotation(p, p[3], p[4] + m->eff.angle, p[5]);
    for (int e = 0; e < 3; ++e) mm.m[e] *= 0.9f, mm.m[4 + e] *= 0.9f, mm.m[8 + e] *= 0.9f;
    s->cars[0] = (typeof(s->cars[0])){ c->pack, c->paint, m->aero[m->maker][m->car], 0, 0, 0, { 0 } };
    for (int e = 0; e < 16; ++e) s->cars[0].model[e] = mm.m[e];
    tg_wear(m, &s->cars[0]);
    s->n = 1;
    return 1;
  }
  if (m->screen == MENU_TG_GET || m->screen == MENU_TG_AERO) {
    const cardb_color *c = &chosen_car(m, m->car)->colors[m->color];
    int worn = m->aero[m->maker][m->car];
    /* clSeqTgPartsGetAeroThread's environment 0, clSeqTgChangePartsAeroThread's 3 */
    s->env = m->screen == MENU_TG_GET ? 0 : 3;
    s->fov_x = 60.0f * DEG;
    s->eye[0] = 0, s->eye[1] = 10.0f, s->eye[2] = 50.0f;
    s->at[0] = s->at[1] = s->at[2] = 0;
    if (m->screen == MENU_TG_GET && tg_get_type(m) == TG_GET_WHEEL) {
      /* clSeqTgPartsGetWheelThread::yield: the wheel (data/menu3d/wheel) turned the angle
       * about y at (0, 0, -660) (DB 0x97768c0, 0x97768d0), the camera 1 degree across from
       * (0, 0, 5) at the origin */
      const float pos[3] = { 0, 0, -660.0f };
      mat4 mm = tg_rotation(pos, 0, m->tg.angle, 0);
      s->fov_x = 1.0f * DEG;
      s->env = 14;
      s->eye[0] = s->eye[1] = 0, s->eye[2] = 5.0f;
      s->get_wheel = m->tg.num + 1;
      for (int e = 0; e < 16; ++e) s->wheel_model[e] = mm.m[e];
      return 1;
    }
    if (m->screen == MENU_TG_GET) {
      /* the car the card has, as it is (clMenuCar); the picture GET screens have none */
      if (tg_get_type(m) != TG_GET_CAR) return 0;
      const float pos[3] = { 0.5f, -5.0f, -9.5f };
      mat4 mm = mat4_body(pos, (m->tg.angle - 21.3f) * DEG, 0, 0);
      s->cars[0] = (typeof(s->cars[0])){ c->pack, c->paint, worn, 0, 0, 0, { 0 } };
      for (int e = 0; e < 16; ++e) s->cars[0].model[e] = mm.m[e];
      tg_wear(m, &s->cars[0]);
      s->n = 1;
      return 1;
    }
    int lay = tg_layout(m);
    for (int i = 0; i < m->tg.n && s->n < MENU_MAX_CARS; ++i) {
      float r[4];
      if (!anm_sprite_rect(lay, tg_frame(m, i), anm_sprite(T_TG_DOT), r)) continue;
      int o = m->tg.opt[i];
      const float pos[3] = { 0, 20.0f, m->tg.z[i] };
      mat4 mm = tg_rotation(pos, 10.0f, -30.0f, -4.0f);
      /* the arcade turns X by 10 (the nose down), Y by -30, Z by -4; the dot's scale (its rect is the 2-pixel dot's) times the model's 3 */
      float k = 3.0f * (r[2] - r[0]) / 2.0f;
      for (int e = 0; e < 3; ++e) mm.m[e] *= k, mm.m[4 + e] *= k, mm.m[8 + e] *= k;
      s->cars[s->n] = (typeof(s->cars[0])){ c->pack, c->paint, o < 0 ? worn : o, 1, r[0], r[1], { 0 } };
      tg_wear(m, &s->cars[s->n]);
      for (int e = 0; e < 16; ++e) s->cars[s->n].model[e] = mm.m[e];
      s->n++;
    }
    return 1;
  }
  if (m->screen == MENU_TG_PART && (m->tg.kind == 6 || m->tg.kind == 8)) {
    /* clSeqTgChangePartsNeonThread::yield: the place's car (a clMenuCar a place, its neon
     * on) moved (0, 1.5, -10) (DB 0x9776d90), then turned (3.5, 153.5, 2) degrees (0x9776d80);
     * environment 5, makeProjection(60, -1, 4/3, 5, 20000), setInterest((0, 3, -50), origin) */
    const cardb_color *c = &chosen_car(m, m->car)->colors[m->color];
    const float zero[3] = { 0, 0, 0 };
    mat4 mm = tg_rotation(zero, 3.5f, 153.5f, 2.0f);
    for (int j = 0; j < 3; ++j) mm.m[12 + j] = 1.5f * mm.m[4 + j] - 10.0f * mm.m[8 + j];
    s->fov_x = 60.0f * DEG;
    s->env = 5;
    s->eye[0] = 0, s->eye[1] = 3.0f, s->eye[2] = -50.0f;
    s->at[0] = s->at[1] = s->at[2] = 0;
    s->cars[0] = (typeof(s->cars[0])){ c->pack, c->paint, m->aero[m->maker][m->car], 0, 0, 0, { 0 } };
    for (int e = 0; e < 16; ++e) s->cars[0].model[e] = mm.m[e];
    tg_wear(m, &s->cars[0]);
    s->cars[0].neon = m->tg.opt[m->sel];
    s->n = 1;
    return 1;
  }
  if (m->screen == MENU_TG_PART) {
    /* the card's car (clMenuCar) with the choice on, turned and placed */
    const cardb_color *c = &chosen_car(m, m->car)->colors[m->color];
    int k = tg_part(m), o = m->tg.opt[m->sel];
    const float *r = TG_PART_ROT[k];
    s->fov_x = 60.0f * DEG;
    s->env = k == 2 ? 13 : 5; /* clSeqTgChangePartsMirrorThread's, the bonnet's and wing's */
    for (int e = 0; e < 3; ++e) s->eye[e] = TG_PART_EYE[k][e], s->at[e] = 0;
    mat4 mm = tg_rotation(TG_PART_POS[k], r[0], r[1], r[2]);
    s->cars[0] = (typeof(s->cars[0])){ c->pack, c->paint, m->aero[m->maker][m->car], 0, 0, 0, { 0 } };
    for (int e = 0; e < 16; ++e) s->cars[0].model[e] = mm.m[e];
    tg_wear(m, &s->cars[0]);
    if (k == 0) s->cars[0].bonnet = o;
    else if (k == 1) s->cars[0].wing = o;
    else s->cars[0].mirror = o;
    s->n = 1;
    return 1;
  }
  if (m->screen == MENU_TG_SELECT) {
    /* clTgDressUpPartsSelect::yield: the card's car (clMenuCar) turned -45 degrees about y
     * at the origin; environment 0, setInterest((0, 7.5, 50), (0, -2.5, 0)) */
    const cardb_color *c = &chosen_car(m, m->car)->colors[m->color];
    const float pos[3] = { 0, 0, 0 };
    mat4 mm = tg_rotation(pos, 0, -45.0f, 0);
    s->env = 0;
    s->fov_x = 60.0f * DEG;
    s->eye[0] = 0, s->eye[1] = 7.5f, s->eye[2] = 50.0f;
    s->at[0] = 0, s->at[1] = -2.5f, s->at[2] = 0;
    s->cars[0] = (typeof(s->cars[0])){ c->pack, c->paint, m->aero[m->maker][m->car], 0, 0, 0, { 0 } };
    for (int e = 0; e < 16; ++e) s->cars[0].model[e] = mm.m[e];
    tg_wear(m, &s->cars[0]);
    s->n = 1;
    return 1;
  }
  if (m->screen == MENU_TG_WHEEL) {
    /* clSeqTgChangePartsWheelThread::yield: the card's car with the place's wheel in the
     * colour (clMenuCarSep, one a place and colour) turned 120 degrees about y at (-17.5,
     * 2.5, 0); environment 7, makeProjection(73, -1, 4/3, 5, 20000), setInterest((0, 12,
     * -48), (0, 0, 0)) */
    const cardb_color *c = &chosen_car(m, m->car)->colors[m->color];
    const float pos[3] = { -17.5f, 2.5f, 0 };
    mat4 mm = tg_rotation(pos, 0, 120.0f, 0);
    s->env = 7;
    s->fov_x = 73.0f * DEG;
    s->eye[0] = 0, s->eye[1] = 12.0f, s->eye[2] = -48.0f;
    s->at[0] = s->at[1] = s->at[2] = 0;
    s->cars[0] = (typeof(s->cars[0])){ c->pack, c->paint, m->aero[m->maker][m->car], 0, 0, 0, { 0 } };
    for (int e = 0; e < 16; ++e) s->cars[0].model[e] = mm.m[e];
    tg_wear(m, &s->cars[0]);
    s->cars[0].wheel = tg_wheel_value(m, m->sel, m->tg.col);
    s->n = 1;
    return 1;
  }
  if (m->screen == MENU_TG_VINYL) {
    /* clSeqTgChangePartsStickerThread::yield: the card's car (a clMenuCar with the sticker,
     * check_body: rebuilt as it changes) moved (0.5, 3.5, -8.5) (0x9776cf0), then turned (7,
     * 124, 16) degrees (0x9776ce0); environment 5, makeProjection(60, -1, 4/3, 5, 20000),
     * setInterest((0, 35.5, -50), origin) */
    const cardb_color *c = &chosen_car(m, m->car)->colors[m->color];
    const float zero[3] = { 0, 0, 0 };
    mat4 mm = tg_rotation(zero, 7.0f, 124.0f, 16.0f);
    for (int j = 0; j < 3; ++j) mm.m[12 + j] = 0.5f * mm.m[j] + 3.5f * mm.m[4 + j] - 8.5f * mm.m[8 + j];
    s->fov_x = 60.0f * DEG;
    s->env = 5;
    s->eye[0] = 0, s->eye[1] = 35.5f, s->eye[2] = -50.0f;
    s->at[0] = s->at[1] = s->at[2] = 0;
    s->cars[0] = (typeof(s->cars[0])){ c->pack, c->paint, m->aero[m->maker][m->car], 0, 0, 0, { 0 } };
    for (int e = 0; e < 16; ++e) s->cars[0].model[e] = mm.m[e];
    tg_wear(m, &s->cars[0]);
    s->cars[0].vinyl = tg_vinyl_value(m);
    s->n = 1;
    return 1;
  }
  if (m->screen == MENU_TG_WEAR && m->tg.kind == 0) {
    /* clSeqTgWearPartsWheelThread::yield: the car (clCarModelData's body and tyres) turned
     * (0, -37, 0) degrees (DB 0x9776e00) at (12.5 - 10, -11, -34.6) (0x9776e10), the tyres
     * lifted by the jack (+0x44), the body by that and its own (+0x54, the bounce +0x50); the
     * wheel worn before (state < 0) then the new one; environment 11, makeProjection(60, -1,
     * 4/3, 5, 20000), setInterest((0, 2.2, 14), (0, 0, 0)) */
    const cardb_color *c = &chosen_car(m, m->car)->colors[m->color];
    const float pos[3] = { 2.5f, -11.0f + m->tg.lift + m->tg.body + m->tg.bounce, -34.6f };
    mat4 mm = tg_rotation(pos, 0, -37.0f, 0);
    s->env = 11;
    s->fov_x = 60.0f * DEG;
    s->eye[0] = 0, s->eye[1] = 2.2f, s->eye[2] = 14.0f;
    s->at[0] = s->at[1] = s->at[2] = 0;
    s->cars[0] = (typeof(s->cars[0])){ c->pack, c->paint, m->aero[m->maker][m->car], 0, 0, 0, { 0 } };
    for (int e = 0; e < 16; ++e) s->cars[0].model[e] = mm.m[e];
    tg_wear(m, &s->cars[0]);
    s->cars[0].wheel = m->tg.state < 0 ? m->tg.old : m->tg.chosen;
    s->cars[0].lift = m->tg.body + m->tg.bounce;
    s->n = 1;
    return 1;
  }
  if (m->screen == MENU_TG_WEAR) {
    /* the card's car (clMenuCar) as it is now; a bonnet's, wing's or mirror's shows the
     * one worn before (car[0]) until the white (state 2: car[1]) */
    const cardb_color *c = &chosen_car(m, m->car)->colors[m->color];
    int k = tg_wear_k(m->tg.kind);
    const float *p = TG_WEAR_POSE[k];
    s->fov_x = 60.0f * DEG;
    s->env = k == 4 ? 16 : 11; /* clSeqTgWearPartsAeroThread's: 16 for a mirror (wk_s 4), else 11 */
    s->eye[0] = 0, s->eye[1] = 10.0f, s->eye[2] = k == 4 ? -50.0f : 50.0f;
    s->at[0] = s->at[1] = s->at[2] = 0;
    mat4 mm = tg_rotation(p + 3, p[0], k == 1 ? m->tg.ry : p[1], p[2]);
    s->cars[0] = (typeof(s->cars[0])){ c->pack, c->paint, m->aero[m->maker][m->car], 0, 0, 0, { 0 } };
    for (int e = 0; e < 16; ++e) s->cars[0].model[e] = mm.m[e];
    tg_wear(m, &s->cars[0]);
    if (m->tg.state <= 1) {
      if (k == 2) s->cars[0].bonnet = m->tg.old;
      else if (k == 3) s->cars[0].wing = m->tg.old;
      else if (k == 4) s->cars[0].mirror = m->tg.old;
      else if (m->tg.kind == 6 || m->tg.kind == 8) s->cars[0].neon = m->tg.old;
      else if (m->tg.kind == 5) s->cars[0].vinyl = m->tg.old;
    }
    s->n = 1;
    return 1;
  }
  return 0;
}

static void draw_car_select(const menu_state *m, int clock, void (*draw3d)(void *), void *ctx) {
  if (m->screen == MENU_MAKER) {
    draw_back(ID.bg, clock % 600, 1);
    anm_draw(ID.line, clock % anm_frames(ID.line), 1);
    int f, g;
    if (m->from >= 0) {
      int t = m->t < MAKER_MOVE ? m->t : MAKER_MOVE - 1;
      int k = m->from < m->sel ? m->from : m->sel;
      f = m->from < m->sel ? 80 + 100 * k + t : 80 + 100 * k + MAKER_MOVE - 1 - t;
      g = m->from < m->sel ? 80 + 100 * k + m->t % 5 : 84 + 100 * k - t;
    } else {
      f = 100 * m->sel + m->t % 59;
      g = 1 + 100 * m->sel + m->t % 59;
    }
    anm_draw(ID.maker_layout, f, 1);
    anm_draw(ID.maker_car, g, 1);
    return;
  }
  int car = m->screen == MENU_CAR ? m->sel : m->car;
  const cardb_car *c = chosen_car(m, car);
  draw_back(ID.bg_carsel, clock % 240, 1);
  draw_bg_logo(c, clock % 240, 1);
  anm_draw(ID.line, clock % anm_frames(ID.line), 1);
  if (draw3d) draw3d(ctx);
  anm_swap sw[3 + 12 + 1];
  int n = car_swaps(c, sw);
  anm_draw_ex(ID.car_info, clock % 240, 1, 0, 0, sw, n);
  int r35 = has(c->label, "[R35]");
  if (m->screen == MENU_CAR) {
    /* the R35 and the KPGC10 were new on this version */
    if (r35 || has(c->label, "[KPGC10]")) anm_draw(ID.car_new, m->t % 60, 1);
    anm_draw(ID.car_arrow, clock % 60, 1);
  } else {
    int nc = c->ncolors, k = strip_index(nc);
    for (int i = 0; i < nc; ++i) sw[n + i] = (anm_swap){ anm_sprite(T_SWATCH + nc - i), anm_sprite(c->colors[i].swatch) };
    anm_draw_ex(ID.col_strip[k], color_frame(m), 1, 0, 0, sw + n, nc);
    anm_draw(ID.col_arrow[k], color_frame(m), 1);
    /* the colour's name, right-aligned on its plate (clSpriteFont, datrums 5) */
    anm_swap none = { anm_sprite(T_COL_FONT), -1 };
    anm_draw_ex(ID.col_name, 0, 1, 0, 0, &none, 1);
    int name = anm_sprite(c->colors[m->sel].name);
    anm_draw_sprite(name, 624.0f - anm_sprite_width(name), 316.0f, 1);
  }
  if (r35) anm_draw(ID.mes_r35, m->t % 60, 1);
  if (has(c->label, "[JZS161]")) anm_draw(ID.mes_jzs161, m->t % 60, 1);
}

/* ---- the top bar (clMenuBarStd): the arcade draws its countdown (menu_g_timer_x) where the
 * bar has menu_p_time_dot_3 and hides dots 1 and 2. The port has no time limit in the menus,
 * so a big infinity sign (menupack.py INFINITY_ID) stands in for the seconds. */
enum { T_TIME_DOT1 = 0x771, T_TIME_DOT2 = 0x772, T_TIME_DOT3 = 0x773, T_INFINITY = 102000 };
#define INF_W 128 /* the sign's sprite, centred a little right of and below the dot */
#define INF_H 72
#define INF_DX 3
#define INF_DY 6

/* the bar without its seconds (a screen with a time limit draws them: draw_vs_area) */
static void draw_topbar_timed(int bars, float fade, const anm_swap *mes) {
  anm_swap sw[4] = { { anm_sprite(T_TIME_DOT1), -1 }, { anm_sprite(T_TIME_DOT2), -1 }, { anm_sprite(T_TIME_DOT3), -1 } };
  int n = 3;
  if (mes) sw[n++] = *mes;
  anm_draw_ex(ID.top, bars, fade, 0, 0, sw, n);
}

static void draw_topbar(int bars, float fade, const anm_swap *mes) {
  draw_topbar_timed(bars, fade, mes);
  float r[4];
  int inf = anm_sprite(T_INFINITY);
  if (inf >= 0 && anm_sprite_rect(ID.top, bars, anm_sprite(T_TIME_DOT3), r))
    anm_draw_sprite(inf, r[0] + INF_DX - INF_W / 2, r[1] + INF_DY - INF_H / 2, fade);
}

/* ---- the time zone and BGM select (clSeqStdDayTimeSelectThread, between the course
 * select and the race) ----
 * - menu_layout_yn_card_1_gold: the morning (0) and night (1) cards of a clMenuSpinBoxStd,
 *   100 frames a card: from 100i a 60-frame idle loop, 100i+60..69 decided, 100i+90..94 the
 *   slide on to the next card. menu_layout_yn_ring: the ring's flash, 100i..159 once.
 *   The wheel turns the spin box: here the left/right keys or a tap on its arrows.
 * - menu_g_yn_day_sound: the BGM line. The view change button steps through the list
 *   (nsMenuCheck::getBgmSelectNext): here a tap on the line. The line's name takes the
 *   place of template 0xbaf (clSpriteFont, font 0) and the soundtrack's logo that of 0x1643
 *   (set 0's own, Maxi3). Without the night map the cards stay on morning and the keys step
 *   through the BGM too. */
#define DECIDE_DAY 10
#define SLIDE 5 /* frames 100i+90..94 */
#define BGM_LINES 22
#define BGM_TEXT 101000 /* menupack.py BGM_TEXT_BASE */
enum {
  T_MORNING = 0x1e68, T_NIGHT = 0x1cdc, T_RING = 0x3b5, T_RING_BLUE = 0xb94, T_ARROW = 446, T_ARROW2 = 899,
  T_TOPBAR_MES = 0x1ab5, T_BGM_FONT = 0xbaf,
};
/* The songs of set 0 (0x93dd780) by line, as files of data/sound/bgm (prepare_bgm.sh encodes
 * the arcade's data/sound/bgm/<dir>/<name>.wav): the four of 3DX+, the one of 3DX, then
 * Maxi3's fifteen. Line 0 is RANDOM, line 21 NO BGM. */
static const char *const SONG_FILE[BGM_LINES] = {
  NULL,
  "wm3DX_P_race_01", "wm3DX_P_race_02", "wm3DX_P_race_03", "wm3DX_P_race_04", "wm3DX_race_01_mainD",
  "wm3_01", "wm3_02", "wm3_03", "wm3_04", "wm3_05", "wm3_06", "wm3_07", "wm3_08", "wm3_09", "wm3_10",
  "wm3_11", "wm3_12", "wm3_13", "wm3_14", "wm3_15",
  NULL,
};
/* mseqEntry plays emBgmType 0x5e through the car select, mseqPlay 0x5f from the mode select on */
#define MUSIC_ENTRY "wm3DX_P_sys_01_ent"
#define MUSIC_SELECT "wm3DX_P_sys_02_sel"
/* clRaceThread::result2 plays 0x6d (maxi3/sys_04, looped) with clTaResult or clTaTimeUp */
/* one array: main.m tells tracks apart by pointer, and the result music goes on from the race
 * into the menus */
static const char MUSIC_RESULT[] = "sys_04";
/* the story ending's (seqRaceEnding: getBgm of select 111, emBgmType 54 maxi3/sys_05), from
 * its frame 150 (clRaceThread::race resumeStream) till the skip's or 9420's fade out (120) */
static const char MUSIC_ENDING[] = "sys_05";

float menu_bgm_level(const menu_state *m) {
  if (m->ending != 2 || m->ending_t < MENU_ENDING_SKIP) return 1.0f;
  float v = 1.0f - (float)(m->ending_t - MENU_ENDING_SKIP) / 120.0f;
  return v < 0 ? 0.0f : v;
}

const char *menu_bgm(const menu_state *m, int racing) {
  /* result2 stops the race's song as the race ends; clRaceThread::run only after the select's
   * last period, the song plays on into the next */
  if (m->ending == 2) return racing && m->ending_t >= 150 && m->ending_t < MENU_ENDING_SKIP + 120 ? MUSIC_ENDING : NULL;
  if (racing && m->finish && m->finish_story != 3) return m->finish > m->finish_wait ? MUSIC_RESULT : NULL;
  if (racing || (m->screen == MENU_PRE && m->pre_quiet)) return m->song > 0 ? SONG_FILE[m->song] : NULL;
  if (m->screen < MENU_START) return NULL; /* the attract loop: its movies have their own sound */
  /* mseqTimeAttack stops the music once the BGM is chosen (before seqTaCourseDecide) */
  if ((m->screen == MENU_BGM && m->deciding) || m->screen == MENU_PRE) return NULL;
  /* the result's music plays on through the screens after a race (mseqStory: no stream change
   * from result2's 0x6d till its sequence ends; the story's tuning, the dress-up parts won),
   * the select music back on the mode select; the car setting's on the dress-up menu it opens */
  if (!m->dress_free && (m->screen == MENU_ST_TUNE || m->screen == MENU_TG_GET || m->screen == MENU_TG_AERO || m->screen == MENU_TG_PART ||
      m->screen == MENU_TG_WHEEL || m->screen == MENU_TG_VINYL || m->screen == MENU_TG_WEAR || m->screen == MENU_LVUP || m->screen == MENU_SHOGO ||
      m->screen == MENU_ST_EFF || m->screen == MENU_VS_STAR || m->screen == MENU_POEM || m->screen == MENU_BONUS))
    return MUSIC_RESULT;
  if (m->dress_free) return MUSIC_ENTRY;
  return m->music ? MUSIC_SELECT : MUSIC_ENTRY;
}

/* the cards' layout: a versus' its own (menu_layout_yn_day_vs: higher, the players' dots under) */
static int day_anim(const menu_state *m) { return m->vs.day && ID.day_vs >= 0 ? ID.day_vs : ID.day_cards; }

/* Whether the tap is on the chosen time zone's card (the decide tap), or with sprite set,
 * near that sprite of the spin box (its arrows) */
static int on_card(const menu_state *m, int sprite) {
  float r[4];
  int f = 100 * m->zone + m->card_t % 60;
  if (sprite) {
    if (!anm_sprite_rect(day_anim(m), f, anm_sprite(sprite), r)) return 0;
    return m->tap_x >= r[0] - 24.0f && m->tap_x < r[2] + 24.0f && m->tap_y >= r[1] - 24.0f && m->tap_y < r[3] + 24.0f;
  }
  if (!anm_sprite_rect(day_anim(m), f, anm_sprite(m->zone ? T_NIGHT : T_MORNING), r)) return 0;
  float cx = (r[0] + r[2]) * 0.5f, cy = (r[1] + r[3]) * 0.5f, rad = (r[2] - r[0]) * 0.5f;
  return (m->tap_x - cx) * (m->tap_x - cx) + (m->tap_y - cy) * (m->tap_y - cy) < rad * rad;
}

/* the card chosen: the BGM line's song with it */
static void day_decide(menu_state *m) {
  snd_menu(SND_SELECT);
  m->bgm = m->sel;
  /* getBgmSelectId: RANDOM draws one of the songs (clMTRandom over lines 1..20) */
  m->song = m->sel == 0 ? 1 + rand() % (BGM_LINES - 2) : m->sel == BGM_LINES - 1 ? -1 : m->sel;
}

/* one step of the time zone and BGM select; returns 1 when it decides */
static int day_select_step(menu_state *m, int tap, int key) {
  if (m->slide) {
    /* the other card comes to rest */
    if (--m->slide == 0) {
      m->zone = (m->zone + 1) % 2;
      m->card_t = 0;
    }
    return 0;
  }
  m->card_t++;
  /* the wheel; or a tap on the arrows or on the other card */
  int turn = m->zones > 1 &&
             (key || (tap && (on_card(m, T_ARROW) || on_card(m, T_ARROW2) || on_card(m, m->zone ? T_MORNING : T_NIGHT))));
  if (turn) {
    snd_menu(SND_CURSOR);
    m->slide = SLIDE;
    return 0;
  }
  /* (a versus' BGM is the host's: the others have no line) */
  int bgm = !m->vs.day || m->vs.me == m->vs.host;
  int d = m->zones > 1 || !bgm ? 0 : key;
  /* the BGM line itself (with the arrow keys either side of it) steps on, like the button */
  if (bgm && tap && m->tap_x >= 170.0f && m->tap_x < 560.0f && m->tap_y >= 326.0f && m->tap_y < 392.0f) d = 1;
  if (d) {
    snd_menu(SND_CURSOR);
    m->sel = (m->sel + d + BGM_LINES) % BGM_LINES;
    return 0;
  }
  if (tap && on_card(m, 0)) {
    day_decide(m);
    return 1;
  }
  return 0;
}

/* The versus' time zone vote (seqJoinDayTimeSelect: the same screen, clArg's join set): the
 * card chosen sets this machine's vote (clNet::setJoinDayTime), 20 s (at 0 the card shown is
 * the vote); voted, the card holds decided till the host's result (getJoinDayTime: the most
 * voted, a tie at random), which the card turns to. The BGM is each player's. */
#define DAY_TIME (20 * 60)
static void vs_day_step(menu_state *m, int tap, int key) {
  /* the vote lost its lobby (the host gone, the connection lost): back to the versus screen */
  if (m->vs.state != MENU_VS_LOBBY) {
    m->vs.day = 0;
    go(m, MENU_VS);
    return;
  }
  m->vs.day_t++;
  if (m->vs.day_final >= 0) {
    if (m->zone != m->vs.day_final && !m->slide) m->zone = m->vs.day_final, m->card_t = 0;
    if (!m->vs.day_voted) m->vs.day_voted = 1, m->vs.day_t = 0;
    m->card_t++;
    return;
  }
  if (m->vs.day_timer > 0) {
    int was = (m->vs.day_timer + 59) / 60;
    m->vs.day_timer--;
    int now = (m->vs.day_timer + 59) / 60;
    if (now != was && now >= 1 && now <= 9) snd_menu(SND_TIME_COUNT);
    if (!m->vs.day_timer && !m->vs.day_voted && !m->slide) day_decide(m), m->vs.day_voted = 1, m->vs.day_t = 0;
  }
  if (m->vs.day_voted) {
    m->card_t++;
    return;
  }
  if (day_select_step(m, tap, key)) m->vs.day_voted = 1, m->vs.day_t = 0;
}

/* The omakase key (the bottom bar's middle, HUD_MENU_HIDDEN's place) of the area vote and the
 * time zone vote: the choice left to the others (Allan 10-27: the area strip's omakase card
 * went unseen, the time zone vote had none). On the area vote it is the strip's card 0, which
 * the cursor walks to; on the time zone vote the card shown stays, the BGM line its song. */
static int vs_omakase_shown(const menu_state *m) {
  if (m->vs.state != MENU_VS_LOBBY) return 0;
  if (m->screen == MENU_VS_AREA) return !m->vs.voted && m->vs.area_final < 0;
  return m->screen == MENU_BGM && m->vs.day && !m->vs.day_voted && m->vs.day_final < 0 && !m->slide;
}

static void vs_omakase(menu_state *m) {
  if (m->screen == MENU_VS_AREA) {
    snd_menu(SND_SELECT);
    m->vs.voted = 2;
    m->vs.vote = 0;
    m->vs.vote_t = 0;
  } else {
    day_decide(m);
    m->vs.day_voted = 2;
    m->vs.day_t = 0;
  }
}

/* the key, or (voted so) its caption alone */
static void draw_vs_omakase(const menu_state *m, float screen_w, float screen_h) {
  int voted = m->screen == MENU_VS_AREA ? m->vs.voted == 2 && m->vs.area_final < 0
                                        : m->vs.day_voted == 2 && m->vs.day_final < 0;
  if (!voted && !vs_omakase_shown(m)) return;
  if (!voted) hud_draw_menu_hidden(screen_w, screen_h, m->key_held == HUD_MENU_HIDDEN);
  const char *s = voted ? "LEFT TO THE OTHERS" : "LEAVE IT TO THE OTHERS";
  float px = 2.4f;
  hud_ui_begin(screen_w, screen_h);
  hud_ui_text(s, 0.5f * (screen_w - hud_ui_text_width(s, px)), 680.0f - 3.5f * px, px, 1.0f, voted ? 0.55f : 1.0f,
              voted ? 0.1f : 1.0f, 1);
  hud_ui_end();
}

void menu_vs_day_open(menu_state *m) {
  go(m, MENU_BGM);
  m->vs.day = 1;
  m->vs.day_timer = DAY_TIME;
  m->vs.day_voted = m->vs.day_t = 0;
  m->vs.day_final = -1;
  m->sel = m->bgm;
  m->slide = m->card_t = 0;
  if (m->zone >= m->zones) m->zone = 0;
}

static void vs_timer(int timer, int bars, float fade);

/* enmUpdate's marks of the others' cards (0x8775884): menu_layout_yn_day_vs's template dots
 * menu_p_day_dot01..10 (1463..1472, hidden by the run), a card's n (1..3) taking those of its
 * row (0x93f06c0 morning, 0x93f06f0 night), each set to the player's menu_g_vs_ex_player
 * (frames 1..59 round, its icon menu_p_vs_4p_<n>: 0x93f0748). The dots' places are the
 * layout's (the templates have no pictures to pack, so not looked up): the chosen card's row
 * at y 324, the other's at 271; the chosen card's hop on its decided frames (60..68, 20.2 px
 * a frame down and back); none while the cards slide. */
#define DAY_MARK_SLOT0 1463
#define DAY_MARK_SLOTS 10
#define DAY_ICON 0x39e /* menu_p_vs_4p's template (the area vote's A_ICON) */
static const signed char DAY_MARK[2][3][3] = { { { 1 }, { 6, 7 }, { 0, 1, 2 } }, { { 4 }, { 8, 9 }, { 3, 4, 5 } } };
static const float DAY_DOT[2][DAY_MARK_SLOTS][2] = {
  { { 119, 324 }, { 191, 324 }, { 263, 324 }, { 404, 271 }, { 476, 271 }, { 548, 271 }, { 155, 324 }, { 227, 324 }, { 440, 271 }, { 512, 271 } },
  { { 94.5f, 271 }, { 166.5f, 271 }, { 238.5f, 271 }, { 376, 324 }, { 448, 324 }, { 520, 324 }, { 130, 271 }, { 202.5f, 271 }, { 412, 324 }, { 484, 324 } },
};
static void day_marks(const menu_state *m, int f, int clock, float fade) {
  if (m->slide) return;
  int on[2][3], n[2] = { 0 }, state = f >= 100;
  for (int i = 0; i < m->vs.n && i < MENU_VS_PLAYERS; ++i) {
    if (i == m->vs.me || m->vs.other_day[i] < 0 || m->vs.other_day[i] == 2) continue;
    int z = m->zones > 1 ? m->vs.other_zone[i] & 1 : 0;
    if (n[z] < 3) on[z][n[z]++] = i;
  }
  int t = f % 100 - 60; /* the decided frames' hop */
  float hop = t >= 0 && t <= 8 ? 20.2f * (float)(t < 4 ? t + 1 : 9 - t) : 0.0f;
  for (int z = 0; z < 2; ++z)
    for (int k = 0; k < n[z]; ++k) {
      const float *d = DAY_DOT[state][DAY_MARK[z][n[z] - 1][k]];
      int i = on[z][k];
      anm_swap sw = { anm_sprite(DAY_ICON), anm_sprite(DAY_ICON + i) };
      /* still choosing: fainter */
      anm_draw_ex(ID.vs_ex_player, 1 + clock % 59, fade * (m->vs.other_day[i] ? 1.0f : 0.55f), d[0],
                  d[1] + (z == state ? hop : 0.0f), &sw, 1);
    }
}

static void draw_day_select(const menu_state *m, int clock, float fade, int bars) {
  draw_back(ID.bg, clock % 600, fade);
  anm_draw(ID.line, 0, fade);
  int z = 100 * m->zone, f = z + m->card_t % 60;
  if (m->slide) f = z + 90 + SLIDE - m->slide;
  /* decided (a versus' vote: held so till the result) */
  int t = m->deciding ? DECIDE_DAY + FADE - m->deciding : m->vs.day && m->vs.day_voted && !m->slide ? m->vs.day_t : -1;
  if (t >= 0) f = z + 60 + (t < DECIDE_DAY ? t : DECIDE_DAY - 1);
  /* the spin box's templates: the cards and rings of this screen (its stPimpl); without
   * the night map the arrows that point the way to the other card are left out */
  int one = m->zones < 2;
  anm_swap cards[4 + DAY_MARK_SLOTS] = { { anm_sprite(T_MORNING), anm_sprite(0x10a2) }, { anm_sprite(T_NIGHT), anm_sprite(0xb93) },
                                         { anm_sprite(T_ARROW), one ? -1 : anm_sprite(T_ARROW) },
                                         { anm_sprite(T_ARROW2), one ? -1 : anm_sprite(T_ARROW2) } };
  for (int k = 0; k < DAY_MARK_SLOTS; ++k) cards[4 + k] = (anm_swap){ anm_sprite(DAY_MARK_SLOT0 + k), -1 };
  anm_swap rings[2] = { { anm_sprite(T_RING), anm_sprite(0xb95) }, { anm_sprite(T_RING_BLUE), anm_sprite(0x1b0b) } };
  anm_draw_ex(day_anim(m), f, fade, 0, 0, cards, 4 + DAY_MARK_SLOTS);
  if (m->vs.day) day_marks(m, f, clock, fade);
  int ring = m->vs.day && ID.day_vs_ring >= 0 ? ID.day_vs_ring : ID.day_ring;
  if (!m->slide) anm_draw_ex(ring, z + (m->card_t < 59 ? m->card_t : 59), fade, 0, 0, rings, 2);
  anm_swap title = { anm_sprite(T_TOPBAR_MES), anm_sprite(0x105a) };
  if (m->vs.day) {
    draw_topbar_timed(bars, fade, &title);
    vs_timer(m->vs.day_timer, bars, fade);
  } else {
    draw_topbar(bars, fade, &title);
  }
  anm_draw(ID.bottom, bars, fade);
  if (m->vs.day && m->vs.me != m->vs.host) return; /* a versus' song is the host's */
  anm_swap name = { anm_sprite(T_BGM_FONT), anm_sprite(BGM_TEXT + m->sel) };
  anm_draw_ex(ID.day_sound, 0, fade, 0, 0, &name, 1);
}

/* ---- the car setting (clSeqCarSettingThread: in this build only its debug menus run it,
 * the art is all there), between the colour select and the mode select ----
 * Over the colour select's turntable: STOCK or FULL TUNE top left and AT or MT top right,
 * tapped, on menu_layout_setting's course band (a car picked from the save's keeps its
 * card's STOCK or FULL TUNE: that chip alone). With full tune the left/right keys move the
 * balance as the arcade's wheel does (left more power, right more handling, SE 0x19):
 * power and handling share CAR_TUNE_TOTAL points, neither under 10 (m_select_gage_max =
 * total - 10), the wheel prompt menu_g_setting_handle looping meanwhile. Below, where
 * menu_layout_setting has them, Power and Handling over their gauges (initGageAnime: 24
 * lights each, x from the table at 0x93f0780 less 34 past 30 points, y 266 and 304; lit by
 * ten in the s1, s2 and s3 colours), the horsepower (GetHorsePower) and the balance badges
 * DG..HG (menu_g_setting_setting's, the car's lit: GetHandling). A tap on the car goes on. */
typedef char garage_fits[CARDB_ALL <= MENU_GARAGE_MAKERS && CARDB_MAX_CARS <= MENU_GARAGE_CARS ? 1 : -1];
enum {
  T_SET_BAND = 3364, T_SET_POWER = 3363, T_SET_HANDLING = 5695, T_SET_LIGHT = 7315, /* s1..s3 */
  T_SET_EMPTY = 6270, T_SET_DIGIT = 5684, T_SET_HP = 4067, T_SET_COURSE_BAND = 6804, T_SET_TITLE = 6639,
};
/* the badges dark and lit, DG, D, B, G, HG (car_tune_grade order) */
static const int SET_BADGE[5][2] = { { 7084, 7085 }, { 6800, 6801 }, { 6798, 6799 }, { 6802, 6803 }, { 7086, 7087 } };
static const short GAGE_X[CAR_TUNE_MAX] = { 171, 190, 209, 228, 247, 266, 285, 304, 323, 342, 381, 400,
                                            419, 438, 457, 476, 495, 514, 533, 552, 591, 610, 629, 648 };
#define GAGE_DX (-34.0f) /* past 30 points */
#define SET_TEXT 101100  /* menupack.py SETTING_TEXT_BASE: STOCK, FULL TUNE, AT, MT, the hint */
#define SET_ROW_Y 101.0f /* the chips' top, on the course band (84..148) */
#define SET_CHIP_GAP 28.0f
#define SET_HP_X 268.0f
#define SET_BADGE_X 140.0f /* the first badge's centre (menu_layout_setting: 191.5, 95 apart) */
#define SET_BADGE_DX 90.0f
#define SET_MIN_POWER (CAR_TUNE_TOTAL - CAR_TUNE_MAX)
/* the car on the turntable, where a tap goes on */
#define SET_CAR_X0 150.0f
#define SET_CAR_X1 490.0f
#define SET_CAR_Y0 150.0f
#define SET_CAR_Y1 250.0f

/* the car's tune: the FULL TUNE's setting, else the story's steps (the card's tune, which
 * the arcade's every mode races on; a stock car's setting and card showed 280 hp for a car
 * the story had brought to 400, Allan 10-25) */
void menu_tune(const menu_state *m, int *power, int *handling) {
  int full = m->tune_full[m->maker][m->car];
  *power = full ? m->tune_power[m->maker][m->car] : m->st_power[m->maker][m->car];
  *handling = full ? CAR_TUNE_TOTAL - *power : m->st_handling[m->maker][m->car];
}

/* the chips' left edges: STOCK, FULL TUNE, AT, MT */
static void setting_chips(float x[4]) {
  x[0] = 24.0f;
  x[1] = x[0] + anm_sprite_width(anm_sprite(SET_TEXT)) + SET_CHIP_GAP;
  x[3] = 616.0f - anm_sprite_width(anm_sprite(SET_TEXT + 3));
  x[2] = x[3] - SET_CHIP_GAP - anm_sprite_width(anm_sprite(SET_TEXT + 2));
}

/* one step of the car setting; returns 1 when it goes on */
static int setting_step(menu_state *m, int tap, int key) {
  int mk = m->maker, c = m->car;
  m->spin -= 0.3f;
  if (key) {
    int p = m->tune_power[mk][c] - key;
    if (!m->tune_full[mk][c] || p < SET_MIN_POWER || p > CAR_TUNE_MAX) return 0;
    snd_menu(SND_CURSOR);
    m->tune_power[mk][c] = (signed char)p;
    m->garage_changed = 1;
    return 0;
  }
  if (!tap) return 0;
  float x[4];
  setting_chips(x);
  if (m->tap_y >= SET_ROW_Y - 16.0f && m->tap_y < SET_ROW_Y + 46.0f) {
    for (int k = 3; k >= 0; --k) {
      if (m->tap_x < x[k] - SET_CHIP_GAP * 0.5f) continue;
      if (m->tap_x > x[k] + anm_sprite_width(anm_sprite(SET_TEXT + k)) + SET_CHIP_GAP * 0.5f) break;
      if (k < 2 && m->tune_locked) return 0;
      signed char *v = k < 2 ? &m->tune_full[mk][c] : NULL;
      int now = k < 2 ? *v : m->manual, want = k & 1;
      if (now != want) {
        snd_menu(SND_CURSOR);
        if (v) *v = (signed char)want;
        else m->manual = want;
        m->garage_changed = 1;
      }
      return 0;
    }
    return 0;
  }
  if (m->tap_x >= SET_CAR_X0 && m->tap_x < SET_CAR_X1 && m->tap_y >= SET_CAR_Y0 && m->tap_y < SET_CAR_Y1) {
    snd_menu(SND_SELECT);
    return 1;
  }
  return 0;
}

static void draw_setting(const menu_state *m, int clock, void (*draw3d)(void *), void *ctx, int bars) {
  const cardb_car *c = chosen_car(m, m->car);
  int power, handling;
  menu_tune(m, &power, &handling);
  int full = m->tune_full[m->maker][m->car];
  draw_back(ID.bg_carsel, clock % 240, 1);
  draw_bg_logo(c, clock % 240, 1);
  anm_draw(ID.line, clock % anm_frames(ID.line), 1);
  if (draw3d) draw3d(ctx);
  /* the choices on the course band, the chosen ones lit */
  anm_draw_sprite(anm_sprite(T_SET_COURSE_BAND), -1.7f, 84.3f, 0.5f);
  float x[4];
  setting_chips(x);
  int lit[4] = { !full, full, !m->manual, m->manual };
  /* a saved car's mode fixed: only its chip */
  for (int k = 0; k < 4; ++k)
    if (!(k < 2 && m->tune_locked && !lit[k]))
      anm_draw_sprite(anm_sprite(SET_TEXT + k), x[k], SET_ROW_Y, lit[k] ? 1.0f : 0.3f);
  if (full) anm_draw(ID.set_handle, clock % anm_frames(ID.set_handle), 1);
  /* the gauges */
  anm_draw_sprite(anm_sprite(T_SET_BAND), -192.0f, 246.0f, 0.5f);
  anm_draw_sprite(anm_sprite(T_SET_POWER), 7.0f, 256.0f, 1);
  anm_draw_sprite(anm_sprite(T_SET_HANDLING), 6.0f, 290.0f, 1);
  for (int row = 0; row < 2; ++row) {
    int n = row ? handling : power;
    float y = row ? 304.0f : 266.0f;
    for (int i = 0; i < CAR_TUNE_MAX; ++i) {
      float gx = (float)GAGE_X[i] + GAGE_DX - 32.0f, gy = y - 32.0f;
      anm_draw_sprite(anm_sprite(T_SET_EMPTY), gx, gy, 1);
      if (i < n) anm_draw_sprite(anm_sprite(T_SET_LIGHT + i / 10), gx, gy, 1);
    }
  }
  /* the horsepower (three digits and HP) between the choices, the left/right keys having
   * the place menu_layout_setting gives it */
  int hp = car_tune_hp(c->hp, power);
  for (int i = 0, div = 100; i < 3; ++i, div /= 10)
    anm_draw_sprite(anm_sprite(T_SET_DIGIT + hp / div % 10), SET_HP_X + 22.0f * (float)i, SET_ROW_Y - 18.0f, 1);
  anm_draw_sprite(anm_sprite(T_SET_HP), SET_HP_X + 70.0f, SET_ROW_Y + 2.0f, 1);
  /* the balance badges, the car's lit, drawn in from the keys */
  int grade = car_tune_grade(power, handling);
  for (int k = 0; k < 5; ++k)
    anm_draw_sprite(anm_sprite(SET_BADGE[k][k == grade]), SET_BADGE_X + SET_BADGE_DX * (float)k - 64.0f, 347.5f - 32.0f, 1);
  anm_swap title = { anm_sprite(T_TOPBAR_MES), anm_sprite(T_SET_TITLE) };
  draw_topbar(bars, 1, &title);
  anm_draw(ID.bottom, bars, 1);
  int hint = anm_sprite(SET_TEXT + 4);
  if (!bars) anm_draw_sprite(hint, 320.0f - anm_sprite_width(hint) * 0.5f, 427.0f, 0.75f + 0.25f * cosf((float)clock * 0.1f));
}

/* The setting's sprites (the wheel prompt's too), kept up from the colour select on and
 * uploaded one a frame, so neither the setting's first frame nor a tap on FULL TUNE takes
 * the uploads. */
static void setting_warm(void) {
  enum { MAX_WARM = 96 };
  static int list[MAX_WARM], n = -1;
  if (n < 0) {
    static const int ids[] = { T_SET_BAND, T_SET_POWER, T_SET_HANDLING, T_SET_LIGHT, T_SET_LIGHT + 1, T_SET_LIGHT + 2,
                               T_SET_EMPTY, T_SET_HP, T_SET_COURSE_BAND, T_SET_TITLE };
    n = 0;
    for (int k = 0; k < 5; ++k) list[n++] = anm_sprite(SET_TEXT + k);
    for (int k = 0; k < (int)(sizeof ids / sizeof ids[0]); ++k) list[n++] = anm_sprite(ids[k]);
    for (int k = 0; k < 10; ++k) list[n++] = anm_sprite(T_SET_DIGIT + k);
    for (int k = 0; k < 5; ++k) list[n++] = anm_sprite(SET_BADGE[k][0]), list[n++] = anm_sprite(SET_BADGE[k][1]);
    n += anm_sprites(ID.set_handle, list + n, MAX_WARM - n);
  }
  int fresh = 0;
  for (int i = 0; i < n; ++i) fresh += anm_warm_sprite(list[i], !fresh);
}

/* ---- the port's OPTION screen (not the arcade's: its cabinet has a wheel), from the gear
 * key on the SAVE DATA screen or the maker select, in the car setting's style: CONTROLS,
 * steering by the touch keys (BUTTONS) or by tilting the iPad (GYRO) and the tilt's
 * sensitivity (1..10, the left/right keys or a tap on its lights); GRAPHICS, the bridge
 * reflection on the car, the FSR sharpening, the cars' baked ambient occlusion
 * (tools/stage/bake_ao.c) and the back mirror (app.c draw_backmirror). The back key goes
 * back to the screen it came from. ---- */
#define OPT_TEXT 101900 /* menupack.py OPTION_TEXT_BASE */
enum {
  O_CONTROLS, O_BUTTONS, O_GYRO, O_STEERING, O_SENS, O_GRAPHICS, O_REFLECTION, O_ON, O_OFF, O_TILT, O_TITLE,
  O_SHARP, O_HIDDEN /* the maker select's hidden cars key */, O_PEDALS, O_AO, O_BM,
  O_ALL_ON, O_ALL_OFF /* the colour select's ALL PARTS key */,
  O_HD, O_BLOOM, O_SSAO, O_WET, O_PAINT, O_AA, O_4X, O_2X /* Android's GRAPHICS rows */, O_N
};
#define OPT_CHIP_X 300.0f
#define OPT_STEER_Y 152.0f /* the rows' middles */
#define OPT_SENS_Y 182.0f
#define OPT_PEDAL_Y 212.0f
#define OPT_GRAPHICS_Y 230.0f /* its band's top */
#define OPT_REFL_Y 288.0f
#define OPT_SHARP_Y 310.0f
#define OPT_AO_Y 332.0f
#define OPT_BM_Y 354.0f
#define OPT_RES_Y 376.0f /* (an iPad's: res_row) */
#define OPT_ROW_HALF 15.0f /* a row's reach for a tap, the GRAPHICS rows' */
#define OPT_GRAPHICS_HALF 11.0f
#define OPT_LIGHT_DX 19.0f

/* a row's two chips' left edges */
static void option_chips(int first, float x[2]) {
  x[0] = OPT_CHIP_X;
  x[1] = x[0] + anm_sprite_width(anm_sprite(OPT_TEXT + first)) + SET_CHIP_GAP;
}

/* which of a row's chips a tap is on (-1 none) */
static int option_chip(const menu_state *m, int first, float y) {
  float x[2], half = y > OPT_GRAPHICS_Y ? OPT_GRAPHICS_HALF : OPT_ROW_HALF;
  option_chips(first, x);
  if (m->tap_y < y - half || m->tap_y >= y + half) return -1;
  for (int k = 1; k >= 0; --k)
    if (m->tap_x >= x[k] - SET_CHIP_GAP * 0.5f &&
        m->tap_x < x[k] + anm_sprite_width(anm_sprite(OPT_TEXT + first + k)) + SET_CHIP_GAP * 0.5f)
      return k;
  return -1;
}

static void option_set(menu_state *m, int *v, int want) {
  if (*v == want) return;
  snd_menu(SND_CURSOR);
  *v = want;
  m->garage_changed = 1;
}

/* Android's GRAPHICS (hq_rows): two columns of five rows, the chips at x */
#define OPT_HQ_Y 288.0f /* the first row's middle */
#define OPT_HQ_DY 23.0f
#define OPT_HQ_HALF 11.0f
static const struct { int label, first; float x, chip_x; } OPT_HQ_ROW[10] = {
  { O_REFLECTION, O_ON, 16, 190 }, { O_SHARP, O_ON, 16, 190 }, { O_AO, O_ON, 16, 190 }, { O_BM, O_ON, 16, 190 },
  { O_HD, O_ON, 16, 190 }, { O_BLOOM, O_ON, 332, 500 }, { O_SSAO, O_ON, 332, 500 }, { O_WET, O_ON, 332, 500 },
  { O_PAINT, O_ON, 332, 500 }, { O_AA, O_4X, 332, 500 },
};

/* the switch a row sets (ON / 4X: 1) */
static int *option_hq_value(menu_state *m, int row) {
  int *const v[10] = { &m->over_on, &m->sharp_on, &m->ao_on, &m->bm_on, &m->hd_tex,
                       &m->bloom_on, &m->ssao_on, &m->wet_on, &m->paint_on, &m->aa4 };
  return v[row];
}

static void option_hq_chips(int row, float x[2]) {
  x[0] = OPT_HQ_ROW[row].chip_x;
  x[1] = x[0] + anm_sprite_width(anm_sprite(OPT_TEXT + OPT_HQ_ROW[row].first)) + SET_CHIP_GAP;
}

static void option_hq_step(menu_state *m) {
  for (int row = 0; row < 10; ++row) {
    float y = OPT_HQ_Y + OPT_HQ_DY * (float)(row % 5), x[2];
    if (m->tap_y < y - OPT_HQ_HALF || m->tap_y >= y + OPT_HQ_HALF) continue;
    option_hq_chips(row, x);
    for (int k = 1; k >= 0; --k)
      if (m->tap_x >= x[k] - SET_CHIP_GAP * 0.5f &&
          m->tap_x < x[k] + anm_sprite_width(anm_sprite(OPT_TEXT + OPT_HQ_ROW[row].first + k)) + SET_CHIP_GAP * 0.5f) {
        option_set(m, option_hq_value(m, row), !k);
        break;
      }
  }
}

static void draw_option_hq(const menu_state *m) {
  anm_draw_sprite(anm_sprite(T_SET_BAND), -192.0f, OPT_HQ_Y - 20.0f, 0.5f);
  for (int row = 0; row < 10; ++row) {
    float y = OPT_HQ_Y + OPT_HQ_DY * (float)(row % 5), x[2];
    int on = *option_hq_value((menu_state *)m, row);
    anm_draw_sprite(anm_sprite(OPT_TEXT + OPT_HQ_ROW[row].label), OPT_HQ_ROW[row].x, y - 10.0f, 1);
    option_hq_chips(row, x);
    for (int k = 0; k < 2; ++k)
      anm_draw_sprite(anm_sprite(OPT_TEXT + OPT_HQ_ROW[row].first + k), x[k], y - 15.0f, on == !k ? 1.0f : 0.3f);
  }
}

/* An iPad's RESOLUTION row (res_row; Allan 10-27, for the Retina iPads after the iPad 3): its
 * words in the HUD's own letters (no pre-rendered ones), at the 640x480 screen's x, y; its two
 * chips' left edges and widths */
static const char *const OPT_RES[2] = { "1024 X 768", "2048 X 1536" };
#define OPT_RES_PX 2.0f
static void option_res_chips(float x[2], float w[2]) {
  for (int k = 0; k < 2; ++k) w[k] = hud_ui_text_width(OPT_RES[k], OPT_RES_PX) / 1.6f;
  x[0] = OPT_CHIP_X;
  x[1] = x[0] + w[0] + SET_CHIP_GAP;
}

static void option_res_step(menu_state *m) {
  float x[2], w[2];
  if (!m->res_row || m->tap_y < OPT_RES_Y - OPT_GRAPHICS_HALF || m->tap_y >= OPT_RES_Y + OPT_GRAPHICS_HALF) return;
  option_res_chips(x, w);
  for (int k = 1; k >= 0; --k)
    if (m->tap_x >= x[k] - SET_CHIP_GAP * 0.5f && m->tap_x < x[k] + w[k] + SET_CHIP_GAP * 0.5f) {
      option_set(m, &m->hires, k);
      break;
    }
}

/* (its band is draw_option's, under the bars; the words over them, the HUD's) */
static void draw_option_res(const menu_state *m, float screen_w, float screen_h) {
  float x[2], w[2], k = 1.6f, top = OPT_RES_Y * k - 3.5f * OPT_RES_PX;
  hud_ui_begin(screen_w, screen_h);
  hud_ui_text("RESOLUTION", 16.0f * k, top, OPT_RES_PX, 1, 1, 1, 1);
  option_res_chips(x, w);
  for (int i = 0; i < 2; ++i) hud_ui_text(OPT_RES[i], x[i] * k, top, OPT_RES_PX, 1, 1, 1, m->hires == i ? 1.0f : 0.3f);
  hud_ui_text("(NEXT START)", (x[1] + w[1] + SET_CHIP_GAP) * k, top, OPT_RES_PX, 1, 1, 1, 0.6f);
  hud_ui_end();
}

/* ---- the OPTION screen's KEY LAYOUT (MENU_LAYOUT, Allan 10-29: the touch keys' size and
 * place the player's own): the race's keys over the screen as they will be, a group dragged
 * by a finger moves (kept on the screen), the panel's - and + size the chosen group, RESET ALL
 * puts every group back, DONE (or the back key) back to OPTION. Its key on the OPTION screen
 * on the PEDALS row's right. Its words the HUD's (no pre-rendered ones), in the 1024 x 768
 * layout. ---- */
enum { LAY_SMALLER, LAY_BIGGER, LAY_RESET, LAY_DONE, LAY_BUTTONS };
static const float LAY_PANEL[4] = { 272, 228, 480, 270 };
static const float LAY_BUTTON[LAY_BUTTONS][4] = { { 296, 356, 110, 56 }, { 618, 356, 110, 56 }, { 296, 426, 204, 56 },
                                                  { 524, 426, 204, 56 } };
static const char *const LAY_GROUP[3] = { "TOP KEYS", "LEFT KEYS", "RIGHT KEYS" };
/* the OPTION screen's key to it (640 x 480) */
static const float OPT_LAYOUT_KEY[4] = { 446, 198, 176, 28 };
#define LAY_K_STEP 10

static int layout_button(float x, float y) {
  for (int b = 0; b < LAY_BUTTONS; ++b) {
    const float *r = LAY_BUTTON[b];
    if (x >= r[0] - 6 && x < r[0] + r[2] + 6 && y >= r[1] - 6 && y < r[1] + r[3] + 6) return b;
  }
  return -1;
}

static void layout_clamp(menu_state *m, int g) {
  float dx = (float)m->key_dx[g], dy = (float)m->key_dy[g];
  hud_layout_clamp(g, m->gyro, (float)m->key_k[g] * 0.01f, &dx, &dy);
  m->key_dx[g] = (int)floorf(dx + 0.5f), m->key_dy[g] = (int)floorf(dy + 0.5f);
}

static void layout_apply(const menu_state *m) {
  for (int g = 0; g < HUD_KEY_GROUPS; ++g) hud_layout_set(g, (float)m->key_k[g] * 0.01f, (float)m->key_dx[g], (float)m->key_dy[g]);
}

static void layout_enter(menu_state *m) {
  go(m, MENU_LAYOUT);
  m->lay_drag = m->lay_tap = 0;
  m->lay_touch = 0;
  if (m->lay_sel < 0 || m->lay_sel >= HUD_KEY_GROUPS) m->lay_sel = HUD_KEYS_RIGHT;
  layout_apply(m);
}

/* its touches (menu_touch's, all phases): a panel button, or a group taken and dragged; 0 for
 * one not its own (the back key's) */
static int layout_touch(menu_state *m, void *id, int phase, float x, float y) {
  if (phase == TOUCH_BEGAN && !m->lay_drag) {
    int b = layout_button(x, y);
    if (b >= 0) {
      m->lay_tap = b + 1;
      return 1;
    }
    layout_apply(m);
    int g = hud_layout_hit(m->gyro, x, y);
    if (g < 0) return 0;
    m->lay_sel = g;
    m->lay_drag = 1, m->lay_touch = id;
    m->lay_x = x - (float)m->key_dx[g], m->lay_y = y - (float)m->key_dy[g];
    return 1;
  }
  if (!m->lay_drag || id != m->lay_touch) return phase != TOUCH_BEGAN;
  int g = m->lay_sel;
  m->key_dx[g] = (int)floorf(x - m->lay_x + 0.5f), m->key_dy[g] = (int)floorf(y - m->lay_y + 0.5f);
  layout_clamp(m, g);
  layout_apply(m);
  if (phase == TOUCH_ENDED) {
    m->lay_drag = 0, m->lay_touch = 0;
    m->garage_changed = 1;
  }
  return 1;
}

static void layout_step(menu_state *m, int back) {
  int b = m->lay_tap - 1;
  m->lay_tap = 0;
  if (back || b == LAY_DONE) {
    snd_menu(SND_CANCEL);
    m->lay_drag = 0, m->lay_touch = 0;
    go(m, MENU_OPTION);
    return;
  }
  int g = m->lay_sel;
  if (b == LAY_SMALLER || b == LAY_BIGGER) {
    int k = m->key_k[g] + (b == LAY_BIGGER ? LAY_K_STEP : -LAY_K_STEP);
    if (k < (int)(HUD_KEY_K_MIN * 100 + 0.5f) || k > (int)(HUD_KEY_K_MAX * 100 + 0.5f)) return;
    snd_menu(SND_CURSOR);
    m->key_k[g] = k;
    layout_clamp(m, g);
    m->garage_changed = 1;
  } else if (b == LAY_RESET) {
    snd_menu(SND_SELECT);
    for (int i = 0; i < HUD_KEY_GROUPS; ++i) m->key_k[i] = 100, m->key_dx[i] = m->key_dy[i] = 0;
    m->garage_changed = 1;
  }
  layout_apply(m);
}

static void layout_text_mid(const char *s, float y, float px, float r, float g, float b) {
  hud_ui_text(s, 512.0f - 0.5f * hud_ui_text_width(s, px), y, px, r, g, b, 1);
}

static void draw_layout(const menu_state *m, int clock, float screen_w, float screen_h) {
  draw_back(ID.bg, clock % 600, 1);
  anm_fill(0, 0, 0, 0.6f);
  hud_ui_begin(screen_w, screen_h);
  const float *p = LAY_PANEL;
  hud_ui_rect(p[0], p[1], p[2], p[3], 0.03f, 0.04f, 0.06f, 0.82f);
  hud_ui_rect(p[0], p[1], p[2], 3, 1.0f, 0.42f, 0.05f, 0.95f);
  layout_text_mid("KEY LAYOUT", p[1] + 16, 4, 1, 1, 1);
  layout_text_mid("DRAG A GROUP OF KEYS TO MOVE IT", p[1] + 62, 2, 0.8f, 0.8f, 0.8f);
  char line[48];
  int g = m->lay_sel >= 0 && m->lay_sel < 3 ? m->lay_sel : 0;
  layout_text_mid(LAY_GROUP[g], p[1] + 92, 2.5f, 1.0f, 0.55f, 0.15f);
  static const char *const LABEL[LAY_BUTTONS] = { "-", "+", "RESET ALL", "DONE" };
  for (int b = 0; b < LAY_BUTTONS; ++b) {
    const float *r = LAY_BUTTON[b];
    float px = b < 2 ? 5.0f : 3.0f;
    hud_ui_button(r[0], r[1], r[2], r[3], m->lay_tap == b + 1);
    hud_ui_text(LABEL[b], r[0] + 0.5f * (r[2] - hud_ui_text_width(LABEL[b], px)), r[1] + 0.5f * (r[3] - 7.0f * px), px, 1, 1,
                1, 1);
  }
  snprintf(line, sizeof line, "SIZE %d", m->key_k[g]);
  layout_text_mid(line, LAY_BUTTON[0][1] + 0.5f * (LAY_BUTTON[0][3] - 7.0f * 3), 3, 1, 1, 1);
  hud_ui_end();
  layout_apply(m);
  hud_draw_layout(screen_w, screen_h, m->gyro, g);
}

/* the OPTION screen's key to KEY LAYOUT (its words the HUD's, over the bars like RESOLUTION's) */
static void draw_option_layout_key(float screen_w, float screen_h) {
  const float *r = OPT_LAYOUT_KEY, k = 1.6f;
  hud_ui_begin(screen_w, screen_h);
  hud_ui_button(r[0] * k, r[1] * k, r[2] * k, r[3] * k, 0);
  const char *s = "KEY LAYOUT >";
  hud_ui_text(s, (r[0] + 0.5f * r[2]) * k - 0.5f * hud_ui_text_width(s, 2.5f), (r[1] + 0.5f * r[3]) * k - 3.5f * 2.5f, 2.5f, 1,
              1, 1, 1);
  hud_ui_end();
}

static void option_step(menu_state *m, int tap, int key) {
  if (key) {
    int s = m->gyro_sens + key;
    if (m->gyro && s >= 1 && s <= 10) option_set(m, &m->gyro_sens, s);
    return;
  }
  if (!tap) return;
  const float *lk = OPT_LAYOUT_KEY;
  if (m->tap_x >= lk[0] && m->tap_x < lk[0] + lk[2] && m->tap_y >= lk[1] - 4 && m->tap_y < lk[1] + lk[3] + 4) {
    snd_menu(SND_SELECT);
    layout_enter(m);
    return;
  }
  int k = option_chip(m, O_BUTTONS, OPT_STEER_Y);
  if (k >= 0) option_set(m, &m->gyro, k);
  k = option_chip(m, O_ON, OPT_PEDAL_Y);
  if (k >= 0) option_set(m, &m->pedal_slide, !k);
  if (m->hq_rows) {
    option_hq_step(m);
    k = -1;
  } else {
    k = option_chip(m, O_ON, OPT_REFL_Y);
  }
  if (k >= 0) option_set(m, &m->over_on, !k);
  k = m->hq_rows ? -1 : option_chip(m, O_ON, OPT_SHARP_Y);
  if (k >= 0) option_set(m, &m->sharp_on, !k);
  k = m->hq_rows ? -1 : option_chip(m, O_ON, OPT_AO_Y);
  if (k >= 0) option_set(m, &m->ao_on, !k);
  k = m->hq_rows ? -1 : option_chip(m, O_ON, OPT_BM_Y);
  if (k >= 0) option_set(m, &m->bm_on, !k);
  option_res_step(m);
  /* a light of the gauge: that sensitivity */
  if (m->gyro && m->tap_y >= OPT_SENS_Y - OPT_ROW_HALF && m->tap_y < OPT_SENS_Y + OPT_ROW_HALF) {
    int i = (int)floorf((m->tap_x - OPT_CHIP_X) / OPT_LIGHT_DX);
    if (i >= 0 && i < 10) option_set(m, &m->gyro_sens, i + 1);
  }
}

static void draw_option(const menu_state *m, int clock, int bars) {
  draw_back(ID.bg, clock % 600, 1);
  anm_draw(ID.line, clock % anm_frames(ID.line), 1);
  /* CONTROLS */
  anm_draw_sprite(anm_sprite(T_SET_COURSE_BAND), -1.7f, 84.3f, 0.5f);
  anm_draw_sprite(anm_sprite(OPT_TEXT + O_CONTROLS), 24.0f, 101.0f, 1);
  anm_draw_sprite(anm_sprite(T_SET_BAND), -192.0f, OPT_STEER_Y - 20.0f, 0.5f);
  float x[2];
  anm_draw_sprite(anm_sprite(OPT_TEXT + O_STEERING), 16.0f, OPT_STEER_Y - 10.0f, 1);
  option_chips(O_BUTTONS, x);
  for (int k = 0; k < 2; ++k)
    anm_draw_sprite(anm_sprite(OPT_TEXT + O_BUTTONS + k), x[k], OPT_STEER_Y - 15.0f, m->gyro == k ? 1.0f : 0.3f);
  /* the sensitivity: ten lights (the setting's gauge, lit by the s1..s3 colours) and its number */
  float a = m->gyro ? 1.0f : 0.3f;
  anm_draw_sprite(anm_sprite(OPT_TEXT + O_SENS), 16.0f, OPT_SENS_Y - 10.0f, a);
  for (int i = 0; i < 10; ++i) {
    float gx = OPT_CHIP_X + OPT_LIGHT_DX * ((float)i + 0.5f) - 32.0f, gy = OPT_SENS_Y - 32.0f;
    anm_draw_sprite(anm_sprite(T_SET_EMPTY), gx, gy, a);
    if (i < m->gyro_sens) anm_draw_sprite(anm_sprite(T_SET_LIGHT + (i < 4 ? 0 : i < 8 ? 1 : 2)), gx, gy, a);
  }
  float nx = OPT_CHIP_X + OPT_LIGHT_DX * 10.0f - 10.0f;
  if (m->gyro_sens >= 10) anm_draw_sprite(anm_sprite(T_SET_DIGIT + 1), nx, OPT_SENS_Y - 33.0f, a);
  anm_draw_sprite(anm_sprite(T_SET_DIGIT + m->gyro_sens % 10), nx + 22.0f, OPT_SENS_Y - 33.0f, a);
  /* the slider pedals (Allan, 10-25): ON, the throttle and brake keys take the finger's height */
  option_chips(O_ON, x);
  anm_draw_sprite(anm_sprite(OPT_TEXT + O_PEDALS), 16.0f, OPT_PEDAL_Y - 10.0f, 1);
  for (int k = 0; k < 2; ++k)
    anm_draw_sprite(anm_sprite(OPT_TEXT + O_ON + k), x[k], OPT_PEDAL_Y - 15.0f, m->pedal_slide == !k ? 1.0f : 0.3f);
  /* GRAPHICS */
  anm_draw_sprite(anm_sprite(T_SET_COURSE_BAND), -1.7f, OPT_GRAPHICS_Y, 0.5f);
  anm_draw_sprite(anm_sprite(OPT_TEXT + O_GRAPHICS), 24.0f, OPT_GRAPHICS_Y + 17.0f, 1);
  if (m->hq_rows) draw_option_hq(m);
  else anm_draw_sprite(anm_sprite(T_SET_BAND), -192.0f, OPT_REFL_Y - 20.0f, 0.5f);
  option_chips(O_ON, x);
  for (int row = 0; !m->hq_rows && row < 4; ++row) {
    static const float Y[4] = { OPT_REFL_Y, OPT_SHARP_Y, OPT_AO_Y, OPT_BM_Y };
    static const int LABEL[4] = { O_REFLECTION, O_SHARP, O_AO, O_BM };
    const int *const ON[4] = { &m->over_on, &m->sharp_on, &m->ao_on, &m->bm_on };
    float y = Y[row];
    int on = *ON[row];
    anm_draw_sprite(anm_sprite(OPT_TEXT + LABEL[row]), 16.0f, y - 10.0f, 1);
    for (int k = 0; k < 2; ++k) anm_draw_sprite(anm_sprite(OPT_TEXT + O_ON + k), x[k], y - 15.0f, on == !k ? 1.0f : 0.3f);
  }
  if (m->res_row) anm_draw_sprite(anm_sprite(T_SET_BAND), -192.0f, OPT_RES_Y - 20.0f, 0.5f);
  anm_swap title = { anm_sprite(T_TOPBAR_MES), anm_sprite(OPT_TEXT + O_TITLE) };
  draw_topbar(bars, 1, &title);
  anm_draw(ID.bottom, bars, 1);
  int hint = anm_sprite(OPT_TEXT + O_TILT);
  if (!bars && m->gyro)
    anm_draw_sprite(hint, 320.0f - anm_sprite_width(hint) * 0.5f, 427.0f, 0.75f + 0.25f * cosf((float)clock * 0.1f));
}

/* ---- the port's SAVE DATA screen (the arcade keeps a player on a card, checked between
 * START and the car select; the iPad keeps MENU_SAVES saves instead), in the OPTION
 * screen's style: NEW GAME or LOAD GAME on the course band (a tap, or the left/right keys),
 * under it the saves, each its cars, their story episodes won and 10-in-a-row gold medals
 * and when it was last saved. LOAD GAME loads a tapped save with data; NEW GAME starts over
 * in a tapped empty save, in one with data on a second tap. The back key goes back to the
 * title screen. ---- */
#define SAVE_TEXT 101950  /* menupack.py SAVE_TEXT_BASE */
#define SAVE_GLYPH 101980 /* menupack.py SAVE_GLYPH_BASE: 0..9, '/', ':' in font 1 */
#define SAVE_ADV 8.0f     /* font 1's advance */
/* the player's name's characters (namedb.h) in font 8, 10 and 5: menupack.py NAME_GLYPH8_BASE
 * (then the grid's back space and space marks), NAME_GLYPH10_BASE, NAME_GLYPH5_BASE */
#define NAME_GLYPH8 106000
#define NAME_GLYPH10 106300
#define NAME_GLYPH5 106600
enum {
  S_TITLE, S_NEW, S_LOAD, S_SLOT, S_NO_DATA = S_SLOT + MENU_SAVES, S_STORY, S_MEDALS, S_CARS, S_HINT_NEW, S_HINT_LOAD,
  S_HINT_AGAIN, S_ERASE_HINT = S_HINT_AGAIN + MENU_SAVES, S_DELETE, S_HINT_DEL, S_HINT_DEL_AGAIN
};
#define SAVE_Y0 152.0f /* the first save's band */
#define SAVE_DY 64.0f
#define SAVE_H 58.0f
#define SAVE_INFO_X 176.0f
/* the "i" key on the chips' row and its page (MENU_INFO, Allan 10-27: who made the port and
 * that it is not Bandai Namco's): menupack.py INFO_KEY, INFO_PAGE (tools/infopage.py, drawn
 * there at 1.6 times this space's size) */
#define INFO_KEY 108000
#define INFO_PAGE 108001
#define INFO_SCALE 1.6f
static const float INFO_KEY_RECT[4] = { 572, 96, 40, 40 };

static void save_enter(menu_state *m) {
  go(m, MENU_SAVE);
  m->save_new = m->save_del = 0;
  m->save_confirm = 0;
  m->save_scan = 1;
}

/* the chips: NEW GAME, LOAD GAME, DELETE (Allan 10-25) */
static int save_chip_text(int k) { return anm_sprite(SAVE_TEXT + (k < 2 ? S_NEW + k : S_DELETE)); }

/* the chips' left edges */
static void save_chips(float x[3]) {
  x[0] = 24.0f;
  for (int k = 1; k < 3; ++k) x[k] = x[k - 1] + anm_sprite_width(save_chip_text(k - 1)) + SET_CHIP_GAP;
}

static int save_chip(const menu_state *m) { return m->save_del ? 2 : m->save_new ? 0 : 1; }

static void save_chip_set(menu_state *m, int k) {
  snd_menu(SND_CURSOR);
  m->save_new = k == 0;
  m->save_del = k == 2;
  m->save_confirm = 0;
}

/* one step of the save screen; returns 1 when a save is chosen (save_pick set) */
static int save_step(menu_state *m, int tap, int key) {
  if (key) {
    int k = save_chip(m) + key;
    if (k >= 0 && k < 3) save_chip_set(m, k);
    return 0;
  }
  if (!tap) return 0;
  const float *ik = INFO_KEY_RECT;
  if (anm_sprite(INFO_KEY) >= 0 && m->tap_x >= ik[0] - 8 && m->tap_x < ik[0] + ik[2] + 8 && m->tap_y >= ik[1] - 8 &&
      m->tap_y < ik[1] + ik[3] + 8) {
    snd_menu(SND_SELECT);
    m->save_confirm = 0;
    go(m, MENU_INFO);
    return 0;
  }
  if (m->tap_y >= SET_ROW_Y - 16.0f && m->tap_y < SET_ROW_Y + 46.0f) {
    float x[3];
    save_chips(x);
    for (int k = 2; k >= 0; --k) {
      if (m->tap_x < x[k] - SET_CHIP_GAP * 0.5f) continue;
      if (m->tap_x > x[k] + anm_sprite_width(save_chip_text(k)) + SET_CHIP_GAP * 0.5f) break;
      if (k != save_chip(m)) save_chip_set(m, k);
      return 0;
    }
    return 0;
  }
  int k = (int)floorf((m->tap_y - SAVE_Y0) / SAVE_DY);
  if (k < 0 || k >= MENU_SAVES || m->tap_y - SAVE_Y0 - SAVE_DY * (float)k >= SAVE_H) return 0;
  const menu_save_info *s = &m->save[k];
  if (!m->save_new && !s->used) {
    snd_menu(SND_CANCEL);
    return 0;
  }
  if ((m->save_new || m->save_del) && s->used && m->save_confirm != k + 1) {
    snd_menu(SND_CURSOR);
    m->save_confirm = k + 1;
    return 0;
  }
  if (m->save_del) {
    /* tapped again: deleted (the app takes it off and scans the saves again) */
    snd_menu(SND_SELECT);
    m->save_confirm = 0;
    m->save_erase = k + 1;
    return 0;
  }
  snd_menu(SND_SELECT);
  m->save_confirm = 0;
  m->save_pick = k + 1;
  m->save_pick_new = m->save_new;
  m->sel = k;
  return 1;
}

/* a number or a date in font 1's glyphs from (x, y); returns the x after it */
static float save_glyphs(const char *text, float x, float y, float a) {
  for (const char *c = text; *c; ++c, x += SAVE_ADV) {
    int g = *c >= '0' && *c <= '9' ? *c - '0' : *c == '/' ? 10 : *c == ':' ? 11 : -1;
    if (g >= 0) anm_draw_sprite(anm_sprite(SAVE_GLYPH + g), x, y, a);
  }
  return x;
}

static void draw_save(const menu_state *m, int clock, int bars) {
  draw_back(ID.bg, clock % 600, 1);
  anm_draw(ID.line, clock % anm_frames(ID.line), 1);
  anm_draw_sprite(anm_sprite(T_SET_COURSE_BAND), -1.7f, 84.3f, 0.5f);
  float x[3];
  save_chips(x);
  for (int k = 0; k < 3; ++k) anm_draw_sprite(save_chip_text(k), x[k], SET_ROW_Y, save_chip(m) == k ? 1.0f : 0.3f);
  static const float BAND[4] = { 192, 0, 832, 128 };
  for (int k = 0; k < MENU_SAVES; ++k) {
    const menu_save_info *s = &m->save[k];
    float y = SAVE_Y0 + SAVE_DY * (float)k;
    /* a save to be tapped again blinks; LOAD GAME dims the empty ones */
    float band = m->save_confirm == k + 1 ? 0.5f + 0.3f * cosf((float)clock * 0.2f) : 0.5f;
    float a = !m->save_new && !s->used ? 0.3f : 1.0f;
    const float dst[4] = { 0, y, 640, y + SAVE_H };
    anm_draw_part(anm_sprite(T_SET_BAND), BAND, dst, band);
    anm_draw_sprite(anm_sprite(SAVE_TEXT + S_SLOT + k), 16.0f, y + 14.0f, a);
    if (!s->used) {
      anm_draw_sprite(anm_sprite(SAVE_TEXT + S_NO_DATA), SAVE_INFO_X, y + 19.0f, 0.5f * a);
      continue;
    }
    char num[32];
    float lx = SAVE_INFO_X;
    const int label[3] = { S_CARS, S_STORY, S_MEDALS }, value[3] = { s->cars, s->story, s->medals };
    static const float LABEL_X[3] = { 0, 90, 260 };
    for (int i = 0; i < 3; ++i) {
      int t = anm_sprite(SAVE_TEXT + label[i]);
      anm_draw_sprite(t, lx + LABEL_X[i], y + 8.0f, a);
      snprintf(num, sizeof num, "%d", value[i]);
      save_glyphs(num, lx + LABEL_X[i] + anm_sprite_width(t) - 4.0f, y + 8.0f, a);
    }
    time_t when = (time_t)s->when;
    struct tm tm;
    if (when > 0 && localtime_r(&when, &tm)) {
      strftime(num, sizeof num, "%Y/%m/%d %H:%M", &tm);
      save_glyphs(num, lx, y + 30.0f, a);
    }
    /* the player's name, in the name grid's font */
    for (int i = 0; i < 5 && s->name[i] >= 0; ++i)
      anm_draw_sprite(anm_sprite(NAME_GLYPH8 + s->name[i]), lx + 170.0f + 24.0f * (float)i, y + 23.0f, a);
  }
  anm_swap title = { anm_sprite(T_TOPBAR_MES), anm_sprite(SAVE_TEXT + S_TITLE) };
  draw_topbar(bars, 1, &title);
  anm_draw(ID.bottom, bars, 1);
  int hint = anm_sprite(SAVE_TEXT + (m->save_del ? (m->save_confirm ? S_HINT_DEL_AGAIN + m->save_confirm - 1 : S_HINT_DEL)
                                     : m->save_confirm ? S_HINT_AGAIN + m->save_confirm - 1
                                     : m->save_new ? S_HINT_NEW : S_HINT_LOAD));
  if (!bars) anm_draw_sprite(hint, 320.0f - anm_sprite_width(hint) * 0.5f, 427.0f, 0.75f + 0.25f * cosf((float)clock * 0.1f));
  int key = anm_sprite(INFO_KEY);
  if (key >= 0) {
    float src[4] = { 0, 0, anm_sprite_width(key), anm_sprite_height(key) };
    const float *r = INFO_KEY_RECT;
    float dst[4] = { r[0], r[1], r[0] + r[2], r[1] + r[3] };
    anm_draw_part(key, src, dst, 1);
  }
}

/* the "i" page: the port's own words over the SAVE DATA screen's background; a tap or the
 * back key back to it */
static void draw_info(int clock, int bars) {
  draw_back(ID.bg, clock % 600, 1);
  anm_draw(ID.line, clock % anm_frames(ID.line), 1);
  int page = anm_sprite(INFO_PAGE);
  if (page >= 0) {
    float w = anm_sprite_width(page), h = anm_sprite_height(page);
    float src[4] = { 0, 0, w, h };
    /* between the top bar and the bottom one */
    float k = fminf(1.0f / INFO_SCALE, 322.0f / h), x = 320.0f - 0.5f * w * k, y = 72.0f;
    float dst[4] = { x, y, x + w * k, y + h * k };
    anm_draw_part(page, src, dst, 1);
  }
  anm_swap title = { anm_sprite(T_TOPBAR_MES), anm_sprite(SAVE_TEXT + S_TITLE) };
  draw_topbar(bars, 1, &title);
  anm_draw(ID.bottom, bars, 1);
}

/* ---- the save's cars: each one of the arcade's cards (clV386CardData: its car, tune,
 * parts, story and 10-in-a-row results). menu_state keeps the tune and parts per car (by
 * src/cardb.h's maker and car, so a save has one car of a kind), the story and the
 * 10-in-a-row per car too, the driven car's in st_div, st_bits and ten_result. ---- */
int menu_own_find(const menu_state *m, int maker, int car) {
  for (int k = 0; k < m->n_own; ++k)
    if (m->own[k].maker == maker && m->own[k].car == car) return k;
  return -1;
}

void menu_own_store(menu_state *m) {
  if (m->own_sel < 0 || m->own_sel >= m->n_own) return;
  int mk = m->own[m->own_sel].maker, c = m->own[m->own_sel].car;
  m->st_div_car[mk][c] = m->st_div;
  m->st_bits_car[mk][c] = m->st_bits;
  memcpy(m->ten_car[mk][c], m->ten_result, sizeof m->ten_result);
}

void menu_own_load(menu_state *m, int k) {
  if (k < 0 || k >= m->n_own) return;
  const menu_own *o = &m->own[k];
  int mk = o->maker, c = o->car;
  m->own_sel = k;
  m->maker = mk, m->car = c, m->color = o->color;
  m->st_div = m->st_div_car[mk][c];
  m->st_bits = m->st_bits_car[mk][c];
  memcpy(m->ten_result, m->ten_car[mk][c], sizeof m->ten_result);
}

/* ---- the class (GetClass 0x857bb0c over the table at 0x9332980, src/hud.h HUD_CLASSES
 * rows): the sum of four counts, each the last row k such that rows 0..k all hold, 46 at
 * most: the maxi coins (card+0x1a8) and the points at card+0x180 at least the rows' first and
 * second columns (the ghost battles' and the versus', which the port has not: 0 here, no row
 * past N), the story by the third column (over 99: the story's loops, getStoryClearLoop, at
 * least column / 100; else isStoryClearOnce(column - 1)), plus the 10-in-a-row's courses
 * with gold (3) on all of their levels 0..9 (getTenResult). ---- */
static const short CLASS_STORY[47] = { 1, 10, 20, 30, 40, 50, 60, 70, 80, 100, 200, 300, 400, 500, 700, 1000,
                                       1500, 2000, 2500, 3000, 3500, 4000, 4500, 5000, 5500, 6000, 6500, 7000,
                                       7500, 8000, 8500, 9000, 9500, 10000, 10500, 11000, 11500, 12000, 12500,
                                       13000, 13500, 14000, 14500, 15000, 15500, 16000, 16500 };

/* clV386CardData::isStoryClearOnce(n): the story's episode n (0..99, the five divisions of
 * 20 of a loop) won once: card+0x270 the divisions cleared (st_div), +0x188 the episodes of
 * the one under way (st_bits) */
static int story_clear_once(int div, int bits, int n) {
  if (div / 5 > 0) return 1;
  int d = n % 100 / 20;
  return d < div % 5 || (d == div % 5 && (bits >> (n % 20) & 1));
}

int menu_class(const menu_state *m, int mk, int c) {
  int div = m->st_div_car[mk][c], bits = m->st_bits_car[mk][c], story = 0, ten = 0;
  for (int k = 0; k < 47; ++k) {
    int col = CLASS_STORY[k];
    if (!(col > 99 ? div / 5 >= col / 100 : story_clear_once(div, bits, col - 1))) break;
    story = k;
  }
  for (int course = 0; course < 10; ++course) {
    int all = 1;
    for (int level = 0; level < 10; ++level) all &= m->ten_car[mk][c][course][level] >= 3;
    ten += all;
  }
  return story + ten > 46 ? 46 : story + ten;
}

void menu_class_update(menu_state *m) {
  menu_own_store(m);
  for (int k = 0; k < m->n_own; ++k) {
    int mk = m->own[k].maker, c = m->own[k].car, cls = menu_class(m, mk, c);
    if (cls > m->class_car[mk][c]) m->class_car[mk][c] = (signed char)cls;
  }
}

/* a new card's (and a deleted one's): stock, no parts, no results */
static void own_clear(menu_state *m, int mk, int c) {
  m->tune_full[mk][c] = 0, m->tune_power[mk][c] = CAR_TUNE_MAX;
  m->st_power[mk][c] = m->st_handling[mk][c] = m->st_gauge[mk][c] = 0;
  m->dress_lv[mk][c] = m->aero[mk][c] = m->mirror[mk][c] = m->wing[mk][c] = m->bonnet[mk][c] = 0;
  m->wheel[mk][c] = 0;
  m->neon[mk][c] = 0;
  m->vinyl[mk][c] = 0;
  m->st_div_car[mk][c] = m->st_bits_car[mk][c] = 0;
  memset(m->ten_car[mk][c], 0, sizeof m->ten_car[mk][c]);
  m->class_car[mk][c] = 0;
  m->vs_stars_car[mk][c] = m->vs_count_car[mk][c] = 0;
  m->st_bonus_car[mk][c] = 0;
}

/* the colour select's ALL PARTS key (a new kind of car's card, or one already in the save) */
static int all_parts_shown(const menu_state *m) {
  return m->screen == MENU_COLOR && !m->save_skip && !m->deciding;
}

/* the car the colour select decided, as a new card (one of its kind already in the save is
 * that one, in the new colour); it is the car driven from now on */
static void own_register(menu_state *m) {
  menu_own_store(m);
  int mk = m->maker, c = m->car, k = menu_own_find(m, mk, c);
  m->tune_locked = k >= 0;
  if (k < 0 && m->n_own < MENU_OWN_MAX) {
    k = m->n_own++;
    m->own[k] = (menu_own){ (signed char)mk, (signed char)c, (signed char)m->color, (long long)time(NULL), 0 };
    /* (the mode the setting chose kept) */
    signed char full = m->tune_full[mk][c], power = m->tune_power[mk][c];
    own_clear(m, mk, c);
    if (m->new_card) m->tune_full[mk][c] = full, m->tune_power[mk][c] = power;
  }
  if (m->all_parts && k >= 0) {
    /* ALL PARTS: every dress-up part won (aero sets, wings, bonnets, mirrors, wheels, neons,
     * stickers: level 63); the tune stays the setting's (STOCK the story's own, from none: it
     * gave a full tune with STOCK chosen, Allan 10-29) */
    m->dress_lv[mk][c] = 63;
    m->garage_changed = 1;
  }
  m->all_parts = 0;
  m->new_card = 0;
  if (k < 0) return;
  m->own[k].color = (signed char)m->color;
  menu_own_load(m, k);
  m->garage_changed = 1;
}

/* ---- HAVE: clSeqCardHaveSelectThread ("Do you have a card?"): its spin box
 * (menu_layout_yn_card_1_gold, the time zone select's) with the YES card
 * (menu_p_yn_card_have_yes, 0x1720) in place of the morning one and NO (0x13c5) in place
 * of the night one; here YES goes to the save's cars and NO to a new car, captioned so
 * (menupack.py CARD_HAVE). The left/right keys or a tap on the arrows or the other card
 * turn it, a tap on the card at rest decides it. The port's KEEP is the same box after a new
 * car's card: YES keeps the car in the save, NO takes it out again (Allan 10-25). ---- */
#define CARD_TEXT 105000 /* menupack.py CARD_TEXT_BASE: */
enum {
  C_HAVE_TITLE, C_GARAGE_TITLE, C_GARAGE_HINT, C_CARD_HINT, C_PLAYER, C_TITLE0 = C_PLAYER + MENU_SAVES, C_HP, C_KM,
  C_KEEP_TITLE, C_HAVE_YES, C_HAVE_NO, C_KEEP_YES, C_KEEP_NO
};
#define CARD_GLYPH2 105020 /* font 2: 0..9, '-', ',' */
#define CARD_GLYPH5 105040 /* font 5: 0..9, '-' */
#define CARD_CAR 105100    /* each car's name, two lines */
#define CARD_COLOR 105200  /* the colour names, by cardb's text (100002 on) */
#define CARD_CLASS 105400  /* the title row by class: "B7級　/湾岸の新人" */
#define CARD_CLASSP 105450 /* and without the title: "B7級　/" */
/* the titles' characters (src/titledb.h TITLEDB_CHAR) in fonts 1, 5 and 7: menupack.py
 * TITLE_GLYPH1_BASE, TITLE_GLYPH5_BASE, TITLE_GLYPH7_BASE */
#define TITLE_GLYPH1 110000
#define TITLE_GLYPH5 112000
#define TITLE_GLYPH7 114000
enum { T_CARD_MENU = 0x846, S_CARD_MENU_R35 = 0x11ed, S_CARD_TOPBAR = 0x1db2 };

static void have_enter(menu_state *m) {
  go(m, MENU_HAVE);
  m->have_sel = 0;
  m->slide = m->card_t = 0;
  m->bars = 1;
  m->keep_ask = 0;
}

static void keep_enter(menu_state *m) {
  have_enter(m);
  m->keep_ask = 1;
}

/* KEEP's NO: the new car out of the save again, as if never chosen */
static void keep_drop(menu_state *m) {
  int k = menu_own_find(m, m->maker, m->car);
  if (k < 0) return;
  /* (its ALL PARTS on again for the card made next) */
  if (m->dress_lv[m->maker][m->car] == 63) m->all_parts = 1;
  own_clear(m, m->maker, m->car);
  for (int i = k; i + 1 < m->n_own; ++i) m->own[i] = m->own[i + 1];
  m->n_own--;
  if (m->own_sel == k) m->own_sel = -1;
  else if (m->own_sel > k) m->own_sel--;
  m->garage_changed = 1;
}

/* the tap on the card at rest (sprite 0), or near a sprite of the spin box (its arrows) */
static int have_on(const menu_state *m, int sprite) {
  float r[4];
  int f = 100 * m->have_sel + m->card_t % 60;
  if (sprite) {
    if (!anm_sprite_rect(ID.day_cards, f, anm_sprite(sprite), r)) return 0;
    return m->tap_x >= r[0] - 24.0f && m->tap_x < r[2] + 24.0f && m->tap_y >= r[1] - 24.0f && m->tap_y < r[3] + 24.0f;
  }
  if (!anm_sprite_rect(ID.day_cards, f, anm_sprite(m->have_sel ? T_NIGHT : T_MORNING), r)) return 0;
  float cx = (r[0] + r[2]) * 0.5f, cy = (r[1] + r[3]) * 0.5f, rad = (r[2] - r[0]) * 0.5f;
  return (m->tap_x - cx) * (m->tap_x - cx) + (m->tap_y - cy) * (m->tap_y - cy) < rad * rad;
}

/* one step of HAVE; returns 1 when it decides */
static int have_step(menu_state *m, int tap, int key) {
  if (m->slide) {
    if (--m->slide == 0) {
      m->have_sel ^= 1;
      m->card_t = 0;
    }
    return 0;
  }
  m->card_t++;
  if (key || (tap && (have_on(m, T_ARROW) || have_on(m, T_ARROW2) || have_on(m, m->have_sel ? T_MORNING : T_NIGHT)))) {
    snd_menu(SND_CURSOR);
    m->slide = SLIDE;
    return 0;
  }
  if (tap && have_on(m, 0)) {
    snd_menu(SND_SELECT);
    return 1;
  }
  return 0;
}

static void draw_have(const menu_state *m, int clock, float fade, int bars) {
  draw_back(ID.bg, clock % 600, fade);
  anm_draw(ID.line, 0, fade);
  int z = 100 * m->have_sel, f = z + m->card_t % 60;
  if (m->slide) f = z + 90 + SLIDE - m->slide;
  if (m->deciding) {
    int t = DECIDE_DAY + FADE - m->deciding;
    f = z + 60 + (t < DECIDE_DAY ? t : DECIDE_DAY - 1);
  }
  int yes = m->keep_ask ? C_KEEP_YES : C_HAVE_YES, no = m->keep_ask ? C_KEEP_NO : C_HAVE_NO;
  anm_swap cards[2] = { { anm_sprite(T_MORNING), anm_sprite(CARD_TEXT + yes) },
                        { anm_sprite(T_NIGHT), anm_sprite(CARD_TEXT + no) } };
  anm_swap rings[2] = { { anm_sprite(T_RING), anm_sprite(0xb95) }, { anm_sprite(T_RING_BLUE), anm_sprite(0x1b0b) } };
  anm_draw_ex(ID.day_cards, f, fade, 0, 0, cards, 2);
  if (!m->slide) anm_draw_ex(ID.day_ring, z + (m->card_t < 59 ? m->card_t : 59), fade, 0, 0, rings, 2);
  anm_swap title = { anm_sprite(T_TOPBAR_MES), anm_sprite(CARD_TEXT + (m->keep_ask ? C_KEEP_TITLE : C_HAVE_TITLE)) };
  draw_topbar(bars, fade, &title);
  anm_draw(ID.bottom, bars, fade);
}

/* ---- GARAGE: the port's look at the save's cars, on the colour select's turntable in the
 * parts they wear, with the car select's name panel and colour plate; the left/right keys
 * (or a tap either side) go round them, a tap on the car takes it (then its card). ---- */
static void garage_show(menu_state *m, int k) {
  m->own_view = k;
  m->maker = m->own[k].maker, m->car = m->own[k].car, m->color = m->own[k].color;
}

static void garage_enter(menu_state *m, int k) {
  go(m, MENU_GARAGE);
  garage_show(m, k >= 0 && k < m->n_own ? k : 0);
  m->spin = -24.0f;
  m->bars = 1;
  m->erase_armed = 0;
}

/* the port's: the car shown deleted from the save with its tune, parts and results (the
 * key, a bin in the gear key's place, pressed twice); the next one shown, none left: a new
 * car */
static void garage_erase(menu_state *m) {
  int k = m->own_view;
  if (k < 0 || k >= m->n_own) return;
  menu_own_store(m);
  own_clear(m, m->own[k].maker, m->own[k].car);
  for (int i = k; i + 1 < m->n_own; ++i) m->own[i] = m->own[i + 1];
  m->n_own--;
  if (m->own_sel == k) m->own_sel = -1;
  else if (m->own_sel > k) m->own_sel--;
  m->garage_changed = 1;
  m->erase_armed = 0;
  if (!m->n_own) {
    car_select_enter(m, MENU_MAKER);
    m->bars = 1;
    return;
  }
  garage_show(m, k < m->n_own ? k : m->n_own - 1);
  m->spin = -24.0f;
}

static int garage_step(menu_state *m, int tap, int key) {
  m->spin -= 0.3f;
  if (!tap && !key) return 0;
  int d = key;
  if (!d && m->tap_x < SET_CAR_X0) d = -1;
  if (!d && m->tap_x >= SET_CAR_X1) d = 1;
  if (d) {
    if (m->n_own < 2) return 0;
    snd_menu(SND_CURSOR);
    garage_show(m, (m->own_view + d + m->n_own) % m->n_own);
    m->spin = -24.0f;
    return 0;
  }
  if (m->loading || m->tap_y < SET_CAR_Y0 - 40.0f || m->tap_y >= SET_CAR_Y1 + 60.0f) return 0;
  snd_menu(SND_SELECT);
  return 1;
}

#define GARAGE_HP_X 492.0f
#define GARAGE_HP_Y 112.0f

static void draw_garage(const menu_state *m, int clock, void (*draw3d)(void *), void *ctx, int bars) {
  const cardb_car *c = chosen_car(m, m->car);
  draw_back(ID.bg_carsel, clock % 240, 1);
  draw_bg_logo(c, clock % 240, 1);
  anm_draw(ID.line, clock % anm_frames(ID.line), 1);
  if (draw3d) draw3d(ctx);
  anm_swap sw[3 + 12 + 1];
  int n = car_swaps(c, sw);
  anm_draw_ex(ID.car_info, clock % 240, 1, 0, 0, sw, n);
  if (m->n_own > 1) anm_draw(ID.car_arrow, clock % 60, 1);
  anm_swap none = { anm_sprite(T_COL_FONT), -1 };
  anm_draw_ex(ID.col_name, 0, 1, 0, 0, &none, 1);
  int name = anm_sprite(c->colors[m->color].name);
  anm_draw_sprite(name, 624.0f - anm_sprite_width(name), 316.0f, 1);
  anm_swap title = { anm_sprite(T_TOPBAR_MES), anm_sprite(CARD_TEXT + C_GARAGE_TITLE) };
  draw_topbar(bars, 1, &title);
  anm_draw(ID.bottom, bars, 1);
  if (bars) return;
  /* which of them: n/N */
  char num[16];
  snprintf(num, sizeof num, "%d/%d", m->own_view + 1, m->n_own);
  save_glyphs(num, 600.0f - SAVE_ADV * (float)strlen(num), 92.0f, 1);
  /* its horsepower (the setting's digits): the full tune's, else the story's steps (Allan 10-25) */
  int mk = m->maker, cr = m->car;
  int power = m->tune_full[mk][cr] ? m->tune_power[mk][cr] : m->st_power[mk][cr];
  int hp = car_tune_hp(c->hp, power);
  for (int i = 0, div = 100; i < 3; ++i, div /= 10)
    anm_draw_sprite(anm_sprite(T_SET_DIGIT + hp / div % 10), GARAGE_HP_X + 22.0f * (float)i, GARAGE_HP_Y, 1);
  anm_draw_sprite(anm_sprite(T_SET_HP), GARAGE_HP_X + 70.0f, GARAGE_HP_Y + 20.0f, 1);
  int hint = anm_sprite(m->erase_armed ? SAVE_TEXT + S_ERASE_HINT : CARD_TEXT + C_GARAGE_HINT);
  anm_draw_sprite(hint, 320.0f - anm_sprite_width(hint) * 0.5f, 427.0f, 0.75f + 0.25f * cosf((float)clock * 0.1f));
}

/* ---- CARD: clSeqCardDisp1, the card's first page ("Card data confirmed (1/9)", the top bar
 * 0x1db2): menu_g_cardcheck_1_waku (Machine Registration; the R35's menu 0x11ed in place of
 * 0x846) over menu_g_bg_image, menu_layout_cardcheck_1's fields in its fonts (datrums 11):
 * font 5 the name (0x7ff), the card's number (0x58b, card+0x11c % 10000 as "%02d-%02d"; here
 * the save's and the car's) and the title (0x800); font 2 the car's name in two lines
 * (0x7da, 0x7db: get_car_model_data +0, +4), the colour (0x7dc, after its swatch in place of
 * 0x1de2), the horsepower (0x7dd), the mileage (0x7de, getOdoCount, "%d km" with commas);
 * the mileage password and the card's plays left (0x7df, 0x7e0) are the arcade card's own.
 * A tap goes on to the car setting. ---- */
enum {
  T_CARD_NO = 0x58b, T_CARD_NAME = 0x7ff, T_CARD_TITLE = 0x800, T_CARD_CAR1 = 0x7da, T_CARD_CAR2 = 0x7db,
  T_CARD_COLOR = 0x7dc, T_CARD_HP = 0x7dd, T_CARD_ODO = 0x7de, T_CARD_PASS = 0x7df, T_CARD_LIFE = 0x7e0,
  T_CARD_SWATCH = 0x1de2
};

static void card_enter(menu_state *m, int back) {
  go(m, MENU_CARD);
  m->card_back = back;
  m->bars = 1;
}

/* ---- the player's name (seqCardNameSelect, clSeqCardNameSelectThread), asked for as soon
 * as a NEW GAME starts, as the arcade asks a new card's:
 * - menu_g_name_name_back behind the five big characters (font 10, at 89 + 86i, 96), each over
 *   its menu_use_name line (138 + 86i, 196; the next one's menu_g_name_line blinking);
 * - menu_layout_name_select's frame and in it the page's grid (font 8, namedb.h's columns
 *   and rows); under it the page tabs and END (menu_g_name_*, at 285 + 68k, 362; the digits'
 *   tab is the target search's);
 * - the wheel moves the arcade's cursor and the gas pedal puts its character in, which pops
 *   (1 + 0.1n, n 5..0): here a tap on a character puts it in, on a tab changes the page (as do
 *   the left/right keys), on END takes the name (not an empty one: the arcade's
 *   menu_g_name_no_name). ← and the back key take the last character off, the back key on an
 *   empty name goes back to the saves; ゛ and ゜ voice the character before. ---- */
#define NAME_POP 5
#define NAME_X 89.0f
#define NAME_DX 86.0f
#define NAME_Y 96.0f
/* the tabs sit on the bottom bar between its keys (Allan 10-25; the arcade's 285 + 68k, 362
 * is the frame's last row, under the grid's) */
#define NAME_TAB_X 183.0f
#define NAME_TAB_DX 68.0f
#define NAME_TAB_Y 425.0f
#define S_NAME_TOPBAR 6351 /* menu_p_topbar_mes_name_1 */
#define S_NAME_RING 5587   /* menu_p_name_select_ring */

static int name_len(const menu_state *m) {
  int n = 0;
  while (n < 5 && m->name[n] >= 0) n++;
  return n;
}

static void name_enter(menu_state *m) {
  go(m, MENU_NAME);
  memset(m->name, -1, sizeof m->name);
  m->name_page = 0;
  m->name_key = -1;
  m->name_pop = m->name_char_pop = 0;
  m->bars = 1;
}

static void name_erase(menu_state *m) {
  int n = name_len(m);
  if (!n) return;
  snd_menu(SND_CANCEL);
  m->name[n - 1] = -1;
  m->name_char_pop = 0;
}

/* a tap on the name screen; returns 1 when the name is taken */
static int name_tap(menu_state *m) {
  float x = m->tap_x, y = m->tap_y;
  int n = name_len(m);
  for (int k = 0; k < 5; ++k) {
    if (fabsf(x - (NAME_TAB_X + NAME_TAB_DX * (float)k)) >= 32.0f || fabsf(y - NAME_TAB_Y) >= 18.0f) continue;
    if (k == 4) {
      if (!n) {
        snd_menu(SND_CANCEL);
        return 0;
      }
      snd_menu(SND_SELECT);
      m->garage_changed = 1;
      return 1;
    }
    if (k != m->name_page) {
      snd_menu(SND_CURSOR);
      m->name_page = k;
    }
    return 0;
  }
  int row = (int)floorf((y - (float)NAMEDB_GRID_Y[0] + 4.0f) / 32.0f), col = 0;
  if (row < 0 || row > 2) return 0;
  for (int c = 1; c < 20; ++c)
    if (fabsf(x - 12.0f - (float)NAMEDB_GRID_X[c]) < fabsf(x - 12.0f - (float)NAMEDB_GRID_X[col])) col = c;
  if (fabsf(x - 12.0f - (float)NAMEDB_GRID_X[col]) >= 16.0f) return 0;
  int key = row * 20 + col, v = NAMEDB_GRID[m->name_page][key];
  if (v == NAMEDB_BS) {
    if (n) name_erase(m);
    else snd_menu(SND_CANCEL);
  } else if (v == NAMEDB_DAKU || v == NAMEDB_HANDAKU) {
    int to = n ? NAMEDB_VOICED[m->name[n - 1]][v == NAMEDB_HANDAKU] : -1;
    if (to < 0) {
      snd_menu(SND_CANCEL);
      return 0;
    }
    snd_menu(SND_CURSOR);
    m->name[n - 1] = (short)to;
    m->name_char_pop = NAME_POP;
  } else if (v >= 0) {
    if (n == 5) {
      snd_menu(SND_CANCEL);
      return 0;
    }
    snd_menu(SND_CURSOR);
    m->name[n] = (short)v;
    m->name_char_pop = NAME_POP;
  } else {
    return 0;
  }
  m->name_key = key;
  m->name_pop = NAME_POP;
  return 0;
}

/* a glyph sprite (square) from (x, y), scaled about its middle */
static void name_glyph(int sprite, float x, float y, float size, float scale, float a) {
  if (sprite < 0) return;
  const float src[4] = { 0, 0, size, size };
  float c = size * 0.5f, h = c * scale;
  const float dst[4] = { x + c - h, y + c - h, x + c + h, y + c + h };
  anm_draw_part(sprite, src, dst, a);
}

static void draw_name(const menu_state *m, int clock, int bars) {
  draw_back(ID.bg, clock % 600, 1);
  anm_draw(ID.line, clock % anm_frames(ID.line), 1);
  anm_draw(ID.name_back, 0, 1);
  int n = name_len(m);
  for (int i = 0; i < 5; ++i) {
    float x = NAME_X + NAME_DX * (float)i;
    anm_draw_ex(ID.name_use, i < n ? 0 : 1, 1, x + 49.0f, 196.0f, NULL, 0);
    if (i == n) anm_draw_ex(ID.name_line, clock % anm_frames(ID.name_line), 1, x + 49.0f, 196.0f, NULL, 0);
    if (i < n) {
      float pop = i == n - 1 ? 1.0f + 0.1f * (float)m->name_char_pop : 1.0f;
      name_glyph(anm_sprite(NAME_GLYPH10 + m->name[i]), x, NAME_Y, 100.0f, pop, 1);
    }
  }
  anm_draw(ID.name_select, 0, 1);
  for (int key = 0; key < 60; ++key) {
    int v = NAMEDB_GRID[m->name_page][key];
    int g = v == NAMEDB_BS ? NAMEDB_N : v == NAMEDB_DAKU ? NAMEDB_DAKU_MARK : v == NAMEDB_HANDAKU ? NAMEDB_HANDAKU_MARK
          : v >= 0 && NAMEDB_WCHAR[v] == 0x3000 ? NAMEDB_N + 1 : v;
    if (g < 0) continue;
    float x = (float)NAMEDB_GRID_X[key % 20], y = (float)NAMEDB_GRID_Y[key / 20];
    int hit = key == m->name_key && m->name_pop;
    if (hit) {
      float ring = anm_sprite_width(anm_sprite(S_NAME_RING));
      name_glyph(anm_sprite(S_NAME_RING), x + 12.0f - ring * 0.5f, y + 16.0f - ring * 0.5f, ring, 1.0f,
                 (float)m->name_pop / NAME_POP);
    }
    name_glyph(anm_sprite(NAME_GLYPH8 + g), x, y, 32.0f, hit ? 1.0f + 0.1f * (float)m->name_pop : 1.0f, 1);
  }
  anm_swap title = { anm_sprite(T_TOPBAR_MES), anm_sprite(S_NAME_TOPBAR) };
  draw_topbar(bars, 1, &title);
  anm_draw(ID.bottom, bars, 1);
  for (int k = 0; k < 5 && !bars; ++k) {
    int lit = k == 4 ? n > 0 : k == m->name_page;
    anm_draw_ex(ID.name_tab[k], lit ? 0 : anm_frames(ID.name_tab[k]) - 1, 1, NAME_TAB_X + NAME_TAB_DX * (float)k,
                NAME_TAB_Y, NULL, 0);
  }
}

/* text in a font's glyphs (CARD_GLYPH2 / 5: digits, then the extra characters) from (x, y);
 * returns the x after it */
static float card_glyphs(int base, const char *extra, float adv, const char *text, float x, float y, float a) {
  for (const char *c = text; *c; ++c, x += adv) {
    const char *e = strchr(extra, *c);
    int g = *c >= '0' && *c <= '9' ? *c - '0' : e ? 10 + (int)(e - extra) : -1;
    if (g >= 0 && *c) anm_draw_sprite(anm_sprite(base + g), x, y, a);
  }
  return x;
}

/* the place of a field of menu_layout_cardcheck_1 */
static int card_field(int t, float *x, float *y) {
  float r[4];
  if (!anm_sprite_rect(ID.card_layout, 0, anm_sprite(t), r)) return 0;
  *x = r[0], *y = r[1];
  return 1;
}

static void draw_card(const menu_state *m, int clock, float fade, int bars) {
  const menu_own *o = &m->own[m->own_sel >= 0 ? m->own_sel : 0];
  const cardb_car *c = &CARDB_CARS[o->maker][o->car];
  draw_back(ID.bg, clock % 600, fade);
  anm_swap menu = { anm_sprite(T_CARD_MENU), anm_sprite(has(c->label, "[R35]") ? S_CARD_MENU_R35 : T_CARD_MENU) };
  anm_draw_ex(ID.card_waku, 0, fade, 0, 0, &menu, 1);
  anm_swap sw[11] = {
    { anm_sprite(T_CARD_NO), -1 }, { anm_sprite(T_CARD_NAME), -1 }, { anm_sprite(T_CARD_TITLE), -1 },
    { anm_sprite(T_CARD_CAR1), -1 }, { anm_sprite(T_CARD_CAR2), -1 }, { anm_sprite(T_CARD_COLOR), -1 },
    { anm_sprite(T_CARD_HP), -1 }, { anm_sprite(T_CARD_ODO), -1 }, { anm_sprite(T_CARD_PASS), -1 },
    { anm_sprite(T_CARD_LIFE), -1 }, { anm_sprite(T_CARD_SWATCH), anm_sprite(c->colors[o->color].swatch) },
  };
  anm_draw_ex(ID.card_layout, 0, fade, 0, 0, sw, 11);
  float x, y;
  char text[32];
  int slot = m->save_slot >= 1 && m->save_slot <= MENU_SAVES ? m->save_slot : 1;
  if (card_field(T_CARD_NO, &x, &y)) {
    snprintf(text, sizeof text, "%02d-%02d", slot, (m->own_sel >= 0 ? m->own_sel : 0) + 1);
    card_glyphs(CARD_GLYPH5, "-", 16.0f, text, x, y, fade);
  }
  if (card_field(T_CARD_NAME, &x, &y)) {
    /* the player's name in font 5 (its full width 28), a save without one PLAYER n */
    if (m->name[0] >= 0 && anm_sprite(NAME_GLYPH5) >= 0)
      for (int i = 0; i < 5 && m->name[i] >= 0; ++i) anm_draw_sprite(anm_sprite(NAME_GLYPH5 + m->name[i]), x + 28.0f * (float)i, y, fade);
    else
      anm_draw_sprite(anm_sprite(CARD_TEXT + C_PLAYER + slot - 1), x, y, fade);
  }
  if (card_field(T_CARD_TITLE, &x, &y)) {
    /* the class's "B7級　/" (its advances: the sprite less the cell, plus the '/''s half),
     * then the car's title in font 5's glyphs (a menu.bin from before them: the row with
     * 湾岸の新人, or before the classes the title alone) */
    int cls = m->class_car[o->maker][o->car], pre = anm_sprite(CARD_CLASSP + cls);
    int title = m->title_car[o->maker][o->car] > 0 ? m->title_car[o->maker][o->car] : TITLE_FIRST;
    const unsigned short *g;
    int n = title_glyphs(title, &g);
    if (pre >= 0 && n > 0 && anm_sprite(TITLE_GLYPH5 + g[0]) >= 0) {
      anm_draw_sprite(pre, x, y, fade);
      x += anm_sprite_width(pre) - 32.0f + 16.0f;
      for (int i = 0; i < n; ++i) {
        anm_draw_sprite(anm_sprite(TITLE_GLYPH5 + g[i]), x, y, fade);
        x += title_glyph_half(g[i]) ? 16.0f : 28.0f;
      }
    } else {
      int row = anm_sprite(CARD_CLASS + cls);
      anm_draw_sprite(row >= 0 ? row : anm_sprite(CARD_TEXT + C_TITLE0), x, y, fade);
    }
  }
  int flat = o->car;
  for (int mk = 0; mk < o->maker; ++mk) flat += CARDB_NCARS[mk];
  if (card_field(T_CARD_CAR1, &x, &y)) anm_draw_sprite(anm_sprite(CARD_CAR + 2 * flat), x, y, fade);
  if (card_field(T_CARD_CAR2, &x, &y)) anm_draw_sprite(anm_sprite(CARD_CAR + 2 * flat + 1), x, y, fade);
  if (card_field(T_CARD_COLOR, &x, &y))
    anm_draw_sprite(anm_sprite(CARD_COLOR + c->colors[o->color].name - 100002), x, y, fade);
  if (card_field(T_CARD_HP, &x, &y)) {
    int power = m->tune_full[o->maker][o->car] ? m->tune_power[o->maker][o->car] : m->st_power[o->maker][o->car];
    snprintf(text, sizeof text, "%d", car_tune_hp(c->hp, power));
    x = card_glyphs(CARD_GLYPH2, "-,", 8.0f, text, x, y, fade);
    anm_draw_sprite(anm_sprite(CARD_TEXT + C_HP), x, y, fade);
  }
  if (card_field(T_CARD_ODO, &x, &y)) {
    long km = (long)(o->odo / 1000.0);
    char digits[24];
    snprintf(digits, sizeof digits, "%ld", km);
    int n = (int)strlen(digits), j = 0;
    for (int i = 0; i < n; ++i) {
      if (i && (n - i) % 3 == 0) text[j++] = ',';
      text[j++] = digits[i];
    }
    text[j] = 0;
    x = card_glyphs(CARD_GLYPH2, "-,", 8.0f, text, x, y, fade);
    anm_draw_sprite(anm_sprite(CARD_TEXT + C_KM), x + 8.0f, y, fade);
  }
  anm_swap title = { anm_sprite(T_TOPBAR_MES), anm_sprite(S_CARD_TOPBAR) };
  draw_topbar(bars, fade, &title);
  anm_draw(ID.bottom, bars, fade);
  int hint = anm_sprite(CARD_TEXT + C_CARD_HINT);
  if (!bars) anm_draw_sprite(hint, 320.0f - anm_sprite_width(hint) * 0.5f, 427.0f, fade * (0.75f + 0.25f * cosf((float)clock * 0.1f)));
}

/* ---- the race's pause menu: the arcade's continue dialog (clSeqContinueThread sets up
 * menu_layout_yn_card_1_gold with YES 0x13ca and NO 0x10a1 in place of the morning and
 * night cards, under "Continue?" 0x1ab0), at 60 % in a box (menu_p_maxi_tornament_waku,
 * drawn without its rule) over the race. A tap on YES decides it; a tap on NO slides the
 * spin box over to it, then decides it. ---- */
enum { T_YES = 0x13ca, T_NO = 0x10a1, T_CONTINUE = 0x1ab0, T_PANEL = 0x1a75 };
#define PAUSE_K 0.6f
#define PAUSE_DX (320.0f * (1.0f - PAUSE_K)) /* the cards scaled about (320, 262) */
#define PAUSE_DY (262.0f * (1.0f - PAUSE_K))
#define PAUSE_IN 8 /* frames the box takes to fade in */

void menu_pause_open(menu_state *m) {
  memset(&m->pause, 0, sizeof m->pause);
  m->pause.open = 1;
  snd_menu(SND_CURSOR);
}

/* whether the pause menu's tap is on card k (0 YES, 1 NO) */
static int pause_on_card(const menu_pause *p, int k) {
  float r[4];
  if (!anm_sprite_rect(ID.day_cards, 100 * p->sel + p->card_t % 60, anm_sprite(k ? T_NIGHT : T_MORNING), r)) return 0;
  float cx = PAUSE_DX + PAUSE_K * (r[0] + r[2]) * 0.5f, cy = PAUSE_DY + PAUSE_K * (r[1] + r[3]) * 0.5f;
  float rad = PAUSE_K * (r[2] - r[0]) * 0.42f + 10.0f; /* the disc, and a finger's slack */
  return (p->tap_x - cx) * (p->tap_x - cx) + (p->tap_y - cy) * (p->tap_y - cy) < rad * rad;
}

static void pause_decide(menu_pause *p) {
  snd_menu(p->sel ? SND_CANCEL : SND_SELECT);
  p->deciding = DECIDE_DAY + FADE;
}

static int pause_step(menu_pause *p) {
  int tap = p->tap;
  p->tap = 0;
  p->open++;
  if (p->deciding) return --p->deciding ? MENU_PAUSED : p->sel ? MENU_LEAVE : MENU_RESUME;
  if (p->slide) {
    if (--p->slide == 0) {
      p->sel ^= 1;
      p->card_t = 0;
      if (p->go) pause_decide(p);
    }
    return MENU_PAUSED;
  }
  p->card_t++;
  if (!tap || p->open < PAUSE_IN) return MENU_PAUSED;
  if (pause_on_card(p, p->sel)) {
    pause_decide(p);
  } else if (pause_on_card(p, !p->sel)) {
    snd_menu(SND_CURSOR);
    p->slide = SLIDE;
    p->go = 1;
  }
  return MENU_PAUSED;
}

static void go(menu_state *m, int screen);
static void tune_open(menu_state *m);
static void lvup_open(menu_state *m);
static void poem_open(menu_state *m);
static void ending_open(menu_state *m);
static void shogo_open(menu_state *m);

void menu_vs_closed(menu_state *m, const char *note) {
  m->vs.state = MENU_VS_OFF;
  m->vs.n = 0;
  m->finish_story = 0;
  m->finish = 0;
  m->pause.open = 0;
  go(m, MENU_VS);
  m->t = 0;
  m->bars = 1;
  m->tap = m->key = m->key_held = m->back = m->leaving = 0;
  snprintf(m->vs.note, sizeof m->vs.note, "%s", note);
}

/* The race left with the screen white (the pause menu's NO) or black (its end): back on the
 * time zone and BGM select (a story race: the game mode select), out of the white like the car select from the title, or out of
 * the black as from the course select */
static void back_from_race(menu_state *m, int white) {
  /* a story race goes back to the game mode select; one whose select runs a second race on
   * (menu_finish_story's next) to that race's title */
  if (m->finish_story == 3) {
    /* clRaceThread runs the select's next periods on without the menus: no title page, the
     * screen in the race's fade while the next one loads (initStory), then that one (its
     * realtime movie first) in from it */
    m->finish_story = 0;
    go(m, MENU_PRE);
    m->pre_quiet = 1;
    m->pre_fade = m->finish_fade;
    m->t = 0;
    m->tap = m->key = m->key_held = m->back = m->leaving = 0;
    return;
  }
  /* an online versus back to its lobby (the class-up and the rest wait for the next race) */
  if (m->vs.state == MENU_VS_RACE) {
    m->vs.state = MENU_VS_LOBBY;
    m->finish_story = 0;
    /* the stars won or lost first (seqJoStarGet) */
    if (m->star_delta && !white) {
      menu_vs_star_open(m);
      return;
    }
    m->star_delta = 0;
    go(m, MENU_VS);
    m->t = white ? FADE : 0;
    m->bars = 1;
    m->flash = white ? FLASH_LEN : 0;
    m->tap = m->key = m->key_held = m->back = m->leaving = 0;
    return;
  }
  /* the class risen: the class-up screen first (seqLvUp, right after the race), out of the black */
  if (m->lvup_due && !white) {
    lvup_open(m);
    return;
  }
  m->lvup_due = 0;
  /* new titles: the title select (judgeTitle and seqTitleSelect right after seqLvUp), out of
   * the black */
  if (m->title_n && !white) {
    shogo_open(m);
    return;
  }
  m->title_n = 0;
  /* a full story gauge: the tuning screen (mseqStory's seqStTune), out of the black */
  if (m->game == MENU_GAME_STORY && m->st_tune_due && !white) {
    tune_open(m);
    m->tap = m->key = m->key_held = m->back = m->leaving = 0;
    return;
  }
  /* a won select's poem (mseqStory's seqStPoem, after the tuning) */
  if (m->game == MENU_GAME_STORY && m->poem >= 0 && !white) {
    poem_open(m);
    return;
  }
  m->poem = -1;
  if (m->game == MENU_GAME_STORY && m->ending == 1 && !white) {
    ending_open(m);
    return;
  }
  m->ending = 0;
  /* an aero set won: the dress-up screens, out of the black */
  if (!white && tg_due(m)) return;
  /* the 10-in-a-row back to the game mode select too */
  int game = m->game != MENU_GAME_TA;
  go(m, game ? MENU_GAME : MENU_BGM);
  m->t = white ? FADE : 0; /* white: no fade in from black */
  m->sel = game ? game_slot(m->game) : m->bgm;
  m->slide = m->card_t = 0;
  m->bars = 1;
  m->music = 1;
  m->flash = white ? FLASH_LEN : 0;
  m->tap = m->key = m->key_held = m->back = m->leaving = 0;
}

int menu_pause_update(menu_state *m, float dt) {
  m->acc += dt;
  if (m->acc > 0.25f) m->acc = 0.25f;
  while (m->acc >= 1.0f / 60.0f) {
    m->acc -= 1.0f / 60.0f;
    int r = pause_step(&m->pause);
    if (r == MENU_PAUSED) continue;
    m->pause.open = 0;
    if (r == MENU_LEAVE) back_from_race(m, 1);
    return r;
  }
  return MENU_PAUSED;
}

/* ---- the end of a time attack (clRaceThread::result2). At the goal and when the time runs
 * out the consoles go (clStandardConsole and the time attack's draw only in yt::CState 1 and
 * 2: app.c) and the race's song stops as the race ends; it runs on 90 frames past the goal,
 * 60 once the car has stopped after the time ran out (no goal camera in the time attack).
 * Then, with the result music:
 * - clTaResult, over the race running on: FINISH (menu_g_finish_finish) with finish1, and by
 *   its counter finish2 at 150, then the result's rows, each with cutin2: the course (180),
 *   the course's record (220), the car type's (255), the player's own (290), the sections
 *   (325), TOTAL TIME (370), the top speed and the ranking (440). A record the run set has
 *   its NEW mark (template 0x1f59) pulse, and the first of them starts CONGRATULATIONS. A tap
 *   once the counter is past 440 (or the counter past 4199) leaves: the result goes, the
 *   screen fades to black in 1/6 s and stays black 60 frames.
 * - clTaTimeUp: the screen fades to black in 1/6 s, TIME UP (menu_g_finish_timeup_ta) plays
 *   280 frames over the black, 30 frames of black follow.
 * Then the menus come back out of the black (the arcade goes on to its card and continue
 * screens). ---- */
#define FINISH_GOAL_WAIT 90
#define FINISH_TIMEUP_WAIT 60
enum { RES_FINISH2 = 150, RES_COURSE = 180, RES_COURSE_BEST = 220, RES_CAR_BEST = 255, RES_PERSONAL = 290,
       RES_SECTIONS = 325, RES_TOTAL = 370, RES_SPEED = 440, RES_TIMEOUT = 4199 };
#define RES_FINISH_LEN 180
#define RES_ROW_LEN 70       /* the best rows' animation (the NEW mark's pulse) and TOTAL TIME's */
#define RES_TOTAL_LOOP 10    /* TOTAL TIME's zoom in, then frames 10..69 looped */
#define RES_CONGRATS_LEN 240 /* CONGRATULATIONS lights up, then frames 120..239 looped */
#define RES_CONGRATS_LOOP 120
#define RES_OUT_FADE 10
#define STORY_FADE 20 /* clRaceThread's clFader::setFade between a select's periods: 1/3 s */
#define RES_OUT_BLACK 60
#define TIMEUP_FADE 10
#define TIMEUP_LEN 280
#define TIMEUP_BLACK 30
/* nsTaData's digits (menu_p_finish_ta_0..9, 0x93d22c0) and TOTAL TIME's (0x93d2320); the
 * templates setTime7 fills with tens of minutes and m'ss"mmm, for the best rows (0x93d22e8)
 * and TOTAL TIME (0x93d2348), and setTime6 with m'ss"mmm for each section (0x93d2380...);
 * the sections' numbers (0x93d2428) */
#define RES_DIGIT 2291
#define RES_BIG_DIGIT 6219
static const int BEST_TEMPLATE[7] = { 7271, 7265, 7266, 7267, 7268, 7269, 7270 };
static const int TOTAL_TEMPLATE[7] = { 6220, 6221, 6222, 6223, 6224, 6225, 6226 };
static const int SECTION_TEMPLATE[4][6] = {
  { 7275, 7276, 7277, 7278, 7279, 7280 }, { 7281, 7282, 7283, 7284, 7285, 7286 },
  { 7287, 7288, 7289, 7290, 7291, 7292 }, { 7293, 7294, 7295, 7296, 7297, 7298 },
};
static const int SECTION_LABEL[7] = { 7265, 7266, 7267, 7268, 7269, 7270, 7271 };
/* the NEW mark (menu_p_finish_ta_overwrite), the top speed's hundreds, tens and ones, the
 * ranking's; the course's name and time zone (as on the race's title page) */
enum { T_RES_NEW = 0x1f59, RES_NEW = 0x1a70, T_RES_COURSE = 0x1b9a, T_RES_TIME = 0x15ed };
static const int SPEED_TEMPLATE[3] = { 0x1c60, 0x1c61, 0x1c62 };
static const int RANK_TEMPLATE[3] = { 0x1c80, 7267, 7268 };
static const int RES_TIME_NAME[2] = { 4751, 3301 };

/* clTaResult's constructor sets the rows' sprites once (setImageReplace) */
static struct {
  anm_swap course[2];
  anm_swap best[3][8]; /* the time's 7 digits, the NEW mark */
  anm_swap sections[4 * 7], sections2[3 * 7];
  anm_swap total[7];
  anm_swap speed[3], rank[3];
} RES;

/* nsTaData::setTime7: (int)(s * 1000) to 99'59"999, tens of minutes blank below 10' */
static void set_time7(anm_swap *sw, const int *tmpl, int digit, float s) {
  int ms = (int)(s * 1000.0f);
  ms = ms < 0 ? 0 : ms >= 6000000 ? 5999999 : ms;
  int d[7] = { ms / 600000, ms % 600000 / 60000, ms % 60000 / 10000, ms % 10000 / 1000, ms % 1000 / 100, ms % 100 / 10, ms % 10 };
  for (int i = 0; i < 7; ++i) sw[i] = (anm_swap){ anm_sprite(tmpl[i]), i == 0 && ms <= 599999 ? -1 : anm_sprite(digit + d[i]) };
}

/* nsTaData::setTime6: m'ss"mmm to 9'59"999 */
static void set_time6(anm_swap *sw, const int *tmpl, float s) {
  int ms = (int)(s * 1000.0f);
  ms = ms < 0 ? 0 : ms > 599999 ? 599999 : ms;
  int d[6] = { ms / 60000, ms % 60000 / 10000, ms % 10000 / 1000, ms % 1000 / 100, ms % 100 / 10, ms % 10 };
  for (int i = 0; i < 6; ++i) sw[i] = (anm_swap){ anm_sprite(tmpl[i]), anm_sprite(RES_DIGIT + d[i]) };
}

static void result_setup(const menu_state *m) {
  const menu_result *r = &m->result;
  RES.course[0] = (anm_swap){ anm_sprite(T_RES_COURSE), anm_sprite(COURSE_NAME[car_course_id(m->course)]) };
  RES.course[1] = (anm_swap){ anm_sprite(T_RES_TIME), anm_sprite(RES_TIME_NAME[m->zone]) };
  const float best[3] = { r->course_record, r->car_record, r->personal };
  const int news[3] = { r->new_course, r->new_car, r->new_personal };
  for (int k = 0; k < 3; ++k) {
    set_time7(RES.best[k], BEST_TEMPLATE, RES_DIGIT, best[k]);
    RES.best[k][7] = (anm_swap){ anm_sprite(T_RES_NEW), news[k] ? anm_sprite(RES_NEW) : -1 };
  }
  /* menu_g_finish_ta_section_1's four rows (section_2's 5..7 are for course ids 12 and 13) */
  for (int k = 0; k < 4; ++k) {
    RES.sections[7 * k] = (anm_swap){ anm_sprite(SECTION_LABEL[k]), anm_sprite(RES_DIGIT + 1 + k) };
    set_time6(&RES.sections[7 * k + 1], SECTION_TEMPLATE[k], r->section[k]);
  }
  /* section_2's rows 5..7, with rows 1..3's templates */
  for (int k = 0; k < 3; ++k) {
    RES.sections2[7 * k] = (anm_swap){ anm_sprite(SECTION_LABEL[4 + k]), anm_sprite(RES_DIGIT + 5 + k) };
    set_time6(&RES.sections2[7 * k + 1], SECTION_TEMPLATE[k], r->section[4 + k]);
  }
  set_time7(RES.total, TOTAL_TEMPLATE, RES_BIG_DIGIT, r->total);
  int v = r->top_speed < 0 ? 0 : r->top_speed > 999 ? 999 : r->top_speed;
  const int speed[3] = { v / 100, v / 10 % 10, v % 10 };
  for (int i = 0; i < 3; ++i) RES.speed[i] = (anm_swap){ anm_sprite(SPEED_TEMPLATE[i]), anm_sprite(RES_DIGIT + speed[i]) };
  v = r->rank < 0 ? 0 : r->rank > 999 ? 999 : r->rank;
  const int rank[3] = { v / 100, v / 10 % 10, v % 10 };
  for (int i = 0; i < 3; ++i)
    RES.rank[i] = (anm_swap){ anm_sprite(RANK_TEMPLATE[i]), (i == 0 && v < 100) || (i == 1 && v < 10) ? -1 : anm_sprite(RES_DIGIT + rank[i]) };
}

void menu_finish_open(menu_state *m, int goal, const menu_result *res) {
  m->finish = 1;
  m->finish_goal = goal != 0;
  m->finish_wait = goal ? FINISH_GOAL_WAIT : FINISH_TIMEUP_WAIT;
  m->finish_out = 0;
  m->finish_story = 0;
  m->tap = 0;
  m->acc = 0;
  if (goal && res) {
    m->result = *res;
    result_setup(m);
  }
}

void menu_finish_vs(menu_state *m, int place, int n) {
  m->finish_story = place == 1 ? 5 : 6;
  m->finish_goal = 1;
  m->finish_wait = FINISH_GOAL_WAIT;
  m->vs.last_place = place;
  m->vs.last_n = n;
}

void menu_finish_story(menu_state *m, int won, int next, float gap, int fade) {
  m->finish_story = next ? 3 : won ? 1 : 2;
  m->finish_goal = 1;
  /* clRaceThread::race: the Suspend (play state 10) fades out at once, no FINISH */
  m->finish_wait = next ? 0 : FINISH_GOAL_WAIT;
  m->finish_fade = next ? fade : 0;
  m->st_show = 0; /* menu_story_points sets it */
  m->st_gap = gap;
}

/* The 10-in-a-row's end (clSeqTenResultThread::run 0x89632d0, clTenResult): after a TIME UP
 * (or a retire) clMenuFade first darkens the screen in 10 frames and stays black under it.
 * Three phases, each run till its update gives 0 (clTenResult::update), the next one's
 * reset then: 1. WIN (all beaten) or TIME UP for 181 frames; 2. the faces of the round's
 * rivals (menu_layout_or_result_face) with an X (menu_g_or_finish_X, set into the face's
 * dot) over each beaten one, the "10 in a row" header and the medal (menu_layout_or_finish_result);
 * 3. the summary (menu_layout_or_result_A): course, level, the clear time, a character's
 * line picked at random for the result. No skip. */
enum { TEN_FADE = 10, TEN_R1_LEN = 180, TEN_R3_LEN = 360, TEN_MAX = 32, MAX_TEN_WARM = 128 };
static int is_template(int id);
/* clSpriteAnime::setAnime's state: mode 1 a still of frame start, 2 hidden, 4 played from
 * start for len frames (0: to the end) and held */
typedef struct {
  int anim, mode, start, len, f;
} ten_anim;
static void ta_set(ten_anim *a, int mode, int start, int len) {
  a->mode = mode;
  a->start = a->f = start;
  a->len = len;
}
static void ta_update(ten_anim *a) {
  int end = a->len ? a->start + a->len : anm_frames(a->anim);
  if (a->mode == 4 && a->f + 1 < end) a->f++;
}
static void ta_draw(const ten_anim *a, float dx, float dy, const anm_swap *sw, int n) {
  if (a->mode != 2) anm_draw_ex(a->anim, a->f, 1, dx, dy, sw, n);
}

/* the faces by level (0x9770b20; the last one's is the 0x73c template, "?", till he is the
 * one left), the result's medal frames by getTenResult (0x93d04b0), its course names
 * (0x93d1fe0) and levels (0x93d2020), the clear time's templates and digits (0x93d1de8,
 * 0x93d1dc0) and what it blanks when not all were beaten (set_clear_time) */
static const short TEN_FACE_ID[11][TEN_MAX] = {
  { 1821, 1822, 1823, 1824, 1825, 1826, 1827, 1831, 1829, 1850 },
  { 1822, 1824, 1825, 1831, 1830, 1841, 1842, 1845, 1846, 1851 },
  { 1825, 1853, 1829, 1831, 1832, 1841, 1842, 1843, 1849, 1851 },
  { 1853, 1826, 1825, 1827, 1828, 1847, 1842, 1848, 1850, 1851 },
  { 1837, 1838, 1839, 1840, 1836, 1828, 1826, 1833, 1834, 1830 },
  { 1824, 1828, 1841, 1829, 1844, 1849, 1835, 1848, 1836, 1851 },
  { 1829, 1835, 1848, 1837, 1836, 1828, 1833, 1826, 1834, 1830 },
  { 1835, 1838, 1839, 1840, 1834, 1836, 1843, 1844, 1848, 1849 },
  { 1831, 1841, 1853, 1843, 1844, 1845, 1846, 1847, 1848, 1850 },
  { 1853, 1842, 1843, 1845, 1846, 1847, 1848, 1849, 1850, 1851 },
  { 1821, 1822, 1823, 1824, 1825, 1826, 1827, 1828, 1829, 1830, 1831, 1832, 1833, 1834, 1835, 1836,
    1837, 1838, 1839, 1840, 1841, 1842, 1853, 1843, 1844, 1846, 1847, 1848, 1845, 1849, 1850, 1851 },
};
enum { T_TEN_FACE = 0x73c, T_TEN_DOT = 0x6b4, T_TEN_OR = 0xf36, T_TEN_COURSE = 0xcbe, T_TEN_LEVEL = 0xb1e,
       T_TEN_MSG_FACE = 0xb0d, T_TEN_MSG_NAME = 0xb18, T_TEN_MSG = 0x915 };
static const short TEN_MEDAL_AT[6] = { 400, 400, 400, 100, 200, 300 };
static const short TEN_COURSE_NAME[10] = { 3262, 3263, 3264, 3265, 3266, 3267, 3270, 3268, 3269, 3271 };
static const short TEN_LEVEL_NAME[11] = { 2846, 2847, 2848, 2849, 2850, 2851, 2852, 2853, 2854, 2855, 5599 };
static const int TEN_TIME_TEMPLATE[7] = { 6219, 6220, 6221, 6222, 6223, 6224, 6225 };
static const short TEN_TIME_BLANK[10] = { 0x184b, 0x184c, 0x184d, 0x184e, 0x184f, 0x1850, 0x1851, 0x1d4a, 0x1d4b, 0x1289 };
/* the lines (face, name, words) by result: 0x93d1e04 counts (a pick of 0..n), the tables
 * 0x93d1e20, 0x93d1e80, 0x93d1ec0, 0x93d1f20, 0x93d1fa0, 0x93d1fc4 */
static const short TEN_MSG_N[6] = { 5, 4, 6, 8, 2, 0 };
static const short TEN_MSG[6][9][3] = {
  { { 3230, 3256, 2835 }, { 3233, 3259, 2838 }, { 2832, 2843, 2328 }, { 3880, 3892, 3255 }, { 5590, 5595, 4740 },
    { 2834, 2845, 2330 } },
  { { 3871, 3884, 3246 }, { 2323, 2332, 1866 }, { 2831, 2842, 2327 }, { 2324, 2333, 1867 }, { 5593, 5598, 4743 } },
  { { 3231, 3257, 2836 }, { 3232, 3258, 2837 }, { 4734, 4744, 3882 }, { 2833, 2844, 2329 }, { 3878, 3890, 3253 },
    { 5591, 5596, 4741 }, { 5592, 5597, 4742 } },
  { { 2830, 2841, 2326 }, { 5589, 5594, 4739 }, { 3872, 3885, 3247 }, { 2322, 2331, 1865 }, { 3873, 3883, 3248 },
    { 3875, 3887, 3250 }, { 3876, 3888, 3251 }, { 3877, 3889, 3252 }, { 3879, 3891, 3254 } },
  { { 2829, 2840, 2325 }, { 1854, 1868, 1562 }, { 3234, 3260, 2839 } },
  { { 3874, 3886, 3249 } },
};

static struct {
  int phase, t, fade;
  int level, course, beaten, total, result, msg;
  float time;
  ten_anim r1, face[TEN_MAX], x[TEN_MAX], head, medal, r3;
  int step[TEN_MAX];
  int warm[MAX_TEN_WARM], nwarm;
} TEN;

static int ten_extend(void) { return TEN.level == 10; }
/* the faces drawn: 10, or in the extend mode those beaten and 4 more, 11 to 32 */
static int ten_faces(void) {
  if (!ten_extend()) return 10;
  int n = TEN.beaten + 4;
  return n < 11 ? 11 : n > TEN_MAX ? TEN_MAX : n;
}
static int ten_face_id(int i) {
  int n = ten_extend() ? TEN_MAX : 10;
  return i < n - 1 || TEN.beaten >= n - 1 ? TEN_FACE_ID[TEN.level][i] : T_TEN_FACE;
}
/* the extend mode's faces come in from the right in steps (0x93d0580: 6 frames, 60 px a
 * step) at the times of 0x93d0620 (face i's step k at 5 (i + k) up to 50, then 130 + 10 a
 * step), as far as 0x93d0d20 lets them for the count beaten */
static int ten_step_at(int i, int k) {
  int j = i + k;
  return k > 12 || j > 32 ? -1 : j <= 10 ? 5 * j : 130 + 10 * (j - 11);
}
static int ten_step_max(int i) {
  int m = 3 - i + (TEN.beaten > 7 ? TEN.beaten : 7);
  return m < 0 ? 0 : m > 13 ? 13 : m;
}

static void ten_phase_start(void) {
  TEN.t = 0;
  if (TEN.phase == 0) {
    TEN.r1.anim = TEN.beaten == TEN.total ? ID.win : ID.ten_timeup;
    ta_set(&TEN.r1, 4, 0, 0);
  } else if (TEN.phase == 1) {
    int n = ten_extend() ? TEN_MAX : 10;
    for (int i = 0; i < n; ++i) {
      TEN.face[i].anim = ID.ten_face;
      TEN.x[i].anim = ID.ten_x;
      /* 0x93d04e0: the normal levels' faces stand in a row, 60 px apart from the left */
      if (ten_extend()) ta_set(&TEN.face[i], 2, 0, 0);
      else ta_set(&TEN.face[i], 1, 220 - 20 * i, 1);
      ta_set(&TEN.x[i], 2, 0, 0);
      TEN.step[i] = 0;
    }
    TEN.head.anim = TEN.medal.anim = ID.ten_finish;
    ta_set(&TEN.head, 4, 0, 60);
    ta_set(&TEN.medal, 2, 0, 0);
  } else {
    TEN.r3.anim = ID.ten_summary;
    ta_set(&TEN.r3, 4, 0, 0);
  }
}

/* one frame of the phase's update; 0 when it is over */
static int ten_phase_update(void) {
  int t = TEN.t;
  if (TEN.phase == 0) {
    if (t > TEN_R1_LEN) return 0;
    ta_update(&TEN.r1);
  } else if (TEN.phase == 1) {
    if (t > TEN.beaten * 10 + 220) return 0;
    int ext = ten_extend(), n = ten_faces();
    if (ext)
      for (int i = 0; i < n; ++i) {
        int k = TEN.step[i], at = ten_step_at(i, k);
        if (k <= ten_step_max(i) && at >= 0 && at == t) {
          ta_set(&TEN.face[i], 4, 10 + 20 * k, 6);
          TEN.step[i]++;
        }
      }
    /* the beaten ones crossed out 10 frames apart (from 10, or 60 in the extend mode), the
     * medal 10 frames after the last */
    int first = ext ? 60 : 10;
    for (int i = 0; i < TEN.beaten && i < TEN_MAX; ++i)
      if (t == first + 10 * i) {
        ta_set(&TEN.x[i], 4, 0, 0);
        snd_menu(SND_RESULT2);
        break;
      }
    if (t == TEN.beaten * 10 + first) {
      ta_set(&TEN.medal, 4, TEN_MEDAL_AT[TEN.result], 20);
      snd_menu(SND_RESULT_PASS);
    }
    for (int i = 0; i < n; ++i) {
      ta_update(&TEN.face[i]);
      ta_update(&TEN.x[i]);
    }
    ta_update(&TEN.head);
    ta_update(&TEN.medal);
  } else {
    if (t > TEN_R3_LEN) return 0;
    if (t == 0 || t == 50 || t == 65 || t == 80 || t == 140 || (t == 95 && TEN.beaten == TEN.total)) snd_menu(SND_CUTIN2);
    ta_update(&TEN.r3);
  }
  TEN.t++;
  return 1;
}

static void draw_ten_result(void) {
  if (TEN.fade) anm_fill(0, 0, 0, TEN.fade < TEN_FADE ? (float)TEN.fade / TEN_FADE : 1.0f);
  if (TEN.fade && TEN.fade < TEN_FADE) return;
  anm_swap or = { anm_sprite(T_TEN_OR), anm_sprite(T_TEN_OR + ten_extend()) };
  if (TEN.phase == 0) {
    ta_draw(&TEN.r1, 0, 0, NULL, 0);
  } else if (TEN.phase == 1) {
    int dot = anm_sprite(T_TEN_DOT);
    for (int i = 0; i < ten_faces(); ++i) {
      const ten_anim *f = &TEN.face[i];
      if (f->mode == 2) continue;
      anm_swap sw[2] = { { anm_sprite(T_TEN_FACE), anm_sprite(ten_face_id(i)) }, { dot, -1 } };
      ta_draw(f, 0, 0, sw, 2);
      /* the X stands in for the face's dot (setImageReplace(0x6b4, X)) */
      float r[4];
      if (TEN.x[i].mode != 2 && anm_sprite_rect(f->anim, f->f, dot, r)) ta_draw(&TEN.x[i], r[0], r[1], NULL, 0);
    }
    ta_draw(&TEN.head, 0, 0, &or, 1);
    anm_swap none = { anm_sprite(T_TEN_OR), -1 }; /* setImageReplace(0xf36, 1): no sprite */
    ta_draw(&TEN.medal, 0, 0, &none, 1);
  } else if (TEN.phase == 2) {
    anm_swap sw[16];
    int n = 0;
    sw[n++] = or;
    sw[n++] = (anm_swap){ anm_sprite(T_TEN_COURSE), anm_sprite(TEN_COURSE_NAME[TEN.course]) };
    sw[n++] = (anm_swap){ anm_sprite(T_TEN_LEVEL), anm_sprite(TEN_LEVEL_NAME[TEN.level]) };
    const short *msg = TEN_MSG[TEN.result][TEN.msg];
    sw[n++] = (anm_swap){ anm_sprite(T_TEN_MSG_FACE), anm_sprite(msg[0]) };
    sw[n++] = (anm_swap){ anm_sprite(T_TEN_MSG_NAME), anm_sprite(msg[1]) };
    sw[n++] = (anm_swap){ anm_sprite(T_TEN_MSG), anm_sprite(msg[2]) };
    if (TEN.beaten != TEN.total) {
      for (int k = 0; k < 10; ++k) sw[n++] = (anm_swap){ anm_sprite(TEN_TIME_BLANK[k]), -1 };
    } else {
      set_time7(sw + n, TEN_TIME_TEMPLATE, RES_BIG_DIGIT, TEN.time);
      n += 7;
    }
    ta_draw(&TEN.r3, 0, 0, sw, n);
  }
}

static void ten_warm_add(int sprite) {
  if (sprite < 0 || is_template(anm_sprite_id(sprite))) return;
  for (int k = 0; k < TEN.nwarm; ++k)
    if (TEN.warm[k] == sprite) return;
  if (TEN.nwarm < (int)(sizeof TEN.warm / sizeof TEN.warm[0])) TEN.warm[TEN.nwarm++] = sprite;
}

void menu_finish_ten(menu_state *m, int beaten, float time, int medal) {
  memset(&TEN, 0, sizeof TEN);
  TEN.level = m->ten_level < 0 ? 0 : m->ten_level > 10 ? 10 : m->ten_level;
  TEN.course = menu_ten_course(m);
  if (TEN.course < 0) TEN.course = 0;
  if (TEN.course > 9) TEN.course = 9;
  TEN.total = ten_extend() ? TEN_MAX : 10;
  TEN.beaten = beaten < 0 ? 0 : beaten > TEN.total ? TEN.total : beaten;
  TEN.time = time;
  /* getTenResult (0x857f008): all beaten, the medal + 2 (3..5); else by how many were */
  if (TEN.beaten == TEN.total) TEN.result = (medal < 1 ? 1 : medal > 3 ? 3 : medal) + 2;
  else if (ten_extend()) TEN.result = TEN.beaten <= 25 ? 0 : TEN.beaten <= 30 ? 1 : 2;
  else TEN.result = TEN.beaten <= 6 ? 0 : TEN.beaten <= 8 ? 1 : 2;
  /* clTenResult3::reset: clMTRandom over 0..n */
  TEN.msg = rand() % (TEN_MSG_N[TEN.result] + 1);
  m->finish_story = 4;
  m->finish_goal = 1;
  m->finish_wait = TEN.beaten == TEN.total ? FINISH_GOAL_WAIT : FINISH_TIMEUP_WAIT;
  TEN.fade = TEN.beaten != TEN.total ? 1 : 0;
  TEN.phase = -1;
  /* the sprites to have up before they show (menu_finish_warm) */
  int s[MAX_TEN_WARM];
  const int anims[6] = { TEN.beaten == TEN.total ? ID.win : ID.ten_timeup, ID.ten_face, ID.ten_x, ID.ten_finish,
                         ID.ten_summary, -1 };
  for (int a = 0; anims[a] >= 0; ++a) {
    int ns = anm_sprites(anims[a], s, MAX_TEN_WARM);
    for (int i = 0; i < ns; ++i) ten_warm_add(s[i]);
  }
  for (int i = 0; i < ten_faces(); ++i) ten_warm_add(anm_sprite(ten_face_id(i)));
  ten_warm_add(anm_sprite(T_TEN_OR + ten_extend()));
  ten_warm_add(anm_sprite(TEN_COURSE_NAME[TEN.course]));
  ten_warm_add(anm_sprite(TEN_LEVEL_NAME[TEN.level]));
  for (int k = 0; k < 3; ++k) ten_warm_add(anm_sprite(TEN_MSG[TEN.result][TEN.msg][k]));
  for (int d = 0; d < 10; ++d) ten_warm_add(anm_sprite(RES_BIG_DIGIT + d));
}

/* one frame of the 10-in-a-row's end from its counter t (after the wait); 0 when it is over */
static int ten_result_step(int t) {
  if (TEN.fade && TEN.fade < TEN_FADE) {
    TEN.fade++;
    return 1;
  }
  if (TEN.phase < 0) {
    TEN.phase = 0;
    ten_phase_start();
  }
  (void)t;
  if (!ten_phase_update()) {
    if (++TEN.phase > 2) return 0;
    ten_phase_start();
  }
  return 1;
}

/* The story result (clRaceResultConsole_story, point_disp 0x85de618): from its counter's t0
 * (30 after WIN, 0 after LOSE; point_calc), at t0+121 the advantage row (menu_g_finish_tg_result's
 * first frame: |advantage| m, a minus when not ahead), at t0+180 the gauge's frame
 * (menu_g_finish_point_waku), its title (menu_g_finish_st_moji 0) and the gauge
 * (menu_g_finish_st_bar: frame = points / 5) at the points before the race; at t0+220 it
 * counts up 4 frames a tick (SE 0x43 looped till there); full, it goes on 1 a frame through
 * the flash to 299, SE 0x22 at 200 and 10 frames on the title gives way to the next tuning's
 * message (frames 80..158; T <= 19 basic, then the car's tuner: get_tuner_data). The console
 * lasts 7 s (its timer from point_calc) with no skip. */
enum { ST_RES_LEN = 420, ST_RES_ADV = 121, ST_RES_GAUGE = 180, ST_RES_COUNT = 220, ST_BAR_STEP = 4,
       ST_BAR_FULL = 200, ST_BAR_END = 299, ST_LOSE_LEN = 180 };
enum { T_ADV_MINUS = 0xafd, T_ADV_D0 = 0x1843, T_ADV_D1 = 0x1844, T_ADV_COMMA = 0xafc, T_ADV_D2 = 0x1845,
       T_ADV_D3 = 0x1846, T_ADV_D4 = 0x1847, T_ST_GET = 0x1d47, T_ST_GET2 = 0x1f3c,
       ADV_DIGIT = 1193 /* menu_p_finish_0..9 (0x93d0260) */ };
static int st_t0(const menu_state *m) { return m->finish_story == 1 ? 30 : 0; }
static int st_full(const menu_state *m) { return m->st_prev + m->st_gain >= 1000; }
/* the counter the gauge fills at (point_disp's +0x520: then SE 0x22, 10 on the message) */
static int st_fill_at(const menu_state *m) { return ST_RES_COUNT + (int)(m->st_gain * 0.2f / ST_BAR_STEP) + 10; }

/* the gauge's frame at counter t from t0 (+0x514 stepped toward +0x518) */
static int st_bar_frame(const menu_state *m, int t) {
  int pos = m->st_prev / 5;
  if (t <= ST_RES_COUNT || !m->st_gain) return pos;
  int target = st_full(m) ? ST_BAR_END : pos + (int)(m->st_gain * 0.2f);
  for (int k = ST_RES_COUNT + 1; k < t; ++k) {
    if (pos >= ST_BAR_FULL) pos++;
    else if (pos != target) pos += ST_BAR_STEP;
    if (pos > target) pos = target;
  }
  return pos;
}

static void st_result_step(const menu_state *m, int t) {
  if (!m->st_show) return;
  t -= st_t0(m);
  if (t == ST_RES_COUNT && m->st_gain) snd_menu_loop(SND_TUNING_BAR, 1);
  if (t <= ST_RES_COUNT || !m->st_gain) return;
  if (st_full(m)) {
    if (t == st_fill_at(m) - 10) {
      snd_menu_loop(SND_TUNING_BAR, 0);
      snd_menu(SND_CUTIN2);
    }
  } else if (st_bar_frame(m, t) >= st_bar_frame(m, ST_RES_LEN) && st_bar_frame(m, t - 1) < st_bar_frame(m, ST_RES_LEN)) {
    snd_menu_loop(SND_TUNING_BAR, 0);
  }
}

static void draw_st_result(const menu_state *m, int t) {
  t -= st_t0(m);
  if (t >= ST_RES_ADV) {
    /* point_calc's digits: ten thousands .. ones with a comma from 1000, leading zeros blank
     * and the minus just before the first digit, blank too when ahead */
    static const int SLOT[7] = { T_ADV_MINUS, T_ADV_D0, T_ADV_D1, T_ADV_COMMA, T_ADV_D2, T_ADV_D3, T_ADV_D4 };
    int v = (int)fabsf(m->st_gap);
    if (v > 99999) v = 99999;
    int d[7] = { 10, v / 10000 % 10, v / 1000 % 10, v > 999 ? 12 : 11, v / 100 % 10, v / 10 % 10, v % 10 };
    if (!d[1]) { d[1] = 10; d[0] = 11;
      if (!d[2]) { d[3] = 10; d[2] = 11; d[1] = 11;
        if (!d[4]) { d[4] = 10; d[3] = 11;
          if (!d[5]) { d[5] = 10; d[4] = 11; } } } }
    if (m->st_gap > 0)
      for (int k = 0; k < 7; ++k)
        if (d[k] == 10) d[k] = 11;
    anm_swap sw[7];
    for (int k = 0; k < 7; ++k) {
      sw[k].from = anm_sprite(SLOT[k]);
      sw[k].to = d[k] == 11 ? -1 : anm_sprite(d[k] < 10 ? ADV_DIGIT + d[k] : d[k] == 10 ? T_ADV_MINUS : T_ADV_COMMA);
    }
    anm_draw_ex(ID.st_adv, 0, 1, 0, 0, sw, 7);
  }
  if (!m->st_show || t < ST_RES_GAUGE) return;
  anm_draw(ID.st_waku, 0, 1);
  int fill = st_full(m) ? st_fill_at(m) : 1 << 30;
  if (t >= fill) {
    int f = 80 + (t - fill < 78 ? t - fill : 78);
    anm_swap sw[2] = { { anm_sprite(T_ST_GET), anm_sprite(m->st_get) }, { anm_sprite(T_ST_GET2), anm_sprite(m->st_get) } };
    anm_draw_ex(ID.st_moji, f, 1, 0, 0, sw, 2);
  } else {
    anm_draw(ID.st_moji, 0, 1);
  }
  anm_draw(ID.st_bar, st_bar_frame(m, t), 1);
}

/* the counter of the first best row with a new record (CONGRATULATIONS starts with it), or -1 */
static int congrats_at(const menu_result *r) {
  return r->new_course ? RES_COURSE_BEST : r->new_car ? RES_CAR_BEST : r->new_personal ? RES_PERSONAL : -1;
}

static int finish_leave(menu_state *m) {
  m->finish = m->finish_out = 0;
  back_from_race(m, 0);
  return MENU_LEAVE;
}

int menu_finish_update(menu_state *m, float dt) {
  m->acc += dt;
  if (m->acc > 0.25f) m->acc = 0.25f;
  while (m->finish && m->acc >= 1.0f / 60.0f) {
    m->acc -= 1.0f / 60.0f;
    int tap = m->tap;
    m->tap = 0;
    int t = m->finish++ - m->finish_wait; /* clTaResult's or clTaTimeUp's counter */
    if (t < 0) continue;
    if (!m->finish_goal) {
      if (t >= TIMEUP_FADE + TIMEUP_LEN + TIMEUP_BLACK) return finish_leave(m);
      continue;
    }
    if (m->finish_out) {
      if (++m->finish_out > RES_OUT_FADE + RES_OUT_BLACK) return finish_leave(m);
      continue;
    }
    if (m->finish_story == 4) {
      if (!ten_result_step(t)) m->finish_out = 1;
      continue;
    }
    if (m->finish_story == 3) {
      /* Suspend: clRaceThread::race fades out over 1/3 s (getFade; none: at once) and returns,
       * no result and the race's BGM on; clRaceThread::run sets up the select's next period */
      if (!m->finish_fade || t + 1 >= STORY_FADE) return finish_leave(m);
      continue;
    }
    if (m->finish_story >= 5) { /* a versus: WIN or LOSE, then out (a tap from 2 s) */
      if (t == 0) snd_menu(m->finish_story == 5 ? SND_FINISH : SND_FINISH2);
      if (t >= 300 || (tap && t >= 120)) m->finish_out = 1;
      continue;
    }
    if (m->finish_story) {
      /* WIN plays its 150 frames, LOSE its first 180, under the advantage and the points */
      if (t == 0) snd_menu(m->finish_story == 1 ? SND_FINISH : SND_FINISH2);
      st_result_step(m, t);
      if (t >= ST_RES_LEN) {
        snd_menu_loop(SND_TUNING_BAR, 0);
        m->finish_out = 1;
      }
      continue;
    }
    if (t == 0) snd_menu(SND_FINISH);
    if (t == RES_FINISH2) snd_menu(SND_FINISH2);
    if (t == RES_COURSE || t == RES_COURSE_BEST || t == RES_CAR_BEST || t == RES_PERSONAL || t == RES_SECTIONS ||
        t == RES_TOTAL || t == RES_SPEED)
      snd_menu(SND_CUTIN2);
    if (t > RES_TIMEOUT || (tap && t > RES_SPEED)) m->finish_out = 1;
  }
  return MENU_PAUSED;
}

static void draw_result(const menu_state *m, int t) {
  const menu_result *r = &m->result;
  if (t < RES_FINISH_LEN) anm_draw(ID.finish[1], t, 1);
  if (t >= RES_COURSE) anm_draw_ex(ID.res_course, 0, 1, 0, 0, RES.course, 2);
  static const int AT[3] = { RES_COURSE_BEST, RES_CAR_BEST, RES_PERSONAL };
  const int news[3] = { r->new_course, r->new_car, r->new_personal };
  for (int k = 0; k < 3; ++k) {
    if (t < AT[k]) continue;
    /* a new record's row loops (its NEW mark pulses), the others play once and hold */
    int f = t - AT[k];
    anm_draw_ex(ID.res_best[k], news[k] ? f % RES_ROW_LEN : f < RES_ROW_LEN ? f : RES_ROW_LEN - 1, 1, 0, 0, RES.best[k], 8);
  }
  int c = congrats_at(r);
  if (c >= 0 && t >= c) {
    c = t - c;
    anm_draw(ID.res_congrats, c < RES_CONGRATS_LEN ? c : RES_CONGRATS_LOOP + (c - RES_CONGRATS_LEN) % (RES_CONGRATS_LEN - RES_CONGRATS_LOOP), 1);
  }
  if (t >= RES_SECTIONS) anm_draw_ex(ID.res_sections, 0, 1, 0, 0, RES.sections, 4 * 7);
  /* clTaResult::run: section_2 too on course ids 12 and 13 (Tokyo, Kanagawa) */
  int id = car_course_id(m->course);
  if (t >= RES_SECTIONS && (id == 12 || id == 13) && ID.res_sections2 >= 0)
    anm_draw_ex(ID.res_sections2, 0, 1, 0, 0, RES.sections2, 3 * 7);
  if (t >= RES_TOTAL) {
    int f = t - RES_TOTAL;
    anm_draw_ex(ID.res_total, f < RES_ROW_LEN ? f : RES_TOTAL_LOOP + (f - RES_ROW_LEN) % (RES_ROW_LEN - RES_TOTAL_LOOP), 1, 0, 0,
                RES.total, 7);
  }
  if (t >= RES_SPEED) {
    anm_draw_ex(ID.res_speed, 0, 1, 0, 0, RES.speed, 3);
    /* the ranking only for a place in the top 100 (the password only with a card) */
    if (r->rank >= 1 && r->rank <= 100) anm_draw_ex(ID.res_rank, 0, 1, 0, 0, RES.rank, 3);
  }
}

void menu_finish_draw(const menu_state *m) {
  if (!m->finish || m->finish <= m->finish_wait) return;
  int t = m->finish - 1 - m->finish_wait;
  if (!m->finish_goal) {
    anm_fill(0, 0, 0, t < TIMEUP_FADE ? (float)(t + 1) / TIMEUP_FADE : 1.0f);
    if (t >= TIMEUP_FADE && t < TIMEUP_FADE + TIMEUP_LEN) anm_draw(ID.finish[0], t - TIMEUP_FADE, 1);
    return;
  }
  if (m->finish_story == 3) {
    float g = m->finish_fade == 2 ? 1.0f : 0.0f;
    if (m->finish_fade) anm_fill(g, g, g, t + 1 >= STORY_FADE ? 1.0f : (float)(t + 1) / STORY_FADE);
  } else if (m->finish_out) anm_fill(0, 0, 0, m->finish_out < RES_OUT_FADE ? (float)m->finish_out / RES_OUT_FADE : 1.0f);
  else if (m->finish_story >= 5) {
    int a = m->finish_story == 5 ? ID.win : ID.lose, len = m->finish_story == 5 ? anm_frames(a) : ST_LOSE_LEN;
    anm_draw(a, t < len ? t : len - 1, 1);
  } else if (m->finish_story == 1 || m->finish_story == 2) {
    int a = m->finish_story == 1 ? ID.win : ID.lose, len = m->finish_story == 1 ? anm_frames(a) : ST_LOSE_LEN;
    anm_draw(a, t < len ? t : len - 1, 1);
    draw_st_result(m, t);
  } else if (m->finish_story == 4) draw_ten_result();
  else draw_result(m, t);
}

/* The end's sprites, one a frame from when the race stops running (the first time they show
 * they would take that frame some 50 ms), those up kept from anm_trim; in the order they
 * show, the templates left out (digits and names take their place). */
static int is_template(int id) {
  return (id >= 7264 && id <= 7271) || (id >= 7275 && id <= 7298) || id == T_RES_NEW || id == T_RES_COURSE || id == T_RES_TIME;
}

void menu_finish_warm(int goal) {
  if (goal == 3) {
    int fresh = 0;
    for (int i = 0; i < TEN.nwarm; ++i) fresh += anm_warm_sprite(TEN.warm[i], !fresh);
    return;
  }
  enum { MAX_WARM = 256 };
  static int list[3][MAX_WARM], n[3] = { -1, -1, -1 };
  int g = goal == 2 ? 2 : goal != 0;
  if (n[g] < 0) {
    n[g] = 0;
    int extra[40], ne = 0;
    for (int d = 0; d < 10; ++d) extra[ne++] = RES_DIGIT + d;
    for (int d = 0; d < 10; ++d) extra[ne++] = RES_BIG_DIGIT + d;
    extra[ne++] = RES_NEW;
    for (int k = 0; k < N_COURSES; ++k) extra[ne++] = COURSE_NAME[car_course_id(k)];
    extra[ne++] = RES_TIME_NAME[0];
    extra[ne++] = RES_TIME_NAME[1];
    const int anims[10] = { ID.finish[1], ID.res_course, ID.res_best[0], ID.res_best[1], ID.res_best[2],
                            ID.res_congrats, ID.res_sections, ID.res_total, ID.res_speed, ID.res_rank };
    int sprites[MAX_WARM];
    const int story[6] = { ID.win, ID.lose, ID.st_adv, ID.st_waku, ID.st_bar, ID.st_moji };
    for (int a = 0; a < (g == 2 ? 6 : g ? 10 : 1); ++a) {
      int ns = anm_sprites(g == 2 ? story[a] : g ? anims[a] : ID.finish[0], sprites, MAX_WARM);
      /* the digits and the names right after FINISH */
      if (g == 1 && a == 1)
        for (int i = 0; i < ne && ns < MAX_WARM; ++i) sprites[ns++] = anm_sprite(extra[i]);
      /* the story result's advantage digits and tuning messages with its row */
      if (g == 2 && a == 2) {
        for (int d = 0; d < 10 && ns < MAX_WARM; ++d) sprites[ns++] = anm_sprite(ADV_DIGIT + d);
        for (int k = 0; k < 18 && ns < MAX_WARM; ++k) sprites[ns++] = anm_sprite(STTUNE_GET[k]);
      }
      for (int i = 0; i < ns; ++i) {
        int s = sprites[i], seen = s < 0 || is_template(anm_sprite_id(s));
        for (int k = 0; k < n[g] && !seen; ++k) seen = list[g][k] == s;
        if (!seen && n[g] < MAX_WARM) list[g][n[g]++] = s;
      }
    }
  }
  int fresh = 0;
  for (int i = 0; i < n[g]; ++i) fresh += anm_warm_sprite(list[g][i], !fresh);
}

void menu_pause_draw(const menu_state *m) {
  const menu_pause *p = &m->pause;
  if (!p->open) return;
  float a = p->open < PAUSE_IN ? (float)p->open / PAUSE_IN : 1.0f;
  int t = p->deciding ? DECIDE_DAY + FADE - p->deciding : 0;
  /* after YES is decided the box fades away; after NO the screen goes white */
  if (p->deciding && !p->sel && t >= DECIDE_DAY) a = (float)p->deciding / FADE;
  anm_fill(0, 0, 0, 0.55f * a);
  int panel = anm_sprite(T_PANEL), title = anm_sprite(T_CONTINUE);
  static const float top_src[4] = { 81, 20, 431, 146 }, top_dst[4] = { 145, 128, 495, 259 };
  static const float bottom_src[4] = { 81, 150, 431, 240 }, bottom_dst[4] = { 145, 259, 495, 353 };
  static const float title_src[4] = { 1, 1, 147, 30 }, title_dst[4] = { 247, 140, 393, 169 };
  anm_draw_part(panel, top_src, top_dst, a);
  anm_draw_part(panel, bottom_src, bottom_dst, a);
  anm_draw_part(title, title_src, title_dst, a);
  int z = 100 * p->sel, f = z + p->card_t % 60;
  if (p->slide) f = z + 90 + SLIDE - p->slide;
  if (p->deciding) f = z + 60 + (t < DECIDE_DAY ? t : DECIDE_DAY - 1);
  anm_swap cards[2] = { { anm_sprite(T_MORNING), anm_sprite(T_YES) }, { anm_sprite(T_NIGHT), anm_sprite(T_NO) } };
  anm_swap rings[2] = { { anm_sprite(T_RING), anm_sprite(0xb95) }, { anm_sprite(T_RING_BLUE), anm_sprite(0x1b0b) } };
  anm_draw_xf(ID.day_cards, f, a, PAUSE_DX, PAUSE_DY, PAUSE_K, cards, 2);
  if (!p->slide) anm_draw_xf(ID.day_ring, z + (p->card_t < 59 ? p->card_t : 59), a, PAUSE_DX, PAUSE_DY, PAUSE_K, rings, 2);
  if (p->deciding && p->sel && t >= DECIDE_DAY) anm_fill(1, 1, 1, (float)(t - DECIDE_DAY + 1) / FADE);
}

/* ---- the race's title page (clSeqTaCourseDecideThread), up while the course loads ----
 * menu_g_pre_bg with the course's background for the time zone (template 0xb2f),
 * menu_layout_pre_ta with the course's name (0x1b9a) and the time zone's (0x15ed),
 * menu_g_pre_sound with the race's song (font template 0xbaf, logo 0x1a96). */
#define PRE_LEN 150 /* frames it shows at least */
/* nsMenuCheck::getCourseImageTaBg (morning, night) and getCourseImageTaCourseName by course
 * id (0x93ddac0), getTimeSprite4CourseDecide by time zone (0x93dde34) */
static const int PRE_BG[2][15] = {
  { 2863, 2866, 2869, 2865, 2870, 2873, 2880, 2880, 2357, 2355, 2885, 2885, 3930, 6244, 2359 },
  { 3901, 3904, 3907, 3903, 3908, 3911, 3918, 3918, 3293, 3292, 3923, 3923, 5608, 6779, 3295 },
};
static const int PRE_NAME[15] = { 3931, 3932, 3933, 3934, 3936, 3936, 5610, 3937, 3944, 3939, 5611, 3941, 3942, 3943, 5612 };
static const int PRE_TIME[2] = { 4751, 3301 };
enum { T_PRE_BG = 0xb2f, T_PRE_NAME = 0x1b9a, T_PRE_TIME = 0x15ed, T_PRE_LOGO = 0x1a96, T_LOGO_MAXI3 = 0x1643,
       T_PRE_TITLE_TA = 2909, S_PRE_TITLE_VS = 2910 };

/* A story race's title (clSeqStEnemyInformationThread): menu_g_pre_bg with its ramp's
 * picture, menu_layout_pre_st with the episode's picture and title, its rivals' cars and
 * "<number> WA" in the digit templates (src/storypre.h); menu_g_pre_sound_st. */
enum { T_ST_PICTURE = 0x1e10, T_ST_TITLE = 0x1295, T_ST_CAR = 0x1e0d, T_ST_CAR_SPOT = 3947, T_ST_DIGIT = 0x1d1d,
       T_ST_WA = 0xdb0 };
#define STORY_NUMBER_DX 66.0f

static int draw_pre_story(const menu_state *m, float fade) {
  if (m->game != MENU_GAME_STORY || m->story_sel < 0 || m->story_sel >= STORYPRE_N || ID.pre_st < 0) return 0;
  const storypre_select *s = &STORYPRE[m->story_sel];
  anm_swap bg = { anm_sprite(T_PRE_BG), anm_sprite(s->bg[m->zone != 0]) };
  draw_photo_bg(&bg, fade);
  anm_swap sw[13];
  int n = 0;
  sw[n++] = (anm_swap){ anm_sprite(T_ST_PICTURE), anm_sprite(s->picture) };
  sw[n++] = (anm_swap){ anm_sprite(T_ST_TITLE), anm_sprite(s->title) };
  for (int k = 0; k < 3; ++k) sw[n++] = (anm_swap){ anm_sprite(T_ST_CAR + k), s->cars[k] > 1 ? anm_sprite(s->cars[k]) : -1 };
  sw[n++] = (anm_swap){ anm_sprite(T_ST_CAR_SPOT), -1 };
  for (int k = 0; k < 6; ++k) sw[n + k] = (anm_swap){ anm_sprite(k < 5 ? T_ST_DIGIT + k : T_ST_WA), -1 };
  anm_draw_ex(ID.pre_st, 0, fade, 0, 0, sw, n + 6);
  /* The number: this data's title sprite reads "Chapter" where the arcade's digit places
   * begin, so the digits go after it (STORY_NUMBER_DX) and WA is left out. Drawn on its own,
   * everything but the digit places hidden. */
  int nd = 0;
  anm_swap num[6 + 6];
  char text[8];
  snprintf(text, sizeof text, "%d", s->episode);
  int len = (int)strlen(text);
  for (int k = 0; k < 6; ++k)
    num[nd++] = (anm_swap){ anm_sprite(k < 5 ? T_ST_DIGIT + k : T_ST_WA), k < len && k < 5 ? anm_sprite(STORYPRE_DIGIT[text[k] - '0']) : -1 };
  for (int k = 0; k < n; ++k) num[nd++] = (anm_swap){ sw[k].from, -1 };
  anm_draw_ex(ID.pre_st, 0, fade, STORY_NUMBER_DX, 0, num, nd);
  return 1;
}

/* the 10-in-a-row's (clThread_7_3_LoadDisp::run 0x88a8630): menu_layout_pre_or with the
 * course's background, name, time zone and start ramp by level (src/tendb.h, 0x940d420) and
 * the level (0x940d3e0) in 0x186f */
static int draw_pre_ten(const menu_state *m, float fade) {
  if (m->game != MENU_GAME_TEN || ID.pre_or < 0) return 0;
  const int *p = TENDB_PRE[m->ten_course][m->ten_level];
  anm_swap bg = { anm_sprite(T_PRE_BG), anm_sprite(p[0]) };
  draw_photo_bg(&bg, fade);
  anm_swap sw[4] = { { anm_sprite(T_PRE_NAME), anm_sprite(p[1]) }, { anm_sprite(T_PRE_TIME), anm_sprite(p[2]) },
                     { anm_sprite(0x186f), anm_sprite(TENDB_PRE_LV[m->ten_level]) }, { anm_sprite(0x1870), anm_sprite(p[3]) } };
  anm_draw_ex(ID.pre_or, 0, fade, 0, 0, sw, 4);
  return 1;
}

/* (vs: the versus', the time attack's page with menu_p_pre_title_vs for its title) */
static void draw_pre_as(const menu_state *m, float fade, int vs) {
  int k = car_course_id(m->course), z = m->zone;
  int story = !vs && (draw_pre_story(m, fade) || draw_pre_ten(m, fade));
  if (!story) {
    anm_swap bg = { anm_sprite(T_PRE_BG), anm_sprite(PRE_BG[z][k]) };
    draw_photo_bg(&bg, fade);
    anm_swap ta[3] = { { anm_sprite(T_PRE_NAME), anm_sprite(PRE_NAME[k]) }, { anm_sprite(T_PRE_TIME), anm_sprite(PRE_TIME[z]) },
                       { anm_sprite(T_PRE_TITLE_TA), anm_sprite(S_PRE_TITLE_VS) } };
    anm_draw_ex(ID.pre_ta, 0, fade, 0, 0, ta, vs ? 3 : 2);
  }
  /* getBgmName / getBgmIcon of the song drawn (NO BGM has no logo) */
  int line = m->song > 0 ? m->song : BGM_LINES - 1;
  anm_swap snd[2] = { { anm_sprite(T_BGM_FONT), anm_sprite(BGM_TEXT + line) },
                      { anm_sprite(T_PRE_LOGO), m->song > 0 ? anm_sprite(T_LOGO_MAXI3) : -1 } };
  /* the name slides in once (30 frames) and stays */
  int sound = story && ID.pre_sound_st >= 0 ? ID.pre_sound_st : ID.pre_sound;
  int last = anm_frames(sound) - 1;
  anm_draw_ex(sound, m->t < last ? m->t : last, fade, 0, 0, snd, 2);
}

static void draw_pre(const menu_state *m, float fade) { draw_pre_as(m, fade, 0); }

/* ---- the story's level and rival select ----
 * seqStEnemyLevelSelect (0x8835c2c, its clSeqStEnemyLevelSelectThread) and seqStEnemyCharaSelect
 * (clThread_5_2_EnemySelect::run 0x88395ec) on the card's story progress (clV386CardData:
 * getStoryClearDivCount C, the loop C / 5, the division D = C % 5, the division's 20 cleared
 * bits). Episode n of a loop is 20 D + 5 level + slot; its story index get_story_index(loop, n).
 * - menu_layout_st_level: the four levels C, B, A, A+ as a spin box (as menu_layout_mode_sel_3:
 *   k*100 idle, +60 decided, +90..94 on to k+1). clMenuSelect puts the levels' names in
 *   0x1645.., the lock over a level not open yet in 0x1c98.. (levels 2..4), hides the class
 *   bubbles 0xb72.. after the first loop's first division and the layout's own arrows (for
 *   menu_layout_st_level_arrow's, k*100, 60 a loop), and colours the backlights 0x1b0c...
 * - menu_layout_st_level_clear (k*100 + 1, 59 a loop): CLEARED over the cleared levels, small
 *   (0x1b96..) and large (0x1b92..). menu_layout_st_level_ring: its flash once at rest.
 * - the level's five episodes at (100 + 110 i, 346), menu_g_st_lv_<colour>: open 1..59,
 *   cleared 101..159 (its frame the plain 0x760), locked 201..259, the picture in 0x1bb1; the
 *   line under them menu_g_st_lv_back 80..84.
 * - menu_layout_st_rival: the five episodes' places (2x2 markers 0x1bb1..0x1bb5), a spin box
 *   again. On each place a menu_g_st_rival_<colour> (its 2 centres an even number of title
 *   glyphs), the chosen one large (open 1, cleared 101, locked 201), the others small (+70):
 *   the picture in 0x96d (grey 0x164a), "第 <loop * 100 + n + 1> 話" in the glyph templates
 *   0x1e18..0x1e1e (the 2s: 0x1e1f first). menu_layout_st_rival_arrow: k*100 (60 a loop),
 *   4 + k when episode k + 1 is locked.
 * - menu_layout_st_rival_ramp: the chapter's name (font 4, for 0x49), START RAMP (0x580) and
 *   the episode's start ramp (0x59f), pre-rendered (src/storysel.h).
 * The arcade's 3D miniature of the course (clMiniatureCourse) is not drawn. */
enum {
  T_LV_NAME = 0x1645, T_LV_LOCK = 0x1c98, T_LV_FUKI = 0xb72, T_LV_ARROW = 0x1be, T_LV_ARROW2 = 0x383,
  T_LV_BACKLIGHT = 0x1b0c, T_LV_CLEAR_S = 0x1b96, T_LV_CLEAR_L = 0x1b92, S_CLEAR_S = 0x716, S_CLEAR_L = 0x4b5,
  S_LOCK = 0x75f, T_EP_PICTURE = 0x1bb1, S_EP_WAKU = 0x760, T_EP_STAGE = 0x96d, T_EP_GREY = 0x164a,
  T_EP_NAME = 0xfe9, T_EP_GLYPH = 0x1e18, T_EP_GLYPH7 = 0x1e1f, S_GLYPH_DIGIT = 4153, S_GLYPH_DAI = 5700,
  S_GLYPH_WA = 4902, T_RAMP_TITLE = 0x49, T_RAMP_LABEL = 0x580, T_RAMP_NAME = 0x59f, S_MES_LEVEL = 0x1704,
  S_MES_RIVAL = 0x1ab2,
};
static const int LV_BACKLIGHT[4] = { 0x1dbe, 0x1e69, 0x1ed8, 0x1cdd };
static const int LV_WAKU[4] = { 0x1327, 0x1649, 0x1881, 0x1044 }; /* menu_p_st_lv_waku_<colour> (0x93fd020) */
#define ST_NONE 99 /* a tap on nothing */

static int st_loop(const menu_state *m) { return m->st_div / 5; }
static int st_division(const menu_state *m) { return m->st_div % 5; }

/* clV386CardData::isStoryClear (0x86c5524): the divisions before the card's are cleared */
static int st_cleared(const menu_state *m, int n) {
  int r = n % 100, d = r / 20;
  if (d != st_division(m)) return d < st_division(m);
  return (m->st_bits >> (r % 20)) & 1;
}

/* get_story_index (0x8835590), also get_story_image_data's row: the loops after the first
 * run the tuner's story (100..110) in place of their first ten */
static int st_index(int loop, int n) {
  if (loop <= 0 || n > 9) return n;
  if (loop == 1 || (loop & 1)) return 100 + n;
  return n == 9 ? 110 : 100 + n;
}

/* seqStEnemyLevelSelect: each level's state and its episodes' (0 locked, 1 open, 2 cleared),
 * and where the cursor starts, on the first episode not cleared. In the first loop's later
 * divisions one episode opens after the other; otherwise a level's first four are open and
 * its fifth, the strong rival (IsStoryOpen 0x8835120), once those four are cleared. */
typedef struct { int level[4], ep[4][5], lv, slot; } st_states;
static void st_states_of(const menu_state *m, st_states *s) {
  int base = 20 * st_division(m), found = 0;
  memset(s, 0, sizeof *s);
  if (st_division(m) > 0 && st_loop(m) == 0) {
    for (int lv = 0; lv < 4 && !found; ++lv) {
      s->level[lv] = 1;
      for (int k = 0; k < 5 && !found; ++k) {
        s->ep[lv][k] = st_cleared(m, base + 5 * lv + k) ? 2 : 1;
        if (s->ep[lv][k] == 1) s->lv = lv, s->slot = k, found = 1;
      }
      if (!found) s->level[lv] = 2;
    }
    return;
  }
  for (int lv = 0; lv < 4; ++lv) {
    int cleared = 0;
    s->level[lv] = 1;
    for (int k = 0; k < 4; ++k) {
      s->ep[lv][k] = st_cleared(m, base + 5 * lv + k) ? 2 : 1;
      if (s->ep[lv][k] == 2) cleared++;
      else if (!found) s->lv = lv, s->slot = k, found = 1;
    }
    if (st_cleared(m, base + 5 * lv + 4)) {
      s->ep[lv][4] = s->level[lv] = 2;
    } else {
      s->ep[lv][4] = cleared == 4;
      if (!found) s->lv = lv, s->slot = 4, found = 1;
    }
  }
}

void menu_story_clear(menu_state *m, int n) {
  if (n < 0) return;
  int r = n % 100;
  if (r / 20 == st_division(m)) m->st_bits |= 1 << (r % 20);
  /* the division's last episode: setStoryClearDivCount(+1), which clears the bits */
  if (r % 20 == 19) {
    m->st_div = m->st_div + 1 > 0x1391 ? m->st_div - 9 : m->st_div + 1;
    m->st_bits = 0;
  }
  m->garage_changed = 1;
}

/* the level's names: the division's, a later loop's first division the tuner's story's first */
static int st_level_name(const menu_state *m, int k) {
  return st_loop(m) > 0 && st_division(m) == 0 && k < 2 ? STORYSEL_TUNER[k] : STORYSEL_LEVEL[st_division(m)][k];
}

/* the spin boxes' frame (as the mode select's) */
static int st_frame(const menu_state *m) {
  if (m->from >= 0) return m->from < m->sel ? m->from * 100 + 91 + m->t : m->sel * 100 + 94 - m->t;
  if (m->deciding) {
    int t = DECIDE_MODE + FADE - m->deciding;
    return m->sel * 100 + 60 + (t < DECIDE_MODE ? t : DECIDE_MODE - 1);
  }
  return m->sel * 100 + m->t % 60;
}

/* where the rival select's episode k sits in the layout's frame f */
static int st_place(int f, int k, float *x, float *y) {
  float r[4];
  if (!anm_sprite_rect(ID.st_rival, f, anm_sprite(T_EP_PICTURE + k), r)) return 0;
  *x = r[0] + 1.0f;
  *y = r[1] + 1.0f;
  return 1;
}

/* Which way a tap points: -1, +1, 0 the chosen item, ST_NONE */
static int st_tap(const menu_state *m) {
  int f = st_frame(m), best = -1;
  float bd = 1e9f;
  if (m->screen == MENU_ST_LEVEL) {
    /* the levels' names, or the episodes below (the chosen level) */
    if (m->tap_y > 300.0f && m->tap_y < 410.0f) return 0;
    for (int k = 0; k < 4; ++k) {
      float r[4];
      if (!anm_sprite_rect(ID.st_level, f, anm_sprite(T_LV_NAME + k), r)) continue;
      float cx = (r[0] + r[2]) * 0.5f, cy = (r[1] + r[3]) * 0.5f, h = (r[2] - r[0]) * 0.5f;
      float d = hypotf(m->tap_x - cx, m->tap_y - cy);
      if (d < h + 10.0f && d < bd) bd = d, best = k;
    }
  } else {
    for (int k = 0; k < 5; ++k) {
      float x, y;
      if (!st_place(f, k, &x, &y)) continue;
      float d = hypotf(m->tap_x - x, m->tap_y - y);
      if (d < (k == m->sel ? 100.0f : 55.0f) && d < bd) bd = d, best = k;
    }
  }
  if (best < 0) return ST_NONE;
  return best == m->sel ? 0 : best < m->sel ? -1 : 1;
}

static void st_step(menu_state *m, int tap, int key) {
  m->card_t++;
  if (m->from >= 0) {
    if (m->t >= MOVE_MODE) m->from = -1, m->t = 0;
    return;
  }
  if (!tap && !key) return;
  int d = key ? key : st_tap(m);
#ifdef WMMT_EGL
  if (getenv("WMMT_ST_TAP_GO")) d = 0; /* test: any tap decides (a menu.bin without the episode pictures) */
#endif
  if (d == ST_NONE) return;
  st_states s;
  st_states_of(m, &s);
  int rival = m->screen == MENU_ST_RIVAL;
  if (d == 0) {
    if (!(rival ? s.ep[m->st_level][m->sel] : s.level[m->sel])) {
      snd_menu(SND_CANCEL);
      return;
    }
    snd_menu(SND_SELECT);
    m->deciding = DECIDE_MODE + FADE;
    m->t = 0;
    return;
  }
  int to = m->sel + (d > 0 ? 1 : -1);
  if (to < 0 || to >= (rival ? 5 : 4)) return;
  if (rival && !s.ep[m->st_level][to]) return; /* the arrows stop before a locked episode */
  snd_menu(SND_CURSOR);
  m->from = m->sel;
  m->sel = to;
  m->t = 0;
}

/* on to the level select (from the mode select), its cursor on the level to play next */
static void st_level_enter(menu_state *m) {
  st_states s;
  st_states_of(m, &s);
  go(m, MENU_ST_LEVEL);
  m->sel = s.lv;
  m->card_t = 0;
}

static void st_rival_enter(menu_state *m) {
  st_states s;
  st_states_of(m, &s);
  go(m, MENU_ST_RIVAL);
  m->sel = 0;
  m->card_t = 0;
  if (s.lv == m->st_level) {
    m->sel = s.slot;
  } else {
    for (int k = 4; k >= 0; --k)
      if (s.ep[m->st_level][k] == 1) m->sel = k;
  }
}

/* ---- the story's points and tuning (clRaceResultConsole::point_calc, seqStTune) ---- */

/* a FULL TUNE car races the story on its full tune, a stock one on the story's steps (Allan 10-25) */
void menu_story_tune(const menu_state *m, int *power, int *handling) {
  if (m->tune_full[m->maker][m->car]) {
    menu_tune(m, power, handling);
    return;
  }
  *power = m->st_power[m->maker][m->car];
  *handling = m->st_handling[m->maker][m->car];
}

int menu_story_loop(const menu_state *m) { return st_loop(m); }

int menu_story_losses(const menu_state *m) { return m->st_lost_n == m->story_n ? m->st_lost : 0; }

void menu_story_lost(menu_state *m, int won) {
  if (!won && m->story_n >= 0) m->st_bonus_car[m->maker][m->car] |= 8, m->garage_changed = 1; /* hasStoryLose */
  if (won || m->st_lost_n != m->story_n) m->st_lost = 0;
  m->st_lost_n = m->story_n;
  if (!won && m->st_lost < 99) m->st_lost++;
}

/* ---- the story's poem (seqStPoem 0x883e1d8, clStPoem; "5-9. finish: one picture"): mseqStory
 * after a won story select (getRank 0), after the tuning screens (seqStTune, seqStTuneEff) and
 * before the ending (select 99) and the dress-up: the select's picture (the table at 0x93fd420
 * by getStorySelect, 0 none) played once (setAnime, layer 4) between clMenuFade's fade in and
 * out, no tap skipping it; then on as from the tuning work. ---- */
static const struct { short select; const char *anim; } POEMS[] = {
  { 9, "menu_p_poem_10" }, { 14, "menu_p_poem_15" }, { 19, "menu_p_poem_20" }, { 29, "menu_p_poem_30" },
  { 39, "menu_p_poem_90" }, { 49, "menu_p_poem_40" }, { 59, "menu_p_poem_60" }, { 69, "menu_p_poem_80" },
  { 79, "menu_p_poem_50" }, { 84, "menu_p_poem_isida85" }, { 89, "menu_p_poem_zero90" }, { 99, "menu_p_poem_fd100" },
  { 109, "menu_p_poem_tuner110" }, { 110, "menu_p_poem_tuner110" },
};
#define N_POEMS ((int)(sizeof POEMS / sizeof POEMS[0]))
#define POEM_FADE 60 /* clMenuFade's 1 s */

static int poem_anim(const menu_state *m) { return m->poem >= 0 && m->poem < N_POEMS ? anm_find(POEMS[m->poem].anim) : -1; }

static void poem_open(menu_state *m) {
  fprintf(stderr, "story poem: select %d, %s\n", m->story_pick, m->poem >= 0 ? POEMS[m->poem].anim : "-");
  go(m, MENU_POEM);
  m->poem_t = 0;
  m->bars = 0;
  m->music = 1;
  m->tap = m->key = m->key_held = m->back = m->leaving = 0;
}

static int tg_due(menu_state *m);

/* on as from the tuning work: the dress-up screens due, else the game mode select */
static void ending_open(menu_state *m);

static void poem_leave(menu_state *m) {
  m->poem = -1;
  if (m->ending == 1) {
    ending_open(m);
    return;
  }
  if (tg_due(m)) return;
  go(m, MENU_GAME);
  m->sel = game_slot(MENU_GAME_STORY);
  m->slide = m->card_t = 0;
  m->bars = 1;
  m->music = 1;
}

static void poem_step(menu_state *m) {
  int id = poem_anim(m), len = id >= 0 ? anm_frames(id) : 0;
  if (id >= 0 && m->poem_t == 0) anm_preload(id);
  if (++m->poem_t >= POEM_FADE + len + POEM_FADE || id < 0) poem_leave(m);
}

static void draw_poem(const menu_state *m) {
  int id = poem_anim(m), len = id >= 0 ? anm_frames(id) : 0, t = m->poem_t;
  anm_fill(0, 0, 0, 1);
  if (id < 0 || len <= 0) return;
  int f = t - POEM_FADE;
  anm_draw(id, f < 0 ? 0 : f >= len ? len - 1 : f, 1);
  float a = t < POEM_FADE ? (float)t / POEM_FADE : t >= POEM_FADE + len ? 1.0f - (float)(t - POEM_FADE - len) / POEM_FADE : 1.0f;
  anm_fill(0, 0, 0, 1.0f - (a < 0 ? 0 : a));
}

/* ---- the story's ending (seqRaceEnding 0x8906b68): mseqStory's after episode 100 (the
 * select in its loop 99) is won, after the poem: clSeqNowLoading, then clRaceThread in play
 * mode 3 (clRaceThread::initEnding 0x88f860c): clEffStoryType 111's period 0, a realtime
 * movie (MovieTableData 90, KgAirport_Out: the player's car and two 964s, series 145), the
 * BGM getBgm 54 (maxi3/sys_05) from frame 150; clEndingConsole (0x85ca700, update 0x85cba50)
 * plays its 56 events (0x93cf260: frame, cluster, anime, from, frames, screen) over it: black
 * masks (menu_layout_ed_base_*), the staff roll (_roll_1..7), fades and the BANDAI NAMCO logo
 * (_bng). A roll's screen 1 or 2 puts the movie in the bottom or top 640 x 192
 * (clSubScreen::setScreen_), 0 back on the whole screen. Its button (0x40; here a tap) jumps
 * to frame 9420 (the logo; clRaceThread::race fades the BGM out over 120 frames there), the
 * race ends at 9780. Then the menus as after the poem. The port shows the band as the movie's
 * own picture there (the rest black) where the arcade renders the movie into it. ---- */
enum { ED_FADE_BLACK, ED_FADE_WHITE, ED_BNG, ED_BASE1, ED_ROLL1 = ED_BASE1 + 4, ED_NONE = ED_ROLL1 + 7, ED_ANIMS };
static const char *const ED_NAME[ED_NONE] = {
  "menu_layout_ed_fade_black", "menu_layout_ed_fade_white", "menu_layout_ed_bng", "menu_layout_ed_base_1",
  "menu_layout_ed_base_2", "menu_layout_ed_base_3", "menu_layout_ed_base_4", "menu_layout_ed_roll_1",
  "menu_layout_ed_roll_2", "menu_layout_ed_roll_3", "menu_layout_ed_roll_4", "menu_layout_ed_roll_5",
  "menu_layout_ed_roll_6", "menu_layout_ed_roll_7",
};
#define FB ED_FADE_BLACK
#define FW ED_FADE_WHITE
#define B(k) (ED_BASE1 + (k) - 1)
#define R(k) (ED_ROLL1 + (k) - 1)
static const struct { short frame; unsigned char layer, anim; short from, len; unsigned char screen; } ED[] = {
  { 0, 3, FB, 1, 119, 0 }, { 318, 0, B(1), 1, 119, 0 }, { 438, 0, B(1), 121, 1, 0 }, { 438, 2, R(1), 1, 581, 0 },
  { 1021, 0, B(1), 121, 1, 0 }, { 1021, 3, FB, 241, 119, 0 }, { 1141, 0, B(2), 121, 1, 0 }, { 1141, 3, FB, 361, 119, 0 },
  { 1261, 0, B(2), 121, 1, 0 }, { 1261, 2, R(2), 1, 581, 0 }, { 1843, 2, ED_NONE, 0, 0, 0 }, { 1843, 0, B(2), 121, 1, 0 },
  { 1843, 3, FB, 241, 119, 0 }, { 1963, 0, B(3), 121, 1, 0 }, { 1963, 3, FB, 361, 119, 0 }, { 2083, 0, B(3), 121, 1, 0 },
  { 2083, 2, R(3), 1, 797, 0 }, { 2881, 0, B(3), 121, 1, 0 }, { 2881, 3, FB, 121, 119, 0 }, { 3001, 2, ED_NONE, 0, 0, 0 },
  { 3001, 0, B(4), 121, 1, 0 }, { 3001, 3, FB, 1, 119, 0 }, { 3121, 0, B(4), 121, 1, 0 }, { 3121, 2, R(4), 1, 1397, 1 },
  { 4519, 2, ED_NONE, 0, 0, 0 }, { 4519, 0, B(4), 121, 1, 0 }, { 4519, 3, FB, 241, 119, 0 }, { 4639, 0, B(3), 121, 1, 0 },
  { 4639, 3, FB, 361, 119, 0 }, { 4759, 0, B(3), 121, 1, 0 }, { 4759, 2, R(5), 1, 1403, 2 }, { 6163, 2, ED_NONE, 0, 0, 0 },
  { 6163, 0, B(3), 121, 1, 0 }, { 6163, 3, FB, 241, 119, 0 }, { 6283, 0, B(1), 121, 1, 0 }, { 6283, 3, FB, 361, 119, 0 },
  { 6403, 0, B(1), 121, 1, 0 }, { 6403, 2, R(6), 1, 1800, 0 }, { 8209, 0, B(1), 121, 1, 0 }, { 8209, 3, FB, 241, 119, 0 },
  { 8329, 0, B(4), 121, 1, 0 }, { 8329, 3, FB, 361, 119, 0 }, { 8449, 0, B(4), 121, 1, 0 }, { 8449, 2, R(7), 1, 270, 0 },
  { 8719, 3, FB, 121, 119, 0 }, { 8839, 2, ED_NONE, 0, 0, 0 }, { 8839, 3, FB, 120, 1, 0 }, { 8839, 2, R(7), 560, 190, 0 },
  { 9300, 0, B(4), 121, 1, 3 }, { 9300, 3, FW, 121, 119, 0 }, { 9420, 2, ED_NONE, 0, 0, 0 }, { 9420, 0, ED_BNG, 0, 0, 0 },
  { 9420, 3, FW, 1, 119, 0 }, { 9540, 0, ED_BNG, 0, 0, 0 }, { 9660, 0, ED_BNG, 0, 0, 0 }, { 9660, 3, FB, 121, 119, 0 },
};
#undef FB
#undef FW
#undef B
#undef R
#define N_ED ((int)(sizeof ED / sizeof ED[0]))

/* layer's last event by frame t (-1 none); a skip clears the layers (update's 0x40 branch) */
static int ed_event(int layer, int t, int skipped) {
  int e = -1;
  for (int k = 0; k < N_ED && ED[k].frame <= t; ++k)
    if (ED[k].layer == layer && (!skipped || ED[k].frame >= MENU_ENDING_SKIP)) e = k;
  return e;
}

int menu_ending_band(int t) {
  int e = ed_event(2, t, 0);
  return e >= 0 && (ED[e].screen == 1 || ED[e].screen == 2) ? ED[e].screen : 0;
}

void menu_ending_draw(const menu_state *m) {
  static int id[ED_NONE], found;
  if (!found) {
    for (int k = 0; k < ED_NONE; ++k) id[k] = anm_find(ED_NAME[k]);
    found = 1;
  }
  int t = m->ending_t;
  for (int layer = 0; layer < 4; ++layer) {
    int e = ed_event(layer, t, m->ending_skip);
    if (e < 0 || ED[e].anim >= ED_NONE || id[ED[e].anim] < 0) continue;
    int f = ED[e].from + (t - ED[e].frame), last = ED[e].len > 0 ? ED[e].from + ED[e].len - 1 : ED[e].from;
    int n = anm_frames(id[ED[e].anim]);
    if (f > last) f = last;
    if (f >= n) f = n - 1;
    if (f >= 0) anm_draw(id[ED[e].anim], f, 1);
  }
}

static void ending_open(menu_state *m) {
  fprintf(stderr, "story ending\n");
  go(m, MENU_PRE);
  m->pre_quiet = 1;
  m->pre_fade = 0;
  m->ending = 2;
  m->ending_t = m->ending_skip = 0;
  m->tap = m->key = m->key_held = m->back = m->leaving = 0;
}

static void bonus_next(menu_state *m);

void menu_ending_done(menu_state *m) {
  m->ending = m->ending_t = m->ending_skip = 0;
  m->finish = 0;
  if (m->bonus_due) {
    bonus_next(m);
    return;
  }
  if (tg_due(m)) return;
  go(m, MENU_GAME);
  m->sel = game_slot(MENU_GAME_STORY);
  m->slide = m->card_t = 0;
  m->bars = 1;
  m->music = 1;
}

/* ---- the bonuses after the ending (mseqStory 0x88ccd4f..): the story clear bonus
 * (seqStStoryModeClearBonus 0x8840ff0, clStStoryModeClearBonus) once a card (+0x279), then
 * with no story race lost this loop (before_card.hasStoryLose) the undefeated bonus
 * (seqStUndefeatedBonus 0x88403b8, +0x186) and the second (seqStUndefeatedBonus2 0x8841ba4,
 * +0x278), each once. Each (ctor 0x8840e68, 0x884026c, 0x8841a58; run 0x88407f0, 0x883fc30,
 * 0x8841450): menu_g_shogo_bg_1 and menu_g_bg_line behind, on them menu_layout_cd_cd's frames
 * 0..179 (the card; the clear bonus' with 0x712, 0x713 swapped for its design 0x905, 0x906) or
 * menu_layout_meter_meter2's (the second's meter), held at 179; clMenuFade in over 1/6 s, SE
 * 0x22 (cutin2), the decide button from frame 100 or frame 601 (the second: 901) leaves, the
 * fade out over 1/6 s. The card designs and the meter they give are not in the port (it keeps
 * the card's flags). ---- */
#define BONUS_FADE 10
static void bonus_next(menu_state *m) {
  int k = m->bonus_due & 1 ? 0 : m->bonus_due & 2 ? 1 : 2;
  m->bonus_due &= ~(1 << k);
  fprintf(stderr, "story bonus %d\n", k);
  go(m, MENU_BONUS);
  m->bonus_kind = k;
  m->bonus_t = 0;
  m->bars = 0;
  m->music = 1;
  m->tap = m->key = m->key_held = m->back = m->leaving = 0;
}

static void bonus_step(menu_state *m, int tap, int key) {
  int t = ++m->bonus_t, f = t - BONUS_FADE, end = m->bonus_kind == 2 ? 901 : 601;
  if (f == 0) snd_menu(SND_CUTIN2);
  if (m->leaving) {
    if (++m->leaving > BONUS_FADE) {
      m->leaving = 0;
      if (m->bonus_due) {
        bonus_next(m);
        return;
      }
      if (tg_due(m)) return;
      go(m, MENU_GAME);
      m->sel = game_slot(MENU_GAME_STORY);
      m->slide = m->card_t = 0;
      m->bars = 1;
      m->music = 1;
    }
    return;
  }
  if (f >= end || ((tap || key) && f > 100)) m->leaving = 1;
}

static void draw_bonus(const menu_state *m, int clock) {
  static int cd = -2, meter;
  if (cd == -2) cd = anm_find("menu_layout_cd_cd"), meter = anm_find("menu_layout_meter_meter2");
  anm_fill(0, 0, 0, 1);
  if (ID.shogo_bg1 >= 0) anm_draw(ID.shogo_bg1, clock % anm_frames(ID.shogo_bg1), 1);
  if (ID.line >= 0) anm_draw(ID.line, clock % anm_frames(ID.line), 1);
  int f = m->bonus_t - BONUS_FADE, id = m->bonus_kind == 2 ? meter : cd;
  if (f < 0) f = 0;
  if (f > 179) f = 179;
  if (id >= 0) {
    int n = anm_frames(id);
    if (f >= n) f = n - 1;
    anm_swap sw[2] = { { anm_sprite(0x712), anm_sprite(0x905) }, { anm_sprite(0x713), anm_sprite(0x906) } };
    anm_draw_ex(id, f, 1, 0, 0, m->bonus_kind == 0 ? sw : NULL, m->bonus_kind == 0 ? 2 : 0);
  }
  float a = m->bonus_t < BONUS_FADE ? (float)m->bonus_t / BONUS_FADE : 1.0f;
  if (m->leaving) a = 1.0f - (float)m->leaving / BONUS_FADE;
  anm_fill(0, 0, 0, 1.0f - (a < 0 ? 0 : a));
}

/* point_calc 0x85df20c, the story's branch: the share of a full gauge (1000 points, 20
 * units) the race earns, by the tune so far (T), the loop, the episode's place in it and
 * whether it was cleared before; a full gauge's surplus is lost */
int menu_story_points(menu_state *m, int won) {
  int mk = m->maker, c = m->car, n = m->story_n;
  if (n < 0) return 0;
  int p, h;
  menu_story_tune(m, &p, &h);
  /* (a full tune is past the gauge's 33 steps: no points, no tuning screen) */
  int t = p + h, sel = n % 100, cleared = st_cleared(m, n), u = 0;
  m->st_prev = m->st_gauge[mk][c] * 50;
  if (t > 33) {
    u = 0;
  } else if (st_loop(m) > 0) {
    u = cleared || !won ? 0 : 1;
  } else if (sel <= 19) {
    if (t > 19) u = 0;
    else if (cleared) u = won ? (t <= 9 ? 10 : 7) : 0;
    else u = won ? 20 : t <= 9 ? 10 : 7;
  } else if (sel <= 39) {
    u = t > 29 ? 0 : cleared ? (won ? 5 : 0) : won ? 10 : 5;
  } else if (sel <= 59) {
    u = t > 32 || cleared || !won ? 0 : 2;
  } else {
    u = cleared || !won ? 0 : 1;
  }
  /* the result shows the gauge but past 33 steps, and in a later loop only when it gains */
  m->st_show = t <= 33 && (st_loop(m) == 0 || u);
  m->st_gain = m->st_prev + u * 50 > 1000 ? 1000 - m->st_prev : u * 50;
  m->st_get = t <= 19 ? T_ST_GET : STTUNE_GET[t <= 29 ? STTUNE_CAR[mk][c].t20 : STTUNE_CAR[mk][c].t30];
  int g = m->st_gauge[mk][c] + u;
  if (g >= 20) {
    m->st_gauge[mk][c] = 0;
    m->st_tune_due = 1;
  } else {
    m->st_gauge[mk][c] = (signed char)g;
  }
  if (u) m->garage_changed = 1;
  /* mseqStory: getStorySelectInLoop 99, the last episode, won: seqRaceEnding after the poem */
  m->ending = won && n % 100 == 99;
  /* and the bonuses after it (by the card before the race: hasStoryLose, then cleared by
   * finiStory's episode 100 won) */
  m->bonus_due = 0;
  if (m->ending) {
    unsigned char *b = &m->st_bonus_car[mk][c];
    if (!(*b & 1)) m->bonus_due |= 1;
    if (!(*b & 8) && !(*b & 2)) m->bonus_due |= 2;
    if (!(*b & 8) && !(*b & 4)) m->bonus_due |= 4;
    *b = (unsigned char)((*b | m->bonus_due) & ~8);
    m->garage_changed = 1;
  }
  m->poem = -1;
  for (int k = 0; won && k < N_POEMS; ++k)
    if (POEMS[k].select == m->story_pick) m->poem = k;
  return u;
}

/* The tuning screen (clSeqStTuneThread): menu_layout_st_tune over menu_g_bg_image_st_tune,
 * idling on frames 1..59; the part (template 0x1ef9), the tuner's shop (0x1ea2) and
 * portrait (0x19d7), the step line (ＳＴＥＰn：the part, font 1 on 0x6a8), the tuner's name
 * and three lines on menu_layout_st_tune_serifu (frame 12: 0x1a9c, 0x580..0x582), the top
 * bar's message the tuner's. Power over handling, each a row of cells (menu_g_setting_gage_1:
 * x from 0x93fc818, 19 apart; y 333 and 371), the next step's cell lit big (90..94) on the
 * chosen one, and their P and H markers (menu_g_setting_p_h, clMenuGSettingPHAnime). The
 * left key or a tap on the Power row chooses power, the right key or Handling handling
 * (SE 0x19; not when the step is fixed): the other's step cell goes out (61..64), the
 * chosen one's comes on (71..74), the red and blue bars swap, and changeTune crossfades to
 * the new part and tuner (frames 71..75 for power, 81..85 for handling: templates 0x16af and
 * 0x133d). A tap anywhere else decides (SE 0x1a, the arcade's gas pedal; its 10 s
 * countdown is the port's infinity). Not ported: the R35's own layout and the tuner's
 * work on the car after (seqStTuneEff). */
enum { T_TUNE_PARTS = 0x1ef9, T_TUNE_SHOP = 0x1ea2, T_TUNE_PORTRAIT = 0x19d7, T_TUNE_PARTS2 = 0x16af,
       T_TUNE_PORTRAIT2 = 0x133d, T_TUNE_FONT = 0x6a8, T_TUNE_NAME = 0x1a9c, T_TUNE_LINE = 0x580,
       T_TUNE_RED = 0x132a, T_TUNE_BLUE = 0x16ad, T_PH_TENS_P = 0x964, T_PH_ONES_P = 0x963,
       T_PH_TENS_H = 0x95c, T_PH_ONES_H = 0x95b, S_PH_DIGIT_P = 2402, S_PH_DIGIT_H = 2392 };
#define TUNE_PARTS_ENGINE 0x16cb /* the second power part of a car whose model has +0x28 in {1, 2} */
#define TUNE_PARTS_SWAP 0x16d6   /* car 0x24's first */
#define TUNE_WARN_NAME 0x1bc2    /* Rikako's name and portrait (menu_p_st_tuner_rikako10) */
#define TUNE_WARN_PORTRAIT 0x1893
static const float TUNE_CELL_X[2][3] = { { 171, 381, 0 }, { 137, 339, 541 } };
static const float TUNE_ROW_Y[2] = { 333, 371 };
/* the cells' states (0x93fc080: start, frames, mode): lit loops, the others hold */
static const int TUNE_STATE[5][3] = { { 0, 0, 1 }, { 1, 59, 6 }, { 61, 4, 4 }, { 71, 4, 4 }, { 90, 5, 4 } };

typedef struct { int chara, parts, shop, name, line[3]; } tune_view;

/* the constructor's (and changeTune's) part, tuner and lines for the step at these values */
static tune_view tune_view_of(const menu_state *m, int power, int handling, int choice) {
  const sttune_car *car = &STTUNE_CAR[m->maker][m->car];
  int t = m->tune.total;
  tune_view v = { 0, 1, 1, -1, { -1, -1, -1 } };
  const sttune_row *r = NULL;
  if (t <= 19) {
    if (!choice) {
      r = car->vip ? &STTUNE_HAND_VIP[handling] : &STTUNE_HAND[handling];
    } else {
      r = car->vip ? &STTUNE_POWER_VIP[power] : car->na && power == 1 ? &STTUNE_POWER_NA
          : car->e24 && power == 1 ? &STTUNE_POWER_24 : &STTUNE_POWER[power];
    }
    v.chara = r->chara;
    v.parts = r->parts;
    v.name = r->name;
    for (int k = 0; k < 3; ++k) v.line[k] = r->line[k];
    if (choice && car->k28 && power == 2) v.parts = TUNE_PARTS_ENGINE;
    if (choice && car->e24 && power == 1) v.parts = TUNE_PARTS_SWAP;
  } else if (t <= 29) {
    int i = t - 20;
    v.chara = car->t20;
    v.name = STTUNE_MID_NAME[car->vip][i];
    v.parts = STTUNE_MID_PARTS[car->vip][i];
    v.shop = STTUNE_CHARA[v.chara][2];
    const short (*l)[3] = STTUNE_MID_LINES[v.chara < 17 && STTUNE_MID_HAS[v.chara] ? v.chara : 2];
    for (int k = 0; k < 3; ++k) v.line[k] = l[i][k];
  } else {
    v.chara = car->t30;
    v.shop = STTUNE_CHARA[v.chara][2];
    int i = t - 30 < 4 ? t - 30 : 3;
    for (int k = 0; k < 3; ++k) v.line[k] = STTUNE_TOP_LINES[v.chara][i][k];
  }
  return v;
}

/* Rikako's warning, one side three steps ahead of the other (steps 1..20 only) */
static int tune_warn(const menu_state *m, int choice) {
  if (m->tune.total > 19) return 0;
  return (m->tune.power - m->tune.handling > 2 && choice) || (m->tune.handling - m->tune.power > 2 && !choice);
}

static void tune_cell(menu_state *m, int row, int i, int state) {
  if (i < 0 || i >= 24) return;
  m->tune.state[row][i] = (unsigned char)state;
  m->tune.since[row][i] = m->t;
}

/* clMenuLayout's constructor */
static void tune_open(menu_state *m) {
  int p = m->st_power[m->maker][m->car], h = m->st_handling[m->maker][m->car], t = p + h;
  memset(&m->tune, 0, sizeof m->tune);
  go(m, MENU_ST_TUNE);
  m->tune.total = t;
  m->tune.cells = t > 29 ? t - 9 : 20;
  if (m->tune.cells > 24) m->tune.cells = 24;
  m->tune.power = p;
  m->tune.handling = h;
  if (t <= 19) {
    m->tune.fixed = !(p <= 9 && h <= 9);
    if (p > h || p > 9) m->tune.handling = h + 1, m->tune.choice = 0;
    else m->tune.power = p + 1, m->tune.choice = 1;
  } else {
    m->tune.power = p + 1;
    m->tune.choice = 1;
  }
  m->tune.choice0 = m->tune.choice;
  m->tune.power0 = m->tune.power;
  m->tune.handling0 = m->tune.handling;
  m->tune.changed = -1;
  for (int row = 0; row < 2; ++row) {
    int v = row ? m->tune.handling : m->tune.power;
    for (int i = 0; i < m->tune.cells; ++i) tune_cell(m, row, i, i < v);
  }
  int c = m->tune.choice;
  tune_cell(m, !c, (c ? m->tune.power : m->tune.handling) - 1, 4);
  m->card_t = 0;
  m->bars = 1;
  m->music = 1;
}

/* key 1 (power) or 2 (handling) */
static void tune_choose(menu_state *m, int power) {
  if (m->tune.fixed || m->tune.choice == power) return;
  snd_menu(SND_CURSOR);
  m->tune.choice = power;
  int *was = power ? &m->tune.handling : &m->tune.power, *now = power ? &m->tune.power : &m->tune.handling;
  tune_cell(m, power ? 1 : 0, *was - 1, 2);
  (*was)--;
  (*now)++;
  tune_cell(m, power ? 0 : 1, *now - 1, 3);
  m->tune.changed = m->t;
}

static void tune_step(menu_state *m, int tap, int key) {
  m->card_t++;
  if (key) {
    tune_choose(m, key < 0);
    return;
  }
  if (!tap) return;
  if (m->tap_y >= 312.0f && m->tap_y < 352.0f) {
    tune_choose(m, 1);
    return;
  }
  if (m->tap_y >= 352.0f && m->tap_y < 395.0f) {
    tune_choose(m, 0);
    return;
  }
  if (m->tap_y < 40.0f) return;
  snd_menu(SND_SELECT);
  m->deciding = FADE;
}

/* the screen decided: setTunePower / setTuneHandling, on to the mode select */
static void tune_done(menu_state *m) {
  int mk = m->maker, c = m->car;
  if (m->tune.choice) m->st_power[mk][c] = (signed char)(m->st_power[mk][c] + 1 > 24 ? 24 : m->st_power[mk][c] + 1);
  else m->st_handling[mk][c] = (signed char)(m->st_handling[mk][c] + 1 > 24 ? 24 : m->st_handling[mk][c] + 1);
  m->st_tune_due = 0;
  m->garage_changed = 1;
}

/* a cell's frame of menu_g_setting_gage_1 */
static int tune_cell_frame(const menu_state *m, int row, int i) {
  const int *s = TUNE_STATE[m->tune.state[row][i]];
  int t = m->t - m->tune.since[row][i], base = 200 * (i / 10);
  if (s[2] == 6) return base + s[0] + t % s[1];
  return base + s[0] + (t < s[1] ? t : s[1]);
}

/* a view's top bar message, name and portrait, Rikako's when she warns */
static void tune_face(const tune_view *v, int warn, int *name, int *portrait, int *top) {
  *name = warn ? TUNE_WARN_NAME : STTUNE_CHARA[v->chara][0];
  *portrait = warn ? TUNE_WARN_PORTRAIT : STTUNE_CHARA[v->chara][1];
  *top = warn ? 0 : STTUNE_CHARA[v->chara][3];
}

static int tune_text(int k) { return k < 0 ? -1 : anm_sprite(STTUNE_TEXT + k); }

static void draw_st_tune(const menu_state *m, int clock, float fade, int bars) {
  tune_view v0 = tune_view_of(m, m->tune.power0, m->tune.handling0, m->tune.choice0);
  tune_view v = tune_view_of(m, m->tune.power, m->tune.handling, m->tune.choice);
  int w0 = m->tune.total <= 19 && ((m->tune.power0 - m->tune.handling0 > 2 && m->tune.choice0) ||
                                   (m->tune.handling0 - m->tune.power0 > 2 && !m->tune.choice0));
  int w = tune_warn(m, m->tune.choice);
  int name0, portrait0, top0, name, portrait, top;
  tune_face(&v0, w0, &name0, &portrait0, &top0);
  tune_face(&v, w, &name, &portrait, &top);
  if (w && m->tune.changed >= 0) portrait0 = portrait;
  for (int k = 0; k < 3 && w; ++k) v.line[k] = k == 1 ? STTUNE_WARN[m->tune.power > m->tune.handling ? 0 : 1] : -1;
  draw_back(ID.bg, clock % 600, fade);
  draw_back(ID.bg_st_tune, clock % anm_frames(ID.bg_st_tune), fade);
  int f = 1 + m->card_t % 59;
  if (m->tune.changed >= 0) {
    int t = m->t - m->tune.changed;
    f = (m->tune.choice ? 71 : 81) + (t < 4 ? t : 4);
  }
  anm_swap sw[9] = {
    { anm_sprite(T_TUNE_PARTS), anm_sprite(v0.parts) }, { anm_sprite(T_TUNE_SHOP), anm_sprite(v0.shop) },
    { anm_sprite(T_TUNE_PORTRAIT), anm_sprite(portrait0) }, { anm_sprite(T_TUNE_PARTS2), anm_sprite(v.parts) },
    { anm_sprite(T_TUNE_PORTRAIT2), anm_sprite(portrait) }, { anm_sprite(T_TUNE_FONT), -1 },
    { anm_sprite(T_TUNE_RED), m->tune.choice ? anm_sprite(T_TUNE_RED) : -1 },
    { anm_sprite(T_TUNE_BLUE), m->tune.choice ? -1 : anm_sprite(T_TUNE_BLUE) },
  };
  anm_draw_ex(ID.st_tune, f, fade, 0, 0, sw, 8);
  /* the step line where the layout has its font */
  float r[4];
  if (anm_sprite_rect(ID.st_tune, f, anm_sprite(T_TUNE_FONT), r)) {
    int step = anm_sprite(STTUNE_TEXT + STTUNE_STEP[m->tune.total < 35 ? m->tune.total : 34]);
    anm_draw_sprite(step, r[0], r[1], fade);
    if (m->tune.total <= 29 && v.name >= 0)
      anm_draw_sprite(tune_text(STTUNE_COLON[v.name]), r[0] + anm_sprite_width(step), r[1], fade);
  }
  /* the gauges, then the P and H markers over them */
  int gtype = m->tune.total > 29;
  for (int row = 0; row < 2; ++row) {
    for (int i = 0; i < m->tune.cells; ++i) {
      float x = TUNE_CELL_X[gtype][i / 10] + 19.0f * (float)(i % 10), y = TUNE_ROW_Y[row];
      if (m->tune.state[row][i] == 0) {
        int locked = m->tune.total <= 19 && i > 9;
        anm_draw_ex(locked ? ID.gage_waku : ID.gage_waku2, 0, fade, x, y, NULL, 0);
      } else {
        anm_draw_ex(ID.gage, tune_cell_frame(m, row, i), fade, x, y, NULL, 0);
      }
    }
  }
  for (int row = 0; row < 2; ++row) {
    int val = row ? m->tune.handling : m->tune.power, idx = val - 1;
    if (idx < 0 || idx >= 24) continue;
    int frame = 3 * row + (idx <= 8 ? 0 : idx <= 19 ? 1 : 2);
    int digit = row ? S_PH_DIGIT_H : S_PH_DIGIT_P;
    anm_swap ph[2] = { { anm_sprite(row ? T_PH_TENS_H : T_PH_TENS_P), anm_sprite(digit + val / 10 % 10) },
                       { anm_sprite(row ? T_PH_ONES_H : T_PH_ONES_P), anm_sprite(digit + val % 10) } };
    float x = 15.0f + TUNE_CELL_X[gtype][idx / 10] + 19.0f * (float)(idx % 10);
    anm_draw_ex(ID.ph, frame, fade, x, 55.0f + (row ? 304.0f : 266.0f), ph, 2);
  }
  /* the tuner's lines */
  anm_swap ln[4] = { { anm_sprite(T_TUNE_NAME), anm_sprite(name) } };
  for (int k = 0; k < 3; ++k) ln[1 + k] = (anm_swap){ anm_sprite(T_TUNE_LINE + k), tune_text(v.line[k]) };
  anm_draw_ex(ID.st_tune_serifu, 12, fade, 0, 0, ln, 4);
  anm_swap mes = { anm_sprite(T_TOPBAR_MES), top > 0 ? anm_sprite(top) : -1 };
  draw_topbar(bars, fade, &mes);
  anm_draw(ID.bottom, bars, fade);
}

static void draw_st_level(const menu_state *m, int clock, float fade, int bars) {
  st_states s;
  st_states_of(m, &s);
  int f = st_frame(m), lv = m->sel, rest = m->from < 0 && !m->deciding;
  draw_back(ID.bg, clock % 600, fade);
  anm_draw(ID.line, 0, fade);
  anm_swap sw[24];
  int n = 0;
  for (int k = 0; k < 4; ++k) sw[n++] = (anm_swap){ anm_sprite(T_LV_NAME + k), anm_sprite(st_level_name(m, k)) };
  for (int k = 1; k < 4; ++k) sw[n++] = (anm_swap){ anm_sprite(T_LV_LOCK + k - 1), s.level[k] ? -1 : anm_sprite(S_LOCK) };
  if (st_division(m) > 0 || st_loop(m) > 0)
    for (int k = 0; k < 4; ++k) sw[n++] = (anm_swap){ anm_sprite(T_LV_FUKI + k), -1 };
  sw[n++] = (anm_swap){ anm_sprite(T_LV_ARROW), -1 };
  sw[n++] = (anm_swap){ anm_sprite(T_LV_ARROW2), -1 };
  for (int k = 0; k < 4; ++k) sw[n++] = (anm_swap){ anm_sprite(T_LV_BACKLIGHT + k), anm_sprite(LV_BACKLIGHT[k]) };
  anm_draw_ex(ID.st_level, f, fade, 0, 0, sw, n);
  anm_swap cl[8];
  for (int k = 0; k < 4; ++k) {
    cl[2 * k] = (anm_swap){ anm_sprite(T_LV_CLEAR_S + k), s.level[k] == 2 ? anm_sprite(S_CLEAR_S) : -1 };
    cl[2 * k + 1] = (anm_swap){ anm_sprite(T_LV_CLEAR_L + k), s.level[k] == 2 ? anm_sprite(S_CLEAR_L) : -1 };
  }
  if (rest) {
    anm_draw_ex(ID.st_level_clear, lv * 100 + 1 + m->t % 59, fade, 0, 0, cl, 8);
    if (m->t < 60) anm_draw(ID.st_level_ring, lv * 100 + m->t, fade);
    anm_draw(ID.st_level_arrow, lv * 100 + m->t % 60, fade);
  }
  /* the level's five episodes */
  anm_draw(ID.st_lv_back, 80 + (m->t < 4 ? m->t : 4), fade);
  int base = 20 * st_division(m) + 5 * lv;
  for (int i = 0; i < 5; ++i) {
    int st = s.ep[lv][i], row = st_index(st_loop(m), base + i);
    anm_swap ep[2] = { { anm_sprite(T_EP_PICTURE), anm_sprite(STORYSEL_IMAGE[row][0]) },
                       { anm_sprite(LV_WAKU[lv]), st == 2 ? anm_sprite(S_EP_WAKU) : anm_sprite(LV_WAKU[lv]) } };
    int frame = (st == 0 ? 200 : st == 1 ? 0 : 100) + 1 + clock % 59;
    anm_draw_ex(ID.st_lv[lv], frame, fade, 100.0f + 110.0f * (float)i, 346.0f, ep, 2);
  }
  anm_swap mes = { anm_sprite(T_TOPBAR_MES), anm_sprite(S_MES_LEVEL) };
  draw_topbar(bars, fade, &mes);
  anm_draw(ID.bottom, bars, fade);
}

static void draw_st_rival(const menu_state *m, int clock, float fade, int bars) {
  st_states s;
  st_states_of(m, &s);
  int f = st_frame(m), lv = m->st_level, loop = st_loop(m), rest = m->from < 0 && !m->deciding;
  int base = 20 * st_division(m) + 5 * lv;
  draw_back(ID.bg, clock % 600, fade);
  anm_draw(ID.line, 0, fade);
  anm_swap marks[5];
  for (int k = 0; k < 5; ++k) marks[k] = (anm_swap){ anm_sprite(T_EP_PICTURE + k), -1 };
  anm_draw_ex(ID.st_rival, f, fade, 0, 0, marks, 5);
  /* the chosen one last, over its neighbours */
  int order[5], no = 0;
  for (int k = 0; k < 5; ++k)
    if (k != m->sel) order[no++] = k;
  order[no++] = m->sel;
  for (int j = 0; j < 5; ++j) {
    int i = order[j];
    float x, y;
    if (!st_place(f, i, &x, &y)) continue;
    int st = s.ep[lv][i], n = base + i, row = st_index(loop, n);
    char num[12];
    snprintf(num, sizeof num, "%d", loop * 100 + n + 1);
    int len = (int)strlen(num), glyphs = len + 2, even = !(glyphs & 1);
    /* the glyph places left to right: 0..6, the 2s 7 first */
    int place[8], places = 0;
    if (even) place[places++] = T_EP_GLYPH7;
    for (int k = 0; k < 7; ++k) place[places++] = T_EP_GLYPH + k;
    int first = (places - glyphs) / 2;
    anm_swap sw[12];
    int nsw = 0;
    sw[nsw++] = (anm_swap){ anm_sprite(T_EP_STAGE), anm_sprite(STORYSEL_IMAGE[row][0]) };
    sw[nsw++] = (anm_swap){ anm_sprite(T_EP_GREY), anm_sprite(STORYSEL_IMAGE[row][1]) };
    sw[nsw++] = (anm_swap){ anm_sprite(T_EP_NAME), -1 };
    for (int k = 0; k < places; ++k) {
      int g = k - first, to = -1;
      if (g == 0) to = S_GLYPH_DAI;
      else if (g == glyphs - 1) to = S_GLYPH_WA;
      else if (g > 0 && g < glyphs - 1) to = S_GLYPH_DIGIT + num[g - 1] - '0';
      sw[nsw++] = (anm_swap){ anm_sprite(place[k]), to >= 0 ? anm_sprite(to) : -1 };
    }
    int frame = (st == 0 ? 200 : st == 1 ? 0 : 100) + (i == m->sel && m->from < 0 ? 1 : 71);
    anm_draw_ex(ID.st_ep[lv][even], frame, fade, x, y, sw, nsw);
  }
  if (rest) {
    int block = m->sel < 4 && !s.ep[lv][m->sel + 1] ? (m->sel ? 4 + m->sel : -1) : m->sel;
    if (block >= 0) anm_draw(ID.st_rival_arrow, block * 100 + m->t % 60, fade);
  }
  /* the chapter and the chosen episode's start ramp */
  int n = base + m->sel, later = loop > 0 && n <= 9;
  int title = later ? 20 + n / 5 : n / 5, ramp = STORYSEL_RAMP[later ? 100 + n : n];
  anm_swap rp[3] = { { anm_sprite(T_RAMP_TITLE), anm_sprite(STORYSEL_TEXT + title) },
                     { anm_sprite(T_RAMP_LABEL), anm_sprite(STORYSEL_START_RAMP) },
                     { anm_sprite(T_RAMP_NAME), anm_sprite(STORYSEL_RAMP_TEXT + ramp) } };
  anm_draw_ex(ID.st_rival_ramp, 0, fade, 0, 0, rp, 3);
  anm_swap mes = { anm_sprite(T_TOPBAR_MES), anm_sprite(S_MES_RIVAL) };
  draw_topbar(bars, fade, &mes);
  anm_draw(ID.bottom, bars, fade);
}

/* clSeqTaModeSelectThread's spin box puts a car (clSpriteAnimeCarModel, setImageReplace) where
 * its layout has menu_f_font_dot01 and dot02: the card's car under Personal Best, the official
 * machine (setupTaOfficialCar) under Official Machine. The port has none there yet, and the
 * markers themselves are not to be seen. */
static const int MODE_CAR_TEMPLATE[2] = { 1716, 1717 };
static anm_swap mode_cars[2];

/* ---- the 10-in-a-row's course and level select (mseqTen) ----
 * seqTenCourseSelect (clThread_7_1_CourseSelect::run 0x88a5c78) is the time attack's course
 * select on menu_layout_or_course: ten cards in menu order (menuOrderFromTenCourseID: C1
 * in, C1 out, the New Belt Line both ways, Wangan, Yokohane, Nagoya, Osaka, Fukuoka,
 * Hakone), at rest on k*100 + 1, the same moves and decide frames, the strip held at its
 * fourth place till the seventh card; mapChange as the time attack's; the top bar's
 * message 0x1be7.
 * seqTenLevelSelect (clSeqTenLevelSelectThread::run 0x88a6d14, its stPimpl 0x88aaf20):
 * - the levels as a carousel of menu_layout_or_lv boxes, each on its place -3..3 from the
 *   chosen one: at rest on the frames 0x940d118[place] (19 off screen, 39, 59, the chosen
 *   one's loop 65..123, 149, 169; 0 hidden), moving a place right on 0x940d100[place + 3]
 *   (5 frames on), left on 0x940d100[place + 2] backwards; decided, the chosen box on
 *   210..220, the others shrinking away on 0x940d100[9 + place]. clBox::init (0x88aada0)
 *   puts the level's name in 0x104c, its tune (0x940d320: the R35's tune step, the others'
 *   horsepower) in 0x19eb and the course's medal (0x940d3a4) in 0x3b6, but not on the
 *   32-man box.
 * - menu_layout_or_lv_ring: its flash (1..59) at each move; menu_layout_or_lv_arrow: both
 *   ways 1..59 looped, HARD only (61..) on the first level, EASY only (121..) on the last.
 * - menu_layout_or_lv_face: the level's rivals (0x940d160 into 0x71d..0x725; the boss is
 *   the "?"), popping in on 1..60, out on 61..70 once decided; on the 32-man level hidden.
 * - SE 0x19 a move, 0x1a decided, 0x1b back. Not ported: its time limit. */
#define TEN_COURSES 10
#define TEN_MOVE 6        /* setAnime(4, start, 5): start..start + 5 */
#define DECIDE_TEN_LV 11  /* 210..220 */
enum { T_OR_LV = 0x104c, T_OR_TUNE = 0x19eb, T_OR_MEDAL = 0x3b6, T_OR_FACE = 0x71d, S_MES_OR_COURSE = 0x1be7,
       S_MES_OR_LV = 0x1703 };
/* menu order -> clEffTenCourseType, and its EStage */
static const int TEN_ORDER[TEN_COURSES] = { 0, 1, 2, 3, 4, 5, 6, 7, 9, 8 };
static const int TEN_STAGE[TEN_COURSES] = { 333, 334, 335, 336, 337, 338, 341, 339, 340, 342 };
static const short OR_BOX[12] = { 19, 39, 59, 129, 149, 169, 65, 240, 260, 210, 280, 300 };
/* 0x940d320: the level's name, its horsepower, its R35 tune step */
static const short OR_LV[11][3] = {
  { 915, 3374, 6326 }, { 916, 3376, 6327 }, { 917, 3378, 6329 }, { 918, 3379, 6330 }, { 919, 3380, 6332 },
  { 920, 3381, 6333 }, { 921, 3382, 6334 }, { 922, 3383, 6335 }, { 923, 3385, 6337 }, { 924, 3386, 6338 },
  { 2514, 1, 1 },
};
static const short OR_MEDAL[4] = { 950, 2818, 2819, 1815 }; /* yn_waku, lv_or_bronze, _silver, _gold */
static const short OR_FACE[11][9] = {
  { 1821, 1822, 1823, 1824, 1825, 1826, 1827, 1831, 1829 },
  { 1822, 1824, 1825, 1831, 1830, 1841, 1842, 1845, 1846 },
  { 1825, 1853, 1829, 1831, 1832, 1841, 1842, 1843, 1849 },
  { 1853, 1826, 1825, 1827, 1828, 1847, 1842, 1848, 1850 },
  { 1837, 1838, 1839, 1840, 1836, 1828, 1826, 1833, 1834 },
  { 1824, 1828, 1841, 1829, 1844, 1849, 1835, 1848, 1836 },
  { 1829, 1835, 1848, 1837, 1836, 1828, 1833, 1826, 1834 },
  { 1835, 1838, 1839, 1840, 1834, 1836, 1843, 1844, 1848 },
  { 1831, 1841, 1853, 1843, 1844, 1845, 1846, 1847, 1848 },
  { 1853, 1842, 1843, 1845, 1846, 1847, 1848, 1849, 1850 },
  { 1852, 1852, 1852, 1852, 1852, 1852, 1852, 1852, 1852 },
};

/* the title page's time zone 0x15ed: menu_p_pre_day_night (3301) or _morning */
static int ten_night(int order, int level) { return TENDB_PRE[order][level][2] == 3301; }

int menu_ten_course(const menu_state *m) { return TEN_ORDER[m->ten_course]; }
int menu_ten_stage(const menu_state *m) { return TEN_STAGE[menu_ten_course(m)]; }

static void ten_course_enter(menu_state *m) {
  go(m, MENU_TEN_COURSE);
  if (getenv("WMMT_TEN_COURSE")) m->ten_course = atoi(getenv("WMMT_TEN_COURSE")); /* test: this card */
  m->sel = m->ten_course;
  m->course_pos = m->sel <= 3 ? m->sel : m->sel >= TEN_COURSES - 3 ? m->sel - (TEN_COURSES - 6) : 3;
  m->card_t = 0;
}

/* seqTenLevelSelect: the levels on offer (the 32-man one once all ten have gold), and the
 * first one without a medal (else the tenth) to start on */
static void ten_level_enter(menu_state *m) {
  const signed char *r = m->ten_result[menu_ten_course(m)];
  int gold = 1, first = -1;
  for (int l = 0; l < 10; ++l) {
    if (r[l] <= 2) gold = 0;
    if (r[l] == 0 && first < 0) first = l;
  }
  m->ten_levels = gold ? 11 : 10;
  go(m, MENU_TEN_LEVEL);
  m->sel = first >= 0 ? first : 9;
  m->ten_face_lv = m->sel;
  m->ten_face_t = 0;
  m->card_t = 0;
}

static void ten_level_step(menu_state *m, int tap, int key) {
  if (m->from >= 0 && m->t >= TEN_MOVE) m->from = -1;
  m->ten_face_t++;
  m->card_t++;
  if (m->from >= 0 || (!tap && !key)) return;
  /* a tap on the chosen box (x 192..448 on the arcade's screen) decides, one aside moves */
  int d = key ? key : m->tap_x < 192.0f ? -1 : m->tap_x > 448.0f ? 1 : 0;
  if (d == 0) {
    snd_menu(SND_SELECT); /* SE 0x1a */
    m->deciding = DECIDE_TEN_LV + FADE;
    m->t = 0;
    return;
  }
  int to = m->sel + d;
  if (to < 0 || to >= m->ten_levels) return;
  snd_menu(SND_CURSOR); /* SE 0x19 */
  /* the faces: hidden on the 32-man level, popping in again from it */
  if (to == 10) m->ten_face_lv = 10;
  else {
    if (m->ten_face_lv == 10) m->ten_face_t = 0;
    m->ten_face_lv = to;
  }
  m->from = m->sel;
  m->sel = to;
  m->t = 0;
}

static int or_box_frame(const menu_state *m, int i) {
  int s = i - m->sel;
  if (m->deciding) {
    int t = DECIDE_TEN_LV + FADE - m->deciding;
    if (t > 10) t = 10;
    if (s == 0) return OR_BOX[9] + t;
    return s >= -2 && s <= 2 ? OR_BOX[9 + s] + t : 0;
  }
  if (m->from >= 0 && m->t < TEN_MOVE) {
    int s0 = i - m->from, t = m->t < 5 ? m->t : 5;
    if (m->sel < m->from) return s0 >= -3 && s0 < 3 ? OR_BOX[s0 + 3] + t : 0;
    return s0 > -3 && s0 <= 3 ? OR_BOX[s0 + 2] + 5 - t : 0;
  }
  if (s == 0) return OR_BOX[6] + m->t % 59;
  return s >= -2 && s <= 2 ? OR_BOX[s + 3] : 0;
}

static void draw_ten_level(const menu_state *m, int clock, float fade, int bars) {
  draw_back(ID.bg, clock % 600, fade);
  anm_draw(ID.line, 0, fade);
  const signed char *res = m->ten_result[menu_ten_course(m)];
  const cardb_car *car = chosen_car(m, m->car);
  int r35 = has(car->label, "[R35]");
  for (int i = 0; i < m->ten_levels; ++i) {
    int f = or_box_frame(m, i);
    if (f == 0) continue;
    anm_swap sw[3] = { { anm_sprite(T_OR_LV), anm_sprite(OR_LV[i][0]) },
                       { anm_sprite(T_OR_TUNE), anm_sprite(OR_LV[i][r35 ? 2 : 1]) },
                       { anm_sprite(T_OR_MEDAL), anm_sprite(OR_MEDAL[res[i] >= 0 && res[i] <= 3 ? res[i] : 0]) } };
    anm_draw_ex(ID.or_lv, f, fade, 0, 0, sw, i < 10 ? 3 : 2);
  }
  if (!m->deciding) {
    /* m->t counts from the last move (or the screen's start) */
    if (m->t < 58) anm_draw(ID.or_lv_ring, 1 + m->t, fade);
    int a = m->sel == 0 ? 61 : m->sel == m->ten_levels - 1 ? 121 : 1;
    anm_draw(ID.or_lv_arrow, a + m->t % 59, fade);
  }
  if (m->ten_face_lv < 10) {
    anm_swap fc[9];
    for (int k = 0; k < 9; ++k) fc[k] = (anm_swap){ anm_sprite(T_OR_FACE + k), anm_sprite(OR_FACE[m->ten_face_lv][k]) };
    int f;
    if (m->deciding) {
      int t = DECIDE_TEN_LV + FADE - m->deciding;
      f = 61 + (t < 9 ? t : 9);
    } else {
      f = 1 + (m->ten_face_t < 59 ? m->ten_face_t : 59);
    }
    anm_draw_ex(ID.or_lv_face, f, fade, 0, 0, fc, 9);
  }
  anm_swap mes = { anm_sprite(T_TOPBAR_MES), anm_sprite(S_MES_OR_LV) };
  draw_topbar(bars, fade, &mes);
  anm_draw(ID.bottom, bars, fade);
}

void menu_init(menu_state *m) {
  ID.title = anm_find("atract_layout_title");
  ID.bg = anm_find("menu_g_bg_image");
  ID.line = anm_find("menu_g_bg_line");
  ID.logo = anm_find("menu_g_bg_logo");
  ID.top = anm_find("menu_g_topbar_bar");
  ID.bottom = anm_find("menu_g_bottombar_bar");
  ID.mode = anm_find("menu_layout_ta_carsel");
  ID.mode_ring = anm_find("menu_layout_ta_carsel_ring");
  for (int k = 0; k < 2; ++k) mode_cars[k] = (anm_swap){ anm_sprite(MODE_CAR_TEMPLATE[k]), -1 };
  ID.game = anm_find("menu_layout_mode_sel");
  ID.game_image = anm_find("menu_layout_mode_image");
  ID.game_ring = anm_find("menu_layout_mode_ring");
  ID.vs_sel = anm_find("menu_layout_mode_sel_3");
  ID.vs_sel_image = anm_find("menu_layout_mode_sel_3_image");
  ID.vs_sel_ring = anm_find("menu_layout_mode_sel_3_ring");
  ID.vs_course = anm_find("menu_layout_vs_course1");
  ID.vs_icon = anm_find("menu_g_vs_icon");
  ID.vs_icon_omakase = anm_find("menu_g_vs_icon_omakase");
  ID.vs_icon_omakase2 = anm_find("menu_g_vs_icon_omakase2");
  ID.vs_ex_player = anm_find("menu_g_vs_ex_player");
  ID.timer = anm_find("menu_g_timer_x");
  ID.shogo_bg = anm_find("menu_g_shogo_bg_1");
  for (int n = 0; n < 3; ++n) {
    char a[48];
    snprintf(a, sizeof a, "menu_p_pre_vs_rival_%d_shadow", n + 2);
    ID.pre_shadow[n] = anm_find(a);
    snprintf(a, sizeof a, "menu_p_pre_vs_rival_%d_car", n + 2);
    ID.pre_cars[n] = anm_find(a);
    snprintf(a, sizeof a, "menu_p_pre_vs_rival_%d_vs", n + 2);
    ID.pre_vs[n] = anm_find(a);
  }
  ID.plate = anm_find("menu_g_car_tg_decide");
  ID.plate_ph = anm_find("menu_g_car_tg_decide_ph");
  ID.course = anm_find("menu_layout_ta_course");
  ID.course_map = anm_find("menu_g_course_map");
  ID.arrow = anm_find("menu_layout_ta_course_arrow");
  ID.maker[0] = anm_find("atract_layout_logo_bng");
  ID.maker[1] = anm_find("atract_layout_logo_namco");
  ID.maker[2] = anm_find("atract_layout_logo_nvidia");
  ID.prologue[0] = anm_find("atract_g_02");
  ID.prologue[1] = anm_find("atract_g_01");
  ID.pre_bg = anm_find("menu_g_pre_bg");
  ID.ranking_bg = anm_find("menu_g_ranking_bg2");
  ID.ranking = anm_find("menu_layout_ranking");
  ID.bg_carsel = anm_find("menu_g_bg_carsel_2");
  ID.maker_layout = anm_find("menu_layout_carsel_maker_");
  ID.maker_car = anm_find("menu_g_carsel_maker_car_image");
  ID.car_info = anm_find("menu_g_carsel_car");
  ID.car_arrow = anm_find("menu_layout_carsel_car_arrow");
  ID.car_new = anm_find("menu_layout_carsel_new");
  ID.mes_r35 = anm_find("menu_layout_carsel_mes_r35");
  ID.mes_jzs161 = anm_find("menu_p_carsel_mes_jzs161");
  ID.col_strip[0] = anm_find("menu_g_carsel_col_04");
  ID.col_strip[1] = anm_find("menu_g_carsel_col_05");
  ID.col_strip[2] = anm_find("menu_g_carsel_col_06");
  ID.col_arrow[0] = anm_find("menu_g_carsel_col_04_arrow");
  ID.col_arrow[1] = anm_find("menu_g_carsel_col_05_arrow");
  ID.col_arrow[2] = anm_find("menu_g_carsel_col_06_arrow");
  ID.col_strip[3] = anm_find("menu_g_carsel_col_01"); /* (no arrows) */
  ID.col_arrow[3] = -1;
  ID.col_name = anm_find("menu_g_carsel_col_name");
  ID.mes_maker = anm_find("menu_g_topbar_mes_carsel_maker");
  ID.mes_car = anm_find("menu_g_topbar_mes_carsel_car");
  ID.mes_color = anm_find("menu_g_topbar_mes_carsel_color");
  ID.set_handle = anm_find("menu_g_setting_handle");
  ID.card_waku = anm_find("menu_g_cardcheck_1_waku");
  ID.card_layout = anm_find("menu_layout_cardcheck_1");
  ID.day_cards = anm_find("menu_layout_yn_card_1_gold");
  ID.day_ring = anm_find("menu_layout_yn_ring");
  ID.day_vs = anm_find("menu_layout_yn_day_vs");
  ID.day_vs_ring = anm_find("menu_layout_yn_day_vs_ring");
  ID.day_sound = anm_find("menu_g_yn_day_sound");
  ID.pre_ta = anm_find("menu_layout_pre_ta");
  ID.pre_sound = anm_find("menu_g_pre_sound");
  ID.pre_st = anm_find("menu_layout_pre_st");
  ID.pre_sound_st = anm_find("menu_g_pre_sound_st");
  ID.loading = anm_find("menu_layout_pre_vs_loading");
  ID.name_select = anm_find("menu_layout_name_select");
  ID.name_back = anm_find("menu_g_name_name_back");
  ID.name_use = anm_find("menu_use_name");
  ID.name_line = anm_find("menu_g_name_line");
  ID.name_tab[0] = anm_find("menu_g_name_katakana");
  ID.name_tab[1] = anm_find("menu_g_name_hiragana");
  ID.name_tab[2] = anm_find("menu_g_name_capital");
  ID.name_tab[3] = anm_find("menu_g_name_small");
  ID.name_tab[4] = anm_find("menu_g_name_end");
  static const char *const COLOR[4] = { "blue", "green", "orange", "red" };
  char name[48];
  ID.st_tune = anm_find("menu_layout_st_tune");
  ID.st_tune_serifu = anm_find("menu_layout_st_tune_serifu");
  ID.bg_st_tune = anm_find("menu_g_bg_image_st_tune");
  ID.gage = anm_find("menu_g_setting_gage_1");
  ID.gage_waku = anm_find("menu_g_setting_gage_waku");
  ID.gage_waku2 = anm_find("menu_g_setting_gage_waku_2");
  ID.ph = anm_find("menu_g_setting_p_h");
  ID.shogo_bg1 = anm_find("menu_g_shogo_bg_1");
  ID.shogo_bg2 = anm_find("menu_g_shogo_bg_2");
  ID.lvup = anm_find("menu_layout_lvup");
  ID.star_bg = anm_find("menu_g_vs_star_bg");
  ID.star_logo = anm_find("menu_layout_vs_star_logo");
  ID.star_car = anm_find("menu_layout_vs_star_car");
  ID.star_title = anm_find("menu_layout_vs_star_title");
  ID.star_wins = anm_find("menu_layout_vs_star_wins");
  ID.star_use = anm_find("menu_use_vs_star");
  for (int k = 0; k < 3; ++k) {
    char n[48];
    snprintf(n, sizeof n, "menu_g_vs_star_%d", k + 1);
    ID.star_add[k] = anm_find(n);
    snprintf(n, sizeof n, "menu_g_vs_star_keta_a_%d", k + 1);
    ID.star_keta_a[k] = anm_find(n);
    snprintf(n, sizeof n, "menu_g_vs_star_keta_b_%d", k + 1);
    ID.star_keta_b[k] = anm_find(n);
  }
  ID.star_finish[0] = anm_find("menu_g_vs_star_finish_1");
  ID.star_finish[1] = anm_find("menu_g_vs_star_finish_100");
  ID.star_finish[2] = anm_find("menu_g_vs_star_finish_1000");
  ID.star_finish[3] = anm_find("menu_g_vs_star_finish_10000");
  ID.star_minus[0] = anm_find("menu_g_vs_star_minus1");
  ID.star_minus[1] = anm_find("menu_g_vs_star_minus2");
  ID.shogo_moji = anm_find("menu_g_shogo_bg_moji");
  ID.shogo_title = anm_find("menu_g_shogo_title");
  ID.shogo_n[0] = anm_find("menu_g_shogo_shogo_1");
  ID.shogo_n[1] = anm_find("menu_g_shogo_shogo_2");
  ID.shogo_n[2] = anm_find("menu_g_shogo_shogo_3");
  ID.shogo_rw = anm_find("menu_layout_shogo_rewrite");
  ID.yn_rank = anm_find("menu_layout_yn_rank");
  ID.eff_top = anm_find("menu_g_topbar_st_install");
  ID.eff_result = anm_find("menu_layout_st_instal_result");
  ID.eff_handle = anm_find("menu_layout_st_instal_result_handle");
  ID.eff_complete = anm_find("menu_layout_st_install_complete");
  ID.eff_complete2 = anm_find("menu_layout_st_install_complete2");
  static const char *const INSTALL[13] = { "front_a", "left_a", "left_b", "left_c", "left_d", "left_e", "leftback_a",
                                           "leftback_b", "leftback_c", "leftfront_a", "leftfront_b", "rightback_a",
                                           "rightback_b" };
  for (int k = 0; k < 13; ++k) {
    snprintf(name, sizeof name, "menu_layout_st_install_%s", INSTALL[k]);
    ID.eff_install[k] = anm_find(name);
  }
  ID.tg_bg1 = anm_find("menu_g_tg_bg_1");
  ID.tg_bg3 = anm_find("menu_g_tg_bg_3");
  ID.top_notime = anm_find("menu_g_topbar_bar_notime");
  ID.tg_get_title = anm_find("menu_layout_tg_tune_get_title");
  ID.tg_get_parts = anm_find("menu_layout_tg_tune_get_parts");
  ID.tg_aero = anm_find("menu_layout_tg_tune_aero");
  ID.tg_aero2 = anm_find("menu_layout_tg_tune_aero_2");
  ID.tg_aero3 = anm_find("menu_layout_tg_tune_aero_3");
  ID.tg_aero5 = anm_find("menu_layout_tg_tune_aero_5");
  ID.tg_get_image = anm_find("menu_layout_tg_tune_get_parts_image");
  ID.tg_get_wheel = anm_find("menu_layout_tg_tune_get_wheel");
  ID.tg_bonnet = anm_find("menu_layout_tg_tune_4_bonnet");
  ID.tg_bonnet_arrow = anm_find("menu_layout_tg_tune_4_arrow");
  ID.tg_wing5 = anm_find("menu_layout_tg_tune_5_wing");
  ID.tg_wing5_arrow = anm_find("menu_layout_tg_tune_5_arrow");
  ID.tg_wing6 = anm_find("menu_layout_tg_tune_6_wing");
  ID.tg_wing6_arrow = anm_find("menu_layout_tg_tune_6_arrow");
  ID.tg_mirror = anm_find("menu_layout_tg_tune_2");
  ID.tg_mirror_arrow = anm_find("menu_layout_tg_tune_2_arrow");
  ID.tg_neon = anm_find("menu_layout_tg_tune_8_neon");
  ID.tg_neon_arrow = anm_find("menu_layout_tg_tune_8_neon_arrow");
  ID.tg_vinyl = anm_find("menu_layout_tg_tune_8_vinyl");
  ID.tg_vinyl_col = anm_find("menu_layout_tg_tune_8_vinyl_col");
  ID.tg_vinyl_arrow = anm_find("menu_layout_tg_tune_vinyl_arrow_8");
  ID.tg_vinyl_gem = anm_find("menu_layout_tg_tune_2_gem");
  ID.tg_wear_title = anm_find("menu_layout_tg_tune_get_title_no_lv");
  ID.tg_wear_logo = anm_find("menu_layout_tg_tune_install_wheel_logo");
  ID.whiteout = anm_find("menu_p_whiteout");
  {
    static const char *const BAR[5] = { "10", "5", "16", "8", "11" }, *const ARROW[5] = { "10", "5", "11", "8", "11" };
    char name[64];
    for (int k = 0; k < 5; ++k) {
      snprintf(name, sizeof name, "menu_g_tg_tune_wheel_%s", BAR[k]);
      ID.tg_wheel_bar[k] = anm_find(name);
      snprintf(name, sizeof name, "menu_layout_tg_tune_wheel_arrow_%s", ARROW[k]);
      ID.tg_wheel_arrow[k] = anm_find(name);
    }
  }
  ID.tg_wheel_col = anm_find("menu_layout_tg_tune_wheel_col_col");
  ID.tg_wheel_cursor = anm_find("menu_layout_tg_tune_wheel_col_cursor");
  ID.tg_wheel_logo = anm_find("menu_layout_tg_tune_wheel_col_logo");
  ID.tg_wheel_waku = anm_find("menu_layout_tg_tune_wheel_col_waku_2");
  ID.tg_complete = anm_find("menu_layout_tg_tune_complete");
  ID.tg_all[0] = anm_find("menu_layout_tg_tune_all");
  ID.tg_all[1] = anm_find("menu_layout_tg_tune_gem_all");
  ID.wmaker_bg = anm_find("menu_layout_wheelmaker_bg");
  ID.wmaker_sel = anm_find("menu_layout_wheelmaker_sel");
  ID.st_level = anm_find("menu_layout_st_level");
  ID.st_level_arrow = anm_find("menu_layout_st_level_arrow");
  ID.st_level_clear = anm_find("menu_layout_st_level_clear");
  ID.st_level_ring = anm_find("menu_layout_st_level_ring");
  ID.st_lv_back = anm_find("menu_g_st_lv_back");
  ID.st_rival = anm_find("menu_layout_st_rival");
  ID.st_rival_arrow = anm_find("menu_layout_st_rival_arrow");
  ID.st_rival_ramp = anm_find("menu_layout_st_rival_ramp");
  for (int k = 0; k < 4; ++k) {
    snprintf(name, sizeof name, "menu_g_st_lv_%s", COLOR[k]);
    ID.st_lv[k] = anm_find(name);
    snprintf(name, sizeof name, "menu_g_st_rival_%s", COLOR[k]);
    ID.st_ep[k][0] = anm_find(name);
    snprintf(name, sizeof name, "menu_g_st_rival_%s2", COLOR[k]);
    ID.st_ep[k][1] = anm_find(name);
  }
  ID.finish[0] = anm_find("menu_g_finish_timeup_ta");
  ID.finish[1] = anm_find("menu_g_finish_finish");
  ID.win = anm_find("menu_g_finish_win");
  ID.lose = anm_find("menu_g_finish_lose");
  ID.st_adv = anm_find("menu_g_finish_tg_result");
  ID.st_waku = anm_find("menu_g_finish_point_waku");
  ID.st_bar = anm_find("menu_g_finish_st_bar");
  ID.st_moji = anm_find("menu_g_finish_st_moji");
  ID.res_course = anm_find("menu_g_finish_ta_course");
  ID.res_best[0] = anm_find("menu_g_finish_ta_total_best");
  ID.res_best[1] = anm_find("menu_g_finish_ta_car_best");
  ID.res_best[2] = anm_find("menu_g_parsonal_best");
  ID.res_congrats = anm_find("menu_g_finish_ta_congratulations");
  ID.res_sections = anm_find("menu_g_finish_ta_section_1");
  ID.res_sections2 = anm_find("menu_g_finish_ta_section_2");
  ID.res_total = anm_find("menu_g_finish_ta_totaltime");
  ID.res_speed = anm_find("menu_g_finish_ta_topspeed");
  ID.res_rank = anm_find("menu_g_finish_ta_ranking");
  ID.or_course = anm_find("menu_layout_or_course");
  ID.or_lv = anm_find("menu_layout_or_lv");
  ID.or_lv_ring = anm_find("menu_layout_or_lv_ring");
  ID.or_lv_arrow = anm_find("menu_layout_or_lv_arrow");
  ID.or_lv_face = anm_find("menu_layout_or_lv_face");
  ID.pre_or = anm_find("menu_layout_pre_or");
  ID.ten_timeup = anm_find("menu_g_finish_timeup");
  ID.ten_face = anm_find("menu_layout_or_result_face");
  ID.ten_x = anm_find("menu_g_or_finish_X");
  ID.ten_finish = anm_find("menu_layout_or_finish_result");
  ID.ten_summary = anm_find("menu_layout_or_result_A");
  ranking_init();
  km_init();
  /* the FD3S, the car the port started with */
  m->maker = 1;
  m->car = 2;
  m->color = 0;
  /* getBgmSelect1st: a player without a card starts on RANDOM */
  m->bgm = 0;
  m->song = -1;
  m->music = 0;
  m->zone = 0;
  m->zones = 1;
  menu_garage_reset(m);
  m->gyro = 0, m->gyro_sens = 5, m->over_on = 1, m->sharp_on = 1, m->ao_on = 1, m->bm_on = 1;
  for (int g = 0; g < 3; ++g) m->key_k[g] = 100, m->key_dx[g] = m->key_dy[g] = 0;
  m->poem = -1;
  /* (the SSAO off: its 20 ms on the Mi 6 cost the 60 fps) */
  m->hd_tex = 1, m->bloom_on = 1, m->ssao_on = 0, m->wet_on = 1, m->paint_on = 1, m->aa4 = 1;
  m->garage_changed = 0;
  m->game = MENU_GAME_TA;
  m->story_sel = -1;
  m->story_pick = m->story_n = -1;
  m->ten_levels = 10;
  m->save_slot = m->save_skip = 0;
  srand((unsigned)time(NULL));
  /* the arcade boots into the maker logos and the prologue; the attract movie follows them */
  m->demo = -1;
  m->screen = MENU_LOGO;
  m->t = 0;
  m->tap = m->key = m->key_held = 0;
}

void menu_garage_reset(menu_state *m) {
  /* every car stock, its full tune leaning to power (the port's tune before the setting) */
  memset(m->tune_full, 0, sizeof m->tune_full);
  memset(m->tune_power, CAR_TUNE_MAX, sizeof m->tune_power);
  m->manual = 0;
  m->st_div = m->st_bits = 0;
  m->st_level = 0;
  m->ten_course = m->ten_level = 0;
  memset(m->ten_result, 0, sizeof m->ten_result);
  memset(m->st_power, 0, sizeof m->st_power);
  memset(m->st_handling, 0, sizeof m->st_handling);
  memset(m->st_gauge, 0, sizeof m->st_gauge);
  memset(m->dress_lv, 0, sizeof m->dress_lv);
  memset(m->aero, 0, sizeof m->aero);
  memset(m->mirror, 0, sizeof m->mirror);
  memset(m->wing, 0, sizeof m->wing);
  memset(m->bonnet, 0, sizeof m->bonnet);
  memset(m->wheel, 0, sizeof m->wheel);
  m->st_tune_due = m->n_dress_due = 0;
  m->st_lost = 0;
  m->n_own = 0;
  m->own_sel = m->own_view = -1;
  memset(m->st_div_car, 0, sizeof m->st_div_car);
  memset(m->st_bits_car, 0, sizeof m->st_bits_car);
  memset(m->ten_car, 0, sizeof m->ten_car);
  memset(m->class_car, 0, sizeof m->class_car);
  memset(m->title_car, 0, sizeof m->title_car);
  memset(m->st_streak_car, 0, sizeof m->st_streak_car);
  memset(m->vs_wins_car, 0, sizeof m->vs_wins_car);
  memset(m->vs_stars_car, 0, sizeof m->vs_stars_car);
  memset(m->vs_count_car, 0, sizeof m->vs_count_car);
  memset(m->st_bonus_car, 0, sizeof m->st_bonus_car);
  m->title_n = 0;
  memset(m->name, -1, sizeof m->name);
}

static void go(menu_state *m, int screen) {
  m->screen = screen;
  m->pre_quiet = 0;
  m->t = 0;
  m->load_shown = m->load_out = 0;
  m->sel = 0;
  m->from = -1;
  m->deciding = 0;
  m->lock = 0;
}

void menu_open(menu_state *m) {
  car_select_enter(m, m->game != MENU_GAME_TA ? MENU_GAME : MENU_MODE);
  m->bars = 1;
  m->tap = m->back = m->leaving = 0;
  m->key = m->key_held = 0;
}

static int in_attract(const menu_state *m) { return m->screen <= MENU_PROLOGUE; }

int menu_booting(const menu_state *m) { return m->demo < 0; }

void menu_start(menu_state *m) {
  if (!in_attract(m) || menu_booting(m)) return;
  snd_menu(SND_START);
  go(m, MENU_START);
  m->music = 0;
}

int menu_movie(const menu_state *m) { return m->screen == MENU_MOVIE ? m->demo % 3 : -1; }

void menu_movie_done(menu_state *m) {
  if (m->screen == MENU_MOVIE) go(m, MENU_TITLE);
}

void menu_touch(menu_state *m, void *id, int phase, float x, float y) {
  if (m->finish) {
    if (phase == TOUCH_BEGAN) m->tap = 1; /* anywhere: leaves the result */
    return;
  }
  if (m->pause.open) {
    if (phase == TOUCH_BEGAN) {
      m->pause.tap = 1;
      m->pause.tap_x = x * 640.0f / 1024.0f;
      m->pause.tap_y = y * 480.0f / 768.0f;
    }
    return;
  }
  if (m->screen == MENU_LAYOUT && !(id && id == m->back_touch) && layout_touch(m, id, phase, x, y)) return;
  /* the back key works like a button: lit while pressed, it goes back only when the finger
   * comes off it (sliding off cancels), so a stray tap elsewhere never leaves the screen */
  if (id && id == m->back_touch) {
    int on = hud_menu_key(x, y) == HUD_MENU_BACK;
    if (phase == TOUCH_MOVED) {
      m->key_held = on ? HUD_MENU_BACK : 0;
      return;
    }
    if (phase == TOUCH_ENDED) {
      m->back_touch = 0;
      m->key_held = 0;
      if (on && m->screen >= MENU_MAKER) m->back = 1;
      return;
    }
  }
  if (phase == TOUCH_ENDED) m->vs.held = 0;
  if (phase == TOUCH_ENDED && !m->back_touch) m->key_held = 0;
  if (phase != TOUCH_BEGAN) return;
  if (in_attract(m)) {
    m->tap = 1; /* anywhere: the start button */
    return;
  }
  int key = hud_menu_key(x, y);
  if (key == HUD_MENU_BACK) {
    if (m->screen >= MENU_MAKER) m->back_touch = id, m->key_held = key;
    return;
  }
  /* the versus screen's own buttons; the game mode select's VS key (the OPTION button's place) */
  if (m->screen == MENU_VS) {
    m->vs.tap_x = x, m->vs.tap_y = y;
    m->vs.tap = 1;
    m->vs.held = vs_hit(m, x, y) + 1;
    return;
  }
  /* the maker select's and the SAVE DATA screen's gear key: the OPTION screen */
  if (key == HUD_MENU_OPTION && (m->screen == MENU_MAKER || (m->screen == MENU_SAVE && !m->save_confirm))) {
    m->option = 1;
    m->key_held = key;
    return;
  }
  /* the maker select's hidden cars key */
  if (key == HUD_MENU_HIDDEN && m->screen == MENU_MAKER) {
    m->hidden = 1;
    m->key_held = key;
    return;
  }
  /* the colour select's ALL PARTS key (a new kind of car's): on and off */
  if (key == HUD_MENU_HIDDEN && all_parts_shown(m)) {
    m->all_parts ^= 1;
    snd_menu(SND_CURSOR);
    m->key_held = key;
    return;
  }
  /* the save's car view's bin (in the gear key's place) */
  if (key == HUD_MENU_OPTION && m->screen == MENU_GARAGE && !m->deciding) {
    m->erase = 1;
    m->key_held = key;
    return;
  }
  /* the car setting's wheel key (in the gear key's place): the dress-up menu (a new car's
   * once its card is made) */
  if (key == HUD_MENU_OPTION && m->screen == MENU_SETTING && !m->new_card && !m->deciding) {
    m->dress = 1;
    m->key_held = key;
    return;
  }
  /* the votes' omakase key */
  if (key == HUD_MENU_HIDDEN && vs_omakase_shown(m)) {
    vs_omakase(m);
    m->key_held = key;
    return;
  }
  /* the maker and colour screens have no keys: their items are tapped */
  if ((key == 1 || key == -1) && m->screen != MENU_MAKER && m->screen != MENU_COLOR) {
    m->key = m->key_held = key;
    return;
  }
  /* to the arcade's 640x480 screen */
  m->tap_x = x * 640.0f / 1024.0f;
  m->tap_y = y * 480.0f / 768.0f;
  m->tap = 1;
}

/* Which way a tap points on the current screen: -1 previous, +1 next, 0 the selected item. */
/* the course strip's shift: 88 px a card the selection has gone past its place on screen,
 * sliding 88/6 px a frame while it moves */
static float course_dx(const menu_state *m) {
  float to = (float)(m->sel - m->course_pos);
  if ((m->screen == MENU_COURSE || m->screen == MENU_TEN_COURSE) && m->from >= 0 && m->t < 6) {
    float from = (float)m->course_scroll_from;
    to = from + (to - from) * (float)m->t / 6.0f;
  }
  return -88.0f * to;
}

static int tap_dir(const menu_state *m) {
  if (m->screen == MENU_GAME) {
    /* menu_layout_mode_sel at rest on mode sel: the discs' centres (layout px, y 192), the
     * chosen one's 128 round, the others' 64; a tap on another disc moves one step its way */
    static const float AT[GAME_SLOTS][GAME_SLOTS] = {
      { 139, 326, 440, 554 }, { 84, 260, 440, 554 }, { 84, 198, 380, 554 }, { 84, 198, 312, 503 } };
    if (m->tap_y > 300.0f) return 0;
    const float *at = AT[m->sel >= 0 && m->sel < GAME_SLOTS ? m->sel : 0];
    int hit = m->sel;
    for (int k = 0; k < GAME_SLOTS; ++k)
      if (fabsf(m->tap_x - at[k]) < (k == m->sel ? 128.0f : 64.0f)) hit = k;
    return hit < m->sel ? -1 : hit > m->sel ? 1 : 0;
  }
  if (m->screen == MENU_VS_SEL) {
    /* menu_layout_mode_sel_3 at rest: the discs' centres (as the game mode select's) */
    static const float AT[3][3] = { { 139, 376, 554 }, { 84, 320, 554 }, { 84, 272, 503 } };
    if (m->tap_y > 300.0f) return 0;
    const float *at = AT[m->sel >= 0 && m->sel < 3 ? m->sel : 0];
    int hit = m->sel;
    for (int k = 0; k < 3; ++k)
      if (fabsf(m->tap_x - at[k]) < (k == m->sel ? 128.0f : 64.0f)) hit = k;
    return hit < m->sel ? -1 : hit > m->sel ? 1 : 0;
  }
  if (m->screen == MENU_MODE) {
    int item = m->tap_x < 320.0f ? 0 : 1;
    return item - m->sel;
  }
  /* the selected course card moves right by about 88 px per course, the strip as it slides */
  float left = 18.0f + 88.5f * (float)m->sel + course_dx(m), right = left + 164.0f;
  if (m->tap_x < left) return -1;
  if (m->tap_x > right) return 1;
  return 0;
}

/* ---- dress-up (the ghost battle's "TG" parts, src/dressdb.h). The arcade wins its parts in
 * ghost battle, a level a battle won (1..63, get_tb_level_parts); here a story race won or a
 * 10-in-a-row rival beaten. An aero set won comes up on the way back from the race:
 * - MENU_TG_GET, clSeqTgPartsGetAeroThread::run: menu_g_tg_bg_1 and _3, the top bar without
 *   time, menu_layout_tg_tune_get_title (the level's digits in templates 0x1efe, 0x1efc: no
 *   tens below 10; the kind's title in 0x1f96) and menu_layout_tg_tune_get_parts (the part's
 *   name picture in 0x1f7c, get_parts_data + 0x18). The car turns before it: 0.66 degrees a
 *   frame from 0, at (0.5, -5, -9.5) heading angle - 21.3 degrees, the camera 60 degrees
 *   across from (0, 10, 50) at the origin. The screen fades in over 10 frames, then cutin2
 *   (SE 0x22); it goes once the angle is past -185 or on a tap 3.2 s on, fading out over 10.
 * - MENU_TG_AERO, clSeqTgChangePartsAeroThread: the choices are the car's stock body and the
 *   aero sets it has won (get_parts_id_table_from_dressup_jyunsei), then Refuse to change
 *   (not on the gem cars); the layout by their number (2 menu_layout_tg_tune_aero_2, 3 _3,
 *   4 menu_layout_tg_tune_aero, 5 _5), 100 frames a place: choice i is at place
 *   r = (sel - i) mod n, resting on 100r..100r+59, sliding on 100r+60..88 (backwards going
 *   left), decided 100r+90..98. Each place's dot (0x6b4) carries a clSpriteAnimeCPModel:
 *   the car in that aero set, scaled 3 * the dot's scale, turned (10, -30, -4) degrees at
 *   (0, 20, z), seen from (0, 0, 1000) on the axis through the dot; z eases 13.33 a frame
 *   to 0 at places 0 and 1, to -400 at the others. The plate 0x1f48 becomes 0x1bc8, the
 *   names (0x1344, 0x1ec2) Original shape, Aero set A..C (the gem cars': Aero set) or Refuse
 *   to change, NEW (0xb83) only on the newest set. It opens on the newest; cursor and decide
 *   SE 0x19, 0x1a; the decision fades out over 20 frames. The arcade's 10 s limit (the port
 *   has none in the menus: the bar's infinity). ---- */
static int tg_gem(const menu_state *m) {
  const char *p = CARDB_PACKS[chosen_car(m, m->car)->colors[0].pack];
  return p[0] == 'g' && p[1] == 'e' && p[2] == 'm';
}

void menu_dress_win(menu_state *m, int wins) {
  int mk = m->maker, c = m->car, gem = tg_gem(m);
  for (int k = 0; k < wins && m->dress_lv[mk][c] < 63; ++k) {
    int lv = ++m->dress_lv[mk][c];
    /* every part but the gem cars' empty levels (kind 9) comes up on its GET screen */
    if (DRESS_LADDER[gem][lv][0] != 9 && m->n_dress_due < MENU_DRESS_DUE) m->dress_due[m->n_dress_due++] = lv;
    /* a wheel won is chosen on its change screen after its GET (tg_wheel_open) */
    m->garage_changed = 1;
  }
}

const char *menu_wheel_get(int wheel) {
  for (size_t i = 0; wheel > 0 && i < sizeof DRESS_WHEELS / sizeof DRESS_WHEELS[0]; ++i)
    if (DRESS_WHEELS[i].model == wheel - 1) return DRESS_WHEELS[i].menu3d;
  return NULL;
}

const char *menu_wheel_pack(int wheel, int *set) {
  for (size_t i = 0; wheel > 0 && i < sizeof DRESS_WHEELS / sizeof DRESS_WHEELS[0]; ++i)
    if (DRESS_WHEELS[i].model == wheel - 1) {
      *set = DRESS_WHEELS[i].set;
      return DRESS_WHEELS[i].name;
    }
  return NULL;
}

static int tg_maker_newest(const menu_state *m, int maker);
static const signed char WM_MAKER[4];

int menu_wheel_next(const menu_state *m, int *out, int max) {
  int n = 0;
#define WANT(w) do { int w_ = (w); if (w_ > 0 && n < max) out[n++] = w_; } while (0)
  if (m->screen == MENU_TG_GET && m->tg.kind == 0) {
    WANT(m->tg.num + 1);
    WANT(m->wheel[m->maker][m->car]);
  } else if (m->screen == MENU_TG_WHEEL) {
    for (int d = 1; d <= 2; ++d) {
      if (m->sel + d < m->tg.n) WANT(tg_wheel_value(m, m->sel + d, 0));
      if (m->sel - d >= 0) WANT(tg_wheel_value(m, m->sel - d, 0));
    }
    for (int c = 1; c < 6 && m->sel > 1; ++c) WANT(tg_wheel_value(m, m->sel, (m->tg.col + c) % 6));
  } else if (m->screen == MENU_TG_WEAR && m->tg.kind == 0) {
    WANT(m->tg.chosen);
  } else if (m->screen == MENU_TG_MAKER) {
    /* the maker's bar opens on its newest */
    WANT(tg_maker_newest(m, WM_MAKER[m->sel]));
  }
#undef WANT
  return n;
}

static void tg_get_open(menu_state *m) {
  int lv = m->dress_due[0], gem = tg_gem(m);
  for (int i = 1; i < m->n_dress_due; ++i) m->dress_due[i - 1] = m->dress_due[i];
  m->n_dress_due--;
  memset(&m->tg, 0, sizeof m->tg);
  go(m, MENU_TG_GET);
  m->tg.lv = lv;
  m->tg.gem = gem;
  m->tg.kind = DRESS_LADDER[gem][lv][0];
  m->tg.num = DRESS_LADDER[gem][lv][1];
  m->tg.name = DRESS_LADDER[gem][lv][2];
  m->tg.image = DRESS_LADDER[gem][lv][3];
  /* the wheel's turn starts at -60 degrees (clSeqTgPartsGetWheelThread::run) */
  if (m->tg.kind == 0) m->tg.angle = TG_WHEEL_START;
  m->card_t = 0;
  m->bars = 0;
  m->music = 1;
}

/* the aero sets won by the car's level, in their order (get_parts_id_table_from_dressup_jyunsei) */
static void tg_aero_open(menu_state *m) {
  int mk = m->maker, c = m->car, gem = tg_gem(m), lv = m->dress_lv[mk][c];
  go(m, MENU_TG_AERO);
  m->tg.gem = gem;
  m->tg.n = 0;
  m->tg.opt[m->tg.n++] = 0;
  for (int l = 1; l <= lv && l < 64; ++l)
    if (DRESS_LADDER[gem][l][0] == 1 && m->tg.n < 4) m->tg.opt[m->tg.n++] = DRESS_LADDER[gem][l][1];
  m->tg.newest = m->tg.n - 1;
  if (!gem) m->tg.opt[m->tg.n++] = -1;
  m->sel = m->tg.newest;
  m->tg.move = m->tg.move_t = m->tg.decided = 0;
  /* the run's 30 frames before it shows: the depths settled */
  for (int i = 0; i < m->tg.n; ++i) {
    int r = ((m->sel - i) % m->tg.n + m->tg.n) % m->tg.n;
    m->tg.z[i] = r <= 1 ? 0.0f : -400.0f;
  }
  m->card_t = 0;
  m->bars = 1;
  m->music = 1;
}

/* after a race (or the story's tuning screen): the dress-up screens due, else the mode select */
static int tg_due(menu_state *m) {
  if (!m->n_dress_due) return 0;
  m->dress_free = 0;
  tg_get_open(m);
  m->tap = m->key = m->key_held = m->back = m->leaving = 0;
  return 1;
}


static void tg_step(menu_state *m, int tap, int key) {
  m->card_t++;
  if (m->screen == MENU_TG_GET) {
    int type = tg_get_type(m);
    /* the wheel turns 0.3 degrees a frame till -120 (then only fading out), the car 0.66 */
    if (type != TG_GET_WHEEL) m->tg.angle -= 0.66f;
    else if (m->tg.angle >= TG_WHEEL_END || m->deciding) m->tg.angle -= 0.3f;
    if (m->card_t == TG_FADE) snd_menu(SND_CUTIN2);
    /* the port's: a tap once the screen is in goes on (a 10-in-a-row run brings a part a
     * rival beaten); the arcade's car and wheel screens take one 3.2 s on, the others none */
    int end = type == TG_GET_CAR ? m->tg.angle < TG_GET_END
              : type == TG_GET_WHEEL ? m->tg.angle < TG_WHEEL_END : m->card_t >= TG_FADE + TG_GET_HOLD;
    if (end || (tap && m->card_t >= TG_FADE)) m->deciding = TG_FADE;
    return;
  }
  for (int i = 0; i < m->tg.n; ++i) {
    if (tg_place(m, i) <= 1) m->tg.z[i] = m->tg.z[i] + 13.333333f > 0.0f ? 0.0f : m->tg.z[i] + 13.333333f;
    else m->tg.z[i] = m->tg.z[i] - 13.333333f < -400.0f ? -400.0f : m->tg.z[i] - 13.333333f;
  }
  if (m->tg.move) {
    if (++m->tg.move_t >= TG_MOVE) m->tg.move = 0;
    return;
  }
  if (m->card_t < TG_FADE) return;
  int d = key;
  if (!d && tap) d = m->tap_x < 200.0f ? -1 : m->tap_x > 440.0f ? 1 : 0;
  if (key || (tap && d)) {
    snd_menu(SND_CURSOR);
    m->tg.move = d;
    m->tg.move_t = 0;
    /* the slide plays from the places before the move: kept in from */
    m->from = m->sel;
    m->sel = ((m->sel + d) % m->tg.n + m->tg.n) % m->tg.n;
    return;
  }
  if (tap) {
    snd_menu(SND_SELECT);
    m->tg.decided = 1;
    m->t = 0;
    m->deciding = FADE + TG_DECIDE;
  }
}

/* the decided choice: the aero set worn (Refuse to change keeps it) */
static void tg_aero_done(menu_state *m) {
  int o = m->tg.opt[m->sel];
  if (o >= 0 && o != m->aero[m->maker][m->car]) {
    m->aero[m->maker][m->car] = (signed char)o;
    m->garage_changed = 1;
  }
}

/* ---- the bonnet's, wing's and mirror's change (clSeqTgChangePartsBonnet/Wing/MirrorThread),
 * after their GET screen: menu_g_tg_bg_1, the stand menu_g_tg_bg_3 moved and squashed
 * (setPos, setScale: the bonnet's (20, -36), the wing's (20, 130) at half height, the
 * mirror's (20, 130) at 0.6), the layout (menu_layout_tg_tune_4_bonnet; _6_wing for a car with
 * a wing of its own in the aero set it wears (car_to_wing_number, NormalWingGetFromCarID),
 * else _5_wing; menu_layout_tg_tune_2), its arrows (parts_num_to_arrow_anim) and the top bar's
 * message. The choices (make_*_struct): the bonnet's stock and those won, in the order won;
 * the wing's Refuse to change (the one worn), stock (a wing of its own), none and those won;
 * the mirror's stock and aero. 100 frames a place: resting on 100 sel .. + 59 in a loop,
 * sliding 5 frames on from 100 sel + 79 (the wing's + 80) going right, back from the new
 * place's going left (SE 0x19); the arrows loop from 100 sel, the last choice's from the
 * tables 0x9776a70, 0x9776b20, 0x9776af4. It opens on the newest (NEW on it), the places not
 * won yet locked (setImageReplace 0x19dd, the wing's 0x1700), the dots 0x6b4..0x6b7 off. The
 * car is the card's with the choice on (menu_scene3d). Decide SE 0x1a, then the fade out over
 * 10 frames; the arcade's limit (5 s a choice and more): none here. The Hiace's lower stand
 * and car (flags +0x41, +0x4a): the port has no Hiace. A tap on a place steps towards it, on
 * the one chosen or off the bar decides. ---- */
#define TG_SLIDE 5
/* NormalWingGetFromCarID by CARDB_PACKS: bit a, a wing of its own in aero set a (0x9777348) */
static const unsigned char TG_STOCK_WING[CARDB_NPACKS] = {
  0, 0, 0, 15, 15, 15, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
  0, 0, 14, 14, 10, 14, 14, 15, 14, 10, 10, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 14,
  10, 0, 14, 15, 14, 15, 0, 0, /* the hidden cars */
};
/* by layout (the bonnet's, the 6 and 5 place wing's, the mirror's): its places, the slide's
 * first frame past 100 sel, the top bar's message, the lock; by place the last choice's
 * arrows, the icon, the NEW template, the template the lock replaces */
typedef struct {
  int slots, slide, topbar, lock_to;
  short arrow_last[8], icon[8], new_t[8], lock[8];
} tg_layout_def;
static const tg_layout_def TG_PART_LAYOUTS[5] = {
  { 4, 79, 0x1f51, 0x19dd, { 0, 400, 500, 300 }, { 0x1da1, 0x1da3, 0x1da2, 0x1da4 }, { 0, 0x18ab, 0x18ac, 0x18ad },
    { 0, 0x1da3, 0x1da2, 0x1da4 } },
  { 6, 80, 0x1ed2, 0x1700, { 0, 700, 800, 900, 1000, 500 }, { 0x1f4b, 0x135b, 0x1048, 0x135c, 0x135d, 0x135f },
    { 0, 0, 0, 0x18ac, 0x18ad, 0x18ae }, { 0, 0, 0, 0, 0x135d, 0x135f } },
  { 5, 80, 0x1ed2, 0x1700, { 0, 600, 700, 800, 400 }, { 0x1f4b, 0x1048, 0x135c, 0x135d, 0x135f },
    { 0, 0, 0x18ac, 0x18ad, 0x18ae }, { 0, 0, 0, 0x135d, 0x135f } },
  { 2, 79, 0x1f36, 0, { 0, 100 }, { 0x18a9, 0x18aa }, { 0, 0xb83 }, { 0, 0 } },
  /* the neons' (the straight neons'; the spot neons' top bar, pictures and lock below) */
  { 8, 5, 0x1ab3, 0x16f1, { 0, 900, 1000, 1100, 1200, 1300, 1400, 700 },
    { 0x1347, 0x1049, 0x1bc9, 0x1bca, 0x1bcb, 0x1bcc, 0x1bcd, 0x1bce },
    { 0, 0, 0x18ad, 0x18ae, 0x18af, 0x18b0, 0x18b1, 0x18b2 }, { 0, 0, 0x1bc9, 0x1bca, 0x1bcb, 0x1bcc, 0x1bcd, 0x1bce } },
};
/* the spot neons' (make_Neon_struct: wk_s 8): their pictures in the straight neons' places,
 * the lock, the top bar's message */
#define TG_SPOT_ICON 0x1bcf
#define TG_SPOT_LOCK 0x16f3
#define TG_SPOT_TOPBAR 0x1ab4
static int tg_neon_kind(int kind) { return kind == 6 || kind == 8; }

static int tg_part_layout(const menu_state *m) {
  return tg_neon_kind(m->tg.kind) ? 4 : m->tg.kind == 2 ? 0 : m->tg.kind == 4 ? 3 : m->tg.slots == 6 ? 1 : 2;
}

static int tg_part_anim(const menu_state *m, int arrow) {
  switch (tg_part_layout(m)) {
  case 0: return arrow ? ID.tg_bonnet_arrow : ID.tg_bonnet;
  case 1: return arrow ? ID.tg_wing6_arrow : ID.tg_wing6;
  case 2: return arrow ? ID.tg_wing5_arrow : ID.tg_wing5;
  case 4: return arrow ? ID.tg_neon_arrow : ID.tg_neon;
  default: return arrow ? ID.tg_mirror_arrow : ID.tg_mirror;
  }
}

static int tg_part_frame(const menu_state *m) {
  int slide = TG_PART_LAYOUTS[tg_part_layout(m)].slide;
  if (tg_part_layout(m) == 4) {
    /* the neons' (0x9409ec0, 0x9409ee0): held on 10 sel + 1; a move right plays 10 old +
     * 5.. for 5 frames, left the same of the new place backwards */
    if (m->tg.move > 0) return 10 * m->from + slide + m->tg.move_t;
    if (m->tg.move < 0) return 10 * m->sel + slide + TG_SLIDE - 1 - m->tg.move_t;
    return 10 * m->sel + 1;
  }
  if (m->tg.move > 0) return 100 * m->from + slide + m->tg.move_t;
  if (m->tg.move < 0) return 100 * m->sel + slide + TG_SLIDE - 1 - m->tg.move_t;
  return 100 * m->sel + (m->card_t - m->tg.rest_t) % 60;
}

/* the arrows keep looping from the place before a slide until it ends (an_reset) */
static int tg_part_arrow_frame(const menu_state *m) {
  int i = m->tg.move ? m->from : m->sel;
  int base = i == m->tg.n - 1 ? TG_PART_LAYOUTS[tg_part_layout(m)].arrow_last[i] : 100 * i;
  return base + (m->card_t - m->tg.rest_t) % 60;
}

static void tg_part_open(menu_state *m) {
  int mk = m->maker, c = m->car, gem = m->tg.gem, lv = m->dress_lv[mk][c], kind = m->tg.kind;
  go(m, MENU_TG_PART);
  m->tg.n = 0;
  if (kind == 2) {
    m->tg.slots = 4;
    m->tg.opt[m->tg.n++] = 0;
  } else if (kind == 3) {
    int own = TG_STOCK_WING[chosen_car(m, c)->colors[m->color].pack] >> m->aero[mk][c] & 1;
    m->tg.slots = own ? 6 : 5;
    m->tg.opt[m->tg.n++] = m->wing[mk][c];
    if (own) m->tg.opt[m->tg.n++] = 0;
    m->tg.opt[m->tg.n++] = 4;
  } else if (tg_neon_kind(kind)) {
    /* get_parts_id_table_from_dressup_neon: none, the one worn (Refuse to change), then the
     * kind's won so far */
    m->tg.slots = 8;
    m->tg.opt[m->tg.n++] = 0;
    m->tg.opt[m->tg.n++] = m->neon[mk][c];
  } else {
    m->tg.slots = 2;
    m->tg.opt[m->tg.n++] = 0;
    m->tg.opt[m->tg.n++] = 1;
  }
  if (kind != 4)
    for (int l = 1; l <= lv && l < 64; ++l)
      if (DRESS_LADDER[gem][l][0] == kind && m->tg.n < m->tg.slots) m->tg.opt[m->tg.n++] = DRESS_LADDER[gem][l][1];
  m->tg.newest = m->tg.n - 1;
  m->sel = m->tg.newest;
  /* the neons' from the dress-up menu (the arcade's past level 63) open on Refuse to change */
  if (tg_neon_kind(kind) && m->dress_free) m->sel = 1;
  m->from = m->sel;
  m->tg.move = m->tg.move_t = m->tg.decided = m->tg.rest_t = 0;
  m->card_t = 0;
  m->bars = 1;
  m->music = 1;
}

/* the place a tap is on (its icon, as the frame shows it), else -1 */
static int tg_part_hit(const menu_state *m) {
  const tg_layout_def *d = &TG_PART_LAYOUTS[tg_part_layout(m)];
  int lay = tg_part_anim(m, 0), f = tg_part_frame(m), best = -1;
  float best_d = 1e9f;
  /* the nearest place whose picture holds the tap: the neons' pictures have wide clear
   * margins, so their rects overlap the next place's */
  for (int k = 0; k < d->slots; ++k) {
    float r[4];
    if (anm_sprite_rect(lay, f, anm_sprite(d->icon[k]), r) && m->tap_x >= r[0] - 8.0f && m->tap_x <= r[2] + 8.0f &&
        m->tap_y >= r[1] - 8.0f && m->tap_y <= r[3] + 8.0f) {
      float dx = m->tap_x - 0.5f * (r[0] + r[2]), dy = m->tap_y - 0.5f * (r[1] + r[3]);
      if (dx * dx + dy * dy < best_d) best_d = dx * dx + dy * dy, best = k;
    }
  }
  return best;
}

static void tg_part_step(menu_state *m, int tap, int key) {
  m->card_t++;
  if (m->tg.move) {
    if (++m->tg.move_t >= TG_SLIDE) m->tg.move = 0, m->tg.rest_t = m->card_t;
    return;
  }
  if (m->card_t < TG_FADE) return;
  int d = key;
  if (!d && tap) {
    int k = tg_part_hit(m);
    d = k < 0 || k == m->sel ? 0 : k > m->sel ? 1 : -1;
  }
  if ((d > 0 && m->sel < m->tg.n - 1) || (d < 0 && m->sel > 0)) {
    snd_menu(SND_CURSOR);
    m->tg.move = d;
    m->tg.move_t = 0;
    m->from = m->sel;
    m->sel += d;
    return;
  }
  if (tap && !d) {
    snd_menu(SND_SELECT);
    m->tg.decided = 1;
    m->deciding = TG_FADE;
  }
}

/* the part the card keeps (card+6, +7, +0xc) */
static signed char *tg_part_value(menu_state *m) {
  int mk = m->maker, c = m->car;
  if (tg_neon_kind(m->tg.kind)) return &m->neon[mk][c];
  return m->tg.kind == 2 ? &m->bonnet[mk][c] : m->tg.kind == 3 ? &m->wing[mk][c] : &m->mirror[mk][c];
}

/* the decided choice worn */
static void tg_part_done(menu_state *m) {
  int o = m->tg.opt[m->sel];
  signed char *v = tg_part_value(m);
  if (*v != o) {
    *v = (signed char)o;
    m->garage_changed = 1;
  }
}

/* ---- the wheel's change (clSeqTgChangePartsWheelThread: run 0x886d390, yield 0x886c8a8,
 * an_reset 0x886caf8), after its GET screen (mseqTarget: seqTgPartsGet, seqTgChangeParts,
 * seqTgWearParts). The places (get_parts_id_table_from_dressup_wheel): stock, No change (the
 * one worn), then the won one's maker's wheels won so far in the ladder's order. The bar
 * (menu_g_tg_tune_wheel_<n>, n the maker's wheels and 2: wheel_maker_to_number) has a place
 * for each, those not won yet locked (menu_p_t_tuning_wheel_no), NEW on the newest, where it
 * opens; its font dots off. It rests on frame 10 sel; a move right plays 10 old + 4..8, left
 * 10 new + 8..4 (SE 0x19). Rays's 16 places scroll the bar 10 pixels a frame past a window:
 * right, the cursor's place goes up to 8, then the bar scrolls till the 14th place; left,
 * down to 3, then it scrolls from the 3rd. The arrows (menu_layout_tg_tune_wheel_arrow_*)
 * loop 59 frames from 100 times the cursor's place (the last place's from
 * wheel_maker_to_left_arrow_frame). A wheel of more than one colour has the colour column:
 * its swatches (col_col, get_parts_data(0, wheel, colour, gem) + 4 in 0x1e60.., frame 6 -
 * colours), the cursor (col_cursor, 80 frames from 0, 80, 180, .. by colour) and the top
 * bar's message 0x1f0a (else 0x1f6e); the panel (col_waku_2, frame 6 - colours) shows the
 * colour's big picture (+ 0x24; No change's: the worn one's first colour; stock none),
 * col_logo the name logo (+ 0x1c). The cabinet's colour button turns the colour: here a tap
 * on a swatch or the panel. A tap on a place steps towards it, off the bar it decides (SE
 * 0x1a), then the fade out; the arcade's limit (5 s a place and 5): none here. ---- */
#define TG_WHEEL_SLIDE 5
enum { T_WHEEL_STOCK = 0x16ff, T_WHEEL_KEEP = 0x18b4, T_WHEEL_FIRST = 0xbb3, T_WHEEL_SWATCH = 0x1e60, T_WHEEL_DOT = 0x6b4,
       T_WHEEL_LOGO = 0x1ed4, S_WHEEL_LOCK = 0x18b5, S_WHEEL_MES = 0x1f0a, S_WHEEL_MES_ONE = 0x1f6e };
/* by maker: wheel_maker_to_number, wheel_maker_to_left_arrow_frame by the cursor's place */
static const int WHEEL_PLACES[5] = { 10, 5, 16, 8, 11 };
static const short WHEEL_LEFT_ARROW[5][11] = {
  { 0, 1100, 1200, 1300, 1400, 1500, 1600, 1700, 1800, 1900, 0 },
  { 0, 600, 700, 800, 900, 1500, 1600, 1700, 1800, 1900, 0 },
  { 0, 1200, 1300, 1400, 1500, 1600, 1700, 1800, 1900, 2000, 2100 },
  { 0, 900, 1000, 1100, 1200, 1300, 1400, 1500, 1900, 2000, 2100 },
  { 0, 1200, 1300, 1400, 1500, 1600, 1700, 1800, 1900, 2000, 2100 },
};
/* the places' NEW marks (0x9408be0: menu_p_t_tuning_new_tmp3..16), the colour cursor's loops */
static const short WHEEL_NEW_T[16] = { 0, 0, 0x18ad, 0x18ae, 0x18af, 0x18b0, 0x18b1, 0x18b2,
                                       0x18b3, 0x19df, 0x19e0, 0x19e1, 0x19e2, 0x19e3, 0x19e4, 0x19e5 };
static const short WHEEL_COL_FRAME[6] = { 0, 80, 180, 280, 380, 480 };

/* the bar of the maker's wheels (after a GET: the won one's maker; from the dress-up menu:
 * the maker select's, the gem cars' 4) */
static void tg_wheel_open_at(menu_state *m, int maker) {
  int mk = m->maker, c = m->car, gem = m->tg.gem, lv = m->dress_lv[mk][c];
  go(m, MENU_TG_WHEEL);
  m->tg.kind = 0;
  m->tg.maker = maker;
  int places = WHEEL_PLACES[m->tg.maker];
  m->tg.n = 0;
  m->tg.wl[m->tg.n++] = 0;
  m->tg.wl[m->tg.n++] = m->wheel[mk][c];
  for (int l = 1; l <= lv && l < 64 && m->tg.n < places; ++l) {
    const dress_wheel_set *w = DRESS_LADDER[gem][l][0] == 0 ? wheel_set(DRESS_LADDER[gem][l][1] + 1, NULL) : NULL;
    if (w && w->maker == m->tg.maker) m->tg.wl[m->tg.n++] = DRESS_LADDER[gem][l][1] + 1;
  }
  m->tg.newest = m->tg.n - 1;
  m->sel = m->from = m->tg.newest;
  m->tg.col = 0;
  /* the bar's window: the moves right from the first place (run 0x886da1b) */
  m->tg.win = 0;
  m->tg.scroll = 0;
  for (int i = 0; i < m->sel; ++i) {
    if (places <= 11 || m->tg.win <= 7 || i > 12) m->tg.win++;
    else m->tg.scroll -= 50.0f;
  }
  m->tg.scroll_dir = 0;
  m->tg.move = m->tg.move_t = m->tg.decided = 0;
  m->tg.reset_t = 0;
  m->card_t = 0;
  m->bars = 1;
  m->music = 1;
}

static void tg_wheel_open(menu_state *m) {
  const dress_wheel_set *won = wheel_set(m->tg.num + 1, NULL);
  tg_wheel_open_at(m, won ? won->maker : 0);
}

/* the place's colours (WheelModelIDMemberGetFromWheelModelID; stock's 6) */
static int tg_wheel_colours(const menu_state *m, int i) {
  const dress_wheel_set *w = i ? wheel_set(m->tg.wl[i], NULL) : NULL;
  return w ? w->n : 6;
}

static int tg_wheel_column(const menu_state *m) { return m->sel > 1 && tg_wheel_colours(m, m->sel) > 1; }

static int tg_wheel_bar_frame(const menu_state *m) {
  if (m->tg.move > 0) return 10 * m->from + 4 + m->tg.move_t;
  if (m->tg.move < 0) return 10 * m->sel + 8 - m->tg.move_t;
  return 10 * m->sel;
}

static int near_rect(const menu_state *m, const float r[4], float dx, float pad) {
  return m->tap_x >= r[0] + dx - pad && m->tap_x <= r[2] + dx + pad && m->tap_y >= r[1] - pad && m->tap_y <= r[3] + pad;
}

/* the place a tap is on, else -1 */
static int tg_wheel_hit(const menu_state *m) {
  int bar = ID.tg_wheel_bar[m->tg.maker], f = tg_wheel_bar_frame(m);
  for (int i = 0; i < m->tg.n; ++i) {
    float r[4];
    int t = i == 0 ? T_WHEEL_STOCK : i == 1 ? T_WHEEL_KEEP : T_WHEEL_FIRST + i;
    if (anm_sprite_rect(bar, f, anm_sprite(t), r) && near_rect(m, r, m->tg.scroll, 4.0f)) return i;
  }
  return -1;
}

/* the colour a tap is on (its swatch; the panel: the next one), else -1 */
static int tg_wheel_colour_hit(const menu_state *m) {
  if (!tg_wheel_column(m)) return -1;
  int n = tg_wheel_colours(m, m->sel);
  for (int b = 0; b < n; ++b) {
    float r[4];
    if (anm_sprite_rect(ID.tg_wheel_col, 6 - n, anm_sprite(T_WHEEL_SWATCH + b), r) && near_rect(m, r, 0, 3.0f)) return b;
  }
  if (m->tap_x < 176.0f && m->tap_y >= 97.0f && m->tap_y < 300.0f) return (m->tg.col + 1) % n;
  return -1;
}

static void tg_wheel_step(menu_state *m, int tap, int key) {
  m->card_t++;
  if (m->tg.move) {
    m->tg.scroll += 10.0f * (float)m->tg.scroll_dir;
    if (++m->tg.move_t >= TG_WHEEL_SLIDE) m->tg.move = 0, m->tg.scroll_dir = 0;
    return;
  }
  if (m->card_t < TG_FADE || (!tap && !key)) return;
  int d = key, decide = 0;
  if (!d) {
    int col = tg_wheel_colour_hit(m);
    if (col >= 0) {
      if (col != m->tg.col) {
        snd_menu(SND_CURSOR);
        m->tg.col = col;
        m->tg.reset_t = m->card_t;
      }
      return;
    }
    int k = tg_wheel_hit(m);
    if (k >= 0) d = k > m->sel ? 1 : k < m->sel ? -1 : 0, decide = k == m->sel;
    else if (m->tap_y >= 270.0f && m->tap_y < 400.0f) d = m->tap_x < 320.0f ? -1 : 1; /* (the bar's arrows) */
    else decide = 1;
  }
  if ((d > 0 && m->sel < m->tg.n - 1) || (d < 0 && m->sel > 0)) {
    snd_menu(SND_CURSOR);
    m->tg.move = d;
    m->tg.move_t = 0;
    m->from = m->sel;
    m->sel += d;
    m->tg.col = 0;
    if (WHEEL_PLACES[m->tg.maker] <= 11) m->tg.win = m->sel;
    else if (d > 0 && m->tg.win > 7 && m->sel <= 13) m->tg.scroll_dir = -1;
    else if (d < 0 && m->tg.win <= 3 && m->sel > 2) m->tg.scroll_dir = 1;
    else m->tg.win += d;
    m->tg.reset_t = m->card_t;
    return;
  }
  if (decide) {
    snd_menu(SND_SELECT);
    m->tg.decided = 1;
    m->deciding = TG_FADE;
  }
}

static void draw_tg_wheel(const menu_state *m, float fade, int bars, void (*draw3d)(void *), void *ctx) {
  int mk = m->tg.maker, sel = m->sel, n = m->tg.n, cols = tg_wheel_colours(m, sel), t = m->card_t - m->tg.reset_t;
  const dress_wheel_set *w = sel ? wheel_set(m->tg.wl[sel], NULL) : NULL;
  int column = tg_wheel_column(m);
  anm_draw(ID.tg_bg1, 0, fade);
  if (draw3d) draw3d(ctx);
  /* the panel: the colour's big picture */
  int pic = !w ? -1 : sel == 1 ? w->pic[0] : w->pic[m->tg.col];
  anm_swap big = { anm_sprite(T_WHEEL_DOT), pic > 0 ? anm_sprite(pic) : -1 };
  anm_draw_ex(ID.tg_wheel_waku, 6 - cols, fade, 0, 0, &big, 1);
  if (column) {
    anm_draw(ID.tg_wheel_cursor, WHEEL_COL_FRAME[m->tg.col] + t % 80, fade);
    anm_swap sw[6];
    for (int b = 0; b < 6; ++b) sw[b] = (anm_swap){ anm_sprite(T_WHEEL_SWATCH + b), b < cols ? anm_sprite(w->swatch[b]) : -1 };
    anm_draw_ex(ID.tg_wheel_col, 6 - cols, fade, 0, 0, sw, 6);
  }
  if (w) {
    anm_swap logo = { anm_sprite(T_WHEEL_LOGO), anm_sprite(w->logo) };
    anm_draw_ex(ID.tg_wheel_logo, 0, fade, 0, 0, &logo, 1);
  }
  /* the bar: the places' pictures (get_parts_data + 0x10), the locks, NEW on the newest */
  anm_swap sw[11 + 2 * 16];
  int k = 0;
  for (int i = 0; i < 11; ++i) sw[k++] = (anm_swap){ anm_sprite(T_WHEEL_DOT + i), -1 };
  for (int i = 2; i < WHEEL_PLACES[mk]; ++i) {
    const dress_wheel_set *p = i < n ? wheel_set(m->tg.wl[i], NULL) : NULL;
    sw[k++] = (anm_swap){ anm_sprite(T_WHEEL_FIRST + i), i >= n ? anm_sprite(S_WHEEL_LOCK) : p ? anm_sprite(p->bar) : -1 };
    /* (the dress-up menu's, the arcade's once every part is won: no NEW) */
    sw[k++] = (anm_swap){ anm_sprite(WHEEL_NEW_T[i]), i == m->tg.newest && !m->dress_free ? anm_sprite(T_TG_NEW) : -1 };
  }
  anm_draw_ex(ID.tg_wheel_bar[mk], tg_wheel_bar_frame(m), fade, m->tg.scroll, 0, sw, k);
  int win = m->tg.win < 11 ? m->tg.win : 10;
  anm_draw(ID.tg_wheel_arrow[mk], (sel == n - 1 ? WHEEL_LEFT_ARROW[mk][win] : 100 * win) + t % 59, fade);
  anm_swap mes = { anm_sprite(T_TOPBAR_MES), anm_sprite(column ? S_WHEEL_MES : S_WHEEL_MES_ONE) };
  draw_topbar(bars, fade, &mes);
  anm_draw(ID.bottom, bars, fade);
}

/* the decided place worn (wk_s + 4) */
static void tg_wheel_done(menu_state *m) {
  int v = tg_wheel_value(m, m->sel, m->tg.col);
  if (m->wheel[m->maker][m->car] != v) {
    m->wheel[m->maker][m->car] = (unsigned char)v;
    m->garage_changed = 1;
  }
}

/* ---- the stickers' change (clSeqTgChangePartsStickerThread: run 0x8876c1c, initAnim
 * 0x88751a0, make_Sticker_struct 0x88762ac, yield 0x88758a4, check_body 0x8875e24), after
 * their GET screen and from the dress-up menu. The places
 * (get_parts_id_table_from_dressup_sticker): none, then the ladder's stickers in its order
 * (line 1, fire 1, line 2, fire 2, Pac-Man, namco, the works colours, line 3, fire 3,
 * tribal), those not won yet locked (menu_p_tg_tune_vinyl_no), NEW on the newest (its font
 * dots off); the gem cars' none and their works colours. A sticker's number is its place's
 * plus the colour: the lines', fires' and tribal's ten (fire 2's swatches its gradations),
 * the gem cars' works colours' five (swatches 1, 2, 3, 9, 8); the others have no colour
 * (the works colours' by the paint). Up to level 63 it opens on the newest in its first
 * colour, past it (here the dress-up menu) on the one worn. The bar
 * (menu_layout_tg_tune_8_vinyl) rests on 10 sel; a move right plays 10 old + 5..9, left the
 * new place's backwards (SE 0x19); the gem cars' (_2_gem) rest on 100 sel and slide on
 * 79..83. The colour button (SE 0x19) turns the colour, back to the first after the last:
 * here a tap on a swatch picks it, on the column's frame the next. The cursor's place on
 * the bar's window goes up to 5 moving right, then the bar scrolls 70 pixels (14 a frame)
 * till the 8th place; left down to 3, then it scrolls from the 3rd. The arrows
 * (menu_layout_tg_tune_vinyl_arrow_8, the gem cars' _2_arrow) loop 60 frames from 100 times
 * the window's place (the last won place's 0x9776c80). The colour column (_8_vinyl_col)
 * shows on the places with colours, its cursor looping 100 frames from 500 + 100 colour
 * (the gem cars' 100 colour), with the top bar's message 0x1f6d (else 0x1f09). The arcade
 * waits 8 frames after a key before the move: none here. A tap on a place steps towards
 * it, on the one chosen or off the bar decides (SE 0x1a). ---- */
#define TG_VINYL_SLIDE 5
#define TG_VINYL_PLACES 11
enum { S_VINYL_LOCK = 0x1702, T_VINYL_NONE = 0x16e6, T_VINYL_SWATCH = 0x1da5, S_VINYL_GRADATION = 0x1f2b,
       T_VINYL_WORKS = 0x1cc2, T_VINYL_WORKS_GEM = 0x1f06, S_VINYL_MES_COL = 0x1f6d, S_VINYL_MES = 0x1f09 };
/* the places' pictures (0x9409220) and NEW marks (0x9409260) */
static const short VINYL_ICON[TG_VINYL_PLACES] = { T_VINYL_NONE, 0x1bde, 0x1bdb, 0x1bdf, 0x1bdc, 0x18cb, 0x1aaa,
                                                   T_VINYL_WORKS, 0x1be0, 0x1bdd, 0x1daf };
static const short VINYL_NEW_T[TG_VINYL_PLACES] = { 0, 0x18ac, 0x18ad, 0x18ae, 0x18af, 0x18b0, 0x18b1, 0x18b2,
                                                    0x18b3, 0x19df, 0x19e0 };
/* the last won place's arrows by the window's place (0x9776c80) */
static const short VINYL_LAST_ARROW[8] = { 0, 900, 1000, 1100, 1200, 1300, 1400, 700 };

/* the colours of the place whose first sticker is v (make_Sticker_struct's 1 and 2: the
 * lines', fires' and tribal's; the gem cars' works colours) */
static int vinyl_colours(int gem, int v) {
  if (gem) return v == 43 ? 5 : 0;
  return v == 1 || v == 11 || v == 21 || v == 31 || v == 48 || v == 58 || v == 68 ? 10 : 0;
}

static int tg_vinyl_value(const menu_state *m) {
  int v = m->tg.wl[m->sel];
  return v + (vinyl_colours(m->tg.gem, v) ? m->tg.col : 0);
}

/* whether s starts with pre (whole: and ends there); the patched iOS SDK has no plain strcmp
 * or strncmp for armv7 */
static int name_is(const char *s, const char *pre, int whole) {
  while (*pre && *s == *pre) s++, pre++;
  return !*pre && (!whole || !*s);
}

/* the works colours' picture: the car's maker's (get_car_data + 0: menu_p_tg_tune_vinyl_maker_*) */
static int vinyl_works_icon(const menu_state *m) {
  const char *p = CARDB_PACKS[chosen_car(m, m->car)->colors[0].pack];
  if (name_is(p, "nis_R35", 0) || name_is(p, "gem", 0)) return 0x1cc2;
  if (name_is(p, "nis_SELEN", 1)) return 0x1cc8;
  if (name_is(p, "nis_KPGC10", 1)) return 0x1cc9;
  static const char *const MK[5] = { "maz", "mit", "nis", "sub", "toy" };
  for (int k = 0; k < 5; ++k)
    if (name_is(p, MK[k], 0)) return 0x1cc3 + k;
  return 0x1cc2;
}

static void tg_vinyl_open(menu_state *m) {
  int mk = m->maker, c = m->car, gem = tg_gem(m), lv = m->dress_lv[mk][c], v = m->vinyl[mk][c];
  go(m, MENU_TG_VINYL);
  m->tg.kind = 5;
  m->tg.gem = gem;
  m->tg.slots = m->tg.n = 1;
  m->tg.wl[0] = 0;
  for (int l = 1; l < 64 && m->tg.slots < TG_VINYL_PLACES; ++l)
    if (DRESS_LADDER[gem][l][0] == 5) {
      m->tg.wl[m->tg.slots++] = DRESS_LADDER[gem][l][1];
      if (l <= lv) m->tg.n = m->tg.slots;
    }
  m->tg.newest = m->tg.n - 1;
  m->sel = m->tg.newest;
  m->tg.col = 0;
  if (m->dress_free)
    for (int i = 0; i < m->tg.n; ++i) {
      int w = m->tg.wl[i], k = vinyl_colours(gem, w);
      if (v == w || (k && v > w && v < w + k)) m->sel = i, m->tg.col = v - w;
    }
  m->from = m->sel;
  /* the bar's window: the moves right from the first place (run 0x8876f3e) */
  m->tg.win = 0;
  m->tg.scroll = 0;
  for (int i = 0; i < m->sel; ++i) {
    if (gem || m->tg.win <= 4 || i > 7) m->tg.win++;
    else m->tg.scroll -= 70.0f;
  }
  m->tg.scroll_dir = 0;
  m->tg.move = m->tg.move_t = m->tg.decided = 0;
  m->tg.reset_t = 0;
  m->card_t = 0;
  m->bars = 1;
  m->music = 1;
}

static int tg_vinyl_bar(const menu_state *m) { return m->tg.gem ? ID.tg_vinyl_gem : ID.tg_vinyl; }

static int tg_vinyl_bar_frame(const menu_state *m) {
  if (m->tg.gem) return m->tg.move > 0 ? 79 + m->tg.move_t : m->tg.move < 0 ? 83 - m->tg.move_t : 100 * m->sel;
  if (m->tg.move > 0) return 10 * m->from + 5 + m->tg.move_t;
  if (m->tg.move < 0) return 10 * m->sel + 9 - m->tg.move_t;
  return 10 * m->sel;
}

static int tg_vinyl_col_frame(const menu_state *m) {
  return (m->tg.gem ? 0 : 500) + 100 * m->tg.col + (m->card_t - m->tg.reset_t) % 100;
}

/* the place a tap is on, else -1 */
static int tg_vinyl_hit(const menu_state *m) {
  int bar = tg_vinyl_bar(m), f = tg_vinyl_bar_frame(m);
  for (int i = 0; i < m->tg.slots; ++i) {
    float r[4];
    int t = m->tg.gem ? (i ? T_VINYL_WORKS_GEM : T_VINYL_NONE) : VINYL_ICON[i];
    if (anm_sprite_rect(bar, f, anm_sprite(t), r) && near_rect(m, r, m->tg.scroll, 4.0f)) return i;
  }
  return -1;
}

/* the colour a tap is on (its swatch; the column's frame: the next one), else -1 */
static int tg_vinyl_colour_hit(const menu_state *m) {
  int n = vinyl_colours(m->tg.gem, m->tg.wl[m->sel]);
  if (!n) return -1;
  float r[4];
  for (int b = 0; b < n; ++b)
    if (anm_sprite_rect(ID.tg_vinyl_col, tg_vinyl_col_frame(m), anm_sprite(T_VINYL_SWATCH + b), r) && near_rect(m, r, 0, 2.0f))
      return b;
  if (anm_sprite_rect(ID.tg_vinyl_col, tg_vinyl_col_frame(m), anm_sprite(0x1e54), r) && near_rect(m, r, 0, 4.0f))
    return (m->tg.col + 1) % n;
  return -1;
}

static void tg_vinyl_step(menu_state *m, int tap, int key) {
  m->card_t++;
  if (m->tg.move) {
    m->tg.scroll += 14.0f * (float)m->tg.scroll_dir;
    if (++m->tg.move_t >= TG_VINYL_SLIDE) m->tg.move = 0, m->tg.scroll_dir = 0, m->tg.reset_t = m->card_t;
    return;
  }
  if (m->card_t < TG_FADE || (!tap && !key)) return;
  int d = key, decide = 0;
  if (!d) {
    int col = tg_vinyl_colour_hit(m);
    if (col >= 0) {
      if (col != m->tg.col) {
        snd_menu(SND_CURSOR);
        m->tg.col = col;
        m->tg.reset_t = m->card_t;
      }
      return;
    }
    int k = tg_vinyl_hit(m);
    if (k >= 0) d = k > m->sel ? 1 : k < m->sel ? -1 : 0, decide = k == m->sel;
    else decide = 1;
  }
  if ((d > 0 && m->sel < m->tg.n - 1) || (d < 0 && m->sel > 0)) {
    snd_menu(SND_CURSOR);
    m->tg.move = d;
    m->tg.move_t = 0;
    m->from = m->sel;
    m->sel += d;
    m->tg.col = 0;
    /* setScroll: right, the window's place up to 5, then the bar till the 8th place; left,
     * down to 3, then the bar from the 3rd */
    if (m->tg.gem) m->tg.win = m->sel;
    else if (d > 0 && m->tg.win > 4 && m->sel <= 8) m->tg.scroll_dir = -1;
    else if (d < 0 && m->tg.win <= 3 && m->sel > 2) m->tg.scroll_dir = 1;
    else m->tg.win += d;
    return;
  }
  if (decide) {
    snd_menu(SND_SELECT);
    m->tg.decided = 1;
    m->deciding = TG_FADE;
  }
}

static void draw_tg_vinyl(const menu_state *m, float fade, int bars, void (*draw3d)(void *), void *ctx) {
  int sel = m->sel, n = m->tg.n, t = m->card_t - m->tg.reset_t, cols = vinyl_colours(m->tg.gem, m->tg.wl[sel]);
  anm_draw(ID.tg_bg1, 0, fade);
  /* yield: the stand scaled (0.75, 1.55) at (160.5, -184.5) */
  anm_draw_xy(ID.tg_bg3, 0, fade, 160.5f, -184.5f, 0.75f, 1.55f, 0, 0);
  if (draw3d) draw3d(ctx);
  /* the bar: the locks, the works colours' maker, NEW on the newest, the font dots off */
  anm_swap sw[2 * TG_VINYL_PLACES + 2];
  int k = 0;
  sw[k++] = (anm_swap){ anm_sprite(T_TG_DOT), -1 };
  if (m->tg.gem) {
    sw[k++] = (anm_swap){ anm_sprite(T_TG_NEW), m->dress_free ? -1 : anm_sprite(T_TG_NEW) };
  } else {
    for (int i = 1; i < m->tg.slots; ++i) {
      int to = i >= n ? anm_sprite(S_VINYL_LOCK) : VINYL_ICON[i] == T_VINYL_WORKS ? anm_sprite(vinyl_works_icon(m)) : -2;
      if (to != -2) sw[k++] = (anm_swap){ anm_sprite(VINYL_ICON[i]), to };
      sw[k++] = (anm_swap){ anm_sprite(VINYL_NEW_T[i]), i == m->tg.newest && !m->dress_free ? anm_sprite(T_TG_NEW) : -1 };
    }
  }
  anm_draw_ex(tg_vinyl_bar(m), tg_vinyl_bar_frame(m), fade, m->tg.scroll, 0, sw, k);
  /* the colour column (fire 2's gradations; the gem cars' 4th and 5th swatches the 9th and 8th) */
  if (cols) {
    anm_swap cs[10];
    int c = 0;
    if (m->tg.gem) {
      cs[c++] = (anm_swap){ anm_sprite(T_VINYL_SWATCH + 3), anm_sprite(T_VINYL_SWATCH + 8) };
      cs[c++] = (anm_swap){ anm_sprite(T_VINYL_SWATCH + 4), anm_sprite(T_VINYL_SWATCH + 7) };
    } else if (m->tg.wl[sel] == 31) {
      for (int b = 0; b < 10; ++b) cs[c++] = (anm_swap){ anm_sprite(T_VINYL_SWATCH + b), anm_sprite(S_VINYL_GRADATION + b) };
    }
    anm_draw_ex(ID.tg_vinyl_col, tg_vinyl_col_frame(m), fade, 0, 0, cs, c);
  }
  int w = m->tg.win < 8 ? m->tg.win : 7;
  int arrow = m->tg.gem ? 100 * w : sel == n - 1 ? VINYL_LAST_ARROW[w] : 100 * w;
  anm_draw(m->tg.gem ? ID.tg_mirror_arrow : ID.tg_vinyl_arrow, arrow + t % 60, fade);
  anm_swap mes = { anm_sprite(T_TOPBAR_MES), anm_sprite(cols ? S_VINYL_MES_COL : S_VINYL_MES) };
  draw_topbar(bars, fade, &mes);
  anm_draw(ID.bottom, bars, fade);
}

/* the decided sticker worn (wk_s + 4) */
static void tg_vinyl_done(menu_state *m) {
  int v = tg_vinyl_value(m);
  if (m->vinyl[m->maker][m->car] != v) {
    m->vinyl[m->maker][m->car] = (unsigned char)v;
    m->garage_changed = 1;
  }
}

/* the dress-up menu left: back to the car setting it was opened from */
static void tg_free_leave(menu_state *m) {
  m->dress_free = 0;
  car_select_enter(m, MENU_SETTING);
}

static void tg_leave(menu_state *m) {
  go(m, MENU_GAME);
  m->sel = game_slot(m->game);
  m->slide = m->card_t = 0;
  m->bars = 1;
  m->music = 1;
}

/* ---- the dress-up menu (seqTgDressUpPartsSelect, clTgDressUpPartsSelect: ctor 0x8883188,
 * run 0x8883d50, yield 0x8882eb4): the arcade's comes after a ghost battle once the car has
 * won every part; here the car setting's wheel key opens it at any level.
 * menu_g_tg_bg_1, the stand menu_g_tg_bg_3 at (20, 130) at half height, the card's car
 * turned -45 degrees about y at the origin (environment 0, setInterest((0, 7.5, 50), (0,
 * -2.5, 0))), the places' layout (menu_layout_tg_tune_all, the gem cars' _gem_all; its
 * stand template 0x104a off) and the top bar's message 0x1e5a. The places (0x940a280):
 * Refuse to change, then the aero set, wheel, bonnet, wing, mirror, sticker, neon and neon
 * spot (the gem cars': aero set, wheel, sticker, gull wing). 100 frames a place: resting on
 * 100 sel .. + 59 in a loop; a move right plays 100 old + 90..94, left the new place's
 * backwards (SE 0x19), no wrap; decided 100 sel + 60..69 (SE 0x1a), then the fade. It
 * opens on the kind chosen last (wk_s + 0). The arcade goes on from the part's change and
 * wear (or Refuse to change) to the game; here the wear comes back to the menu, and Refuse
 * to change or the back key goes back to the car setting. The gull wing's screen is not
 * ported: its place only sounds SE 0x1b. A tap on a place
 * steps towards it, on the one chosen or off the places decides. ---- */
#define TG_SEL_SLIDE 5
#define TG_SEL_DECIDE 10
static const signed char TG_SEL_KIND[2][9] = { { 9, 1, 0, 2, 3, 4, 5, 6, 8 }, { 9, 1, 0, 5, 7 } };
static const int TG_SEL_PLACES[2] = { 9, 5 };
/* the places' pictures (Refuse to change's, menu_p_t_tune_parts_*) */
static const short TG_SEL_ICON[2][9] = { { 0x16fe, 0x18a0, 0x19da, 0x1a9e, 0x18a3, 0x1a9f, 0x19d9, 0x18a2, 0x1d75 },
                                         { 0x16fe, 0x18a0, 0x19da, 0x19d9, 0x18a1 } };
enum { T_SEL_DODAI = 0x104a, S_SEL_MES = 0x1e5a, S_MAKER_MES = 0x1ccf };

static int tg_sel_ported(int kind) { return kind <= 5 || kind == 9 || tg_neon_kind(kind); }
static void tg_maker_open(menu_state *m);

/* the port's (the arcade's menu comes once every part is won): an aero set, wheel or
 * bonnet with none won and stock on has nothing to change to, so its place only sounds
 * SE 0x1b too */
static int tg_sel_open_ok(const menu_state *m, int kind) {
  int mk = m->maker, c = m->car, lv = m->dress_lv[mk][c];
  if (!tg_sel_ported(kind)) return 0;
  if (kind != 0 && kind != 1 && kind != 2) return 1;
  int on = kind == 0 ? m->wheel[mk][c] : kind == 1 ? m->aero[mk][c] : m->bonnet[mk][c];
  if (on) return 1;
  for (int l = 1; l <= lv && l < 64; ++l)
    if (DRESS_LADDER[m->tg.gem][l][0] == kind) return 1;
  return 0;
}

/* a change decided with the part kept (Refuse to change, no change, the one on): no wear
 * (the arcade's plays it; Allan: the port skips it), back to the dress-up menu, else on to
 * the next part due (a win's) */
static void tg_select_open(menu_state *m);
static int tg_kept(menu_state *m, int old, int now) {
  if (old != now) return 0;
  if (m->dress_free) tg_select_open(m);
  else if (!tg_due(m)) tg_leave(m);
  return 1;
}

static void tg_select_open(menu_state *m) {
  int gem = tg_gem(m);
  go(m, MENU_TG_SELECT);
  m->tg.gem = gem;
  for (int i = 0; i < TG_SEL_PLACES[gem]; ++i)
    if (TG_SEL_KIND[gem][i] == m->dress_kind) m->sel = i;
  m->from = m->sel;
  m->tg.move = m->tg.move_t = m->tg.decided = m->tg.rest_t = 0;
  m->card_t = 0;
  m->bars = 1;
  m->music = 1;
}

static int tg_sel_frame(const menu_state *m) {
  if (m->tg.move > 0) return 100 * m->from + 90 + m->tg.move_t;
  if (m->tg.move < 0) return 100 * m->sel + 94 - m->tg.move_t;
  if (m->tg.decided) {
    int t = TG_SEL_DECIDE + TG_FADE - m->deciding;
    return 100 * m->sel + 60 + (t < TG_SEL_DECIDE ? t : TG_SEL_DECIDE - 1);
  }
  return 100 * m->sel + (m->card_t - m->tg.rest_t) % 60;
}

/* the place a tap is on (the chosen one's picture is big: the others' first), else -1 */
static int tg_sel_hit(const menu_state *m) {
  int gem = m->tg.gem, f = tg_sel_frame(m), hit = -1;
  for (int i = 0; i < TG_SEL_PLACES[gem]; ++i) {
    float r[4];
    if (anm_sprite_rect(ID.tg_all[gem], f, anm_sprite(TG_SEL_ICON[gem][i]), r) && near_rect(m, r, 0, 6.0f) &&
        (hit < 0 || i != m->sel))
      hit = i;
  }
  return hit;
}

/* a move under way (+1, -1) over TG_SEL_SLIDE frames, or a tap or key's step or decision */
static int tg_sel_move(menu_state *m, int places, int tap, int key, int (*hit)(const menu_state *)) {
  m->card_t++;
  if (m->tg.move) {
    if (++m->tg.move_t >= TG_SEL_SLIDE) m->tg.move = 0, m->tg.rest_t = m->card_t;
    return 0;
  }
  if (m->card_t < TG_FADE || (!tap && !key)) return 0;
  int d = key;
  if (!d) {
    int k = hit(m);
    d = k < 0 || k == m->sel ? 0 : k > m->sel ? 1 : -1;
    if (!d) return 1;
  }
  if ((d > 0 && m->sel < places - 1) || (d < 0 && m->sel > 0)) {
    snd_menu(SND_CURSOR);
    m->tg.move = d;
    m->tg.move_t = 0;
    m->from = m->sel;
    m->sel += d;
  }
  return 0;
}

static void tg_select_step(menu_state *m, int tap, int key) {
  if (!tg_sel_move(m, TG_SEL_PLACES[m->tg.gem], tap, key, tg_sel_hit)) return;
  if (!tg_sel_open_ok(m, TG_SEL_KIND[m->tg.gem][m->sel])) {
    snd_menu(SND_CANCEL);
    return;
  }
  snd_menu(SND_SELECT);
  m->tg.decided = 1;
  m->deciding = TG_SEL_DECIDE + TG_FADE;
}

static void draw_tg_select(const menu_state *m, int bars, void (*draw3d)(void *), void *ctx) {
  anm_draw(ID.tg_bg1, 0, 1);
  anm_draw_xy(ID.tg_bg3, 0, 1, 20, 130, 1, 0.5f, 0, 0);
  if (draw3d) draw3d(ctx);
  anm_swap dodai = { anm_sprite(T_SEL_DODAI), -1 };
  anm_draw_ex(ID.tg_all[m->tg.gem], tg_sel_frame(m), 1, 0, 0, &dodai, 1);
  anm_swap mes = { anm_sprite(T_TOPBAR_MES), anm_sprite(S_SEL_MES) };
  draw_topbar(bars, 1, &mes);
  anm_draw(ID.bottom, bars, 1);
}

/* the chosen kind's screen: the wheel maker select (the gem cars' bar at once), the aero
 * set's, the bonnet's, wing's or mirror's change; Refuse to change: the game mode select */
static void tg_select_done(menu_state *m) {
  int kind = TG_SEL_KIND[m->tg.gem][m->sel];
  m->dress_kind = kind;
  m->tg.kind = kind;
  if (kind == 9) {
    tg_free_leave(m);
  } else if (kind == 0) {
    if (m->tg.gem) tg_wheel_open_at(m, 4);
    else tg_maker_open(m);
  } else if (kind == 1) {
    tg_aero_open(m);
  } else if (kind == 5) {
    tg_vinyl_open(m);
  } else {
    tg_part_open(m);
  }
}

/* ---- the wheel maker select (seqTgWheelMakerSelect, clSeqTgWheelMakerThread: stPimpl
 * 0x88a3df4, run 0x8884da4, yield 0x8884b50), the dress-up menu's wheel (not the gem
 * cars'): menu_g_shogo_bg_1 and _2, the maker's wheel picture (menu_layout_wheelmaker_bg)
 * under the places (menu_layout_wheelmaker_sel: Enkei, Rays, Watanabe, Yokohama, the
 * bar's makers 3, 2, 1, 0: 0x940a2c8) and the top bar's message 0x1ccf; no car. 100 frames
 * a place: resting on 100 sel + 1 .. + 59 in a loop; a move right plays the places 100 new
 * - 10.. for 5 frames (the picture 100 new - 9.. for 4), left 100 new + 90.. backwards (the
 * picture + 91..), SE 0x19, no wrap; decided 100 sel + 61..70 (SE 0x1a), 30 frames, then
 * the fade. It opens on the maker last chosen (wk_s + 0x10: at first Enkei's); the back key
 * (the cabinet's cancel, SE 0x1b) goes back to the dress-up menu. ---- */
#define WM_DECIDE 10
#define WM_WAIT 30 /* 0x9771198 */
static const signed char WM_MAKER[4] = { 3, 2, 1, 0 };
static const short WM_ICON[4] = { 0x171a, 0x13c2, 0x1afe, 0x1aff };

static void tg_maker_open(menu_state *m) {
  go(m, MENU_TG_MAKER);
  for (int i = 0; i < 4; ++i)
    if (WM_MAKER[i] == m->tg.maker) m->sel = i;
  m->from = m->sel;
  m->tg.move = m->tg.move_t = m->tg.decided = m->tg.rest_t = 0;
  m->card_t = 0;
  m->bars = 1;
  m->music = 1;
}

static int tg_maker_frame(const menu_state *m, int bg) {
  int t = m->tg.move_t, s = 100 * m->sel;
  if (m->tg.move && t < (bg ? 4 : TG_SEL_SLIDE)) return m->tg.move > 0 ? s - 10 + bg + t : s + 94 - t;
  if (m->tg.move) return s + 1 + t - 4;
  if (m->tg.decided && !bg) {
    int d = WM_DECIDE + WM_WAIT + TG_FADE - m->deciding;
    return s + 61 + (d < WM_DECIDE ? d : WM_DECIDE - 1);
  }
  return s + 1 + (m->card_t - m->tg.rest_t) % 59;
}

static int tg_maker_hit(const menu_state *m) {
  int f = tg_maker_frame(m, 0), hit = -1;
  for (int i = 0; i < 4; ++i) {
    float r[4];
    if (anm_sprite_rect(ID.wmaker_sel, f, anm_sprite(WM_ICON[i]), r) && near_rect(m, r, 0, 6.0f) && (hit < 0 || i != m->sel))
      hit = i;
  }
  return hit;
}

static void tg_maker_step(menu_state *m, int tap, int key) {
  if (!tg_sel_move(m, 4, tap, key, tg_maker_hit)) return;
  snd_menu(SND_SELECT);
  m->tg.decided = 1;
  m->deciding = WM_DECIDE + WM_WAIT + TG_FADE;
}

static void draw_tg_maker(const menu_state *m, int bars) {
  anm_draw(ID.shogo_bg1, 0, 1);
  anm_draw(ID.shogo_bg2, 0, 1);
  anm_draw(ID.wmaker_bg, tg_maker_frame(m, 1), 1);
  anm_draw(ID.wmaker_sel, tg_maker_frame(m, 0), 1);
  anm_swap mes = { anm_sprite(T_TOPBAR_MES), anm_sprite(S_MAKER_MES) };
  draw_topbar(bars, 1, &mes);
  anm_draw(ID.bottom, bars, 1);
}

/* the maker's newest wheel won (where its bar opens), 0 none */
static int tg_maker_newest(const menu_state *m, int maker) {
  int lv = m->dress_lv[m->maker][m->car], gem = tg_gem(m), w = 0;
  for (int l = 1; l <= lv && l < 64; ++l) {
    const dress_wheel_set *s = DRESS_LADDER[gem][l][0] == 0 ? wheel_set(DRESS_LADDER[gem][l][1] + 1, NULL) : NULL;
    if (s && s->maker == maker) w = DRESS_LADDER[gem][l][1] + 1;
  }
  return w;
}

/* ---- the part worn (seqTgWearParts, clSeqTgWearPartsAeroThread: run 0x887b714, yield
 * 0x887bf00), after the change: menu_g_tg_bg_1, the stand menu_g_tg_bg_3 moved by the kind
 * (table 0x9409f00: the aero set's (20, 120), the bonnet's (20, 87), the wing's (20, 268) and
 * the mirror's (20, 248) at half height), the top bar without time,
 * menu_layout_tg_tune_get_title_no_lv and the car (TG_WEAR_POSE). The yield's states: a
 * bonnet's, wing's or mirror's starts at 0 with the one worn before: 60 frames on the white
 * comes up (menu_p_whiteout 0..29, SE 0x41 parts_equip), 45 on it goes (69..98) with the new
 * one under it (SE 0x1e tuning), 60 on (state 3) the banner comes down: the frame
 * menu_layout_tg_tune_install_wheel_logo at (0, 23) (its wheel and font templates off) and
 * the part's name and what was done (get_parts_data + 0, + 0x20: font 4 datrums 4, centred
 * on (320, 140)), but not for the part kept (wk_s +0x42); 40 on, state 5. The aero set's
 * starts at state 2 with both SEs, the car turning. The screen fades in over 10 frames (the
 * aero set's 20); then 8 s (skip_enable_stop: no tap), at state 5 30 frames more and the fade
 * out over 10. The port takes a tap once the banner is down. The level 63 part's COMPLETE
 * (menu_layout_tg_tune_complete, SE 0x48) comes with a sticker or a wheel, which the port
 * does not wear yet. ---- */
#define WEAR_TEXT 101800
#define WEAR_HOLD 480 /* the run's 8 s */
#define WEAR_WHEEL_HOLD 240 /* clSeqTgWearPartsWheelThread::run's 4 s */
#define WEAR_WAIT 30  /* 0x9771198 */
#define WEAR_COMPLETE 180 /* its COMPLETE's hold */
/* tools/wear_texts.txt's rows of the wheel's banner: 装着完了！ (font 4 datrums 11, at the
 * banner's font template), 純正ホイールに交換完了！ (datrums 4, centred on (330, 118)) */
enum { WEAR_WHEEL = 16, WEAR_WHEEL_STOCK = 17, WEAR_NEON = 18, WEAR_STICKER = 31 };
enum { T_WEAR_WHEEL = 0x1ed4, T_WEAR_FONT = 0x49, S_WEAR_WAKU = 0x1f61, S_WHITE = 0x1c6 };

/* tools/wear_texts.txt's row of the part (weartexts.py) */
static int wear_text(int gem, int kind, int num) {
  static const signed char FIRST[5] = { -1, 0, 5, 9, 14 }, N[5] = { 0, 4, 4, 5, 2 };
  /* the neons': 18 none (取り外し完了！), 19.. the straight neons 1..6, the spot neons 7..12 */
  if (kind == 6 || kind == 8) return num >= 0 && num <= 12 ? WEAR_NEON + num : -1;
  /* the stickers': none's 18 (取り外し完了！), 31.. by pattern (get_parts_data(5, n)'s names) */
  if (kind == 5) {
    static const signed char FROM[10] = { 1, 11, 21, 31, 41, 42, 43, 48, 58, 68 };
    if (num == 0) return WEAR_NEON;
    for (int k = 9; k >= 0; --k)
      if (num >= FROM[k]) return num <= 77 ? WEAR_STICKER + k : -1;
    return -1;
  }
  if (kind < 1 || kind > 4 || num < 0 || num >= N[kind]) return -1;
  if (kind == 1 && gem && num == 1) return 4;
  return FIRST[kind] + num;
}

static int tg_wear_in(const menu_state *m) { return m->tg.kind == 1 ? 2 * TG_FADE : TG_FADE; }

static void tg_wear_open(menu_state *m, int old, int chosen) {
  go(m, MENU_TG_WEAR);
  m->tg.old = old;
  m->tg.chosen = chosen;
  m->tg.same = old == chosen;
  m->tg.state = m->tg.kind == 1 ? 2 : 0;
  m->tg.timer = 60;
  m->tg.left = WEAR_HOLD;
  m->tg.ry = TG_WEAR_POSE[tg_wear_k(m->tg.kind)][1];
  m->tg.white_from = -1;
  m->card_t = 0;
  m->bars = 0;
  m->music = 1;
  if (m->tg.kind == 0) {
    /* the wheel's: the yield's states from -5, the jack down, the body's bounce 0.5 */
    m->tg.state = -5;
    m->tg.timer = 30;
    m->tg.left = WEAR_WHEEL_HOLD;
    m->tg.lift = m->tg.body = m->tg.bounce = m->tg.phase = 0;
    m->tg.amp = 0.5f;
    m->tg.complete_t = 0;
  }
  if (m->tg.kind == 1) {
    snd_effect("parts_equip", 1);
    snd_effect("tuning", 1);
  }
}

/* clSeqTgWearPartsWheelThread::yield's states (0x9409f5c): -5, 30 frames then the jack up
 * (SE 0x47), the body to 0.6 over the tyres and then the car 1.2 up on a sine (0.2 a frame
 * to 1.57); -2, 20 frames to the white (0..29, SE 0x22 cutin2); -1, 70 more (SE 0x4a 40 on)
 * and the white goes (69..98) with the new wheel on; 0, 60 frames; 1, the jack down (SE
 * 0x46); 2, the car down 0.1 a frame; 3, the body down 0.1 a frame and its bounce (-sin of
 * a phase 0.1 a frame to 9.4, the size 0.5 by 0.93 a frame), then the banner (SE 0x4b); 4,
 * 40 frames; 5 for the run */
static void tg_wear_wheel_yield(menu_state *m) {
  switch (m->tg.state) {
  case -5:
    if (--m->tg.timer == 0) snd_effect("jack_up", 1);
    if (m->tg.timer < 0) {
      float y = sinf(m->tg.phase) * 1.2f;
      if (m->tg.phase >= 1.57f) {
        m->tg.phase = 0;
        m->tg.state = -2;
        m->tg.timer = 20;
        m->tg.body = m->tg.lift = 0.6f;
      } else {
        m->tg.phase += 0.2f;
        m->tg.body = y >= 0.6f ? 0.6f : y;
        m->tg.lift = y >= 0.6f ? y - 0.6f : 0;
      }
    }
    break;
  case -2:
    if (--m->tg.timer < 0) {
      m->tg.white_from = 0, m->tg.white_at = m->card_t;
      snd_menu(SND_CUTIN2);
      m->tg.state = -1;
      m->tg.timer = 70;
    }
    break;
  case -1:
    if (--m->tg.timer == 30) snd_effect("tuning1", 1);
    if (m->tg.timer < 0) {
      m->tg.white_from = 69, m->tg.white_at = m->card_t;
      m->tg.state = 0;
      m->tg.timer = 60;
    }
    break;
  case 0:
    if (--m->tg.timer < 0) m->tg.state = 1;
    break;
  case 1:
    m->tg.state = 2;
    snd_effect("jack_down", 1);
    break;
  case 2:
    if (m->tg.lift > 0.1f) m->tg.lift -= 0.1f;
    else m->tg.lift = 0, m->tg.state = 3;
    break;
  case 3: {
    if (m->tg.body > 0) m->tg.body = (float)((double)m->tg.body - 0.1);
    float x = m->tg.phase;
    m->tg.bounce = -sinf(x) * m->tg.amp;
    m->tg.phase = x + 0.1f;
    m->tg.amp *= 0.93f;
    if (m->tg.phase > 9.4f) {
      m->tg.state = 4;
      m->tg.timer = 40;
      snd_effect("tuning2", 1);
    }
    break;
  }
  case 4:
    if (--m->tg.timer < 0) m->tg.state = 5;
    break;
  }
}

static void tg_wear_step(menu_state *m, int tap) {
  m->card_t++;
  if (m->tg.kind == 0) {
    tg_wear_wheel_yield(m);
    if (m->card_t < TG_FADE) return;
    if (m->tg.left > 0 && tap && m->tg.state == 5) m->tg.left = 0;
    /* the level 63 part's COMPLETE (wk_s + 0x40: SE 0x48, 180 frames) before the wait */
    if (m->tg.lv == 63 && m->tg.state == 5 && m->tg.left <= 0) {
      if (!m->tg.complete_t) {
        m->tg.complete_t = m->card_t;
        snd_effect("parts_comp", 1);
      }
      if (m->card_t - m->tg.complete_t < WEAR_COMPLETE) return;
    }
    if (m->tg.left > 0 || m->tg.state == 5) m->tg.left--;
    if (m->tg.left <= -(WEAR_WAIT + TG_FADE)) m->deciding = 1;
    return;
  }
  /* yield */
  if (m->tg.kind == 1 && m->tg.state > 0) m->tg.ry -= 0.3f;
  switch (m->tg.state) {
  case 0:
    if (--m->tg.timer < 0) {
      m->tg.white_from = 0, m->tg.white_at = m->card_t;
      snd_effect("parts_equip", 1);
      m->tg.timer = 45;
      m->tg.state = 1;
    }
    break;
  case 1:
    if (--m->tg.timer < 0) {
      m->tg.white_from = 69, m->tg.white_at = m->card_t;
      m->tg.state = 2;
      m->tg.timer = 60;
      snd_effect("tuning", 1);
    }
    break;
  case 2:
    if (--m->tg.timer < 0) m->tg.state = 3;
    break;
  case 3:
    m->tg.state = 4;
    m->tg.timer = 40;
    break;
  case 4:
    if (--m->tg.timer < 0) m->tg.state = 5;
    break;
  }
  /* run: the 8 s once the screen is in, then the wait and the fade out (left below 0) */
  if (m->card_t < tg_wear_in(m)) return;
  if (m->tg.left > 0 && tap && m->tg.state == 5) m->tg.left = 0;
  if (m->tg.left > 0 || m->tg.state == 5) m->tg.left--;
  if (m->tg.left <= -(WEAR_WAIT + TG_FADE)) m->deciding = 1;
}

/* the dress-up screens' top bar without time (clMenuBarStd ctor: loop 0..119, in 190..199,
 * out 160..169; past 170 it is up off the screen): in as the screen opens, then the loop,
 * out over the `out` frames of its leaving (-1 not leaving) */
static int topbar_notime_frame(int t, int out) {
  if (out >= 0) return 160 + (out < 9 ? out : 9);
  return t < 10 ? 190 + t : (t - 10) % 120;
}

/* ---- the class-up screen (seqLvUp 0x8781e10, clSeqLvUpThread), after a race whose card's
 * class rose (mseqStory, mseqTen, mseqTimeAttack: GetClass of the card before the race and
 * after): menu_g_shogo_bg_1 and _2 under menu_layout_lvup, its class before (the small
 * plate, template 0x185e) and after (the large one, 0x185d) from the table at 0x93f09c0
 * (class k: menu_p_lvup_s_(47 - k), menu_p_lvup_l_(47 - k)). It fades in over 10 frames,
 * plays its intro (1..124: levelup, SE 0x34, at frame 101), then loops 201..259; a tap in
 * the intro goes to the loop, one in the loop fades out over 10 frames (the arcade's 10 s
 * countdown is the port's infinity). Then the rest of the way back from the race. ---- */
enum { T_LVUP_S = 0x185e, T_LVUP_L = 0x185d };
#define LVUP_INTRO 124
#define LVUP_LOOP 201
#define LVUP_LOOP_LEN 59
#define LVUP_SE 101

static int lvup_small(int k) { return k < 38 ? 1555 - k : 1261 - k; }
static int lvup_large(int k) { return k < 38 ? 1517 - k : 1252 - k; }

void menu_lvup_check(menu_state *m) {
  menu_own_store(m);
  int mk = m->maker, c = m->car, cls = menu_class(m, mk, c);
  if (cls <= m->class_car[mk][c]) return;
  m->lvup_due = 1;
  m->lvup_from = m->class_car[mk][c];
  m->lvup_to = cls;
  m->class_car[mk][c] = (signed char)cls;
  m->garage_changed = 1;
}

static void lvup_open(menu_state *m) {
  go(m, MENU_LVUP);
  m->lvup_due = 0;
  m->lvup_loop = 0;
  m->card_t = 0;
  m->bars = 0;
  m->music = 1;
  m->tap = m->key = m->key_held = m->back = m->leaving = 0;
}

static void lvup_step(menu_state *m, int tap, int key) {
  if (m->t == LVUP_SE) snd_effect("levelup", 1);
  if (!m->lvup_loop && m->t >= LVUP_INTRO) m->lvup_loop = LVUP_INTRO;
  if (!(tap || key) || m->t < TG_FADE) return;
  if (!m->lvup_loop) m->lvup_loop = m->t;
  else m->deciding = TG_FADE;
}

static void draw_lvup(const menu_state *m) {
  int t = m->t, from = m->lvup_from, to = m->lvup_to;
  int f = !m->lvup_loop ? (t < 1 ? 1 : t) : LVUP_LOOP + (t - m->lvup_loop) % LVUP_LOOP_LEN;
  float fade = m->deciding ? (float)m->deciding / TG_FADE : t < TG_FADE ? (float)(t + 1) / TG_FADE : 1.0f;
  anm_draw(ID.shogo_bg1, 0, 1);
  anm_draw(ID.shogo_bg2, 0, 1);
  anm_swap sw[2] = { { anm_sprite(T_LVUP_S), from >= 0 && from < 47 ? anm_sprite(lvup_small(from)) : -1 },
                     { anm_sprite(T_LVUP_L), to >= 0 && to < 47 ? anm_sprite(lvup_large(to)) : -1 } };
  anm_draw_ex(ID.lvup, f, 1, 0, 0, sw, 2);
  anm_fill(0, 0, 0, 1.0f - fade);
}

/* ---- the versus stars (seqJoStarGet 0x882bdd0, clSeqJoStarGetThread): after a versus whose
 * stars changed (clRaceThread::finiJoin: the table 0x94128e0, or 0x9412940 above 2000 stars,
 * by machines and place), on the way back to the lobby. The screen (ctor 0x882a8b4, yield
 * 0x88285c0): menu_g_vs_star_bg with the emblem of the stars held swapped into 0x18e4, the
 * car turning (clMenuCar about y, 0.3 degrees a frame back, environment 0, eye (0, 11, 50) at
 * (0, 4, 0)), menu_layout_vs_star_logo, _car (the car's logo into 0x1595), _title (the name,
 * the class and title, the power) and _wins (the versus wins in menu_g_st_result_L digits);
 * the stars held (drawHaveStar 0x8827d1c): by 1 (the stars % 100), 100, 1000 and 10000 in
 * menu_use_vs_star's four frames, the biggest first, 20 a row from the tables at 0x93faf60.
 * runAddStar (0x882c490): menu_g_vs_star_<n> (220 frames, SE 0x2c win at each star's
 * landing, frames 20 / 30 / 40), or with a carry into a bigger star keta_a_<n> (130) then
 * keta_b_<the ones left + 1> (90, its 100 star swapped for 1000 / 10000 by the carry); at
 * 130 an emblem changed by the new stars swaps in (SE 0x2d approach3); at 220 the new stars
 * fade in over 30 frames into the stars held, a new big star with its finish_<100..10000>
 * (160, SE 0x38 win_bigstar); then a tap, or frame 600, fades out. runSubStar (0x882df04):
 * minus1 / minus2 (200 frames). ---- */
enum { T_STAR_EMBLEM = 0x18e4, T_STAR_LOGO = 0x1595, T_STAR_L100 = 0xd44, T_STAR_NAME = 0x58b, T_STAR_CLASS = 0x37d,
       T_STAR_TITLE = 0x476, T_STAR_POWER = 0x477, T_STAR_WIN_D1 = 2657, T_STAR_WIN_D2 = 2666, T_STAR_WIN_D3 = 2665,
       S_STAR_DIGIT = 2657 };
#define STAR_FADE 15
#define STAR_ADD_LEN 220
#define STAR_KETA_A 130
#define STAR_EMBLEM_AT 130
#define STAR_IN_LEN 30
#define STAR_FINISH_LEN 160
#define STAR_SUB_LEN 200
#define STAR_MAX 600
static const int STAR_EMBLEMS[10] = { -1, 0x18e5, 0x1706, 0x1ab8, 0x1381, 0x1382, 0x1707, 0x1708, 0x18e6, 0x18e4 };
static const int STAR_TIERS[9] = { 1000, 2000, 4000, 7000, 10000, 15000, 20000, 30000, 45000 };
static const float STAR_XSTEP[4] = { 30, 68, 68, 102 }, STAR_YSTEP[4] = { 32, 60, 60, 104 };
static const float STAR_X0[4] = { 21, 14, 14, 13 }, STAR_Y0[4] = { 72, 82, 82, 68 };
static const int STAR_BIG[4] = { 0, T_STAR_L100, 0x1081, 0x1380 };

static int star_tier(int s) {
  int k = 0;
  while (k < 9 && s >= STAR_TIERS[k]) k++;
  return k;
}

static void star_counts(int s, int n[4]) {
  n[0] = s % 100, n[1] = s / 100 % 10, n[2] = s / 1000 % 10, n[3] = s / 10000 % 10;
}

/* the carry of adding d to s: 0 none, 1 a new 100 star, 2 a 1000, 3 a 10000 (the stars past
 * 0..99 of ones turn into one bigger star; the screen only shows one) */
static int star_carry(int s, int d) {
  if (d <= 0 || s % 100 + d <= 99) return 0;
  int t = s + d;
  if (t / 100 % 10) return 1;
  if (t / 1000 % 10) return 2;
  return t / 10000 % 10 ? 3 : 0;
}

int menu_vs_stars(menu_state *m, int place, int n) {
  static const signed char LOW[3][4] = { { 1, 0 }, { 2, 1, 0 }, { 3, 2, 1, 0 } };
  static const signed char HIGH[3][4] = { { 1, -1 }, { 2, 1, -1 }, { 3, 2, 1, -1 } };
  int mk = m->maker, c = m->car;
  n = n < 2 ? 2 : n > 4 ? 4 : n;
  int r = place - 1 < 0 ? 0 : place - 1 >= n ? n - 1 : place - 1;
  int *s = &m->vs_stars_car[mk][c];
  int d = (*s > 2000 ? HIGH : LOW)[n - 2][r];
  m->star_from = *s;
  *s = *s + d < 0 ? 0 : *s + d > 999999 ? 999999 : *s + d;
  m->vs_count_car[mk][c] = m->vs_count_car[mk][c] < 999999 ? m->vs_count_car[mk][c] + 1 : 999999;
  /* seqJoStarGet: the screen for a change of -2..3 */
  d = *s - m->star_from;
  m->star_delta = d >= -2 && d <= 3 ? d : 0;
  m->garage_changed = 1;
  return d;
}

void menu_vs_star_open(menu_state *m) {
  go(m, MENU_VS_STAR);
  m->star_t = 0;
  m->star_skip = 0;
  m->star_spin = 0;
  m->bars = 0;
  m->music = 1;
  m->tap = m->key = m->key_held = m->back = m->leaving = 0;
}

/* the frames the stars' part takes (from the fade in), till a tap may leave */
static int star_len(const menu_state *m) {
  int d = m->star_delta;
  if (d < 0) return STAR_SUB_LEN;
  return STAR_ADD_LEN + STAR_IN_LEN + (star_carry(m->star_from, d) ? STAR_FINISH_LEN : 0);
}

static void star_to_lobby(menu_state *m);

static void star_step(menu_state *m, int tap, int key) {
  int t = ++m->star_t, f = t - STAR_FADE, d = m->star_delta;
  m->star_spin -= 0.3f;
  if (m->star_skip) {
    if (++m->star_skip > STAR_FADE) star_to_lobby(m);
    return;
  }
  if (d > 0 && f >= 0) {
    int carry = star_carry(m->star_from, d);
    if ((f == 30) || (f == 20 && d >= 2) || (f == 40 && d == 3)) snd_effect("win", 1);
    if (f == STAR_EMBLEM_AT && star_tier(m->star_from + d) != star_tier(m->star_from)) snd_menu(SND_APPROACH3);
    if (carry && f == STAR_ADD_LEN + STAR_IN_LEN) snd_effect("win_bigstar", 1);
  }
  if (f >= STAR_MAX || ((tap || key) && f >= star_len(m))) m->star_skip = 1;
}

/* the centre of star k (0 the first) of kind `kind` with the stars held s; 0 if none */
static int star_slot(int s, int kind, int k, float *x, float *y) {
  int n[4];
  star_counts(s, n);
  float yy = 0;
  for (int j = 3; j >= 0; --j) {
    if (n[j] <= 0) continue;
    if (yy == 0) yy = STAR_Y0[j];
    float xx = STAR_X0[j];
    for (int i = 0; i < n[j]; ++i) {
      if (i && i % 20 == 0) yy += STAR_YSTEP[j], xx = STAR_X0[j];
      if (j == kind && i == k) {
        *x = xx + 0.5f * STAR_XSTEP[j], *y = yy + 0.5f * STAR_YSTEP[j];
        return 1;
      }
      xx += STAR_XSTEP[j];
    }
    yy += STAR_YSTEP[j];
  }
  return 0;
}

/* the stars held s, the last `fresh` of the ones at alpha af, the last of kind `hide` (1..3)
 * left out (its finish draws it) */
static void star_held(int s, int fresh, float af, int hide) {
  int n[4];
  star_counts(s, n);
  for (int j = 3; j >= 0; --j)
    for (int i = 0; i < n[j]; ++i) {
      float x, y, al = j == 0 && i >= n[0] - fresh ? af : 1.0f;
      if (j && j == hide && i == n[j] - 1) continue;
      if (al > 0.0f && star_slot(s, j, i, &x, &y)) anm_draw_ex(ID.star_use, j, al, x, y, NULL, 0);
    }
}

static void draw_vs_star(const menu_state *m, void (*draw3d)(void *), void *ctx) {
  int t = m->star_t, f = t - STAR_FADE, d = m->star_delta, from = m->star_from, to = from + d;
  int carry = star_carry(from, d), mk = m->maker, cr = m->car;
  const cardb_car *c = &CARDB_CARS[mk][cr];
  float a = m->star_skip ? 1.0f - (float)m->star_skip / STAR_FADE : t < STAR_FADE ? (float)t / STAR_FADE : 1.0f;
  /* the emblem by the stars: the new one from frame 130 */
  int now = d > 0 && f >= STAR_EMBLEM_AT ? to : d < 0 && f >= STAR_SUB_LEN ? to : from;
  int em = STAR_EMBLEMS[star_tier(now)];
  anm_swap bg = { anm_sprite(T_STAR_EMBLEM), em >= 0 ? anm_sprite(em) : -1 };
  anm_draw_ex(ID.star_bg, 0, 1, 0, 0, &bg, 1);
  if (draw3d) draw3d(ctx);
  anm_draw(ID.star_logo, 0, 1);
  anm_swap logo = { anm_sprite(T_STAR_LOGO), anm_sprite(c->logo) };
  anm_draw_ex(ID.star_car, 0, 1, 0, 0, &logo, 1);
  /* the title row: the name, the class and title, the power */
  anm_swap tsw[4] = { { anm_sprite(T_STAR_NAME), -1 }, { anm_sprite(T_STAR_CLASS), -1 }, { anm_sprite(T_STAR_TITLE), -1 },
                      { anm_sprite(T_STAR_POWER), -1 } };
  int tf = anm_frames(ID.star_title) - 1;
  anm_draw_ex(ID.star_title, tf < 0 ? 0 : tf, 1, 0, 0, tsw, 4);
  float r[4];
  if (anm_sprite_rect(ID.star_title, 0, anm_sprite(T_STAR_NAME), r) && anm_sprite(NAME_GLYPH5) >= 0)
    for (int i = 0; i < 5 && m->name[i] >= 0; ++i) {
      int s = anm_sprite(NAME_GLYPH5 + m->name[i]);
      float src[4] = { 0, 0, anm_sprite_width(s), anm_sprite_height(s) };
      float dst[4] = { r[0] + 22.0f * (float)i, r[1], r[0] + 22.0f * (float)i + 0.8f * src[2], r[1] + 0.8f * src[3] };
      if (s >= 0) anm_draw_part(s, src, dst, 1);
    }
  if (anm_sprite_rect(ID.star_title, 0, anm_sprite(T_STAR_CLASS), r)) {
    int pre = anm_sprite(CARD_CLASSP + m->class_car[mk][cr]);
    if (pre >= 0) {
      float src[4] = { 0, 0, anm_sprite_width(pre), anm_sprite_height(pre) };
      float dst[4] = { r[0], r[1], r[0] + 0.72f * src[2], r[1] + 0.72f * src[3] };
      anm_draw_part(pre, src, dst, 1);
    }
  }
  if (anm_sprite_rect(ID.star_title, 0, anm_sprite(T_STAR_TITLE), r)) {
    int title = m->title_car[mk][cr] > 0 ? m->title_car[mk][cr] : TITLE_FIRST;
    const unsigned short *g;
    int n = title_glyphs(title, &g);
    float x = r[0];
    for (int i = 0; i < n; ++i) {
      int s = anm_sprite(TITLE_GLYPH1 + g[i]);
      float src[4] = { 0, 0, 20, 20 }, dst[4] = { x, r[1], x + 20, r[1] + 20 };
      if (s >= 0) anm_draw_part(s, src, dst, 1);
      x += title_glyph_half(g[i]) ? 8.0f : 16.0f;
    }
  }
  if (anm_sprite_rect(ID.star_title, 0, anm_sprite(T_STAR_POWER), r)) {
    char text[16];
    int power = m->tune_full[mk][cr] ? m->tune_power[mk][cr] : m->st_power[mk][cr];
    snprintf(text, sizeof text, "%d", car_tune_hp(c->hp, power));
    float x = card_glyphs(CARD_GLYPH2, "-,", 8.0f, text, r[0], r[1], 1);
    anm_draw_sprite(anm_sprite(CARD_TEXT + C_HP), x, r[1], 1);
  }
  /* the wins: frame 1 one digit, 0 two, 2 three */
  int w = m->vs_wins_car[mk][cr] < 999 ? m->vs_wins_car[mk][cr] : 999;
  int nd = w >= 100 ? 3 : w >= 10 ? 2 : 1;
  anm_swap wsw[3];
  if (nd == 1) {
    wsw[0] = (anm_swap){ anm_sprite(T_STAR_WIN_D1), anm_sprite(S_STAR_DIGIT + w) };
  } else if (nd == 2) {
    wsw[0] = (anm_swap){ anm_sprite(T_STAR_WIN_D1), anm_sprite(S_STAR_DIGIT + w / 10) };
    wsw[1] = (anm_swap){ anm_sprite(T_STAR_WIN_D2), anm_sprite(S_STAR_DIGIT + w % 10) };
  } else {
    wsw[0] = (anm_swap){ anm_sprite(T_STAR_WIN_D1), anm_sprite(S_STAR_DIGIT + w / 100) };
    wsw[1] = (anm_swap){ anm_sprite(T_STAR_WIN_D2), anm_sprite(S_STAR_DIGIT + w / 10 % 10) };
    wsw[2] = (anm_swap){ anm_sprite(T_STAR_WIN_D3), anm_sprite(S_STAR_DIGIT + w % 10) };
  }
  anm_draw_ex(ID.star_wins, nd == 1 ? 1 : nd == 2 ? 0 : 2, 1, 0, 0, wsw, nd);
  /* the stars held, and the race's */
  if (d > 0) {
    int done = f >= STAR_ADD_LEN;
    float in = done ? (f - STAR_ADD_LEN >= STAR_IN_LEN ? 1.0f : (float)(f - STAR_ADD_LEN) / STAR_IN_LEN) : 0.0f;
    if (!done) star_held(from, 0, 1, 0);
    else if (!carry) star_held(to, d, in, 0);
    else {
      /* the new big star with its finish (held from its end), the ones left fading in */
      int k = carry, n[4], g = f - STAR_ADD_LEN - STAR_IN_LEN;
      star_counts(to, n);
      star_held(to, to % 100, in, g < STAR_FINISH_LEN ? k : 0);
      float x, y;
      if (g >= 0 && g < STAR_FINISH_LEN && star_slot(to, k, n[k] - 1, &x, &y)) anm_draw_ex(ID.star_finish[k], g, 1, x, y, NULL, 0);
    }
    if (f >= 0 && f < STAR_ADD_LEN) {
      if (!carry) anm_draw(ID.star_add[d - 1], f, 1);
      else if (f < STAR_KETA_A) anm_draw(ID.star_keta_a[d - 1], f, 1);
      else {
        int left = to % 100;
        anm_swap big = { anm_sprite(T_STAR_L100), anm_sprite(STAR_BIG[carry]) };
        anm_draw_ex(ID.star_keta_b[left < 3 ? left : 2], f - STAR_KETA_A, 1, 0, 0, &big, 1);
      }
    }
  } else if (d < 0) {
    star_held(f >= STAR_SUB_LEN ? to : from, 0, 1, 0);
    if (f >= 0 && f < STAR_SUB_LEN) anm_draw(ID.star_minus[-d - 1], f, 1);
  } else star_held(from, 0, 1, 0);
  anm_fill(0, 0, 0, 1.0f - a);
}

static void star_to_lobby(menu_state *m) {
  m->star_delta = m->star_skip = 0;
  go(m, MENU_VS);
  m->bars = 1;
  m->flash = 0;
  m->tap = m->key = m->key_held = m->back = m->leaving = 0;
}

/* ---- the title select (seqTitleSelect 0x8785e08, clSeqTitleSelectThread), after the
 * class-up: the titles judgeTitle found for the race (src/title.h; menu_title_due).
 * stPimpl: menu_g_shogo_bg_1 and _2 (bg_line in place of _2 on REWRITE) and
 * menu_g_shogo_bg_moji, the bars (clMenuBarStd: the top bar's message 0x1e59 "rewrite the
 * title?"), the spin box, the GET part (clMenuGet: menu_g_shogo_shogo_<n> with the titles
 * in font 7 datrums 9 on templates 1450..1452, menu_g_shogo_title looped) and REWRITE's
 * (menu_layout_shogo_rewrite, the title on 0x1bb0). run:
 * - runGet: fades in over 10 frames (1/6 s); its frame f counts to 200: at 60 the frames play
 *   1..139 once, SE 0x30 (rank) at 65 and, with a second and a third title, 90 and 120; at
 *   200 they loop 141..199. A tap before 200 goes to the loop, one after it fades out (the
 *   arcade's 10 s timer is the port's infinity). bg_moji stays on its first frame.
 * - runSelect for each new title: the spin box (clMenuSpinBoxStd on menu_layout_yn_rank, its
 *   templates' backlight 0x18ea, light 0xd4b and frame 0x3b6; YES with the new title on
 *   0x581, NO with the card's on 0x583, font 1 datrums 9), YES first; the bars come in,
 *   bg_moji plays 1..59 then loops 61..659. Decided: the card's 10 frames, 30 more, then the
 *   fade and the bars out. YES makes it the card's title.
 * - runRewrite once one was taken: the layout plays 1..179 once (SE 0x30 at 40), then loops
 *   200..258; a tap before 180 goes to the loop, one after it fades out.
 * Then the rest of the way back from the race. ---- */
enum { T_SHOGO_FONT = 1450, T_RW_FONT = 0x1bb0, T_RW_YES = 6922, T_RW_NO = 6683, T_YN_FONT = 0x580,
       T_YN_BACKLIGHT = 0x1f38, T_YN_LIGHT = 0x1dc0, T_YN_WAKU = 0x1721, T_SHOGO_MES = 0x1e59 };
#define SHOGO_FADE 10
#define SHOGO_IN 60     /* runGet: the frames start */
#define SHOGO_ONCE 139  /* 1..139 */
#define SHOGO_LOOP 200  /* then 141..199 */
#define SHOGO_SE1 65
#define SHOGO_SE2 90
#define SHOGO_SE3 120
#define SHOGO_DECIDED 10 /* the spin box's decided frames, */
#define SHOGO_WAIT 30    /* then 0x9771198's */
#define RW_ONCE 179      /* runRewrite: 1..179, */
#define RW_LOOP 180      /* then 200..258 */
#define RW_SE 40

void menu_title_due(menu_state *m, const int titles[3]) {
  /* (a versus' titles wait for the next race's way back: both race's kept, three at most) */
  for (int i = 0; i < 3; ++i) {
    int id = titles[i], dup = 0;
    for (int j = 0; j < m->title_n; ++j) dup |= m->title_new[j] == id;
    if (id > 0 && !dup && m->title_n < 3) m->title_new[m->title_n++] = id;
  }
}

int menu_title(const menu_state *m) {
  int t = m->title_car[m->maker][m->car];
  return t > 0 ? t : TITLE_FIRST;
}

static void shogo_open(menu_state *m) {
  go(m, MENU_SHOGO);
  m->title_k = m->title_phase = m->title_taken = m->title_f = m->title_dec = m->title_out = 0;
  m->card_t = m->slide = 0;
  m->bars = 0;
  m->music = 1;
  m->tap = m->key = m->key_held = m->back = m->leaving = 0;
}

/* the next phase, out of the fade: the spin box for title k, REWRITE, or the way on */
static void shogo_next(menu_state *m) {
  m->title_out = m->title_f = m->title_dec = 0;
  if (m->title_phase == 1) m->title_k++;
  if (m->title_phase <= 1 && m->title_k < m->title_n) {
    m->title_phase = 1;
    m->sel = 0; /* clArg +4: YES */
    m->slide = m->card_t = 0;
    m->bars = 1;
    return;
  }
  if (m->title_phase <= 1 && m->title_taken) {
    m->title_phase = 2;
    return;
  }
  m->title_n = 0;
  m->deciding = 1;
}

/* whether the tap is on card k (0 YES, 1 NO) of the spin box at rest */
static int shogo_on_card(const menu_state *m, int k) {
  float r[4];
  if (!anm_sprite_rect(ID.yn_rank, 100 * m->sel + m->card_t % 60, anm_sprite(k ? T_RW_NO : T_RW_YES), r)) return 0;
  return m->tap_x >= r[0] && m->tap_x < r[2] && m->tap_y >= r[1] && m->tap_y < r[3];
}

static int shogo_on_arrow(const menu_state *m) {
  float r[4];
  if (!anm_sprite_rect(ID.yn_rank, 100 * m->sel + m->card_t % 60, anm_sprite(T_ARROW), r)) return 0;
  return m->tap_x >= r[0] - 40.0f && m->tap_x < r[2] + 40.0f && m->tap_y >= r[1] - 30.0f && m->tap_y < r[3] + 30.0f;
}

static void shogo_step(menu_state *m, int tap, int key) {
  if (m->title_out) {
    if (++m->title_out > SHOGO_FADE) shogo_next(m);
    return;
  }
  int f = ++m->title_f;
  if (m->title_phase == 0) {
    int n = m->title_n;
    if ((f == SHOGO_SE1) || (f == SHOGO_SE2 && n > 1) || (f == SHOGO_SE3 && n > 2)) snd_effect("rank", 1);
    if (f > SHOGO_LOOP) m->title_f = f = SHOGO_LOOP + 1 + (f - SHOGO_LOOP - 1) % 59;
    if (!(tap || key) || f <= SHOGO_FADE) return;
    if (f < SHOGO_LOOP) m->title_f = SHOGO_LOOP;
    else m->title_out = 1;
    return;
  }
  if (m->title_phase == 2) {
    if (f == RW_SE) snd_effect("rank", 1);
    if (f > RW_LOOP + 58) m->title_f = f = RW_LOOP + 1;
    if (!(tap || key) || f <= SHOGO_FADE) return;
    if (f < RW_LOOP) m->title_f = RW_LOOP;
    else m->title_out = 1;
    return;
  }
  /* the spin box: decided, sliding, or at rest */
  if (m->title_dec) {
    if (++m->title_dec > SHOGO_DECIDED + SHOGO_WAIT) m->title_out = 1;
    return;
  }
  if (m->slide) {
    if (--m->slide == 0) {
      m->sel ^= 1;
      m->card_t = 0;
    }
    return;
  }
  m->card_t++;
  if (f <= SHOGO_FADE) return;
  if (key || (tap && (shogo_on_arrow(m) || shogo_on_card(m, !m->sel)))) {
    snd_menu(SND_CURSOR);
    m->slide = SLIDE;
    return;
  }
  if (tap && shogo_on_card(m, m->sel)) {
    snd_menu(m->sel ? SND_CANCEL : SND_SELECT);
    if (!m->sel) {
      m->title_car[m->maker][m->car] = (short)m->title_new[m->title_k];
      m->title_taken = 1;
      m->garage_changed = 1;
    }
    m->title_dec = 1;
  }
}

/* a title in a font's glyphs where a frame has template tmpl (clSpriteFont datrums 9: the
 * advances' sum W centred on the template's x + full / 2, at its top; scaled with it), at
 * the template's opacity */
static void shogo_text(int anim, int frame, int tmpl, int base, int half, int full, int cell, int id, float alpha) {
  float r[4], a;
  int t = anm_sprite(tmpl);
  float tw = anm_sprite_width(t), th = anm_sprite_height(t);
  if (id <= 0 || t < 0 || tw <= 0.0f || th <= 0.0f) return;
  const unsigned short *g;
  int n = title_glyphs(id, &g);
  float w = 0.0f;
  for (int i = 0; i < n; ++i) w += (float)(title_glyph_half(g[i]) ? half : full);
  /* each of the frame's records of the template (a decided card has a hidden one too) */
  for (int k = 0; anm_sprite_rect_at(anim, frame, t, k, r, &a); ++k) {
    if ((a *= alpha) <= 0.0f) continue;
    float sx = (r[2] - r[0]) / tw, sy = (r[3] - r[1]) / th, x = ((float)full - w) / 2.0f;
    for (int i = 0; i < n; ++i) {
      int s = anm_sprite(base + g[i]);
      float src[4] = { 0, 0, (float)cell, (float)cell };
      float dst[4] = { r[0] + sx * x, r[1], r[0] + sx * (x + (float)cell), r[1] + sy * (float)cell };
      if (s >= 0) anm_draw_part(s, src, dst, a);
      x += (float)(title_glyph_half(g[i]) ? half : full);
    }
  }
}

static void draw_shogo(const menu_state *m, int clock) {
  int f = m->title_f, phase = m->title_phase;
  float fade = m->title_out ? 1.0f - (float)m->title_out / SHOGO_FADE
               : m->deciding ? 0.0f
               : f < SHOGO_FADE ? (float)(f + 1) / SHOGO_FADE : 1.0f;
  anm_draw(ID.shogo_bg1, 0, 1);
  if (phase == 2) anm_draw(ID.line, clock % anm_frames(ID.line), 1);
  else anm_draw(ID.shogo_bg2, clock % anm_frames(ID.shogo_bg2), 1);
  int moji = phase != 1 ? 0 : f < 59 ? 1 + f : 61 + (f - 59) % 599;
  anm_draw(ID.shogo_moji, moji, 1);
  if (phase == 0) {
    int n = m->title_n < 1 ? 1 : m->title_n > 3 ? 3 : m->title_n, anim = ID.shogo_n[n - 1];
    int sf = f < SHOGO_IN ? 0 : f < SHOGO_LOOP ? 1 + (f - SHOGO_IN < SHOGO_ONCE - 1 ? f - SHOGO_IN : SHOGO_ONCE - 1)
                                               : 141 + (f - SHOGO_LOOP) % 59;
    anm_swap off[3] = { { anm_sprite(T_SHOGO_FONT), -1 }, { anm_sprite(T_SHOGO_FONT + 1), -1 },
                        { anm_sprite(T_SHOGO_FONT + 2), -1 } };
    anm_draw_ex(anim, sf, 1, 0, 0, off, 3);
    anm_draw(ID.shogo_title, clock % anm_frames(ID.shogo_title), 1);
    for (int i = 0; i < n; ++i) shogo_text(anim, sf, T_SHOGO_FONT + i, TITLE_GLYPH7, 18, 36, 40, m->title_new[i], 1);
  } else if (phase == 2) {
    int rf = f < RW_LOOP ? (f < 1 ? 1 : f > RW_ONCE ? RW_ONCE : f) : 200 + (f - RW_LOOP) % 59;
    anm_swap off = { anm_sprite(T_RW_FONT), -1 };
    anm_draw_ex(ID.shogo_rw, rf, 1, 0, 0, &off, 1);
    shogo_text(ID.shogo_rw, rf, T_RW_FONT, TITLE_GLYPH7, 18, 36, 40, menu_title(m), 1);
  } else {
    /* the spin box: at rest, sliding, or decided */
    int z = 100 * m->sel, sf = z + m->card_t % 60;
    if (m->slide) sf = z + 90 + SLIDE - m->slide;
    int d = m->title_dec - 1;
    if (m->title_dec) sf = z + 60 + (d < SHOGO_DECIDED ? d : SHOGO_DECIDED - 1);
    anm_swap sw[7] = { { anm_sprite(T_YN_BACKLIGHT), anm_sprite(0x18ea) }, { anm_sprite(T_YN_LIGHT), anm_sprite(0xd4b) },
                       { anm_sprite(T_YN_WAKU), anm_sprite(0x3b6) }, { anm_sprite(T_YN_FONT), -1 },
                       { anm_sprite(T_YN_FONT + 1), -1 }, { anm_sprite(T_YN_FONT + 2), -1 },
                       { anm_sprite(T_YN_FONT + 3), -1 } };
    anm_draw_ex(ID.yn_rank, sf, 1, 0, 0, sw, 7);
    shogo_text(ID.yn_rank, sf, T_YN_FONT + 1, TITLE_GLYPH1, 8, 16, 20, m->title_new[m->title_k], 1);
    shogo_text(ID.yn_rank, sf, T_YN_FONT + 3, TITLE_GLYPH1, 8, 16, 20, menu_title(m), 1);
    if (!m->slide && !m->title_dec) anm_draw(ID.day_ring, z + (m->card_t < 59 ? m->card_t : 59), 1);
    /* the bars: in as the spin box opens, out as it fades */
    int bars = m->title_out ? 160 + (m->title_out < 10 ? m->title_out : 10) : m->bars ? 188 + m->bars : clock % 160;
    anm_swap mes = { anm_sprite(T_TOPBAR_MES), anm_sprite(T_SHOGO_MES) };
    draw_topbar(bars, 1, &mes);
    anm_draw(ID.bottom, bars, 1);
  }
  anm_fill(0, 0, 0, 1.0f - fade);
}

/* ---- the tuning work (seqStTuneEff 0x883f7c0, clSeqStTuneEffThread), after the story's
 * tuning screen: the part going onto the car. Its row (src/steff.h, clArg 0x8846640) by T
 * (power + handling after) and the side tuned: the top bar's title, the part's picture, the
 * install anime and the car's pose, two lines of text; the arcade's overrides by the car
 * (the jump tables at 0x94081e0 by power, 0x940820c by T - 21) after it.
 * - stPimpl: menu_g_shogo_bg_1 and menu_g_bg_line, the bar without time, the top bar
 *   menu_g_topbar_st_install (by power or handling and T <= 9 or over: from 0, 200, 400 or
 *   600; in at +190 over 10 frames, then +0..119 looped, out at +160; T's digits in 0x479
 *   and 0x47a, the title in 0xa81), the install anime (the part's picture in 0x1ef9), the car
 *   (the card's, scaled 0.9, turned and placed by its pose, environment 8, 20 degrees across
 *   from (0, 28.6, -143) at (0, 5, 0)).
 * - run: in over 10 frames; the install anime (90 frames), SE 0x1e tuning, out to black over
 *   20, the car rebuilt (clMenu3DCar::change), 40 frames on the car's pose's second set and
 *   it turns 0.3 degrees a frame, in over 40; the text box (clMenuText: power
 *   menu_layout_st_instal_result 1..420 with the horsepower before (0xa6c..0xa6e) and after
 *   (0xa61..0xa63), handling menu_layout_st_instal_result_handle 1..299; the lines in font 1
 *   centred on the template 0x580, with it) and SE 0x34 levelup 150 frames on (power) or 0x22
 *   cutin2 75 on (handling). Its 10 s: a step that ends a stage (T 20, 30, 34) shows its
 *   complete picture 3 s before the end (menu_layout_st_install_complete, the tuners 14..16's
 *   _complete2, their picture in 0x1d6f / 0x1e21 from 0x93fd5e0; SE 0x36 result_pass, the
 *   text gone, 3 s more). Then 30 frames, the top bar out, out over 10.
 * The port's: a tap after the text box is on shows the complete picture now (when there is
 * one), else ends it (the arcade's runs its 10 s, and with no time limit never shows the
 * picture). The R35's own (step20, stepup) are not ported: it has the others' rows, as on
 * the tuning screen. ---- */
enum { T_EFF_PARTS = 0x1ef9, T_EFF_TENS = 0x479, T_EFF_ONES = 0x47a, T_EFF_TITLE = 0xa81, T_EFF_FONT = 0x580,
       T_EFF_HP0 = 0xa6c, T_EFF_HP1 = 0xa61, T_EFF_FACE = 0x1d6f, T_EFF_FACE2 = 0x1e21 };
#define EFF_IN 10              /* the fade in, then the install anime */
#define EFF_INSTALL 90
#define EFF_DARK (EFF_IN + EFF_INSTALL) /* SE tuning, out to black over 20 */
#define EFF_CHANGE (EFF_DARK + 20 + 40) /* the new car, in over 40 */
#define EFF_MAIN (EFF_CHANGE + 40)      /* the text box on */
#define EFF_TIME 600
#define EFF_COMPLETE 119
#define EFF_QUIT 30

static void eff_open(menu_state *m, int power, int p0) {
  int mk = m->maker, c = m->car, p = m->st_power[mk][c], h = m->st_handling[mk][c], t = p + h;
  const steff_car *f = &STEFF_CAR[mk][c];
  const steff_row *r;
  int chara = 0;
  if (t <= 20) {
    if (!power) r = &(f->ucf10 ? STEFF_HAND_UCF10 : STEFF_HAND)[h - 1 < 0 ? 0 : h - 1];
    else if (f->r2 && p == 1) r = &STEFF_POWER_R2[0];
    else if (f->na && p == 1) r = &STEFF_POWER_NA[0];
    else r = &STEFF_POWER[p - 1 < 0 ? 0 : p - 1 > 9 ? 9 : p - 1];
  } else if (t <= 30) {
    r = &(f->ucf10 ? STEFF_MID_UCF10 : STEFF_MID)[t - 21];
    chara = f->u30;
  } else {
    r = &STEFF_TOP[0];
    chara = f->u33;
  }
  memset(&m->eff, 0, sizeof m->eff);
  m->eff.title = r->title, m->eff.parts = r->parts, m->eff.anim = r->anim, m->eff.pose = r->pose;
  m->eff.text = r->text;
  /* the overrides: by power after when power was tuned, by handling after otherwise */
  int *a = &m->eff.anim, *pose = &m->eff.pose;
  if (power) {
    switch (p) {
    case 1:
      if (f->gem) *pose = 1, *a = f->na ? 0x238 : 0x239;
      else if (f->r2) m->eff.parts = TUNE_PARTS_SWAP, *pose = 3, *a = 0x23b;
      else if (f->hiace) *pose = 1, *a = 0x23c;
      break;
    case 2:
      if (f->gem) m->eff.parts = TUNE_PARTS_ENGINE;
      break;
    case 4: case 10:
      if (f->gem) *pose = 7, *a = 0x23d;
      break;
    case 3: case 5: case 6: case 7: case 8: case 9:
      if (f->m12) *pose = 1, *a = 0x238;
      if (f->m11) *pose = 1, *a = 0x23a;
      if (f->hiace) *a = 0x23c;
      break;
    }
  } else {
    if (h == 4 && f->hilift) *a = 0x237;
    if (h == 7 && f->m0f) *pose = 3, *a = 0x23b;
  }
  /* and by T: 21, 25..27, 29 */
  if (t == 21 || t == 25 || t == 26 || t == 27 || t == 29) {
    if (f->m11) *pose = 1, *a = 0x23a;
    if (f->m12) *pose = 1, *a = 0x238;
  }
  m->eff.power = power;
  m->eff.total = t;
  m->eff.milestone = t == 20 || t == 30 || t == 34;
  m->eff.chara = chara;
  int hp = chosen_car(m, c)->hp;
  m->eff.hp0 = car_tune_hp(hp, p0);
  m->eff.hp1 = car_tune_hp(hp, p);
  m->eff.timer = EFF_TIME;
  go(m, MENU_ST_EFF);
  m->card_t = 0;
  m->bars = 0;
  m->music = 1;
  m->tap = m->key = m->key_held = m->back = m->leaving = 0;
}

static void eff_complete(menu_state *m) {
  m->eff.complete_t = m->t;
  snd_menu(SND_RESULT_PASS);
  m->eff.timer += 180;
}

static void eff_step(menu_state *m, int tap, int key) {
  int t = m->t;
  if (t == EFF_DARK) snd_effect("tuning", 1);
  if (t == EFF_CHANGE) m->eff.after = 1;
  if (m->eff.after) m->eff.angle -= 0.3f;
  if (t == EFF_MAIN) m->eff.main_t = t;
  if (!m->eff.main_t) return;
  int k = t - m->eff.main_t;
  if (m->eff.power && k == 150) snd_effect("levelup", 1);
  if (!m->eff.power && k == 75) snd_menu(SND_CUTIN2);
  if (m->eff.quit) {
    if (--m->eff.quit == 0) m->deciding = TG_FADE;
    return;
  }
  /* the arcade's: the complete picture 3 s before the end, once the text box is in */
  int text_end = m->eff.power ? 420 : 299;
  if (m->eff.milestone && !m->eff.complete_t && m->eff.timer <= 180 && k >= text_end) eff_complete(m);
  if (--m->eff.timer <= 0 && (!m->eff.complete_t || t - m->eff.complete_t >= EFF_COMPLETE)) m->eff.quit = EFF_QUIT;
  if (!(tap || key)) return;
  if (m->eff.milestone && !m->eff.complete_t) eff_complete(m);
  else m->eff.quit = EFF_QUIT;
}

/* the horsepower's digits: the hundreds' none below 100 */
static void eff_hp(anm_swap *sw, int tmpl, int hp) {
  sw[0] = (anm_swap){ anm_sprite(tmpl), hp >= 100 ? anm_sprite(tmpl + hp / 100 % 10) : -1 };
  sw[1] = (anm_swap){ anm_sprite(tmpl + 1), anm_sprite(tmpl + hp / 10 % 10) };
  sw[2] = (anm_swap){ anm_sprite(tmpl + 2), anm_sprite(tmpl + hp % 10) };
}

static void draw_st_eff(const menu_state *m, void (*draw3d)(void *), void *ctx) {
  int t = m->t, T = m->eff.total;
  anm_draw(ID.shogo_bg1, 0, 1);
  anm_draw(ID.line, 0, 1);
  if (draw3d) draw3d(ctx);
  int out = m->deciding ? TG_FADE - m->deciding : -1;
  anm_draw(ID.top_notime, topbar_notime_frame(t, out), 1);
  int base = m->eff.power ? (T <= 9 ? 0 : 200) : (T <= 9 ? 400 : 600);
  int tf = out >= 0 ? base + 160 + (out < 9 ? out : 9) : t < 10 ? base + 190 + t : base + (t - 10) % 120;
  anm_swap top[3] = { { anm_sprite(T_EFF_TENS), anm_sprite(T_EFF_TENS + T / 10 % 10) },
                      { anm_sprite(T_EFF_ONES), anm_sprite(T_EFF_TENS + T % 10) },
                      { anm_sprite(T_EFF_TITLE), m->eff.title > 1 ? anm_sprite(m->eff.title) : -1 } };
  anm_draw_ex(ID.eff_top, tf, 1, 0, 0, top, 3);
  /* the text box and its lines, till the complete picture */
  if (m->eff.main_t && !m->eff.complete_t) {
    int box = m->eff.power ? ID.eff_result : ID.eff_handle, end = m->eff.power ? 420 : 299;
    int f = t - m->eff.main_t + 1;
    if (f > end) f = end;
    anm_swap sw[7];
    int n = 0;
    sw[n++] = (anm_swap){ anm_sprite(T_EFF_FONT), -1 };
    if (m->eff.power) {
      eff_hp(sw + n, T_EFF_HP0, m->eff.hp0);
      eff_hp(sw + n + 3, T_EFF_HP1, m->eff.hp1);
      n += 6;
    }
    anm_draw_ex(box, f, 1, 0, 0, sw, n);
    float r[4], a = anm_sprite_alpha(box, f, anm_sprite(T_EFF_FONT));
    int text = m->eff.text >= 0 ? anm_sprite(STEFF_TEXT + m->eff.text) : -1;
    /* clSpriteFont datrums 1: the lines' widest advance centred on the template (the
     * sprite is a cell wider) */
    if (text >= 0 && a > 0 && anm_sprite_rect(box, f, anm_sprite(T_EFF_FONT), r))
      anm_draw_sprite(text, r[0] - (anm_sprite_width(text) - 20.0f) / 2.0f, r[1], a);
  }
  int ia = m->eff.anim - STEFF_ANIM;
  if (ia >= 0 && ia < 13) {
    anm_swap part = { anm_sprite(T_EFF_PARTS), m->eff.parts > 1 ? anm_sprite(m->eff.parts) : -1 };
    int f = t < EFF_IN ? 0 : t - EFF_IN < EFF_INSTALL ? t - EFF_IN : EFF_INSTALL - 1;
    anm_draw_ex(ID.eff_install[ia], f, 1, 0, 0, &part, 1);
  }
  if (m->eff.complete_t) {
    int c = m->eff.chara, two = c >= 14 && c <= 16, f = t - m->eff.complete_t + 1;
    anm_swap face = { anm_sprite(two ? T_EFF_FACE2 : T_EFF_FACE),
                      two || T > 20 ? anm_sprite(STEFF_COMPLETE[c]) : anm_sprite(T_EFF_FACE) };
    anm_draw_ex(two ? ID.eff_complete2 : ID.eff_complete, f < EFF_COMPLETE ? f : EFF_COMPLETE, 1, 0, 0, &face, 1);
  }
  /* clMenuFade: in over 10, out to black over 20 after the install, in over 40, out over 10 */
  float black = t < EFF_IN ? 1.0f - (float)(t + 1) / EFF_IN
                : t >= EFF_DARK && t < EFF_DARK + 20 ? (float)(t - EFF_DARK + 1) / 20.0f
                : t >= EFF_DARK + 20 && t < EFF_CHANGE ? 1.0f
                : t >= EFF_CHANGE && t < EFF_MAIN ? 1.0f - (float)(t - EFF_CHANGE + 1) / 40.0f : 0.0f;
  if (m->deciding) black = 1.0f - (float)m->deciding / TG_FADE;
  anm_fill(0, 0, 0, black);
}

static void draw_tg_wear(const menu_state *m, int clock, void (*draw3d)(void *), void *ctx) {
  /* the stand by the part (the wheel's: clSeqTgWearPartsWheelThread::yield setPosition) */
  static const float STAND[5][3] = { { 20, 118, 1 }, { 20, 120, 1 }, { 20, 87, 1 }, { 20, 268, 0.5f }, { 20, 248, 0.5f } };
  int k = m->tg.kind >= 0 && m->tg.kind <= 4 ? m->tg.kind : 1;
  const dress_wheel_set *w = k == 0 ? wheel_set(m->tg.chosen, NULL) : NULL;
  int text = k == 0 ? (w ? WEAR_WHEEL : WEAR_WHEEL_STOCK) : wear_text(m->tg.gem, m->tg.kind, m->tg.chosen);
  int name = text < 0 ? -1 : anm_sprite(WEAR_TEXT + text);
  /* the banner's pictures up before it comes down, one a frame */
  if (m->card_t < 3) anm_warm_sprite(m->card_t == 0 ? name : anm_sprite(m->card_t == 1 ? S_WEAR_WAKU : S_WHITE), 1);
  anm_draw(ID.tg_bg1, 0, 1);
  anm_draw_xy(ID.tg_bg3, 0, 1, STAND[k][0], STAND[k][1], 1, STAND[k][2], 0, 0);
  if (draw3d) draw3d(ctx);
  int out = -m->tg.left - WEAR_WAIT;
  anm_draw(ID.top_notime, topbar_notime_frame(m->card_t, out >= 0 ? out : -1), 1);
  anm_draw(ID.tg_wear_title, 0, 1);
  if (k == 0) {
    /* the wheel's banner from state 4 (setPos(0, 0); up off the screen before): the name
     * logo (+ 0x1c) and 装着完了！ at the font template, the stock's text alone centred */
    if (m->tg.state >= 4 && !m->tg.same) {
      int f = clock % anm_frames(ID.tg_wear_logo);
      anm_swap sw[2] = { { anm_sprite(T_WEAR_WHEEL), w ? anm_sprite(w->logo) : -1 }, { anm_sprite(T_WEAR_FONT), -1 } };
      anm_draw_ex(ID.tg_wear_logo, f, 1, 0, 0, sw, 2);
      float r[4];
      if (name >= 0 && w && anm_sprite_rect(ID.tg_wear_logo, f, anm_sprite(T_WEAR_FONT), r))
        anm_draw_sprite(name, r[0], r[1], 1);
      else if (name >= 0 && !w)
        anm_draw_sprite(name, 330.0f - anm_sprite_width(name) / 2.0f, 118.0f - 14.0f - 2.0f, 1);
    }
    if (m->tg.complete_t) {
      /* the COMPLETE over the second fade (to 0.5 at 1/6 a frame) */
      int t = m->card_t - m->tg.complete_t;
      anm_fill(0, 0, 0, t < 3 ? (float)(t + 1) / 6.0f : 0.5f);
      anm_draw(ID.tg_complete, t < anm_frames(ID.tg_complete) ? t : anm_frames(ID.tg_complete) - 1, 1);
    }
    return;
  }
  if (m->tg.state >= 3 && !m->tg.same) {
    anm_swap off[2] = { { anm_sprite(T_WEAR_WHEEL), -1 }, { anm_sprite(T_WEAR_FONT), -1 } };
    anm_draw_ex(ID.tg_wear_logo, clock % anm_frames(ID.tg_wear_logo), 1, 0, 23.0f, off, 2);
    /* clSpriteFont::draw datrums 4: the advances' sum W and the line's 28 centred on the
     * place, each 32-pixel cell 2 up and left of its advance (the sprite is W + 4 wide) */
    if (name >= 0) anm_draw_sprite(name, 320.0f - anm_sprite_width(name) / 2.0f, 140.0f - 14.0f - 2.0f, 1);
  }
}

/* the GET screens' stand (tg_bg_3) moved by the part (clSeqTgPartsGetWheelThread::yield,
 * clSeqTgPartsGetWingThread::yield setPosition): the wheel's (20, 111), the bonnet's (20, -12),
 * the wing's (20, 15), the mirror's (20, -6); the car's where it is */
static void tg_get_stand(const menu_state *m, float *dx, float *dy) {
  static const float STAND[5][2] = { { 20, 111 }, { 0, 0 }, { 20, -12 }, { 20, 15 }, { 20, -6 } };
  int k = m->screen == MENU_TG_GET && m->tg.kind >= 0 && m->tg.kind <= 4 ? m->tg.kind : 1;
  *dx = STAND[k][0], *dy = STAND[k][1];
}

static void draw_tg_bg(const menu_state *m, float fade) {
  /* the neon parts' screens have the black stripe (setImageReplace(0x5af, 0x111f)) */
  int black = m->screen == MENU_TG_GET && (m->tg.kind == 6 || m->tg.kind == 8);
  anm_swap sw = { anm_sprite(T_TG_BG), anm_sprite(S_TG_BG_BLACK) };
  anm_draw_ex(ID.tg_bg1, 0, fade, 0, 0, &sw, black);
  float dx, dy;
  tg_get_stand(m, &dx, &dy);
  anm_draw_ex(ID.tg_bg3, 0, fade, dx, dy, 0, 0);
}

static void draw_tg_get(const menu_state *m, int clock, float fade, void (*draw3d)(void *), void *ctx) {
  draw_tg_bg(m, fade);
  if (draw3d) draw3d(ctx);
  (void)clock;
  anm_draw(ID.top_notime, topbar_notime_frame(m->card_t, m->deciding ? TG_FADE - m->deciding : -1), fade);
  int lv = m->tg.lv;
  anm_swap title[3] = {
    { anm_sprite(T_TG_TENS), lv >= 10 ? anm_sprite(DRESS_DIGIT[lv / 10 % 10]) : -1 },
    { anm_sprite(T_TG_ONES), anm_sprite(DRESS_DIGIT[lv % 10]) },
    { anm_sprite(T_TG_KIND), m->tg.kind >= 0 && m->tg.kind < 9 ? anm_sprite(DRESS_KIND_TITLE[m->tg.kind]) : -1 },
  };
  anm_draw_ex(ID.tg_get_title, 0, fade, 0, 0, title, 3);
  int type = tg_get_type(m);
  if (type == TG_GET_WHEEL) {
    /* get_wheel from 101 on, its maker's logo in 0x1e5f (the wheel turning behind it) */
    anm_swap name = { anm_sprite(T_TG_WHEEL_NAME), anm_sprite(m->tg.name) };
    int f = TG_WHEEL_FROM + m->card_t;
    anm_draw_ex(ID.tg_get_wheel, f < anm_frames(ID.tg_get_wheel) ? f : anm_frames(ID.tg_get_wheel) - 1, fade, 0, 0, &name, 1);
    return;
  }
  if (type == TG_GET_PICTURE && m->card_t >= TG_FADE) {
    /* the part's picture: get_parts_image 80..99 from the end of the fade in, then held,
     * moved by the part (the bonnet's (-10, 10), the wing's (0, 5) or the per-car wing's
     * (50, 16), the mirror's (20, 10)), under the name */
    int t = m->card_t - TG_FADE;
    float dx = m->tg.kind == 2 ? -10.0f : m->tg.kind == 4 ? 20.0f : m->tg.num == 1 ? 50.0f : 0.0f;
    float dy = m->tg.kind == 3 ? (m->tg.num == 1 ? 16.0f : 5.0f) : 10.0f;
    anm_swap img = { anm_sprite(T_TG_IMAGE), anm_sprite(m->tg.image) };
    anm_draw_ex(ID.tg_get_image, TG_IMAGE_FROM + (t < TG_IMAGE_LEN ? t : TG_IMAGE_LEN - 1), fade, dx, dy, &img, 1);
  }
  anm_swap part = { anm_sprite(T_TG_PART), anm_sprite(m->tg.name) };
  anm_draw_ex(ID.tg_get_parts, m->card_t % anm_frames(ID.tg_get_parts), fade, 0, 0, &part, 1);
}

static void draw_tg_part(const menu_state *m, int clock, float fade, int bars, void (*draw3d)(void *), void *ctx) {
  /* the stand's place and scale across, down; the neons' (setScale(0.95, 0.75), setPos(16,
   * 60)) on the black stripe (setImageReplace(0x5af, 0x111f)) */
  static const float STAND[5][4] = { { 20, -36, 1, 1 }, { 20, 130, 1, 0.5f }, { 20, 130, 1, 0.5f }, { 20, 130, 1, 0.6f },
                                     { 16, 60, 0.95f, 0.75f } };
  int l = tg_part_layout(m), spot = m->tg.kind == 8;
  const tg_layout_def *d = &TG_PART_LAYOUTS[l];
  anm_swap black = { anm_sprite(T_TG_BG), anm_sprite(S_TG_BG_BLACK) };
  anm_draw_ex(ID.tg_bg1, 0, fade, 0, 0, &black, l == 4);
  anm_draw_xy(ID.tg_bg3, 0, fade, STAND[l][0], STAND[l][1], STAND[l][2], STAND[l][3], 0, 0);
  if (draw3d) draw3d(ctx);
  anm_swap sw[32];
  int n = 0;
  for (int k = 0; k < 4; ++k) sw[n++] = (anm_swap){ anm_sprite(T_TG_DOT + k), -1 };
  for (int k = 0; k < d->slots; ++k) {
    if (d->new_t[k]) sw[n++] = (anm_swap){ anm_sprite(d->new_t[k]), k == m->tg.newest && !m->dress_free ? anm_sprite(T_TG_NEW) : -1 };
    if (d->lock[k] && k >= m->tg.n) sw[n++] = (anm_swap){ anm_sprite(d->lock[k]), anm_sprite(spot ? TG_SPOT_LOCK : d->lock_to) };
    else if (l == 4 && spot && k >= 2) sw[n++] = (anm_swap){ anm_sprite(d->icon[k]), anm_sprite(TG_SPOT_ICON + k - 2) };
  }
  /* the neons' bar white (setImageReplace(0xb82, 0x19dc)) */
  if (l == 4) sw[n++] = (anm_swap){ anm_sprite(0xb82), anm_sprite(0x19dc) };
  anm_draw_ex(tg_part_anim(m, 0), tg_part_frame(m), fade, 0, 0, sw, n);
  anm_draw(tg_part_anim(m, 1), tg_part_arrow_frame(m), fade);
  (void)clock;
  anm_swap mes = { anm_sprite(T_TOPBAR_MES), anm_sprite(l == 4 && spot ? TG_SPOT_TOPBAR : d->topbar) };
  draw_topbar(bars, fade, &mes);
  anm_draw(ID.bottom, bars, fade);
}

static void draw_tg_aero(const menu_state *m, int clock, float fade, int bars, void (*draw3d)(void *), void *ctx) {
  draw_tg_bg(m, fade);
  if (draw3d) draw3d(ctx);
  int lay = tg_layout(m);
  for (int i = 0; i < m->tg.n; ++i) {
    int o = m->tg.opt[i];
    int name = o < 0 ? S_TG_REFUSE : o == 0 ? S_TG_STOCK : m->tg.gem ? S_TG_AERO_GEM : T_TG_NAME + o - 1;
    anm_swap sw[5] = {
      { anm_sprite(T_TG_PLATE), anm_sprite(S_TG_PLATE) },
      { anm_sprite(T_TG_NAME), anm_sprite(name) },
      { anm_sprite(T_TG_NAME2), anm_sprite(name) },
      { anm_sprite(T_TG_NEW), i == m->tg.newest ? anm_sprite(T_TG_NEW) : -1 },
      { anm_sprite(T_TG_DOT), -1 },
    };
    anm_draw_ex(lay, tg_frame(m, i), fade, 0, 0, sw, 5);
  }
  anm_swap mes = { anm_sprite(T_TOPBAR_MES), anm_sprite(S_TG_TOPBAR) };
  draw_topbar(bars, fade, &mes);
  anm_draw(ID.bottom, bars, fade);
}

/* ---- the versus (MENU_VS, the port's own screen, no arcade sprites but its top bar's title:
 * hud.c's panels and 5x7 text), for the kind the versus select chose (MENU_VS_SEL). Left over
 * Bluetooth or the LAN the games found nearby (a tap joins) and HOST A GAME; over a server its
 * address with a keypad and CONNECT. Right the lobby's players, then the host's start ramp
 * (the arcade's join ramps, clRaceThread::initJoin's table) and distance, and START. Taps in
 * the menus' points (1024 x 768). */
static const char *const VS_RAMPS[MENU_VS_RAMPS] = {
  "C1 INNER - KANDABASHI", "C1 OUTER - KANDABASHI", "C1 INNER - SHIODOME", "C1 OUTER - SHIBAKOEN",
  "NEW BELT IN - ARIAKE", "NEW BELT OUT - KIBA", "WANGAN OUT - 13GOCHI", "WANGAN IN - DAIKOKU",
  "WANGAN OUT - AIRPORT", "WANGAN IN - AIRPORT", "YOKOHANE DOWN - SHIBAURA", "YOKOHANE UP - SHIOIRI",
  "YOKOHANE UP - HANEDA", "YOKOHANE DOWN - HANEDA", "OSAKA - TONBORI", "HAKONE OUTWARD",
  "HAKONE HOMEWARD", "NAGOYA - MARUNOUCHI", "FUKUOKA - MEINOHAMA", "FUKUOKA - NISHIKOEN",
  "FUKUOKA - HAKATA EKI", "FUKUOKA - HANMICHIBASHI",
};

const char *menu_vs_ramp_name(int ramp) { return ramp >= 0 && ramp < MENU_VS_RAMPS ? VS_RAMPS[ramp] : ""; }

/* the kana as romaji (hiragana from U+3041; katakana are 0x60 on) */
static const char *const KANA[83] = {
  "A", "A", "I", "I", "U", "U", "E", "E", "O", "O", "KA", "GA", "KI", "GI", "KU", "GU", "KE", "GE", "KO", "GO",
  "SA", "ZA", "SHI", "JI", "SU", "ZU", "SE", "ZE", "SO", "ZO", "TA", "DA", "CHI", "DI", "", "TSU", "DU", "TE", "DE",
  "TO", "DO", "NA", "NI", "NU", "NE", "NO", "HA", "BA", "PA", "HI", "BI", "PI", "FU", "BU", "PU", "HE", "BE", "PE",
  "HO", "BO", "PO", "MA", "MI", "MU", "ME", "MO", "YA", "YA", "YU", "YU", "YO", "YO", "RA", "RI", "RU", "RE", "RO",
  "WA", "WA", "WI", "WE", "WO", "N",
};

void menu_name_glyphs(short g[5], const char *ascii) {
  char s[8] = { 0 };
  int n = 0;
  /* vs_name's PLAYER n (a save from before the names): nP, as the arcade's sides */
  if (ascii[0] == 'P' && ascii[1] == 'L' && ascii[2] == 'A' && ascii[3] == 'Y' && ascii[4] == 'E' && ascii[5] == 'R' &&
      ascii[6] == ' ' && ascii[7] >= '1' && ascii[7] <= '9')
    s[0] = ascii[7], s[1] = 'P';
  else
    for (int i = 0; ascii[i] && n < 5; ++i)
      if (ascii[i] != ' ') s[n++] = ascii[i];
  n = 0;
  for (int i = 0; s[i] && n < 5; ++i) {
    char c = s[i] >= 'a' && s[i] <= 'z' ? (char)(s[i] - 32) : s[i];
    for (int k = 0; k < NAMEDB_N; ++k)
      if (NAMEDB_WCHAR[k] == (unsigned)(unsigned char)c + 0xfee0) {
        g[n++] = (short)k;
        break;
      }
  }
  for (int i = n; i < 5; ++i) g[i] = -1;
}

void menu_name_ascii(const menu_state *m, char *out, int n) {
  int k = 0;
  for (int i = 0; i < 5 && m->name[i] >= 0 && m->name[i] < NAMEDB_N; ++i) {
    unsigned c = NAMEDB_WCHAR[m->name[i]];
    char one[2] = { 0, 0 };
    const char *s = one;
    if (c >= 0xff01 && c <= 0xff5e) one[0] = (char)(c - 0xfee0);
    else if (c == 0x3000) one[0] = ' ';
    else if (c == 0x30fc) one[0] = '-';
    else if (c == 0x30f4) s = "VU";
    else if (c >= 0x30a1 && c <= 0x30f3) s = KANA[c - 0x30a1];
    else if (c >= 0x3041 && c <= 0x3093) s = KANA[c - 0x3041];
    else one[0] = '*';
    for (; *s && k < n - 1; ++s) out[k++] = *s >= 'a' && *s <= 'z' ? (char)(*s - 32) : *s;
  }
  if (!k && n > 6) memcpy(out, "PLAYER", 6), k = 6;
  out[k < n ? k : n - 1] = 0;
}

/* menupack.py's VS_TEXT_BASE (the top bar's VERSUS, then BLUETOOTH, LAN and ONLINE VERSUS),
 * VS_ICON_BASE and VS_LINE_BASE (the versus select's discs, tools/vsdiscs.py) */
#define VS_TEXT 107000
#define VS_ICON 107010
#define VS_LINE 107020
enum { VS_KEYS = 12, VS_CONNECT = VS_KEYS, VS_RAMP_L, VS_RAMP_R, VS_KM_L, VS_KM_R, VS_START, VS_BUTTONS };
#define VS_LEFT 60.0f
#define VS_RIGHT 520.0f
#define VS_RIGHT_W 460.0f
#define VS_ROW_Y 165.0f
#define VS_ROW_H 54.0f

/* a button's place: under VS_KEYS the server's keypad, or the games found nearby's rows */
static void vs_button(const menu_state *m, int b, float r[4]) {
  if (b < VS_KEYS && m->vs.kind != MENU_VS_NET) {
    r[0] = VS_LEFT, r[1] = 186.0f + (float)b * 64.0f, r[2] = 410.0f, r[3] = 56.0f;
    return;
  }
  if (b < VS_KEYS) {
    r[0] = VS_LEFT + (float)(b % 3) * 140.0f, r[1] = 250.0f + (float)(b / 3) * 64.0f, r[2] = 130.0f, r[3] = 56.0f;
    return;
  }
  static const float R[VS_BUTTONS - VS_KEYS][4] = {
    { VS_LEFT, 512, 410, 56 },        /* CONNECT */
    { VS_RIGHT, 436, 64, 52 }, { VS_RIGHT + VS_RIGHT_W - 64, 436, 64, 52 }, /* the ramp */
    { VS_RIGHT, 500, 64, 52 }, { VS_RIGHT + VS_RIGHT_W - 64, 500, 64, 52 }, /* the distance */
    { VS_RIGHT, 568, VS_RIGHT_W, 58 }, /* START */
  };
  memcpy(r, R[b - VS_KEYS], sizeof R[0]);
}

/* a lobby row's name in the arcade's glyphs (from a build that sends them, a menu.bin with them) */
#define VS_GLYPH_W 28.0f /* font 5's advance (the card's) */
#define VS_GLYPH_H 30.0f
static int vs_glyphs(const menu_state *m, int i) { return m->vs.glyph[i][0] >= 0 && anm_sprite(NAME_GLYPH5) >= 0; }

static int vs_host(const menu_state *m) { return m->vs.state == MENU_VS_LOBBY && m->vs.me == m->vs.host; }

/* which of the screen's buttons is at (x, y) and on now: its number, -1 none */
static int vs_hit(const menu_state *m, float x, float y) {
  for (int b = 0; b < VS_BUTTONS; ++b) {
    if (b < VS_KEYS && m->vs.state != MENU_VS_OFF) continue;
    if (b < VS_KEYS && m->vs.kind != MENU_VS_NET && b >= m->vs.n_found) continue;
    if (b > VS_CONNECT && !vs_host(m)) continue;
    if (b == VS_RAMP_L || b == VS_RAMP_R) continue; /* the area is voted (MENU_VS_AREA) */
    float r[4];
    vs_button(m, b, r);
    if (x >= r[0] - 6 && x < r[0] + r[2] + 6 && y >= r[1] - 4 && y < r[1] + r[3] + 4) return b;
  }
  return -1;
}

static void vs_step(menu_state *m, int back) {
  int tap = m->vs.tap;
  m->vs.tap = 0;
  if (m->vs.state >= MENU_VS_LOADING) return; /* the race is on its way */
  if (back) {
    snd_menu(SND_CANCEL);
    m->vs.connect = -1;
    go(m, MENU_VS_SEL);
    return;
  }
  if (!tap) return;
  int b = vs_hit(m, m->vs.tap_x, m->vs.tap_y);
  if (b < 0) return;
  size_t len = strlen(m->vs.ip);
  if (b < VS_KEYS && m->vs.kind != MENU_VS_NET) {
    /* a game found nearby: join it */
    m->vs.join = b + 1;
    snd_menu(SND_SELECT);
  } else if (b < VS_KEYS) {
    static const char KEY[VS_KEYS] = { '1', '2', '3', '4', '5', '6', '7', '8', '9', '.', '0', 0 };
    if (!KEY[b]) {
      if (len) m->vs.ip[len - 1] = 0;
    } else if (len < sizeof m->vs.ip - 1) {
      m->vs.ip[len] = KEY[b];
      m->vs.ip[len + 1] = 0;
    }
    snd_menu(SND_CURSOR);
  } else if (b == VS_CONNECT) {
    if (m->vs.state == MENU_VS_OFF && !len && m->vs.kind == MENU_VS_NET) return;
    m->vs.connect = m->vs.state == MENU_VS_OFF ? 1 : -1;
    snd_menu(m->vs.connect > 0 ? SND_SELECT : SND_CANCEL);
  } else if (b == VS_RAMP_L || b == VS_RAMP_R) {
    m->vs.ramp = (m->vs.ramp + (b == VS_RAMP_L ? MENU_VS_RAMPS - 1 : 1)) % MENU_VS_RAMPS;
    snd_menu(SND_CURSOR);
  } else if (b == VS_KM_L || b == VS_KM_R) {
    m->vs.km += b == VS_KM_L ? -1 : 1;
    m->vs.km = m->vs.km < 1 ? 1 : m->vs.km > 50 ? 50 : m->vs.km;
    snd_menu(SND_CURSOR);
  } else if (b == VS_START) {
    int all = m->vs.n >= 2;
    for (int i = 0; i < m->vs.n; ++i) all &= !m->vs.racing[i];
    if (!all) return;
    m->vs.start = 1;
    snd_menu(SND_SELECT);
  }
}

static void vs_label(const char *s, float x, float y, float px, float a) { hud_ui_text(s, x, y, px, 1, 1, 1, a); }
static void vs_centred(const char *s, const float r[4], float px, float cr, float cg, float cb, float a) {
  hud_ui_text(s, r[0] + 0.5f * (r[2] - hud_ui_text_width(s, px)), r[1] + 0.5f * (r[3] - 7.0f * px), px, cr, cg, cb, a);
}

static const char *vs_place_name(int place) {
  static char s[8];
  snprintf(s, sizeof s, "%d%s", place, place == 1 ? "ST" : place == 2 ? "ND" : place == 3 ? "RD" : "TH");
  return s;
}

/* The race's loading after the votes (the arcade goes from them to the race's loading, the
 * lobby not again): the race's title page as the time attack's (the course, the time zone,
 * this player's song), NOW LOADING bottom left; loaded first, waiting for the others' */
static void draw_vs_loading(const menu_state *m, int clock, float screen_w, float screen_h) {
  /* (black while the cars flying in first come in) */
  if (m->vs.match_n >= 2 && !m->vs.match_played) {
    anm_fill(0, 0, 0, 1);
    return;
  }
  float fade = m->t < FADE ? (float)(m->t + 1) / FADE : 1.0f;
  draw_pre_as(m, fade, 1);
  if (!m->vs.loaded) {
    int f = m->t + 1;
    float a = f > LOADING_POP ? 0.725f + 0.275f * cosf((float)(f - LOADING_POP) * (2.0f * (float)M_PI / 60.0f)) : 1.0f;
    anm_draw_xf(ID.loading, f < LOADING_POP ? f : LOADING_POP, a * fade, LOADING_PRE_DX, 0, 1, NULL, 0);
  } else if (m->vs.wait_others) {
    static const char *const WAIT[4] = { "WAITING FOR THE OTHER PLAYER", "WAITING FOR THE OTHER PLAYER.",
                                         "WAITING FOR THE OTHER PLAYER..", "WAITING FOR THE OTHER PLAYER..." };
    hud_ui_begin(screen_w, screen_h);
    float px = 3.0f, y = screen_h * 0.5f + 60.0f;
    hud_ui_rect(0, y - 14.0f, hud_ui_text_width(WAIT[3], px) + 72.0f, 7.0f * px + 28.0f, 0, 0, 0, 0.6f);
    hud_ui_text(WAIT[(clock / 20) % 4], 36.0f, y, px, 1.0f, 0.55f, 0.1f, fade);
    hud_ui_end();
  }
}

static void draw_vs(const menu_state *m, int clock, int bars, float screen_w, float screen_h) {
  if (m->vs.state == MENU_VS_LOADING) {
    draw_vs_loading(m, clock, screen_w, screen_h);
    return;
  }
  float fade = m->t < FADE ? (float)(m->t + 1) / FADE : 1.0f;
  draw_back(ID.bg, clock % 600, 1);
  anm_draw(ID.line, 0, 1);
  anm_swap title = { anm_sprite(T_TOPBAR_MES), anm_sprite(VS_TEXT + 1 + m->vs.kind) };
  draw_topbar(bars, 1, &title);
  anm_draw(ID.bottom, bars, 1);
  hud_ui_begin(screen_w, screen_h);
  const float O[3] = { 1.0f, 0.55f, 0.1f }; /* the arcade's orange */
  float r[4];
  int off = m->vs.state == MENU_VS_OFF;
  if (m->vs.kind == MENU_VS_NET) {
    /* the server */
    vs_label("SERVER ADDRESS", VS_LEFT, 160, 2.5f, 0.85f);
    hud_ui_rect(VS_LEFT, 182, 410, 56, 0, 0, 0, 0.55f);
    char ip[24];
    int caret = off && (clock / 30) % 2;
    snprintf(ip, sizeof ip, "%s_", m->vs.ip);
    /* as big as fits the box, the caret's room kept (a long address overflowed it: Allan, 10-26) */
    float px = fminf(5.0f, (410.0f - 28.0f) / hud_ui_text_width(ip, 1.0f));
    if (!caret) ip[strlen(ip) - 1] = 0;
    hud_ui_text(ip[0] ? ip : " ", VS_LEFT + 14, 182.0f + (56.0f - 7.0f * px) * 0.5f, px, 1, 1, 1, off ? 1.0f : 0.7f);
    for (int b = 0; b < VS_KEYS && off; ++b) {
      static const char *const LABEL[VS_KEYS] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "0", "DEL" };
      vs_button(m, b, r);
      hud_ui_button(r[0], r[1], r[2], r[3], m->vs.held == b + 1);
      vs_centred(LABEL[b], r, b == 11 ? 3.5f : 4.5f, 1, 1, 1, 0.95f);
    }
  } else {
    /* the games nearby (Bluetooth: the devices hosting one), a tap joins; or host one */
    vs_label(off ? "GAMES NEARBY" : m->vs.hosting ? "HOSTING A GAME" : "JOINED A GAME", VS_LEFT, 160, 2.5f, 0.85f);
    for (int b = 0; b < MENU_VS_FOUND && off; ++b) {
      vs_button(m, b, r);
      if (b < m->vs.n_found) {
        hud_ui_button(r[0], r[1], r[2], r[3], m->vs.held == b + 1);
        hud_ui_text(m->vs.found[b], r[0] + 16, r[1] + 17, 3.0f, 1, 1, 1, 0.95f);
      } else {
        hud_ui_rect(r[0], r[1], r[2], r[3], 0, 0, 0, 0.3f);
        if (b == m->vs.n_found)
          hud_ui_text((clock / 20) % 4 == 0 ? "SEARCHING" : (clock / 20) % 4 == 1 ? "SEARCHING." : (clock / 20) % 4 == 2 ? "SEARCHING.." : "SEARCHING...",
                      r[0] + 16, r[1] + 20, 2.5f, 1, 1, 1, 0.45f);
      }
    }
    if (off)
      hud_ui_text(m->vs.kind == MENU_VS_BT ? "BLUETOOTH ON BOTH DEVICES" : "THE SAME WI-FI ON EVERY DEVICE", VS_LEFT, 448, 2.0f, 1, 1, 1, 0.6f);
  }
  vs_button(m, VS_CONNECT, r);
  hud_ui_button(r[0], r[1], r[2], r[3], m->vs.held == VS_CONNECT + 1);
  const char *act = m->vs.kind == MENU_VS_NET ? (off ? "CONNECT" : "DISCONNECT")
                    : off                     ? "HOST A GAME"
                    : m->vs.hosting           ? "STOP HOSTING"
                                              : "LEAVE";
  vs_centred(act, r, 4.0f, 1, 1, 1, 0.95f);
  if (m->vs.note[0]) hud_ui_text(m->vs.note, VS_LEFT, 584, 2.5f, 1, 0.85f, 0.4f, 1);
  if (m->vs.last_place) {
    char s[48];
    snprintf(s, sizeof s, "LAST RACE: %s OF %d", vs_place_name(m->vs.last_place), m->vs.last_n);
    hud_ui_text(s, VS_LEFT, 612, 2.5f, 1, 1, 1, 0.9f);
  }
  /* the lobby */
  vs_label("PLAYERS", VS_RIGHT, 140, 2.5f, 0.85f);
  for (int i = 0; i < MENU_VS_PLAYERS; ++i) {
    float y = VS_ROW_Y + (float)i * (VS_ROW_H + 8);
    int on = i < m->vs.n && m->vs.state >= MENU_VS_LOBBY;
    hud_ui_rect(VS_RIGHT, y, VS_RIGHT_W, VS_ROW_H, i == m->vs.me && on ? 0.35f : 0, i == m->vs.me && on ? 0.18f : 0, 0, 0.55f);
    char k[4];
    snprintf(k, sizeof k, "%d", i + 1);
    hud_ui_text(k, VS_RIGHT + 12, y + 13, 4.0f, O[0], O[1], O[2], on ? 1.0f : 0.4f);
    if (!on) {
      hud_ui_text("---", VS_RIGHT + 60, y + 18, 3.0f, 1, 1, 1, 0.35f);
      continue;
    }
    if (!vs_glyphs(m, i)) hud_ui_text(m->vs.name[i], VS_RIGHT + 60, y + 7, 3.5f, 1, 1, 1, 1);
    hud_ui_text(m->vs.car[i], VS_RIGHT + 60, y + 35, 2.0f, 0.8f, 0.85f, 0.95f, 0.9f);
    const char *tag = m->vs.racing[i] ? "RACING" : i == m->vs.host ? "HOST" : "READY";
    hud_ui_text(tag, VS_RIGHT + VS_RIGHT_W - 12 - hud_ui_text_width(tag, 2.5f), y + 19, 2.5f,
                m->vs.racing[i] ? 1.0f : 0.5f, m->vs.racing[i] ? 0.5f : 1.0f, 0.4f, 1);
  }
  /* the race: the host chooses */
  int host = vs_host(m);
  vs_label("COURSE / DISTANCE", VS_RIGHT, 414, 2.5f, 0.85f);
  float row[2][4] = { { VS_RIGHT, 436, VS_RIGHT_W, 52 }, { VS_RIGHT, 500, VS_RIGHT_W, 52 } };
  char km[16];
  snprintf(km, sizeof km, "%d KM", m->vs.km);
  for (int k = 0; k < 2; ++k) {
    hud_ui_rect(row[k][0], row[k][1], row[k][2], row[k][3], 0, 0, 0, 0.55f);
    vs_centred(k ? km : "AREA CHOSEN BY MAJORITY", row[k], k ? 3.5f : 2.5f, 1, 1, 1, host || m->vs.state == MENU_VS_LOBBY ? 1 : 0.5f);
    for (int s = 0; host && k && s < 2; ++s) {
      int b = VS_RAMP_L + 2 * k + s;
      vs_button(m, b, r);
      hud_ui_button(r[0], r[1], r[2], r[3], m->vs.held == b + 1);
      vs_centred(s ? ">" : "<", r, 4.0f, 1, 1, 1, 0.95f);
    }
  }
  vs_button(m, VS_START, r);
  if (host) {
    int ready = m->vs.n >= 2;
    for (int i = 0; i < m->vs.n; ++i) ready &= !m->vs.racing[i];
    hud_ui_button(r[0], r[1], r[2], r[3], m->vs.held == VS_START + 1);
    vs_centred(ready ? "START" : m->vs.n < 2 ? "WAITING FOR PLAYERS" : "WAITING FOR THE RACE", r, ready ? 5.0f : 3.0f, 1, 1, 1,
               ready ? 0.95f : 0.5f);
  } else if (m->vs.state == MENU_VS_LOBBY) {
    vs_centred("WAITING FOR THE HOST", r, 3.0f, 1, 1, 1, 0.7f);
  }
  hud_ui_end();
  /* the names as the card has them, in font 5's glyphs (over the rows the UI drew; the UI's
   * units are the screen's, the sprites' the 640x480 layout's), VS_GLYPH_H tall */
  float kx = 640.0f / screen_w, ky = 480.0f / screen_h;
  for (int i = 0; i < m->vs.n && m->vs.state >= MENU_VS_LOBBY; ++i) {
    if (!vs_glyphs(m, i)) continue;
    float y = VS_ROW_Y + (float)i * (VS_ROW_H + 8) + 3.0f, x = VS_RIGHT + 60.0f;
    for (int j = 0; j < 5 && m->vs.glyph[i][j] >= 0; ++j) {
      int sp = anm_sprite(NAME_GLYPH5 + m->vs.glyph[i][j]);
      float w = anm_sprite_width(sp), h = anm_sprite_height(sp);
      if (h <= 0) continue;
      float k = VS_GLYPH_H / h; /* screen px per sprite px */
      const float src[4] = { 0, 0, w, h };
      float dst[4] = { x * kx, y * ky, (x + w * k) * kx, (y + VS_GLYPH_H) * ky };
      anm_draw_part(sp, src, dst, 1);
      x += VS_GLYPH_W * k;
    }
  }
  hud_draw_menu_keys(screen_w, screen_h, m->key_held, 0, 1);
  anm_fill(0, 0, 0, 1.0f - fade);
  if (m->flash) anm_fill(1, 1, 1, (float)m->flash / FLASH_LEN);
}

/* ---- the area vote (MENU_VS_AREA): seqJoCourseSelect's clseqJoCourseSelectThread, the
 * versus' "Please select the Driving area / Area shall be chosen by majority" after the
 * lobby's START.
 * - menu_layout_vs_course1, the strip of nine cards (0 omakase, then the areas by menu order:
 *   C1, Shinkanjo, Wangan, Yokohane, Nagoya, Osaka, Fukuoka, Hakone), 100 frames a card k:
 *   k*100..58 the card chosen at rest (the arrows' pulse), k*100+60.. decided, k*100+90..94
 *   the 5 frames on to card k + 1 (back: the same, the other way). The run's init sets the
 *   cards' pictures to the whole areas' and hides every "Can not select" and the distances.
 * - The card's place on screen (0..5) and the strip's scroll: going right it stays at 3 from
 *   the fourth card on while the card was 5 or less, going left at 2 from the third on, the
 *   strip sliding 17.6 px a frame for the move's 5 frames (0x93fba54).
 * - The map: clMiniatureCourse with the versus' table (0x93fa400: Tokyo's cards on the 3D
 *   miniature) or mapChange's city picture (Nagoya, Osaka, Fukuoka, Hakone), as the time
 *   attack's.
 * - 20 s on the top bar (clMenuBarStd::setTime: menu_g_timer_x, ti_count each second from 9);
 *   at 0 the card under the cursor is the vote. Omakase votes a random area (the timer's
 *   bits: Hakone, 6, at half the odds), which the cursor walks to.
 * - dispOther: the other players' marks (menu_p_vs_4p_N) under the card each has; three a
 *   card, 32 px apart; a mark off the screen goes to its edge with an arrow (menu_g_vs_icon,
 *   or its omakase pair after an omakase vote).
 * - When all have voted the area with the most votes wins, a tie by a random pick among them
 *   (clNet::getJoinArea); the strip goes to it and plays its decided frames.
 * The vote rides each player's lobby status (menu_vs_status); the host decides and starts the
 * race on a random ramp of the area (app.c vs_menu). */
#define AREA_TIME (20 * 60)
#define AREA_MOVE 5
#define AREA_SCROLL 17.6f
#define AREA_DECIDE 10 /* the decided frames the result plays (k*100+60..69) */
#define AREA_VOTED 16  /* this player's own vote: k*100+60..75, then held on 80 (the strip, no arrows) */
enum { A_ICON = 0x39e, A_ARROW_R = 7377, A_ARROW_L = 7771, A_ARROW = 2535, A_ARROW_L_IMG = 3393,
       A_TOPBAR = 7149, A_TIME_DIGIT = 693, A_TIME_LAST = 2524 };
/* the cards' pictures (the run's init: the areas' own) and what it hides */
static const int AREA_PICTURE[8][2] = { { 4646, 5545 }, { 4648, 5546 }, { 4650, 5550 }, { 4652, 5557 },
                                        { 4658, 4659 }, { 4654, 4657 }, { 4655, 5559 }, { 4661, 4661 } };
static const int AREA_HIDE[] = { 6524, 7009, 7010, 7011, 7012, 7013, 7014, 7015, 2777, 3168,
                                 2239, 2240, 2241, 2242, 2243, 2244, 2245, 2246, 2247, 2248,
                                 7031, 7032, 7033, 7034, 7035, 7036, 7037, 7038, 7039, 7040,
                                 7226, 7227, 7228, 7229, 7230, 7231, 7232, 7233, 7234, 7235, 7236, 7237, 7238, 7493 };
#define AREA_NSWAP (8 + (int)(sizeof AREA_HIDE / sizeof AREA_HIDE[0]))
/* dispOther's places (0x93f9fe0): a mark under a card left of this player's, under it, right
 * of it; rows 32 px down. Off the screen (0x93f9f80): the left or right edge, from the bottom */
static const float AREA_MARK_X0[3] = { 60, 100, 140 }, AREA_MARK_Y0[3] = { 210, 260, 210 };
static const float AREA_EDGE_X[2] = { 69, 570 }, AREA_EDGE_Y[3] = { 316, 348, 380 };
/* menu_g_vs_icon's frames by player (0x93f9f5c) */
static const int AREA_ICON_FROM[MENU_VS_PLAYERS] = { 1, 101, 201, 301 };
/* areaIDFromMenuOrder: the area of card k (1..8) */
static const int CARD_AREA[MENU_VS_CARDS] = { -1, 0, 1, 2, 3, 4, 5, 7, 6 };

int menu_vs_card_area(int card) { return card > 0 && card < MENU_VS_CARDS ? CARD_AREA[card] : -1; }

int menu_vs_status(const menu_state *m) {
  if (m->screen == MENU_BGM && m->vs.day) {
    if (m->vs.day_final >= 0 && m->vs.me == m->vs.host) return MENU_VS_DAY_FINAL + m->vs.day_final;
    return (m->vs.day_voted == 2 ? MENU_VS_DAY_OMAKASE : m->vs.day_voted ? MENU_VS_DAY_VOTED : MENU_VS_DAY) + m->zone;
  }
  /* (on the stars screen as if still racing: the host's START waits for it) */
  if (m->screen == MENU_VS_STAR) return 1;
  if (m->screen != MENU_VS_AREA) return m->vs.state < MENU_VS_LOADING ? 0 : m->vs.loaded ? MENU_VS_LOADED : 1;
  if (m->vs.area_final >= 0 && m->vs.me == m->vs.host) return MENU_VS_FINAL + m->vs.area_final;
  if (m->vs.voted) return (m->vs.voted == 2 ? MENU_VS_OMAKASE : MENU_VS_VOTED) + m->vs.vote;
  return MENU_VS_CHOOSING + m->vs.item;
}

void menu_vs_loading(menu_state *m) {
  if (m->screen == MENU_VS_AREA || m->screen == MENU_VS_MATCH || (m->screen == MENU_BGM && m->vs.day)) go(m, MENU_VS);
  m->vs.state = MENU_VS_LOADING;
  m->vs.day = m->vs.loaded = m->vs.wait_others = m->vs.match_played = 0;
}

static void vs_area_enter(menu_state *m) {
  go(m, MENU_VS_AREA);
  m->vs.area_go = 0;
  m->vs.item = m->vs.pos = 0;
  m->vs.item_from = -1;
  m->vs.move_t = 0;
  m->vs.scroll = m->vs.scroll_from = 0;
  m->vs.timer = AREA_TIME;
  m->vs.voted = m->vs.vote = m->vs.vote_t = 0;
  m->vs.area_final = -1;
  m->vs.final_t = 0;
  m->music = 1;
}

/* one card on (d = +1) or back (-1), as the arcade's moves */
static void area_move(menu_state *m, int d) {
  int to = m->vs.item + d;
  if (to < 0 || to >= MENU_VS_CARDS) return;
  snd_menu(SND_CURSOR);
  int scroll = d > 0 ? m->vs.pos > 2 && m->vs.item <= 5 : m->vs.pos <= 2 && m->vs.item > 2;
  if (!scroll) m->vs.pos += d;
  m->vs.scroll_from = m->vs.scroll;
  if (scroll) m->vs.scroll -= AREA_SCROLL * AREA_MOVE * (float)d;
  m->vs.item_from = m->vs.item;
  m->vs.item = to;
  m->vs.move_t = 0;
}

void menu_vs_song(menu_state *m, int song) {
  m->song = song > 0 && song < BGM_LINES - 1 ? song : -1;
  m->bgm = m->song > 0 ? m->song : BGM_LINES - 1;
}

/* the arcade's omakase: an area from the timer's bits, Hakone (6) at half the odds */
int menu_vs_random_card(void) {
  int a = rand() & 7;
  if (a == 6 && (rand() & 1)) a = (rand() >> 2) & 7;
  for (int k = 1; k < MENU_VS_CARDS; ++k)
    if (CARD_AREA[k] == a) return k;
  return 1;
}

static void area_vote(menu_state *m) {
  snd_menu(SND_SELECT);
  if (m->vs.item == 0) {
    /* the arcade's omakase is a random area; here it leaves the choice to the others (the
     * host counts it so: random only when every vote is omakase) */
    m->vs.voted = 2;
    m->vs.vote = 0;
  } else {
    m->vs.voted = 1;
    m->vs.vote = m->vs.item;
  }
  m->vs.vote_t = 0;
}

/* the strip's card under a tap: -1 left of this player's, +1 right, 0 on it */
static int area_tap_dir(const menu_state *m) {
  float left = 88.0f * (float)m->vs.item - 10.0f + m->vs.scroll;
  if (m->tap_y > 290.0f) return 2; /* below the strip: nothing */
  return m->tap_x < left ? -1 : m->tap_x > left + 164.0f ? 1 : 0;
}

static void vs_area_step(menu_state *m, int tap, int key) {
  /* the vote lost its lobby (the host gone, the connection lost): back to the versus screen */
  if (m->vs.state != MENU_VS_LOBBY) {
    go(m, MENU_VS);
    return;
  }
  if (m->vs.move_t < AREA_MOVE && m->vs.item_from >= 0 && ++m->vs.move_t >= AREA_MOVE) m->vs.item_from = -1;
  if (m->vs.area_final >= 0) {
    if (m->vs.final_t == 0 && m->vs.item != m->vs.area_final) {
      /* the strip jumps to the result: its place and scroll as the cursor's walk there */
      m->vs.item = m->vs.area_final;
      m->vs.item_from = -1;
      m->vs.pos = 0;
      m->vs.scroll = 0;
      for (int k = 0; k < m->vs.item; ++k) {
        if (m->vs.pos <= 2 || k > 5) m->vs.pos++;
        else m->vs.scroll -= 88.0f;
      }
    }
    m->vs.final_t++;
    return;
  }
  /* the 20 s (debug_time_keep aside): the shown second's ti_count from 9 down */
  if (m->vs.timer > 0) {
    int was = (m->vs.timer + 59) / 60;
    m->vs.timer--;
    int now = (m->vs.timer + 59) / 60;
    if (now != was && now >= 1 && now <= 9) snd_menu(SND_TIME_COUNT);
    if (!m->vs.timer && !m->vs.voted) area_vote(m);
  }
  if (m->vs.voted) {
    /* omakase's walk to its area, a card at a time; there, the decided frames */
    if (m->vs.item_from >= 0) return;
    if (m->vs.item != m->vs.vote) area_move(m, m->vs.vote > m->vs.item ? 1 : -1);
    else m->vs.vote_t++;
    return;
  }
  if (m->vs.item_from >= 0) return;
  if (key) {
    area_move(m, key > 0 ? 1 : -1);
  } else if (tap) {
    int d = area_tap_dir(m);
    if (d == 0) area_vote(m);
    else if (d != 2) area_move(m, d);
  }
}

/* the strip's frame: at rest, moving, decided (own vote, the result) */
static int area_frame(const menu_state *m) {
  int k = m->vs.item;
  if (m->vs.area_final >= 0) return k * 100 + 60 + (m->vs.final_t < AREA_DECIDE ? m->vs.final_t : AREA_DECIDE - 1);
  if (m->vs.item_from >= 0) {
    int t = m->vs.move_t < AREA_MOVE ? m->vs.move_t : AREA_MOVE - 1;
    return m->vs.item_from < k ? m->vs.item_from * 100 + 90 + t : k * 100 + 94 - t;
  }
  if (m->vs.voted && m->vs.item == m->vs.vote) {
    /* the walk's last step done: decided, then the card held without its arrows */
    return m->vs.vote_t < AREA_VOTED ? k * 100 + 60 + m->vs.vote_t : k * 100 + 80;
  }
  return k * 100 + m->t % 59;
}

/* the strip's shift: the scroll, sliding over a move */
static float area_dx(const menu_state *m) {
  if (m->vs.item_from < 0 || m->vs.move_t >= AREA_MOVE) return m->vs.scroll;
  return m->vs.scroll_from + (m->vs.scroll - m->vs.scroll_from) * (float)m->vs.move_t / AREA_MOVE;
}

/* clMenuBarStd::setTime's countdown (menu_g_timer_x) where the bar has its third dot: the
 * second shown, then at the second's end its scale-out into the next */
static void vs_timer(int timer, int bars, float fade) {
  float r[4];
  if (ID.timer < 0 || !anm_sprite_rect(ID.top, bars, anm_sprite(T_TIME_DOT3), r)) return;
  int s = (timer + 59) / 60, p = s * 60 - timer; /* p: frames into the second shown */
  if (p > 59) p = 59;
  anm_swap sw[8];
  int n = 0;
  for (int set = 0; set < 2; ++set) {
    int v = set ? s - 1 : s;
    if (v < 0) v = 0;
    /* the templates: tens, ones, one digit, the last seconds' red */
    int tens = A_TIME_DIGIT + set, ones = A_TIME_DIGIT + 4 + set, one = A_TIME_DIGIT + 2 + set, last = A_TIME_LAST + set;
    sw[n++] = (anm_swap){ anm_sprite(tens), v >= 10 ? anm_sprite(A_TIME_DIGIT + v / 10) : -1 };
    sw[n++] = (anm_swap){ anm_sprite(ones), v >= 10 ? anm_sprite(A_TIME_DIGIT + v % 10) : -1 };
    sw[n++] = (anm_swap){ anm_sprite(one), -1 };
    sw[n++] = (anm_swap){ anm_sprite(last), v < 10 ? anm_sprite(A_TIME_LAST + v) : -1 };
  }
  /* (a record takes one swap: the templates are digits too) */
  anm_draw_ex(ID.timer, p, fade, r[0], r[1], sw, n);
}

/* dispOther: the others' marks */
static void area_marks(const menu_state *m, int clock, float dx, float fade) {
  int under[MENU_VS_CARDS] = { 0 }, edge[2] = { 0 };
  for (int i = 0; i < m->vs.n && i < MENU_VS_PLAYERS; ++i) {
    if (i == m->vs.me || m->vs.other_vote[i] < 0) continue;
    int card = m->vs.other[i], omakase = m->vs.other_vote[i] == 2;
    if (card < 0 || card >= MENU_VS_CARDS) continue;
    int set = card < m->vs.item ? 0 : card == m->vs.item ? 1 : 2;
    int row = under[card]++;
    if (row > 2) continue;
    float x = AREA_MARK_X0[set] + 88.0f * (float)card + dx, y = AREA_MARK_Y0[set] + 32.0f * (float)row;
    int f = clock % 59;
    if (x >= 0.0f && x <= 640.0f) {
      anm_swap sw = { anm_sprite(A_ICON), anm_sprite(A_ICON + i) };
      anm_draw_ex(omakase ? ID.vs_icon_omakase : ID.vs_ex_player, 1 + f, fade, x, y, &sw, 1);
      continue;
    }
    int side = set == 0 ? 0 : 1, c = edge[side]++;
    if (c > 2) continue;
    x = AREA_EDGE_X[side] + (omakase ? (side ? -18.0f : 19.0f) : 0.0f);
    y = AREA_EDGE_Y[c < 2 ? 2 - c : 0];
    anm_swap sw[2] = { { anm_sprite(A_ARROW_R + i), side ? -1 : anm_sprite(A_ARROW) },
                       { anm_sprite(A_ARROW_L + i), side ? anm_sprite(A_ARROW_L_IMG) : -1 } };
    anm_draw_ex(omakase ? ID.vs_icon_omakase2 : ID.vs_icon, AREA_ICON_FROM[i] + f, fade, x, y, sw, 2);
  }
}

static void draw_vs_area(const menu_state *m, int clock, int bars, float screen_w, float screen_h,
                         void (*mini3d)(void *, float)) {
  float fade = m->t < FADE ? (float)(m->t + 1) / FADE : 1.0f;
  draw_back(ID.bg, clock % 600, fade);
  anm_draw(ID.line, 0, fade);
  draw_bg_logo(chosen_car(m, m->car), clock % 240, fade);
  /* mapChange: the city's picture for Nagoya, Osaka, Fukuoka, Hakone; Tokyo's the miniature */
  static const int CITY_MAP[4] = { 0x1832, 0x15b9, 0x19a4, 0x1831 };
  int k = m->vs.item, city = k >= 5 && k <= 8;
  if (mini3d && k >= 1 && k <= 4) mini3d((void *)m, fade);
  if (city && m->vs.item_from < 0 && ID.course_map >= 0) {
    anm_swap map = { anm_sprite(0x15b9), anm_sprite(CITY_MAP[k - 5]) };
    anm_draw_ex(ID.course_map, 0, fade, 0, 0, &map, 1);
  }
  anm_swap sw[AREA_NSWAP];
  int n = 0;
  for (int i = 0; i < 8; ++i) sw[n++] = (anm_swap){ anm_sprite(AREA_PICTURE[i][0]), anm_sprite(AREA_PICTURE[i][1]) };
  for (int i = 0; i < AREA_NSWAP - 8; ++i) sw[n++] = (anm_swap){ anm_sprite(AREA_HIDE[i]), -1 };
  float dx = area_dx(m);
  anm_draw_ex(ID.vs_course, area_frame(m), fade, dx, 0, sw, n);
  area_marks(m, clock, dx, fade);
  anm_swap mes = { anm_sprite(T_TOPBAR_MES), anm_sprite(A_TOPBAR) };
  draw_topbar_timed(bars, fade, &mes);
  vs_timer(m->vs.timer, bars, fade);
  anm_draw(ID.bottom, bars, fade);
  if (!m->vs.voted && m->vs.area_final < 0) hud_draw_menu_keys(screen_w, screen_h, m->key_held, 1, 0);
  draw_vs_omakase(m, screen_w, screen_h);
}

/* ---- the cars flying in before a versus race (MENU_VS_MATCH): the arcade's
 * clRaceThread::JoMatchingInformationMove (with clSeqJoMatchingInformation's set-up). The
 * race's machines' cars (clMenuCar, environment 0) follow data/menu3d/demo/Battle_anm.igb's
 * car (battle.h: its 3D camera too, makeProjection(55, -1, 4/3, 5, 20000)), each placed by
 * the tables for its count (0x9412500..): scaled, turned about x and moved; the second set of
 * two (slot 1) mirrored across x, coming from the other side. Over menu_g_shogo_bg_1 behind
 * them; frame f from the screen's start:
 *   f 40     the cars show and the animation runs (a frame of it each frame, held at 200);
 *            the engine (SNDEnginePlay, accel on): rpm 3500 + 116.7 a frame to f 70, then
 *            brakes and slip till f 135, the rpm down 233.3 a frame to f 100
 *   f 80     menu_p_pre_vs_rival_<n>_shadow (frames 1..39, then 101..159 round)
 *   f 120    _car: each machine's name plate (clNamePlate2, at its dot sprite 1716 + PCB)
 *   f 160    _vs: VS (cutin2, SE 0x22)
 *   f 260    over (the fade out); the race */
#define MATCH_CARS 40
#define MATCH_SHADOW 80
#define MATCH_PLATES 120
#define MATCH_VS 160
#define MATCH_END 260
#define MATCH_FADE 10
/* by machine count (2..4) and machine: the set, scale, turn about x (degrees), x and y */
static const signed char MATCH_SLOT[3][4] = { { 0, 1 }, { 0, 1, 0 }, { 0, 0, 0, 0 } };
static const float MATCH_SCALE[3][4] = { { 1, 1 }, { 0.7f, 0.7f, 0.7f }, { 0.6f, 0.6f, 0.6f, 0.6f } };
static const float MATCH_ROT[3][4] = { { 17, -6 }, { 19, 6, -7 }, { 18, 7, -2, -11.5f } };
static const float MATCH_X[3][4] = { { -1.5f, 1.5f }, { -1.5f, 1.5f, -1.5f }, { -4.5f, -4.5f, -4.5f, -4.5f } };
static const float MATCH_Y[3][4] = { { 20, -9.5f }, { 21.5f, 4, -14 }, { 23.5f, 10, -2.5f, -15.5f } };
/* clNamePlate2's templates: the car's name plate, the power and the handling (its two
 * fonts' places), the class, the title and the name */
enum { T_PLATE_CAR = 7450, T_PLATE_POWER = 1391, T_PLATE_HANDLING = 1392, T_PLATE_CLASS = 1389, T_PLATE_TITLE = 1390,
       T_PLATE_NAME = 1439, T_PLATE_DOT = 1716 };

void menu_vs_match(menu_state *m) {
  go(m, MENU_VS_MATCH);
  m->vs.match_t = 0;
}

int menu_vs_match_done(const menu_state *m) {
  return m->screen == MENU_VS_MATCH && m->vs.match_t >= MATCH_END + MATCH_FADE;
}

void menu_vs_match_back(menu_state *m) {
  go(m, MENU_VS);
  m->vs.match_played = 1;
}

/* the animation's frame at screen frame f (the arcade's counter runs from the cars' showing) */
static int match_anim(int f) {
  int a = f - MATCH_CARS + 1;
  return a < 0 ? 0 : a >= BATTLE_FRAMES ? BATTLE_FRAMES - 1 : a;
}

/* row-vector matrices (Alchemy's, GL's floats): out = a b, a first */
static void match_mul(float *out, const float *a, const float *b) {
  float r[16];
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j)
      r[4 * i + j] = a[4 * i] * b[j] + a[4 * i + 1] * b[4 + j] + a[4 * i + 2] * b[8 + j] + a[4 * i + 3] * b[12 + j];
  memcpy(out, r, sizeof r);
}

static int match_set(const menu_state *m) {
  int n = m->vs.match_n;
  return n < 2 ? 0 : n > 4 ? 2 : n - 2;
}

/* car i's matrix: the animation's body (mirrored for slot 1) * scale * turn about x * move */
static void match_car_matrix(const menu_state *m, int i, const float *body, float *out) {
  int k = match_set(m);
  float b[16];
  memcpy(b, body, sizeof b);
  if (MATCH_SLOT[k][i]) /* F b F, F = (-1, 1, 1) */
    for (int r = 0; r < 4; ++r)
      for (int c = 0; c < 4; ++c)
        if ((r == 0) != (c == 0)) b[4 * r + c] = -b[4 * r + c];
  float sc = MATCH_SCALE[k][i], a = MATCH_ROT[k][i] * DEG, cs = cosf(a), sn = sinf(a);
  const float S[16] = { sc, 0, 0, 0, 0, sc, 0, 0, 0, 0, sc, 0, 0, 0, 0, 1 };
  const float R[16] = { 1, 0, 0, 0, 0, cs, sn, 0, 0, -sn, cs, 0, 0, 0, 0, 1 };
  const float T[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, MATCH_X[k][i], MATCH_Y[k][i], 0, 1 };
  match_mul(out, b, S);
  match_mul(out, out, R);
  match_mul(out, out, T);
}

static int match_scene3d(const menu_state *m, menu_scene *s) {
  const battle_frame *bf = &BATTLE[match_anim(m->vs.match_t)];
  s->env = 0;
  s->fov_x = 55.0f * DEG;
  memcpy(s->eye, bf->eye, sizeof s->eye);
  memcpy(s->at, bf->at, sizeof s->at);
  for (int i = 0; i < m->vs.match_n && i < MENU_VS_PLAYERS; ++i) {
    int mk = m->vs.match[i].maker, cr = m->vs.match[i].car, co = m->vs.match[i].color;
    if (mk < 0 || mk >= CARDB_ALL || cr < 0 || cr >= CARDB_NCARS[mk]) mk = m->maker, cr = m->car, co = m->color;
    const cardb_car *c = &CARDB_CARS[mk][cr];
    const cardb_color *col = &c->colors[co >= 0 && co < c->ncolors ? co : 0];
    menu_car *mc = &s->cars[s->n++];
    mc->pack = col->pack;
    mc->paint = col->paint;
    mc->aero = m->vs.match[i].aero, mc->mirror = m->vs.match[i].mirror, mc->wing = m->vs.match[i].wing;
    mc->bonnet = m->vs.match[i].bonnet, mc->wheel = m->vs.match[i].wheel, mc->neon = m->vs.match[i].neon;
    mc->vinyl = m->vs.match[i].vinyl;
    match_car_matrix(m, i, bf->body, mc->model);
    /* the corners' spin and steer (TIRE_ROT_k * TIRE_k), mirrored with the car */
    mc->turned = 1;
    for (int w = 0; w < 4; ++w) {
      float *t = mc->tyre[w];
      memset(t, 0, sizeof mc->tyre[w]);
      for (int col3 = 0; col3 < 3; ++col3)
        for (int r = 0; r < 3; ++r) {
          float v = bf->wheel[w][3 * col3 + r];
          if (MATCH_SLOT[match_set(m)][i] && (col3 == 0) != (r == 0)) v = -v;
          t[4 * col3 + r] = v;
        }
      t[15] = 1;
    }
  }
  return 1;
}

/* the overlays' frames: once 1..39 from their start, then 101..159 round */
static int match_overlay_frame(int since) {
  return since < 39 ? 1 + since : 101 + (since - 39) % 59;
}

static int match_r35(int maker, int car) {
  return maker >= 0 && maker < CARDB_ALL && car >= 0 && car < CARDB_NCARS[maker] && has(CARDB_CARS[maker][car].label, "[R35]");
}

/* clNamePlate2 of machine i at (x, y), sy its height's scale (the 3 machines' plates
 * unfold): the plate of an R35 (this one's or the player's) shows the tune's power and
 * handling, the other its horsepower; the car's name plate, the player's name */
static void match_plate(const menu_state *m, int i, float x, float y, float sy, float a) {
  int mk = m->vs.match[i].maker, cr = m->vs.match[i].car;
  if (mk < 0 || mk >= CARDB_ALL || cr < 0 || cr >= CARDB_NCARS[mk]) return;
  const cardb_car *c = &CARDB_CARS[mk][cr];
  int me = m->vs.match_me >= 0 && m->vs.match_me < m->vs.match_n ? m->vs.match_me : 0;
  int ph = match_r35(mk, cr) || match_r35(m->vs.match[me].maker, m->vs.match[me].car);
  int anim = ph ? ID.plate_ph : ID.plate;
  if (anim < 0) return;
  anm_swap sw[] = {
    { anm_sprite(T_PLATE_CAR), anm_sprite(c->plate) }, { anm_sprite(T_PLATE_POWER), -1 },
    { anm_sprite(T_PLATE_HANDLING), -1 }, { anm_sprite(T_PLATE_CLASS), -1 },
    { anm_sprite(T_PLATE_TITLE), -1 }, { anm_sprite(T_PLATE_NAME), -1 },
  };
  anm_draw_xy(anim, 0, a, x, y, 1, sy, sw, (int)(sizeof sw / sizeof sw[0]));
  if (sy < 0.95f) return; /* the texts once it is open */
  float r[4];
  char text[16];
  if (anm_sprite_rect(anim, 0, anm_sprite(T_PLATE_POWER), r)) {
    if (ph) snprintf(text, sizeof text, "%d", m->vs.match[i].power);
    else snprintf(text, sizeof text, "%d", car_tune_hp(c->hp, m->vs.match[i].power));
    card_glyphs(CARD_GLYPH2, "-,", 8.0f, text, x + r[0], y + r[1], a);
  }
  if (ph && anm_sprite_rect(anim, 0, anm_sprite(T_PLATE_HANDLING), r)) {
    snprintf(text, sizeof text, "%d", m->vs.match[i].handling);
    card_glyphs(CARD_GLYPH2, "-,", 8.0f, text, x + r[0], y + r[1], a);
  }
  /* the name centred on its template (the plate's middle: a clSpriteFont's), 28 px a glyph:
   * from the template's left edge a long one ran off the plate (Allan 10-27) */
  if (anm_sprite_rect(anim, 0, anm_sprite(T_PLATE_NAME), r) && anm_sprite(NAME_GLYPH5) >= 0) {
    int n = 0;
    while (n < 5 && m->vs.match[i].glyph[n] >= 0) n++;
    float x0 = x + 0.5f * (r[0] + r[2]) - 14.0f * (float)n;
    for (int j = 0; j < n; ++j)
      anm_draw_sprite(anm_sprite(NAME_GLYPH5 + m->vs.match[i].glyph[j]), x0 + 28.0f * (float)j, y + r[1], a);
  }
}

static void match_step(menu_state *m) {
  int f = ++m->vs.match_t;
  if (f == MATCH_VS + 1) snd_menu(SND_CUTIN2);
}

static void draw_vs_match(const menu_state *m, void (*draw3d)(void *), void *ctx) {
  int f = m->vs.match_t, k = match_set(m);
  float fade = f < MATCH_FADE ? (float)(f + 1) / MATCH_FADE
               : f > MATCH_END ? 1.0f - (float)(f - MATCH_END) / MATCH_FADE : 1.0f;
  if (fade < 0) fade = 0;
  anm_fill(0, 0, 0, 1);
  if (ID.shogo_bg >= 0) anm_draw(ID.shogo_bg, 0, 1);
  if (draw3d && f >= MATCH_CARS) draw3d(ctx);
  if (f > MATCH_SHADOW && ID.pre_shadow[k] >= 0) anm_draw(ID.pre_shadow[k], match_overlay_frame(f - MATCH_SHADOW - 1), 1);
  if (f > MATCH_PLATES && ID.pre_cars[k] >= 0) {
    int pf = match_overlay_frame(f - MATCH_PLATES - 1);
    for (int i = 0; i < m->vs.match_n && i < MENU_VS_PLAYERS; ++i) {
      int dot = anm_sprite(T_PLATE_DOT + i);
      float r[4];
      if (!anm_sprite_rect(ID.pre_cars[k], pf, dot, r)) continue;
      float h = anm_sprite_height(dot), sy = h > 0 ? (r[3] - r[1]) / h : 1;
      match_plate(m, i, r[0], r[1], sy, anm_sprite_alpha(ID.pre_cars[k], pf, dot));
    }
  }
  if (f > MATCH_VS && ID.pre_vs[k] >= 0) anm_draw(ID.pre_vs[k], match_overlay_frame(f - MATCH_VS - 1), 1);
  anm_fill(0, 0, 0, 1.0f - fade);
}

static void step(menu_state *m) {
  m->t++;
#ifdef WMMT_EGL
  static int tg_now;
  if (m->screen == MENU_GAME && !tg_now && getenv("WMMT_TG_NOW")) { /* test: these dress-up wins now */
    tg_now = 1;
    menu_dress_win(m, atoi(getenv("WMMT_TG_NOW")));
    tg_due(m);
  }
  static int title_now;
  if (m->screen == MENU_GAME && !title_now && getenv("WMMT_TITLE_NOW")) { /* test: these new titles now */
    title_now = 1;
    int t[3] = { 0, 0, 0 };
    sscanf(getenv("WMMT_TITLE_NOW"), "%d %d %d", &t[0], &t[1], &t[2]);
    menu_title_due(m, t);
    shogo_open(m);
    return;
  }
#endif
  if (m->screen == MENU_PRE) {
    if (m->loading) m->load_shown = m->loading, m->load_out = 0;
    else if (m->load_shown && m->load_out < LOADING_OUT) m->load_out++;
  }
  int tap = m->tap, key = m->key, back = m->back, option = m->option, dress = m->dress, erase = m->erase;
  int hidden = m->hidden;
  m->tap = m->key = m->back = m->option = m->dress = m->erase = m->hidden = 0;
  if (m->flash) m->flash--;
  if (m->bars && ++m->bars > 11) m->bars = 0;
  if (in_attract(m)) {
    if ((tap || key) && !menu_booting(m)) {
      menu_start(m);
      return;
    }
    if (m->screen == MENU_TITLE && m->t >= TITLE_LEN + FADE) go(m, MENU_RANKING);
    else if (m->screen == MENU_RANKING && m->t >= RANKING_LEN + FADE) go(m, MENU_LOGO);
    else if (m->screen == MENU_LOGO && m->t >= 3 * LOGO_LEN) go(m, MENU_PROLOGUE);
    else if (m->screen == MENU_PROLOGUE && m->t >= 2 * PROLOGUE_LEN) {
      m->demo++;
      go(m, MENU_MOVIE);
    }
    return;
  }
  if (m->screen == MENU_VS && m->vs.area_go) {
    vs_area_enter(m);
    return;
  }
  if (m->screen == MENU_VS) {
    vs_step(m, back);
    return;
  }
  if (m->screen == MENU_VS_AREA) {
    vs_area_step(m, tap, key);
    return;
  }
  if (m->screen == MENU_VS_MATCH) {
    match_step(m);
    return;
  }
  if (m->screen == MENU_START) {
    if (m->t >= START_LEN) {
      if (m->save_skip) car_select_enter(m, MENU_MAKER);
      else save_enter(m);
      m->flash = FLASH_LEN;
      m->bars = 1;
    }
    return;
  }
  if (m->deciding) {
    m->deciding--;
    return;
  }
  if (m->screen == MENU_INFO) {
    if (back || tap) {
      snd_menu(SND_CANCEL);
      go(m, MENU_SAVE);
    }
    return;
  }
  if (m->screen == MENU_SAVE) {
    if (back) {
      snd_menu(SND_CANCEL);
      m->leaving = 1;
      m->deciding = FADE;
      return;
    }
    if (option && !m->save_confirm) {
      snd_menu(SND_SELECT);
      m->opt_from = MENU_SAVE;
      go(m, MENU_OPTION);
      return;
    }
    if (save_step(m, tap, key)) m->deciding = FADE;
    return;
  }
  if (m->screen == MENU_NAME) {
    if (m->name_pop) m->name_pop--;
    if (m->name_char_pop) m->name_char_pop--;
    if (back) {
      if (name_len(m)) name_erase(m);
      else {
        snd_menu(SND_CANCEL);
        save_enter(m);
      }
      return;
    }
    if (key) {
      snd_menu(SND_CURSOR);
      m->name_page = (m->name_page + key + 4) % 4;
      return;
    }
    if (tap && name_tap(m)) m->deciding = FADE;
    return;
  }
  if (m->screen == MENU_HAVE) {
    if (back && !m->slide) {
      snd_menu(SND_CANCEL);
      if (m->keep_ask) card_enter(m, MENU_SETTING);
      else save_enter(m);
      return;
    }
    if (have_step(m, tap, key)) m->deciding = DECIDE_DAY + FADE;
    return;
  }
  if (m->screen == MENU_GARAGE) {
    if (erase) {
      if (m->erase_armed) {
        snd_menu(SND_SELECT);
        garage_erase(m);
      } else {
        snd_menu(SND_CURSOR);
        m->erase_armed = 1;
      }
      return;
    }
    /* anything else takes the bin's arming back first */
    if (m->erase_armed && (back || tap || key)) {
      snd_menu(SND_CANCEL);
      m->erase_armed = 0;
      return;
    }
    if (back) {
      snd_menu(SND_CANCEL);
      have_enter(m);
      return;
    }
    if (garage_step(m, tap, key)) m->deciding = FADE;
    return;
  }
  if (m->screen == MENU_CARD) {
    if (back) {
      snd_menu(SND_CANCEL);
      if (m->card_back == MENU_SETTING) {
        /* a new car's card: not kept yet, its setting again (its choice there kept) */
        int mk = m->maker, c = m->car;
        signed char full = m->tune_full[mk][c], power = m->tune_power[mk][c];
        keep_drop(m);
        m->tune_full[mk][c] = full, m->tune_power[mk][c] = power;
        m->new_card = 1;
        m->tune_locked = 0;
      }
      if (m->card_back == MENU_GARAGE) garage_enter(m, m->own_sel);
      else car_select_enter(m, m->card_back == MENU_SETTING ? MENU_SETTING : MENU_COLOR);
      return;
    }
    if (tap) {
      snd_menu(SND_SELECT);
      m->deciding = FADE;
    }
    return;
  }
  if (m->screen == MENU_LAYOUT) {
    layout_step(m, back);
    return;
  }
  if (m->screen == MENU_OPTION) {
    if (back) {
      snd_menu(SND_CANCEL);
      if (m->opt_from == MENU_SAVE) {
        save_enter(m);
      } else {
        car_select_enter(m, MENU_MAKER);
        m->sel = m->opt_sel;
      }
      return;
    }
    option_step(m, tap, key);
    return;
  }
  if (option && m->screen == MENU_MAKER && m->from < 0) {
    snd_menu(SND_SELECT);
    m->opt_sel = m->sel;
    m->opt_from = MENU_MAKER;
    go(m, MENU_OPTION);
    return;
  }
  /* "Maybe......Hidden Cars?": the hidden cars' select, as a maker's (the port's own) */
  if (hidden && m->screen == MENU_MAKER && m->from < 0) {
    snd_menu(SND_SELECT);
    m->hidden_go = 1;
    m->deciding = FADE;
    m->t = 0;
    return;
  }
  if (back && m->screen == MENU_MAKER && m->from < 0) {
    snd_menu(SND_CANCEL);
    /* back to the save's cars, or the saves (a test without them: the title screen) */
    if (m->save_skip) {
      m->leaving = 1;
      m->deciding = FADE;
    } else if (m->n_own) {
      have_enter(m);
    } else {
      save_enter(m);
    }
    return;
  }
  /* the car setting goes back to the save's cars: the car is one of them by now */
  if (back && m->screen == MENU_SETTING && m->from < 0 && !m->save_skip && !m->new_card && m->own_sel >= 0) {
    snd_menu(SND_CANCEL);
    garage_enter(m, m->own_sel);
    return;
  }
  if (back && (m->screen == MENU_ST_LEVEL || m->screen == MENU_ST_RIVAL) && m->from < 0) {
    snd_menu(SND_CANCEL);
    if (m->screen == MENU_ST_RIVAL) {
      go(m, MENU_ST_LEVEL);
      m->sel = m->st_level;
      m->card_t = FADE;
    } else {
      car_select_enter(m, MENU_GAME);
    }
    return;
  }
  if (back && m->screen == MENU_VS_SEL && m->from < 0) {
    snd_menu(SND_CANCEL);
    go(m, MENU_GAME);
    return;
  }
  if (back && (m->screen == MENU_TEN_COURSE || m->screen == MENU_TEN_LEVEL) && m->from < 0) {
    snd_menu(SND_CANCEL); /* SE 0x1b */
    if (m->screen == MENU_TEN_LEVEL) ten_course_enter(m);
    else car_select_enter(m, MENU_GAME);
    return;
  }
  if (back && m->screen > MENU_MAKER && m->screen <= MENU_BGM && m->from < 0 && m->rot_speed == 0.0f && !m->slide && !m->vs.day) {
    snd_menu(SND_CANCEL);
    car_select_enter(m, m->screen - 1);
    return;
  }
  if (m->screen <= MENU_COLOR) {
    if (car_select_step(m, tap, key)) {
      m->deciding = FADE;
      m->t = 0;
    }
    return;
  }
  /* the car setting's wheel key: the dress-up menu (on the kind chosen last; the wheel
   * makers' select on Enkei's, wk_s + 0x10 = 3); its back key, and on a change screen it
   * opened (the cabinet's cancel there): back to it, from the wheel's bar to the makers' */
  if (dress && m->screen == MENU_SETTING && m->from < 0 && !m->new_card) {
    snd_menu(SND_SELECT);
    m->dress_free = 1;
    m->dress_kind = -1;
    m->tg.maker = 3;
    tg_select_open(m);
    return;
  }
  if (m->screen == MENU_SETTING) {
    if (setting_step(m, tap, key)) m->deciding = FADE;
    return;
  }
  if (m->screen == MENU_BGM && m->vs.day) {
    vs_day_step(m, tap, key);
    return;
  }
  if (m->screen == MENU_BGM) {
    if (day_select_step(m, tap, key)) m->deciding = DECIDE_DAY + FADE;
    return;
  }
  if (m->screen == MENU_PRE) {
    if (m->pre_quiet && m->ready && !m->deciding && (m->ending != 2 || m->t >= 60)) m->deciding = 1;
    if (m->t >= PRE_LEN && m->ready && !m->deciding) m->deciding = FADE;
    return;
  }
  if (m->screen == MENU_ST_LEVEL || m->screen == MENU_ST_RIVAL) {
    st_step(m, tap, key);
    return;
  }
  if (m->screen == MENU_ST_TUNE) {
    tune_step(m, tap, key);
    return;
  }
  if (m->screen == MENU_LVUP) {
    lvup_step(m, tap, key);
    return;
  }
  if (m->screen == MENU_VS_STAR) {
    star_step(m, tap, key);
    return;
  }
  if (m->screen == MENU_SHOGO) {
    shogo_step(m, tap, key);
    return;
  }
  if (m->screen == MENU_POEM) {
    poem_step(m);
    return;
  }
  if (m->screen == MENU_BONUS) {
    bonus_step(m, tap, key);
    return;
  }
  if (m->screen == MENU_ST_EFF) {
    eff_step(m, tap, key);
    return;
  }
  if (back && m->dress_free && !m->tg.move && m->card_t >= TG_FADE &&
      (m->screen == MENU_TG_SELECT || m->screen == MENU_TG_MAKER || m->screen == MENU_TG_AERO ||
       m->screen == MENU_TG_PART || m->screen == MENU_TG_WHEEL || m->screen == MENU_TG_VINYL)) {
    snd_menu(SND_CANCEL);
    if (m->screen == MENU_TG_SELECT) {
      tg_free_leave(m);
    } else if (m->screen == MENU_TG_WHEEL && !m->tg.gem) {
      tg_maker_open(m);
    } else {
      tg_select_open(m);
    }
    return;
  }
  if (m->screen == MENU_TG_SELECT) {
    tg_select_step(m, tap, key);
    return;
  }
  if (m->screen == MENU_TG_MAKER) {
    tg_maker_step(m, tap, key);
    return;
  }
  if (m->screen == MENU_TG_GET || m->screen == MENU_TG_AERO) {
    tg_step(m, tap, key);
    return;
  }
  if (m->screen == MENU_TG_PART) {
    tg_part_step(m, tap, key);
    return;
  }
  if (m->screen == MENU_TG_WHEEL) {
    tg_wheel_step(m, tap, key);
    return;
  }
  if (m->screen == MENU_TG_VINYL) {
    tg_vinyl_step(m, tap, key);
    return;
  }
  if (m->screen == MENU_TG_WEAR) {
    tg_wear_step(m, tap);
    return;
  }
  if (m->screen == MENU_TEN_LEVEL) {
    ten_level_step(m, tap, key);
    return;
  }
  if (m->screen == MENU_TEN_COURSE) m->card_t++;
  int ten = m->screen == MENU_TEN_COURSE;
  int spin = m->screen == MENU_MODE || m->screen == MENU_GAME || m->screen == MENU_VS_SEL;
  int move_len = spin ? MOVE_MODE : MOVE_COURSE;
  int count = m->screen == MENU_GAME     ? GAME_SLOTS
              : m->screen == MENU_VS_SEL ? MENU_VS_KINDS
              : m->screen == MENU_MODE   ? 2
              : ten                      ? TEN_COURSES
                                         : N_COURSES;
  if (m->from >= 0) {
    if (m->t >= move_len) { m->from = -1; m->t = 0; }
    return;
  }
  if (!tap && !key) return;
  int d = key ? key : tap_dir(m);
  if (d == 0) {
    snd_menu(SND_SELECT);
    m->deciding = (m->screen == MENU_COURSE || ten ? DECIDE_COURSE : DECIDE_MODE) + FADE;
    m->t = 0;
  } else {
    int to = m->sel + (d > 0 ? 1 : -1);
    if (to < 0 || to >= count) return;
    snd_menu(SND_CURSOR);
    if (m->screen == MENU_COURSE || ten) {
      /* the spin box's place on screen: going right it stays at 3 from the fourth card on
       * till the ninth (the 10-in-a-row's: the seventh), going left at 2 from the third card on */
      m->course_scroll_from = m->sel - m->course_pos;
      if (d > 0 && !(m->course_pos > 2 && m->sel <= (ten ? TEN_COURSES - 4 : 8))) m->course_pos++;
      if (d < 0 && (m->course_pos > 2 || m->sel <= 2)) m->course_pos--;
    }
    m->from = m->sel;
    m->sel = to;
    m->t = 0;
  }
}

int menu_update(menu_state *m, float dt) {
  m->acc += dt;
  if (m->acc > 0.25f) m->acc = 0.25f;
  while (m->acc >= 1.0f / 60.0f) {
    m->acc -= 1.0f / 60.0f;
    int was_deciding = m->deciding;
    step(m);
    if (was_deciding == 1 && m->deciding == 0) {
      if (m->leaving) {
        /* out of the white, the attract loop again from its title screen */
        m->leaving = 0;
        go(m, MENU_TITLE);
        m->flash = FLASH_LEN;
      } else if (m->screen == MENU_SAVE && m->save_pick_new) {
        /* a NEW GAME asks the player's name first */
        name_enter(m);
      } else if (m->screen == MENU_SAVE || m->screen == MENU_NAME) {
        /* a save with cars asks which; without, a new car */
        if (m->n_own) {
          have_enter(m);
        } else {
          car_select_enter(m, MENU_MAKER);
          m->bars = 1;
        }
      } else if (m->screen == MENU_HAVE && m->keep_ask) {
        /* the new car kept (its card already saved), or taken out and the car select again */
        m->keep_ask = 0;
        if (m->have_sel == 0) {
          car_select_enter(m, MENU_GAME);
        } else {
          keep_drop(m);
          car_select_enter(m, MENU_COLOR);
          m->bars = 1;
        }
      } else if (m->screen == MENU_HAVE) {
        if (m->have_sel == 0) {
          garage_enter(m, m->own_sel);
        } else {
          car_select_enter(m, MENU_MAKER);
          m->bars = 1;
        }
      } else if (m->screen == MENU_GARAGE) {
        menu_own_store(m);
        menu_own_load(m, m->own_view);
        m->tune_locked = 1;
        m->new_card = 0;
        card_enter(m, MENU_GARAGE);
      } else if (m->screen == MENU_CARD) {
        /* (a new card's setting came first: whether to keep it, then on) */
        if (m->card_back == MENU_SETTING) keep_enter(m);
        else car_select_enter(m, MENU_SETTING);
      } else if (m->screen == MENU_MAKER) {
        int to = m->hidden_go ? CARDB_HIDDEN : m->sel;
        if (m->hidden_go) m->hidden_from = m->sel;
        m->hidden_go = 0;
        if (to != m->maker) m->car = 0, m->color = 0;
        m->maker = to;
        car_select_enter(m, MENU_CAR);
      } else if (m->screen == MENU_CAR) {
        if (m->sel != m->car) m->color = 0;
        m->car = m->sel;
        m->all_parts = 0;
        car_select_enter(m, MENU_COLOR);
      } else if (m->screen == MENU_COLOR) {
        m->color = m->sel;
        /* the new car's card (a test without the saves: straight on) */
        if (m->save_skip) {
          own_register(m);
          car_select_enter(m, MENU_SETTING);
        } else if (menu_own_find(m, m->maker, m->car) < 0) {
          /* a new kind: its setting first, stock to start with, then its card */
          m->new_card = 1;
          m->tune_locked = 0;
          m->tune_full[m->maker][m->car] = 0, m->tune_power[m->maker][m->car] = CAR_TUNE_MAX;
          car_select_enter(m, MENU_SETTING);
        } else {
          own_register(m);
          card_enter(m, MENU_COLOR);
        }
      } else if (m->screen == MENU_SETTING) {
        if (m->new_card) {
          own_register(m);
          card_enter(m, MENU_SETTING);
        } else {
          car_select_enter(m, MENU_GAME);
        }
      } else if (m->screen == MENU_GAME) {
        m->game = GAME_AT[m->sel];
        /* the versus shows the save's name, as its card does (a save from before the names
         * stays PLAYER n there: nothing asked, Allan 10-26) */
        if (m->game == MENU_GAME_VS) {
          go(m, MENU_VS_SEL);
        } else if (m->game == MENU_GAME_STORY) {
          st_level_enter(m);
        } else if (m->game == MENU_GAME_TEN) {
          ten_course_enter(m);
        } else {
          car_select_enter(m, MENU_MODE);
        }
      } else if (m->screen == MENU_VS_SEL) {
        m->vs.kind = m->sel;
        go(m, MENU_VS);
        m->bars = 1;
        m->vs.held = 0;
      } else if (m->screen == MENU_MODE) {
        m->official = m->sel;
        go(m, MENU_COURSE);
        /* the first card, at the strip's first place (the last visit's place left a gap of
         * empty places before it, Allan 10-25) */
        m->course_pos = 0;
      } else if (m->screen == MENU_COURSE) {
        m->course = m->sel;
        go(m, MENU_BGM);
        m->sel = m->bgm;
        m->slide = m->card_t = 0;
        m->bars = 1;
      } else if (m->screen == MENU_TEN_COURSE) {
        m->ten_course = m->sel;
        ten_level_enter(m);
      } else if (m->screen == MENU_TEN_LEVEL) {
        /* the level's time zone (clEffTenCourseType::getDayTime, as the title page's) and a
         * song at random */
        m->ten_level = m->sel;
        m->zone = ten_night(m->ten_course, m->ten_level);
        go(m, MENU_PRE);
        m->song = 1 + rand() % (BGM_LINES - 2);
      } else if (m->screen == MENU_ST_LEVEL) {
        m->st_level = m->sel;
        st_rival_enter(m);
      } else if (m->screen == MENU_ST_RIVAL) {
        /* the episode's race: its start, its rivals; a song at random (there is no BGM
         * select on the way) */
        m->story_n = 20 * st_division(m) + 5 * m->st_level + m->sel;
        m->story_pick = st_index(st_loop(m), m->story_n);
        if (m->story_pick > 111) m->story_pick = 111;
        go(m, MENU_PRE);
        m->song = 1 + rand() % (BGM_LINES - 2);
      } else if (m->screen == MENU_ST_TUNE) {
        /* the part going on (seqStTuneEff) */
        int power = m->tune.choice, p0 = m->st_power[m->maker][m->car];
        tune_done(m);
        eff_open(m, power, p0);
      } else if (m->screen == MENU_ST_EFF) {
        /* on to the game mode select, as from a race (the select's poem, the dress-up screens
         * due first) */
        if (m->poem >= 0) {
          poem_open(m);
        } else if (m->ending == 1) {
          ending_open(m);
        } else if (!tg_due(m)) {
          go(m, MENU_GAME);
          m->sel = game_slot(MENU_GAME_STORY);
          m->slide = m->card_t = 0;
          m->bars = 1;
        }
      } else if (m->screen == MENU_LVUP || m->screen == MENU_SHOGO) {
        back_from_race(m, 0);
      } else if (m->screen == MENU_TG_GET) {
        /* a wheel, an aero set, a bonnet, wing or mirror: on to its change (with a choice to
         * make), else the next one due */
        if (m->tg.kind == 0) tg_wheel_open(m);
        else if (m->tg.kind == 1) tg_aero_open(m);
        else if ((m->tg.kind >= 2 && m->tg.kind <= 4) || tg_neon_kind(m->tg.kind)) tg_part_open(m);
        else if (m->tg.kind == 5) tg_vinyl_open(m);
        else if (!tg_due(m)) tg_leave(m);
      } else if (m->screen == MENU_TG_AERO) {
        /* seqTgWearParts after the change (the choice: Refuse to change keeps the one worn) */
        int old = m->aero[m->maker][m->car];
        tg_aero_done(m);
        if (!tg_kept(m, old, m->aero[m->maker][m->car])) tg_wear_open(m, old, m->aero[m->maker][m->car]);
      } else if (m->screen == MENU_TG_WHEEL) {
        int old = m->wheel[m->maker][m->car];
        tg_wheel_done(m);
        if (!tg_kept(m, old, m->wheel[m->maker][m->car])) tg_wear_open(m, old, m->wheel[m->maker][m->car]);
      } else if (m->screen == MENU_TG_VINYL) {
        int old = m->vinyl[m->maker][m->car];
        tg_vinyl_done(m);
        if (!tg_kept(m, old, m->vinyl[m->maker][m->car])) tg_wear_open(m, old, m->vinyl[m->maker][m->car]);
      } else if (m->screen == MENU_TG_PART) {
        int old = *tg_part_value(m);
        tg_part_done(m);
        if (!tg_kept(m, old, *tg_part_value(m))) tg_wear_open(m, old, *tg_part_value(m));
      } else if (m->screen == MENU_TG_WEAR) {
        if (m->dress_free) tg_select_open(m);
        else if (!tg_due(m)) tg_leave(m);
      } else if (m->screen == MENU_TG_SELECT) {
        tg_select_done(m);
      } else if (m->screen == MENU_TG_MAKER) {
        m->tg.maker = WM_MAKER[m->sel];
        tg_wheel_open_at(m, m->tg.maker);
      } else if (m->screen == MENU_BGM) {
        go(m, MENU_PRE);
      } else if (m->screen == MENU_PRE) {
        return 1;
      }
    }
  }
  return 0;
}

/* frame of the layout animation for the current state */
static int layout_frame(const menu_state *m, int *arrow) {
  *arrow = -1;
  if (m->screen == MENU_MODE || m->screen == MENU_GAME || m->screen == MENU_VS_SEL) {
    if (m->from >= 0)
      return m->from < m->sel ? m->from * 100 + 91 + m->t : m->sel * 100 + 94 - m->t;
    if (m->deciding) {
      int t = DECIDE_MODE + FADE - m->deciding;
      return m->sel * 100 + 60 + (t < DECIDE_MODE ? t : DECIDE_MODE - 1);
    }
    return m->sel * 100 + m->t % 60;
  }
  int k = m->sel;
  if (m->from >= 0)
    return m->from < k ? m->from * 100 + 90 + m->t : k * 100 + 99 - m->t;
  if (m->deciding) {
    int t = DECIDE_COURSE + FADE - m->deciding;
    return k * 100 + 61 + (t < DECIDE_COURSE ? t : DECIDE_COURSE - 1);
  }
  /* the arrows by the card's place on screen (the first has none to its left, the last none
   * to its right) */
  static const int ARROW_AT[6] = { 0, 100, 200, 300, 400, 600 };
  *arrow = ARROW_AT[m->course_pos >= 0 && m->course_pos < 6 ? m->course_pos : 3] + 10 + m->t % 40;
  return k * 100 + (m->screen == MENU_TEN_COURSE); /* menu_layout_or_course rests on k*100 + 1 */
}

void menu_draw(const menu_state *m, float screen_w, float screen_h, void (*draw3d)(void *), void *ctx,
               void (*mini3d)(void *, float)) {
  photo_bg = 0;
  switch (m->screen) {
  case MENU_MOVIE: /* the movie view covers the screen */
    return;
  case MENU_TITLE: {
    int t = m->t;
    draw_title(t < TITLE_LEN ? t : TITLE_LEN - 1, t < TITLE_LEN ? 1.0f : 1.0f - (float)(t - TITLE_LEN) / FADE);
    if (m->flash) anm_fill(1, 1, 1, (float)m->flash / FLASH_LEN);
    return;
  }
  case MENU_RANKING:
    draw_ranking(m);
    return;
  case MENU_LOGO:
    anm_draw(ID.maker[m->t / LOGO_LEN < 3 ? m->t / LOGO_LEN : 2], m->t % LOGO_LEN, 1.0f);
    return;
  case MENU_PROLOGUE:
    anm_draw(ID.prologue[m->t < PROLOGUE_LEN ? 0 : 1], m->t % PROLOGUE_LEN, 1.0f);
    return;
  case MENU_START:
    anm_fill(1, 1, 1, m->t < 6 ? (float)(m->t + 1) / 6.0f : 1.0f);
    return;
  }
  float fade = 1.0f;
  if (m->deciding && m->deciding < FADE) fade = (float)m->deciding / FADE;
  /* the backgrounds keep running across screens; the bars slide in when the menus open */
  static int clock;
  clock++;
  int bars = m->bars ? 188 + m->bars : 0;
  if (m->screen <= MENU_COLOR) {
    /* the car select fades as the arcade's clMenuFade does, to black over everything */
    draw_car_select(m, clock, draw3d, ctx);
    draw_topbar(bars, 1, NULL);
    if (!bars) anm_draw(m->screen == MENU_MAKER ? ID.mes_maker : m->screen == MENU_CAR ? ID.mes_car : ID.mes_color, 0, 1);
    anm_draw(ID.bottom, bars, 1);
    /* cars on show still loading: none of them yet, NOW LOADING (its pop-in, then held) */
    if (m->loading) anm_draw_xf(ID.loading, m->loading - 1, 1, LOADING_DX, LOADING_DY, 1, NULL, 0);
    if (!m->deciding) hud_draw_menu_keys(screen_w, screen_h, m->key_held, m->screen == MENU_CAR, 1);
    if (!m->deciding && m->screen == MENU_MAKER) {
      hud_draw_menu_option(screen_w, screen_h, m->key_held == HUD_MENU_OPTION);
      /* the hidden cars key, its caption centred on it (the bottom bar's middle: 320, 425) */
      hud_draw_menu_hidden(screen_w, screen_h, m->key_held == HUD_MENU_HIDDEN);
      int cap = anm_sprite(OPT_TEXT + O_HIDDEN);
      if (cap >= 0) anm_draw_sprite(cap, 320.0f - 0.5f * anm_sprite_width(cap), 415.0f, 1);
    }
    if (!m->deciding && all_parts_shown(m)) {
      /* the ALL PARTS key: held down while on */
      hud_draw_menu_hidden(screen_w, screen_h, m->all_parts || m->key_held == HUD_MENU_HIDDEN);
      int cap = anm_sprite(OPT_TEXT + (m->all_parts ? O_ALL_ON : O_ALL_OFF));
      if (cap >= 0) anm_draw_sprite(cap, 320.0f - 0.5f * anm_sprite_width(cap), 415.0f, 1);
    }
    /* leaving for the title screen, it fades to white instead */
    float c = m->leaving ? 1.0f : 0.0f;
    anm_fill(c, c, c, 1.0f - fade);
    if (m->flash) anm_fill(1, 1, 1, (float)m->flash / FLASH_LEN);
    if (m->screen == MENU_COLOR) setting_warm();
    return;
  }
  if (m->screen == MENU_HAVE) {
    if (m->t < FADE && !m->deciding) fade = (float)(m->t + 1) / FADE;
    if (m->deciding) {
      int t = DECIDE_DAY + FADE - m->deciding;
      fade = t < DECIDE_DAY ? 1.0f : (float)m->deciding / FADE;
    }
    draw_have(m, clock, fade, bars);
    if (!m->deciding) hud_draw_menu_keys(screen_w, screen_h, m->key_held, 1, 1);
    return;
  }
  if (m->screen == MENU_GARAGE) {
    if (m->t < FADE && !m->deciding) fade = (float)(m->t + 1) / FADE;
    draw_garage(m, clock, draw3d, ctx, bars);
    /* its car still loading: NOW LOADING, as the car select's (a blank turntable before, Allan 10-25) */
    if (m->loading) anm_draw_xf(ID.loading, m->loading - 1, 1, LOADING_DX, LOADING_DY, 1, NULL, 0);
    if (!m->deciding) hud_draw_menu_keys(screen_w, screen_h, m->key_held, m->n_own > 1, 1);
    if (!m->deciding) hud_draw_menu_erase(screen_w, screen_h, m->key_held == HUD_MENU_OPTION, m->erase_armed);
    anm_fill(0, 0, 0, 1.0f - fade);
    return;
  }
  if (m->screen == MENU_CARD) {
    if (m->t < FADE && !m->deciding) fade = (float)(m->t + 1) / FADE;
    draw_card(m, clock, 1, bars);
    if (!m->deciding) hud_draw_menu_keys(screen_w, screen_h, m->key_held, 0, 1);
    anm_fill(0, 0, 0, 1.0f - fade);
    return;
  }
  if (m->screen == MENU_NAME) {
    if (m->t < FADE && !m->deciding) fade = (float)(m->t + 1) / FADE;
    draw_name(m, clock, bars);
    if (!m->deciding) hud_draw_menu_keys(screen_w, screen_h, m->key_held, 1, 1);
    anm_fill(0, 0, 0, 1.0f - fade);
    return;
  }
  if (m->screen == MENU_INFO) {
    draw_info(clock, bars);
    hud_draw_menu_keys(screen_w, screen_h, m->key_held, 1, 1);
    anm_fill(0, 0, 0, 1.0f - fade);
    return;
  }
  if (m->screen == MENU_SAVE) {
    /* out of START's white; to black for the maker select, to white for the title screen */
    draw_save(m, clock, bars);
    if (!m->deciding) hud_draw_menu_keys(screen_w, screen_h, m->key_held, 1, 1);
    if (!m->deciding && !m->save_confirm) hud_draw_menu_option(screen_w, screen_h, m->key_held == HUD_MENU_OPTION);
    float c = m->leaving ? 1.0f : 0.0f;
    anm_fill(c, c, c, 1.0f - fade);
    if (m->flash) anm_fill(1, 1, 1, (float)m->flash / FLASH_LEN);
    return;
  }
  if (m->screen == MENU_LAYOUT) {
    if (m->t < FADE) fade = (float)(m->t + 1) / FADE;
    draw_layout(m, clock, screen_w, screen_h);
    hud_draw_menu_keys(screen_w, screen_h, m->key_held, 0, 1);
    anm_fill(0, 0, 0, 1.0f - fade);
    return;
  }
  if (m->screen == MENU_OPTION) {
    if (m->t < FADE) fade = (float)(m->t + 1) / FADE;
    draw_option(m, clock, bars);
    if (m->res_row) draw_option_res(m, screen_w, screen_h);
    draw_option_layout_key(screen_w, screen_h);
    hud_draw_menu_keys(screen_w, screen_h, m->key_held, m->gyro, 1);
    anm_fill(0, 0, 0, 1.0f - fade);
    return;
  }
  if (m->screen == MENU_SETTING) {
    /* out of the colour select's black, and back into it */
    if (m->t < FADE && !m->deciding) fade = (float)(m->t + 1) / FADE;
    draw_setting(m, clock, draw3d, ctx, bars);
    if (!m->deciding) hud_draw_menu_keys(screen_w, screen_h, m->key_held, m->tune_full[m->maker][m->car], 1);
    if (!m->deciding && !m->new_card) hud_draw_menu_dress(screen_w, screen_h, m->key_held == HUD_MENU_OPTION);
    anm_fill(0, 0, 0, 1.0f - fade);
    if (m->flash) anm_fill(1, 1, 1, (float)m->flash / FLASH_LEN);
    setting_warm();
    return;
  }
  if (m->screen == MENU_BGM) {
    /* the screen fades in from the course select's black */
    if (m->t < FADE && !m->deciding) fade = (float)(m->t + 1) / FADE;
    draw_day_select(m, clock, fade, bars);
    if (!m->deciding && !(m->vs.day && m->vs.day_voted)) hud_draw_menu_keys(screen_w, screen_h, m->key_held, 1, !m->vs.day);
    if (m->vs.day) draw_vs_omakase(m, screen_w, screen_h);
    if (m->flash) anm_fill(1, 1, 1, (float)m->flash / FLASH_LEN); /* back from the race's pause menu */
    return;
  }
  if (m->screen == MENU_ST_LEVEL || m->screen == MENU_ST_RIVAL) {
    /* both fade in from the black the screen before faded to (card_t: frames on the screen) */
    if (m->card_t < FADE && !m->deciding) fade = (float)(m->card_t + 1) / FADE;
    if (m->screen == MENU_ST_LEVEL) draw_st_level(m, clock, fade, bars);
    else draw_st_rival(m, clock, fade, bars);
    if (!m->deciding) hud_draw_menu_keys(screen_w, screen_h, m->key_held, 1, 1);
    if (m->flash) anm_fill(1, 1, 1, (float)m->flash / FLASH_LEN);
    return;
  }
  if (m->screen == MENU_ST_TUNE) {
    if (m->card_t < FADE && !m->deciding) fade = (float)(m->card_t + 1) / FADE;
    draw_st_tune(m, clock, fade, bars);
    if (!m->deciding) hud_draw_menu_keys(screen_w, screen_h, m->key_held, !m->tune.fixed, 0);
    return;
  }
  if (m->screen == MENU_LVUP) {
    draw_lvup(m);
    return;
  }
  if (m->screen == MENU_VS_STAR) {
    draw_vs_star(m, draw3d, ctx);
    return;
  }
  if (m->screen == MENU_SHOGO) {
    draw_shogo(m, clock);
    return;
  }
  if (m->screen == MENU_POEM) {
    draw_poem(m);
    return;
  }
  if (m->screen == MENU_BONUS) {
    draw_bonus(m, clock);
    return;
  }
  if (m->screen == MENU_ST_EFF) {
    draw_st_eff(m, draw3d, ctx);
    return;
  }
  if (m->screen == MENU_TG_GET) {
    /* clMenuFade: in and out over 10 frames */
    fade = m->deciding ? (float)m->deciding / TG_FADE : m->card_t < TG_FADE ? (float)(m->card_t + 1) / TG_FADE : 1.0f;
    draw_tg_get(m, clock, 1, draw3d, ctx);
    anm_fill(0, 0, 0, 1.0f - fade);
    return;
  }
  if (m->screen == MENU_TG_PART) {
    /* clMenuFade: in over 10 frames, out over 10 */
    fade = m->deciding ? (float)m->deciding / TG_FADE : m->card_t < TG_FADE ? (float)(m->card_t + 1) / TG_FADE : 1.0f;
    draw_tg_part(m, clock, 1, bars, draw3d, ctx);
    if (!m->deciding) hud_draw_menu_keys(screen_w, screen_h, m->key_held, m->tg.n > 1, m->dress_free);
    anm_fill(0, 0, 0, 1.0f - fade);
    return;
  }
  if (m->screen == MENU_TG_WHEEL) {
    fade = m->deciding ? (float)m->deciding / TG_FADE : m->card_t < TG_FADE ? (float)(m->card_t + 1) / TG_FADE : 1.0f;
    draw_tg_wheel(m, 1, bars, draw3d, ctx);
    if (!m->deciding) hud_draw_menu_keys(screen_w, screen_h, m->key_held, m->tg.n > 1, m->dress_free);
    anm_fill(0, 0, 0, 1.0f - fade);
    return;
  }
  if (m->screen == MENU_TG_VINYL) {
    fade = m->deciding ? (float)m->deciding / TG_FADE : m->card_t < TG_FADE ? (float)(m->card_t + 1) / TG_FADE : 1.0f;
    draw_tg_vinyl(m, 1, bars, draw3d, ctx);
    if (!m->deciding) hud_draw_menu_keys(screen_w, screen_h, m->key_held, m->tg.n > 1, m->dress_free);
    anm_fill(0, 0, 0, 1.0f - fade);
    return;
  }
  if (m->screen == MENU_TG_WEAR) {
    /* clMenuFade: in over 10 frames (the aero set's 20), out over 10 */
    int in = tg_wear_in(m), out = -m->tg.left - WEAR_WAIT;
    fade = m->card_t < in ? (float)(m->card_t + 1) / (float)in : out > 0 ? 1.0f - (float)out / TG_FADE : 1.0f;
    draw_tg_wear(m, clock, draw3d, ctx);
    anm_fill(0, 0, 0, 1.0f - (fade < 0 ? 0 : fade));
    if (m->tg.white_from >= 0) {
      int f = m->tg.white_from + m->card_t - m->tg.white_at;
      if (f <= m->tg.white_from + 29) anm_draw(ID.whiteout, f, 1);
      else if (!m->tg.white_from) anm_draw(ID.whiteout, 29, 1);
    }
    return;
  }
  if (m->screen == MENU_TG_SELECT || m->screen == MENU_TG_MAKER) {
    /* clMenuFade: in over 10 frames; out over 10 after the decision's frames */
    fade = m->deciding ? (m->deciding < TG_FADE ? (float)m->deciding / TG_FADE : 1.0f)
                       : m->card_t < TG_FADE ? (float)(m->card_t + 1) / TG_FADE : 1.0f;
    if (m->screen == MENU_TG_SELECT) draw_tg_select(m, bars, draw3d, ctx);
    else draw_tg_maker(m, bars);
    if (!m->deciding) hud_draw_menu_keys(screen_w, screen_h, m->key_held, 1, 1);
    anm_fill(0, 0, 0, 1.0f - fade);
    return;
  }
  if (m->screen == MENU_TG_AERO) {
    if (m->card_t < TG_FADE && !m->deciding) fade = (float)(m->card_t + 1) / TG_FADE;
    draw_tg_aero(m, clock, 1, bars, draw3d, ctx);
    if (!m->deciding) hud_draw_menu_keys(screen_w, screen_h, m->key_held, m->tg.n > 1, m->dress_free);
    anm_fill(0, 0, 0, 1.0f - fade);
    return;
  }
  if (m->screen == MENU_TEN_LEVEL) {
    if (m->card_t < FADE && !m->deciding) fade = (float)(m->card_t + 1) / FADE;
    draw_ten_level(m, clock, fade, bars);
    if (!m->deciding) hud_draw_menu_keys(screen_w, screen_h, m->key_held, 1, 1);
    return;
  }
  if (m->screen == MENU_PRE) {
    if (m->t < FADE && !m->deciding) fade = (float)(m->t + 1) / FADE;
    if (m->pre_quiet) { /* the race's fade colour (black for none), no NOW LOADING */
      float g = m->pre_fade == 2 ? 1.0f : 0.0f;
      anm_fill(g, g, g, 1.0f);
      /* but the ending's clSeqNowLoading */
      if (m->ending == 2 && ID.loading >= 0) {
        int f = m->t + 1;
        float a = f > LOADING_POP ? 0.725f + 0.275f * cosf((float)(f - LOADING_POP) * (2.0f * (float)M_PI / 60.0f)) : 1.0f;
        anm_draw_xf(ID.loading, f < LOADING_POP ? f : LOADING_POP, a, LOADING_PRE_DX, 0, 1, NULL, 0);
      }
      return;
    }
    draw_pre(m, fade);
    /* the course and the car still loading: NOW LOADING bottom left (LOADING_PRE_DX) */
    int f = m->loading ? m->loading : m->load_shown ? m->load_shown + m->load_out : 0;
    if (f) {
      float a = m->loading ? 1.0f : 1.0f - (float)m->load_out / LOADING_OUT;
      if (f > LOADING_POP) a *= 0.725f + 0.275f * cosf((float)(f - LOADING_POP) * (2.0f * (float)M_PI / 60.0f));
      anm_draw_xf(ID.loading, f < LOADING_POP ? f : LOADING_POP, a * fade, LOADING_PRE_DX, 0, 1, NULL, 0);
    }
    return;
  }
  if (m->screen == MENU_VS) {
    draw_vs(m, clock, bars, screen_w, screen_h);
    return;
  }
  if (m->screen == MENU_VS_AREA) {
    draw_vs_area(m, clock, bars, screen_w, screen_h, mini3d);
    return;
  }
  if (m->screen == MENU_VS_MATCH) {
    draw_vs_match(m, draw3d, ctx);
    return;
  }
  draw_back(ID.bg, clock % 600, fade);
  anm_draw(ID.line, 0, fade);
  draw_bg_logo(chosen_car(m, m->car), clock % 240, fade);
  int arrow;
  int f = layout_frame(m, &arrow);
  if (m->screen == MENU_GAME) {
    anm_draw(ID.game_image, f, fade);
    anm_draw(ID.game, f, fade);
    if (m->from < 0 && !m->deciding && m->t < 60) anm_draw(ID.game_ring, m->sel * 100 + m->t, fade);
  } else if (m->screen == MENU_VS_SEL) {
    /* the versus' discs in the story's, 10 out run's and time attack's places, the ghost
     * versus battle's picture behind each, the port's lines under it */
    static const int ICON[MENU_VS_KINDS] = { 2825, 2824, 2826 }, PIC[MENU_VS_KINDS] = { 3224, 3223, 3225 },
                     LINE[MENU_VS_KINDS] = { 2317, 2316, 2318 };
    anm_swap sw[2 * MENU_VS_KINDS];
    for (int k = 0; k < MENU_VS_KINDS; ++k) {
      sw[2 * k] = (anm_swap){ anm_sprite(PIC[k]), anm_sprite(3226) };
      sw[2 * k + 1] = (anm_swap){ anm_sprite(LINE[k]), anm_sprite(VS_LINE + k) };
    }
    anm_draw_ex(ID.vs_sel_image, f, fade, 0, 0, sw, 2 * MENU_VS_KINDS);
    for (int k = 0; k < MENU_VS_KINDS; ++k) sw[k] = (anm_swap){ anm_sprite(ICON[k]), anm_sprite(VS_ICON + k) };
    anm_draw_ex(ID.vs_sel, f, fade, 0, 0, sw, MENU_VS_KINDS);
    if (m->from < 0 && !m->deciding && m->t < 60) anm_draw(ID.vs_sel_ring, m->sel * 100 + m->t, fade);
  } else if (m->screen == MENU_MODE) {
    anm_draw_ex(ID.mode, f, fade, 0, 0, mode_cars, 2);
    if (m->from < 0 && !m->deciding && m->t < 60) anm_draw(ID.mode_ring, m->sel * 100 + m->t, fade);
  } else {
    /* clThread_6_2_CourseSelect::mapChange (the 10-in-a-row's clThread_7_1_CourseSelect::
     * mapChange, by tenCourseIDFromMenuOrder's id: 8 Hakone, 9 Fukuoka, cards 9 and 8): the
     * city's map for cards 6..9, Nagoya, Osaka, Fukuoka, Hakone; the others' (Tokyo's, the time
     * attack's 10 and 11 too; flag 0x3c / 0x38 clear) clMiniatureCourse::update, the 3D map */
    static const int CITY_MAP[4] = { 0x1832, 0x15b9, 0x19a4, 0x1831 };
    int city = m->sel >= 6 && m->sel <= 9;
    if (mini3d && !city) mini3d((void *)m, fade);
    if (city && m->from < 0 && ID.course_map >= 0) {
      anm_swap map = { anm_sprite(0x15b9), anm_sprite(CITY_MAP[m->sel - 6]) };
      anm_draw_ex(ID.course_map, 0, fade, 0, 0, &map, 1);
    }
    if (m->screen == MENU_TEN_COURSE) anm_draw_ex(ID.or_course, f, fade, course_dx(m), 0, NULL, 0);
    else anm_draw_ex(ID.course, f, fade, course_dx(m), 0, km_swaps, km_nswaps);
  }
  if (arrow >= 0) anm_draw(ID.arrow, arrow, fade);
  anm_swap or_mes = { anm_sprite(T_TOPBAR_MES), anm_sprite(S_MES_OR_COURSE) };
  anm_swap vs_mes = { anm_sprite(T_TOPBAR_MES), anm_sprite(VS_TEXT) };
  draw_topbar(bars, fade, m->screen == MENU_TEN_COURSE ? &or_mes : m->screen == MENU_VS_SEL ? &vs_mes : NULL);
  anm_draw(ID.bottom, bars, fade);
  if (!m->deciding) hud_draw_menu_keys(screen_w, screen_h, m->key_held, 1, 1);
  if (m->flash) anm_fill(1, 1, 1, (float)m->flash / FLASH_LEN);
}
