#!/bin/bash

set -e

cd "$(dirname "$0")"
PYTHON_BIN="${PYTHON_BIN:-python3}"

THAI_INTERVALS=(--additional-intervals 0x0E00,0x0E7F)
CJK_INTERVALS=(
  --additional-intervals 0x2E80,0x303F
  --additional-intervals 0x3400,0x4DBF
  --additional-intervals 0x4E00,0x9FFF
)
CLOUDLOOP_FONT_SIZES=(12 14 16 18 20)
NOTOSANS_THAI_LOOPED_REGULAR_SIZES=(8 10 12 14 16 18 20)
NOTOSANS_THAI_LOOPED_UI_BOLD_SIZES=(10 12 14 16 18 20)
NOTOSANSSC_REGULAR_SIZES=(8 10 12)
NOTOSANSSC_BOLD_SIZES=(10 12)

"$PYTHON_BIN" build_notosanssc_subset.py

for size in ${CLOUDLOOP_FONT_SIZES[@]}; do
  font_name="cloudloop_${size}_regular"
  font_path="../builtinFonts/source/CloudLoop/CloudLoop-Regular.otf"
  output_path="../builtinFonts/${font_name}.h"
  "$PYTHON_BIN" fontconvert.py $font_name $size $font_path --2bit --compress "${THAI_INTERVALS[@]}" > $output_path
  echo "Generated $output_path"
done

for size in ${NOTOSANS_THAI_LOOPED_REGULAR_SIZES[@]}; do
  font_name="notosansthailooped_${size}_regular"
  font_path="../builtinFonts/source/NotoSansThaiLooped/NotoSansThaiLooped-Regular.ttf"
  output_path="../builtinFonts/${font_name}.h"
  "$PYTHON_BIN" fontconvert.py $font_name $size $font_path --2bit --compress "${THAI_INTERVALS[@]}" > $output_path
  echo "Generated $output_path"
done

for size in ${NOTOSANS_THAI_LOOPED_UI_BOLD_SIZES[@]}; do
  font_name="notosansthailooped_${size}_bold"
  font_path="../builtinFonts/source/NotoSansThaiLooped/NotoSansThaiLooped-Bold.ttf"
  output_path="../builtinFonts/${font_name}.h"
  "$PYTHON_BIN" fontconvert.py $font_name $size $font_path --2bit --compress "${THAI_INTERVALS[@]}" > $output_path
  echo "Generated $output_path"
done

for size in ${NOTOSANSSC_REGULAR_SIZES[@]}; do
  font_name="notosanssc_${size}_regular"
  font_path="../builtinFonts/source/NotoSansSC/NotoSansSC-Subset-Regular.otf"
  output_path="../builtinFonts/${font_name}.h"
  "$PYTHON_BIN" fontconvert.py $font_name $size $font_path --2bit --compress "${CJK_INTERVALS[@]}" > $output_path
  echo "Generated $output_path"
done

for size in ${NOTOSANSSC_BOLD_SIZES[@]}; do
  font_name="notosanssc_${size}_bold"
  font_path="../builtinFonts/source/NotoSansSC/NotoSansSC-Subset-Bold.otf"
  output_path="../builtinFonts/${font_name}.h"
  "$PYTHON_BIN" fontconvert.py $font_name $size $font_path --2bit --compress "${CJK_INTERVALS[@]}" > $output_path
  echo "Generated $output_path"
done

echo ""
echo "Running compression verification..."
"$PYTHON_BIN" verify_compression.py ../builtinFonts/
