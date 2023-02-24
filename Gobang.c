#include <stdio.h>
#include <Windows.h>
#include <stdlib.h>
#include <conio.h>
#include <stdarg.h>
#include <time.h>

#pragma comment(lib, "Ws2_32.lib")


#define IPADDR "255.255.255.255"
#define DEBUG_IPADDR "127.0.0.1"
#define TCP_PORT 35486
#define UDP_PORT 35487
#define ROW 20
#define COLUMN 20
#define TIMEOUT 1000
#define FREQUENCY 500
#define FLAG_DEFAULT 0
#define FLAG_MATCH 1
#define FLAG_INVITE 2
#define FLAG_JOIN 3
#define FLAG_MATCHED 4
#define FLAG_MACHINE 5
#define CHESS_WHITE 1
#define CHESS_BLACK 2
#define KEY_ENTER 13
#define KEY_ESC 27
#define KEY_UP 72
#define KEY_DOWN 80
#define KEY_LEFT 75
#define KEY_RIGHT 77
#define KEY_FLUSH 255
#define KEY_SPACE 32
#define KEY_W 119
#define KEY_S 115
#define KEY_A 97
#define KEY_D 100
#define MODE_DEFAULT 0
#define MODE_TCP 1
#define MODE_UDP 2
#define DISABLE 0
#define ENABLE 1
#define TIMEOUT 120

#define DEVELOPER_MODE 0

typedef struct {
	char name[64];
	void (*function[255])(char* []);
}COMMAND;
typedef struct sockaddr_in SO_IN;

WINBASEAPI HWND WINAPI GetConsoleWindow();
const char* MATCH_MSG = "GoBang-Match";
HWND hwnd = NULL;
HDC hdc = NULL, hdccpy = NULL;
HPEN hpen = NULL;
SOCKET udp_listener = INVALID_SOCKET;
SOCKET tcp_listener = INVALID_SOCKET;
SOCKET conn = INVALID_SOCKET;
RECT rect, all;
HBRUSH wBrush;
HBRUSH bBrush;
HBRUSH bkBrush;
int enable, chess, chessCount, isFinished, width, height, flag = 0, board[ROW][COLUMN], pointX, pointY, putX, putY, offsetWidth, offsetHeight, totalWidth, totalHeight, unit;
struct hostent** self_ip;
unsigned long LANIP[3][2];
int weight[][5] = { {7,15,400,1800,10000},{35,0,0,0,0},{800,0,0,0,0},{15000,0,0,0,0},{80000,0,0,0,0 } };

HANDLE cthread(LPTHREAD_START_ROUTINE);
SOCKET tcp_listen();
SOCKET tcp_connect(SO_IN);
int udp_listen(SO_IN*);
int udp_send();
void fast_match();
void invite_match();
void join_match();
void machine_match();
void initEnv();
void setPen(int, int, int);
void moveTo(int, int);
void drawBoard();
void startGame();
void put(int, int, int);
void flush(int);
void setMsg(char*);
void setMsgColor(int, int, int);
void run(char*);
char* my_itoa(int);
char* createCommand(int, ...);
int isWin(int, int);
int isValid(int, int);
int isSelf(SO_IN);
IN_ADDR getLANIP();
void cls();
void reset();
wchar_t* chtowc(char*);
void calcPut(int*,int*);


void command_init(char* args[]) {
    if (args[1][0] == '1')enable = ENABLE;
    if (args[2][0] == '1')chess = CHESS_BLACK;
}

void command_put(char* args[]) {
    int row = atoi(args[1]), col = atoi(args[2]);
    setMsgColor(0x00, 0xe0, 0x9e);
    setMsg("你的回合");
    put(row, col, chess == 1 ? 2 : 1);
    enable = ENABLE;
}

COMMAND commands[255] = { {"init",&command_init},{ "put",&command_put }};

