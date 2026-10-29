#!/bin/sh
# Encode the menu and race BGM out of the arcade data (read only) to AAC for AVAudioPlayer,
# into data/sound/bgm: the entry, select and result music (src/menu.c MUSIC_ENTRY, MUSIC_SELECT,
# MUSIC_RESULT), the story ending's (MUSIC_ENDING) and the twenty songs of the BGM select (SONG_FILE). Tracks already encoded are kept.
set -e
cd "$(dirname "$0")"
SRC="../Wangan Midnight Maximum Tune 3DX+ (Japan)/data/sound/bgm"
mkdir -p data/sound/bgm
for f in maxi3plus/wm3DX_P_sys_01_ent maxi3plus/wm3DX_P_sys_02_sel maxi3/sys_04 maxi3/sys_05 \
         maxi3plus/wm3DX_P_race_01 maxi3plus/wm3DX_P_race_02 maxi3plus/wm3DX_P_race_03 maxi3plus/wm3DX_P_race_04 \
         maxi3dx/wm3DX_race_01_mainD \
         maxi3/wm3_01 maxi3/wm3_02 maxi3/wm3_03 maxi3/wm3_04 maxi3/wm3_05 maxi3/wm3_06 maxi3/wm3_07 maxi3/wm3_08 \
         maxi3/wm3_09 maxi3/wm3_10 maxi3/wm3_11 maxi3/wm3_12 maxi3/wm3_13 maxi3/wm3_14 maxi3/wm3_15; do
  out="data/sound/bgm/$(basename "$f").m4a"
  [ -f "$out" ] && continue
  # into a temporary name first, so a stopped run leaves no half-written track behind
  afconvert -f m4af -d aac -b 128000 "$SRC/$f.wav" "$out.part"
  mv "$out.part" "$out"
done
chmod 644 data/sound/bgm/*.m4a
ls -l data/sound/bgm
