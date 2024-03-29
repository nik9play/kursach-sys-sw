//#define NO_BOARD

#define WIN32

#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <stdlib.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include "hidapi.h"
#include <stdint.h>
#include <math.h>
#define MAX_STR 255
#include <string.h>
#include <wchar.h>
#include <time.h>
#include "fonts.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define SCALE_X 20
#define SCALE_Y 20
#define TRANSLATE_X 64
#define TRANSLATE_Y 32

uint8_t SCREEN_BUFFER[SCREEN_WIDTH * SCREEN_HEIGHT / 8];

struct POINT {
    int x;
    int y;
};

void update_screen(hid_device* handle);
void draw_pixel(int x, int y, int color);
int plot_line(int x0, int y0, int x1, int y1);
int plot_triangle(int x0, int y0, int x1, int y1, int x2, int y2);
void rotate_point(float *x, float *y, float *z, float ax, float ay, float az);
void adjust_point(float x, float y, struct POINT *point);
void draw_cube(hid_device* handle);
void draw_string_to_buffer(int x, int y, char* str);

float CUBE_CORNERS[8][3] = {
        {-1, -1, -1},
        { 1, -1, -1},
        {-1, -1 , 1},
        { 1, -1,  1},
        {-1,  1, -1},
        { 1,  1, -1},
        {-1,  1,  1},
        { 1,  1,  1}
};

int TRIANGLE_INDEXES[12][3] = {
        {0, 4, 5},
        {0, 1, 5},

        {4, 5, 6},
        {6, 7, 5},

        {2, 6, 7},
        {2, 3, 7},

        {7, 5, 1},
        {3, 1, 7},

        {2, 3, 1},
        {0, 1, 2},

        {2, 0, 4},
        {4, 5, 0}
};

struct POINT rotated_points[8] = {};

