#include "bbs.h"
#include "service.h"

#define MAX_ROW  9
#define MAX_COL  9
#define MAX_BALL 81
#define IBC      5  // initial ball count
#define CPG      3  // ball count per group
#define BALL_REC_FILE "etc/ball.rec"

struct point {
    int row;
    int col;
};

struct ball {
    struct point p;
    int key;  // start及end中记录状态，ballpath中记录前驱
};

int balls[MAX_ROW][MAX_COL];
int ballvisited[MAX_ROW][MAX_COL];
int head, tail;

struct ball start, end;
struct ball ballpath[MAX_BALL];
int ngtype[CPG];
int score=0, hole;
int topS[20];
char topID[20][IDLEN+2];
time_t toptime[20];

int erase_ball(int, int);

void ball_help()
{
    move(3, 40);
    prints("操作说明: ");
    move(5, 42);
    prints("方向键移动光标，空格选中");
    move(7, 42);
    prints("Home/A、End/D  移动光标至行首、行尾");
    move(9, 42);
    prints("PgUp/W、PgDn/S 移动光标至列首、列尾");
    move(11, 42);
    prints("Ctrl+C 退出游戏");
}

void show_frame()
{
    int i;
    move(0, 0);
    clrtobot();
    prints("┏━┯━┯━┯━┯━┯━┯━┯━┯━┓\n");
    for (i=1;i<MAX_COL;i++){
        prints("┃  │  │  │  │  │  │  │  │  ┃\n");
        prints("┠─┼─┼─┼─┼─┼─┼─┼─┼─┨\n");
    }
    prints("┃  │  │  │  │  │  │  │  │  ┃\n");
    prints("┗━┷━┷━┷━┷━┷━┷━┷━┷━┛\n");
    move(20, 0);
    prints("下轮颜色:            得分:");
    ball_help();
}

void ball_saverec()
{
    FILE *fp;
    int n;
    char buf[128];

    sprintf(buf, BALL_REC_FILE);
    fp = fopen(buf, "w");
    for (n = 0; n <= 19; n++) {
        fprintf(fp, "%12s %10d %d\n", topID[n], topS[n], (int) toptime[n]);
    }
    fclose(fp);
}

void ball_loadrec()
{
    FILE *fp; 
    int n;
    char buf[128];
            
    sprintf(buf, BALL_REC_FILE);
    for (n = 0; n <= 19; n++) {
        strcpy(topID[n], "unknown.");
        topS[n] = 0; 
        toptime[n] = 0;
    }   
    fp = fopen(buf, "r");
    if (fp == NULL) {
        ball_saverec();
        return;
    }   
    for (n = 0; n <= 19; n++)
        fscanf(fp, "%s %d %d\n", topID[n], &topS[n], (int *) &toptime[n]);
    fclose(fp);
}

void ball_showrec()
{   
    int n;
    char buf[200];

    ball_loadrec();
    clear();
    prints
        ("\033[44;37m                           -    彩球连珠排行榜   -                              \r\n\033[m");
    prints
        ("\033[41m No.        ID                 Score                   时间                        \033[m\n\r");
    for (n = 0; n <= 19; n++) {
        sprintf(buf,
                "\033[1;37m%3d\033[32m%13s\033[0;37m\033[m%20d           \033[1;37m%s\033[m\n\r",
                n + 1, topID[n], topS[n], Ctime(toptime[n]));
        prints(buf);
    }
    prints("\033[41m                                                                               \033[m\n\r");
    pressanykey();
}

