//
// Created by nik9 on 28.05.2022.
//

#ifndef MAIN_C_FONTS_H
#define MAIN_C_FONTS_H

#endif //MAIN_C_FONTS_H

typedef struct {
    const unsigned char FontWidth;
    const unsigned char FontHeight;
    const unsigned char *fontEn;
    const unsigned char *fontRu;
} FontDef;

// some available fonts
extern FontDef Font_7x9;
