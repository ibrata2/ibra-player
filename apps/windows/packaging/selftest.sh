#!/usr/bin/env bash
# Generate real media in many formats and play each one through iBRA PLAYER.
# Usage: selftest.sh "<path to iBRA PLAYER.exe>" <work-dir>
set -uo pipefail
EXE="$1"
WORK="$2"
MEDIA="$WORK/media"
OUT="$WORK/report"
mkdir -p "$MEDIA" "$OUT"
cd "$MEDIA"
Q="-hide_banner -loglevel error -y"
V="-f lavfi -i testsrc2=size=640x360:rate=25:duration=8"
A="-f lavfi -i sine=frequency=440:sample_rate=48000:duration=8"
S="-f lavfi -i sine=frequency=523:sample_rate=48000:duration=6"

cat > ar.srt <<'EOF'
1
00:00:00,500 --> 00:00:07,500
مرحباً بكم في iBRA PLAYER
نص مختلط: Arabic + English 123
EOF
mk() { ffmpeg $Q "$@" || echo "skip: ${@: -1}"; }
mk $V $A -c:v libx264 -preset ultrafast -c:a aac -shortest v01_h264_aac.mp4
cp ar.srt v01_h264_aac.srt
mk $V $A -c:v libx265 -preset ultrafast -x265-params log-level=error -c:a ac3 -shortest v02_hevc_ac3.mkv
mk $V $A -c:v libsvtav1 -preset 12 -c:a libopus -shortest v03_av1_opus.webm
mk $V $A -c:v libvpx-vp9 -deadline realtime -cpu-used 8 -b:v 500k -c:a libopus -shortest v04_vp9.webm
mk $V $A -c:v libvpx -deadline realtime -cpu-used 8 -b:v 500k -c:a libvorbis -shortest v05_vp8.webm
mk $V $A -c:v mpeg4 -vtag xvid -c:a libmp3lame -shortest v06_xvid.avi
mk $V $A -c:v mpeg2video -b:v 2M -c:a ac3 -shortest -f vob v07_dvd.vob
mk $V $A -c:v libx264 -preset ultrafast -c:a aac -shortest v08.ts
mk $V $A -c:v wmv2 -b:v 1M -c:a wmav2 -shortest v09_wmv.wmv
mk $V $A -c:v flv -c:a libmp3lame -ar 44100 -shortest v10.flv
mk $V $A -c:v libx264 -preset ultrafast -c:a alac -shortest v11_alac.mov
mk $V $A -c:v libtheora -q:v 6 -c:a libvorbis -shortest v12.ogv
mk $V $A -c:v libx264 -preset ultrafast -c:a eac3 -shortest -f mpegts v13.m2ts
mk $V $A -c:v mpeg1video -b:v 1M -c:a mp2 -shortest v14_mpeg1.mpg
mk -f lavfi -i testsrc2=size=3840x2160:rate=24:duration=6 -c:v libx265 -preset ultrafast -pix_fmt yuv420p10le \
   -x265-params "log-level=error:colorprim=bt2020:transfer=smpte2084:colormatrix=bt2020nc:hdr10=1" v15_4k_hdr10.mkv
mk -f lavfi -i testsrc2=size=640x360:rate=25:duration=8 -c:v libvvenc -preset faster v16_vvc.mkv
mk $S -c:a libmp3lame a01.mp3
mk $S -c:a flac a02.flac
mk $S -c:a pcm_s16le a03.wav
mk $S -c:a libopus a04.opus
mk $S -c:a aac a05.m4a
mk $S -c:a dca -strict -2 a06.dts
mk $S -c:a truehd -strict -2 a07_truehd.mka
mk $S -c:a wmav2 a08.wma
mk $S -c:a libopencore_amrnb -ar 8000 -ac 1 -b:a 12.2k a09.amr
head -c 60000 /dev/urandom > z_corrupt.mkv
ls -la

"$EXE" --selftest "$(cygpath -w "$OUT")" $(for f in "$MEDIA"/*; do case "$f" in *.srt) ;; *) cygpath -w "$f";; esac; done)
echo "player exit code: $?"
