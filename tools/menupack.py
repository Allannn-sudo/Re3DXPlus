"""Pack the arcade's menu animations (.anm) and the sprites they use into data/menu.bin.

usage: menupack.py <arcade root (has main, data/)> <out menu.bin>

.anm: u32 nframes, u32 maxrec, nframes * (u32 start, u32 count), then 64-byte records
{i32 sprite id, f32 a,b,c,d (2x2), f32 x,y (top-left on 640x480), i32 mul r,g,b,a (256 = 1),
 i32 add r,g,b,a (255 = 1), i32 blend}: clSpriteAnime::draw hands the sprite manager blend mode 1
for 8 (clDrawBuffer::draw: ONE, ONE, add: the glows and rings stored on black), 3 for 9 (the
same, subtracted), else the default (SRC_ALPHA, ONE_MINUS_SRC_ALPHA). Sprite ids index clImageResourceManager's table in main
(0x9551560, 44-byte entries, +0 = path of the PNG).

menu.bin ("WMN3"):
  u32 nsprites; per sprite: i32 arcade sprite id (TEXT_BASE + k for TEXTS[k]), u16 w, u16 h,
                u32 zsize, then zsize bytes of zlib-packed w*h*4 RGBA, or (zsize 0) the RGBA itself
  u32 nanims;   per anim: char name[48], u32 nframes, u32 nrecs,
                nframes * (u32 start, u32 count),
                nrecs * {u16 sprite (0xffff = none), u16 blend (0, 1 add, 2 subtract), f32 a,b,c,d,x,y,
                         i16 mul[4], i16 add[4]}
"""
import json
import os
import struct
import sys
import zlib

import math

from PIL import Image, ImageDraw, ImageFilter

# (anm name, number of frames kept; None = all)
ANIMS = [
    ("atract_layout_title", None),
    ("menu_g_bg_image", None),
    ("menu_g_bg_line", None),
    ("menu_g_bg_logo", None),
    ("menu_g_topbar_bar", None),
    ("menu_g_bottombar_bar", None),
    ("menu_layout_ta_carsel", None),
    ("menu_layout_ta_carsel_ring", None),
    ("menu_layout_ta_course", None),  # the 12 course cards, 100 frames each
    ("menu_g_course_map", None),      # mapChange: the other cities' map beside cards 6..9
    ("menu_layout_ta_course_arrow", None),
    # attract: the maker logos (seqAtructCi) and the time attack ranking (clRanking)
    ("atract_layout_logo_bng", None),
    ("atract_layout_logo_namco", None),
    ("atract_layout_logo_nvidia", None),
    # after the logos, seqPrologue: "This game is a work of fiction..." and, in Japan, the story
    ("atract_g_02", None),
    ("atract_g_01", None),
    ("menu_g_pre_bg", None),
    ("menu_g_ranking_bg2", None),
    ("menu_layout_ranking", None),
    # the car select: maker, car (on its 3D carousel) and body colour
    ("menu_layout_carsel_maker_", None),
    ("menu_g_carsel_maker_car_image", None),
    ("menu_g_bg_carsel_2", None),
    ("menu_g_carsel_car", None),
    ("menu_layout_carsel_car_arrow", None),
    ("menu_layout_carsel_new", None),
    ("menu_layout_carsel_mes_r35", None),
    ("menu_p_carsel_mes_jzs161", None),
    ("menu_g_carsel_col_01", None),
    ("menu_g_carsel_col_04", None),
    ("menu_g_carsel_col_05", None),
    ("menu_g_carsel_col_06", None),
    ("menu_g_carsel_col_04_arrow", None),
    ("menu_g_carsel_col_05_arrow", None),
    ("menu_g_carsel_col_06_arrow", None),
    ("menu_g_carsel_col_name", None),
    ("menu_g_topbar_mes_carsel_maker", None),
    ("menu_g_topbar_mes_carsel_car", None),
    ("menu_g_topbar_mes_carsel_color", None),
    # the time zone and BGM select (clSeqStdDayTimeSelectThread): the morning and night
    # cards (clMenuSpinBoxStd), the ring flash round the chosen one, and the BGM line
    ("menu_layout_yn_card_1_gold", None),
    ("menu_layout_yn_ring", None),
    ("menu_g_yn_day_sound", None),
    # the versus' (seqJoinDayTimeSelect): the cards higher, the players' dots under them
    ("menu_layout_yn_day_vs", None),
    ("menu_layout_yn_day_vs_ring", None),
    # the race's title page before a time attack (clSeqTaCourseDecideThread, shown while the
    # course loads): menu_g_pre_bg above, the course and time zone, the chosen BGM
    ("menu_layout_pre_ta", None),
    ("menu_g_pre_sound", None),
    # the VS title page's NOW LOADING: the car select shows it while its cars load
    ("menu_layout_pre_vs_loading", None),
    # the end of a time attack: FINISH at the goal (clTaResult), TIME UP (clTaTimeUp)
    ("menu_g_finish_finish", None),
    ("menu_g_finish_timeup_ta", None),
    # clTaResult's rows: the course, the course's, car type's and player's best (with the NEW
    # mark), CONGRATULATIONS, sections 1..4, TOTAL TIME, the top speed, the ranking
    ("menu_g_finish_ta_course", None),
    ("menu_g_finish_ta_total_best", None),
    ("menu_g_finish_ta_car_best", None),
    ("menu_g_parsonal_best", None),
    ("menu_g_finish_ta_congratulations", None),
    ("menu_g_finish_ta_section_1", None),
    ("menu_g_finish_ta_section_2", None),  # sections 5..7 of course ids 12, 13
    ("menu_g_finish_ta_totaltime", None),
    ("menu_g_finish_ta_topspeed", None),
    ("menu_g_finish_ta_ranking", None),
    # the start's countdown (clCountDownConsole): 2, 1, GO at the top, and the manual
    # gearbox's "Put the gear in 2" (setKeepShift, its digit swapped in below)
    ("cons_g_countdown", None),
    ("cons_g_or_men", None),
    ("cons_g_splittime", None),
    ("cons_g_cpu", None),
    # the versus' console (clJoinConsole): the place and the distance left, and a box per other
    # machine (clRivalInfoConsole1; _ph with an R35 in the race)
    ("cons_g_place", None),
    ("cons_g_rival", None),
    ("cons_g_rival_ph", None),
    # the rival's mark over its car (clRivalMarkConsole::clRivalMark), its R swapped in; _tg: a
    # versus machine's (the arrow and its name) and a story's irregular rival's (the arrow)
    ("cons_g_R", None),
    ("cons_g_R_tg", None),
    # the 10-in-a-row's banner as each rival comes on (clTenStartRivalNameDisp): the round
    # (template 0x59f), the rival's name (0x5a0) and its car's name (0x1d2) on a tuning bar
    ("menu_layout_or_demo_0705b", None),
    # the 10-in-a-row's rival lines (Cut::clCutIn, clCutinDisp::setSpeak): the face (template
    # 0x18fc) and the line (font template 0x37d), and the flash across behind them
    ("cons_g_cutin_serifu", None),
    ("cons_g_cutin_effect", None),
    ("cons_g_cutin2_serifu", None),
    ("cons_g_cutin2_effect", None),
    ("cons_g_count_mis_irero", None),
    # the car setting screen (clSeqCarSettingThread): the wheel prompt, Power on the left,
    # Handling on the right
    ("menu_g_setting_handle", None),
    # the game mode select (clSeqModeSelectThread with a card, 4 modes: its spin box, the
    # mode's picture and the ring round it), the story's level and rival select
    # (seqStLevelSelect / seqStRivalSelect) and its result bar
    ("menu_layout_mode_sel", None),
    ("menu_layout_mode_image", None),
    ("menu_layout_mode_ring", None),
    ("menu_layout_st_level", None),
    ("menu_layout_st_level_arrow", None),
    ("menu_layout_st_level_ring", None),
    ("menu_layout_st_level_clear", None),
    ("menu_g_st_lv", None),
    ("menu_layout_st_rival", None),
    ("menu_layout_st_rival_arrow", None),
    ("menu_layout_st_rival_ramp", None),
    ("menu_g_st_rival", None),
    # the level select's line under the five episodes, the rival select's episodes by level
    # (blue, green, orange, red; the 2s centre an even count of title glyphs)
    ("menu_g_st_lv_back", None),
    ("menu_g_st_lv_blue", None),
    ("menu_g_st_lv_green", None),
    ("menu_g_st_lv_orange", None),
    ("menu_g_st_lv_red", None),
    ("menu_g_st_rival_blue", None),
    ("menu_g_st_rival_blue2", None),
    ("menu_g_st_rival_green", None),
    ("menu_g_st_rival_green2", None),
    ("menu_g_st_rival_orange", None),
    ("menu_g_st_rival_orange2", None),
    ("menu_g_st_rival_red", None),
    ("menu_g_st_rival_red2", None),
    # a story race's result (clRaceResultConsole): the advantage row (its first frame), the
    # tuning points gauge's frame, gauge and title / message
    ("menu_g_finish_tg_result", 1),
    ("menu_g_finish_point_waku", None),
    ("menu_g_finish_st_bar", None),
    ("menu_g_finish_st_moji", None),
    ("menu_g_finish_win", None),
    ("menu_g_finish_lose", None),
    ("menu_layout_pre_st", None),
    # the 10-in-a-row (mseqTen): its course cards, the level select's boxes, ring, arrows and
    # faces, its race title page
    ("menu_layout_or_course", None),
    ("menu_layout_or_lv", None),
    ("menu_layout_or_lv_ring", None),
    ("menu_layout_or_lv_arrow", None),
    ("menu_layout_or_lv_face", None),
    ("menu_layout_pre_or", None),
    # its end (clTenResult): WIN or TIME UP, the faces crossed out one by one with the header
    # and the medal, then the summary (course, level, time, a character's line)
    ("menu_g_finish_timeup", None),
    ("menu_layout_or_result_face", None),
    ("menu_g_or_finish_X", None),
    ("menu_layout_or_finish_result", None),
    ("menu_layout_or_result_A", None),
    ("menu_g_pre_sound_st", None),
    # the story's tuning screen (clSeqStTuneThread): its layout and background, the tuner's
    # lines, the gauges' lights and frames, the P and H markers (clMenuGSettingPHAnime)
    ("menu_layout_st_tune", None),
    ("menu_layout_st_tune_serifu", None),
    ("menu_g_bg_image_st_tune", None),
    ("menu_g_setting_gage_1", None),
    ("menu_g_setting_gage_waku", None),
    ("menu_g_setting_gage_waku_2", None),
    ("menu_g_setting_p_h", None),
    # dress-up (the ghost battle's "TG" tuning screens): the GET screen
    # (clSeqTgPartsGetAeroThread) and the aero set's change (clSeqTgChangePartsAeroThread),
    # by its number of choices: 2 _2, 3 _3, 4 (none), 5 _5
    ("menu_g_tg_bg_1", None),
    ("menu_g_tg_bg_3", None),
    ("menu_g_topbar_bar_notime", None),
    ("menu_layout_tg_tune_get_title", None),
    ("menu_layout_tg_tune_get_parts", None),
    ("menu_layout_tg_tune_aero", None),
    ("menu_layout_tg_tune_aero_2", None),
    ("menu_layout_tg_tune_aero_3", None),
    ("menu_layout_tg_tune_aero_5", None),
    # the bonnet's, wing's and mirror's GET screen (clSeqTgPartsGetWingThread) and the wheels'
    ("menu_layout_tg_tune_get_parts_image", None),
    ("menu_layout_tg_tune_get_wheel", None),
    # the bonnet's, wing's and mirror's change (clSeqTgChangePartsBonnet/Wing/MirrorThread)
    # and their arrows (parts_num_to_arrow_anim): the wing's by its number of choices
    ("menu_layout_tg_tune_4_bonnet", None),
    ("menu_layout_tg_tune_4_arrow", None),
    ("menu_layout_tg_tune_5_wing", None),
    ("menu_layout_tg_tune_5_arrow", None),
    ("menu_layout_tg_tune_6_wing", None),
    ("menu_layout_tg_tune_6_arrow", None),
    ("menu_layout_tg_tune_2", None),
    ("menu_layout_tg_tune_2_arrow", None),
    # the neons' change (clSeqTgChangePartsNeonThread) and its arrows
    ("menu_layout_tg_tune_8_neon", None),
    ("menu_layout_tg_tune_8_neon_arrow", None),
    # the stickers' change (clSeqTgChangePartsStickerThread): the bar, the colour column, the
    # arrows; the gem cars' (works colours alone) and their arrows
    ("menu_layout_tg_tune_8_vinyl", None),
    ("menu_layout_tg_tune_8_vinyl_col", None),
    ("menu_layout_tg_tune_vinyl_arrow_8", None),
    ("menu_layout_tg_tune_2_gem", None),
    # the part worn (clSeqTgWearPartsAeroThread): its title, the name's banner, the white flash
    ("menu_layout_tg_tune_get_title_no_lv", None),
    ("menu_layout_tg_tune_install_wheel_logo", None),
    # the wheel change (clSeqTgChangePartsWheelThread): the bar by the maker's number of
    # places (wheel_maker_to_cursor_anim) and its arrows (wheel_maker_to_arrow_anim), the
    # colour column's swatches, cursor, name logo and frame; the level 63 part's COMPLETE
    ("menu_g_tg_tune_wheel_10", None),
    ("menu_g_tg_tune_wheel_5", None),
    ("menu_g_tg_tune_wheel_16", None),
    ("menu_g_tg_tune_wheel_8", None),
    ("menu_g_tg_tune_wheel_11", None),
    ("menu_layout_tg_tune_wheel_arrow_10", None),
    ("menu_layout_tg_tune_wheel_arrow_11", None),
    ("menu_layout_tg_tune_wheel_arrow_5", None),
    ("menu_layout_tg_tune_wheel_arrow_8", None),
    ("menu_layout_tg_tune_wheel_col_col", None),
    ("menu_layout_tg_tune_wheel_col_cursor", None),
    ("menu_layout_tg_tune_wheel_col_logo", None),
    ("menu_layout_tg_tune_wheel_col_waku_2", None),
    ("menu_layout_tg_tune_complete", None),
    # the dress-up menu (clTgDressUpPartsSelect: the places, the gem cars' fewer) and the wheel
    # maker select (clSeqTgWheelMakerThread: the maker's wheel picture, the places)
    ("menu_layout_tg_tune_all", None),
    ("menu_layout_tg_tune_gem_all", None),
    ("menu_layout_wheelmaker_bg", None),
    ("menu_layout_wheelmaker_sel", None),
    ("menu_p_whiteout", None),
    # the card's first page (clSeqCardDisp1): its frame and its fields' fonts
    ("menu_g_cardcheck_1_waku", None),
    ("menu_layout_cardcheck_1", None),
    # the class-up screen (clSeqLvUpThread): its backgrounds and layout
    ("menu_g_shogo_bg_1", None),
    ("menu_g_shogo_bg_2", None),
    ("menu_layout_lvup", None),
    # the story's tuning work (clSeqStTuneEffThread): its top bar, text boxes (power,
    # handling), complete pictures and install animes (src/steff.h)
    ("menu_g_topbar_st_install", None),
    ("menu_layout_st_instal_result", None),
    ("menu_layout_st_instal_result_handle", None),
    ("menu_layout_st_install_complete", None),
    ("menu_layout_st_install_complete2", None),
] + [("menu_layout_st_install_" + n, None) for n in (
    "front_a", "left_a", "left_b", "left_c", "left_d", "left_e", "leftback_a", "leftback_b", "leftback_c",
    "leftfront_a", "leftfront_b", "rightback_a", "rightback_b")]