void ball_sort()
{
    int n, n2, tmp, rank = 19;
    time_t time;
    char tmpID[IDLEN+2];
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

void ball_checkrec(int score, time_t time)
{
    char id[IDLEN+2];
    int n;
    ball_loadrec();
    strcpy(id, getCurrentUser()->userid);
    for (n = 0; n <= 19; n++)
        if (!strcmp(topID[n], id)) {
            if (score > topS[n]) {
                topS[n] = score;
                toptime[n] = time;
                ball_sort();
                ball_saverec();
                ball_showrec();
            }
            return;
        }
    if (score > topS[19]) {
        strcpy(topID[19], id);
        topS[19] = score;
        toptime[19] = time;
        ball_sort();
        ball_saverec();
        ball_showrec();
        return;
    }
}

void show_ng_score()
{
    int i;
    char buf[STRLEN];
    move(20, 10);
    for (i=0;i<CPG;i++) {
        sprintf(buf, "\033[1;3%dm●\033[m", ngtype[i]);
        prints(buf);
    }
    move(20, 26);
    prints("%d", score);
}

void show_all_balls()
{
    int i, j, btype;
    char buf[STRLEN];
    for (i=0;i<MAX_ROW;i++){
        for (j=0;j<MAX_COL;j++) {
            move(i*2+1, j*4+2);
            btype=balls[i][j];
            if (btype>0) {
                sprintf(buf, "\033[1;3%dm●\033[m", btype);
            } else
                sprintf(buf, "  ");
            prints(buf);
        }
    }
    /*
    move(20, 10);
    for (i=0;i<CPG;i++) {
        sprintf(buf, "\033[1;3%dm●\033[m", ngtype[i]);
        prints(buf);
    }
    move(20, 26);
    prints("%d", score);
    */
    show_ng_score();

    return;
}
/* 访问一个点，被占或已访问返回-1，为目的地时返回1 */
int visit_point(struct point p, int dir)
{
    struct point dp[4] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
    p.row += dp[dir].row;
    p.col += dp[dir].col;
    if (p.row<0 || p.row>=MAX_ROW || p.col<0 || p.col>=MAX_COL)
        return -1;
    if (balls[p.row][p.col] || ballvisited[p.row][p.col])
        return -1;
    ballvisited[p.row][p.col] = 1;
    ballpath[tail].p = p;
    ballpath[tail].key = head-1;
    tail++;
    if (p.row==end.p.row && p.col==end.p.col)
        return 1;
    return 0;
}

int get_path()
{
    int i, found=0;
    struct point p;

    head = 0;
    tail = 1;
    bzero(ballvisited, MAX_BALL * sizeof(int));
    bzero(ballpath, sizeof(ballpath));
    ballpath[0] = start;
    ballpath[0].key = -1;
    while (head<tail) {
        p = ballpath[head].p;
        head++;
        for (i=0;i<4;i++) {
            if (visit_point(p, i)==1) {
                found = 1;
                break;
            }
        }
        if (found)
            break;
    }
    return found;
}

void show_path_point(struct point p[], int btype, int count)
{
    int i;
    char buf[STRLEN];
    for (i=count;i>0;i--) {
        move(p[i-1].row*2+1, p[i-1].col*4+2);
        sprintf(buf, "\033[1;3%d;47m●\033[m", btype);
        prints(buf);
    }
}

void show_path()
{
    int count=0, btype=balls[start.p.row][start.p.col];
    struct point ballpath2[MAX_BALL];
    struct ball b = ballpath[tail-1];

    do {
        ballpath2[count++] = b.p;
        b = ballpath[b.key];
    } while (b.key!=-1);

    show_path_point(ballpath2, btype, count);
    refresh();
}

int create_ball()
{
    return rand()%7+1;
}

/* 安放并显示一个ball，检查安放位置可否消除，caller保证有hole */
void deposit_ball(int btype)
{
    char buf[STRLEN];
    int row, col;
    int erase;
    while (1) {
        row = rand()%MAX_ROW;
        col = rand()%MAX_COL;
        if (balls[row][col]==0) {
            balls[row][col] = btype;
            /* 显示该ball */
            move(row*2+1, col*4+2);
            sprintf(buf, "\033[1;3%dm●\033[m", btype);
            prints(buf);

            erase = erase_ball(row, col);
            if (erase) {
                hole += erase;
                score += (erase - 4) * 10;
            }
            break;
        }
    }
}

/* 产生5个ball */
void init_ball()
{
    int i, btype;
    for (i=0;i<IBC;i++) {
        btype = create_ball();
        deposit_ball(btype);
    }
    hole = MAX_BALL - IBC;
}

/* 产生一组（3个）ball */
void create_group()
{
    int i;
    if (hole<=0)
        return;
    for (i=0;i<CPG;i++)
        ngtype[i] = create_ball();
    return;
}

/* 安放一组（3个）ball并产生下一组，没有空位就不安放 */
int deposit_group()
{
    int i;
    for (i=0;i<CPG;i++) {
        deposit_ball(ngtype[i]);
        hole--;
        if (hole<=0)
            return -1;
    }
    create_group();
    show_ng_score();
    return 0;
}

int move_ball()
{
    if (get_path()) {
        show_path();
        balls[end.p.row][end.p.col] = balls[start.p.row][start.p.col];
        balls[start.p.row][start.p.col] = 0;
        return 1;
    }
    return 0;
}

/* 消除连接成功的ball，并去掉被消除ball的显示 */
int erase_ball(int crow, int ccol)
{
    struct point dp[4] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
    int dir, v, count, totalcount=1, row, col;

    v = balls[crow][ccol];
    for (dir=0;dir<4;dir++) {
        count = 1;
        /* 沿小的方向检查 */
        row = crow;
        col = ccol;
        while (1) {
            row = row - dp[dir].row;
            col = col - dp[dir].col;
            if (row<0 || row>=MAX_ROW || col<0 || col>=MAX_COL || balls[row][col]!=v)
                break;
            count++;
        }
        /* 沿大的方向检查 */
        row = crow;
        col = ccol;
        while (1) {
            row = row + dp[dir].row;
            col = col + dp[dir].col;
            if (row<0 || row>=MAX_ROW || col<0 || col>=MAX_COL || balls[row][col]!=v)
                break;
            count++;
        }
        if (count>4) {
            totalcount = totalcount + count - 1;
            while (count>0) {
                row = row - dp[dir].row;
                col = col - dp[dir].col;
                balls[row][col] = 0;
                move(row*2+1, col*4+2);
                prints("  ");
                count--;
            }
        }
    }
    return totalcount>1 ? totalcount : 0;
}

int ball_main()
{
    int i, row, col, moveball, erase;
    char buf[STRLEN];
    srand(time(0));

    row=col=0;
    ball_showrec();
    show_frame();
    init_ball();
    create_group();
    show_all_balls();
    while(1) {
        moveball = 0;
        bzero(&start, sizeof(struct ball));
        bzero(&end, sizeof(struct ball));
        while(!moveball) {
            if (start.key) {    //显示选中ball
                move(start.p.row*2+1, start.p.col*4+2);
                sprintf(buf, "\033[1;3%d;47m●\033[m", balls[start.p.row][start.p.col]);
                prints(buf);
            }
            row = (row+MAX_ROW)%MAX_ROW;
            col = (col+MAX_COL)%MAX_COL;
            move(row*2+1, col*4+2);
            i = igetkey();
            switch(toupper(i)) {
                case KEY_UP:
                    row--;
                    break;
                case KEY_DOWN:
                    row++;
                    break;
                case KEY_LEFT:
                    col--;
                    break;
                case KEY_RIGHT:
                    col++;
                    break;
                case KEY_HOME:
                case 'A':
                    col=0;
                    break;
                case KEY_END:
                case 'D':
                    col=MAX_COL-1;
                    break;
                case KEY_PGUP:
                case 'W':
                    row=0;
                    break;
                case KEY_PGDN:
                case 'S':
                    row=MAX_ROW-1;
                    break;
                case ' ':
                    if (balls[row][col]) { //选中的是有ball的点
                        if (start.key) { //去掉原来选中ball的显示
                            move(start.p.row*2+1, start.p.col*4+2);
                            sprintf(buf, "\033[1;3%dm●\033[m", balls[start.p.row][start.p.col]);
                            prints(buf);
                        }
                        start.p.row = row;
                        start.p.col = col;
                        start.key = 1;
                    } else { //选中的是没有ball的点
                        if (!start.key) //还没选起点
                            continue;
                        else {
                            end.p.row = row;
                            end.p.col = col;
                            end.key = 1;
                            /* 开始移动 */
                            if (move_ball()==0) {
                                move(22, 10);
                                prints("无法到达目的地");
                                pressreturn();
                                move(start.p.row*2+1, start.p.col*4+2);
                                sprintf(buf, "\033[1;3%dm●\033[m", balls[start.p.row][start.p.col]);
                                prints(buf);
                                move(22, 10);clrtoeol();
                            } else {
                                /* 移动完毕 */
                                show_all_balls();
                                usleep(200);
                                refresh();
                                /* 消除连接的成功balls */
                                erase = erase_ball(end.p.row, end.p.col);
                                if (erase) {
                                    hole += erase;
                                    score += (erase - 4) * 10;
                                    show_ng_score();
                                } else
                                    moveball = 1;
                            }
                            start.key = end.key = 0;
                        }
                    }
                    break;
                case Ctrl('C'):
                    moveball = 1;
                    break;
                default:
                    break;
            }
        }
        if (i==Ctrl('C')) {
            ball_checkrec(score, time(0));
            return 0;
        }
        if (!erase) {  // 上次移动未产生消除ball时，放置旧的，产生新的
            if (deposit_group()==-1) {
                show_all_balls();
                move(22, 10);
                prints("游戏结束");
                ball_checkrec(score, time(0));
                pressreturn();
                return 0;
            }
            //create_group(); 如果deposit_group刚好放满，就GG了，所以挪到deposit_group中
        }
    }
    return 0;
}
