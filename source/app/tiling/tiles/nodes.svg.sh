#!/bin/bash
#
function gen_svg() {
i=${1}
j=${2}
n=${3}
m=8
s=$((5*m))
h=$((s/2))
p=5
cat << EOD > ./nodes_${n}.svg
<?xml version="1.0" encoding="UTF-8"?>
<svg
   xmlns:svg="http://www.w3.org/2000/svg"
   xmlns="http://www.w3.org/2000/svg"
   width="${s}"
   height="${s}"
   viewBox="0 0 ${s} ${s}"
   version="1.1"
   id="svg_tile_${n}">
  <rect
    style="fill:black;fill-opacity:1;stroke:none"
    x="0" y="0" width="${s}" height="${s}"
    id="tile_bg" />
  <path
    style="fill:none;stroke:white;stroke-width:${p}"
    d="M -1 ${h} L 1 ${h} C 2 ${h} $(((i+1)*m-1)) $(((j+1)*m)) $(((i+1)*m)) $(((j+1)*m))"
    id="path_l" />
  <path
    style="fill:none;stroke:white;stroke-width:${p}"
	d="M $((s+1)) ${h} L $((s-1)) ${h} C $((s-2)) ${h} $(((i+1)*m+1)) $(((j+1)*m)) $(((i+1)*m)) $(((j+1)*m))"
    id="path_r" />
  <path
    style="fill:none;stroke:white;stroke-width:${p}"
	d="M ${h} -1 L ${h} 1 C ${h} 2 $(((i+1)*m)) $(((j+1)*m-1)) $(((i+1)*m)) $(((j+1)*m))"
    id="path_t" />
  <path
    style="fill:none;stroke:white;stroke-width:${p}"
	d="M ${h} $((s+1)) L ${h} $((s-1)) C ${h} $((s-2)) $(((i+1)*m-1)) $(((j+1)*m)) $(((i+1)*m)) $(((j+1)*m))"
    id="path_t" />
  <circle
    style="fill:white;stroke:white;stroke-width:${p}"
    cx="$(((i+1)*m))" cy="$(((j+1)*m))" r="${p}"
    id="node" />
</svg>
EOD
}

gen_svg 0 0 0
gen_svg 1 0 1
gen_svg 2 0 2
gen_svg 3 0 3
gen_svg 0 1 4
gen_svg 1 1 5
gen_svg 2 1 6
gen_svg 3 1 7
gen_svg 0 2 8
gen_svg 1 2 9
gen_svg 2 2 A
gen_svg 3 2 B
gen_svg 0 3 C
gen_svg 1 3 D
gen_svg 2 3 E
gen_svg 3 3 F