DWORD WINAPI wait_udp(LPVOID pm) {
    SO_IN addr;
    char str[512];
    if (udp_listen(&addr)) {
        addr.sin_port = htons(TCP_PORT);
        while (flag != FLAG_DEFAULT) {
            conn = tcp_connect(addr);
            if (flag == FLAG_DEFAULT)return 0;
            if (conn != INVALID_SOCKET) {
                recv(conn, str, 512, 0);
                run(str);
                flag = FLAG_MATCHED;
                break;
            }
        }
    }
    return 0;
}

DWORD WINAPI wait_tcp(LPVOID pm) {
    char* content;
    conn = tcp_listen();
    if (flag == FLAG_DEFAULT)return 0;
    if (conn != INVALID_SOCKET) {
        int dice1 = rand() % 2, dice2 = rand() % 2;
        enable = !dice1;
        chess = !dice2 + 1;
        content = createCommand(3, "init", my_itoa(dice1), my_itoa(dice2));
        send(conn, content, strlen(content) + 1, 0);
        free(content);
        flag = FLAG_MATCHED;
    }
    return 0;
}


DWORD WINAPI wait_conn(LPVOID pm) {
    while (1) {
        char str[512];
        memset(str, 0, 512);
        if (recv(conn, str, 512, 0) <= 0)break;
        run(str);
    }
    if (flag != FLAG_DEFAULT) {
        setMsg("对方已退出，游戏结束");
        flush(KEY_FLUSH);
        flag = FLAG_DEFAULT;
    }
    return 0;
}

int main()
{
    initEnv();
    while (1) {
        reset();
        puts("------------菜单-----------\n");
        puts("1、快速匹配\t2、邀请对局\n3、加入对局\t4、人机对弈\n5、退出");
        puts("---------------------------");
        puts("<操作说明>\n  ↑      W\n←↓→  A S D移动\nEnter或[空格]落子\nEsc逃跑");
        puts("---------------------------\n");
        switch (getch()) {
        case '1':
            fast_match();
            break;
        case '2':
            invite_match();
            break;
        case '3':
            join_match();
            break;
        case '4':
            machine_match();
            break;
        case '5':
            return 0;
        }
    }
}

void startGame() {
        int x, y;
    drawBoard();
    if (flag == FLAG_MACHINE && !enable) {
        calcPut(&x, &y);
        put(y, x, chess == 1 ? 2 : 1);
        enable = ENABLE;
    }
    if (enable) {
        setMsgColor(0x00, 0xe0, 0x9e);
        setMsg("你的回合");
    }
    else {
        setMsgColor(0xf0, 0x00, 0x56);
        setMsg("对方回合");
    }
    flush(KEY_FLUSH);
    if (flag != FLAG_MACHINE)cthread(wait_conn);
    while (flag == FLAG_MATCHED || flag == FLAG_MACHINE) {
        char ch = _getch();
        switch (ch) {
        case KEY_ESC:
            flag = FLAG_DEFAULT;
        default:
            flush(ch);
        }
    }
    if (flag != FLAG_MACHINE)closesocket(conn);
    if (isFinished)_getch();
}

HANDLE cthread(LPTHREAD_START_ROUTINE fun) {
    return CreateThread(NULL, 0, fun, NULL, 0, NULL);
}

