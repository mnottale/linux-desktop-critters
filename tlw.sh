#! /bin/bash

tlw=$(wmctrl -l |cut -d\  -f 1 | sed -re 's/0x(0)*/0x/' |xargs echo |sed -e 's/\s/|/g')

xwininfo -root -tree |egrep $tlw |egrep -o '[0-9]+x[0-9]+\+[0-9]+\+[0-9]+'