# The story's realtime movies' shows (src/storydemo.h, tools/storydemo.py): their animes, whose
# sprites are big pictures kept packed (anm.c uploads them for the movie that shows them)
DEMO_ANIMS = sorted({l.split('"')[1] for l in open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                               "..", "src", "storydemo.h"), encoding="utf-8")
                     if l.startswith("  { ") and '"' in l and not l.split('"')[1].endswith("wav")
                     and l.count(",") == 4})
ANIMS += [(n, None) for n in DEMO_ANIMS if (n, None) not in ANIMS]

# Sprites the game swaps into the animations at run time (clSpriteAnime::setImageReplace),
# so they are packed even though no animation names them. Ids index the table in main.
EXTRA = [
    # the card's first page: the top bar's "Card data confirmed (1/9)" and the R35's frame
    0x1DB2, 0x11ED,
    *range(3352, 3362),  # menu_p_ranking_time0..9: rank, time and horsepower digits (0x93f1ec0)
    4062,                # menu_p_ranking_waku_1: the ranking plate
    2718,                # menu_p_car_bariki_b: handling grade B (GetHandling of an untuned car)
    3349,                # menu_p_ranking_rankN: class N
    0x19cd, 0x1ba8,      # menu_p_ranking_title_waku(_2): course name frame (nsTaData::getCourceNameBgSprite)
    # nsTaData::getBgSprite / getCourceNameSprite (0x93e33e0, 0x93e3420) for the ranked
    # course ids 0..4 (menu.c RANKED_COURSES)
    2863, 2866, 2869, 2865, 2870,
    3931, 3932, 3933, 3934, 5609,
    # car names of the default ranking entries (get_car_model_data(model)+0x2c)
    7627, 7801, 7439, 7448, 7636, 7798, 7952, 7905, 7907,
    *range(2239, 2249),  # menu_p_course_no_0..9: the course cards' km (clThread_6_2_CourseSelect)
    # the car select's templates (setImageReplace), in case a strip's frames skip one
    7911, 7910, 7683, 6958, *range(7650, 7662),
    # the time zone select (its stPimpl): the morning and night cards, the gold and
    # ultramarine rings, the top bar's "Select the time zone"
    0x10a2, 0xb93, 0xb95, 0x1b0b, 0x105a,
    # the race's title page (clSeqTaCourseDecideThread): by course id 0..5 the morning and
    # night backgrounds and the course name (nsMenuCheck::getCourseImageTa*, 20-byte records
    # at 0x93ddac0), the time zone's name (getTimeSprite4CourseDecide, 0x93dde34)
    2863, 2866, 2869, 2865, 2870, 2873, 3901, 3904, 3907, 3903, 3908, 3911,
    3931, 3932, 3933, 3934, 3936, 4751, 3301,
    # the race's pause menu, the arcade's continue dialog (clSeqContinueThread): the YES and
    # NO cards, "Continue?", and the box it sits in here (menu_p_maxi_tornament_waku)
    0x13ca, 0x10a1, 0x1ab0, 0x1a75,
    # setKeepShift(n)'s digit for the template 0x1b18: cons_p_mis_irero_1..6
    *range(0x7c9, 0x7cf),
    # clTaResult (nsTaData::setTime7/6): the result's digits (menu_p_finish_ta_0..9) and
    # TOTAL TIME's, the NEW mark (menu_p_finish_ta_overwrite)
    *range(2291, 2301), *range(6219, 6229), 0x1a70,
    # the car setting screen (clSeqCarSettingThread and menu_layout_setting): the gauges'
    # band, Power and Handling, the gauges' lights by ten (s1..s3) and their empty frame,
    # the horsepower digits and HP, the balance badges dark and lit (DG, D, B, G, HG), the
    # course band, the top bar's "Setting"
    3364, 3363, 5695, 7315, 7316, 7317, 6270, *range(5684, 5694), 4067,
    7084, 7085, 6800, 6801, 6798, 6799, 6802, 6803, 7086, 7087, 6804, 6639,
    # the top bar's messages of the dress-up menu and the wheel maker select
    0x1e5a, 0x1ccf,
]

