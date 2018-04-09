#include "service.h"
#include "bbs.h"
#include <sys/times.h>

#define WIDTH  40
#define HEIGHT 20

#define SNAKE_REC_DIR  "etc/snake"
struct snake {
    int x;
    int y;
    struct snake *prev;
    struct snake *next;
};

struct snake *snake_head;
struct snake *snake_tail;

int delay = 100;
int food_x, food_y, food_type;
int tail_x = 1, tail_y = 1;
int data[WIDTH][HEIGHT];
int score = 0, level = 1, left_steps = 0;
char topID[20][20];
int topS[20];
time_t toptime[20];
char version[8] = "1.2";

extern struct user_info uinfo;
extern int getch0();

void snake_saverec();
void snake_sort();
void snake_food();
int snake_checkahead(int, int*, int*);

int getch()
{
    int c, d, e;
    int ret;

    static int *retbuf;
    static int retlen = 0;

    static time_t old = 0;
    time_t now;

    extern int keymem_total;
    extern struct key_struct *keymem;

    if (retlen > 0) {
        ret = *retbuf;
        retbuf++;
        retlen--;

        return ret;
    }

    c = getch0();
//    if(c==3||c==4||c==-1) return -1;
    if (c == Ctrl('C'))
        return c;

    if (c != KEY_ESC)
        ret = c;
    else {
        d = getch0();
        e = getch0();
        if (d == 0)
            ret = c;
        else if (e == 'A')
            ret = KEY_UP;
        else if (e == 'B')
            ret = KEY_DOWN;
        else if (e == 'C')
            ret = KEY_RIGHT;
        else if (e == 'D')
            ret = KEY_LEFT;
    }

    if (keymem_total) {
        int i, j, p;
        for (i = 0; i < keymem_total; i++) {
            p = !keymem[i].status[0];
            if (keymem[i].status[0] == -1)
                continue;
            j = 0;
            while (keymem[i].status[j] && j < 10) {
                if (keymem[i].status[j] == TETRIS)
                    p = 1;
                j++;
            }
            if (p && ret == keymem[i].key) {
                j = 0;
                while (keymem[i].mapped[j] && j < 10)
                    j++;
                if (j == 0)
                    continue;
                ret = keymem[i].mapped[0];
                retlen = j - 1;
                retbuf = keymem[i].mapped + 1;
                break;
            }
        }
    }

    if (ret) {
        now = time(0);
        if (now - old > 60) {
            uinfo.freshtime = now;
            UPDATE_UTMP(freshtime, uinfo);
            old = now;
        }
    }
    return ret;
}

extern select_func x_select;
extern read_func x_read;

