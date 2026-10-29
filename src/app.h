/* Platform-independent game loop; main.m forwards lifecycle, touches and frames. */
#ifndef APP_H
#define APP_H

/* data_dir holds c1.wpk, c1.wcl and fd3s.wpk. Returns 0 on success. */
/* Before app_init: a writable folder for the player's files (records, garage, the last time
 * zone, perf.log) when data_dir is read-only, as the app bundle of an .ipa install. */
void app_save_dir(const char *dir);
/* Before app_init on a screen wider than 4:3 (an iPhone's 3:2): the 1024x768 layout in its
 * middle, edge layout px more on each side; the race's touch keys key_k times as big. It also
 * turns on the iPhone's cuts. Touches then come in the layout's px (x from -edge). */
void app_wide(float edge, float key_k);
/* Before app_wide on a fast device (Android): no iPhone cuts though the screen is wide, the
 * maps drawn further. */
void app_console(void);
int app_init(const char *data_dir, int fb_width, int fb_height, float screen_w, float screen_h);
void app_touch(void *id, int phase, float x, float y);
void app_frame(float dt);
/* The iPad's turn about the screen's normal from level (radians, clockwise as the player
 * sees it positive), for steering by tilt (the OPTION screen's GYRO); main.m each frame. */
void app_tilt(float radians);
/* Steering by tilt is on (main.m reads the gravity only then). */
int app_wants_tilt(void);
/* The start button (a tap on the attract movie): leaves the attract loop if it is up. */
void app_press_start(void);
/* The attract movie to show now (0 atruct, 1 playdemo, 2 tutorial; see main.m), or -1. */
int app_movie(void);
/* The movie shown for app_movie() ended, or its file is missing. */
void app_movie_done(void);
/* The menu or race BGM that should play: a file name in data_dir/sound/bgm (without
 * .m4a), or NULL for none (the attract loop has its own sound). */
const char *app_bgm(void);
/* the BGM's level over its own (1; the story ending's fade out): main.m and main_android.c */
float app_bgm_level(void);
/* Loading on a thread of its own, so the menus keep their frames: after app_init main.m
 * runs app_loader on a new thread with a GL context sharing the app's current there; it
 * never returns. Without it the menu frames load in their spare time. */
void app_loader(void);
/* main.m after each frame's presentRenderbuffer, with the time it waited there: the loading
 * thread runs till the next frame (app_loader) */
void app_frame_done(double present_ms);
/* The app goes to the background (1) or comes back (0): the loading thread stops while it
 * is away, where iOS allows no GL. */
void app_suspend(int away);
/* iOS is short of memory: free what can be loaded again. */
void app_memory_warning(void);
const char *app_error(void);

#endif