# The story race's title (src/storypre.h, tools/storypre.py): each episode's picture and
# title, its rivals' cars, its ramp's backgrounds; the episode digits and WA.
def _storypre():
    import re
    h = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "storypre.h")).read()
    rows = re.findall(r"^  \{ (\d+), (\d+), (\d+), \{ (\d+), (\d+), (\d+) \}, \{ (\d+), (\d+) \}, (\d+) \},", h, re.M)
    ids = {int(v) for r in rows for v in r[1:]}
    ids |= {int(v) for v in re.search(r"STORYPRE_DIGIT\[10\] = \{ ([^}]*) \}", h).group(1).split(",")}
    return sorted(i for i in ids if i > 1)
STORY_SPRITES = _storypre()
EXTRA += STORY_SPRITES + [0xdb0]

# The story's level and rival select (src/storysel.h, tools/storysel.py): every episode's
# stage picture and its grey (zlib-packed like the title's), the levels' names, the episodes'
# frames by level, the lock, the CLEARED marks, the top bar's messages, the title glyphs
# (menu_p_st_cpu_title_0..9, dai, wa)
def _storysel():
    import re
    h = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "storysel.h")).read()
    body = h[h.index("STORYSEL_IMAGE"):h.index("STORYSEL_TEXT")]
    return sorted({int(v, 0) for v in re.findall(r"\b(?:0x[0-9a-f]+|\d+)\b", body) if int(v, 0) >= 100} - {111})
STORYSEL_SPRITES = _storysel()
EXTRA += STORYSEL_SPRITES + [0x1327, 0x1649, 0x1881, 0x1044, 0x760, 0x75f, 0x716, 0x4b5, 0x1704, 0x1ab2,
                             *range(4153, 4163), 5700, 4902,
                             0x1dbe, 0x1e69, 0x1ed8, 0x1cdd]
# the story title's pictures are drawn once a race, so they are kept zlib-packed (some 230 MB
# raw); only the digits stay raw
ZIPPED = set(STORY_SPRITES) - set(range(3106, 3116))
ZIPPED |= {i for i in STORYSEL_SPRITES if i > 2000}

# Strings the game draws with clSpriteFont, pre-rendered here: (FONT_ID, text).
# The default ranking entries' name (clRankingDefault::data_) and title 1 (youTitle::TitleTableData).
TEXT_BASE = 100000
TEXTS = [
    (8, "\uff27\uff35\uff25\uff33\uff34"),  # ＧＵＥＳＴ
    (1, "\u6e7e\u5cb8\u306e\u65b0\u4eba"),  # 湾岸の新人
]

# The car select (tools/cars.json, from cardb.py): each car's logo, drive, gears and
# background logo sprites, each colour's swatch, and the colour names (font 0, after the
# texts above: src/cardb.h numbers them on from TEXT_BASE + 2).
CARS = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "cars.json")))
for _maker in CARS["makers"]:
    for _car in _maker["cars"]:
        EXTRA += [_car[k] for k in ("logo", "drive", "speed", "bglogo", "plate")]
        EXTRA += [c["swatch"] for c in _car["colors"]]
TEXTS += [(0, t) for t in CARS["texts"]]
# The BGM select of a player without a card: nsMenuCheck's set 0 (counts at 0x93dd760, ids
# at 0x93dd780) holds RANDOM (0x6e), twenty songs and NO BGM (-1). Each line's name
# (getBgmSelectName: nsAudio::getMusicTitle's 12-byte {path, volume, title} records at
# 0x931f4e0) is pre-rendered in font 0 as BGM_TEXT_BASE + its place in the list.
BGM_TEXT_BASE = 101000
BGM_SET, BGM_COUNTS, BGM_MUSIC = 0x93DD780, 0x93DD760, 0x931F4E0
FONT_TABLE = 0x930E4E0  # clSpriteFont::setFontType: 36-byte entries, 11 fonts

TABLE_VA, TABLE_N = 0x9551560, 0x1F9B

# The car setting screen's own choices (menu.c), pre-rendered as SETTING_TEXT_BASE + k.
SETTING_TEXT_BASE = 101100
# The story's rival select's texts (tools/storysel_texts.txt: font, text), as STORYSEL_TEXT + k.
STORYSEL_TEXT_BASE = 101200
STORYSEL_TEXTS = [(int(l.split("\t")[0]), l.split("\t")[1].rstrip("\n"))
                  for l in open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "storysel_texts.txt"), encoding="utf-8") if "\t" in l]

# The story's tuning screen (src/sttune.h, tools/sttune.py): the tuners' names, portraits,
# shops and top bar messages, the parts' pictures, the P and H markers' digits; its texts
# (tools/sttune_texts.txt) as STTUNE_TEXT + k, zlib-packed.
STTUNE_TEXT_BASE = 101300
STTUNE_TEXTS = [(int(l.split("\t")[0]), l.split("\t")[1].rstrip("\n"))
                for l in open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "sttune_texts.txt"), encoding="utf-8") if "\t" in l]
# The part worn's banner (tools/wear_texts.txt, tools/weartexts.py) as WEAR_TEXT_BASE + k.
WEAR_TEXT_BASE = 101800
WEAR_TEXTS = [(int(l.split("\t")[0]), l.split("\t")[1].rstrip("\n"))
              for l in open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "wear_texts.txt"), encoding="utf-8") if "\t" in l]
def _sttune():
    import re
    h = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "sttune.h")).read()
    ids = set()
    for name in ("STTUNE_CHARA", "STTUNE_MID_PARTS"):
        body = h[h.index(name):]
        body = body[:body.index("};")]
        ids |= {int(v) for v in re.findall(r"\b\d+\b", body[body.index("{"):])}
    for r in re.findall(r"^  \{ (\d+), (\d+), -?\d+, \{", h, re.M):
        ids.add(int(r[1]))
    for r in re.findall(r"STTUNE_POWER_(?:NA|24) = \{ \d+, (\d+),", h):
        ids.add(int(r))
    return sorted(i for i in ids if i > 100)
STTUNE_SPRITES = _sttune()
EXTRA += STTUNE_SPRITES + [0x16cb, 0x16d6, 0x1893, 0x1bc2, *range(2392, 2412)]
# the result's tuning messages by tuner (src/sttune.h STTUNE_GET) and the advantage's digits (0x93d0260)
FINISH_ST_SPRITES = [7916, 7958, 7996, 7997, 7998, 7999, 8000, 8022, 8023, 8024, 8038, 8049, *range(1193, 1203)]
EXTRA += FINISH_ST_SPRITES
# dress-up (src/dressdb.h): the GET screen's level digits, kind titles and the aero sets'
# names; the change screen's choices (Original shape, Aero set A..C, the gem cars' Aero set,
# Refuse to change), its plate, its top bar message and tg_bg_1's black stripe (neon)
EXTRA += [*range(5876, 5886), 7128, 7733, 7348, 6823, 7349, 7125, 6822, 6821, 7329, 7330, 7331, 6819,
          0x1bc6, 0x1344, 0x1345, 0x1346, 0xd2a, 0x1e2e, 0x1bc8, 0x1ecf, 0x111f,
          # the change screens' locks (bonnet, wing) and top bar messages (bonnet, wing, mirror)
          0x19dd, 0x1700, 0x1f51, 0x1ed2, 0x1f36]