int main(int argc, char* argv[])
{
    // Инициализируем DLL
    WSADATA wsaData;
    WSAStartup( MAKEWORD(2, 2), &wsaData);
    // Создаем сокет
    SOCKET servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    // Привязываем сокет
    struct sockaddr_in sockAddr;
    memset (& sockAddr, 0, sizeof (sockAddr)); // Каждый байт заполняется 0
    sockAddr.sin_family = PF_INET; // Использовать IPv4-адрес
    sockAddr.sin_addr.s_addr = inet_addr ("0.0.0.0"); // Определенный IP-адрес
    sockAddr.sin_port = htons (502); // Порт
    bind(servSock, (SOCKADDR*)&sockAddr, sizeof(SOCKADDR));
    // Входим в состояние мониторинга
    listen(servSock, 20);
    // Получение клиентского запроса
    SOCKADDR clntAddr;
    int nSize;
    char *str = NULL;
    int full_answer_length;
    char *szBuffer[MAXBYTE] = {0};
    SOCKET clntSock;

    // Структура modbus протокола
    char packet_buff[8] = { 0x00, 0x00, // TransactionId - 2 bytes
                            0x00, 0x00, // Protocol - 2 bytes
                            0x00, 0x00, // MessageLength - 2 bytes
                            0x00, // UnitId - 1 byte
                            0x00 // FunctionCode - 1 byte
    };

    int res;
    unsigned char buf[256];
    wchar_t wstr[MAX_STR];
    hid_device *handle;
    int i;

    struct hid_device_info;

    printf("hidapi test/example tool. Compiled with hidapi version %s, runtime version %s.\n", HID_API_VERSION_STR, hid_version_str());
    if (hid_version()->major == HID_API_VERSION_MAJOR && hid_version()->minor == HID_API_VERSION_MINOR && hid_version()->patch == HID_API_VERSION_PATCH) {
        printf("Compile-time version matches runtime version of hidapi.\n\n");
    }
    else {
        printf("Compile-time version is different than runtime version of hidapi.\n]n");
    }

    if (hid_init())
        return -1;

    // Set up the command buffer.
    memset(buf,0x00,sizeof(buf));
    buf[0] = 0x01;
    buf[1] = 0x81;

    // Open the device using the VID, PID,
    // and optionally the Serial number.
    handle = hid_open(0x1234, 0x0001, NULL);
    if (!handle) {
        printf("unable to open device\n");
        return 1;
    }

    // Read the Manufacturer String
    wstr[0] = 0x0000;
    res = hid_get_manufacturer_string(handle, wstr, MAX_STR);
    printf("Manufacturer String: %ls\n", wstr);

    // Read the Product String
    wstr[0] = 0x0000;
    res = hid_get_product_string(handle, wstr, MAX_STR);
    printf("Product String: %ls\n", wstr);

    // Read the Serial Number String
    wstr[0] = 0x0000;
    res = hid_get_serial_number_string(handle, wstr, MAX_STR);
    printf("Serial Number String: (%d) %ls", wstr[0], wstr);
    printf("\n");

    // Read Indexed String 1
    wstr[0] = 0x0000;
    res = hid_get_indexed_string(handle, 1, wstr, MAX_STR);
    printf("Indexed String 1: %ls\n", wstr);

    // Очистка экрана
    for (int i = 0; i < 128; i++) {
        for (int j = 0; j < 64; j++) {
            draw_pixel(i, j, 0);
        }
    }
    update_screen(handle);
    // keys
    // Read a Feature Report from the device
    int exit = 0;

    while (!exit)
    {
        nSize = sizeof(SOCKADDR);
        clntSock = accept(servSock, (SOCKADDR*)&clntAddr, &nSize);
        // Получение пакета от клиента
        recv(clntSock, szBuffer, MAXBYTE, 0);
        char *buff = szBuffer;
        char function_code = buff[7]; // Код функции
        // Сообщение с кодом функции
        printf("Request with code %d\n", function_code);
        // Ответ сервера
        char answer[50];
        // Длина сообщения с ответом сервера
        int message_length;
        // Выполнение функций
        char server_password[] = "serverpass";

        switch (function_code) {
            case 0x42:
                // Вычисление длины байта
                full_answer_length = 31 * sizeof(char); // 8 + 22 + 1
                // Выделение памяти под ответ
                str = malloc(full_answer_length);
                // Копирование структуры modbus
                memcpy(str, packet_buff, 8);
                // Код функции
                str[7] = (char)0x42;
                // message_length + 2 (unitId + function_code)
                str[5] = (char)(22 + 1 + 2);
                // Запись сообщения после структуры modbus
                strcpy(str + 8, "The server is running!");
                break;
            case 0x43:
                full_answer_length = 10 * sizeof(char);
                str = malloc(full_answer_length);
                memcpy(str, packet_buff, 8);
                str[7] = (char)0x43;
                str[5] = (char)(2 + 2);
                str[8] = buff[8];
                str[9] = buff[9];

                buf[0] = 0x02; // Код функции
                buf[1] = buf[3] = buf[5] = buff[8];
                buf[2] = buf[4] = buf[6] = buff[9];

                res = hid_send_feature_report(handle,buf,7);

                break;
            case 0x44:
                full_answer_length = 8 * sizeof(char);
                str = malloc(full_answer_length);
                memcpy(str, packet_buff, 8);
                str[7] = (char)0x44;
                str[5] = (char)(2);

                draw_cube(handle);

                for (int i = 0; i < 128; i++) {
                    for (int j = 0; j < 64; j++) {
                        draw_pixel(i, j, 0);
                    }
                }
                update_screen(handle);

                break;
            case 0x45:
                // Сравнение пароля сервера с пришедшим от клиента
                if(strcmp(server_password, buff + 8) == 0) {
                    exit = 1;
                    sprintf(answer, "Shutdown the server!");
                }
                else {
                    sprintf(answer, "Wrong password!");
                }
                // Длина сообщения от сервера
                message_length = (int)strlen(answer);
                // Длина полного ответа от сервера
                full_answer_length = (int)((8 + strlen(answer) + 1) * sizeof(char));
                // Выделение памяти на ответ сервера
                str = malloc(full_answer_length);
                // Копирование структуры modbus
                memcpy(str, packet_buff, 8);
                // Код функции
                str[7] = (char)0x44;
                // message_length + 2 + '\0'
                str[5] = (char)(2 + message_length + 1);
                // Копирование ответа от сервера
                strcpy(str + 8, answer);
                break;

        }

        send(clntSock, str, full_answer_length, 0);
        // Освобождение указателя
        free(str);
        str = NULL;
        printf("Answer sent\n");
    }
    return 0;
}

