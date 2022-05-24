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
#include <locale.h>
#include <string.h>
#include <wchar.h>
#include <time.h>
#pragma comment(lib, "ws2_32.lib")


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define SCALE_X 20
#define SCALE_Y 20
#define TRANSLATEX 64
#define TRANSLATEY 32

uint8_t SCREEN_BUFFER[128 * 64 / 8];

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

struct POINT {
    int x;
    int y;
};

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

int CORNER_INDEXES[12][2] = {
        {0,1},
        {1,3},
        {3,2},
        {2,0},
        {0,4},
        {1,5},
        {2,6},
        {3,7},
        {4,5},
        {5,7},
        {7,6},
        {6,4}
};

struct POINT rotated_points[8] = {};

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

void rotate_point(float *x, float *y, float *z, float ax, float ay, float az) {
    float rotatedX, rotatedY, rotatedZ;

    // x axis
    rotatedX = *x;
    rotatedY = (*y * cosf(ax) - *z * sinf(ax));
    rotatedZ = (*y * (float)sinf(ax) + *z * (float)cosf(ax));
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
    point->x = (int)(x * SCALE_X + TRANSLATEX);
    point->y = (int)(y * SCALE_Y + TRANSLATEY);
}

void draw_cube(hid_device* handle)
{
    int exit = 0;
    int16_t previous_slider = 0;

    while (!exit) {
        int16_t slider = 0;
        unsigned char buf[256];
        buf[0] = 0x03; // код функции

        int res = hid_get_feature_report(handle, buf, 3);

        memcpy(&slider, &buf[0]+1, 2);
        if (slider > 1500) {
            slider = 1500;
        }
        if (abs(previous_slider - slider) <= 40) {
            slider = previous_slider;
        }

        float ay = (((float)slider - 0) / (2000 - 0)) * (M_PI);

        printf("%d %d %d %f\n", slider, buf[1], buf[2], ay);


        for (int i=0; i<8; i++) {
            float x = CUBE_CORNERS[i][0];
            float y = CUBE_CORNERS[i][1];
            float z = CUBE_CORNERS[i][2];
            rotate_point(&x,&y,&z,0.3f,ay,0.0f);

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
            struct POINT from = rotated_points[CORNER_INDEXES[i][0]];
            struct POINT to = rotated_points[CORNER_INDEXES[i][1]];
            //printf("from: %d %d==to: %d %d\n", from.x, from.y, to.x, to.y);
            plot_line(from.x, from.y, to.x, to.y);
        }
        previous_slider = slider;
        update_screen(handle);
    }
}

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
#define MAX_STR 255
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
/*
devs = hid_enumerate(0x0, 0x0);
cur_dev = devs;
while (cur_dev) {
printf("Device Found\n type: %04hx %04hx\n path: %s\n serial_number: %ls", cur_dev->vendor_id, cur_dev->product_id, cur_dev->path, cur_dev->serial_number);
printf("\n");
printf(" Mandufacturer: %ls\n", cur_dev->manufacturer_string);
printf(" Product: %ls\n", cur_dev->product_string);
printf(" Release: %hx\n", cur_dev->release_number);
printf(" Interface: %d\n", cur_dev->interface_number);
printf(" Usage (page): 0x%hx (0x%hx)\n", cur_dev->usage, cur_dev->usage_page);
printf("\n");
cur_dev = cur_dev->next;
}
hid_free_enumeration(devs);
*/
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
// рисование квадрата
/*
for (int i = 0; i < 30; i++) {
for (int j = 0; j < 30; j++) {
buf[0] = 0x04;
buf[1] = 15 + i; //
buf[2] = 15 + j;
buf[3] = 0x01;
res = hid_send_feature_report(handle,buf,4);
}
}*/
// keys
// Read a Feature Report from the device
    while(1)
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

        switch (function_code) {
            case 0x42:
// Вычисление длины ответа
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

                buf[0] = 0x02; // код функции
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