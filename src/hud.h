/* On-screen virtual controls and gauges. Coordinates are in points on a
 * 1024x768 landscape screen, origin top-left. */
#ifndef HUD_H
#define HUD_H

#include "car.h"

enum { TOUCH_BEGAN, TOUCH_MOVED, TOUCH_ENDED };

typedef struct {
  void *left_touch, *right_touch; /* steering buttons */
  void *throttle_touch, *brake_touch;
  int shift_up, shift_down; /* pending presses, consumed by hud_input */
  int camera_pressed, reset_pressed, trans_pressed;
  int pause_pressed; /* the pause button (only it: a tap in the middle paused by mistake) */
  float screen_w, screen_h;
  /* the gyro steering (the OPTION screen's): the iPad turned like a wheel steers, the steer
   * buttons go, the brake takes their place and the shift buttons the brake's. tilt: the
   * turn (radians, + to the right, app_tilt); full: the turn that is full lock */
  int gyro;
  float tilt, full;
  /* the OPTION screen's slider pedals: a pedal's travel is the finger's height on its key
   * (the key's foot 0, its top and above 1); the pedals' last travels */
  int slide;
  float throttle_v, brake_v;
} hud_state;

void hud_init(hud_state *h, float screen_w, float screen_h);
/* A screen wider than 4:3 (an iPhone's 3:2): edge layout px more on each side of the
 * 1024x768 layout (0 on the iPad); the consoles move out to the sides, the race's touch keys
 * too, key_k times their size. */
void hud_wide(float edge, float key_k);
/* A phone's 16:9 (after hud_wide), clear of the touch keys: the meters and the plate under
 * them dy layout px up, the shift gate and the map over it left px up; the top keys either
 * side of the back mirror (hud_wide's row is for the iPhone, without it). */
void hud_lift(float dy, float left);
/* By part (an iPhone's bigger keys): the meters, the name plate and the challenger
 * pill up (layout px), the map with its name and seconds left up, and the shift gate up. */
void hud_lift_parts(float cluster, float plate, float pill, float map, float gate);
/* The driver's class on the plate under the meters: 0 N .. 46 SSS (GetClass's). */
#define HUD_CLASSES 47
void hud_set_class(int cls);
/* And the card's title (src/title.h's id, 0 for 湾岸の新人) and name (namedb.h characters, -1
 * past its end; NULL or none: GUEST), drawn in the menus' glyphs (menu.bin). */
void hud_set_card(int title, const short *name);
/* `id` identifies a finger (the UITouch pointer on iOS). */
void hud_touch(hud_state *h, void *id, int phase, float x, float y);
/* Fill the car input from the controls; edge-triggered buttons are consumed. */
void hud_input(hud_state *h, car_input *in);

int hud_gl_init(void);
/* Loads the arcade gauge sprites from data_dir/hud.bin (after hud_gl_init). */
int hud_load(const char *data_dir);
/* The car's own meters and gear pattern: its CARDB_PACKS index (src/meterdb.h), and the
 * night meters or the morning ones. Loads their sprites from meters.bin. */
void hud_set_car(int pack, int night);
/* The card's meter (clV386CardData::getMeter, the card check's cardcheck_7): 0 the car's own
 * (Normal), 1 For Racing, 2 SPECIAL; from the next hud_set_car. */
void hud_set_meter(int style);
/* First-draws up to n of the HUD's sprites not drawn yet (the car's meters included), for
 * the race's title page; returns how many are left. */
int hud_warm(float screen_w, float screen_h, int n);
/* The car's tune on its plate: horsepower (car_tune_hp) and handling grade (car_tune_grade). */
void hud_set_tune(int hp, int grade);
/* The race's course (car_start's), for the time attack console: its name on the route map,
 * and the map starts afresh. */
void hud_set_course(int course);
/* The minimap of a city (CAR_TOKYO..): navi.bin Tokyo's, navi_<city>.bin the others' */
void hud_set_city(int city);
/* A checkpoint gave time: TIME EXTEND shows from the race's total `total` (car_race). */
void hud_time_extend(float total);
/* The route signs over the car (car_race.sign 1..HUD_SIGNS): the red ones and the caution
 * blink, and their sound repeats while they show. */
#define HUD_SIGNS 14
int hud_sign_blinks(int sign);
/* The course's RECORD TIME and the player's own (YOUR RECORD), in s; 0 shows dashes. */
void hud_set_records(float record, float yours);
/* A story race: the time attack's records and course name are left out. */
void hud_set_story(int story);
/* A story race's rivals by their place (RIVALDB's slot): ECharacter and whether they get a box
 * (src/storyinfo.h's STORYINFO_SHOWN), n of them; sel the story select (the marks over the
 * cars: nsMenuCheck::checkSrotyIrregularRival) */
void hud_set_story_rivals(const int *chara, const unsigned char *shown, int n, int sel);
/* A 10-in-a-row race at the level (0..9, 10 the 32-man extend), -1 for none: clTenConsole's
 * LV, rival count and split times stand in for the time attack's column. */