SOCKET tcp_listen() {
    int length, size, timeout;
    SO_IN addr, target;
    SOCKET sock;
    char str[255];
    tcp_listener = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_listener == INVALID_SOCKET) {
        if (DEVELOPER_MODE)printf("socket creating error\n");
        return INVALID_SOCKET;
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TCP_PORT);
    addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
    if (bind(tcp_listener, (LPSOCKADDR)&addr, sizeof(addr)) == SOCKET_ERROR) {
        if (DEVELOPER_MODE)printf("socket binding error:%d\n", WSAGetLastError());
        return INVALID_SOCKET;
    }
    if (listen(tcp_listener, 3) == SOCKET_ERROR) {
        if (DEVELOPER_MODE)printf("socket listening error:%d\n", WSAGetLastError());
        return INVALID_SOCKET;
    }
    length = sizeof(LPSOCKADDR);
    size = sizeof(target);
    sock = INVALID_SOCKET;
    while (flag != FLAG_DEFAULT) {
        sock = accept(tcp_listener, (LPSOCKADDR)&target, &size);
        timeout = 1000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        if (sock == INVALID_SOCKET) {
            if (DEVELOPER_MODE)printf("socket creating error\n");
            return NULL;
        }
        if (!isSelf(target) && recv(sock, str, 255, 0) > 0 && !strcmp(str, MATCH_MSG))break;
        closesocket(tcp_listener);
    }
    timeout = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    closesocket(tcp_listener);
    return sock;
}

SOCKET tcp_connect(SO_IN addr) {
    SOCKET soc = socket(AF_INET, SOCK_STREAM, 0);
    if (connect(soc, (LPSOCKADDR)&addr, sizeof(addr)) != SOCKET_ERROR && send(soc, MATCH_MSG, strlen(MATCH_MSG) + 1, 0) != SOCKET_ERROR)return soc;
    closesocket(soc);
    return INVALID_SOCKET;
}

int udp_listen(SO_IN* target) {
    int size, length;
    char content[512];
    SO_IN addr;
    udp_listener = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_listener == INVALID_SOCKET) {
        if (DEVELOPER_MODE)printf("socket creating error\n");
        return 0;
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT);
    addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
    if (bind(udp_listener, (LPSOCKADDR)&addr, sizeof(addr)) == SOCKET_ERROR) {
        if (DEVELOPER_MODE)printf("socket binding error:%d\n", WSAGetLastError());
        return 0;
    }
    size = sizeof(SO_IN);
    while (flag != FLAG_DEFAULT) {
        length = recvfrom(udp_listener, content, 512, 0, (LPSOCKADDR)target, &size);
        if (!isSelf(*target) && length > 0 && !strcmp(content, MATCH_MSG)) break;
    }
    return 1;
}

int udp_send() {
    SOCKET soc = socket(AF_INET, SOCK_DGRAM, 0);
    SO_IN addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT);
    addr.sin_addr.S_un.S_addr = inet_addr(DEVELOPER_MODE ? DEBUG_IPADDR : IPADDR);
    connect(soc, (LPSOCKADDR)&addr, sizeof(addr));
    if (send(soc, MATCH_MSG, strlen(MATCH_MSG) + 1, 0) == SOCKET_ERROR) {
        if (DEVELOPER_MODE)printf("UDP请求发送失败\n");
        return 1;
    }
    closesocket(soc);
    return 0;
}

void fast_match() {
    int count = 0;
    flag = FLAG_MATCH;
    if (DEVELOPER_MODE != MODE_UDP)cthread(wait_tcp);
    if (DEVELOPER_MODE != MODE_TCP)cthread(wait_udp);
    cls();
    puts("等待玩家中...");
    while (flag == FLAG_MATCH) {
        if (DEVELOPER_MODE != MODE_UDP)udp_send();
        count++;
        if (count == TIMEOUT) {
            flag = FLAG_DEFAULT;
            closesocket(tcp_listener);
            closesocket(udp_listener);
            cls();
            puts("<未发现玩家>\n按任意键返回...");
            getch();
            return;
        }
        Sleep(500);
    }
    if (DEVELOPER_MODE != MODE_TCP)closesocket(udp_listener);
    startGame();
}

void invite_match()
{
    int count = 0, i;
    flag = FLAG_INVITE;
    cthread(wait_tcp);
    cls();
    puts("等待玩家中...");
    for (i = 0; self_ip[i] != NULL; i++) {
        printf("IP:%s\n", inet_ntoa(*(IN_ADDR*)self_ip[i]));
    }
    while (flag == FLAG_INVITE) {
        count++;
        if (count == TIMEOUT) {
            flag = FLAG_DEFAULT;
            closesocket(tcp_listener);
            cls();
            puts("<未发现玩家>\n按任意键返回...");
            getch();
            return;
        }
        Sleep(500);
    }
    startGame();
}

