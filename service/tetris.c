#include "service.h"
#include "bbs.h"
#include <sys/times.h>

#define KTYPE  15
#define KDIR   4
#define KBLOCK 5

#define NORMAL 7
#define HARD   15

#define LEFT   1
#define RIGHT  2
#define DOWN   3

static int on=-1;
int a[21][12]={
    {8,0,0,0,0,0,0,0,0,0,0,8},
    {8,0,0,0,0,0,0,0,0,0,0,8},
    {8,0,0,0,0,0,0,0,0,0,0,8},
    {8,0,0,0,0,0,0,0,0,0,0,8},
    {8,0,0,0,0,0,0,0,0,0,0,8},
    {8,0,0,0,0,0,0,0,0,0,0,8},
    {8,0,0,0,0,0,0,0,0,0,0,8},
    {8,0,0,0,0,0,0,0,0,0,0,8},
    {8,0,0,0,0,0,0,0,0,0,0,8},
    {8,0,0,0,0,0,0,0,0,0,0,8},
    {8,0,0,0,0,0,0,0,0,0,0,8},
    {8,0,0,0,0,0,0,0,0,0,0,8},
    {8,0,0,0,0,0,0,0,0,0,0,8},
    {8,0,0,0,0,0,0,0,0,0,0,8},
    {8,0,0,0,0,0,0,0,0,0,0,8},
    {8,0,0,0,0,0,0,0,0,0,0,8},
    {8,0,0,0,0,0,0,0,0,0,0,8},
    {8,0,0,0,0,0,0,0,0,0,0,8},
    {8,0,0,0,0,0,0,0,0,0,0,8},
    {8,0,0,0,0,0,0,0,0,0,0,8},
    {8,8,8,8,8,8,8,8,8,8,8,8}
};

unsigned char userid[30]="unknown.";
int dx[KTYPE][KDIR][KBLOCK], dy[KTYPE][KDIR][KBLOCK];
int d[KTYPE][KDIR][KBLOCK]={
    {{0,1,4,5,0}, {0,1,4,5,0}, {0,1,4,5,0}, {0,1,4,5,0}},
    {{4,5,6,7,4}, {1,5,9,13,1}, {4,5,6,7,4}, {1,5,9,13,1}},
    {{0,1,5,6,0}, {1,4,5,8,1}, {0,1,5,6,0}, {1,4,5,8,1}},
    {{1,2,4,5,1}, {0,4,5,9,0}, {1,2,4,5,1}, {0,4,5,9,0}},
    {{0,1,2,4,0}, {0,1,5,9,0}, {2,4,5,6,2}, {0,4,8,9,0}},
    {{0,1,2,6,0}, {1,5,8,9,1}, {0,4,5,6,0}, {0,1,4,8,0}},
    {{0,1,2,5,0}, {1,4,5,9,1}, {1,4,5,6,1}, {0,4,5,8,0}},
    {{1,1,1,1,1}, {1,1,1,1,1}, {1,1,1,1,1}, {1,1,1,1,1}},
    {{1,2,1,2,1}, {1,5,1,5,1}, {1,2,1,2,1}, {1,5,1,5,1}},
    {{1,2,6,1,2}, {2,5,6,2,6}, {1,5,6,1,5}, {1,2,5,1,2}},
    {{4,5,6,4,5}, {1,5,9,1,5}, {4,5,6,4,5}, {1,5,9,1,5}},
    {{0,1,2,4,6}, {1,2,6,9,10}, {0,2,4,5,6}, {0,1,4,8,9}},
    {{1,4,5,6,9}, {1,4,5,6,9}, {1,4,5,6,9}, {1,4,5,6,9}},
    {{1,2,5,8,9}, {0,4,5,6,10}, {1,2,5,8,9}, {0,4,5,6,10}},
    {{0,1,5,9,10}, {2,4,5,6,8}, {0,1,5,9,10}, {2,4,5,6,8}}
};

int k,n,y,x,e;
int newk=0;
int lines=0;
int difficulty=NORMAL;
int delay, level, score;
char topID[20][20];
int topT[20],topS[20];

