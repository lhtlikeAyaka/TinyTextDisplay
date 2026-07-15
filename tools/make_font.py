from PIL import Image, ImageFont, ImageDraw
import os

SIZE = 16
OUTPUT = "font.h"
# 字体
FONT = "NotoSerifCJKsc-Regular.otf" #自行更改文件名


font = ImageFont.truetype(
    FONT,
    SIZE
)

# 生成字符集合

chars = set()
# 3500常用汉字
with open(
    "3500.txt", #自行更改文件名
    "r",
    encoding="utf-8"
) as f:

    chars.update(
        f.read()
    )
# 日文假名

chars.update(
"""
ぁあぃいぅうぇえぉお
かきくけこ
がぎぐげご
さしすせそ
ざじずぜぞ
たちつてと
だぢづでど
なにぬねの
はひふへほ
ばびぶべぼ
ぱぴぷぺぽ
まみむめも
やゆよ
らりるれろ
わをん

ァアィイゥウェエォオ
カキクケコ
ガギグゲゴ
サシスセソ
ザジズゼゾ
タチツテト
ダヂヅデド
ナニヌネノ
ハヒフヘホ
バビブベボ
パピプペポ
マミムメモ
ヤユヨ
ラリルレロ
ワヲン
"""
)

# ASCII
chars.update(
chr(i)
for i in range(32,127)
)

# 标点
chars.update(
"""
，。！？；：“”‘’（）【】《》
、·…—～「」『』
,.!?;:'"()[]<>
+-=*/%#@&_
"""
)

# 去掉换行
chars = [
c for c in chars
if c not in "\n\r\t"
]
chars = sorted(chars)
print(
"总字符数:",
len(chars)
)
# 生成点阵
def make_bitmap(ch):
    img = Image.new(
        "1",
        (16,16),
        0
    )
    draw = ImageDraw.Draw(img)
    box = draw.textbbox(
        (0,0),
        ch,
        font=font
    )
    w = box[2]-box[0]
    h = box[3]-box[1]
    x=(16-w)//2-box[0]
    y=(16-h)//2-box[1]
    draw.text(
        (x,y),
        ch,
        font=font,
        fill=1
    )
    data=[]
    for y in range(16):
        for x in range(0,16,8):
            b=0
            for bit in range(8):
                if img.getpixel(
                    (x+bit,y)
                ):
                    b |= 1<<(7-bit)
            data.append(b)
    return data
# 输出 font.h
with open(
    OUTPUT,
    "w",
    encoding="utf-8"
) as f:


    f.write(
"""#ifndef FONT_H
#define FONT_H

#include <Arduino.h>

typedef struct
{
uint16_t code;
uint8_t data[32];

}Font16;


const Font16 font16[] PROGMEM =
{
"""
    )


    for ch in chars:

        data=make_bitmap(ch)


        f.write(
            "{0x%04X,{%s}}, // %s\n"
            %
            (
                ord(ch),
                ",".join(
                    "0x%02X"%x
                    for x in data
                ),
                ch
            )
        )


    f.write(
"""
};


const int font16_count =
sizeof(font16)/sizeof(font16[0]);


#endif
"""
    )



print("font.h生成完成")
