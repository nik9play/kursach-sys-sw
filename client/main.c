#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>

void client_menu(); // Меню
int get_choice(int menu_items); // Выбор пункта меню
void ping_function(); // Проверка состояния сервера
void led_function(); // Приветствие
void shutdown_function(); // Остановка сервера
void cube_function(); // кубик
int create_packet(int function_code, char *message, int message_length, char **result); // Сборка пакета
void send_packet(char *packet, int packet_length); // Отправка пакета


int main() {
// Инициализируем dll
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
// Входим в функцию пользовательского взаимодействия с программой
    client_menu();
// Прекращаем использование dll
    WSACleanup();
    system("pause");
    return 0;
}

void client_menu() // Меню
{
    int choice = -1;
    while (choice != 0) {
        printf("\n1) Ping\n2) LED Brightness\n3) Shutdown server\n4) Cube \n0) exit\n\n");
        switch (choice = get_choice(4)) {
            case 1:
                ping_function();
                break;
            case 2:
                led_function();
                break;
            case 3:
                shutdown_function();
                break;
            case 4:
                cube_function();
                break;
            default:
                break;
        }
    }
}

int get_choice(int menu_items) // Выбор пункта меню
{
    int choice;
    printf("Enter Number: ");
    while (scanf("%d", &choice) != 1 || choice < 0 || choice > menu_items) {
        printf("\nIncorrect. Try again: ");
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
    }
    return choice;
}

void ping_function() {
// Формирование пакета
    char *packet;
    int packet_length = create_packet(0x42, NULL, 0, &packet);
// Отправка пакета
    send_packet(packet, packet_length);
// Освобождение указателя
    free(packet);
    packet = NULL;
}

void cube_function() {
// Формирование пакета
    char *packet;
    int packet_length = create_packet(0x44, NULL, 0, &packet);
// Отправка пакета
    send_packet(packet, packet_length);
// Освобождение указателя
    free(packet);
    packet = NULL;
}

void led_function() {
// считывание яркости
    unsigned short brightness = 0;
    scanf("%u", &brightness);

    if (brightness > 1000)
        brightness = 1000;

    char buf[2];
    memcpy(&buf[0], &brightness, 2);

// Формирование пакета
    char *packet;
    int packet_length = create_packet(0x43, buf, sizeof(unsigned short), &packet);
// Отправка пакета
    send_packet(packet, packet_length);
// Освобождение указателя
    free(packet);
    packet = NULL;
}

void shutdown_function() {
// Формирование строки с паролем
    char password[30], buff;
    int i = 0;
    printf("\nEnter the password: ");
    while ((getchar()) != '\n');
    while (buff != '\n') {
        buff = (char) getchar();
        password[i] = buff;
        i++;
    }
    password[i - 1] = '\0';
// Формирование пакета
    char *packet;
// Длина всего пакета (modbus + message)
    int packet_length = create_packet(0x45, password, strlen(password) + 1, &packet);
// Отправка пакета
    send_packet(packet, packet_length);
// Освобождение указателя
    free(packet);
    packet = NULL;
}

void send_packet(char *packet, int packet_length) {
// Создаем сокет
    SOCKET sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
// Инициируем запрос к серверу
    struct sockaddr_in sockAddr;
    memset(&sockAddr, 0, sizeof(sockAddr));
    sockAddr.sin_family = PF_INET;
    sockAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    sockAddr.sin_port = htons(502);
// Попытка установить соединение
    if (connect(sock, (SOCKADDR *) &sockAddr, sizeof(SOCKADDR))) {
        printf("\nServer connection error: %d\n", WSAGetLastError());
        return;
    }
    char szBuffer[MAXBYTE];
// Отправка пакета на сервер
    send(sock, packet, packet_length, 0);
// Получение данных, возвращаемых сервером
    recv(sock, szBuffer, MAXBYTE, 0);
    //char *buff = szBuffer;
// Сообщение от сервера (после 8 байт modbus)
    printf("\nServer responded\n");
// Закрываем сокет
    closesocket(sock);
}

int create_packet(int function_code, char *message, int message_length, char **result) {
// Структура modbus протокола
    char packet_buff[8] = {0x00, 0x00, // TransactionId - 2 bytes
                           0x00, 0x00, // Protocol - 2 bytes
                           0x00, message_length + 2, // MessageLength - 2 bytes (+ unitId 1 byte + function_code 1 byte)
                           0x00, // UnitId - 1 byte
                           function_code,}; // FunctionCode - 1 byte
// Размер пакета
    char packet_size = (char) ((8 + message_length) * sizeof(char)); // Вес структуры modbus + вес сообщения
// Выделение памяти для пакета
    char *packet = (char *) malloc(packet_size);
// Копирование данных в собираемый пакет modbus
    memcpy(packet, packet_buff, 8);
    if (message_length > 0) // Если пакет отправляется с сообщением
        memcpy(packet + 8, message, message_length); // Копирование сообщения после структуры modbus
    *result = packet;
    return (int) ((8 + message_length) * sizeof(char)); // Возвращаем вес структуры modbus + вес сообщения
}