SMTH_API struct user_info uinfo;

SMTH_API select_func x_select;
SMTH_API read_func x_read;

int sh(int,int,int,int,int);
int intr(void);
int start(void);
int quit(void);
int down(void);
int win_showrec(void);
int win_checkrec(int,int);
int checklines(void);
int count(void);
int win_saverec(void);
int win_sort(void);

int getch0()
{
    char ch;
    fd_set rfds;
    struct timeval tv;
    FD_ZERO(&rfds);
    FD_SET(0, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = 50000;
    if ((*x_select)(1, &rfds, NULL, NULL, &tv)) {
        if ((*x_read)(0, &ch, 1)<=0) exit(-1);
        return ch;
    } else
        return 0;
}

int getch()
{
    int c,d,e;
    int ret;

    static int *retbuf;
    static int retlen=0;

    static time_t old=0;
    time_t now;

    SMTH_API int keymem_total;
    SMTH_API struct key_struct *keymem;

    if (retlen > 0) {
        ret = *retbuf;
        retbuf ++;
        retlen --;

        return ret;
    }

    c=getch0();
    if (c==3||c==4||c==-1) return -1;
    if (c==Ctrl('C')) return c;

    if (c!=KEY_ESC) ret=c;
    else {
        d=getch0();
        e=getch0();
        if (d==0) ret=c;
        else if (e=='A') ret=KEY_UP;
        else if (e=='B') ret=KEY_DOWN;
        else if (e=='C') ret=KEY_RIGHT;
        else if (e=='D') ret=KEY_LEFT;
        else ret=0;
    }

    if (keymem_total) {
        int i,j,p;
        for (i=0;i<keymem_total;i++) {
            p=!keymem[i].status[0];
            if (keymem[i].status[0]==-1) continue;
            j=0;
            while (keymem[i].status[j]&&j<10) {
                if (keymem[i].status[j]==TETRIS) p=1;
                j++;
            }
            if (p&&ret==keymem[i].key) {
                j=0;
                while (keymem[i].mapped[j]&&j<10) j++;
                if (j==0) continue;
                ret = keymem[i].mapped[0];
                retlen = j-1;
                retbuf = keymem[i].mapped+1;
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

int color(int c)
{
    static int lastc=-1;
    char tmp[200];

    if (c==lastc) return -1;
    lastc=c;
    //if (c==4) c=12;
    /* 默认都是高亮吧 */
    sprintf(tmp,"\033[1;%dm",c%8+30);
    prints(tmp);
    return 0;
}

int clear2()
{
    clear();
    move(2,0);
    prints("                                      \033[1;33m┌─┐     \n");
    prints("                                      │Ｔ│     \n");
    prints("                                      └─┘     \n");
    prints("                                      \033[1;34m┌─┐     \n");
    prints("                                      │Ｅ│     \n");
    prints("                                      └─┘     \n");
    prints("                                      \033[1;33m┌─┐     \n");
    prints("                                      │Ｔ│     \n");
    prints("                                      └─┘     \n");
    prints("                                      \033[1;35m┌─┐     \n");
    prints("                                      │Ｒ│     \n");
    prints("                                      └─┘     \n");
    prints("                                      \033[1;31m┌─┐     \n");
    prints("                                      │Ｉ│     \n");
    prints("                                      └─┘     \n");
    prints("                                      \033[1;32m┌─┐     \n");
    prints("                                      │Ｓ│     \n");
    prints("                                      └─┘\033[m     \n");

    move(21,0);
    prints("\033[1;33m祝你玩的愉快! \033[m按 '\033[1;32mCtrl+C\033[m'退出.");
    return 0;
}

int sh2()
{
    static int oy=-1, ox=-1, ok=-1;

    if (oy==y && ox==x && ok==k && on==n) return -1;
    sh(oy, ox, ok, on, 0);
    oy=y; ox=x; ok=k; on=n;
    sh(oy,ox,ok,on,ok+1);
    refresh();
    return 0;
}

int sh(int y, int x, int k, int n, int c)
{
    if (n==-1) return -1;
    if (k==7&&c==0) {
        move(y+dy[7][0][0],2*(x+dx[7][0][0]));
        c=a[y+dy[7][0][0]][x+dx[7][0][0]];
        color(c);
        if (c)prints("■");else prints("  ");
    } else {
        for (e=0;e<KBLOCK;e++) {
            move(y+dy[k][n][e],2*(x+dx[k][n][e]));
            color(c);
            if (c)prints("■");else prints("  ");
        }
    }
    move(0,0);
    return 0;
}

int show0(int init)
{
    int ytmp,xtmp;
    if (init) {
        for (ytmp=0;ytmp<=20;ytmp++) {
            move(ytmp, 0);
            prints("\033[1;30m■");
            move(ytmp, 22);
            prints("\033[1;30m■");
        }
        for (xtmp=1;xtmp<=10;xtmp++) {
            move(20, 2*xtmp);
            prints("\033[1;30m■");
        }
        move(0,26);
        prints("\033[1;33m下一个\033[m");
        move(8,28);
        prints("\033[1;32m%8d\033[m", score);
        move(7,26);
        prints("\033[1;33m得分\033[m");
        move(11,28);
        prints("\033[1;32m%8d\033[m", lines);
        move(10,26);
        prints("\033[1;33m行数\033[m");
        move(13,26);
        prints("\033[1;33m级别\033[m");
        move(14,28);
        prints("\033[1;32m%8d\033[m", level);
    }
    for (ytmp=0;ytmp<20;ytmp++) {
        move(ytmp,2);
        for (xtmp=1;xtmp<=10;xtmp++) {
            color(a[ytmp][xtmp]);
            if (a[ytmp][xtmp])
                prints("■");
            else
                prints("  ");
        }
    }
    refresh();
    return 0;
}

int tetris_main(void)
{
    strcpy((void*)userid,getCurrentUser()->userid);
    modify_user_mode(TETRIS);
    intr();
    start();
    quit();
    return 0;
}

int quit(void)
{
    color(7);
    prints("\033[H\033[J欢迎常来, 再见!\n");
    refresh();
    return 0;
}

int init_data(void)
{
    for (k=0;k<KTYPE;k++)
        for (n=0;n<KDIR;n++)
            for (e=0;e<KBLOCK;e++) {
                dx[k][n][e]=d[k][n][e]%4;
                dy[k][n][e]=d[k][n][e]/4;
            }
    for (y=0;y<=19;y++)
        for (x=1;x<=10;x++) {
            a[y][x]=0;
        }
    srand(time(0));
    newk=rand()%difficulty;
    level=0;
    delay=200;
    lines=0;
    score=0;
    return 0;
}

int crash2(int x, int y, int k, int n, int dir)
{
    if (dir==LEFT)
        x--;
    else if (dir==RIGHT)
        x++;
    else if (dir==DOWN) {
        y++;
        if (k==7) {
            for(;y<20;y++)
                if (a[y+dy[7][0][0]][x+dx[7][0][0]]==0)
                    return 0;
            return 1;
        }
    }
    for (e=0;e<KBLOCK;e++)
        if (a[y+dy[k][n][e]][x+dx[k][n][e]])return 1;
    return 0;
}

int start(void)
{
    int c,t,first,newn;
    struct tms faint;
    win_showrec();
    while (1) {
        init_data();
        clear2();
        show0(1);
        first=1;
        newn=rand()%KDIR;
        while (1) {
            k=newk;
            newk=rand()%difficulty;
            n=newn;
            newn=rand()%KDIR;
            color(0);
            move(2,25);prints("            ");
            move(3,25);prints("            ");
            move(4,25);prints("            ");
            move(5,25);prints("            ");
            sh(2,14,newk,newn,newk+1);
            //n=0;
            x=3;y=0;
            sh2();
            if (first) {pressanykey();first=0;}
            if (crash2(x,y,k,n,0)) {
                win_checkrec(score,lines);
                break;
            }
            t=times(&faint);
            while (1) {
                //c = igetkey();
                c = getch();
                if (c==-1) return 0;
                if (c==27) pressanykey();
                if (c == Ctrl('D') || c == Ctrl('C')) {
                    win_checkrec(score,lines);
                    return 0;
                }
                if (c==KEY_LEFT||c=='a'||c=='A') if (!crash2(x,y,k,n,LEFT)) {x--;sh2();}
                if (c==KEY_RIGHT||c=='s'||c=='S') if (!crash2(x,y,k,n,RIGHT)) {x++;sh2();}
                if (c=='b'||c=='B'||c=='\n'||c=='\r') if (!crash2(x,y,k,(n+1)%KDIR,0)) {n=(n+1)%KDIR;sh2();}
                if (c=='h'||c=='H'||c==KEY_UP) if (!crash2(x,y,k,(n+3)%KDIR,0)) {n=(n+3)%KDIR;sh2();}
                if (c=='J'||c=='j') if (!crash2(x,y,k,(n+2)%KDIR,0)) {n=(n+2)%KDIR;sh2();}
                if (c==' ') {while (!crash2(x,y,k,n,DOWN))y++;sh2();down();break;}
                if ((int)times(&faint)-t>delay||c==KEY_DOWN||c=='z'||c=='Z') {
                    t=times(&faint);
                    if (crash2(x,y,k,n,DOWN)) {down();break;}
                    else {y++;sh2();}
                }
            }
        }
    }
    return 0;
}

int down(void)
{
    for (e=0;e<KBLOCK;e++)
        a[y+dy[k][n][e]][x+dx[k][n][e]]=k+1;
    checklines();
    on=-1;
    return 0;
}

int checklines(void)
{
    int y1,x1;
    int y2;
    int s;
    s=0;
    for (y1=0;y1<=19;y1++) {
        for (x1=1;x1<=10;x1++)
            if (a[y1][x1]==0)break;
        if (x1<=10) continue;
        s++;
        //move(23,0);
        //prints("\033[33mLines =\033[32m%3d", lines+1);
        //if (lines==0) prints("                         ");
        for (y2=y1;y2>=1;y2--)
            for (x1=1;x1<=10;x1++)
                a[y2][x1]=a[y2-1][x1];
        for (x1=1;x1<=10;x1++)
            a[0][x1]=0;
        if ((++lines)%30==0)
            {delay*=.8; level++; bell();move(14,28);prints("\033[1;32m%8d\033[m", level);}
    }
    if (s==1) score+=10;
    else if (s==2) score+=30;
    else if (s==3) score+=50;
    else if (s==4) score+=100;
    if (s) {
        move(8,28);
        prints("\033[1;32m%8d\033[m", score);
        move(11,28);
        prints("\033[1;32m%8d\033[m", lines);
        //move(23,15);
        //prints("\033[33mScore = \033[32m%5d",score);
        show0(0);
    }
    return 0;
}

void select_difficulty()
{
    int ch;
    while(1){
        move(15, 0);
        prints("选择游戏难度: %s", (difficulty==NORMAL)?"[\033[32m简单\033[m] [困难]":"[简单] [\033[32m困难\033[m]");
        ch = igetkey();
        if (ch==KEY_LEFT || ch==KEY_RIGHT || ch==KEY_TAB)
            difficulty = (difficulty==NORMAL) ? HARD : NORMAL;
        else if (ch=='\r'||ch=='\n')
            break;
    }
    return;
}

int intr(void)
{
    clear();
    prints("欢迎光临\033[1;32m%s!\033[1m \033[1;35m%s.\033[m 您是本游戏的第 \033[1;33m%d\033[m 位访问者。\r\n\r\n",BBS_FULL_NAME, userid, count());
    prints("键盘设置: \r\n");
    prints("左: '\033[1;32ma\033[m', '\033[1;32m←\033[m';\r\n");
    prints("右: '\033[1;32ms\033[m', '\033[1;32m→\033[m';\r\n");
    prints("下: '\033[1;32mz\033[m', '\033[1;32m↓\033[m';\r\n");
    prints("顺时针转动: '\033[1;32mb\033[m', <\033[1;32mCR\033[m>; 逆时针转动: '\033[1;32mh\033[m', '\033[1;32m↑\033[m'; 180度转动: '\033[1;32mj\033[m';\r\n");
    prints("快降: ' ', 暂停: <\033[1;32mESC\033[m>.\r\n");
    prints("退出: '\033[1;32m^C\033[m', '\033[1;32m^D\033[m'.\r\n\r\n");
    prints("每消 \033[1;33m30\033[m 行升一级. \r\n");
    //prints("\n\n\033[1;31m注意:不支持在ssh下进行游戏,只支持telnet\n");
    //pressanykey();
    select_difficulty();
    clear();
    return 0;
}

int count()
{
    int c;
    FILE *fp;
    fp=fopen("tetris.count","r");
    if (fp==NULL) {system("echo 1 > tetris.count"); return 1;}
    fscanf(fp,"%d",&c);
    fclose(fp);
    fp=fopen("tetris.count","w");
    fprintf(fp,"%d",c+1);
    fclose(fp);
    return c+1;
}

int win_loadrec()
{
    char file[STRLEN];
    FILE *fp;
    int n;
    for (n=0;n<=19;n++) {
        strcpy(topID[n],"unknown.");
        topT[n]=0;
        topS[n]=0;
    }
    sprintf(file, "tetris.rec.%s", difficulty==NORMAL?"normal":"hard");
    fp=fopen(file,"r");
    if (fp==NULL) {win_saverec();return -1;}
    for (n=0;n<=19;n++)
        fscanf(fp,"%s %d %d\n",topID[n],&topT[n],&topS[n]);
    fclose(fp);
    return 0;
}

int win_saverec()
{
    char file[STRLEN];
    FILE *fp;
    int n;
    sprintf(file, "tetris.rec.%s", difficulty==NORMAL?"normal":"hard");
    fp=fopen(file,"w");
    for (n=0;n<=19;n++) {
        fprintf(fp,"%s %d %d\n",topID[n],topT[n],topS[n]);
    }
    fclose(fp);
    return 0;
}

int win_showrec()
{
    int n;
    char tmp[200];

    win_loadrec();
    clear();
    prints("\033[44;37m                         - TETRIS 排行榜 (%s) -                                 \r\n\033[m", difficulty==NORMAL?"简单":"困难");
    prints("\033[41m No.          ID        LINES                         Score                      \033[m\n\r");
    for (n=0;n<=19;n++) {
        sprintf(tmp, "\033[1;37m%3d\033[32m%13s\033[0;37m%12d\033[m%29d\n\r",n+1,topID[n],topT[n]
                ,topS[n]);
        prints(tmp);
    }
    prints("\033[41m                                                                               \033[m\n\r");
    pressanykey();
    return 0;
}

int win_checkrec(int ds,int dt)
{
    char id[30];
    int n;
    win_loadrec();
    strcpy(id,(void*)userid);
    for (n=0;n<=19;n++)
        if (!strcmp(topID[n],id)) {
            if (ds>topS[n]) {
                topT[n]=dt;
                topS[n]=ds;
                win_sort();
                win_saverec();
                win_showrec();
            }
            return 1;
        }
    if (ds>topS[19]) {
        strcpy(topID[19],id);
        topT[19]=dt;
        topS[19]=ds;
        win_sort();
        win_saverec();
        win_showrec();
        return 2;
    }
    return 0;
}

int win_sort()
{
    int n,n2,tmp;
    char tmpID[30];
    clear();
    prints("祝贺! 您刷新了自己的纪录!\r\n");
    pressanykey();
    for (n=0;n<=18;n++)
        for (n2=n+1;n2<=19;n2++)
            if (topS[n]<topS[n2]) {
                tmp=topT[n];topT[n]=topT[n2];topT[n2]=tmp;
                tmp=topS[n];topS[n]=topS[n2];topS[n2]=tmp;
                strcpy(tmpID,topID[n]);strcpy(topID[n],topID[n2]);
                strcpy(topID[n2],tmpID);
            }
    return 0;
}