# every level's part: its name picture and (bonnets, wings, mirrors) its picture
def _dressdb():
    import re
    h = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "dressdb.h")).read()
    ids = set()
    for r in re.findall(r"\{ (-?\d+), -?\d+, (\d+), (\d+) \}", h):
        if int(r[0]) >= 0:
            ids |= {int(r[1]), int(r[2])}
    return sorted(i for i in ids if i > 0)
DRESS_SPRITES = _dressdb()
EXTRA += DRESS_SPRITES
# the wheel change screen (clSeqTgChangePartsWheelThread, src/dressdb.h DRESS_WHEEL_SETS): by
# wheel the bar's picture and the name logo, by colour the swatch and the big picture; the
# bar's lock (menu_p_t_tuning_wheel_no), its NEW marks (new_tmp3..16) and the top bar's messages
def _wheel_sets():
    import re
    h = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "dressdb.h")).read()
    ids = set()
    for v in re.findall(r"^  \{ (\d+), (\d+), (\d+), (\d+), (\d+), (\d+), \{ ([^}]*) \}, \{ ([^}]*) \}, \{ ([^}]*) \} \},$",
                        h.split("DRESS_WHEEL_SETS[]", 1)[1], re.M):
        n = int(v[3])
        ids |= {int(v[4]), int(v[5])}
        ids |= {int(x) for x in v[7].split(",")[:n]} | {int(x) for x in v[8].split(",")[:n]}
    return sorted(ids)
WHEEL_SET_SPRITES = _wheel_sets()
EXTRA += WHEEL_SET_SPRITES + [0x18b5, *range(0x18ad, 0x18b4), *range(0x19df, 0x19e6), 0x1f0a, 0x1f6e]
# the neons' change: the spot neons' pictures, the locks (straight, spot), the top bar's
# messages (straight, spot), the bar white
EXTRA += [*range(0x1bcf, 0x1bd5), 0x16f1, 0x16f3, 0x1ab3, 0x1ab4, 0x19dc]
# the versus' race title page: the time attack's with menu_p_pre_title_vs
EXTRA += [2910]
# the stickers' change: the places' pictures (0x9409220: line 1, fire 1, line 2, fire 2,
# Pac-Man, namco, the makers' works colours, line 3, fire 3, tribal), the lock, the swatches
# (the colours, fire 2's gradations), the NEW marks (0x9409260), the gem cars' works colours
EXTRA += [0x1bde, 0x1bdb, 0x1bdf, 0x1bdc, 0x18cb, 0x1aaa, *range(0x1cc2, 0x1cca), 0x1be0, 0x1bdd, 0x1daf, 0x1702,
          *range(0x1da5, 0x1daf), *range(0x1f2b, 0x1f35), *range(0x18ac, 0x18b4), 0x19df, 0x19e0, 0x1f06,
          # the top bar's messages (with the colour column, without)
          0x1f6d, 0x1f09]
ZIPPED_WHEELS = set(WHEEL_SET_SPRITES)
# all the courses (push73): the course cards' backgrounds and names by course id
# (nsTaData 0x93e33e0, 0x93e3420), the race title page's (0x93ddac0) for both time zones, the
# city maps of mapChange (cards 6..9) and the result's section labels 5..7
ALL_COURSE_SPRITES = sorted({2863, 2866, 2869, 2865, 2870, 2873, 2880, 2357, 2355, 2885, 3930, 6244, 2359,
                             3901, 3904, 3907, 3903, 3908, 3911, 3918, 3293, 3292, 3923, 5608, 6779, 3295,
                             3931, 3932, 3933, 3934, 5609, 3936, 5610, 3937, 3944, 3939, 5611, 3941, 3942, 3943,
                             5612, 0x1832, 0x15b9, 0x19a4, 0x1831, 7269, 7270, 7271})
EXTRA += ALL_COURSE_SPRITES
# the 10-in-a-row (src/tendb.h, menu.c): the level boxes' names, horsepower, R35 tune steps and
# medals (0x940d320, 0x940d3a4), the faces (0x940d160), the top bar's messages, and the race
# title page's backgrounds, names, time zones, ramps and levels (0x940d420, 0x940d3e0)
def _tendb():
    import re
    here = os.path.dirname(os.path.abspath(__file__))
    path = os.path.join(here, "..", "src", "tendb.h")
    if not os.path.exists(path):  # (tools/tendb.py imports this module before it writes tendb.h)
        return []
    t = open(path).read()
    pre = t.split("TENDB_PRE[10]", 1)[1]
    return sorted({int(x) for x in re.findall(r"\d+", pre)} - {10, 11, 4, 0})
TEN_SPRITES = sorted(set(range(915, 925)) | {2514} | set(range(3374, 3387)) | set(range(6326, 6339)) |
                     {950, 2818, 2819, 1815, 0x1be7, 0x1703} | set(range(1821, 1854)) | set(_tendb()))
EXTRA += TEN_SPRITES
# clTenResult3: course names (0x93d1fe0), levels (0x93d2020), OUT RUN 32 (0xf37) and the
# characters' face, name and line by result (0x93d1e20..0x93d1fc4)
TEN_RESULT_SPRITES = sorted(set(range(3262, 3272)) | set(range(2846, 2856)) | {5599, 0xf37} | {
    3230, 3256, 2835, 3233, 3259, 2838, 2832, 2843, 2328, 3880, 3892, 3255, 5590, 5595, 4740, 2834, 2845, 2330,
    3871, 3884, 3246, 2323, 2332, 1866, 2831, 2842, 2327, 2324, 2333, 1867, 5593, 5598, 4743,
    3231, 3257, 2836, 3232, 3258, 2837, 4734, 4744, 3882, 2833, 2844, 2329, 3878, 3890, 3253, 5591, 5596, 4741,
    5592, 5597, 4742, 2830, 2841, 2326, 5589, 5594, 4739, 3872, 3885, 3247, 2322, 2331, 1865, 3873, 3883, 3248,
    3875, 3887, 3250, 3876, 3888, 3251, 3877, 3889, 3252, 3879, 3891, 3254, 2829, 2840, 2325, 1854, 1868, 1562,
    3234, 3260, 2839, 3874, 3886, 3249})
EXTRA += TEN_RESULT_SPRITES
EXTRA += [43, 201, 202]  # cons_p_R, _2, _3: the rival marks' R by rival (src/hud.c draw_rival_marks)
# the class-up screen's plates by class (0x93f09c0: menu_p_lvup_s_1..47, menu_p_lvup_l_1..47)
LVUP_SPRITES = [*range(1206, 1224), *range(1480, 1556)]
EXTRA += LVUP_SPRITES
ZIPPED |= set(LVUP_SPRITES)
ZIPPED |= ZIPPED_WHEELS
# the story's tuning work (src/steff.h, tools/steff.py): the rows' top bar titles and part
# pictures, T's digits (0x479..), the horsepower's digits (menu_g_st_result_L0..9, s0..9),
# the tuners' complete pictures; its lines (tools/steff_texts.txt, two each) as STEFF_TEXT_BASE + k
def _steff():
    import re
    h = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "steff.h")).read()
    ids = {int(a) for r in re.findall(r"\{ (\d+), (\d+), 0x[0-9a-f]+, \d+, -?\d+ \}", h) for a in r}
    body = h[h.index("STEFF_COMPLETE"):]
    ids |= {int(v) for v in re.findall(r"\b\d+\b", body[body.index("{"):body.index("}")])}
    return sorted(i for i in ids if i > 1)
STEFF_SPRITES = _steff() + [*range(0x479, 0x483), *range(2657, 2667), *range(2668, 2678)]
EXTRA += STEFF_SPRITES
STEFF_TEXT_BASE = 105500
STEFF_TEXTS = [l.rstrip("\n").split("\t") for l in open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                                      "steff_texts.txt"), encoding="utf-8") if "\t" in l]
ZIPPED |= set(TEN_RESULT_SPRITES)
ZIPPED |= {i for i in TEN_SPRITES if i not in range(3374, 3387) and i not in range(6326, 6339)}
# Atlas cells that consoles draw with clSpriteAnime::setImageReplaceUV(template, n): entry n
# of the UV table (tools/uvtab.txt, dumped by uvtab.py), cropped and packed as UV_BASE + n.
# The 10-in-a-row's console: the time digits (22..31), ' and " (32, 33), the place digits
# (140..149), 人目 (166), LV (167) and EX (274).
# The rival info console (clRivalInfoConsole, cons_g_cpu): the advantage's digits (168..177),
# ADVANTAGE (181) and the empty cell (183) over the frames.
UV_BASE = 102000
# The versus' (clJoinConsole): the place's /2../4 (150..152), the distance's digits (153..162),
# dot, m and km (163..165); the boxes' comma, minus, m (178..180), HP/ (76), the horsepower's
# digits (77..86) and the grades B, D, DG, G, HG (87..91).
UV_USED = [*range(22, 34), *range(140, 150), 166, 167, 274, *range(168, 178), 181, 183, *range(150, 166),
           *range(178, 181), *range(76, 92)]
