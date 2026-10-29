#!/bin/sh
# Encode the menu and race BGM out of the arcade data (read only) to AAC for AVAudioPlayer,
# into data/sound/bgm: the entry, select and result music (src/menu.c MUSIC_ENTRY, MUSIC_SELECT,
# MUSIC_RESULT), the story ending's (MUSIC_ENDING) and the BGM select's songs of its three sound sources (BGM_SETS:
# 3/3DX/3DX+, Maximum Tune & 2, Wangan Midnight R). Tracks already encoded are kept.
set -e
cd "$(dirname "$0")"
SRC="../Wangan Midnight Maximum Tune 3DX+ (Japan)/data/sound/bgm"
mkdir -p data/sound/bgm
for f in maxi3plus/wm3DX_P_sys_01_ent maxi3plus/wm3DX_P_sys_02_sel maxi3/sys_04 maxi3/sys_05 \
         maxi3plus/wm3DX_P_race_01 maxi3plus/wm3DX_P_race_02 maxi3plus/wm3DX_P_race_03 maxi3plus/wm3DX_P_race_04 \
         maxi3dx/wm3DX_race_01_mainD \
         maxi3/wm3_01 maxi3/wm3_02 maxi3/wm3_03 maxi3/wm3_04 maxi3/wm3_05 maxi3/wm3_06 maxi3/wm3_07 maxi3/wm3_08 \
         maxi3/wm3_09 maxi3/wm3_10 maxi3/wm3_11 maxi3/wm3_12 maxi3/wm3_13 maxi3/wm3_14 maxi3/wm3_15 \
         maxi1/wm2_03 maxi1/wm2_05 maxi1/wm2_08 maxi1/wm2_18 \
         maxi2/wm2_01 maxi2/wm2_02 maxi2/wm2_04 maxi2/wm2_06 maxi2/wm2_07 maxi2/wm2_09 maxi2/wm2_10 maxi2/wm2_11 \
         maxi2/wm2_12 maxi2/wm2_13 maxi2/wm2_14 maxi2/wm2_15 maxi2/wm2_16 maxi2/wm2_17 maxi2/wm2_19 maxi2/wm2_20 \
         maxi2/wm2_21 maxi2/wm2_22 maxi2/wm2_23 \
         r/wmr_01 r/wmr_02 r/wmr_03 r/wmr_04 r/wmr_05 r/wmr_06 r/wmr_07 r/wmr_08 r/wmr_09 r/wmr_10 r/wmr_11 r/wmr_12; do
  out="data/sound/bgm/$(basename "$f").m4a"
  [ -f "$out" ] && continue
  # into a temporary name first, so a stopped run leaves no half-written track behind
  afconvert -f m4af -d aac -b 128000 "$SRC/$f.wav" "$out.part"
  mv "$out.part" "$out"
done
chmod 644 data/sound/bgm/*.m4a
ls -l data/sound/bgm
