/*
    Pirate Bulletin Board System
    Copyright (C) 1990, Edward Luke, lush@Athena.EE.MsState.EDU
    Eagles Bulletin Board System
    Copyright (C) 1992, Raymond Rocker, rocker@rock.b11.ingr.com
                        Guy Vega, gtvega@seabass.st.usm.edu
                        Dominic Tynes, dbtynes@seabass.st.usm.edu

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 1, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "bbs.h"
#include "screen.h"             /* Leeward 98.06.05 */
#define TELNET_WORD_WRAP 1
#define LIKE_SINGLE_LINE 1

time_t calltime = 0;
char calltimememo[40];
int stuffmode = 0;
enum LINE_CODE {
    LINE_NORMAL,                   //normal line
    LINE_NORMAL_NOCF,        //没有 回车的行
    LINE_QUOTA,                    //引文
    LINE_QUOTA_NOCF,         //没有回车的引文
/* like struct for newsmth, added by windinsn, 2013-4-14*/
#ifdef ENABLE_LIKE
    LINE_LIKE_NAME,          // Like开始段落
    LINE_LIKE_INFO,          // Like
    LINE_LIKE_MSG,             // Like的内容
#endif
    LINE_ATTACHMENT,         //附件
    LINE_ATTACHLINK,           //附件链接
    LINE_ATTACHALLLINK           //本文链接
};
#ifdef DEF_HIDEEMFLAG
static int mem_show(char *ptr, int size, int origsize, int row, int numlines, char *fn);
#else
static int mem_show(char *ptr, int size, int row, int numlines, char *fn);
#endif

static generate_attach_link_t current_attach_link;
static void* current_attach_link_arg;
void register_attach_link(generate_attach_link_t fn,void* arg)
{
    current_attach_link=fn;
    current_attach_link_arg=arg;
}

struct ACSHM {
    char line[ACBOARD_MAXLINE][ACBOARD_BUFSIZE];
    int movielines;
    time_t update;
};
struct ACSHM *movieshm;

int nnline = 0, xxxline = 0;
int more_size, more_num;
int displayflag = 0, shownflag = 1;

#ifdef DEF_HIDEEMFLAG
static int mem_more(char *ptr, int size, int origsize, int quit, char *keystr, char *fn, char *title);
#else
static int mem_more(char *ptr, int size, int quit, char *keystr, char *fn, char *title);
#endif