ZIPPED |= {UV_BASE + n for n in UV_USED}
# its face, name and car name of each 10-in-a-row character (getTenChara*Image: the table at
# 0x9333660, 24 bytes each, +8 face, +0xc name, +0x10 car)
TEN_CHARA = 0x9333660
ST_CHAR, ST_CHAR_N = 0x9739300, 240  # getStCharData's table, 20 bytes: ECharacter, face, name, car, wide


def uv_cells():
    here = os.path.dirname(os.path.abspath(__file__))
    cells = {}
    for line in open(os.path.join(here, "uvtab.txt")):
        n, sid, *uv = line.split()
        cells[int(n)] = (int(sid), [int(x) / 1024 for x in uv])
    return cells


ZIPPED |= {i for i in STTUNE_SPRITES if i not in range(7000, 8200)}
ZIPPED |= set(range(STTUNE_TEXT_BASE, STTUNE_TEXT_BASE + len(STTUNE_TEXTS)))
ZIPPED |= set(range(WEAR_TEXT_BASE, WEAR_TEXT_BASE + len(WEAR_TEXTS)))
# clTenStartRivalNameDisp::reset's texts in font 4: TEN_BANNER_TEXT_BASE + round, sprintf
# ("%s人目" at 0x93d2ad0, the round's fullwidth number from 0x93d2240), and + 32 + the
# character, "vs " + name (0x93d21c0)
TEN_BANNER_TEXT_BASE = 101700
TEN_BANNER_ROUNDS, TEN_BANNER_NAMES, TEN_BANNER_FORMAT = 0x93D2240, 0x93D21C0, 0x93D2AD0
ZIPPED |= set(range(TEN_BANNER_TEXT_BASE, TEN_BANNER_TEXT_BASE + 64))

# Cut::clCutIn::DataLoad's 10-in-a-row lines, data/cutin/jp/CutInTenRival.txt: line k of it
# (after its ECUTIN_ header; 2 ECutInStart per character, then 1 ECutInLose each) gives
# CUTIN_TEXT_BASE + k, its text ('#' a space, "\\n" a new line) as clCutinDisp::setSpeak sets it,
# font 3, and CUTIN_FACE_BASE + k, its face: cell (expression - 1) of the character's sheet
# of 128 px faces (setSpeak's image switch at 0x93cffb8 over nsCutinDisp::eChar, the names at
# 0x9768fa0, 32 bytes each)
CUTIN_FILE = "data/cutin/jp/CutInTenRival.txt"
CUTIN_TEXT_BASE, CUTIN_FACE_BASE = 103000, 103200
CUTIN_CHAR_NAMES, CUTIN_CHAR_SWITCH, CUTIN_CHARS = 0x9768FA0, 0x93CFFB8, 0x42
CUTIN_SWITCH_DEFAULT, CUTIN_DEFAULT_IMAGE = 0x85D4703, 0xD71
CUTIN_FONT, CUTIN_FACE = 3, 128


def cutin_lines(root):
    out = []
    for line in open(f"{root}/{CUTIN_FILE}", encoding="utf-8"):
        f = line.split()
        if len(f) >= 9 and f[0].startswith("ECUTINSERIAL_"):
            out.append((f[4].split("::")[1], int(f[5]), f[8].replace("#", " ").split("\\n")))
    return out


def cutin_images(elf):
    """nsCutinDisp::eChar name -> its face sheet's sprite id (the switch's mov $id, %edx)"""
    images = {}
    for k in range(CUTIN_CHARS):
        o = elf.off(CUTIN_CHAR_NAMES + 32 * k)
        name = elf.d[o:elf.d.index(b"\0", o)].decode().split("::")[1]
        case = elf.u32(CUTIN_CHAR_SWITCH + 4 * k)
        o = elf.off(case)
        images[name] = struct.unpack_from("<I", elf.d, o + 1)[0] if elf.d[o] == 0xBA else (
            CUTIN_DEFAULT_IMAGE if case == CUTIN_SWITCH_DEFAULT else None)
    return images


# The shows' lines (tools/storydemo_texts.txt: eChar, expression, text), clCutinDisp::setSpeak's
# like the 10-in-a-row's: DEMO_TEXT_BASE + row, its text in font 3, and DEMO_FACE_BASE + row,
# its face: setSpeak's cell of the character's sheet, 4 cells of 128 a row (expression % 4,
# expression / 4) on the sheet's table size
DEMO_TEXT_BASE, DEMO_FACE_BASE = 104000, 104400
DEMO_TEXTS = [l.rstrip("\n").split("\t") for l in open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                                     "storydemo_texts.txt"), encoding="utf-8") if "\t" in l]


def cutin_image_ids(elf):
    """nsCutinDisp::eChar -> its face sheet's sprite id, by eChar (None: none)"""
    out = []
    for k in range(CUTIN_CHARS):
        case = elf.u32(CUTIN_CHAR_SWITCH + 4 * k)
        o = elf.off(case)
        out.append(struct.unpack_from("<I", elf.d, o + 1)[0] if elf.d[o] == 0xBA else (
            CUTIN_DEFAULT_IMAGE if case == CUTIN_SWITCH_DEFAULT else None))
    return out


SETTING_TEXTS = [(3, "STOCK"), (3, "FULL TUNE"), (3, "AT"), (3, "MT"), (1, "TAP THE CAR TO GO ON")]
# The port's own OPTION screen (menu.c draw_option), pre-rendered as OPTION_TEXT_BASE + k.
OPTION_TEXT_BASE = 101900
OPTION_TEXTS = [(3, "CONTROLS"), (3, "BUTTONS"), (3, "GYRO"), (1, "STEERING"), (1, "GYRO SENSITIVITY"),
                (3, "GRAPHICS"), (1, "BRIDGE REFLECTION"), (3, "ON"), (3, "OFF"),
                (1, "TILT THE IPAD TO STEER"), (3, "OPTION"), (1, "FSR SHARPENING"),
                (1, "Maybe......Hidden Cars?"),  # (the maker select's hidden cars key)
                (1, "SLIDER PEDALS"), (1, "AMBIENT OCCLUSION"), (1, "BACK MIRROR"),
                (1, "ALL PARTS & TUNE:  ON"), (1, "ALL PARTS & TUNE:  OFF"),  # (the colour select's key)
                # Android's GRAPHICS rows (menu.c draw_option's hq rows)
                (1, "HD TEXTURES"), (1, "BLOOM"), (1, "SSAO"), (1, "WET ROAD"), (1, "PAINT REFLECTION"),
                (1, "ANTI-ALIASING"), (3, "4X"), (3, "2X")]
# The versus select (menu.c MENU_VS_SEL, menu_layout_mode_sel_3 and its picture and ring with
# the port's discs in the story, 10 out run and time attack discs' places): the top bars'
# titles as VS_TEXT_BASE + k, the discs (tools/vsdiscs.py, tools/vsdisc/) as VS_ICON_BASE + k
# and their lines under the picture as VS_LINE_BASE + k.
VS_TEXT_BASE, VS_ICON_BASE, VS_LINE_BASE = 107000, 107010, 107020
VS_TEXTS = [(3, "VERSUS"), (3, "BLUETOOTH VERSUS"), (3, "LAN VERSUS"), (3, "ONLINE VERSUS")]
VS_DISCS = 3
ANIMS += [(n, None) for n in ("menu_layout_mode_sel_3", "menu_layout_mode_sel_3_image", "menu_layout_mode_sel_3_ring")]
# The versus' area vote (menu.c MENU_VS_AREA, clseqJoCourseSelectThread): the strip of
# cards, the other players' marks (under a card, or at an edge with an arrow; omakase's pair),
# the top bar's countdown; the areas' own pictures the run swaps in, the marks' labels
# menu_p_vs_4p_1..4, the edge arrows, the title, the countdown's digits and red last digits.
ANIMS += [(n, None) for n in ("menu_layout_vs_course1", "menu_g_vs_icon", "menu_g_vs_icon_omakase",
                              "menu_g_vs_icon_omakase2", "menu_g_vs_ex_player", "menu_g_timer_x")]
EXTRA += [5545, 5546, 5550, 5557, 4659, 4657, 5559, *range(926, 930), 2535, 3393, 7149,
          *range(693, 703), *range(2524, 2534)]