void update_screen(hid_device* handle) {
    uint16_t size = 32;
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));
    buf[0] = 0x06;
    *(uint16_t*)&buf[1] = size;
    for (uint16_t i = 0; i < 32; i++)
    {
        uint16_t offset = size * i;
        *(uint16_t*)&buf[3] = offset;
        memcpy(buf + 5, SCREEN_BUFFER + offset, size);
        int res = hid_send_feature_report(handle, buf, sizeof(buf));
        if (res < 0)
        {
            printf("error");
        }
    }
}

void draw_pixel(int x, int y, int color) {
    if (x >= 0 && y >= 0 && x < SCREEN_WIDTH && y < SCREEN_HEIGHT)
    {
        if (color == 1)
        {
            SCREEN_BUFFER[SCREEN_WIDTH * (y / 8) + x] |= 1 << (y % 8);
        }
        else
        {
            SCREEN_BUFFER[SCREEN_WIDTH * (y / 8) + x] &= ~(1 << (y % 8));
        }
    }
}

int plot_line(int x0, int y0, int x1, int y1)
{
    int dx =  abs (x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs (y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2; /* error value e_xy */

    int i = 0;

    for (;;) {  /* loop */
        draw_pixel(x0, y0, 1);
        i++;
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; } /* e_xy+e_x > 0 */
        if (e2 <= dx) { err += dx; y0 += sy; } /* e_xy+e_y < 0 */
    }

    return i;
}

int plot_triangle(int x0, int y0, int x1, int y1, int x2, int y2) {
    plot_line(x0, y0, x1, y1);
    plot_line(x1, y1, x2, y2);
    plot_line(x2, y2, x0, y0);
}

void rotate_point(float *x, float *y, float *z, float ax, float ay, float az) {
    float rotatedX, rotatedY, rotatedZ;

    // x axis
    rotatedX = *x;
    rotatedY = (*y * cosf(ax) - *z * sinf(ax));
    rotatedZ = (*y * sinf(ax) + *z * cosf(ax));
    *x = rotatedX;
    *y = rotatedY;
    *z = rotatedZ;

    // y axis
    rotatedX = (*z * sinf(ay) + *x * cosf(ay));
    rotatedY = *y;
    rotatedZ = (*z * cosf(ay) - *x * sinf(ay));
    *x = rotatedX;
    *y = rotatedY;
    *z = rotatedZ;

    // z axis
    rotatedX = (*x * cosf(az) - *y * sinf(az));
    rotatedY = (*x * sinf(az) + *y * cosf(az));
    rotatedZ = *z;
    *x = rotatedX;
    *y = rotatedY;
    *z = rotatedZ;
}

void adjust_point(float x, float y, struct POINT *point) {
    point->x = (int)(x * SCALE_X + TRANSLATE_X);
    point->y = (int)(y * SCALE_Y + TRANSLATE_Y);
}

void draw_cube(hid_device* handle)
{
    int exit = 0;
    int16_t previous_slider = 0;

    int top_text_x = 0;
    int top_text_direction = 1;

    int bottom_text_x = 0;
    int bottom_text_direction = 1;
    int axis = 0;
    float ax = 0.3f, ay = 0.3f, az = 0.3f;



    while (!exit) {
        int16_t slider = 0;
        unsigned char buf[256];
        buf[0] = 0x03;

        hid_get_feature_report(handle, buf, 3);

        memcpy(&slider, &buf[0]+1, 2);
        if (slider > 1500) {
            slider = 1500;
        }
        if (abs(previous_slider - slider) <= 30) {
            slider = previous_slider;
        }

        float rotate = (((float)slider - 0) / (1500 - 0)) * (float)M_PI;

        buf[0] = 0x01;
        hid_get_feature_report(handle, buf, 2);

        switch (buf[1]) {
            case 0x01:
                axis = 0;
                break;
            case 0x02:
                axis = 1;
                break;
            case 0x04:
                axis = 2;
                break;
            case 0x08:
                exit = 1;
                continue;
        }

        switch(axis) {
            case 0:
                ax = rotate;
                break;
            case 1:
                ay = rotate;
                break;
            case 2:
                az = rotate;
                break;
        }

        // printf("%d %d %d %f\n", slider, buf[1], buf[2], rotate);

        for (int i=0; i<8; i++) {
            float x = CUBE_CORNERS[i][0];
            float y = CUBE_CORNERS[i][1];
            float z = CUBE_CORNERS[i][2];
            rotate_point(&x,&y,&z,ax,ay,az);

            struct POINT point;
            adjust_point(x,y,&point);

            rotated_points[i] = point;
        }

        for (int i = 0; i < 128; i++) {
            for (int j = 0; j < 64; j++) {
                draw_pixel(i, j, 0);
            }
        }

        for (int i=0; i<12; i++) {
            plot_triangle(rotated_points[TRIANGLE_INDEXES[i][0]].x, rotated_points[TRIANGLE_INDEXES[i][0]].y,
                          rotated_points[TRIANGLE_INDEXES[i][1]].x, rotated_points[TRIANGLE_INDEXES[i][1]].y,
                          rotated_points[TRIANGLE_INDEXES[i][2]].x, rotated_points[TRIANGLE_INDEXES[i][2]].y);
        }

        previous_slider = slider;

        char string[32];
        snprintf(string, 31, "ax: %.2f ay: %.2f az: %.2f", ax, ay, az);
        draw_string_to_buffer(top_text_x, 0, string);

        if (top_text_direction)
            top_text_x -= 3;
        else
            top_text_x += 3;

        if (top_text_x >= 2)
            top_text_direction = 1;

        if (top_text_x <= -56)
            top_text_direction = 0;

        draw_string_to_buffer(bottom_text_x, 56, "Button 1 - x axis, Button 2 - y axis, Button 3 - z axis, Button 4 - exit");

        if (bottom_text_direction)
            bottom_text_x -= 3;
        else
            bottom_text_x += 3;

        if (bottom_text_x >= 2)
            bottom_text_direction = 1;

        if (bottom_text_x <= -384)
            bottom_text_direction = 0;

        printf("%d %d \n", bottom_text_x, bottom_text_direction);

        update_screen(handle);
    }
}

void draw_string_to_buffer(int x, int y, char* str)
{
    uint32_t pixel;
    int font_x, font_y;

    while(*str)
    {
        // write char to display buffer
        for (font_y = 0; font_y < Font_7x9.FontHeight; font_y++)
        {
            char ch = *str;

            // get font pixel
            if(ch < 127)
                pixel = Font_7x9.fontEn[(ch - 32) * Font_7x9.FontHeight + font_y];
            else
                pixel = Font_7x9.fontRu[(ch - 192) * Font_7x9.FontHeight + font_y];
            // write pixel to display buffer
            font_x = Font_7x9.FontWidth;
            while(font_x--)
            {
                if (pixel & 0x0001)
                {
                    draw_pixel(x + font_x, y + font_y, 1);
                }
                else
                {
                    draw_pixel(x + font_x, y + font_y, 0);
                }
                pixel >>= 1;
            }
        }

        x += Font_7x9.FontWidth;

        str++;
    }
}