void hud_set_ten(int level);
/* A versus race (clJoinConsole): the link's other machines' boxes (clRivalInfoConsole1), n of
 * them (0: not a versus), in PCB order as car_set_versus has them: the name's characters
 * (namedb.h's, -1 past the end), the title (src/title.h, 0 the first), the car's name plate
 * (cardb.h's plate), its horsepower and handling grade (car_tune_hp, car_tune_grade), its tune,
 * and whether the box shows the tune's steps (an R35 in the race: changeToStepDisp). With them
 * the place, the cars in the race and the distance left stand in for the time attack's column
 * (car_race vs_rank, vs_gap, remain). seat: the machine's PCB id (0..3), its mark's colour. */
typedef struct {
  short name[5];
  int title, plate, hp, grade, power, handling, steps, seat;
} hud_vs_box;
void hud_set_vs(const hud_vs_box *box, int n);
/* The 10-in-a-row rivals' lines (Cut::clCutIn): `frames` arcade frames of them, from the
 * race's cut-in info (car_race cut_*); hud_draw draws them. */
void hud_ten_cutin(const car_race *r, int frames);
/* A story realtime movie's show (src/storydemo.h's table, -1 none): started with the movie
 * (its pictures put up in GL), one arcade frame of it (1 while it holds the movie still), drawn
 * over the movie, ended with it */
void hud_demo_start(int table);
int hud_demo_step(void);
void hud_demo_draw(const hud_state *h);
void hud_demo_end(void);
/* clGraphicFrame's negaposi (+0x174, neg.cg) as a 10-in-a-row rival is beaten: the 3D scene
 * drawn so far, colours inverted. */
void hud_negate(const hud_state *h);
/* clGraphics' clFader (draw 0x852b118) in white over the whole screen, alpha 0..1 */
void hud_white(const hud_state *h, float alpha);
/* the same in grey (0 black, 1 white) */
void hud_fade(const hud_state *h, float grey, float alpha);
/* The race camera's view and projection (column-major), for the rival's mark over its car */
void hud_rival_view(const float view[16], const float proj[16]);
void hud_draw(const hud_state *h, const car_state *car, const car_input *in, float fps);

/* The player's own touch key layout (the OPTION screen's KEY LAYOUT, menu.c MENU_LAYOUT): the
 * race's keys in three groups, the top row (pause, reset, view, AT/MT), the left side (steer, or
 * the gyro's brake) and the right side (pedals, shift), each scaled about its screen corner by k
 * (HUD_KEY_K_MIN..HUD_KEY_K_MAX, 1 the default) and moved by (dx, dy) layout px */
enum { HUD_KEYS_TOP, HUD_KEYS_LEFT, HUD_KEYS_RIGHT, HUD_KEY_GROUPS };
#define HUD_KEY_K_MIN 0.6f
#define HUD_KEY_K_MAX 1.6f
void hud_layout_set(int group, float k, float dx, float dy);
/* the group whose keys are at (x, y) (with the gyro's keys or the steer buttons'), -1 none */
int hud_layout_hit(int gyro, float x, float y);
/* (dx, dy) brought back so the group, k times its size, stays on the screen */
void hud_layout_clamp(int group, int gyro, float k, float *dx, float *dy);
/* the editor's view: the race's keys as hud_layout_set has them, group sel (-1 none) outlined */
void hud_draw_layout(float screen_w, float screen_h, int gyro, int sel);

/* The menus' left/right keys and back button, in the same style: which one is at (x, y)
 * (-1, +1, HUD_MENU_BACK, or 0), and drawing them (held = the one being pressed, or 0;
 * keys and back say which are on the screen). */
#define HUD_MENU_BACK 2
#define HUD_MENU_OPTION 3 /* the OPTION button (the brand select's, left of back) */
#define HUD_MENU_HIDDEN 4 /* the maker select's hidden cars key, in the bottom bar's middle */
int hud_menu_key(float x, float y);
void hud_draw_menu_keys(float screen_w, float screen_h, int held, int keys, int back);
/* the OPTION button, a gear, in the same style (held: being pressed) */
void hud_draw_menu_option(float screen_w, float screen_h, int held);
/* the car setting's wheel key, in the OPTION button's place (HUD_MENU_OPTION): the
 * dress-up menu */
void hud_draw_menu_dress(float screen_w, float screen_h, int held);
/* the hidden cars key (HUD_MENU_HIDDEN): the button; its caption is the menus' sprite */
void hud_draw_menu_hidden(float screen_w, float screen_h, int held);
/* the save's car view's delete key there, a bin (armed: red, the next press deletes) */
void hud_draw_menu_erase(float screen_w, float screen_h, int held, int armed);

/* The online versus's screen (a port screen, no arcade sprites): panels, buttons and text in
 * a 5x7 font (capitals, digits and . : - / < > ( ) ! ? + #), in the menus' points (1024 x 768,
 * x past EDGE). */
void hud_ui_begin(float screen_w, float screen_h);
void hud_ui_rect(float x, float y, float w, float h, float r, float g, float b, float a);
void hud_ui_button(float x, float y, float w, float h, int held);
/* text with its top left at (x, y), each font pixel px points; returns the x after it */
float hud_ui_text(const char *s, float x, float y, float px, float r, float g, float b, float a);
float hud_ui_text_width(const char *s, float px);
void hud_ui_end(void);

#endif