# The cars flying in before a versus race (menu.c MENU_VS_MATCH, clRaceThread::
# JoMatchingInformation): the background, the shadow, name plates' and VS overlays by machine
# count, and clNamePlate2's plates (an R35's with the tune's levels).
_MATCH = ["menu_g_shogo_bg_1", "menu_g_car_tg_decide", "menu_g_car_tg_decide_ph"]
_MATCH += ["menu_p_pre_vs_rival_%d_%s" % (n, k) for n in (2, 3, 4) for k in ("car", "shadow", "vs")]
ANIMS += [(n, None) for n in _MATCH if (n, None) not in ANIMS]
# The versus stars after a race (menu.c MENU_VS_STAR, clSeqJoStarGetThread): the background
# (its emblem by the stars held swapped in), logo, car frame, title row and wins; the stars
# held (menu_use_vs_star: 1, 100, 1000, 10000); the stars won (1..3), the carry to a bigger
# star (keta_a then keta_b), the new big star's flash (finish_*), the stars lost (minus1, 2).
VS_STAR_ANIMS = ["menu_g_vs_star_bg", "menu_layout_vs_star_logo", "menu_layout_vs_star_car", "menu_layout_vs_star_title",
                 "menu_layout_vs_star_wins", "menu_use_vs_star"]
VS_STAR_ANIMS += ["menu_g_vs_star_%d" % k for k in (1, 2, 3)] + ["menu_g_vs_star_keta_a_%d" % k for k in (1, 2, 3)]
VS_STAR_ANIMS += ["menu_g_vs_star_keta_b_%d" % k for k in (1, 2, 3)]
VS_STAR_ANIMS += ["menu_g_vs_star_finish_%d" % k for k in (1, 100, 1000, 10000)] + ["menu_g_vs_star_minus1", "menu_g_vs_star_minus2"]
ANIMS += [(n, None) for n in VS_STAR_ANIMS]
# The story's poems (menu.c MENU_POEM, seqStPoem): a chapter's one picture after its select is won
POEM_ANIMS = ["menu_p_poem_%s" % k for k in ("10", "15", "20", "30", "40", "50", "60", "70", "80", "90", "fd100",
                                             "isida85", "tuner110", "zero90")]
ANIMS += [(n, None) for n in POEM_ANIMS if (n, None) not in ANIMS]
# The story's ending's staff roll (menu.c ENDING, clEndingConsole): its masks, rolls, fades and logo
ENDING_ANIMS = ["menu_layout_ed_fade_black", "menu_layout_ed_fade_white", "menu_layout_ed_bng"]
ENDING_ANIMS += ["menu_layout_ed_base_%d" % k for k in range(1, 5)] + ["menu_layout_ed_roll_%d" % k for k in range(1, 8)]
ANIMS += [(n, None) for n in ENDING_ANIMS if (n, None) not in ANIMS]
# and the bonuses after it (menu.c MENU_BONUS: clStStoryModeClearBonus, clStUndefeatedBonus,
# clStUndefeatedBonus2): the card (its clear bonus design 0x905, 0x906 swapped in) and the meter
ANIMS += [(n, None) for n in ("menu_layout_cd_cd", "menu_layout_meter_meter2") if (n, None) not in ANIMS]
EXTRA += [0x905, 0x906]
# the emblems by the stars held (none, falcon, horse, scorpion, bear, wasp, shark, tiger,
# suzaku, dragon), the big stars the carry swaps in, the wins' digits menu_g_st_result_L0..9
VS_STAR_EMBLEMS = [0x18e5, 0x1706, 0x1ab8, 0x1381, 0x1382, 0x1707, 0x1708, 0x18e6, 0x18e4]
EXTRA += VS_STAR_EMBLEMS + [0x1081, 0x1380, *range(2657, 2667)]
ZIPPED |= set(VS_STAR_EMBLEMS)
# The port's own SAVE DATA screen (menu.c draw_save), pre-rendered as SAVE_TEXT_BASE + k,
# and font 1's digits, '/' and ':' one by one as SAVE_GLYPH_BASE + k for its numbers and dates.
SAVE_TEXT_BASE, SAVE_GLYPH_BASE = 101950, 101980
SAVE_TEXTS = ([(3, "SAVE DATA"), (3, "NEW GAME"), (3, "LOAD GAME")] + [(3, f"SAVE {n}") for n in range(1, 5)] +
              [(1, "NO DATA"), (1, "STORY WINS"), (1, "GOLD MEDALS"), (1, "CARS"),
               (1, "TAP A SAVE TO START A NEW GAME IN IT"), (1, "TAP A SAVE TO LOAD IT")] +
              [(1, f"TAP SAVE {n} AGAIN TO ERASE IT AND START OVER") for n in range(1, 5)] +
              [(1, "TAP THE BIN AGAIN TO DELETE THIS CAR AND ITS RECORDS")] +
              [(3, "DELETE"), (1, "TAP A SAVE TO DELETE IT")] +
              [(1, f"TAP SAVE {n} AGAIN TO DELETE IT") for n in range(1, 5)])
SAVE_GLYPHS = "0123456789/:"
# the SAVE DATA screen's "i" key and its page (menu.c MENU_INFO; tools/infopage.py, in a CJK
# font of the machine's), the page kept zlib-packed
INFO_KEY, INFO_PAGE = 108000, 108001
ZIPPED |= {INFO_PAGE}
# The save's cars (menu.c's HAVE, GARAGE and CARD screens) as CARD_TEXT_BASE + k; the
# "do you have a card?" cards (clSeqCardHaveSelectThread's menu_p_yn_card_have_yes and _no)
# with the port's captions in place of the arcade's (CARD_HAVE_YES, CARD_HAVE_NO); the card's
# first page (clSeqCardDisp1) in its fonts: font 2's digits, '-' and ',' as CARD_GLYPH2 + k,
# font 5's digits and '-' as CARD_GLYPH5 + k, each car's name in two lines
# (get_car_model_data +0 and +4) as CARD_CAR_BASE + 2 * (its place in cars.json) + line, and
# each colour name (cars.json's texts) as CARD_COLOR_BASE + k.
CARD_TEXT_BASE = 105000
CARD_TEXTS = ([(3, "USE A CAR FROM THIS SAVE?"), (3, "SAVED CARS"), (1, "TAP THE CAR TO DRIVE IT"), (1, "TAP TO GO ON")] +
              [(5, f"PLAYER {n}") for n in range(1, 5)] + [(5, "湾岸の新人"), (2, "馬力"), (2, "km"), (3, "SAVE THIS CAR?")])
CARD_HAVE_YES, CARD_HAVE_NO = CARD_TEXT_BASE + len(CARD_TEXTS), CARD_TEXT_BASE + len(CARD_TEXTS) + 1
# (the sprite, the rows its caption takes, the port's caption)
# (and the new car's "save it?" after its card: menu.c's KEEP)
CARD_HAVE = [(0x1720, 169, 190, "SAVED CARS"), (0x13C5, 145, 172, "NEW CAR"),
             (0x1720, 169, 190, "SAVE IT"), (0x13C5, 145, 172, "DON'T SAVE")]
CARD_GLYPH2, CARD_GLYPHS2 = 105020, "0123456789-,"
CARD_GLYPH5, CARD_GLYPHS5 = 105040, "0123456789-"
CARD_CAR_BASE, CARD_COLOR_BASE = 105100, 105200
# The card's title row (clSeqCardDisp1::run 0x87de991: the class's string, "級", a full-width space, "/", the title)
# for each class 0..46 (GetClassStr's table at 0x9332980), the title the port's one, as
# CARD_CLASS_BASE + class.
CARD_CLASS_BASE = 105400
CLASS_NAMES = ["N"] + [f"{g}{n}" for g in "CBAS" for n in range(9, 0, -1)] + [f"SS{n}" for n in range(9, 0, -1)] + ["SSS"]

# The player's name (menu.c's NAME screen, clSeqCardNameSelectThread): the card's 256 name
# characters (GetCardNameChar2Wchar's table) in font 8 (the grid's, NAME_GLYPH8_BASE + k),
# font 10 (the five big ones, NAME_GLYPH10_BASE + k) and font 5 (the card's name field,
# NAME_GLYPH5_BASE + k); after them in font 8 the grid's own keys: back space and the space's
# mark (the grid strings' U+042E).
NAME_TABLE, NAME_N = 0x932A440, 256
NAME_GLYPH8_BASE, NAME_GLYPH10_BASE, NAME_GLYPH5_BASE = 106000, 106300, 106600
NAME_KEYS = chr(0x2190) + chr(0x42E)
EXTRA += [6351, 5587]  # menu_p_topbar_mes_name_1, menu_p_name_select_ring
ANIMS += [(n, None) for n in ("menu_layout_name_select", "menu_g_name_name_back", "menu_use_name", "menu_g_name_line",
                               "menu_g_name_katakana", "menu_g_name_hiragana", "menu_g_name_capital",
                               "menu_g_name_small", "menu_g_name_end")]


def name_chars(elf):
    return [chr(elf.u32(NAME_TABLE + 4 * k)) for k in range(NAME_N)]


