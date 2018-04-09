/*****
stiger:    2004.2

发表版面使用率到SysTrace

crontab:  2 0 * * * /home/bbs/bin/bonlinelog

******/


#include <time.h>
#include <stdio.h>
#include "bbs.h"

int main(int argc, char **argv)
{
    char path[256];
    char title[256];
    struct stat st;
    time_t now;
    struct tm t;
    int before=0;

    chdir(BBSHOME);

    if (argc > 1) before=atoi(argv[1]);

    now=time(0)-86400*before;
    localtime_r(&now, &t);

    if (init_all()) {
        printf("init data fail\n");
        return -1;
    }

#ifdef NEWSMTH
    sprintf(path, "%s/%d/%d/%d_boarduse.visit.all", BONLINE_LOGDIR, t.tm_year+1900, t.tm_mon+1, t.tm_mday);
#else
    sprintf(path, "%s/%d/%d/%d_boarduse.visit", BONLINE_LOGDIR, t.tm_year+1900, t.tm_mon+1, t.tm_mday);
#endif

    if (stat(path, &st) >= 0) {
        sprintf(title, "%d年%2d月%2d日版面使用数据(次数排序)", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
        post_file(NULL, "", path, "SysTrace", title, 0, 1, getSession());
    }

#ifdef NEWSMTH
    sprintf(path, "%s/%d/%d/%d_boarduse.total.all", BONLINE_LOGDIR, t.tm_year+1900, t.tm_mon+1, t.tm_mday);
#else
    sprintf(path, "%s/%d/%d/%d_boarduse.total", BONLINE_LOGDIR, t.tm_year+1900, t.tm_mon+1, t.tm_mday);
#endif

    if (stat(path, &st) >= 0) {
        sprintf(title, "%d年%2d月%2d日版面使用数据(总时间排序)", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
        post_file(NULL, "", path, "SysTrace", title, 0, 1, getSession());
    }

#ifdef NEWSMTH
    if (t.tm_wday==1) {
        sprintf(path, "%s/%d/%d/%d_boarduse.visit.wall", BONLINE_LOGDIR, t.tm_year+1900, t.tm_mon+1, t.tm_mday);
        if (stat(path, &st) >= 0) {
            sprintf(title, "上周各版面使用数据(次数排序)");
            post_file(NULL, "", path, "SysTrace", title, 0, 1, getSession());
        }
        sprintf(path, "%s/%d/%d/%d_boarduse.total.wall", BONLINE_LOGDIR, t.tm_year+1900, t.tm_mon+1, t.tm_mday);
        if (stat(path, &st) >= 0) {
            sprintf(title, "上周各版面使用数据(总时间排序)");
            post_file(NULL, "", path, "SysTrace", title, 0, 1, getSession());
        }
        sprintf(path, "%s/%d/%d/%d_%d.boardusage.week.all.bak", BONLINE_LOGDIR, t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour);
        f_mv("boardusage.week.all", path);
    }

    if (t.tm_mday==1) {
        sprintf(path, "%s/%d/%d/%d_boarduse.visit.mall", BONLINE_LOGDIR, t.tm_year+1900, t.tm_mon+1, t.tm_mday);
        if (stat(path, &st) >= 0) {
            sprintf(title, "上月各版面使用数据(次数排序)");
            post_file(NULL, "", path, "SysTrace", title, 0, 1, getSession());
        }
        sprintf(path, "%s/%d/%d/%d_boarduse.total.mall", BONLINE_LOGDIR, t.tm_year+1900, t.tm_mon+1, t.tm_mday);
        if (stat(path, &st) >= 0) {
            sprintf(title, "上月各版面使用数据(总时间排序)");
            post_file(NULL, "", path, "SysTrace", title, 0, 1, getSession());
        }
        sprintf(path, "%s/%d/%d/%d_%d.boardusage.month.all.bak", BONLINE_LOGDIR, t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour);
        f_mv("boardusage.month.all", path);
    }
#endif

    return 0;
}