int getch0()
{
    char ch;
    fd_set rfds;
    struct timeval tv;
    FD_ZERO(&rfds);
    FD_SET(0, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = 50000;
    if ((*x_select) (1, &rfds, NULL, NULL, &tv)) {
        if ((*x_read) (0, &ch, 1) <= 0)
            exit(-1);
        return ch;
    } else
        return 0;
}

int snake_initdata()
{
    int x, y;
    for (x = 0; x < WIDTH; x++) {
        for (y = 0; y < HEIGHT; y++) {
            if (x == 0 || x == WIDTH - 1 || y == 0 || y == HEIGHT - 1)
                data[x][y] = 1;
            else
                data[x][y] = 0;
        }
    }
    return 0;
}

int snake_create()
{
    struct snake *s1, *s2;
    int i = 4;

    s1 = s2 = (struct snake *) malloc(sizeof(struct snake));
    while (1) {
        if (snake_head == NULL) {
            snake_head = s1;
            snake_head->prev = NULL;
        } else {
            s2->next = s1;
            s1->prev = s2;
            s1->next = NULL;
        }
        s1->x = i;
        s1->y = HEIGHT / 2;
        data[s1->x][s1->y] = 2;
        i--;
        if (i <= 1) {
            snake_tail = s1;
            break;
        }
        s2 = s1;
        s1 = (struct snake *) malloc(sizeof(struct snake));
    }
    return 1;
}

int snake_count()
{
    int c;
    FILE *fp;

    if (!dashd(SNAKE_REC_DIR))
        system("mkdir "SNAKE_REC_DIR);
    fp = fopen(SNAKE_REC_DIR "/visitcount", "r+");
    if (fp == NULL) {
        system("echo 1 > " SNAKE_REC_DIR "/visitcount");
        return 1;
    }
    fscanf(fp, "%d", &c);
    rewind(fp);
    fprintf(fp, "%d", c + 1);
    fclose(fp);
    return c + 1;
}

void snake_loadrec()
{
    FILE *fp;
    int n;
    char buf[128];

    sprintf(buf, SNAKE_REC_DIR "/snake.%d", level);
    for (n = 0; n <= 19; n++) {
        strcpy(topID[n], "unknown.");
        topS[n] = 0;
        toptime[n] = 0;
    }
    fp = fopen(buf, "r");
    if (fp == NULL) {
        snake_saverec();
        return;
    }
    for (n = 0; n <= 19; n++)
        fscanf(fp, "%s %d %d\n", topID[n], &topS[n], (int *) &toptime[n]);
    fclose(fp);
}

void snake_showrec()
{
    int n;
    char tmp[200];

    snake_loadrec();
    clear();
    prints
        ("\033[44;37m                     -    SNAKE 排行榜(等级:%d)   -                                 \r\n\033[m",
         level);
    prints
        ("\033[41m No.        ID                 Score                   时间                        \033[m\n\r");
    for (n = 0; n <= 19; n++) {
        sprintf(tmp,
                "\033[1;37m%3d\033[32m%13s\033[0;37m\033[m%20d           \033[1;37m%s\033[m\n\r",
                n + 1, topID[n], topS[n], Ctime(toptime[n]));
        prints(tmp);
    }
    prints
        ("\033[41m                                                                               \033[m\n\r");
    pressanykey();
}


void snake_saverec()
{
    FILE *fp;
    int n;
    char buf[128];

    sprintf(buf, SNAKE_REC_DIR "/snake.%d", level);
    fp = fopen(buf, "w");
    for (n = 0; n <= 19; n++) {
        fprintf(fp, "%12s %10d %d\n", topID[n], topS[n], (int) toptime[n]);
    }
    fclose(fp);
}

void snake_checkrec(int ds, time_t time)
{
    char id[30];
    int n;
    snake_loadrec();
    strcpy(id, getCurrentUser()->userid);
    for (n = 0; n <= 19; n++)
        if (!strcmp(topID[n], id)) {
            if (ds > topS[n]) {
                topS[n] = ds;
                toptime[n] = time;
                snake_sort();
                snake_saverec();
                snake_showrec();
            }
            return;
        }
    if (ds > topS[19]) {
        strcpy(topID[19], id);
        topS[19] = ds;
        toptime[19] = time;
        snake_sort();
        snake_saverec();
        snake_showrec();
        return;
    }
}

void snake_sort()
{
    int n, n2, tmp, rank = 19;
    time_t time;
    char tmpID[30];
    clear();
    prints("祝贺! 您刷新了自己的纪录!\r\n");
    pressanykey();
    for (n = 0; n <= 18; n++)
        for (n2 = n + 1; n2 <= 19; n2++)
            if (topS[n] < topS[n2]) {
                tmp = topS[n];
                topS[n] = topS[n2];
                topS[n2] = tmp;
                strcpy(tmpID, topID[n]);
                strcpy(topID[n], topID[n2]);
                strcpy(topID[n2], tmpID);
                time = toptime[n];
                toptime[n] = toptime[n2];
                toptime[n2] = time;
                if (rank == 19)
                    rank = n;
            }
    rank = 20 - rank;
}

void snake_showframe()
{
    int x, y;
    for (x = 0; x < WIDTH; x++) {
        for (y = 0; y < HEIGHT; y++) {
            if (data[x][y] == 1) {
                move(y, 2 * x);
                prints("█");
            }
        }
    }
}

void snake_show(int dir)
{
    struct snake *s;
    int next;

    s = snake_head->next;
    move(food_y, 2 * food_x);
    if (food_type == 1)
        prints("\033[1;31m◎\033[m");
    else if (food_type == 2)
        prints("\033[1;32m▲\033[m");
    else if (food_type == 3)
        prints("\033[1;33m★\033[m");
    else if (food_type == 4)
        prints("\033[1;34m◇\033[m");
    else if (food_type == 5)
        prints("\033[1;35m◤\033[m");
    else if (food_type == 6)
        prints("\033[1;36m○\033[m");
    else
        prints("\033[m·");
    move(tail_y, 2 * (tail_x));
    prints("  ");
    if (s != snake_tail) {
        struct snake *h, *t;
        h = s->prev;
        t = s->next;
        move(s->y, 2 * (s->x));
        if ((h->x>s->x && t->y>s->y) || (t->x>s->x && h->y>s->y))
            prints("╭");
        else if ((h->x<s->x && t->y>s->y) || (t->x<s->x && h->y>s->y))
            prints("╮");
        else if ((h->x>s->x && t->y<s->y) || (t->x>s->x && h->y<s->y))
            prints("╰");
        else if ((h->x<s->x && t->y<s->y) || (t->x<s->x && h->y<s->y))
            prints("╯");
        else if (h->x>s->x || h->x<s->x)
            prints("─");
        else
            prints("│");
    }
    move(snake_tail->y, 2 * (snake_tail->x));
    prints("☉");
    move(snake_head->y, 2 * (snake_head->x));
    next = snake_checkahead(dir, NULL, NULL);
    if (next == 1) {
        if (dir==KEY_UP)
            prints("∨");
        else if (dir==KEY_DOWN)
            prints("∧");
        else if (dir==KEY_LEFT)
            prints("＞");
        else
            prints("＜");
    } else
        prints("⊕");
    move(snake_head->y, 2 * (snake_head->x));
    move(21, 10);clrtoeol();
    prints("\033[1;33m当前等级:   \033[32m%d       \033[33m得分: \033[32m%d\033[m", level, score);
    if (food_type)
        prints("\t\t\033[1;33m剩余步数: \033[31m%2d\033[m", left_steps);
    refresh();
    return;
}

int snake_addhead()
{
    struct snake *s;
    s = (struct snake *) malloc(sizeof(struct snake));
    s->x = food_x;
    s->y = food_y;
    data[food_x][food_y] = 2;
    snake_head->prev = s;
    s->next = snake_head;
    s->prev = NULL;
    snake_head = s;

    if (food_type != 0)
        score += (food_type * left_steps / 10) * 20;
    else
        score += 10;
    if (score < 10)
        score = 10;
    snake_food();
    return 1;
}

int snake_exch()
{
    struct snake *s;

    s = snake_tail;
    tail_x = s->x;
    tail_y = s->y;

    snake_tail = s->prev;
    snake_tail->next = NULL;
    s->prev = NULL;
    s->next = snake_head;
    snake_head->prev = s;
    snake_head = s;

    return 1;
}
/* 获得snake_head下一个位置及状态
 -1: 撞上了
  0: 空
  1: food */
int snake_checkahead(int dir, int *nx, int *ny)
{
    int x, y;
    switch (dir) {
        case KEY_UP:
            x = snake_head->x;
            y = snake_head->y - 1;
            break;
        case KEY_DOWN:
            x = snake_head->x;
            y = snake_head->y + 1;
            break;
        case KEY_LEFT:
            x = snake_head->x - 1;
            y = snake_head->y;
            break;
        case KEY_RIGHT:
            x = snake_head->x + 1;
            y = snake_head->y;
            break;
        default:
            return -1;
    }

    if (nx)
        *nx = x;
    if (ny)
        *ny = y;

    if (data[x][y] > 0)
        return -1;
    if (x == food_x && y == food_y)
        return 1;
    return 0;
}

int snake_forward(int dir)
{
    int nx, ny, ret;

    ret =  snake_checkahead(dir, &nx, &ny);
    if (ret == 0) {
        snake_exch();
        snake_head->x = nx;
        snake_head->y = ny;
        data[tail_x][tail_y] = 0;
        data[nx][ny] = 2;
    } else if (ret == 1)
        snake_addhead();

    return ret;
}

void snake_clear()
{
    struct snake *s, *t;

    s = snake_head;
    while (s != NULL) {
        t = s->next;
        s->next = NULL;
        s->prev = NULL;
        free(s);
        s = t;
    }
    return;
}

void snake_intro()
{
    char buf[3];
    int i;

    move(12, 20);
    prints("欢迎光临 贪食蛇 V%s jiangjun@newsmth", version);
    move(13, 20);
    prints("游戏过程中按空格暂停, Ctrl+C退出");
    move(14, 20);
    prints("您是第 %d 位访问者。", snake_count());

    while (1) {
        getdata(19, 20, "请选择等级:[1-9] ", buf, 2, 1, NULL, 1);
        if (buf[0] > '0' && buf[0] <= '9')
            level = atoi(buf);
        else
            level = 1;
        if (level <= 9)
            break;
    }
    for (i = 0; i < level; i++)
        delay = delay * 0.78;
    snake_showrec();
}

int snake()
{
    int c, t, dir, newdir = 0, ret, accel;
    struct tms faint;
    time_t end_time;

    snake_head = NULL;

    snake_initdata();
    snake_create();
    snake_showframe();
    snake_food();
    dir = KEY_RIGHT;
    newdir = dir;
    t = times(&faint);
    while (1) {
        snake_show(dir);
        c = getch();
        accel = 0;
        if (c == Ctrl('D') || c == Ctrl('C')) {
            end_time = time(0);
            snake_checkrec(score, end_time);
            return 1;
        }
        if ((c == KEY_UP && dir == KEY_DOWN) ||
            (c == KEY_LEFT && dir == KEY_RIGHT) ||
            (c == KEY_DOWN && dir == KEY_UP) ||
            (c == KEY_RIGHT && dir == KEY_LEFT))
            c = 0;

        if (c == KEY_UP || c == KEY_DOWN || c == KEY_LEFT || c == KEY_RIGHT) {
            newdir = c;
            accel = 1;
        }
        if (c == ' ') {
            move(22, 35);
            prints("暂停中");
            pressanykey();
            move(22, 35);
            clrtoeol();
            c = 0;
            continue;
        }
        if ( times(&faint) - t > delay || accel) {
            t = times(&faint);
            dir = newdir;
            ret = snake_forward(dir);
            if (ret == -1) { /* 撞墙 */
                move(10, 30);
                prints("   撞死啦!!!   ");
                pressreturn();
                end_time = time(0);
                snake_checkrec(score, end_time);
                break;
            } else if (ret == 1) /* 吃到food */
                continue;
            if (food_type != 0) {
                left_steps--;
                if (left_steps <= 0) {
                    move(food_y, 2 * food_x);
                    prints("  ");
                    snake_food();
                }
            }
        }
    }
    snake_clear();
    return 1;
}

int snake_main()
{
    delay = 100;
    clear();
    snake_intro();
    clear();
    snake();
    return 1;
}

void snake_food()
{
    int validfood, n;
    srand(time(0));
    n = rand() % 100;
    switch (n) {
    case 1:
    case 2:
        food_type = 6;
        break;
    case 3:
    case 4:
    case 5:
        food_type = 5;
        break;
    case 6:
    case 7:
    case 8:
    case 9:
        food_type = 4;
        break;
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
        food_type = 3;
        break;
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
        food_type = 2;
        break;
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
    case 26:
    case 27:
        food_type = 1;
        break;
    default:
        food_type = 0;
        break;
    }
    if (food_type == 0) {
        move(21, 50);
        clrtoeol();
    }
    while (1) {
        validfood = 1;
        food_x = rand() % (WIDTH - 2) + 1;
        food_y = rand() % (HEIGHT - 2) + 1;
        if (data[food_x][food_y] != 0)
            validfood = 0;
        if (validfood == 1) {
            left_steps = 65 - 5 * food_type;
            return;
        }
    }
}