# The titles (称号, tools/titledb.py; src/title.c): every character the 5200 titles use, in
# font 1 (the title select's spin box, clSeqTitleSelectThread::clSpinBox) as
# TITLE_GLYPH1_BASE + k, font 5 (the card's title row) as TITLE_GLYPH5_BASE + k and font 7
# (the GET and REWRITE screens' big ones) as TITLE_GLYPH7_BASE + k, k its place in titledb's
# TITLEDB_CHAR; the card's row by class without the title (CARD_CLASSP_BASE + class:
# "B7級　/"), the title drawn after it from the glyphs. The title select's screen
# (seqTitleSelect, stPimpl and clSpinBox): its back's letters, title, the new titles' frames,
# the rewrite, the spin box (menu_layout_yn_rank: its templates' backlight and light, the top
# bar's "rewrite the title?").
TITLE_GLYPH1_BASE, TITLE_GLYPH5_BASE, TITLE_GLYPH7_BASE, CARD_CLASSP_BASE = 110000, 112000, 114000, 105450
ANIMS += [(n, None) for n in ("menu_g_shogo_bg_moji", "menu_g_shogo_title", "menu_layout_shogo_rewrite",
                               "menu_g_shogo_shogo_1", "menu_g_shogo_shogo_2", "menu_g_shogo_shogo_3",
                               "menu_layout_yn_rank")]
EXTRA += [0x18EA, 0xD4B, 0x1E59]

# The menus' top bar shows the seconds left (menu_g_timer_x); the port has no time limit
# there, so menu.c draws this big infinity sign instead, in the timer digits' style.
INFINITY_ID = 102000

# Sprites stored fully opaque on black, which show as a black box on the top bar's oval
# (menu_p_time_time, "TIME" beside the timer); unblend_black turns the black into alpha.
ON_BLACK = {1573}