void join_match() {
    SO_IN addr;
    char str[16];
    flag = FLAG_JOIN;
    cls();
    puts("请输入对方IP\n<输入exit退出>");
    while (1) {
        scanf("%s", str);
        if (!strcmp(str, "exit"))return;
        if ((long)inet_addr(str) != -1)break;
        cls();
        puts("输入错误，请重新输入对方IP\n<输入exit退出>");
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.S_un.S_addr = inet_addr(str);
    addr.sin_port = htons(TCP_PORT);
    conn = tcp_connect(addr);
    if (conn != INVALID_SOCKET) {
        recv(conn, str, 512, 0);
        run(str);
        flag = FLAG_MATCHED;
        startGame();
    }
    else {
        cls();
        puts("<未发现玩家>\n按任意键返回...");
        getch();
    }
}

void machine_match()
{
    flag = FLAG_MACHINE;
    chess = rand() % 2 + 1;
    enable = rand() % 2;
    startGame();
}

void initEnv() {
    int bottom, ret;
    RECT area;
    WSADATA data;
    srand(time(NULL));
    system("color 3F");
    LANIP[0][0] = inet_addr("10.0.0.0");
    LANIP[0][1] = inet_addr("10.255.255.255");
    LANIP[1][0] = inet_addr("172.16.0.0");
    LANIP[1][1] = inet_addr("172.31.255.255");
    LANIP[2][0] = inet_addr("192.168.0.0");
    LANIP[2][1] = inet_addr("192.168.255.255");
    hwnd = GetConsoleWindow();
    hdc = GetDC(hwnd);
    GetClientRect(hwnd, &area);
    width = area.right;
    height = area.bottom;
    all.right = width;
    all.bottom = height;
    bBrush = CreateSolidBrush(RGB(0, 0, 0));
    wBrush = CreateSolidBrush(RGB(255, 255, 255));
    bkBrush = CreateSolidBrush(RGB(0x3a, 0x96, 0xdd));
    hdccpy = CreateCompatibleDC(hdc);
    SelectObject(hdccpy, CreateCompatibleBitmap(hdc, width, height));
    SetBkMode(hdccpy, TRANSPARENT);
    setMsgColor(0x00, 0xe0, 0x9e);
    offsetHeight = height / 15, unit = (height - offsetHeight * 2) / ROW, offsetWidth = (width - COLUMN * unit) / 2;
    totalWidth = unit * (COLUMN - 1), totalHeight = offsetHeight + unit * (ROW - 1);
    rect.right = area.right;
    bottom = offsetHeight + totalHeight;
    rect.top = bottom + (height - bottom) / 3;
    rect.bottom = height;
    ret = WSAStartup(MAKEWORD(2, 2), &data);
    if (ret != 0) {
        printf("init error:%d", ret);
        return;
    }
    self_ip = (struct hostent**)gethostbyname("")->h_addr_list;
}

void setPen(int r, int g, int b) {
    if (hpen != NULL)DeleteObject(hpen);
    SelectObject(hdc, hpen = CreatePen(PS_INSIDEFRAME, 0, RGB(r, g, b)));
}

void moveTo(int x, int y) {
    MoveToEx(hdc, x, y, NULL);
}

void put(int row, int col, int type) {
    int x = offsetWidth + col * unit, y = offsetHeight + row * unit;
    chessCount++;
    putX = col, putY = row;
    switch (board[row][col] = type) {
    case CHESS_WHITE:
        SelectObject(hdccpy, wBrush);
        break;
    case CHESS_BLACK:
        SelectObject(hdccpy, bBrush);
        break;
    }
    Ellipse(hdccpy, x - unit / 2, y - unit / 2, x + unit / 2, y + unit / 2);
    if (chessCount == ROW * COLUMN) {
        setMsgColor(0x00, 0xe0, 0x9e);
        setMsg("平局");
        if (enable)isFinished = 1;
        flag = FLAG_DEFAULT;
    }
    else if (isWin(row, col)) {
        if (type == chess) {
            setMsgColor(0x00, 0xe0, 0x9e);
            setMsg("恭喜，你胜利了！");
            isFinished = 1;
        }
        else {
            setMsgColor(0xf0, 0x00, 0x56);
            setMsg("你失败了");
            if(flag==FLAG_MACHINE)isFinished = 1;
        }
        flag = FLAG_DEFAULT;
    }
    flush(KEY_FLUSH);
}

void flush(int key) {
    int lastX = pointX, lastY = pointY, x, y, cur_unit = unit / 2;
    char* command;
    switch (key) {
    case KEY_RIGHT:
    case KEY_D:
        if (pointX + 1 < COLUMN)pointX++;
        break;
    case KEY_LEFT:
    case KEY_A:
        if (pointX - 1 >= 0)pointX--;
        break;
    case KEY_UP:
    case KEY_W:
        if (pointY - 1 >= 0)pointY--;
        break;
    case KEY_DOWN:
    case KEY_S:
        if (pointY + 1 < ROW)pointY++;
        break;
    case KEY_ENTER:
    case KEY_SPACE:
        if (enable == DISABLE || board[pointY][pointX])return;
        if (flag == FLAG_MACHINE) {
            int x, y;
            put(pointY, pointX, chess);
            calcPut(&x,&y);
            put(y, x, chess == 1 ? 2 : 1);
            return;
        }
        else {
            setMsgColor(0xf0, 0x00, 0x56);
            setMsg("对方回合");
            enable = DISABLE;
            put(pointY, pointX, chess);
            command = createCommand(3, "put", my_itoa(pointY), my_itoa(pointX));
            send(conn, command, strlen(command) + 1, 0);
            free(command);
        }
        return;
    default:
        break;
    }
    BitBlt(hdc, 0, 0, width, height, hdccpy, 0, 0, SRCCOPY);
    setPen(255, 0, 0);
    if (putX >= 0 && putY >= 0) {
        x = offsetWidth + putX * unit, y = offsetHeight + putY * unit;
        moveTo(x - cur_unit, y - cur_unit);
        LineTo(hdc, x - cur_unit / 2, y - cur_unit);
        moveTo(x - cur_unit, y - cur_unit);
        LineTo(hdc, x - cur_unit, y - cur_unit / 2);
        moveTo(x + cur_unit, y + cur_unit);
        LineTo(hdc, x + cur_unit / 2, y + cur_unit);
        moveTo(x + cur_unit, y + cur_unit);
        LineTo(hdc, x + cur_unit, y + cur_unit / 2);
        moveTo(x + cur_unit, y - cur_unit);
        LineTo(hdc, x + cur_unit / 2, y - cur_unit);
        moveTo(x + cur_unit, y - cur_unit);
        LineTo(hdc, x + cur_unit, y - cur_unit / 2);
        moveTo(x - cur_unit, y + cur_unit);
        LineTo(hdc, x - cur_unit / 2, y + cur_unit);
        moveTo(x - cur_unit, y + cur_unit);
        LineTo(hdc, x - cur_unit, y + cur_unit / 2);
    }
    x = offsetWidth + pointX * unit, y = offsetHeight + pointY * unit;
    Arc(hdc, x - cur_unit, y - cur_unit, x + cur_unit, y + cur_unit, x, y - cur_unit, x, y - cur_unit);
    moveTo(x - cur_unit, y);
    LineTo(hdc, x + cur_unit, y);
    moveTo(x, y - cur_unit);
    LineTo(hdc, x, y + cur_unit);
    setPen(0, 0, 0);
}

void setMsg(char* msg)
{
    LPCWSTR wchars;
    FillRect(hdccpy, &rect, bkBrush);
    wchars = chtowc(msg);
    DrawText(hdccpy, wchars, lstrlen(wchars), &rect, DT_CENTER);
}

void setMsgColor(int r, int g, int b)
{
    SetTextColor(hdccpy, RGB(r, g, b));
}

void run(char* command)
{
    char* ps[64], * part = strtok(command, " ");
    int p = 0, i;
    while (part != NULL) {
        ps[p++] = part;
        part = strtok(NULL, " ");
    }
    for (i = 0; i < 255; i++)
    {
		if (!strcmp(commands[i].name, ps[0])) {
			(*commands[i].function)(ps);
        }
    }
}

char* my_itoa(int number)
{
    char* buffer = (char*)calloc(64, sizeof(char));
    return itoa(number, buffer, 10);
}

char* createCommand(int sum, ...)
{
    int i;
    char* command = NULL, * arg;
    va_list list;
    va_start(list, sum);
    for (i = 0; i < sum; i++) {
        arg = va_arg(list, char*);
        if (i > 0) {
            int len = strlen(command);
            command = (char*)realloc(command, len + strlen(arg) + 2);
            command[len] = ' ';
            strcpy(command + len + 1, arg);
        }
        else {
            command = (char*)calloc(strlen(arg) + 1, sizeof(char));
            strcpy(command, arg);
        }
    }
    va_end(list);
    return command;
}

int isWin(int row, int col)
{
    int i, j, count = 0, curRow, curCol;
    curCol = col;
    for (i = 0; i < 4; i++)
    {
        for (j = -4; j <= 4; j++)
        {
            switch (i)
            {
            case 0:
                curRow = row;
                curCol = col + j;
                break;
            case 1:
                curRow = row + j;
                curCol = col;
                break;
            case 2:
                curRow = row + j;
                curCol = col + j;
                break;
            case 3:
                curRow = row - j;
                curCol = col + j;
                break;
            }
            if (isValid(curRow, curCol) && board[row][col] == board[curRow][curCol]) {
                count++;
                if (count == 5)return 1;
            }
            else {
                count = 0;
            }
        }
        count = 0;
    }
    return 0;
}

int isValid(int row, int col)
{
    if (row >= 0 && row < ROW && col >= 0 && col < COLUMN)return 1;
    return 0;
}

int isSelf(SO_IN addr)
{
    int i;
    for (i = 0; self_ip[i] != NULL; i++)
    {
        if ((*(IN_ADDR*)self_ip[i]).S_un.S_addr == addr.sin_addr.S_un.S_addr)return 1;
    }
    return 0;
}

int isLAN(unsigned long addr) {
    int i;
    for (i = 0; i < 3; i++)
    {
        if (addr >= LANIP[i][0] && addr <= LANIP[i][1])return 1;
    }
    return 0;
}

IN_ADDR getLANIP()
{
    IN_ADDR addr;
    int i;
    for (i = 0; self_ip[i] != NULL; i++)
    {
        addr = *(IN_ADDR*)self_ip[i];
        if (isLAN(addr.S_un.S_addr))break;
    }
    return addr;
}

void cls()
{
    InvalidateRect(hwnd, &all, 1);
    system("cls");
}

void reset()
{
    enable = chess = chessCount = isFinished = pointX = pointY = 0;
    putX = putY = -1;
    memset(board, 0, ROW * COLUMN * sizeof(int));
    cls();
}

wchar_t* chtowc(char* ch)
{
    int len = strlen(ch) + 1;
    WCHAR* wchars = (WCHAR*)calloc(len, sizeof(WCHAR));
    MultiByteToWideChar(CP_ACP, 0, ch, len, wchars, len);
    return wchars;
}

void calcPut(int *x, int *y)
{
    int i, j, k, white, black,weights[ROW][COLUMN],group_weight,max=0,count=0;
    memset(weights, 0, sizeof(int) * ROW * COLUMN);
    for (i = 2; i < ROW-2; i++)
    {
        for (j = 0; j < COLUMN; j++) {
            white = black = 0;
            for (k = -2; k < 3; k++)
            {
                switch (board[i + k][j]) {
                case CHESS_WHITE:
                    white++;
                    break;
                case CHESS_BLACK:
                    black++;
                    break;
                }
            }
            group_weight = (chess == CHESS_WHITE ?  weight[black][white] :weight[white][black]);
            for (k = -2; k < 3; k++)
            {
                weights[i + k][j] += group_weight;
            }

        }
    }
    for (i = 0; i < ROW; i++)
    {
        for (j = 2; j < COLUMN - 2; j++) {
            white = black = 0;
            for (k = -2; k < 3; k++)
            {
                switch (board[i][j + k]) {
                case CHESS_WHITE:
                    white++;
                    break;
                case CHESS_BLACK:
                    black++;
                    break;
                }
            }
            group_weight = (chess == CHESS_WHITE ? weight[black][white] : weight[white][black]);
            for (k = -2; k < 3; k++)
            {
                weights[i][j + k] += group_weight;
            }

        }
    }
    for (i = 2; i < ROW - 2; i++)
    {
        for (j = 2; j < COLUMN - 2; j++) {
            white = black = 0;
            for (k = -2; k < 3; k++)
            {
                switch (board[i + k][j + k]) {
                case CHESS_WHITE:
                    white++;
                    break;
                case CHESS_BLACK:
                    black++;
                    break;
                }
            }
            group_weight = (chess == CHESS_WHITE ? weight[black][white] : weight[white][black]);
            for (k = -2; k < 3; k++)
            {
                weights[i + k][j + k] += group_weight;
            }

        }
    }
    for (i = 2; i < ROW - 2; i++)
    {
        for (j = 2; j < COLUMN - 2; j++) {
            white = black = 0;
            for (k = -2; k < 3; k++)
            {
                switch (board[i + k][j - k]) {
                case CHESS_WHITE:
                    white++;
                    break;
                case CHESS_BLACK:
                    black++;
                    break;
                }
            }
            group_weight = (chess == CHESS_WHITE ? weight[black][white] : weight[white][black]);
            for (k = -2; k < 3; k++)
            {
                weights[i + k][j - k] += group_weight;
            }
        }
    }
    for (i = 0; i < ROW; i++)
    {
        for (j = 0; j < COLUMN; j++)
        {
            if (board[i][j] > 0)weights[i][j] = 0;
        }
    }
    for (i = 0; i < ROW; i++)
    {
        for (j = 0; j < COLUMN; j++)
        {
            if (weights[i][j] > max) {
                max = weights[i][j];
                count = 0;
            }
            if(weights[i][j]==max)count++;
        }
    }
    count = rand() % count + 1;
    for (i = 0; i < ROW; i++)
    {
        for (j = 0; j < COLUMN; j++)
        {
            if (max == weights[i][j] && --count == 0) {
                *x = j;
                *y = i;
                return;
            }
        }
    }
}

void drawBoard() {
    int i;
    cls();
    SetBkMode(hdc, TRANSPARENT);
    for (i = 0; i < ROW; i++)
    {
        int cur = offsetHeight + i * unit;
        moveTo(offsetWidth, cur);
        LineTo(hdc, offsetWidth + (COLUMN - 1) * unit, cur);
    }
    for (i = 0; i < COLUMN; i++)
    {
        int cur = offsetWidth + i * unit;
        moveTo(cur, offsetHeight);
        LineTo(hdc, cur, offsetHeight + (ROW - 1) * unit);
    }
    BitBlt(hdccpy, 0, 0, width, height, hdc, 0, 0, SRCCOPY);
}