int NNread_init()
{
    FILE *fffd;
    char buf[ACBOARD_BUFSIZE];
    struct stat st;
    time_t ftime, now;
    int iscreate;
#ifdef ACBOARD_BNAME
    struct fileheader fh;
    int fd;
    char fname[PATHLEN];
    int i;
#else
    char *ptr;
#endif

    now = time(0);
#ifdef ACBOARD_BNAME
    if (stat("boards/"ACBOARD_BNAME"/.DIGEST", &st) < 0) {
#else
#if MAXnettyLN > 5
    if (stat("etc/movie7", &st) < 0) {
#else
        if (stat("etc/movie", &st) < 0) {
#endif
#endif
        return 0;
    }
    ftime = st.st_mtime;
    if (movieshm == NULL) {
        if (MAXnettyLN > 5)
            movieshm = (void *) attach_shm("ACBOARD7_SHMKEY", 4124, sizeof(*movieshm), &iscreate);
        else
            movieshm = (void *) attach_shm("ACBOARD_SHMKEY", 4123, sizeof(*movieshm), &iscreate);
    }
    if (abs(now - movieshm->update) < 12 * 60 * 60 && ftime < movieshm->update) {
        return 1;
    }
    /*--- modified by period 2000-10-20 ---*
            if ((fffd = fopen( "etc/movie" , "r" )) == NULL) {
                    return 0;
            }
     ---*/
    nnline = 0;
    xxxline = 0;
    if (!DEFINE(getCurrentUser(), DEF_ACBOARD)) {
        nnline = 1;
        xxxline = 1;
        return 1;
    }
#ifdef ACBOARD_BNAME
    if ((fd=open("boards/"ACBOARD_BNAME"/.DIGEST", O_RDONLY, 0)) == -1)
        return 0;
    while (read(fd, &fh, sizeof(fh)) == sizeof(fh)) {
        if (xxxline >= ACBOARD_MAXLINE)
            break;
        setbfile(fname, ACBOARD_BNAME, fh.filename);
        if ((fffd = fopen(fname, "r")) == NULL)
            continue;
        for (i=0; i<4; i++) //跳过文章头部信息
            fgets(buf, ACBOARD_BUFSIZE, fffd);
        for (i=0; i < MAXnettyLN; i++) {
            if (fgets(buf, ACBOARD_BUFSIZE, fffd) == 0)
                break;
            if (! strcmp(buf, "--\n"))
                break;
            if (xxxline >= ACBOARD_MAXLINE)
                break;
            strncpy(movieshm->line[xxxline], buf, ACBOARD_BUFSIZE);
            movieshm->line[xxxline][ACBOARD_BUFSIZE-1]='\0';
            xxxline ++;
        }
        if (xxxline >= ACBOARD_MAXLINE) {
            fclose(fffd);
            break;
        }
        fclose(fffd);
        for (; i<MAXnettyLN; i++) {
            if (xxxline >= ACBOARD_MAXLINE)
                break;
            sprintf(buf, "%79.79s\n", " ");
            strcpy(movieshm->line[xxxline], buf);
            xxxline++;
        }
    }
    close(fd);
#else
    /*--- 原有程序顺序有误, !DEFINE --> return后没close ---*/
#if MAXnettyLN > 5
    if ((fffd = fopen("etc/movie7", "r")) == NULL)
#else
    if ((fffd = fopen("etc/movie", "r")) == NULL)
#endif
        return 0;
    /*--- ---*/
    while ((xxxline < ACBOARD_MAXLINE) && (fgets(buf, ACBOARD_BUFSIZE, fffd) != NULL)) {
        ptr = movieshm->line[xxxline];
        memcpy(ptr, buf, sizeof(buf));
        xxxline++;
    }
    fclose(fffd);
    sprintf(buf, "%79.79s\n", " ");
    while (xxxline % MAXnettyLN != 0) {
        ptr = movieshm->line[xxxline];
        memcpy(ptr, buf, sizeof(buf));
        xxxline++;
    }
#endif
    movieshm->movielines = xxxline;
    movieshm->update = time(0);
    sprintf(buf, "%d 行 活动看板 更新", xxxline);
    bbslog("user", "%s", buf);
    return 1;
}

int check_calltime()
{

    if (calltime != 0 && time(0) >= calltime) {
        int line;
        int ch;
        int y,x;
        /*
         * if (uinfo.mode != MMENU)
         * {
         * bell();
         * move(0,0);
         * clrtoeol();
         * prints("请到主选单看备忘录......");
         * return;
         * }
         * set_alarm(0,0, NULL,NULL);
         * showusernote();
         * pressreturn();
         * R_monitor(NULL);
         */
        getyx(&y, &x);
        if (uinfo.mode == TALK)
            line = t_lines / 2 - 1;
        else
            line = 0;
        saveline(line, 0, NULL);        /* restore line */
        bell();
        bell();
        bell();
        move(line, 0);
        clrtoeol();
        calltimememo[39]='\0';
        prints("\033[44m\033[32mBBS 系统闹铃: \033[37m%-40s\033[m回车继续",calltimememo);
        refresh();
        for (ch=igetch(); ch!='\r' && ch!='\n'; ch=igetch()) ;
        move(line, 0);
        clrtoeol();
        saveline(line, 1, NULL);
        calltime = calc_calltime(0);
        move(y,x);
        return 1;
    }
    return 0;
}

void setcalltime()
{
    char ans[6];
    int ttt;

    move(1, 0);
    clrtoeol();
    getdata(1, 0, "几分钟后要系统提醒你: ", ans, 3, DOECHO, NULL, true);
    if (!isdigit(ans[0]))
        return;
    ttt = atoi(ans);
    if (ttt <= 0)
        return;
    calltime = time(0) + ttt * 60;

}

#if 0
int morekey()
{
    while (1) {
        switch (igetkey()) {
            case Ctrl('Y'):
                return Ctrl('Y');
            case Ctrl('Z'):
                return 'M';         /* Leeward 98.07.30 support msgX */
            case '!':
                return '!';         /*Haohmaru 98.09.24 */
            case 'q':
            case KEY_LEFT:
            case EOF:
                return KEY_LEFT;
            case ' ':
            case KEY_RIGHT:
            case KEY_PGDN:
            case Ctrl('F'):
                return KEY_RIGHT;
            case KEY_PGUP:
            case Ctrl('B'):
                return KEY_PGUP;
            case '\r':
            case KEY_DOWN:
            case 'j':
                return KEY_DOWN;
            case 'k':
            case KEY_UP:
                return KEY_UP;
                /*************** 新增加阅读时的热键 Luzi 1997.11.1 ****************/
            case 'h':
            case '?':
                return 'H';
            case 'o':
            case 'O':
                return 'O';
            case 'l':
            case 'L':
                return 'L';
            case 'w':
            case 'W':
                return 'W';
            case 'H':
                return 'M';
            case 'X':              /* Leeward 98.06.05 */
                return 'X';
            case 'u':              /*Haohmaru 99.11.29 */
                return 'u';
            case KEY_REFRESH:
                return KEY_REFRESH;
            default:;
        }
    }
}
#endif

int seek_nth_line(int fd, int no, char *more_buf)
{
    int n_read, line_count, viewed;
    char *p, *end;

    lseek(fd, 0, SEEK_SET);
    line_count = viewed = 0;
    if (no > 0)
        while (1) {
            n_read = read(fd, more_buf, MORE_BUFSIZE);
            p = more_buf;
            end = p + n_read;
            for (; p < end && line_count < no; p++)
                if (*p == '\n')
                    line_count++;
            if (line_count >= no) {
                viewed += (p - more_buf);
                lseek(fd, viewed, SEEK_SET);
                break;
            } else
                viewed += n_read;
        }

    more_num = MORE_BUFSIZE + 1;        /* invalidate the readln()'s buffer */
    return viewed;
}

/*Add by SmallPig*/
int countln(fname)
char *fname;
{
    FILE *fp;
    char tmp[256];
    int count = 0;

    if ((fp = fopen(fname, "r")) == NULL)
        return 0;

    while (fgets(tmp, sizeof(tmp), fp) != NULL)
        count++;
    fclose(fp);
    return count;
}

/*
 * below added by netty
                            *//*
                            * Rewrite by SmallPig
                            */
void netty_more()
{
    char buf[350];
    int ne_row = 1;
    int x, y;
    time_t thetime = time(0);

    if (!DEFINE(getCurrentUser(), DEF_ACBOARD)) {
        update_endline();
        return;
    }

    if (movieshm->movielines <= 0)
        nnline = 0;
    else
        nnline = ((thetime / 10) % (movieshm->movielines / MAXnettyLN)) * MAXnettyLN;

    getyx(&y, &x);
    update_endline();
    move(3, 0);
    while ((nnline < movieshm->movielines) /*&&DEFINE(getCurrentUser(),DEF_ACBOARD) */) {
        move(((MAXnettyLN>5)?1:2) + ne_row, 0);
        clrtoeol();

        strcpy(buf, movieshm->line[nnline]);
        showstuff(buf);
        nnline = nnline + 1;
        ne_row = ne_row + 1;
        if (nnline == movieshm->movielines) {
            nnline = 0;
            break;
        }
        if (ne_row > MAXnettyLN) {
            break;
        }
    }
    move(y, x);
}

void printacbar()
{
    int x, y;

    if (MAXnettyLN > 5) return;

    getyx(&y, &x);

    move(2, 0);
    if (DEFINE(getCurrentUser(), DEF_HIGHCOLOR))
        prints("\033[1;35m┌——————————————┤\033[37m活  动  看  版\033[35m├——————————————┐ \033[m\n");
    else
        prints("\033[35m┌——————————————┤\033[37m活  动  看  版\033[35m├——————————————┐ \033[m\n");
    move(3 + MAXnettyLN, 0);
    if (DEFINE(getCurrentUser(), DEF_HIGHCOLOR))
        prints("\033[1;35m└——————————————┤\033[36m" FOOTER_MOVIE "\033[35m├——————————————┘ \033[m\n");
    else
        prints("\033[35m└——————————————┤\033[36m" FOOTER_MOVIE "\033[35m├——————————————┘ \033[m\n");
    move(y, x);

}

extern int idle_count;
void R_monitor(void *data)
{
    if (!DEFINE(getCurrentUser(), DEF_ACBOARD))
        return;

    if (uinfo.mode != MMENU)
        return;
    netty_more();
    printacbar();
    idle_count++;
    set_alarm(10 * idle_count, 0, R_monitor, NULL);
    UNUSED_ARG(data);
}

#ifdef DEF_HIDEEMFLAG
/* 去除文章中的表情符号, jiangjun, 20120209 */
int is_em_flag(char *buf, int len, char *tag)
{
    char *p;
    if (strncmp(buf, "b", len)==0 || strncmp(buf, "i", len)==0 || strncmp(buf, "u", len)==0) {
        strncpy(tag, buf, len);
        tag[len] = '\0';
        return 1;
    }
    if (strncmp(buf, "/img", len)==0 || strncmp(buf, "/swf", len)==0 || strncmp(buf, "/url", len)==0 || strncmp(buf, "/code", len)==0 ||
        strncmp(buf, "/email", len)==0 || strncmp(buf, "/mp3", len)==0 || strncmp(buf, "/upload", len)==0 ||
        strncmp(buf, "/color", len)==0 || strncmp(buf, "/face", len)==0 || strncmp(buf, "/size", len)==0 ||
        strncmp(buf, "/b", len)==0 || strncmp(buf, "/i", len)==0 || strncmp(buf, "/u", len)==0) {
        strncpy(tag, buf+1, len-1);
        tag[len-1] = '\0';
        return -1;
    }
    /* 带url地址的返回2 */
    if (strncmp(buf, "img=", 4)==0) {
        strncpy(tag, buf, 3);
        tag[3] = '\0';
        return 2;
    }
    if (strncmp(buf, "swf=", 4)==0) {
        strncpy(tag, buf, 3);
        tag[3] = '\0';
        return 2;
    }
    if (strncmp(buf, "url=", 4)==0) {
        strncpy(tag, buf, 3);
        tag[3] = '\0';
        return 2;
    }
    if (strncmp(buf, "mp3=", 4)==0) {
        strncpy(tag, buf, 3);
        tag[3] = '\0';
        return 2;
    }
    if (strncmp(buf, "email=", 6)==0) {
        strncpy(tag, buf, 5);
        tag[5] = '\0';
        return 2;
    }
    if (strncmp(buf, "upload=", 7)==0) {
        for (p=buf+7;*p!=']';p++) {
            if (!isdigit(*p))
                return 0;
        }
        strncpy(tag, buf, 6);
        tag[6] = '\0';
        return 1;
    }
    if (strncmp(buf, "color=#", 7)==0) {
        for (p=buf+7;*p!=']';p++) {
            if (!isalnum(*p))
                return 0;
        }
        strncpy(tag, buf, 5);
        tag[5] = '\0';
        return 1;
    }
    if (strncmp(buf, "size=", 5)==0) {
        for (p=buf+5;*p!=']';p++) {
            if (!isdigit(*p))
                return 0;
        }
        strncpy(tag, buf, 4);
        tag[4] = '\0';
        return 1;
    }
    if (strncmp(buf, "face=", 5)==0) { /* 字体需要一个一个对比么? */
        strncpy(tag, buf, 4);
        tag[4] = '\0';
        return 1;
    }
    /* 非成对标签，tag[0]='\0' */
    if (strncmp(buf, "ema", 3)==0 || strncmp(buf, "emb", 3)==0 || strncmp(buf, "emc", 3)==0) {
        for (p=buf+3;*p!=']';p++) {
            if (!isdigit(*p))
                return 0;
        }
        tag[0] = '\0';
        return 1;
    }
    if (strncmp(buf, "em", 2)==0) {
        for (p=buf+2;*p!=']';p++) {
            if (!isdigit(*p))
                return 0;
        }
        tag[0] = '\0';
        return 1;
    }
    if (strncmp(buf, "code=", 5)==0) {
        for (p=buf+5;*p!=']';p++) {
            if (!isalnum(*p))
                return 0;
        }
        strncpy(tag, buf, 4);
        tag[4] = '\0';
        return 1;
    }
    return 0;
}

struct em_arg {
    char *p;
    char *q;
    char tag[8];
    struct em_arg *next;
};

int remove_em_flags(char *ptr, int size)
{
    char *p, *q, *end;
    char tag[8];
    int em_len, newsize, attsize, type;
    struct em_arg *head, *e;

    newsize = size;
    attsize = 0;
    if ((end=memmem(ptr, size, ATTACHMENT_PAD, ATTACHMENT_SIZE))!=NULL) {
        attsize = size - (end - ptr);
        size = end - ptr;
    } else
        end = ptr + size;
    p = ptr;
    head = NULL;
    e = NULL;
    do {
        if ((p=memmem(ptr, size, "[", 1))!=NULL) {
            if ((q=memmem(p, size-(p-ptr), "]", 1))!=NULL && (em_len=q-p-1)>0) {
                type = is_em_flag(p+1, em_len, tag);
                if (type>0) {
                    if (tag[0]=='\0') { /* 是单个表情符号, 向前挪动, 并从']'的下一个继续搜索 */
                        size -= (q+1-ptr);
                        memmove(p, q+1, size+attsize);
                        newsize -= q+1-p;
                        ptr = p;
                        continue;
                    } else { /* 是成对表情符号的第一个，临时保存 */
                        e = (struct em_arg*)malloc(sizeof(struct em_arg));
                        e->p = p;
                        e->q = q;
                        if (type==2) { /* 带url时重新指定p、q */
                            e->p = p+1;
                            e->q = p+1+strlen(tag);
                        }
                        strcpy(e->tag, tag);
                        e->next = head;
                        head = e;
                    }
                } else if (type==-1) { /* 是成对表情符号的第二个，从后往前寻找对应的符号 */
                    e = head;
                    while (e && strcmp(e->tag, tag)) /* 循环找到对应符号 */
                        e = e->next;
                    if (e) { /* 有对应的符号 */
                        int t;
                        struct em_arg *a;

                        t = e->q+1-e->p;
                        /* 挪第一个 */
                        size = end-(e->q+1);
                        memmove(e->p, e->q+1, size+attsize);
                        newsize -= t;

                        e = head;
                        while (e && strcmp(e->tag, tag)) { /* 循环校正指针位置 */
                            a = e;
                            a->p -= t;
                            a->q -= t;
                            e = a->next;
                        }
                        if (e==head)
                            head = e->next;
                        else
                            a->next = e->next;
                        free(e);

                        /* 挪第二个 */
                        p -= t;
                        q -= t;
                        size = end-(q+1);
                        memmove(p, q+1, size+attsize);
                        newsize -= q+1-p;
                        ptr = p;
                        continue;
                    }
                }
            }
            /* 不是表情符号, 从'['的下一个继续搜索 */
            size -= (p+1-ptr);
            ptr = p+1;
        } else
            break;
    } while (size>0);
    while (head) {
        e = head;
        head = e->next;
        free(e);
    }
    return newsize;
}
#endif /* DEF_HIDEEMFLAG */

struct MemMoreLines {
    char *ptr;
    int size;
    char *line[100];
    char ty[100];               /* 0: 普通, 有回车; 1: 普通, 无回车; 2: 引文, 有回车; 3: 引文, 无回车 */
    int len[100];
    int s[100];
    int start;                  /* this->line[start%100]是检索的行号最小的行，行号为 start */
    int num;                    /* 共检索了row到row+num-1这么多行 */
    int curr_line;              /* 当前游标位置 */
    char *curr;                 /* 当前游标的行 */
    char currty;
    int currlen;
    int total;
#ifdef DEF_HIDEEMFLAG
    int hidden_size;
#endif
#ifdef ENABLE_LIKE
    unsigned int like;
    int like_start_size;
    int like_end_size;
#endif
};

/*
  作用:分析一行的数据
  p0是文章缓冲区
  size是缓冲区大小
  *l用于返回行的显示长度
  *s返回行占用字节长度
  oldty是上一行的type
  *ty返回行的type
*/
#ifdef ENABLE_LIKE
int measure_line(char *p0, int size, int *l, int *s, char oldty, char *ty, int like_start_size, int like_end_size)
#else
int measure_line(char *p0, int size, int *l, int *s, char oldty, char *ty)
#endif
{
    int i, w, in_esc = 0, db = 0, space = 0, lastspace = 0, autoline = 1;
    char *p = p0;
    int att_size;

    if (size <= 0) {
        if ((oldty==LINE_ATTACHLINK)&&(size==0)) {
            //这里有个假设，附件是最后一个
            *ty=LINE_ATTACHALLLINK;
            *s=0;
            *l=0;
            return 0;
        }
        return -1;
    }
#ifdef ENABLE_LIKE
    if (oldty!=LINE_ATTACHMENT && oldty!=LINE_LIKE_NAME && oldty!=LINE_LIKE_INFO && oldty!=LINE_LIKE_MSG)
#else
    if (oldty!=LINE_ATTACHMENT) //上一行不是附件
#endif
    {
        for (i = 0, w = 0; i < size; i++, p++) {
            if (*p == '\n') {
                *l = i;
                *s = i + 1;
                break;
            }
            if (*p == '\0') {
                *l = i;
                *s = i;
                break;
            }
            /*        if (asciiart) {
                        continue;
                    } else*/
            if (*p == '\t') {
                db = 0;
                w = (w + 8) / 8 * 8;
                lastspace = i;
            } else if (*p == '\033') {
                db = 0;
                in_esc = 1;
                space = i - 1; /* 先记录下\033位置 */
            } else if (in_esc) {
                if (strchr("suHmMfL@PABCDJK", *p) != NULL) {
                    if (strchr("suHABCDJ", *p) != NULL) autoline=0;
                    /* 不是颜色控制符时才在\033处折行, 即如果是颜色控制符, lastspace不记录在\033处 */
                    else if (*p != 'm')
                        lastspace = space;
                    in_esc = 0;
                    continue;
                }
                //            if (strchr("[0123456789;,", *p) == NULL)
                //                in_esc = 0;
            } else if (isprint2(*p)) {
                if (!db) {
                    if (autoline)
                        if ((w >= scr_cols-1&&(i>=size-1||*(p+1)<0))|| w >= scr_cols) {
#ifdef TELNET_WORD_WRAP
                            if (((unsigned char)*p<128) && (lastspace>0) && (lastspace<i)) {
                                *l = lastspace+1;
                                *s = lastspace+1;
                            } else {
#endif
                                *l = i;
                                *s = i;
#ifdef TELNET_WORD_WRAP
                            }
#endif
                            break;
                        }
                    if ((unsigned char) *p >= 128)
                        db = 1;
                    else if (strchr(" >)]},;'", *p))
                        lastspace = i;
                } else {
                    db = 0;
                    lastspace = i;
                }
                w++;
            }
        }
        if (i >= size) {
            *l = size;
            *s = size;
        }
        if (*s > 0 && ((p0[*s - 1] == '\n') || (p0[*s - 1] == '\0'))) {
            /*有回车的行*/
            switch (oldty) {
                case LINE_NORMAL_NOCF:
                    *ty = LINE_NORMAL;
                    break;
                case LINE_QUOTA_NOCF:
                    *ty = LINE_QUOTA;
                    break;
                default:
#ifdef TELNET_WORD_WRAP
                    if (*l > 1 && (!strncmp(p0, ": " ,2) || !strncmp(p0, "> " ,2)))
#else
                    if (*l > 2 && (!strncmp(p0, ": " ,2) || !strncmp(p0, "> " ,2)))
#endif
                        *ty = LINE_QUOTA; //引文
                    else
                        *ty = LINE_NORMAL;
            }
        } else {
            /*无回车的行*/
            switch (oldty) {
                case LINE_NORMAL_NOCF:
                    *ty = LINE_NORMAL_NOCF;
                    break;
                case LINE_QUOTA_NOCF:
                    *ty = LINE_QUOTA_NOCF;
                    break;
                default:
#ifdef TELNET_WORD_WRAP
                    if (*l > 1 && (!strncmp(p0, ": " ,2) || !strncmp(p0, "> " ,2)))
#else
                    if (*l > 2 && (!strncmp(p0, ": " ,2) || !strncmp(p0, "> " ,2)))
#endif
                        *ty = LINE_QUOTA_NOCF;
                    else
                        *ty = LINE_NORMAL_NOCF;
            }
        }
        if (*s == size)
            return 0;
    }
#ifdef ENABLE_LIKE
    if (oldty!=LINE_ATTACHMENT && oldty!=LINE_ATTACHLINK && oldty!=LINE_ATTACHALLLINK && oldty!=LINE_LIKE_NAME && oldty!=LINE_LIKE_INFO && oldty!=LINE_LIKE_MSG) {
        if(size>=LIKE_PAD_SIZE && !memcmp(p0, LIKE_PAD, LIKE_PAD_SIZE)) {
            // 开始解析Like
            *ty=LINE_LIKE_NAME;
            *s=LIKE_PAD_SIZE;
            *l=0;
            return 0;
        }
    }
    if (oldty==LINE_LIKE_NAME||oldty==LINE_LIKE_MSG) {
        if(size>like_start_size || size<=like_end_size){
            *s=1;
            *l=0;
            *ty=LINE_NORMAL;
            return 0;
        } else if(size<sizeof(struct like) || (size>=strlen("\n") && !memcmp(p0, "\n", strlen("\n")))) {
            // Like完毕
        } else {
            *s=sizeof(struct like);

        //if(*s>size) 
        //    return -1;
        //if((*s+1==size)||(*s<0));
        //    *s=size;
#ifdef LIKE_SINGLE_LINE
            *ty=LINE_LIKE_MSG;
#else        
            *ty=LINE_LIKE_INFO;
            *s=0;
#endif
            *l=0;
            return 0;
        }
    }
#ifndef LIKE_SINGLE_LINE
    if(oldty==LINE_LIKE_INFO) {
        // 上一行是Like的基本信息，本行显示内容
        *ty=LINE_LIKE_MSG;
        *l=0;
        *s=sizeof(struct like);
        if(*s>size)
            return -1;
        return 0;
    }
#endif
#endif
    if (oldty==LINE_ATTACHMENT|| (size > ATTACHMENT_SIZE
                                  && !memcmp(p0,  ATTACHMENT_PAD, ATTACHMENT_SIZE))) {

        *ty = LINE_ATTACHMENT;
        p = p0;
        /* 上一行为附件的行的size=1,因此-1获得真正的attach开始*/
        if (oldty==LINE_ATTACHMENT) p--;
        /*  跳过attachment前面的PAD */
        p += ATTACHMENT_SIZE;

        /* 跳过文件名*/
        if ((p = (char *) memchr(p, '\0', size - (ATTACHMENT_SIZE))) == NULL) {
            return 0;
        }
        p++;
        memcpy(&att_size, p, sizeof(int));
        *s = ntohl(att_size) + p - p0 + sizeof(int);
        if (oldty==LINE_ATTACHMENT) { /*上次是附件，下一行是附件连接行*/
            *ty=LINE_ATTACHLINK;
            /* ??
             * (*s)--;
             */
        } else {
            if ((*s>size)||(*s<0))
                *s=size;
            else
                *s=1;
        }
    } else {
        /* ??
        if (p0[*s-1]=='\0')
            (*s)++;
        */
    }
    return 0;
}

int effectiveline;              /*有效行数, 只计算前面的部分, 头部不含, 空行不含, 签名档不含, 引言不含 */
#ifdef DEF_HIDEEMFLAG
void init_MemMoreLines(struct MemMoreLines *l, char *ptr, int size, int hidden_size)
#else
void init_MemMoreLines(struct MemMoreLines *l, char *ptr, int size)
#endif
{
    int i, s, u;
    char *p0, oldty = 0;

    bzero(l, sizeof(struct MemMoreLines));
    l->ptr = ptr;
    l->size = size;
    l->start = 0;
    l->num = 0;
    l->total = 0;
#ifdef DEF_HIDEEMFLAG
    l->hidden_size = hidden_size;
#endif
#ifdef ENABLE_LIKE
    l->like=0;
    l->like_start_size=0;
    l->like_end_size=0;
    
    char *like_head, *like_tail;
    off_t like_start, like_end;
    if ((like_head=memmem(ptr, size, LIKE_PAD, LIKE_PAD_SIZE))!=NULL) {
        like_start=like_head-ptr;
        if ((like_tail=memmem(ptr, size, ATTACHMENT_PAD, ATTACHMENT_SIZE))!=NULL) {
            like_end=like_tail-ptr;
        } else {
            like_end=size;
        }
        like_start += LIKE_PAD_SIZE;
        like_end --;
        if(like_end>like_start) {
            l->like=(unsigned int)((like_end-like_start)/sizeof(struct like));
            l->like_start_size = size-like_start;
            l->like_end_size = size-like_end;
        }
    }
#endif
    effectiveline = 0;
    for (i = 0, p0 = ptr, s = size; i < 50 && s >= 0; i++) {
        u = (l->start + l->num) % 100;
        l->line[u] = p0;
        if (
#ifdef ENABLE_LIKE
        measure_line(p0, s, &l->len[u], &l->s[u], oldty, &l->ty[u], l->like_start_size, l->like_end_size)
#else    
        measure_line(p0, s, &l->len[u], &l->s[u], oldty, &l->ty[u])
#endif        
            < 0) {
            break;
        }

        oldty = l->ty[u];
        s -= l->s[u];
        p0 = l->line[u] + l->s[u];
        l->num++;
        if (effectiveline >= 0) {
            if (l->len[u] >= 2 && strncmp(l->line[u], "--", 2) == 0)
                effectiveline = -effectiveline;
            else if (l->num > 3 && l->len[u] >= 2 && l->ty[u] < 2)
                effectiveline++;
        }
    }
    if (effectiveline < 0)
        effectiveline = -effectiveline;
    if (s == 0)
        l->total = l->num;
    l->curr_line = 0;
    l->curr = l->line[0];
    l->currlen = l->len[0];
    l->currty = l->ty[0];
}

int next_MemMoreLines(struct MemMoreLines *l)
{
    int n;
    char *p0;

    if (l->curr_line + 1 >= l->start + l->num) {
        char oldty;

        n = (l->start + l->num - 1) % 100;
        /*
                if (l->ptr + l->size == (l->line[n] + l->s[n])) {
                    return -1;
                }
        */
        if (l->num == 100) {
            l->start++;
            l->num--;
        }
        oldty = l->ty[n];
        p0 = l->line[n] + l->s[n];
        n = (l->start + l->num) % 100;
        l->line[n] = p0;
        if (
#ifdef ENABLE_LIKE
            measure_line(p0, l->size - (p0 - l->ptr), &l->len[n], &l->s[n], oldty, &l->ty[n], l->like_start_size, l->like_end_size)
#else
            measure_line(p0, l->size - (p0 - l->ptr), &l->len[n], &l->s[n], oldty, &l->ty[n])
#endif
            ==-1)
            return -1;
        l->num++;
        if (l->size - (p0 - l->ptr) == l->s[n]) {
            l->total = l->start + l->num;
        }
    }
    l->curr_line++;
    l->curr = l->line[l->curr_line % 100];
    l->currlen = l->len[l->curr_line % 100];
    l->currty = l->ty[l->curr_line % 100];
    return l->curr_line;
}

int seek_MemMoreLines(struct MemMoreLines *l, int n)
{
    int i;

    if (n < 0) {
        seek_MemMoreLines(l, 0);
        return -1;
    }
    if (n < l->start) {
        i = l->total;
#ifdef DEF_HIDEEMFLAG
        init_MemMoreLines(l, l->ptr, l->size, l->hidden_size);
#else
        init_MemMoreLines(l, l->ptr, l->size);
#endif
        l->total = i;
    }
    if (n < l->start + l->num) {
        l->curr_line = n;
        l->curr = l->line[l->curr_line % 100];
        l->currlen = l->len[l->curr_line % 100];
        l->currty = l->ty[l->curr_line % 100];
        return l->curr_line;
    }
    while (l->curr_line != n)
        if (next_MemMoreLines(l) < 0)
            return -1;
    return 0;
}

#include <sys/mman.h>

int mmap_show(char *fn, int row, int numlines)
{
    char *ptr;
    off_t size;
    int retv=0;

    BBS_TRY {
        if (safe_mmapfile(fn, O_RDONLY, PROT_READ, MAP_SHARED, &ptr, &size, NULL) == 0)
            BBS_RETURN(0);
#ifdef DEF_HIDEEMFLAG
        if (getCurrentUser() && DEFINE(getCurrentUser(), DEF_HIDEEMFLAG)) {
            char *p;
            int newsize;
            p = (char *)malloc(size);
            memcpy(p, ptr, size);
            newsize = remove_em_flags(p, size);
            retv = mem_show(p, newsize, size, row, numlines, fn);
            free(p);
        } else {
            retv = mem_show(ptr, size, size, row, numlines, fn);
        }
#else
        retv = mem_show(ptr, size, row, numlines, fn);
#endif
    }
    BBS_CATCH {
    }
    BBS_END;
    end_mmapfile((void *) ptr, size, -1);

    return retv;
}

int mmap_more(char *fn, int quit, char *keystr, char *title)
{
    char *ptr;
    off_t size;
    int retv=0;

    BBS_TRY {
        if (safe_mmapfile(fn, O_RDONLY, PROT_READ, MAP_SHARED, &ptr, &size, NULL) == 0)
            BBS_RETURN(-1);
#ifdef DEF_HIDEEMFLAG
        if (getCurrentUser() && DEFINE(getCurrentUser(), DEF_HIDEEMFLAG)) {
            char *p;
            int newsize;
            p = (char *)malloc(size);
            memcpy(p, ptr, size);
            newsize = remove_em_flags(p, size);
            retv = mem_more(p, newsize, size, quit, keystr, fn, title);
            free(p);
        } else {
            retv = mem_more(ptr, size, size, quit, keystr, fn, title);
        }
#else
        retv = mem_more(ptr, size, quit, keystr, fn, title);
#endif
    }
    BBS_CATCH {
    }
    BBS_END;
    end_mmapfile((void *) ptr, size, -1);

    return retv;
}

void mem_printline(struct MemMoreLines *l, char *fn,char* begin)
{
    char* ptr=l->curr;
    int len=l->currlen;
    int ty=l->currty;
    if (ty == LINE_ATTACHMENT || ty == LINE_ATTACHLINK) {
        char slink[256];
        char attachname[STRLEN], *p;
        unsigned int attlen;
        strncpy(attachname, ptr + ATTACHMENT_SIZE, STRLEN);
        attachname[STRLEN - 1] = '\0';

        memcpy(&attlen, (ptr + ATTACHMENT_SIZE + strlen(attachname) + 1), sizeof(int));
        attlen = ntohl(attlen);

        p = strrchr(attachname, '.');
        if (p == NULL) p = "";
        if (ty == LINE_ATTACHMENT) {
            char attlenbuf[16];
            snprintf(attlenbuf, 16, "%d %s",(attlen>8192)?attlen/1024:attlen,(attlen>8192)?"KB":"Bytes");
            if (get_attachment_type_from_ext(p) == ATTACH_IMG)
                prints("\033[m附图: %s (%s) 链接:\n",attachname,attlenbuf);
            else
                prints("\033[m附件: %s (%s) 链接:\n",attachname,attlenbuf);
        } else {
            if (current_attach_link)
#ifdef DEF_HIDEEMFLAG
                (*current_attach_link)(slink,255,p,attlen,ptr-begin+l->hidden_size+ATTACHMENT_SIZE-1,current_attach_link_arg);
#else
                (*current_attach_link)(slink,255,p,attlen,ptr-begin+ATTACHMENT_SIZE-1,current_attach_link_arg);
#endif
            else
                strcpy(slink,"(用www方式阅读本文可以下载此附件)");
            prints("\033[4m%s\033[m\n",slink);
        }
        return;
    } else if (ty==LINE_ATTACHALLLINK) {
        char slink[256];

        if (current_attach_link) {
            (*current_attach_link)(slink,255,NULL,-1,-1,current_attach_link_arg);
            prints("全文链接：\033[4m%s\033[m\n",slink);
        }
        return;
    }
#ifdef ENABLE_LIKE
    else if (ty==LINE_LIKE_NAME && l->like!=0) {
        if (DEFINE(getCurrentUser(), DEF_HIGHCOLOR))
            prints("\033[1;36m有\033[1;33m%d\033[1;36m位用户评价了这篇文章\033[m\n", l->like);
        else
            prints("\033[36m有\033[33m%d\033[36m位用户评价了这篇文章\033[m\n", l->like);
        return;
    }
    else if (ty==LINE_LIKE_INFO || ty==LINE_LIKE_MSG) {
        struct like like;
        
        memcpy(&like, ptr, sizeof(struct like));
#ifndef LIKE_SINGLE_LINE        
        if(ty==LINE_LIKE_INFO) {
#endif
            if(like.score>0)
                prints(" [\033[1;31m+%1d\033[m]", like.score);
            else if(like.score<0)
                prints(" [\033[1;32m%1d\033[m]", like.score);
            else
                prints(" [  ]");
                
            prints(" \033[1;33m%s\033[m", like.user);
#ifndef LIKE_SINGLE_LINE
            char buf[200];
            struct tm *desc=localtime(&like.time);
            sprintf(buf, "  \033[1;36m%04d-%02d-%02d %02d:%02d\033[m", desc->tm_year+1900,desc->tm_mon+1,desc->tm_mday,desc->tm_hour,desc->tm_min);
            prints("%s\n", buf);
        } else {
            prints("   %s\n", like.msg);
        }
#else
            prints(": %s\n", like.msg);
#endif
        return;
    }
#endif
    if (stuffmode) {
        char buf[256];

        memcpy(buf, ptr, (len >= 256) ? 255 : len);
        buf[(len >= 256) ? 255 : len] = 0;
        showstuff(buf);
        prints("\n");
        return;
    }
    if (!strncmp(ptr, "□ 引用", 7) || !strncmp(ptr, "==>", 3)
            || !strncmp(ptr, "【 在", 5) || !strncmp(ptr, "※ 引述", 7)) {
        outns("\033[1;33m", 7);
        outns(ptr, len);
        outns("\033[m\n", 4);
        return;
    }
    if ((ty == LINE_QUOTA)||(ty == LINE_QUOTA_NOCF)) {
        outns("\033[36m", 5);
        outns(ptr, len);
        outns("\033[m\n", 4);
        return;
    }
    outns(ptr, len);
    outns("\n", 1);
}

#ifdef DEF_HIDEEMFLAG
static int mem_show(char *ptr, int size, int origsize, int row, int numlines, char *fn)
#else
static int mem_show(char *ptr, int size, int row, int numlines, char *fn)
#endif
{
    extern int t_lines;
    struct MemMoreLines l;
    int i, curr_line;

#ifdef DEF_HIDEEMFLAG
    init_MemMoreLines(&l, ptr, size, origsize-size);
#else
    init_MemMoreLines(&l, ptr, size);
#endif
    move(row, 0);
    clrtobot();
    prints("\033[m");
    curr_line = l.curr_line;
    for (i = 0; i < t_lines - 1 - row && i < numlines; i++) {
        mem_printline(&l, fn, ptr);
        if (next_MemMoreLines(&l) < 0)
            break;
    }
    return 0;
}

void mem_printbotline(int l1, int l2, int total, int read, int size)
{
    extern int t_lines;

    /* static int n = 0;
     char *(s[4]) = {
      "结束 ← q | ↑ ↓ PgUp PgDn 移动",
      "s 开头 | e 末尾 | b f 向前后翻页",
      "g 跳到指定行 | ? / 向上下搜索字符串",
      "n下一篇 | l上一篇 | R 回文 | E 评价"
     };
     n++;
     if (uinfo.mode == READING)
      n %= 4;
     else
      n %= 3;*/
    move(t_lines - 1, 0);
    resetcolor();
    /* prints
         ("\033[1;44;32m%s (%d%%) 第(%d-%d)行 \033[33m| %s | h 辅助说明\033[m",
          (read >= size) ? "看到末尾啦" : "下面还有喔",
          total ? (100 * l2 / total) : (100 * read / size), l1, l2, s[n]);*/
    if (getCurrentUser() != NULL && DEFINE(getCurrentUser(), DEF_HIGHCOLOR))
        prints("\033[1;44m\033[32m下面还有喔 (%d%%) 第(%d-%d)行\033[33m | g 跳转 |%s / ? 搜索 | s e 开头末尾|", total ? (100 * l2 / total) : (100 * read / size), l1, l2, uinfo.mode==READING?" l n 上下篇 |":"");
    else
        prints("\033[44m\033[32m下面还有喔 (%d%%) 第(%d-%d)行\033[33m | g 跳转 |%s / ? 搜索 | s e 开头末尾|", total ? (100 * l2 / total) : (100 * read / size), l1, l2 ,uinfo.mode==READING?" l n 上下篇 |":"");
    clrtoeol();
    resetcolor();
}

#ifdef DEF_HIDEEMFLAG
int mem_more(char *ptr, int size, int origsize, int quit, char *keystr, char *fn, char *title)
#else
int mem_more(char *ptr, int size, int quit, char *keystr, char *fn, char *title)
#endif
{
    extern int t_lines;
    struct MemMoreLines l;
    static char searchstr[30] = "";
    char buf[256];
    int i, ch = 0, curr_line, last_line, change;
    int oldmode;
    displayflag = 0;
    shownflag = 1;
#ifdef DEF_HIDEEMFLAG
    init_MemMoreLines(&l, ptr, size, origsize-size);
#else
    init_MemMoreLines(&l, ptr, size);
#endif

    prints("\033[m");
    while (1) {
        move(0, 0);
        clear();
        curr_line = l.curr_line;
        for (i = 0;;) {
            if (shownflag) {
                displayflag = 0;
            }
//            move(i, 0);
            mem_printline(&l, fn, ptr);
            i++;
            if (i >= t_lines - 1)
                break;
            if (next_MemMoreLines(&l) < 0)
                break;
        }
        move(i, 0);
        last_line = l.curr_line;
        if (l.total && l.total <= t_lines - 1)
            return 0;
#ifdef ENABLE_LIKE
        if (l.line[last_line % 100] - ptr + l.s[last_line % 100] == size && (ch == KEY_RIGHT || ch == KEY_PGDN || ch == ' ' || ch == Ctrl('f')) && ((l.ty[last_line % 100] != LINE_ATTACHLINK) && (l.ty[last_line % 100] != LINE_LIKE_MSG))) {
#else
        if (l.line[last_line % 100] - ptr + l.s[last_line % 100] == size && (ch == KEY_RIGHT || ch == KEY_PGDN || ch == ' ' || ch == Ctrl('f')) && (l.ty[last_line % 100] != LINE_ATTACHLINK)) {
#endif
            move(t_lines - 1, 0);
            clrtobot();
            return 0;
        }
        change = 0;
        while (change == 0) {
            mem_printbotline(curr_line + 1, last_line + 1, l.total, l.line[last_line % 100] - ptr + l.s[last_line % 100], size);
            ch = igetkey();
            move(t_lines - 1, 0);
            clrtoeol();
            switch (ch) {
                case 'k':
                case KEY_UP:
                    change = -1;
                    break;
                case 'j':
                case KEY_DOWN:
                case 'd':
                case '\n':
                case '\r':
                    change = 1;
                    break;
                case 'b':
                case Ctrl('b'):
                case KEY_PGUP:
                    change = -t_lines + 2;
                    break;
                case ' ':
                case 'f':
                case Ctrl('f'):
                case KEY_PGDN:
                case KEY_RIGHT:
                    if (!l.total)
                        seek_MemMoreLines(&l, last_line + t_lines);
                    change = t_lines - 2;
                    if (l.total && last_line < l.total && curr_line + change + t_lines - 1 > l.total)
                        change = l.total - curr_line - t_lines + 1;
                    break;
                case 's':
                case KEY_HOME:
                    change = -curr_line;
                    break;
                case KEY_END:
                case 'e':
                    if (!l.total) {
                        while (next_MemMoreLines(&l) >= 0);
                        curr_line = l.curr_line;
                    } else
                        curr_line = l.total - 1;
                    change = -t_lines + 2;
                    break;
                case 'g':
                    getdata(t_lines - 1, 0, "跳转到的行号:", buf, 9,
                            1, 0, 1);
                    if (isdigit(buf[0])) {
                        change = atoi(buf) - curr_line;
                    }
                    break;
                case '/':
                case '?':
                    getdata(t_lines - 1, 0,
                            ch ==
                            '/' ? "向下查找字符串:" :
                            "向上查找字符串:", searchstr, 29,
                            1, 0, 0);
                    if (strlen(searchstr) > 0) {
                        int i = curr_line;
                        char *p;
                        while (1) {
                            if (ch == '/')
                                i++;
                            else
                                i--;
                            if (seek_MemMoreLines(&l, i) <
                                    0)
                                break;
#ifdef ENABLE_LIKE
                            if (l.currty == LINE_ATTACHMENT || l.currty == LINE_ATTACHLINK || l.currty == LINE_ATTACHALLLINK || l.currty == LINE_LIKE_NAME || l.currty == LINE_LIKE_INFO || l.currty == LINE_LIKE_MSG)
#else
                            if (l.currty == LINE_ATTACHMENT || l.currty == LINE_ATTACHLINK || l.currty == LINE_ATTACHALLLINK)
#endif
                                continue;
                            /*
                            memcpy(buf, l.curr,
                                   (l.currlen >=
                                    256) ? 255 : l.currlen);
                            buf[(l.currlen >= 256) ? 255 :
                                l.currlen] = 0;
                            if (strcasestr(buf, searchstr)
                                    != NULL) {
                            */
                            /* 由于l.curr实际是文章某行开头起始位置指针，故直接从l.curr开始搜索，当p与之距离小于l.currlen时表明在该行 */
                            if ((p=strcasestr(l.curr, searchstr))!=NULL && (p-l.curr)<l.currlen) {
                                change = i - curr_line;
                                break;
                            }
                        }
                        if (change == 0) {
                            move(t_lines - 1, 0);
                            prints("没有找到呀...");
                            continue;
                        }
                    }
                    break;
                case KEY_LEFT:
                case 'q':
                    return 0;
                case '!':
                    Goodbye();
                    curr_line += t_lines - 1;
                    change = 1 - t_lines;
                    break;
                case 'n':
                    return KEY_DOWN;
                case 'l':
                    return KEY_UP;
                case KEY_REFRESH:
                    curr_line += t_lines - 1;
                    change = 1 - t_lines;
                    break;
                case 'L':
                    if (uinfo.mode != LOOKMSGS) {
                        show_allmsgs();
                        curr_line += t_lines - 1;
                        change = 1 - t_lines;
                    }
                    break;
                case 'M':
                    r_lastmsg();
                    clear();
                    curr_line += t_lines - 1;
                    change = 1 - t_lines;
                    break;
                case 'w': /* 原来是大W 改成小的 统一起见 by pig2532 on 2005-12-1 */
                    if (HAS_PERM(getCurrentUser(), PERM_PAGE)) {
                        oldmode = uinfo.mode;
                        s_msg();
                        modify_user_mode(oldmode);
                        curr_line += t_lines - 1;
                        change = 1 - t_lines;
                    }
                    break;
                case 'u': {
                    oldmode = uinfo.mode;

                    clear();
                    modify_user_mode(QUERY);
                    t_query(NULL);
                    clear();
                    modify_user_mode(oldmode);
                    curr_line += t_lines - 1;
                    change = 1 - t_lines;
                }
                break;
                case 'U':  /* pig2532 2005.12.10 */
                    board_query();
                    curr_line += t_lines - 1;
                    change = 1 - t_lines;
                    break;
                case 'H':
                    show_help("help/morehelp");
                    curr_line += t_lines - 1;
                    change = 1 - t_lines;
                    break;
                case Ctrl('Y'):
                    if (title) {
                        zsend_file(fn, title);
                        curr_line += t_lines - 1;
                        change = 1 - t_lines;
                    }
                    break;
                default:
                    if (keystr != NULL && strchr(keystr, ch) != NULL)
                        return ch;
            }
            if (change < 0 && curr_line == 0) {
                if (quit)
                    return KEY_UP;
                change = 0;
            }
            if (change == 1) {
                if (seek_MemMoreLines(&l, curr_line + t_lines - 1) >= 0) {
                    curr_line++;
                    last_line++;
                    scroll();
                    move(t_lines - 2, 0);
                    mem_printline(&l, fn, ptr);
                    if ((ch == KEY_PGDN || ch == ' ' || ch == Ctrl('f')
                            || ch == KEY_RIGHT || ch == KEY_DOWN || ch == 'j' || ch == '\n')
#ifdef ENABLE_LIKE
                            && (l.ty[last_line % 100] != LINE_ATTACHLINK) && (l.ty[last_line % 100] != LINE_LIKE_MSG)
#else
                            && (l.ty[last_line % 100] != LINE_ATTACHLINK) // 防止一直往下按会不显示全文链接 atppp 20060122
#endif
                            && l.line[last_line % 100] - ptr + l.s[last_line % 100] == size) {
                        move(t_lines - 1, 0);
                        clrtoeol();
                        return 0;
                    }
                } else
                    return 0;
                change = 0;
            }
            if (change == -1) {
                if (seek_MemMoreLines(&l, curr_line - 1) >= 0) {
                    curr_line--;
                    last_line--;
                    rscroll();
                    move(0, 0);
                    mem_printline(&l, fn, ptr);
                }
                change = 0;
            }
            if (!change)
                mem_printbotline(curr_line + 1, last_line + 1, l.total, l.line[last_line % 100] - ptr + l.s[last_line % 100], size);
        }

        seek_MemMoreLines(&l, curr_line + change);
    }
}

int ansimore(char *filename, int promptend)
{
    int ch;

    clear();
    ch = mmap_more(filename, 1, "RrEexp", NULL);
    if (promptend)
        pressanykey();
    if (scrint)
        move(t_lines - 1, 0);
    prints("\033[m\033[m");
    return ch;
}

int ansimore2(filename, promptend, row, numlines)
char *filename;
int promptend;
int row;
int numlines;
{
    int ch;

    if (numlines)
        ch = mmap_show(filename, row, numlines);
    else
        ch = mmap_more(filename, 1, NULL, NULL);
    if (promptend)
        pressanykey();
    return ch;
}

int ansimore_withzmodem(char *filename, int promptend, char *title)
{
    int ch;

    clear();
    ch = mmap_more(filename, 1, "RrEexp", title);
    if (promptend) {
        move(t_lines - 1, 0);
        prints("\x1b[m");
        clrtoeol();
        prints("                                \x1b[5;1;33m按任何键继续 ..\x1b[m");
        if (igetkey()==Ctrl('Y')) {
            zsend_file(filename, title);
        }
        move(t_lines - 1, 0);
        clrtoeol();
    }
    return ch;
}

extern int offsetln;
extern int minln;

int draw_content_more(char *ptr, int size, char *fn, struct fileheader *fh)
{
    struct MemMoreLines l;
    int i, j, curr_line, last_line;
    bool header = true;
    char buf[256];

    displayflag = 0;
    shownflag = 1;
#ifdef DEF_HIDEEMFLAG
    init_MemMoreLines(&l, ptr, size, 0);
#else
    init_MemMoreLines(&l, ptr, size);
#endif

    move(BBS_PAGESIZE / 2+3, 0);
    /*    prints("\033[34m——————————————————预览窗口—————————————————");*/
    /*    move(t_lines/2+1, 0);*/
    if (fh) {
        sprintf(buf, "\033[1;32m\x1b[44m发信人: \033[1;33m%-13.13s\033[1;32m标  题: \033[1;33m%-50.50s\033[1;32m %4.4s\033[m", fh->owner, fh->title, fh->innflag[1] == 'S' ? "[转]" : "");
    } else {
        sprintf(buf, "\x1b[44m%-80.80s\033[m", "");
    }
    outs(buf);
    prints("\n\033[m");
    for (i=BBS_PAGESIZE / 2+4; i<t_lines-1; i++) {
        move(i,0);
        clrtoeol();
    }
    move(BBS_PAGESIZE / 2+4,0);
    curr_line = l.curr_line;
    for (i = 0, j = 0;;) {
        if (shownflag) {
            displayflag = 0;
        }
        if (!header || (!((i == 0) && ((!strncmp(l.curr, "发信人: ", 8) || (!strncmp(l.curr, "寄信人: ", 8))))) &&
                        !((i == 1) && !strncmp(l.curr, "标  题: ", 8)) && !((i == 2) && !strncmp(l.curr, "发信站: ", 8)) && !((i == 3) && (l.currlen == 0||!strncmp(l.curr, "来  源: ", 8)))&&
                        !((i == 4) && (l.currlen==0)))) {
            offsetln = BBS_PAGESIZE / 2+3;
            minln = BBS_PAGESIZE / 2+3;
            mem_printline(&l, fn, ptr);
            offsetln = 0;
            minln = 0;
            j++;
            header = false;
        }
        i++;
        if (j >= BBS_PAGESIZE / 2-1)
            break;
        if (next_MemMoreLines(&l) < 0)
            break;
    }
    last_line = l.curr_line;
//    if (l.total && l.total <= t_lines - t_lines / 2 - 2)
    return 0;
}

int draw_content(char *fn, struct fileheader *fh)
{
    char *ptr;
    off_t size;
    int retv=0;

    BBS_TRY {
        if (safe_mmapfile(fn, O_RDONLY, PROT_READ, MAP_SHARED, &ptr, &size, NULL) == 0)
            BBS_RETURN(-1);
#ifdef DEF_HIDEEMFLAG
        if (DEFINE(getCurrentUser(), DEF_HIDEEMFLAG)) {
            char *p;
            int newsize;
            p = (char *)malloc(size);
            memcpy(p, ptr, size);
            newsize = remove_em_flags(p, size);
            retv = draw_content_more(p, newsize, fn, fh);
            free(p);
        } else {
            retv = draw_content_more(ptr, size, fn, fh);
        }
#else
        retv = draw_content_more(ptr, size, fn, fh);
#endif
    }
    BBS_CATCH {
    }
    BBS_END;
    end_mmapfile((void *) ptr, size, -1);

    return retv;
}

#undef LIKE_SINGLE_LINE