def unblend_black(im):
    """Alpha = brightest channel, colour divided by it: over black it looks the same, and over
    anything else the black falls away and the glow stays soft."""
    out = []
    for r, g, b, _ in im.getdata():
        a = max(r, g, b)
        out.append((r * 255 // a, g * 255 // a, b * 255 // a, a) if a else (0, 0, 0, 0))
    res = Image.new("RGBA", im.size)
    res.putdata(out)
    return res


def render_infinity(w=128, h=72, span=84, stroke=9, shear=0.2):
    """A white italic infinity sign with the soft red glow of menu_p_time_0..9."""
    ss = 4
    mask = Image.new("L", (w * ss, h * ss), 0)
    d = ImageDraw.Draw(mask)
    a, r = span / 2 * ss, stroke / 2 * ss
    cx, cy = w * ss / 2, h * ss / 2
    for i in range(2000):  # lemniscate of Bernoulli, a bit taller, leaning right
        t = 2 * math.pi * i / 2000
        k = 1 + math.sin(t) ** 2
        y = a * math.sin(t) * math.cos(t) / k * 1.25
        x = a * math.cos(t) / k - shear * y
        d.ellipse((cx + x - r, cy + y - r, cx + x + r, cy + y + r), fill=255)
    mask = mask.resize((w, h), Image.LANCZOS)
    out = Image.new("RGBA", (w, h), (255, 0, 0, 0))
    out.putalpha(mask.filter(ImageFilter.GaussianBlur(3.5)).point(lambda v: min(220, int(v * 1.7))))
    white = Image.new("RGBA", (w, h), (254, 254, 254, 255))
    white.putalpha(mask)
    out.alpha_composite(white)
    return out



class Elf:
    def __init__(self, path):
        self.d = open(path, "rb").read()
        phoff, = struct.unpack_from("<I", self.d, 28)
        phentsize, phnum = struct.unpack_from("<HH", self.d, 42)
        self.segs = []
        for i in range(phnum):
            t, off, va, _, fsz, _, _, _ = struct.unpack_from("<8I", self.d, phoff + i * phentsize)
            if t == 1:
                self.segs.append((va, off, fsz))

    def off(self, va):
        for v, o, n in self.segs:
            if v <= va < v + n:
                return o + va - v
        return None

    def u32(self, va):
        return struct.unpack_from("<I", self.d, self.off(va))[0]

    def i16s(self, va, n):
        return struct.unpack_from(f"<{n}h", self.d, self.off(va))

    def wstr(self, va):
        o, s = self.off(va), ""
        while True:
            c = struct.unpack_from("<I", self.d, o)[0]
            if not c:
                return s
            s += chr(c)
            o += 4

    def cstr(self, va):
        o = self.off(va)
        return self.d[o:self.d.index(b"\0", o)].decode("latin1")


def sprite_names(elf):
    names = {}
    for i in range(TABLE_N):
        p = elf.u32(TABLE_VA + 44 * i)
        if p and elf.off(p) is not None:
            names[i] = elf.cstr(p)
    return names


def render_text(elf, names, root, font, text):
    """clSpriteFont::draw: glyph g of a font is cell g % cells of page sprite g // cells;
    the advance is the half or full width (chars above 0x7f)."""
    nglyph, half, full, cell, cols, _, cells, pages, glyph_map = struct.unpack_from(
        "<9I", elf.d, elf.off(FONT_TABLE + 36 * font))
    w = sum(half if ord(c) <= 0x7F else full for c in text) - (half if ord(text[-1]) <= 0x7F else full) + cell
    out = Image.new("RGBA", (w, cell))
    x = 0
    for c in text:
        ch = ord(c)
        page = elf.i16s(glyph_map + 2 * (ch >> 5), 1)[0]
        g = elf.i16s(glyph_map + 0x1000 + 64 * page + 2 * (ch & 31), 1)[0]
        sheet = Image.open(f"{root}/{names[elf.u32(pages + 4 * (g // cells))]}").convert("RGBA")
        k = g % cells
        glyph = sheet.crop(((k % cols) * cell, (k // cols) * cell, (k % cols + 1) * cell, (k // cols + 1) * cell))
        out.alpha_composite(glyph, (x, 0))
        x += half if ch <= 0x7F else full
    return out


def render_lines(elf, names, root, font, lines):
    """clSpriteFont::draw of a string with new lines: each line from the same left, one full
    width further down. The sprite is the widest line's advances (updateSize's width) plus a
    cell wide, so the drawer can take that width back off."""
    _, half, full, cell = struct.unpack_from("<4I", elf.d, elf.off(FONT_TABLE + 36 * font))
    adv = [sum(half if ord(c) <= 0x7F else full for c in t) for t in lines]
    out = Image.new("RGBA", (max(adv) + cell, full * (len(lines) - 1) + cell))
    for i, t in enumerate(lines):
        if t:
            out.alpha_composite(render_text(elf, names, root, font, t), (0, full * i))
    return out


# a record's blend (+0x3c) -> menu.bin's
BLEND = {8: 1, 9: 2}


def load_anm(path, keep):
    d = open(path, "rb").read()
    nf, _ = struct.unpack_from("<2I", d, 0)
    frames = [struct.unpack_from("<2I", d, 8 + 8 * i) for i in range(nf)]
    if keep:
        frames = frames[:keep]
    base = 8 + 8 * nf
    end = max(s + n for s, n in frames)
    recs = [struct.unpack_from("<i6f4i4ii", d, base + 64 * k) for k in range(end)]
    return frames, recs


def main():
    root, out = sys.argv[1], sys.argv[2]
    elf = Elf(f"{root}/main")
    names = sprite_names(elf)
    sprites, index, ids = [], {}, []

    def sprite(sid):
        if sid not in index:
            index[sid] = 0xFFFF
            name = names.get(sid, "")
            if name:
                try:
                    im = Image.open(f"{root}/{name}").convert("RGBA")
                    if sid in ON_BLACK:
                        im = unblend_black(im)
                    # the arcade draws a sprite at its table size (clImageResourceManager::getSize,
                    # +0x10), not its picture's: a few pictures differ (menu_p_t_tuning_parts_get)
                    size = tuple(round(v) for v in struct.unpack_from("<2f", elf.d, elf.off(TABLE_VA + 44 * sid + 16)))
                    if size != im.size and min(size) > 0:
                        im = im.resize(size, Image.LANCZOS)
                    index[sid] = len(sprites)
                    sprites.append(im)
                    ids.append(sid)
                except OSError:
                    print("missing", name)
        return index[sid]

    anims = []
    for name, keep in ANIMS:
        frames, recs = load_anm(f"{root}/data/anime/{name}.anm", keep)
        packed = b"".join(
            struct.pack("<HH6f4h4h", sprite(r[0]), BLEND.get(r[15], 0), *r[1:7], *(max(-32768, min(32767, v)) for v in r[7:15]))
            for r in recs)
        anims.append((name, frames, len(recs), packed))
        if name in DEMO_ANIMS:
            ZIPPED.update(r[0] for r in recs if r[0] >= 0)
    for sid in EXTRA:
        sprite(sid)
    for k in range(32):
        for off in (8, 12, 16):
            sid = elf.u32(TEN_CHARA + 24 * k + off)
            ZIPPED.add(sid)
            sprite(sid)
    # the story race's rival boxes: getStCharData's face, name and car pictures (src/storyinfo.h)
    for k in range(ST_CHAR_N):
        for off in (4, 8, 12):
            sid = elf.u32(ST_CHAR + 20 * k + off)
            ZIPPED.add(sid)
            sprite(sid)
    for k, (font, text) in enumerate(TEXTS):
        sprites.append(render_text(elf, names, root, font, text))
        ids.append(TEXT_BASE + k)
    for k in range(elf.u32(BGM_COUNTS)):
        song = struct.unpack_from("<i", elf.d, elf.off(BGM_SET + 4 * k))[0]
        title = "RANDOM" if song == 0x6E else "NO BGM" if song < 0 else elf.wstr(elf.u32(BGM_MUSIC + 12 * song + 8))
        sprites.append(render_text(elf, names, root, 0, title))
        ids.append(BGM_TEXT_BASE + k)

    for k, (font, text) in enumerate(SETTING_TEXTS):
        sprites.append(render_text(elf, names, root, font, text))
        ids.append(SETTING_TEXT_BASE + k)

    for k, (font, text) in enumerate(OPTION_TEXTS):
        sprites.append(render_text(elf, names, root, font, text))
        ids.append(OPTION_TEXT_BASE + k)

    for k, (font, text) in enumerate(VS_TEXTS):
        sprites.append(render_text(elf, names, root, font, text))
        ids.append(VS_TEXT_BASE + k)
    disc_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "vsdisc")
    for k in range(VS_DISCS):
        for base, name in ((VS_ICON_BASE, "icon"), (VS_LINE_BASE, "txt")):
            sprites.append(Image.open(os.path.join(disc_dir, f"{name}_{k}.png")).convert("RGBA"))
            ids.append(base + k)

    for k, (font, text) in enumerate(SAVE_TEXTS):
        sprites.append(render_text(elf, names, root, font, text))
        ids.append(SAVE_TEXT_BASE + k)
    for k, c in enumerate(SAVE_GLYPHS):
        sprites.append(render_text(elf, names, root, 1, c))
        ids.append(SAVE_GLYPH_BASE + k)
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import infopage
    sprites += [infopage.render_key(), infopage.render_page()]
    ids += [INFO_KEY, INFO_PAGE]

    for k, (font, text) in enumerate(CARD_TEXTS):
        sprites.append(render_text(elf, names, root, font, text))
        ids.append(CARD_TEXT_BASE + k)
    for k, (sid, y0, y1, text) in enumerate(CARD_HAVE):
        card = Image.open(f"{root}/{names[sid]}").convert("RGBA")
        card.paste((0, 0, 0, 0), (0, y0, card.size[0], y1))
        caption = render_text(elf, names, root, 1, text)
        card.alpha_composite(caption, ((card.size[0] - caption.size[0]) // 2, (y0 + y1 - caption.size[1]) // 2))
        sprites.append(card)
        ids.append(CARD_HAVE_YES + k)
    for base, font, glyphs in ((CARD_GLYPH2, 2, CARD_GLYPHS2), (CARD_GLYPH5, 5, CARD_GLYPHS5)):
        for k, c in enumerate(glyphs):
            sprites.append(render_text(elf, names, root, font, c))
            ids.append(base + k)
    for k, name in enumerate(CLASS_NAMES):
        sprites.append(render_text(elf, names, root, 5, name + "級" + chr(0x3000) + "/湾岸の新人"))
        ids.append(CARD_CLASS_BASE + k)
        sprites.append(render_text(elf, names, root, 5, name + "級" + chr(0x3000) + "/"))
        ids.append(CARD_CLASSP_BASE + k)
    for k, car in enumerate(c for m in CARS["makers"] for c in m["cars"]):
        for line in range(2):
            sprites.append(render_text(elf, names, root, 2, car["lines"][line].strip() or " "))
            ids.append(CARD_CAR_BASE + 2 * k + line)
    for k, text in enumerate(CARS["texts"]):
        sprites.append(render_text(elf, names, root, 2, text))
        ids.append(CARD_COLOR_BASE + k)

    for k, (font, text) in enumerate(STORYSEL_TEXTS):
        sprites.append(render_text(elf, names, root, font, text))
        ids.append(STORYSEL_TEXT_BASE + k)

    for k, (font, text) in enumerate(STTUNE_TEXTS):
        sprites.append(render_text(elf, names, root, font, text))
        ids.append(STTUNE_TEXT_BASE + k)

    for k, (font, text) in enumerate(WEAR_TEXTS):
        sprites.append(render_text(elf, names, root, font, text))
        ids.append(WEAR_TEXT_BASE + k)

    for k, (font, *lines) in enumerate(STEFF_TEXTS):
        sprites.append(render_lines(elf, names, root, int(font), [t for t in lines]))
        ids.append(STEFF_TEXT_BASE + k)

    def utf8(va):
        o = elf.off(va)
        return elf.d[o:elf.d.index(b"\0", o)].decode("utf-8")

    fmt = utf8(TEN_BANNER_FORMAT)
    for k in range(64):
        text = fmt % utf8(elf.u32(TEN_BANNER_ROUNDS + 4 * k)) if k < 32 else utf8(elf.u32(TEN_BANNER_NAMES + 4 * (k - 32)))
        sprites.append(render_text(elf, names, root, 4, text))
        ids.append(TEN_BANNER_TEXT_BASE + k)

    images = cutin_images(elf)
    for k, (char, face, text) in enumerate(cutin_lines(root)):
        sprites.append(render_lines(elf, names, root, CUTIN_FONT, text))
        ids.append(CUTIN_TEXT_BASE + k)
        sheet = Image.open(f"{root}/{names[images[char]]}").convert("RGBA")
        c = face - 1
        x, y = c % (sheet.size[0] // CUTIN_FACE) * CUTIN_FACE, c // (sheet.size[0] // CUTIN_FACE) * CUTIN_FACE
        sprites.append(sheet.crop((x, y, x + CUTIN_FACE, y + CUTIN_FACE)))
        ids.append(CUTIN_FACE_BASE + k)
        ZIPPED.update((CUTIN_TEXT_BASE + k, CUTIN_FACE_BASE + k))

    by_char = cutin_image_ids(elf)
    for k, (char, expr, text) in enumerate(DEMO_TEXTS):
        sprites.append(render_lines(elf, names, root, CUTIN_FONT, text.replace("#", " ").split("\\n")))
        ids.append(DEMO_TEXT_BASE + k)
        sid, c = by_char[int(char)], int(expr)
        if sid is not None and names.get(sid):
            sheet = Image.open(f"{root}/{names[sid]}").convert("RGBA")
            size = tuple(round(v) for v in struct.unpack_from("<2f", elf.d, elf.off(TABLE_VA + 44 * sid + 16)))
            if size != sheet.size and min(size) > 0:
                sheet = sheet.resize(size, Image.LANCZOS)
            x, y = c % 4 * CUTIN_FACE, c // 4 * CUTIN_FACE
            sprites.append(sheet.crop((x, y, x + CUTIN_FACE, y + CUTIN_FACE)))
            ids.append(DEMO_FACE_BASE + k)
        ZIPPED.update((DEMO_TEXT_BASE + k, DEMO_FACE_BASE + k))

    cells = uv_cells()
    for n in UV_USED:
        sid, (u0, v0, u1, v1) = cells[n]
        im = Image.open(f"{root}/{names[sid]}").convert("RGBA")
        w, h = im.size
        sprites.append(im.crop((round(u0 * w), round(v0 * h), round(u1 * w), round(v1 * h))))
        ids.append(UV_BASE + n)

    for base, font, chars in ((NAME_GLYPH8_BASE, 8, name_chars(elf) + list(NAME_KEYS)),
                              (NAME_GLYPH10_BASE, 10, name_chars(elf)), (NAME_GLYPH5_BASE, 5, name_chars(elf))):
        for k, c in enumerate(chars):
            sprites.append(render_text(elf, names, root, font, c))
            ids.append(base + k)
            ZIPPED.add(base + k)

    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import titledb
    for base, font in ((TITLE_GLYPH1_BASE, 1), (TITLE_GLYPH5_BASE, 5), (TITLE_GLYPH7_BASE, 7)):
        for k, c in enumerate(titledb.title_chars(titledb.title_strings(elf))):
            sprites.append(render_text(elf, names, root, font, c))
            ids.append(base + k)
            ZIPPED.add(base + k)

    sprites.append(render_infinity())
    ids.append(INFINITY_ID)

    with open(out, "wb") as f:
        f.write(b"WMN3" + struct.pack("<I", len(sprites)))
        for sid, im in zip(ids, sprites):
            px = im.tobytes()
            z = zlib.compress(px, 9) if sid in ZIPPED else b""
            f.write(struct.pack("<iHHI", sid, *im.size, len(z)) + (z or px))
        f.write(struct.pack("<I", len(anims)))
        for name, frames, nrec, packed in anims:
            f.write(name.encode().ljust(48, b"\0") + struct.pack("<II", len(frames), nrec))
            f.write(b"".join(struct.pack("<II", s, n) for s, n in frames))
            f.write(packed)
    px = sum(im.size[0] * im.size[1] for im in sprites)
    print(f"{len(sprites)} sprites ({px * 4 / 1e6:.1f} MB), {len(anims)} anims -> {out}")


if __name__ == "__main__":
    main()
