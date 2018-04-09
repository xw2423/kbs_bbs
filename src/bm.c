
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

    checked global variable
*/

/* XXX: what are these bird words above? should it be cut after the rewrite of deny? - etnlegend */

#include "bbs.h"
#include "read.h"

/*Add by SmallPig*/

extern int ingetdata;
int modify_denytime(time_t denytime, int denyday, int *autofree);

int listdeny(const struct boardheader *bh, int page)
{                               /* Haohmaru.12.18.98.为那些变态得封人超过一屏的版主而写 */
    FILE *fp;
    int x = 0, y = 3, cnt = 0, max = 0, len;
    int i;
    char u_buf[STRLEN * 2], line[STRLEN * 2], *nick;

    clear();

	prints("设定 \033[1;32m%s\033[m版 无法 Post 的名单\n", bh->filename);
	setbfile(genbuf, bh->filename, "deny_users");
    
    move(y, x);
    CreateNameList();
    
    if ((fp = fopen(genbuf, "r")) == NULL) {
        prints("(none)\n");
        return 0;
    }
    for (i = 1; i <= page * 20; i++) {
        if (fgets(genbuf, 2 * STRLEN, fp) == NULL)
            break;
    }
    while (fgets(genbuf, 2 * STRLEN, fp) != NULL) {
        strtok(genbuf, " \n\r\t");
        strcpy(u_buf, genbuf);
        AddNameList(u_buf);
        nick = (char *) strtok(NULL, "\n\r\t");
        if (nick != NULL) {
            while (*nick == ' ')
                nick++;
            if (*nick == '\0')
                nick = NULL;
        }
        if (nick == NULL) {
            strcpy(line, u_buf);
        } else {
            if (cnt < 20)
                sprintf(line, "%-12s %s", u_buf, nick);
        }
        if ((len = strlen(line)) > max)
            max = len;
        /*        if( x + len > 90 )
                    line[ 90 - x ] = '\0';*-P-*/
        if (x + len > 79)
            line[79] = '\0';
        if (cnt < 20)           /*haohmaru.12.19.98 */
            prints("%s", line);
        cnt++;
        if ((++y) >= t_lines - 1) {
            y = 3;
            x += max + 2;
            max = 0;
            /*
             * if( x > 90 )  break;
             */
        }
        move(y, x);
    }
    fclose(fp);
    if (cnt == 0)
        prints("(none)\n");
    return cnt;
}

/* 选择封禁理由new */
struct _simple_select_arg {
    struct _select_item *items;
    int flag;
};
static int denyreason_select(struct _select_def *conf)
{
    int pos = conf->pos;
    if (pos == conf->item_count)
        return SHOW_QUIT;
    return SHOW_SELECT;
}
static int denyreason_show(struct _select_def *conf,int i)
{
    struct _simple_select_arg *arg=(struct _simple_select_arg*)conf->arg;
    outs((char*)((arg->items[i-1]).data));
    return SHOW_CONTINUE;
}
static int denyreason_key(struct _select_def *conf,int key)
{
    struct _simple_select_arg *arg=(struct _simple_select_arg*)conf->arg;
    int i;
    if (key==KEY_ESC)
        return SHOW_QUIT;
    for (i=0; i<conf->item_count; i++)
        if (toupper(key)==toupper(arg->items[i].hotkey)) {
            conf->new_pos=i+1;
            return SHOW_SELCHANGE;
        }
    return SHOW_CONTINUE;
}
int select_deny_reason(char reason[][STRLEN], char *denymsg, int count)
{
    struct _select_item sel[count+3];
    struct _select_def conf;
    struct _simple_select_arg arg;
    POINT pts[count+2];
    char menustr[count+2][STRLEN];
    int i, ret, pos;

    for (i=0;i<count+2;i++) {
        sel[i].x = (i<10) ? 2 : 40;
        sel[i].y = (i<10) ? 4 + i : i - 6;
        sel[i].hotkey = (i<9) ? '1' + i : 'A' + i - 9;
        sel[i].type = SIT_SELECT;
        sel[i].data = menustr[i];
        if (i==count)
            sprintf(menustr[i], "\033[33m[%c] %s\033[m", sel[i].hotkey, "手动输入封禁理由");
        else if (i==count+1)
            sprintf(menustr[i], "\033[31m[%c] %s\033[m", sel[i].hotkey, "放弃此次封禁操作");
        else
            sprintf(menustr[i], "[%c] %s", sel[i].hotkey, reason[i]);
        pts[i].x = sel[i].x;
        pts[i].y = sel[i].y;
    }
    sel[i].x = -1;
    sel[i].y = -1;
    sel[i].hotkey = -1;
    sel[i].type = 0;
    sel[i].data = NULL;

    arg.items = sel;
    arg.flag = SIF_SINGLE;
    bzero(&conf, sizeof(struct _select_def));
    conf.item_count = count + 2;
    conf.item_per_page = MAXDENYREASON + 2;
    conf.flag = LF_LOOP | LF_MULTIPAGE;
    conf.prompt = "◆";
    conf.item_pos = pts;
    conf.arg = &arg;
    conf.pos = 0;
    conf.show_data = denyreason_show;
    conf.key_command = denyreason_key;
    conf.on_select = denyreason_select;

    pos = 0;
    move(3, 0);
    clrtoeol();
    prints("\033[33m请选择封禁理由\033[m");
    while (1) {
        move(4, 0);
        clrtobot();
        conf.pos = pos;
        conf.flag=LF_LOOP;
        ret = list_select_loop(&conf);
        pos = conf.pos;
        if (ret==SHOW_SELECT) {
            if (pos==conf.item_count-1) {
                getdata(arg.items[pos-1].y, arg.items[pos-1].x, "输入理由: ", denymsg, 30, DOECHO, NULL, true);
                if (denymsg[0]=='\0')
                    continue;
                move(arg.items[pos-1].y, arg.items[pos-1].x);
                clrtoeol();
                sprintf(arg.items[pos-1].data, "[%c] %s", arg.items[pos-1].hotkey, denymsg);
            } else
                sprintf(denymsg, "%s", reason[pos-1]);
            move(arg.items[pos-1].y, arg.items[pos-1].x);
            prints("\033[32m%s\033[m", (char *)arg.items[pos-1].data);
            return 1;
        } else 
            return 0;
    }
}

int set_denymsg(const char *boardname, char *denymsg)
{
    int count;
    char reason[MAXDENYREASON][STRLEN];

    count = get_deny_reason(boardname, reason, MAXCUSTOMREASON);
    count += get_deny_reason(NULL, &(reason[count]), MAXDENYREASON-count);
    return select_deny_reason(reason, denymsg, count);
}

#ifdef RECORD_DENY_FILE
/* 记录封禁原文，jiangjun， 20120321 */
int addtodeny(const struct boardheader *bh, struct userec *user, const struct fileheader *fh, int filtermode)
#else
int addtodeny(const struct boardheader *bh, struct userec *user)
#endif
{                               /* 添加 禁止POST用户 */
    char /*buf2[50], */strtosave[256], date[STRLEN] = "0";
    //int maxdeny;
	
    /*
     * Haohmaru.99.4.1.auto notify
     */
    time_t now;
    //char buffer[STRLEN];
    //FILE *fn;
    //char filename[STRLEN];
    int autofree = 0;
    //char filebuf[STRLEN];
    char denymsg[STRLEN];
    int denyday;
    //int reasonfile;
    int gdataret;

    now = time(0);
    strncpy(date, ctime(&now) + 4, 7);
    setbfile(genbuf, bh->filename, "deny_users");
    if (seek_in_file(genbuf, user->userid, NULL) || !strcmp(bh->filename, "denypost"))
        return -1;
    /*
    if (HAS_PERM(getCurrentUser(), PERM_SYSOP) || HAS_PERM(getCurrentUser(), PERM_OBOARDS))
        maxdeny = 70;
    else
        maxdeny = 14;
    */

    *denymsg = 0;
    move(2, 0);
    prints("增加 \033[31m%s\033[m 至 \033[33m%s\033[m 版封禁名单", user->userid, bh->filename);
    /* 选择封禁理由new */
    if (set_denymsg(bh->filename, denymsg)==0)
        return 0;
    /*
    {
        int count;
        char reason[MAXDENYREASON][STRLEN];

        count = get_deny_reason(bh->filename, reason, MAXCUSTOMREASON);
        count += get_deny_reason(NULL, &(reason[count]), MAXDENYREASON-count);
        if ((select_deny_reason(reason, denymsg, count))==0)
            return 0;
    }*/
#if 0
    if ((reasonfile = open("etc/deny_reason", O_RDONLY)) != -1) {
        int reason = -1;
        int maxreason;
        char *file_buf;
        char *denymsglist[50];
        struct stat st;

        move(3, 0);
        clrtobot();
        if (fstat(reasonfile, &st) == 0) {
            int i;

            file_buf = (char *) malloc(st.st_size);
            read(reasonfile, file_buf, st.st_size);
            maxreason = 1;
            denymsglist[0] = file_buf;
            prints("%s", "封禁理由列表.\n");
            for (i = 1; i < st.st_size; i++) {
                if (file_buf[i] == '\n') {
                    file_buf[i] = 0;
                    prints("%d.%s\n", maxreason, denymsglist[maxreason - 1]);
                    if (i == st.st_size - 1)
                        break;
                    if (file_buf[i + 1] == '\r') {
                        if (i + 1 == st.st_size - 1)
                            break;
                        denymsglist[maxreason] = &file_buf[i + 2];
                        maxreason++;
                        i += 2;
                    } else {
                        denymsglist[maxreason] = &file_buf[i + 1];
                        maxreason++;
                        i++;
                    }
                }
            }
            prints("%s", "0.手动输入封禁理由");
            while (1) {
                gdataret = getdata(2, 0, "请从列表选择封禁理由(0为手工输入,*退出):", denymsg, 2, DOECHO, NULL, true);
                if (gdataret == -1 || denymsg[0] == '*') {
                    free(file_buf);
                    close(reasonfile);
                    return 0;
                }
                if (isdigit(denymsg[0])) {
                    reason = atoi(denymsg);
                    if (reason == 0) {
                        denymsg[0] = 0;
                        move(2, 0);
                        clrtobot();
                        break;
                    }
                    if (reason <= maxreason) {
                        strncpy(denymsg, denymsglist[reason - 1], STRLEN - 1);
                        denymsg[STRLEN - 1] = 0;
                        move(2, 0);
                        clrtobot();
                        prints("封禁理由: %s\n", denymsg);
                        break;
                    }
                }
                move(3, 0);
                clrtoeol();
                prints("%s", "输入错误!");
            }
            free(file_buf);
        }
        close(reasonfile);
    }
#endif

    gdataret = 0;
    while (gdataret != -1 && 0 == strlen(denymsg)) {
        gdataret = getdata(2, 0, "输入说明(按*取消): ", denymsg, 30, DOECHO, NULL, true);
    }
    if (gdataret == -1 || denymsg[0] == '*')
        return 0;
#if 0
#ifdef MANUAL_DENY
    autofree = askyn("该封禁是否自动解封？(选 \033[1;31mY\033[m 表示进行自动解封)", true);
#else
    autofree = true;
#endif
#endif
    autofree = 1;
    if ((denyday=modify_denytime(0, 0, &autofree))<0)
        return 0;
#if 0
    sprintf(filebuf, "输入天数(最长%d天)(按*取消封禁)", maxdeny);
    denyday = 0;
    while (!denyday) {
        gdataret = getdata(13, 0, filebuf, buf2, 4, DOECHO, NULL, true);
        if (gdataret == -1 || buf2[0] == '*')return 0;
        if ((buf2[0] < '0') || (buf2[0] > '9'))
            continue;           /*goto MUST1; */
        denyday = atoi(buf2);
        if ((denyday < 0) || (denyday > maxdeny))
            denyday = 0;        /*goto MUST1; */
        else if (!(HAS_PERM(getCurrentUser(), PERM_SYSOP) || HAS_PERM(getCurrentUser(), PERM_OBOARDS)) && !denyday)
            denyday = 0;        /*goto MUST1; */
        else if ((HAS_PERM(getCurrentUser(), PERM_SYSOP) || HAS_PERM(getCurrentUser(), PERM_OBOARDS)) && !denyday && !autofree)
            break;
    }
#endif
#ifdef ZIXIA
    int ndenypic,dpcount;
    clear();
    dpcount=CountDenyPic(DENYPIC);
    sprintf(filebuf, "选择封禁附图(1-%d)(0为随机选择,V为看图片)[0]:",dpcount);
    ndenypic=-1;
    while (ndenypic>dpcount || ndenypic<0) {
        getdata(0,0,filebuf,buf2,4, DOECHO, NULL, true);
        if (buf2[0]=='v' ||buf2[0]=='V') {
            ansimore(DENYPIC, 0);
            continue;
        }
        if (buf2[0]=='\0') {
            ndenypic=0;
            break;
        }
        if ((buf2[0] < '0') || (buf2[0] > '9'))
            continue;
        ndenypic = atoi(buf2);
    }
#endif
    if (denyday && autofree) {
        struct tm *tmtime;
        time_t undenytime = now + denyday * 24 * 60 * 60;

        tmtime = localtime(&undenytime);

        sprintf(strtosave, "%-12.12s %-30.30s%-12.12s %2d月%2d日解\x1b[%lum%lum", user->userid, denymsg, getCurrentUser()->userid, tmtime->tm_mon + 1, tmtime->tm_mday, undenytime, now);   /*Haohmaru 98,09,25,显示是谁什么时候封的 */
    } else {
        struct tm *tmtime;
        time_t undenytime = now + denyday * 24 * 60 * 60;

        tmtime = localtime(&undenytime);
        sprintf(strtosave, "%-12.12s %-30.30s%-12.12s %2d月%2d日后\x1b[%lum%lum", user->userid, denymsg, getCurrentUser()->userid, tmtime->tm_mon + 1, tmtime->tm_mday, undenytime, now);
    }

    if (addtofile(genbuf, strtosave) == 1) {
#if 0
        struct userec *lookupuser, *saveptr;
        int my_flag = 0;        /* Bigman. 2001.2.19 */
        struct userec saveuser;

        /*
         * Haohmaru.4.1.自动发信通知并发文章于版上
         */
        gettmpfilename(filename, "deny");
        //sprintf(filename, "tmp/%s.deny", getCurrentUser()->userid);
        fn = fopen(filename, "w+");
        memcpy(&saveuser, getCurrentUser(), sizeof(struct userec));
        saveptr = getCurrentUser();
        getCurrentUser() = &saveuser;
        sprintf(buffer, "%s被取消在%s版的发文权限", uident, bh->filename);
#ifdef MEMBER_MANAGER		
		int is_core_member=0;
#endif
		
        if ((HAS_PERM(getCurrentUser(), PERM_SYSOP) || HAS_PERM(getCurrentUser(), PERM_OBOARDS)) && !chk_BM_instr(currBM, getCurrentUser()->userid)) {
            strcpy(getCurrentUser()->userid, "SYSOP");
            strcpy(getCurrentUser()->username, NAME_SYSOP);
            my_flag = 0;
            fprintf(fn, "寄信人: SYSOP (%s) \n", NAME_SYSOP);
            fprintf(fn, "标  题: %s\n", buffer);
            fprintf(fn, "发信站: %s (%24.24s)\n", BBS_FULL_NAME, ctime(&now));
            fprintf(fn, "来  源: %s\n", NAME_BBS_ENGLISH);
            fprintf(fn, "\n");
            fprintf(fn, "由于您在 \x1b[4m%s\x1b[m 版 \x1b[4m%s\x1b[m，我很遗憾地通知您， \n", bh->filename, denymsg);
            if (denyday)
                fprintf(fn, DENY_DESC_AUTOFREE " \x1b[4m%d\x1b[m 天", denyday);
            else
                fprintf(fn, DENY_DESC_NOAUTOFREE);
            if (!autofree)
                fprintf(fn, "，到期后请回复\n此信申请恢复权限。\n");
            fprintf(fn, "\n");
#ifdef ZIXIA
            ndenypic=GetDenyPic(fn, DENYPIC, ndenypic, dpcount);
#endif
            fprintf(fn, "                            %s" NAME_SYSOP_GROUP DENY_NAME_SYSOP "：\x1b[4m%s\x1b[m\n", NAME_BBS_CHINESE, saveptr->userid);
            fprintf(fn, "                              %s\n", ctime(&now));
            /*strcpy(getCurrentUser()->realname, NAME_SYSOP);*/
        } else {
#ifdef MEMBER_MANAGER		
			if (!HAS_PERM(getCurrentUser(), PERM_SYSOP)&&!chk_currBM(currBM, getCurrentUser()))
				is_core_member=1;
#endif
				
            my_flag = 1;
            fprintf(fn, "寄信人: %s \n", getCurrentUser()->userid);
            fprintf(fn, "标  题: %s\n", buffer);
            fprintf(fn, "发信站: %s (%24.24s)\n", BBS_FULL_NAME, ctime(&now));
            fprintf(fn, "来  源: %s \n", SHOW_USERIP(getCurrentUser(), getSession()->fromhost));
            fprintf(fn, "\n");
            fprintf(fn, "由于您在 \x1b[4m%s\x1b[m 版 \x1b[4m%s\x1b[m，我很遗憾地通知您， \n", bh->filename, denymsg);
            if (denyday)
                fprintf(fn, DENY_DESC_AUTOFREE " \x1b[4m%d\x1b[m 天", denyday);
            else
                fprintf(fn, DENY_DESC_NOAUTOFREE);
            if (!autofree)
                fprintf(fn, "，到期后请回复\n此信申请恢复权限。\n");
            fprintf(fn, "\n");
#ifdef ZIXIA
            ndenypic=GetDenyPic(fn, DENYPIC, ndenypic, dpcount);
#endif
#ifdef MEMBER_MANAGER
			if (is_core_member)
				fprintf(fn, "                              " NAME_CORE_MEMBER ":\x1b[4m%s\x1b[m\n", saveptr->userid);
			else
#endif			
				fprintf(fn, "                              " NAME_BM ":\x1b[4m%s\x1b[m\n", saveptr->userid);
            fprintf(fn, "                              %s\n", ctime(&now));
        }
        fclose(fn);
#ifdef NEWSMTH
		//if (is_core_member)
		//	mail_file(getCurrentUser()->userid, filename, uident, buffer, 0, NULL);
		//else	
			mail_file(DELIVER, filename, uident, buffer, 0, NULL);
#else
        mail_file(getCurrentUser()->userid, filename, uident, buffer, 0, NULL);
#endif
        fn = fopen(filename, "w+");
        fprintf(fn, "由于 \x1b[4m%s\x1b[m 在 \x1b[4m%s\x1b[m 版的 \x1b[4m%s\x1b[m 行为，\n", uident, bh->filename, denymsg);
        if (denyday)
            fprintf(fn, DENY_BOARD_AUTOFREE " \x1b[4m%d\x1b[m 天。\n", denyday);
        else
            fprintf(fn, DENY_BOARD_NOAUTOFREE "\n");
#ifdef ZIXIA
        GetDenyPic(fn, DENYPIC, ndenypic, dpcount);
#endif
        if (my_flag == 0) {
            fprintf(fn, "                            %s" NAME_SYSOP_GROUP DENY_NAME_SYSOP "：\x1b[4m%s\x1b[m\n", NAME_BBS_CHINESE, saveptr->userid);
        } else {
#ifdef MEMBER_MANAGER
			if (is_core_member)
			fprintf(fn, "                              " NAME_CORE_MEMBER ":\x1b[4m%s\x1b[m\n", saveptr->userid);
			else
#endif		
            fprintf(fn, "                              " NAME_BM ":\x1b[4m%s\x1b[m\n", saveptr->userid);
        }
        fprintf(fn, "                              %s\n", ctime(&now));
        fclose(fn);
#ifdef NEWSMTH
		//if (is_core_member)
		//post_file(getCurrentUser(), "", filename, bh->filename, buffer, 0, 2, getSession());
		//else
        post_file(getCurrentUser(), "", filename, bh->filename, buffer, 0, 1, getSession());
#else
        post_file(getCurrentUser(), "", filename, bh->filename, buffer, 0, 2, getSession());
#endif
        /*
         * unlink(filename);
         */
        getCurrentUser() = saveptr;

        //sprintf(buffer, "%s 被 %s 封禁本版POST权", uident, getCurrentUser()->userid);
        getuser(uident, &lookupuser);

        if (PERM_BOARDS & lookupuser->userlevel)
            sprintf(buffer, "%s 封某版" NAME_BM " %s 在 %s", getCurrentUser()->userid, uident, bh->filename);
        else
            sprintf(buffer, "%s 封 %s 在 %s", getCurrentUser()->userid, uident, bh->filename);
        post_file(getCurrentUser(), "", filename, "denypost", buffer, 0, -1, getSession());
        unlink(filename);
#endif
#ifdef NEW_BOARD_ACCESS
        if (set_nba_status(user, currboardent, NBA_MODE_DENY, 1)<0) {
            move(13, 0);
            prints("\033[31m发生错误, 请报告至sysop版面 <Enter>");
            WAIT_RETURN;
            return 0;
        }
#endif
        /* 使用封禁模版功能 */
#ifdef RECORD_DENY_FILE
        if (deny_announce(user->userid,bh,denymsg,denyday,getCurrentUser(),time(0),0,fh, filtermode)<0 ||
            deny_mailuser(user->userid,bh,denymsg,denyday,getCurrentUser(),time(0),0,autofree)<0) {
            move(13, 0);
            prints("\033[31m发生错误, 请报告至sysop版面 <Enter>");
            WAIT_RETURN;
        }
#else
        if (deny_announce(user->userid,bh,denymsg,denyday,getCurrentUser(),time(0),0)<0 ||
            deny_mailuser(user->userid,bh,denymsg,denyday,getCurrentUser(),time(0),0,autofree)<0) {
            move(13, 0);
            prints("\033[31m发生错误, 请报告至sysop版面 <Enter>");
            WAIT_RETURN;
        }
#endif
        bmlog(getCurrentUser()->userid, bh->filename, 10, 1);
    }
    return 0;
}

/* 修改已封禁id的封禁时间 */
int modify_denytime(time_t denytime, int denyday, int *autofree)
{
    struct tm *tm_time;
    int firstdeny, maxdeny, mindeny, newday, ch, start;
    time_t settime, now;

    now = time(0);
    tm_time = localtime(&now);

    if (!denytime) {    /* 首次封禁 */
        firstdeny = 1;
        denytime = now;
        denyday = 1;
        mindeny = 1;
    } else {            /* 修改封禁 */
        firstdeny = 0;
        mindeny = (now+tm_time->tm_gmtoff)/86400 - (denytime+tm_time->tm_gmtoff)/86400 + 1;
    }

    if (HAS_PERM(getCurrentUser(), PERM_SYSOP) || HAS_PERM(getCurrentUser(), PERM_OBOARDS))
        maxdeny = 70;
    else
        maxdeny = 14;

    newday = denyday;

    move(16, 0);
    prints("操作提示: \033[33m上/下键\033[m调整或直接输入天数，\033[33m回车\033[m确认，\033[33mESC\033[m放弃\n");
    if (!firstdeny) /* 表示是修改封禁时间 */
        prints("特别提示: \033[31m确定修改后将按照首次封禁时间计算封禁天数！\033[m");
    start = 0;
    while (1) {
        settime = denytime + newday * 86400;
        tm_time = localtime(&settime);
        move (15, 0);
        prints("\033[33m设定封禁天数: \033[4;32m %d \033[0;32m天\033[0;33m\t解封日期: \033[32m%d年%2d月%2d日\033[m",
                newday, tm_time->tm_year + 1900, tm_time->tm_mon + 1, tm_time->tm_mday);
        move(15, 16);
        ch = igetkey();
        if(ch==KEY_UP) {
            if (newday < maxdeny)
                newday++;
        } else if (ch==KEY_DOWN) {
            if (newday > mindeny) /* 至少需要mindeny天 */
                newday--;
        } else if (ch>='0' && ch<='9') { /* 直接输入日期，可输入三位数字 */
            if (!start)
                newday = (ch-'0')>mindeny?(ch-'0'):mindeny;
            else {
                int day = ch - '0';
                /* 先处理上次结果 */
                if (newday>100)
                    newday = day>mindeny?day:mindeny;
                else
                    newday = 10*newday + ch - '0';
                /* 再处理此次结果 */
                if (newday>maxdeny)
                    newday = day;
                if (newday<mindeny)
                    newday = mindeny;
            }
            start = 1;
        } else if (ch=='\n' || ch=='\r') {
#ifdef MANUAL_DENY
            char ans[2];
            getdata(15, 52, "是否自动解封? [Y]", ans, 2, DOECHO, NULL, true);
            if (toupper(ans[0])=='N')
                *autofree = 0;
            else
                *autofree = 1;
#endif
            break;
        } else if (ch==KEY_ESC) {
            if (firstdeny)
                return -1;
            else
                return denyday;
        } else
            continue;
    }
    return newday;
}

/* 修改已封禁的id */
int modify_user_deny(const struct boardheader *bh, char *uident, char *denystr)
{
#define MOD_DENY_REASON 0x001
#define MOD_DENY_TIME   0x002
#ifdef MANUAL_DENY
#define MOD_DENY_TYPE   0x004
#endif
    time_t denytime, undenytime;
    char denymsg[STRLEN], newmsg[STRLEN], operator[IDLEN+1];
    int ch, i, denyday, newday;
    struct tm deny_time, undeny_time;
    unsigned int flag;
    int autofree, newfree;

    flag = 0;
    ch = 0;
    i = 1;
    get_denied_reason(denystr, denymsg);
    get_denied_operator(denystr, operator);
#ifdef MANUAL_DENY
    autofree = get_denied_freetype(denystr);
#else
    autofree = 1;
#endif
    denytime = get_denied_time(denystr);
    undenytime = get_undeny_time(denystr);

    strcpy(newmsg, denymsg);
    newfree = autofree;
    localtime_r(&denytime, &deny_time);
    localtime_r(&undenytime, &undeny_time);
    denyday = (undenytime+undeny_time.tm_gmtoff)/86400 - (denytime+deny_time.tm_gmtoff)/86400;
    newday = denyday;
    while(1) {
        localtime_r(&undenytime, &undeny_time);
        move(3, 0);
        clrtobot();
        prints("该用户的封禁情况如下: \n\n");
        prints("  \033[33m被封禁ID:\033[m %s\n", uident);
        prints("  \033[33m封禁理由:\033[m %s%s\033[m\n", (flag&MOD_DENY_REASON)?"\033[31m":"", newmsg);
        prints("  \033[33m封禁日期:\033[m %d年%2d月%2d日\033[m\n", deny_time.tm_year + 1900, deny_time.tm_mon + 1, deny_time.tm_mday);
        prints("  \033[33m封禁天数:\033[m %s%d天\033[m\n", newday!=denyday?"\033[31m":"", newday);
#ifdef MANUAL_DENY
        prints("  \033[33m解封日期:\033[m %s%d年%2d月%2d日\033[m  %s[%s]\033[m\n", (flag&MOD_DENY_TIME)?"\033[31m":"",
                undeny_time.tm_year + 1900, undeny_time.tm_mon + 1, undeny_time.tm_mday,
                (flag&MOD_DENY_TYPE)?"\033[31m":"", newfree?"自动解封":"手动解封");
#else
        prints("  \033[33m解封日期:\033[m %s%d年%2d月%2d日\033[m\033[m\n", (flag&MOD_DENY_TIME)?"\033[31m":"",
                undeny_time.tm_year + 1900, undeny_time.tm_mon + 1, undeny_time.tm_mday);
#endif
        prints("  \033[33m操作者ID:\033[m %s%s\033[m\n", (flag&&strcmp(getCurrentUser()->userid, operator))?"\033[31m":"",
                flag?getCurrentUser()->userid:operator);
        while (1) {
            move(12, 0);
            prints("\033[33m选择操作: \033[m%s[1.删除封禁用户]%s[2.修改封禁理由]%s[3.调整解封日期]%s[4.保存并退出]\033[m",
                    i==1?"\033[32m":"\033[m", i==2?"\033[32m":"\033[m", i==3?"\033[32m":"\033[m", i==4?"\033[32m":"\033[m");
            ch = igetkey();
            if (ch>='1' && ch<='4') {
                i = ch - '0';
            } else if (ch==KEY_RIGHT || ch==KEY_TAB) {
                i++;
                if (i>4)
                    i=1;
            } else if (ch==KEY_LEFT) {
                i--;
                if (i<1)
                    i=4;
            } else if (ch=='\n'||ch=='\r') {
                if (i==1) { /* 删除封禁ID */
                    move(13, 0);
                    prints("%s确定解封？[N]", (undenytime>time(0))?"该用户封禁时限未到，":"");
                    ch = igetkey();
                    if (toupper(ch)=='Y') {
                        if (deldeny(getCurrentUser(), (char *)bh->filename, (char *)uident, 0, 1, getSession())<0) {
                            move(15, 0);
                            prints("\033[31m解封时发生错误 <Enter>");
                            WAIT_RETURN;
                        }
                        return 0;
                    }
                } else if (i==2) { /* 修改封禁理由 */
                    set_denymsg(bh->filename, newmsg);
                    if (strcmp(denymsg, newmsg))
                        flag |= MOD_DENY_REASON;
                    else
                        flag &= ~MOD_DENY_REASON;
                } else if (i==3) { /* 调整封禁天数 */
                    newday = modify_denytime(denytime, newday, &newfree);
                    if (denyday!=newday)
                        flag |= MOD_DENY_TIME;
                    else
                        flag &= ~MOD_DENY_TIME;
                    /* 重新设定解封时间戳 */
                    undenytime = denytime + newday * 86400;
#ifdef MANUAL_DENY
                    if (newfree!=autofree)
                        flag |= MOD_DENY_TYPE;
                    else
                        flag &= ~MOD_DENY_TYPE;
#endif
                } else { /* 保存并退出 */
                    if (flag) { /* 如果发生修改 */
                        char savestr[STRLEN*2], filename[STRLEN], ans[2];
                        getdata(13, 0, "确定修改? [Y]", ans, 2, DOECHO, NULL, true);
                        if (toupper(ans[0])=='N') {
                            prints("\033[33m取消...<Enter>\033[m");
                            WAIT_RETURN;
                            return 0;
                        }
                        if (newfree)
                            sprintf(savestr, "%-12.12s %-30.30s%-12.12s %2d月%2d日解\x1b[%lum%lum",
                                    uident, newmsg, getCurrentUser()->userid, undeny_time.tm_mon+1, undeny_time.tm_mday, undenytime, denytime);
                        else
                            sprintf(savestr, "%-12.12s %-30.30s%-12.12s %2d月%2d日后\x1b[%lum%lum",
                                    uident, newmsg, getCurrentUser()->userid, undeny_time.tm_mon+1, undeny_time.tm_mday, undenytime, denytime);
                        setbfile(filename, bh->filename, "deny_users");
                        if (replace_from_file_by_id(filename, uident, savestr)<0) {
                            move(13, 0);
                            prints("\033[31m修改封禁时发生错误 <Enter>");
                            WAIT_RETURN;
                        }
#ifdef RECORD_DENY_FILE
                        if (deny_announce(uident,bh,newmsg,newday,getCurrentUser(),time(0),flag&MOD_DENY_TIME?2:1,NULL,0)<0 ||
                            deny_mailuser(uident,bh,newmsg,newday,getCurrentUser(),time(0),1,newfree)<0) {
                            move(13, 0);
                            prints("\033[31m发生错误, 请报告至sysop版面 <Enter>");
                        }
#else
                        if (deny_announce(uident,bh,newmsg,newday,getCurrentUser(),time(0),flag&MOD_DENY_TIME?2:1)<0 ||
                            deny_mailuser(uident,bh,newmsg,newday,getCurrentUser(),time(0),1,newfree)<0) {
                            move(13, 0);
                            prints("\033[31m发生错误, 请报告至sysop版面 <Enter>");
                        }
#endif
                    }
                    return 0;
                }
                break;
            } else if (ch==KEY_ESC) { /* 不保存退出 */
                return 0;
            }
        }
    }
#undef MOD_DENY_REASON
#undef MOD_DENY_TIME
#ifdef MANUAL_DENY
#undef MOD_DENY_TYPE
#endif
    return 0;
}

int deny_user(struct _select_def* conf,struct fileheader *fileinfo,void* extraarg)
{                               /* 禁止POST用户名单 维护主函数 */
    char uident[STRLEN];
    int page = 0;
    char ans[10];
    int count;
	
	const struct boardheader *bh;
	
    /*
     * Haohmaru.99.4.1.auto notify
     */
    time_t now;
    int id;
    FILE *fp;
    int find;                   /*Haohmaru.99.12.09 */
    char *lptr;
    time_t ldenytime;
    int filtermode;

    /*   static page=0; *//*
     * * Haohmaru.12.18
     */
    now = time(0);
    if (!HAS_PERM(getCurrentUser(), PERM_SYSOP))
        if (
#ifdef MEMBER_MANAGER
			!check_board_member_manager(&currmember, currboard, BMP_DENY)
#else		
			!chk_currBM(currBM, getCurrentUser())
#endif
		) {
            return DONOTHING;
        }

#ifdef FILTER
#ifdef NEWSMTH
    if (!strcmp(currboard->filename, FILTER_BOARD)||currboard->flag&BOARD_CENSOR_FILTER) {
#else /* NEWSMTH */
    if (!strcmp(currboard->filename, FILTER_BOARD)) {
#endif /* NEWSMTH */
        if (fileinfo==NULL || fileinfo->o_bid <= 0) {
            return DONOTHING;
        }
        if (!(bh=getboard(fileinfo->o_bid)) || 
#ifdef MEMBER_MANAGER
			!check_board_member_manager(&currmember, bh, BMP_DENY)
#else		
			!chk_currBM(bh->BM, getCurrentUser())
#endif			
		) {
            return DONOTHING;
        }
#ifdef NEWSMTH
        filtermode = (strcmp(currboard->filename, FILTER_BOARD)==0)?1:2;
#else
        filtermode = 1;
#endif
    } else {
        bh=currboard;
        filtermode = 0;
    }
#else /* FILTER */
	bh=currboard;
#endif /* FILTER */			
		
    while (1) {
        char querybuf[0xff];
        char LtNing[24];
        if (fileinfo == NULL) LtNing[0] = '\0';
        else sprintf(LtNing, "(O)增加%s ", fileinfo->owner);

Here:
        clear();
		
		count = listdeny(bh, 0);
		if (count > 0 && count < 20)    /*Haohmaru.12.18,看下一屏 */
            snprintf(querybuf, 0xff, "%s(A)增加 (D)删除 (M)调整 or (E)离开 [E]: ", LtNing);
        else if (count >= 20)
            snprintf(querybuf, 0xff, "%s(A)增加 (D)删除 (M)调整 (N)后面第N屏 or (E)离开 [E]: ", LtNing);
        else
            snprintf(querybuf, 0xff, "%s(A)增加 or (E)离开 [E]: ", LtNing);

        getdata(1, 0, querybuf, ans, 7, DOECHO, NULL, true);
        *ans = (char) toupper((int) *ans);

        if (*ans == 'A' || (*ans == 'O' && fileinfo != NULL)) {
            struct userec *denyuser;
#ifdef RECORD_DENY_FILE
            int denyfile=0;
#endif

            move(1, 0);
            if (*ans == 'A')
                usercomplete("增加无法 POST 的使用者: ", uident);
            else
#ifdef RECORD_DENY_FILE
            {
                strncpy(uident, fileinfo->owner, STRLEN - 4);
                /* O方式增加封禁名单，记录封禁原文 */
                denyfile=1;
            }
#else
                strncpy(uident, fileinfo->owner, STRLEN - 4);
#endif
            /*
             * Haohmaru.99.4.1,增加被封ID正确性检查
             */
            if (!(id = getuser(uident, &denyuser))) {   /* change getuser -> searchuser , by dong, 1999.10.26 */
                move(3, 0);
                prints("非法 ID");
                clrtoeol();
                pressreturn();
                goto Here;
            }
            strncpy(uident, denyuser->userid, IDLEN);
            uident[IDLEN] = 0;

            /*
             * windinsn.04.5.17,不准封禁 guest 和 SYSOP
             */
            if (!strcasecmp(uident,"guest")
                    || !strcasecmp(uident,"SYSOP")
#ifdef ZIXIA
                    || !strcasecmp(uident,"SuperMM")
#endif
               ) {
                move(3, 0);
                prints("不能封禁 %s", uident);
                clrtoeol();
                pressreturn();
                goto Here;
            }
            /* fancyrabbit Dec 4 2007, 不准封进不去的 ... */
            if (!check_read_perm(denyuser, bh)
#ifdef COMMEND_ARTICLE
                    && strcmp(bh -> filename, COMMEND_ARTICLE)
#endif
               ) {
                move(3, 0);
                prints("%s 没有本版的读取权限, 不能封禁", uident);
                clrtoeol();
                pressreturn();
                goto Here; /* I hate goto too! */
            }

            if (*uident != '\0') {
#ifdef RECORD_DENY_FILE
                addtodeny(bh, denyuser, (denyfile)?fileinfo:NULL, filtermode);
#else
                addtodeny(bh, denyuser);
#endif
            }
        } else if ((*ans == 'M') && count) {
            int len;
            move(1, 0);
            namecomplete("修改本版无法POST的使用者: ", uident);
            find = 0;
            setbfile(genbuf, bh->filename, "deny_users");
            if ((fp = fopen(genbuf, "r")) == NULL) {
                prints("(none)\n");
                return 0;
            }
            len = strlen(uident);
            if (len) {
                while (fgets(genbuf, 256 /*STRLEN*/, fp) != NULL) {
                    if (!strncasecmp(genbuf, uident, len)) {
                        if (genbuf[len] == 32) {
                            strncpy(uident, genbuf, len);
                            uident[len] = 0;
                            find = 1;
                            break;
                        }
                    }
                }
            }
            fclose(fp);
            if (!find) {
                move(3, 0);
                prints("该ID不在封禁名单内!");
                clrtoeol();
                pressreturn();
                goto Here;
            }
            modify_user_deny(bh, uident, genbuf);
        } else if ((*ans == 'D') && count) {
            int len;

            move(1, 0);
            sprintf(genbuf, "删除无法 POST 的使用者: ");
            getdata(1, 0, genbuf, uident, 13, DOECHO, NULL, true);
            find = 0;           /*Haohmaru.99.12.09.原来的代码如果被封者已自杀就删不掉了 */
            setbfile(genbuf, bh->filename, "deny_users");
            if ((fp = fopen(genbuf, "r")) == NULL) {
                prints("(none)\n");
                return 0;
            }
            len = strlen(uident);
            if (len) {
                while (fgets(genbuf, 256 /*STRLEN*/, fp) != NULL) {
                    if (!strncasecmp(genbuf, uident, len)) {
                        if (genbuf[len] == 32) {
                            strncpy(uident, genbuf, len);
                            uident[len] = 0;
                            find = 1;
                            break;
                        }
                    }
                }
            }
            fclose(fp);
            if (!find) {
                move(3, 0);
                prints("该ID不在封禁名单内!");
                clrtoeol();
                pressreturn();
                goto Here;
            }
            /*--- add to check if undeny time reached. by period 2000-09-11 ---*/
            {
                /*char *lptr;
                time_t ldenytime;*/

                /*
                 * now the corresponding line in genbuf
                 */
                if (NULL != (lptr = strrchr(genbuf, '[')))
                    sscanf(lptr + 1, "%lu", &ldenytime);
                else
                    ldenytime = now + 1;
                if (ldenytime > now) {
                    move(3, 0);
                    prints(genbuf);
                    if (false == askyn("该用户封禁时限未到，确认要解封？", false /*, false */))
                        goto Here;      /* I hate Goto!!! */
                }
            }
            /*---  ---*/
            move(1, 0);
            clrtoeol();
            if (uident[0] != '\0') {
                if (deldeny(getCurrentUser(), (char *)bh->filename, uident, 0, (ldenytime > now) ? 1 : 0, getSession())) {
                }
            }
        } else if (count > 20 && isdigit(ans[0])) {
            if (ans[1] == 0)
                page = *ans - '0';
            else
                page = atoi(ans);
            if (page < 0)
                break;          /*不会封人超过10屏吧?那可是200人啊!  会的！ */
            listdeny(bh, page);
            pressanykey();
        } else
            break;
    }                           /*end of while */
    clear();
    return FULLUPDATE;
}

/* etnlegend, 2005.12.26, 俱乐部授权管理接口结构更新 */
static int func_query_club_users(struct userec *user,void *varg)
{
    if (user->userid[0]&&get_user_club_perm(user,currboard,*(int*)varg))
        AddNameList(user->userid);
    return 0;
}
static int func_remove_club_users(struct userec *user,void *varg)
{
    if (user->userid[0]&&get_user_club_perm(user,currboard,*(int*)varg)&&!del_user_club_perm(user,currboard,*(int*)varg)) {
        if (!(*(int*)varg)&&user==getCurrentUser()&&!check_read_perm(user,currboard))
            set_user_club_perm(user,currboard,*(int*)varg);
        else
            AddNameList(user->userid);
    }
    return 0;
}
static int func_dump_users(char *userid,void *varg)
{
    ((char**)(((void**)varg)[0]))[(*(int*)(((void**)varg)[1]))]=userid;
    (*(int*)(((void**)varg)[1]))++;
    return 0;
}
static int func_clear_send_mail(char *userid,void *varg)
{
    return club_maintain_send_mail(userid,(char*)(((void**)varg)[0]),1,(*(int*)(((void**)varg)[1])),currboard,getSession());
}
#ifdef BOARD_SECURITY_LOG
struct clear_club_report_arg {
    FILE *fn;
    int count;
    int write_perm;
};

static int func_clear_security_report(char *userid, void *varg)
{
    struct clear_club_report_arg *cc = (struct clear_club_report_arg *)(((void**)varg)[0]);
    char *comment = (char *)(((void**)varg)[1]);
    fprintf(cc->fn, "\033[3%dm%4d  %-12s  %s  删除  %s\033[m\n",
            (cc->count%2)?2:3, cc->count, userid, !(cc->write_perm)?"读取":"发表", comment);
    cc->count++;
    return 0;
}
#endif
typedef int (*APPLY_USERS_FUNC)(int(*)(struct userec*,void*),void*);
int clubmember(struct _select_def *conf,struct fileheader *fh,void *varg)
{
    static const char *title="\033[1;32m[设定俱乐部授权用户]\033[m";
    static const char *echo="\033[1;37m翻页[\033[1;36m<SP>\033[1;37m]/增加[\033[1;36mA\033[1;37m]"
                            "/删除[\033[1;36mD\033[1;37m]/批量[\033[1;36mI\033[1;37m]/清理[\033[1;36mC\033[1;37m]"
                            "/群信[\033[1;36mM\033[1;37m]/退出[\033[1;36m<ESC>\033[1;37m/\033[1;36m<CR>\033[1;37m]: \033[m";
    static const char *choice="\033[1;37m操作读取[R]/发表[\033[1;36mP\033[1;37m]权限列表 [R]: \033[m";
    static char comment[128]="";
    APPLY_USERS_FUNC func=(APPLY_USERS_FUNC)apply_users;
    NLNode *head,*start,*curr;
    FILE *fp;
    struct userec *user;
    char genbuf[1024],buf[256],line[256],fn[256],userid[16],ans[4],**p_users, *sp_com;
    int i,j,k,write_perm,need_refresh,count,page;
    void *arg[2];
#ifdef BOARD_SECURITY_LOG
    char filename[STRLEN];
    FILE *ft;
#endif

    if (!(currboard->flag&(BOARD_CLUB_READ|BOARD_CLUB_WRITE))||!(currboard->clubnum>0)||(currboard->clubnum>MAXCLUB))
        return DONOTHING;
	if (!chk_currBM(currBM,getCurrentUser())) {
#ifdef ENABLE_BOARD_MEMBER
		if (!check_board_member_manager(&currmember, currboard, BMP_CLUB))
#endif	
	    return DONOTHING;	
	}
    clear();
    if ((currboard->flag&BOARD_CLUB_READ)&&(currboard->flag&BOARD_CLUB_WRITE)) {
        move(0,0);
        prints("%s",title);
        getdata(1,0,(char*)choice,ans,2,DOECHO,NULL,true);
        ans[0]=toupper(ans[0]);
        write_perm=(ans[0]=='P');
    } else
        write_perm=(currboard->flag&BOARD_CLUB_WRITE);
    move(0,0);
    clrtoeol();
    prints("%s \033[1;36m<%s>\033[m",title,(!write_perm?"读取":"发表"));
    need_refresh=1;
    count=0;
    page=0;
    head=NULL;
    start=NULL;
    while (1) {
        if (need_refresh) {
            CreateNameList();
            func(func_query_club_users,&write_perm);
            SortNameList(0);
            count=GetNameListCount();
            head=GetNameListHead();
            start=head;
            curr=head;
            page=0;
            need_refresh=0;
        } else
            curr=start;
        move(1,0);
        clrtobot();
        move(3,0);
        if (!count)
            prints("\033[1;33m%s\033[m","尚无授权用户...");
        else {
            j=0;
            do {
                k=MaxLen(curr,t_lines-4);
                if (!((j+k)<t_columns))
                    break;
                for (i=3; i<t_lines-1; i++) {
                    move(i,j);
                    prints("%s",curr->word);
                    if (!(curr=curr->next))
                        break;
                }
                j+=(k+2);
            } while (curr);
            move(t_lines-1,0);
            if (!curr) {
                if (!page)
                    sprintf(genbuf,"当前第 %d 页, 共 1 页, 查阅结束 ...",page+1);
                else
                    sprintf(genbuf,"当前第 %d 页, 为列表的最后一页, 按 <SPACE> 键回到第 1 页 ...",page+1);
            } else
                sprintf(genbuf,"当前第 %d 页, 按 <SPACE> 键查阅下一页 ...",page+1);
            for (i=strlen(genbuf); i<t_columns; i++)
                genbuf[i]=32;
            genbuf[i]=0;
            prints("\033[1;32;42m%s\033[m",genbuf);
        }
        move(1,0);
        prints("%s",echo);
        ingetdata=1;
        do {
            ans[1]=0;
            switch (ans[0]=igetkey()) {
                case 10:
                case 13:
                case 27:
                case 32:
                case 'a':
                case 'A':
                case 'd':
                case 'D':
                case 'i':
                case 'I':
                case 'c':
                case 'C':
                case 'm':
                case 'M':
                    ans[1]=1;
                    break;
                default:
                    continue;
            }
        } while (!ans[1]);
        ingetdata=0;
        ans[0]=toupper(ans[0]);
        if (ans[0]==32) {
            if (!curr) {
                start=head;
                page=0;
            } else {
                start=curr;
                page++;
            }
        } else if (ans[0]=='A') {
            move(1,0);
            clrtobot();
            usercomplete("增加俱乐部授权用户: ",buf);
            move(1,0);
            clrtobot();
            snprintf(userid,16,"%s",buf);
            if (!userid[0]||!getuser(userid,&user)) {
                prints("\033[1;33m%s\033[0;33m<Enter>\033[m","错误的用户名...");
                WAIT_RETURN;
                continue;
            }
            if (!strcmp(user->userid,"guest")) {
                prints("\033[1;33m%s\033[0;33m<Enter>\033[m","不允许对 guest 进行授权...");
                WAIT_RETURN;
                continue;
            }
            if (get_user_club_perm(user,currboard,write_perm)) {
                prints("\033[1;33m%s\033[0;33m<Enter>\033[m","该用户已经被授权...");
                WAIT_RETURN;
                continue;
            }
#ifdef SECONDSITE
            if (!canIsend2(getCurrentUser(),user->userid)) {
                prints("\033[1;33m%s\033[0;33m<Enter>\033[m","该用户已拒绝你的授权操作...");
                WAIT_RETURN;
                continue;
            }
#endif
            prints("\033[1;37m增加俱乐部授权用户: \033[1;32m%s\033[m",user->userid);
            sprintf(genbuf,"\033[1;37m附加说明 [\033[1;36m%s\033[1;37m]: \033[m",comment);
            getdata(2,0,genbuf,buf,64,DOECHO,NULL,true);
            if (buf[0]) {
                trimstr(buf);
                snprintf(comment,128,"%s",buf);
            }
            sprintf(genbuf,"\033[1;33m确认授予 \033[1;32m%s\033[1;33m 本俱乐部%s权限 [y/N]: \033[m",
                    user->userid,(!write_perm?"读取":"发表"));
            getdata(3,0,genbuf,ans,2,DOECHO,NULL,true);
            ans[0]=toupper(ans[0]);
            if (ans[0]!='Y')
                continue;
            need_refresh=1;
            move(4,0);
            if (set_user_club_perm(user,currboard,write_perm)) {
                prints("\033[1;33m%s\033[0;33m<Enter>\033[m","操作过程中发生错误...");
                WAIT_RETURN;
                continue;
            }
            club_maintain_send_mail(user->userid,comment,0,write_perm,currboard,getSession());
            prints("\033[1;32m%s\033[0;33m<Enter>\033[m","增加成功!");
#ifdef BOARD_SECURITY_LOG
            gettmpfilename(filename, "club_member");
            if ((ft = fopen(filename, "w"))!=NULL) {
                fprintf(ft, "\033[44m用户ID        类别  操作  附加说明\033[K\033[m\n");
                fprintf(ft, "%-12s  %s  增加  %s\n", user->userid, !write_perm?"读取":"发表", comment);
                fclose(ft);
                sprintf(buf, "授予 %s 俱乐部%s权限", user->userid, !write_perm?"读取":"发表");
                board_security_report(filename, getCurrentUser(), buf, currboard->filename, NULL);
                unlink(filename);
            }
#endif
            WAIT_RETURN;
        } else if (ans[0]=='D') {
            move(1,0);
            clrtoeol();
            namecomplete("删除俱乐部授权用户: ",buf);
            move(1,0);
            clrtobot();
            snprintf(userid,16,"%s",buf);
            if (!userid[0]||!getuser(userid,&user)) {
                prints("\033[1;33m%s\033[0;33m<Enter>\033[m","错误的用户名...");
                WAIT_RETURN;
                continue;
            }
            if (!get_user_club_perm(user,currboard,write_perm)) {
                prints("\033[1;33m%s\033[0;33m<Enter>\033[m","该用户尚未被授权...");
                WAIT_RETURN;
                continue;
            }
            prints("\033[1;37m删除俱乐部授权用户: \033[1;32m%s\033[m",user->userid);
            sprintf(genbuf,"\033[1;37m附加说明 [\033[1;36m%s\033[1;37m]: \033[m",comment);
            getdata(2,0,genbuf,buf,64,DOECHO,NULL,true);
            if (buf[0]) {
                trimstr(buf);
                snprintf(comment,128,"%s",buf);
            }
            sprintf(genbuf,"\033[1;33m确认取消 \033[1;32m%s\033[1;33m 本俱乐部%s权限 [y/N]: \033[m",
                    user->userid,(!write_perm?"读取":"发表"));
            getdata(3,0,genbuf,ans,2,DOECHO,NULL,true);
            ans[0]=toupper(ans[0]);
            if (ans[0]!='Y')
                continue;
            need_refresh=1;
            move(4,0);
            if (del_user_club_perm(user,currboard,write_perm)) {
                prints("\033[1;33m%s\033[0;33m<Enter>\033[m","操作过程中发生错误...");
                WAIT_RETURN;
                continue;
            }
            club_maintain_send_mail(user->userid,comment,1,write_perm,currboard,getSession());
            prints("\033[1;32m%s\033[0;33m<Enter>\033[m","删除成功!");
#ifdef BOARD_SECURITY_LOG
            gettmpfilename(filename, "club_member");
            if ((ft = fopen(filename, "w"))!=NULL) {
                fprintf(ft, "\033[44m用户ID        类别  操作  附加说明\033[K\033[m\n");
                fprintf(ft, "%-12s  %s  删除  %s\n", user->userid, !write_perm?"读取":"发表", comment);
                fclose(ft);
                sprintf(buf, "取消 %s 俱乐部%s权限", user->userid, !write_perm?"读取":"发表");
                board_security_report(filename, getCurrentUser(), buf, currboard->filename, NULL);
                unlink(filename);
            }
#endif
            WAIT_RETURN;
        } else if (ans[0]=='I') {
            move(1,0);
            clrtobot();
            sprintf(genbuf,"\033[1;37m全局附加说明 [\033[1;36m%s\033[1;37m]: \033[m",comment);
            getdata(1,0,genbuf,buf,64,DOECHO,NULL,true);
            if (buf[0]) {
                trimstr(buf);
                snprintf(comment,128,"%s",buf);
            }
            move(3,0);
            prints("%s",
                   "    \033[1;33m[批量操作俱乐部授权列表信息文件格式]\033[m\n\n"
                   "    \033[1;37m以行为单位, 每行操作一位用户, 附加前缀如下:\033[m\n"
                   "    \033[1;33m#\033[1;37m 起始的行为注释行, 无作用;\033[m\n"
                   "    \033[1;31m-\033[1;37m 起始的行表示删除其后的用户;\033[m\n"
                   "    \033[1;32m+\033[1;37m 起始的行表示增加其后的用户;\033[m\n"
                   "    \033[1;37m无前缀时默认操作为增加...\033[m\n\n"
                   "    \033[1;37m用户 ID 后可指定特定的附加说明,\033[m\n"
                   "    \033[1;37m以\033[1;33m空格\033[1;37m作为 ID 与附加说明间的分隔符,\033[m\n"
                   "    \033[1;37m若不指定, 则该行操作使用之前设定的全局附加说明...\033[m\n\n"
                   "\033[1;37m按 \033[1;32m<Enter>\033[1;37m 键后开始编辑批量修改列表: \033[m");
            WAIT_RETURN;
            saveline(0,0,NULL);
            j=uinfo.mode;
            modify_user_mode(EDITANN);
            sprintf(fn,"tmp/club_perm_%ld_%d",time(NULL),getpid());
            k=vedit(fn,0,NULL,NULL,0);
            modify_user_mode(j);
            clear();
            saveline(0,1,NULL);
            move(1,0);
            if (k==-1) {
                unlink(fn);
                prints("\033[1;33m%s\033[0;33m<Enter>\033[m","取消...");
                WAIT_RETURN;
                continue;
            } else {
                sprintf(genbuf,"\033[1;33m确认批量操作俱乐部%s授权列表 [y/N]: \033[m",(!write_perm?"读取":"发表"));
                getdata(1,0,genbuf,ans,2,DOECHO,NULL,true);
                ans[0]=toupper(ans[0]);
                if (ans[0]!='Y')
                    continue;
            }
            if (!(fp=fopen(fn,"r")))
                continue;
            need_refresh=1;
            i=0;
            j=0;
#ifdef BOARD_SECURITY_LOG
            gettmpfilename(filename, "club_member");
            if ((ft = fopen(filename, "w"))!=NULL)
                fprintf(ft, "\033[44m序号  用户ID        类别  操作  附加说明\033[K\033[m\n");
#endif
            while (fgets(line,256,fp)) {
                k=strlen(line);
                if (line[k-1]==10||line[k-1]==13)
                    line[k-1]=0;
                trimstr(line);
                switch (line[0]) {
                    case 0:
                    case 10:
                    case 13:
                    case '#':
                        continue;
                    case '-':
                        trimstr(&line[1]);
                        /* fancy Jan 11 2008, 加入特定附加说明支持 ... */
                        sp_com = strchr(&line[1], 32);
                        if (sp_com) {
                            *sp_com++ = 0;
                            trimstr(sp_com);
                        }
                        if (!getuser(&line[1],&user)||!get_user_club_perm(user,currboard,write_perm))
                            continue;
                        if (!del_user_club_perm(user,currboard,write_perm)) {
                            club_maintain_send_mail(user->userid, sp_com ? sp_com : comment,1,write_perm,currboard,getSession());
                            j++;
#ifdef BOARD_SECURITY_LOG
                            if (ft)
                                fprintf(ft, "\033[3%dm%4d  %-12s  %s  删除  %s\033[m\n",
                                        (i+j)%2?2:3, i+j, user->userid, !write_perm?"读取":"发表", sp_com?sp_com:comment);
#endif
                        }
                        break;
                    case '+':
                        line[0]=32;
                        trimstr(line);
                    default:
                        /* fancy Jan 11 2008, 加入特定附加说明支持 ... */
                        sp_com = strchr(line, 32);
                        if (sp_com) {
                            *sp_com++ = 0;
                            trimstr(sp_com);
                        }
                        if (!getuser(line,&user)||!strcmp(user->userid,"guest")||get_user_club_perm(user,currboard,write_perm))
                            continue;
#ifdef SECONDSITE
                        if (!canIsend2(getCurrentUser(),user->userid))
                            continue;
#endif
                        if (!set_user_club_perm(user,currboard,write_perm)) {
                            club_maintain_send_mail(user->userid, sp_com ? sp_com : comment,0,write_perm,currboard,getSession());
                            i++;
#ifdef BOARD_SECURITY_LOG
                            if (ft)
                                fprintf(ft, "\033[3%dm%4d  %-12s  %s  增加  %s\033[m\n",
                                        (i+j)%2?2:3, i+j, user->userid, !write_perm?"读取":"发表", sp_com?sp_com:comment);
#endif
                        }
                        break;
                }
            }
            fclose(fp);
            unlink(fn);
            move(2,0);
            prints("\033[1;37m增加 \033[1;33m%d\033[1;37m 位用户, 删除 \033[1;33m%d\033[1;37m 位用户..."
                   "\033[1;32m%s\033[0;33m<Enter>\033[m",i,j,"操作已完成!");
#ifdef BOARD_SECURITY_LOG
            if (ft) {
                fclose(ft);
                if (i+j) {
                    sprintf(buf, "批量操作俱乐部%s授权列表", !write_perm?"读取":"发表");
                    board_security_report(filename, getCurrentUser(), buf, currboard->filename, NULL);
                }
                unlink(filename);
            }
#endif
            WAIT_RETURN;
        } else if (ans[0]=='M') {
            /* 注: 俱乐部群信部分原作者为 asing@zixia */
            if (HAS_PERM(getCurrentUser(),PERM_DENYMAIL)||!HAS_PERM(getCurrentUser(),PERM_LOGINOK))
                continue;
            move(1,0);
            clrtobot();
            if (!(i=GetNameListCount())) {
                prints("\033[1;33m%s\033[0;33m<Enter>\033[m","尚无授权用户...");
                WAIT_RETURN;
                continue;
            }
            if (!(p_users=(char**)malloc(i*sizeof(char*))))
                continue;
            i=0;
            arg[0]=p_users;
            arg[1]=&i;
            ApplyToNameList(func_dump_users,arg);
            getdata(1,0,"\033[1;37m设定群信标题: \033[m",buf,40,DOECHO,NULL,true);
            sprintf(genbuf,"[俱乐部 %s 群信] %s",currboard->filename,buf);
            snprintf(buf,ARTICLE_TITLE_LEN,"%s",genbuf);
            saveline(0,0,NULL);
            j=do_gsend(p_users,buf,i);
            free(p_users);
            clear();
            saveline(0,1,NULL);
            move(1,0);
            if (j) {
                prints("\033[1;33m%s\033[0;33m<Enter>\033[m","操作取消或发送群信过程中发生错误...");
                WAIT_RETURN;
                continue;
            }
            prints("\033[1;32m%s\033[0;33m<Enter>\033[m","群信已发送!");
            WAIT_RETURN;
        } else if (ans[0]=='C') {
            move(1,0);
            clrtobot();
            if (!HAS_PERM(getCurrentUser(),(PERM_OBOARDS|PERM_SYSOP))) {
                prints("\033[1;33m\033[0;33m<Enter>\033[m","当前用户不具有清理俱乐部授权列表的权限...");
                WAIT_RETURN;
                continue;
            }
            sprintf(genbuf,"\033[1;37m附加说明 [\033[1;36m%s\033[1;37m]: \033[m",comment);
            getdata(1,0,genbuf,buf,64,DOECHO,NULL,true);
            if (buf[0]) {
                trimstr(buf);
                snprintf(comment,128,"%s",buf);
            }
            sprintf(genbuf,"\033[1;31m确认清理本俱乐部%s授权列表 [y/N]: \033[m",(!write_perm?"读取":"发表"));
            getdata(2,0,genbuf,ans,2,DOECHO,NULL,true);
            ans[0]=toupper(ans[0]);
            if (ans[0]!='Y')
                continue;
            need_refresh=1;
            CreateNameList();
            func(func_remove_club_users,&write_perm);
            arg[0]=comment;
            arg[1]=&write_perm;
            ApplyToNameList(func_clear_send_mail,&arg);
            move(3,0);
            prints("\033[1;32m%s\033[0;33m<Enter>\033[m","已完成清理!");
#ifdef BOARD_SECURITY_LOG
            gettmpfilename(filename, "club_member");
            if ((ft = fopen(filename, "w"))!=NULL) {
                struct clear_club_report_arg cc;

                fprintf(ft, "\033[44m序号  用户ID        类别  操作  附加说明\033[K\033[m\n");
                cc.fn = ft;
                cc.count = 1;
                cc.write_perm = write_perm;
                arg[0] = &cc;
                arg[1] = comment;
                ApplyToNameList(func_clear_security_report, &arg);
                fclose(ft);
                sprintf(buf, "清理俱乐部%s授权列表", !write_perm?"读取":"发表");
                board_security_report(filename, getCurrentUser(), buf, currboard->filename, NULL);
                unlink(filename);
            }
#endif
            WAIT_RETURN;
        } else {
            CreateNameList();
            break;
        }
    }
    clear();
    return FULLUPDATE;
}

#ifdef NEWSMTH
int post_range_func(struct _select_def *conf,struct fileheader *fileinfo,void *varg)
{
    struct read_arg *rarg;

    rarg=(struct read_arg*)conf->arg;
    switch (rarg->mode) {
        case DIR_MODE_NORMAL:
        case DIR_MODE_DIGEST:
        case DIR_MODE_MAIL:
            delete_range(conf, fileinfo, varg);
            break;
        case DIR_MODE_DELETED:
        case DIR_MODE_JUNK:
            undelete_range(conf, fileinfo, varg);
            break;
        default:
            return DONOTHING;
    }
    return FULLUPDATE;
}
#endif

/* etnlegend, 2006.04.21, 区段删除界面更新 */

struct delete_range_arg {
    struct _select_item *items;
    enum delete_range_type {menu_main,menu_sub,menu_sub_safe} type;
    int fw;
    int id_from;
    int id_to;
};

static int delete_range_read(char *buf,int len,const char *valid)
{
#define DELETE_RANGE_READ_FORMAT    "\033[1;32;42m%c\033[m"
#define DELETE_RANGE_READ_BORDER_L  '['
#define DELETE_RANGE_READ_BORDER_R  ']'
#define KEY_SP 32
#define KEY_CR 13
#define KEY_LF 10
#define KEY_BS 8
    int i,row,col;
    getyx(&row,&col);
    prints(DELETE_RANGE_READ_FORMAT,DELETE_RANGE_READ_BORDER_L);
    for (i=0; i<len; i++)
        prints(DELETE_RANGE_READ_FORMAT,KEY_SP);
    prints(DELETE_RANGE_READ_FORMAT,DELETE_RANGE_READ_BORDER_R);
    move(row,col+1);
    i=0;
    ingetdata=1;
    while (!(i>len)) {
        buf[i]=igetkey();
        switch (buf[i]) {
            case KEY_CR:
            case KEY_LF:
                buf[i]=0;
                break;
            case KEY_BS:
                if (i>0) {
                    move(row,col+i);
                    prints(DELETE_RANGE_READ_FORMAT,KEY_SP);
                    move(row,col+i);
                    i--;
                }
                continue;
            case KEY_ESC:
                if (!i) {
                    buf[i]=0;
                    break;
                }
                move(row,col+1);
                for (i=0; i<len; i++)
                    prints(DELETE_RANGE_READ_FORMAT,KEY_SP);
                move(row,col+1);
                i=0;
                continue;
            default:
                break;
        }
        if (!buf[i])
            break;
        if (i==len||(valid&&!strchr(valid,buf[i])))
            continue;
        prints(DELETE_RANGE_READ_FORMAT,buf[i]);
        i++;
    }
    ingetdata=0;
    return i;
#undef DELETE_RANGE_READ_FORMAT
#undef DELETE_RANGE_READ_BORDER_L
#undef DELETE_RANGE_READ_BORDER_R
#undef KEY_SP
#undef KEY_CR
#undef KEY_LF
#undef KEY_BS
}

static int delete_range_select(struct _select_def *conf)
{
    struct delete_range_arg *arg=(struct delete_range_arg*)conf->arg;
    char buf[16];
    if (((arg->type==menu_sub)&&(conf->pos==4))||((arg->type==menu_sub_safe)&&(conf->pos==1))) {
        move(arg->items[conf->pos-1].y,arg->items[conf->pos-1].x);
        clrtoeol();
        delete_range_read(buf,arg->fw,"0123456789");
        if (!buf[0])
            return SHOW_REFRESH;
        arg->id_from=atoi(buf);
        move(arg->items[conf->pos-1].y,arg->items[conf->pos-1].x+(arg->fw+3));
        prints("\033[1;37m%s \033[m","→");
        delete_range_read(buf,arg->fw,"0123456789");
        if (!buf[0])
            return SHOW_REFRESH;
        arg->id_to=atoi(buf);
    }
    return SHOW_SELECT;
}

static int delete_range_show(struct _select_def *conf,int index)
{
    struct _select_item *item=&(((struct delete_range_arg*)conf->arg)->items[index-1]);
    move(item->y,item->x);
    prints("\033[1;37m[\033[1;36m%c\033[1;37m] %s\033[m",item->hotkey,item->data);
    return SHOW_CONTINUE;
}

static int delete_range_key(struct _select_def *conf,int key)
{
    struct delete_range_arg *arg=(struct delete_range_arg*)conf->arg;
    int index;
    if (key==KEY_ESC)
        return SHOW_QUIT;
    for (index=0; index<conf->item_count; index++)
        if (toupper(key)==toupper(arg->items[index].hotkey)) {
            conf->new_pos=(index+1);
            return SHOW_SELCHANGE;
        }
    return SHOW_CONTINUE;
}

static int delete_range_interface_sub_menu(int mode,int current,int total,struct delete_range_arg *arg)
{
    struct _select_item sel[5];
    struct _select_def conf;
    POINT pts[4];
    char menustr[4][128],buf[16];
    int safe,fw[2];
    safe=!(mode&(DELETE_RANGE_BASE_MODE_TOKEN|DELETE_RANGE_BASE_MODE_CLEAR));
    sel[0].x=32; sel[0].y=2; sel[0].hotkey='0'; sel[0].type=SIT_SELECT; sel[0].data=menustr[(safe?0:3)];
    sel[1].x=32; sel[1].y=3; sel[1].hotkey='1'; sel[1].type=SIT_SELECT; sel[1].data=menustr[1];
    sel[2].x=32; sel[2].y=4; sel[2].hotkey='2'; sel[2].type=SIT_SELECT; sel[2].data=menustr[2];
    sel[3].x=32; sel[3].y=5; sel[3].hotkey='3'; sel[3].type=SIT_SELECT; sel[3].data=menustr[(safe?3:0)];
    sel[4].x=-1; sel[4].y=-1; sel[4].hotkey=-1; sel[4].type=0; sel[4].data=NULL;
    pts[0].x=sel[0].x; pts[0].y=sel[0].y;
    pts[1].x=sel[1].x; pts[1].y=sel[1].y;
    pts[2].x=sel[2].x; pts[2].y=sel[2].y;
    pts[3].x=sel[3].x; pts[3].y=sel[3].y;
    sprintf(buf,"%d",current);
    fw[0]=strlen(buf);
    sprintf(buf,"%d",total);
    fw[1]=strlen(buf);
    sprintf(menustr[0],"%s","指定删除区域");
    sprintf(menustr[1],"\033[1;37m当前位置向前 [ \033[1;31m%*d \033[1;37m- \033[1;31m%*d \033[1;37m]\033[m",
            fw[0],1,fw[1],current);
    sprintf(menustr[2],"\033[1;37m当前位置向后 [ \033[1;31m%*d \033[1;37m- \033[1;31m%*d \033[1;37m]\033[m",
            fw[0],current,fw[1],total);
    sprintf(menustr[3],"\033[1;37m全部         [ \033[1;31m%*d \033[1;37m- \033[1;31m%*d \033[1;37m]\033[m",
            fw[0],1,fw[1],total);
    memset(arg,0,sizeof(struct delete_range_arg));
    arg->items=sel;
    arg->type=(safe?menu_sub_safe:menu_sub);
    arg->fw=fw[1];
    memset(&conf,0,sizeof(struct _select_def));
    conf.item_count=4;
    conf.item_per_page=conf.item_count;
    conf.flag=LF_LOOP;
    conf.prompt="◇";
    conf.pos=1;
    conf.item_pos=pts;
    conf.arg=arg;
    conf.title_pos.x=-1;
    conf.title_pos.y=-1;
    conf.on_select=delete_range_select;
    conf.show_data=delete_range_show;
    conf.key_command=delete_range_key;
    if (list_select_loop(&conf)!=SHOW_SELECT)
        return -1;
    switch (conf.pos) {
        case 1:
            if (!safe) {
                arg->id_from=1;
                arg->id_to=total;
            }
            break;
        case 2:
            arg->id_from=1;
            arg->id_to=current;
            break;
        case 3:
            arg->id_from=current;
            arg->id_to=total;
            break;
        case 4:
            if (safe) {
                arg->id_from=1;
                arg->id_to=total;
            }
            break;
        default:
            return -1;
    }
    return 0;
}

static int delete_range_interface_main_menu(void)
{
    struct _select_item sel[6];
    struct _select_def conf;
    struct delete_range_arg arg;
    POINT pts[5];
    int ret;
    sel[0].x=4; sel[0].y=2; sel[0].hotkey='0'; sel[0].type=SIT_SELECT; sel[0].data="删除拟删文章";
    sel[1].x=4; sel[1].y=3; sel[1].hotkey='1'; sel[1].type=SIT_SELECT; sel[1].data="常规区段删除";
    sel[2].x=4; sel[2].y=4; sel[2].hotkey='2'; sel[2].type=SIT_SELECT; sel[2].data="强制区段删除";
    sel[3].x=4; sel[3].y=5; sel[3].hotkey='3'; sel[3].type=SIT_SELECT; sel[3].data="设置拟删标记";
    sel[4].x=4; sel[4].y=6; sel[4].hotkey='4'; sel[4].type=SIT_SELECT; sel[4].data="清除拟删标记";
    sel[5].x=-1; sel[5].y=-1; sel[5].hotkey=-1; sel[5].type=0; sel[5].data=NULL;
    pts[0].x=sel[0].x; pts[0].y=sel[0].y;
    pts[1].x=sel[1].x; pts[1].y=sel[1].y;
    pts[2].x=sel[2].x; pts[2].y=sel[2].y;
    pts[3].x=sel[3].x; pts[3].y=sel[3].y;
    pts[4].x=sel[4].x; pts[4].y=sel[4].y;
    memset(&arg,0,sizeof(struct delete_range_arg));
    arg.items=sel;
    arg.type=menu_main;
    memset(&conf,0,sizeof(struct _select_def));
    conf.item_count=5;
    conf.item_per_page=conf.item_count;
    conf.flag=LF_LOOP;
    conf.prompt="◇";
    conf.pos=1;
    conf.item_pos=pts;
    conf.arg=&arg;
    conf.title_pos.x=-1;
    conf.title_pos.y=-1;
    conf.on_select=delete_range_select;
    conf.show_data=delete_range_show;
    conf.key_command=delete_range_key;
    if ((ret=list_select_loop(&conf))!=SHOW_SELECT)
        return -1;
    switch (conf.pos) {
        case 1:
            return DELETE_RANGE_BASE_MODE_TOKEN;
        case 2:
            return DELETE_RANGE_BASE_MODE_RANGE;
        case 3:
            return DELETE_RANGE_BASE_MODE_FORCE;
        case 4:
            return DELETE_RANGE_BASE_MODE_MPDEL;
        case 5:
            return DELETE_RANGE_BASE_MODE_CLEAR;
        default:
            return -1;
    }
}

int delete_range(struct _select_def *conf,struct fileheader *file,void *varg)
{
#define DELETE_RANGE_ALLOWED_INTERVAL   20
#define DELETE_RANGE_QUIT(n,s)          do{\
        move((n),4);\
        prints("\033[1;33m%s\033[0;33m<Enter>\033[m",(s));\
        if(uinfo.mode==MAIL)\
            modify_user_mode(RMAIL);\
        WAIT_RETURN;\
        return FULLUPDATE;\
    }while(0)
    sigset_t mask_set,old_mask_set;
    struct stat st_src;
    struct read_arg *rarg;
    struct delete_range_arg arg;
    char buf[256],ans[4];
    const char *ident,*src,*dst,*type;
    int mail,current,total,mode,ret,line;
    time_t timestamp;
    rarg=(struct read_arg*)conf->arg;
    total=rarg->filecount;
    current=((conf->pos>total)?total:(conf->pos));
    line=8;
    switch (rarg->mode) {
        case DIR_MODE_NORMAL:
            mail=0;
            ident=currboard->filename;
            src=".DIR";
            dst=".DELETED";
            mode=DELETE_RANGE_BASE_MODE_CHECK;
            break;
        case DIR_MODE_DIGEST:
            mail=0;
            ident=currboard->filename;
            src=".DIGEST";
            dst=NULL;
            mode=DELETE_RANGE_BASE_MODE_CHECK;
            break;
        case DIR_MODE_MAIL:
            mail=1;
            ident=getCurrentUser()->userid;
            if (!(src=strrchr(rarg->direct,'.')))
                return DONOTHING;
            dst=(!strcmp(src,".DELETED")?NULL:".DELETED");
            mode=DELETE_RANGE_BASE_MODE_MAIL;
            break;
        default:
            return DONOTHING;
    }
    if (!mail&&
#ifdef MEMBER_MANAGER
		deny_del_article(currboard,&currmember,NULL,getSession())
#else
		deny_del_article(currboard,NULL,getSession())
#endif
	)
        return DONOTHING;
		
#ifdef MEMBER_MANAGER
	if (!mail&&!HAS_PERM(getSession()->currentuser,PERM_SYSOP)&&!check_board_member_manager(&currmember, currboard, BMP_RANGE))
		return DONOTHING;
#endif		
    timestamp=time(NULL);
    !mail?setbfile(buf,ident,src):setmailfile(buf,ident,src);
    if (stat(buf,&st_src)==-1||!S_ISREG(st_src.st_mode))
        return DONOTHING;
    clear();
    move(0,0);
    prints("\033[1;32m%s \033[1;33m%s\033[m","[区段删除选单]","<Enter>键选择/<Esc>键退出");
    if (uinfo.mode==RMAIL)
        modify_user_mode(MAIL);
    if ((ret=delete_range_interface_main_menu())==-1)
        DELETE_RANGE_QUIT(line,"操作取消...");
    mode|=ret;
    switch (mode&DELETE_RANGE_BASE_MODE_OPMASK) {
        case DELETE_RANGE_BASE_MODE_TOKEN:
            type="删除拟删文章";
            break;
        case DELETE_RANGE_BASE_MODE_RANGE:
            type="常规区段删除";
            break;
        case DELETE_RANGE_BASE_MODE_FORCE:
            type="强制区段删除";
            break;
        case DELETE_RANGE_BASE_MODE_MPDEL:
            type="设置拟删标记";
            break;
        case DELETE_RANGE_BASE_MODE_CLEAR:
            type="清除拟删标记";
            break;
        default:
            DELETE_RANGE_QUIT(line,"发生未知错误, 操作取消...");
    }
    if (delete_range_interface_sub_menu(mode,current,total,&arg)==-1)
        DELETE_RANGE_QUIT(line,"操作取消...");
    if (uinfo.mode==MAIL)
        modify_user_mode(RMAIL);
    move(line++,4);
    sprintf(buf,"\033[1;33m%s \033[1;32m%d \033[1;37m- \033[1;32m%d \033[1;37m确认操作? [y/N] \033[m",
            type,arg.id_from,arg.id_to);
    prints("%s",buf);
    delete_range_read(ans,1,"ynYN");
    switch (ans[0]) {
        case 'Y':
            mode|=DELETE_RANGE_BASE_MODE_OVERM;
        case 'y':
            break;
        default:
            DELETE_RANGE_QUIT(++line,"操作取消...");
    }
    move(++line,4);
    prints("\033[1;33m%s\033[m","区段操作可能需要较长时间, 请耐心等候...");
    refresh();
    sigemptyset(&mask_set);
    sigaddset(&mask_set,SIGHUP);
    sigaddset(&mask_set,SIGBUS);
    sigaddset(&mask_set,SIGPIPE);
    sigaddset(&mask_set,SIGTERM);
    sigprocmask(SIG_SETMASK,NULL,&old_mask_set);
    sigprocmask(SIG_BLOCK,&mask_set,NULL);
    ret=delete_range_base(ident,src,dst,arg.id_from,arg.id_to,mode,NULL,&st_src);
    if (!mail)
        newbbslog(BBSLOG_USER,"delete_range %s %d - %d <%d,%#04x>",ident,arg.id_from,arg.id_to,mode,ret);
    sigprocmask(SIG_SETMASK,&old_mask_set,NULL);
    if (ret==0x21) {
        move(line++,4);
        clrtoeol();
        prints("%s","\033[1;37m系统检测到在您操作期间版面文章列表已经发生变化, \033[m");
        move(line++,4);
        sprintf(buf,"%s","\033[1;33m强制操作\033[1;37m[\033[1;31m严重不建议\033[1;37m]? [y/N] \033[m");
        prints("%s",buf);
        delete_range_read(ans,1,"ynYN");
        if (toupper(ans[0])!='Y')
            DELETE_RANGE_QUIT(++line,"操作取消...");
        if ((time(NULL)-timestamp)<DELETE_RANGE_ALLOWED_INTERVAL) {
            mode&=~DELETE_RANGE_BASE_MODE_CHECK;
            sigprocmask(SIG_BLOCK,&mask_set,NULL);
            ret=delete_range_base(ident,src,dst,arg.id_from,arg.id_to,mode,NULL,NULL);
            if (!mail)
                newbbslog(BBSLOG_USER,"delete_range %s %d - %d <%d,%#04x>",ident,arg.id_from,arg.id_to,mode,ret);
            sigprocmask(SIG_SETMASK,&old_mask_set,NULL);
        } else {
            move(++line,4);
            sprintf(buf,"\033[1;33m强制操作时限为 \033[1;31m%d \033[1;33m秒, "
                    "您此次操作已经超时, 操作取消...\033[0;33m<Enter>\033[m",DELETE_RANGE_ALLOWED_INTERVAL);
            prints("%s",buf);
            WAIT_RETURN;
            return FULLUPDATE;
        }
    }
    if (!mail)
        bmlog(getCurrentUser()->userid,ident,5,1);
    move(line++,4);
    clrtoeol();
    if (!ret) {
        if (!mail) {
            if ((mode&DELETE_RANGE_BASE_MODE_FORCE)
                    ||((mode&DELETE_RANGE_BASE_MODE_OVERM)&&(mode&DELETE_RANGE_BASE_MODE_TOKEN)))
                setboardmark(ident,1);
            setboardorigin(ident,1);
            updatelastpost(ident);
        }
        prints("\033[1;32m%s\033[0;33m<Enter>\033[m","操作完成!");
        WAIT_RETURN;
        return DIRCHANGED;
    }
    sprintf(buf,"\033[1;33m%s\033[1;31m<%#04x>\033[0;33m<Enter>\033[m","操作中发生错误!",ret);
    prints("%s",buf);
    WAIT_RETURN;
    return FULLUPDATE;
#undef DELETE_RANGE_ALLOWED_INTERVAL
#undef DELETE_RANGE_QUIT
}
#ifdef BATCHRECOVERY

/*--------------------------------------区段恢复接口-------------------------------------*/

/* 参考delete_range系列相关函数。 benogy@bupt     20080807 */

struct undelete_range_arg{
   struct _select_item *items;
   enum undelete_range_type{main_menu,sub_menu} type;
   int fw;
   int id_from;
   int id_to;
};


static int undelete_range_select(struct _select_def *conf)
{
   struct undelete_range_arg *arg=(struct undelete_range_arg*)conf->arg;
   char buf[16];

   if((arg->type==sub_menu) && (conf->pos==1)){

      move(arg->items[conf->pos-1].y,arg->items[conf->pos-1].x);
      clrtoeol();
      delete_range_read(buf,arg->fw,"0123456789");
      if(!buf[0])
         return SHOW_REFRESH;
      arg->id_from=atoi(buf);

      move(arg->items[conf->pos-1].y,arg->items[conf->pos-1].x+(arg->fw+3));
      prints("\033[1;37m%s \033[m","→");
      delete_range_read(buf,arg->fw,"0123456789");
      if(!buf[0])
         return SHOW_REFRESH;
      arg->id_to=atoi(buf);
   }
   return SHOW_SELECT;
}


static int undelete_range_key(struct _select_def *conf,int key)
{
   struct undelete_range_arg *arg=(struct undelete_range_arg*)conf->arg;
   int index;

   if(key==KEY_ESC)
      return SHOW_QUIT;

   for(index=0;index<conf->item_count;index++)
      if(toupper(key)==toupper(arg->items[index].hotkey)){
         conf->new_pos=(index+1);
         return SHOW_SELCHANGE;
      }

   return SHOW_CONTINUE;
}


static int undelete_range_show(struct _select_def *conf,int index){
   struct _select_item *item=&(((struct undelete_range_arg*)conf->arg)->items[index-1]);
   move(item->y,item->x);
   prints("\033[1;37m[\033[1;36m%c\033[1;37m] %s\033[m",item->hotkey,item->data);
   return SHOW_CONTINUE;
}


static int undelete_range_interface_main_menu(void)
{
   struct _select_item sel[3];
   struct _select_def conf;
   struct undelete_range_arg arg;
   POINT pts[2];
   int ret;

   sel[0].x=4;sel[0].y=2;sel[0].hotkey='0';sel[0].type=SIT_SELECT;sel[0].data="常规区段恢复(不包含被标记为删除的文章)";
   sel[1].x=4;sel[1].y=3;sel[1].hotkey='1';sel[1].type=SIT_SELECT;sel[1].data="强制区段恢复(所选范围内的所有文章)";
   sel[2].x=-1;sel[2].y=-1;sel[2].hotkey=-1;sel[2].type=0;sel[2].data=NULL;

   pts[0].x=sel[0].x;pts[0].y=sel[0].y;
   pts[1].x=sel[1].x;pts[1].y=sel[1].y;

   memset(&arg,0,sizeof(struct undelete_range_arg));
   arg.items=sel;
   arg.type=main_menu;

   memset(&conf,0,sizeof(struct _select_def));
   conf.item_count=2;
   conf.item_per_page=conf.item_count;
   conf.flag=LF_LOOP;
   conf.prompt="◇";
   conf.pos=1;
   conf.item_pos=pts;
   conf.arg=&arg;
   conf.title_pos.x=-1;
   conf.title_pos.y=-1;
   conf.on_select=undelete_range_select;
   conf.show_data=undelete_range_show;
   conf.key_command=undelete_range_key;

   if((ret=list_select_loop(&conf))!=SHOW_SELECT)
      return -1;

   if(conf.pos==1 || conf.pos==2)
      return conf.pos;
   else
      return -1;
}


static int undelete_range_interface_sub_menu(int current,int total,struct undelete_range_arg *arg)
{
   struct _select_item sel[5];
   struct _select_def conf;
   POINT pts[4];
   char menustr[4][128],buf[16];
   int fw[2];

   sel[0].x=8;sel[0].y=6;sel[0].hotkey='0';sel[0].type=SIT_SELECT;sel[0].data=menustr[0];
   sel[1].x=8;sel[1].y=7;sel[1].hotkey='1';sel[1].type=SIT_SELECT;sel[1].data=menustr[1];
   sel[2].x=8;sel[2].y=8;sel[2].hotkey='2';sel[2].type=SIT_SELECT;sel[2].data=menustr[2];
   sel[3].x=8;sel[3].y=9;sel[3].hotkey='3';sel[3].type=SIT_SELECT;sel[3].data=menustr[3];
   sel[4].x=-1;sel[4].y=-1;sel[4].hotkey=-1;sel[4].type=0;sel[4].data=NULL;

   pts[0].x=sel[0].x;pts[0].y=sel[0].y;
   pts[1].x=sel[1].x;pts[1].y=sel[1].y;
   pts[2].x=sel[2].x;pts[2].y=sel[2].y;
   pts[3].x=sel[3].x;pts[3].y=sel[3].y;

   sprintf(buf,"%d",current);
   fw[0]=strlen(buf);
   sprintf(buf,"%d",total);
   fw[1]=strlen(buf);

   sprintf(menustr[0],"%s","指定恢复区域");
   sprintf(menustr[1],"\033[1;37m当前位置向前 [ \033[1;31m%*d \033[1;37m- \033[1;31m%*d \033[1;37m]\033[m", fw[0],1,fw[1],current);
   sprintf(menustr[2],"\033[1;37m当前位置向后 [ \033[1;31m%*d \033[1;37m- \033[1;31m%*d \033[1;37m]\033[m", fw[0],current,fw[1],total);
   sprintf(menustr[3],"\033[1;37m全部         [ \033[1;31m%*d \033[1;37m- \033[1;31m%*d \033[1;37m]\033[m", fw[0],1,fw[1],total);

   memset(arg,0,sizeof(struct undelete_range_arg));
   arg->items=sel;
   arg->type=sub_menu;
   arg->fw=fw[1];

   memset(&conf,0,sizeof(struct _select_def));
   conf.item_count=4;
   conf.item_per_page=conf.item_count;
   conf.flag=LF_LOOP;
   conf.prompt="◇";
   conf.pos=1;
   conf.item_pos=pts;
   conf.arg=arg;
   conf.title_pos.x=-1;
   conf.title_pos.y=-1;
   conf.on_select=undelete_range_select;
   conf.show_data=undelete_range_show;
   conf.key_command=undelete_range_key;

   if(list_select_loop(&conf)!=SHOW_SELECT)
      return -1;

   switch(conf.pos){
      case 1:
         break;
      case 2:
         arg->id_from=1;
         arg->id_to=current;
         break;
      case 3:
         arg->id_from=current;
         arg->id_to=total;
         break;
      case 4:
         arg->id_from=1;
         arg->id_to=total;
         break;
      default:
         return -1;
   }
   return 0;
}


int undelete_range(struct _select_def *conf,struct fileheader *fhptr,void *varg)
{
#define UNDELETE_RANGE_ALLOWED_INTERVAL   20
#define UNDELETE_RANGE_QUIT(n,s)   do{move((n),4);prints("\033[1;33m%s\033[0;33m<Enter>\033[m",(s)); WAIT_RETURN; return FULLUPDATE; }while(0)
   sigset_t mask_set,old_mask_set;
   struct stat st_src;
   struct read_arg *rarg;
   struct undelete_range_arg arg;
   char buf[256],ans[4];
   const char *src,*type;
   int current,total,mode,ret;
   time_t timestamp;

   rarg=(struct read_arg*)conf->arg;
   total=rarg->filecount;
   current=((conf->pos>total)?total:(conf->pos));

   switch(rarg->mode){
      case DIR_MODE_DELETED:
         src=".DELETED";
         break;

      case DIR_MODE_JUNK:
         src=".JUNK";
         break;

      default:
         return DONOTHING;
   }

#ifdef MEMBER_MANAGER
	if(deny_del_article(currboard,&currmember,NULL,getSession()))
#else 
    if(deny_del_article(currboard,NULL,getSession()))
#endif
	return DONOTHING;

   timestamp=time(NULL);
   setbfile(buf,currboard->filename,src);

   if(stat(buf,&st_src)==-1||!S_ISREG(st_src.st_mode))
      return DONOTHING;

   clear();
   move(0,0);
   prints("\033[1;32m%s \033[1;33m%s\033[m","[区段恢复选单]","<Enter>键选择/<Esc>键退出");

   mode=undelete_range_interface_main_menu();
   switch(mode){
      case 1:
         type="常规区段恢复";
         break;
      case 2:
         type="强制区段恢复";
         break;
      default:
         UNDELETE_RANGE_QUIT(7,"操作取消...");
   }

   if(undelete_range_interface_sub_menu(current,total,&arg)==-1)
      UNDELETE_RANGE_QUIT(11,"操作取消...");

   move(12,4);
   sprintf(buf,"\033[1;33m%s \033[1;32m%d \033[1;37m- \033[1;32m%d \033[1;37m确认操作? [y/N] \033[m", type,arg.id_from,arg.id_to);
   prints("%s",buf);
   delete_range_read(ans,1,"ynYN");
   if(ans[0]!='Y' && ans[0]!='y')
      UNDELETE_RANGE_QUIT(14,"操作取消...");

   move(14,4);
   prints("\033[1;33m%s\033[m","区段操作可能需要较长时间, 请耐心等候...");
   refresh();
   sigemptyset(&mask_set);
   sigaddset(&mask_set,SIGHUP);
   sigaddset(&mask_set,SIGBUS);
   sigaddset(&mask_set,SIGPIPE);
   sigaddset(&mask_set,SIGTERM);
   sigprocmask(SIG_SETMASK,NULL,&old_mask_set);
   sigprocmask(SIG_BLOCK,&mask_set,NULL);

   ret=undelete_range_base(currboard->filename,src,arg.id_from,arg.id_to,mode,&st_src);

   newbbslog(BBSLOG_USER,"delete_range %s %d - %d <%d,%#04x>",currboard->filename,arg.id_from,arg.id_to,mode,ret);
   sigprocmask(SIG_SETMASK,&old_mask_set,NULL);
   if(ret==2){
      move(14,4);
      clrtoeol();
      prints("%s","\033[1;37m系统检测到在您操作期间版面文章列表已经发生变化, \033[m");
      move(15,4);
      sprintf(buf,"%s","\033[1;33m强制操作\033[1;37m[\033[1;31m严重不建议\033[1;37m]? [y/N] \033[m");
      prints("%s",buf);
      delete_range_read(ans,1,"ynYN");
      if(toupper(ans[0])!='Y')
         UNDELETE_RANGE_QUIT(17,"操作取消...");

      if((time(NULL)-timestamp)<UNDELETE_RANGE_ALLOWED_INTERVAL){
         sigprocmask(SIG_BLOCK,&mask_set,NULL);

         ret=undelete_range_base(currboard->filename,src,arg.id_from,arg.id_to,mode,NULL);

         newbbslog(BBSLOG_USER,"delete_range %s %d - %d <%d,%#04x>",currboard->filename,arg.id_from,arg.id_to,mode,ret);
         sigprocmask(SIG_SETMASK,&old_mask_set,NULL);
      }
      else{
         move(17,4);
         sprintf(buf,"\033[1;33m强制操作时限为 \033[1;31m%d \033[1;33m秒, "
               "您此次操作已经超时, 操作取消...\033[0;33m<Enter>\033[m",UNDELETE_RANGE_ALLOWED_INTERVAL);
         prints("%s",buf);
         WAIT_RETURN;
         return FULLUPDATE;
      }
   }
   bmlog(getCurrentUser()->userid,currboard->filename,9,
         (arg.id_to-arg.id_from+1)>0?arg.id_to-arg.id_from+1:0);/* 没有区段恢复这个记录项...... */
   move(15,4);
   clrtoeol();
   move(14,4);
   clrtoeol();
   if(ret==0){
      setboardmark(currboard->filename,1);
      setboardorigin(currboard->filename,1);
      updatelastpost(currboard->filename);
      prints("\033[1;32m%s\033[0;33m<Enter>\033[m","操作完成!");
      WAIT_RETURN;
      return DIRCHANGED;
   }

   switch(ret){
      case 1:
         type="打开文件错误！";
         break;
      case 2:
         type="版面记录有变动！";
         break;
      case 3:
         type="发生未知错误！";
         break;
   }
   
   sprintf(buf,"\033[1;33m%s\033[1;31m<%#04x>\033[0;33m<Enter>\033[m",type,ret);
   prints("%s",buf);
   WAIT_RETURN;
   return FULLUPDATE;
#undef UNDELETE_RANGE_ALLOWED_INTERVAL
#undef UNDELETE_RANGE_QUIT
}

/*--------------------------------end   区段恢复接口-------------------------------------*/
#endif /*BATCHRECOVERY*/

/*
 * 修改封禁理由
 */
int modify_deny_reason(char reason[][STRLEN], int *count, int pos, int type, POINT pts)
{
    char buf[STRLEN];
    int i;

    if (type==0) { /* 添加 */
        getdata(pts.y, pts.x, "请输入理由: ", buf, 30, DOECHO, NULL, true);
        remove_blank_ctrlchar(buf, buf, true, true, true);
        if (buf[0]=='\0')
            return 0;
        for (i=0;i<*count;i++) {
            if (strcasecmp(reason[i], buf)==0) {
                char ans[2];
                getdata(pts.y, pts.x, "输入理由已经存在", ans, 1, DOECHO, NULL, true);
                return 0;
            }
        }
        strcpy(reason[pos-1], buf);
        (*count)++;
        return pos+1;
    } else if (type==1) { /* 修改 */
        strcpy(buf, reason[pos-1]);
        getdata(pts.y+1, pts.x, "请输入理由: ", buf, 30, DOECHO, NULL, false);
        remove_blank_ctrlchar(buf, buf, true, true, true);
        if (buf[0]=='\0')
            return 0;
        for (i=0;i<*count;i++) {
            if (strcasecmp(reason[i], buf)==0 && i!=pos-1) {
                char ans[2];
                getdata(pts.y+1, pts.x, "输入理由已经存在", ans, 1, DOECHO, NULL, true);
                return 0;
            }
        }
        strcpy(reason[pos-1], buf);
        return pos;
    } else if (type==2) { /* 移动 */
        int newpos;
        getdata(pts.y+1, pts.x, "请输入新的位置: ", buf, 3, DOECHO, NULL, true);
        newpos = atoi(buf);
        if (newpos<=0 || newpos>(*count)) {
            getdata(pts.y+1, pts.x, "输入错误! ", buf, 1, DOECHO, NULL, true);
            return 0;
        } else if (newpos==pos)
            return 0;
        strcpy(buf, reason[pos-1]);
        if (newpos>pos) {
            for (i=pos;i<newpos;i++)
                strcpy(reason[i-1], reason[i]);
        } else {
            for (i=pos-1;i>=newpos;i--)
                strcpy(reason[i], reason[i-1]);
        }
        strcpy(reason[newpos-1], buf);
        return newpos;
    } else if (type==3) { /* 删除 */
        move(pts.y+1, pts.x);
        if (!askyn("确认删除", false))
            return 0;
        for (i=pos;i<(*count);i++) {
            strcpy(reason[i-1], reason[i]);
        }
        (*count)--;
        return pos;
    }
    return 0;
}

static int edit_denyreason_key(struct _select_def *conf,int key)
{
    struct _simple_select_arg *arg=(struct _simple_select_arg*)conf->arg;
    int i;
    if (key==KEY_ESC) {
        return SHOW_QUIT;
    }
    for (i=0; i<conf->item_count; i++)
        if (toupper(key)==toupper(arg->items[i].hotkey)) {
            conf->new_pos=i+1;
            return SHOW_SELCHANGE;
        }
    return SHOW_CONTINUE;
}
static int edit_denyreason_select(struct _select_def *conf)
{
    return SHOW_SELECT;
}
int edit_deny_reason(char reason[][STRLEN], int count, int max)
{
    struct _select_item sel[max+2];
    struct _select_def conf;
    struct _simple_select_arg arg;
    POINT pts[max+2];
    char menustr[max+2][STRLEN];
    int i, ret, ch, pos, item_count;

    pos = 1;
    while(1) {
        /* 当count==max时，不出现“添加”选项 */
        item_count = (count<max)? count+2:count+1;
        for (i=0;i<item_count;i++) {
            sel[i].x = (i<10) ? 2 : 40;
            sel[i].y = (i<10) ? 3 + i : i - 7;
            sel[i].hotkey = (i<9) ? '1' + i : 'A' + i - 9;
            sel[i].type = SIT_SELECT;
            sel[i].data = menustr[i];
            if (i==item_count-1) {
                sprintf(menustr[i], "\033[31m[%c] %s\033[m", sel[i].hotkey, "保存并退出");
            } else if (i==item_count-2 && count<max)
                sprintf(menustr[i], "\033[33m[%c] %s\033[m", sel[i].hotkey, "添加封禁理由");
            else
                sprintf(menustr[i], "[%c] %s", sel[i].hotkey, reason[i]);
            pts[i].x = sel[i].x;
            pts[i].y = sel[i].y;
        }
        sel[i].x = -1;
        sel[i].y = -1;
        sel[i].hotkey = -1;
        sel[i].type = 0;
        sel[i].data = NULL;

        arg.items = sel;
        arg.flag = SIF_SINGLE;
        bzero(&conf, sizeof(struct _select_def));
        conf.item_count = item_count;
        conf.item_per_page = MAXDENYREASON + 2;
        conf.flag = LF_LOOP | LF_MULTIPAGE;
        conf.prompt = "◆";
        conf.item_pos = pts;
        conf.arg = &arg;
        conf.pos = pos;
        conf.show_data = denyreason_show;
        conf.key_command = edit_denyreason_key;
        conf.on_select = edit_denyreason_select;

        move(0, 0);
        clrtoeol();
        prints("\033[32m修改封禁理由\033[m");
        while (1) {
            move(3, 0);
            clrtobot();
            conf.pos = pos;
            conf.flag=LF_LOOP;
            ret = list_select_loop(&conf);
            pos = conf.pos;
            if (ret==SHOW_SELECT) { /* 添加、修改或退出 */
                if (pos==conf.item_count) { /* 保存并退出 */
                    move(arg.items[conf.item_count-1].y, arg.items[conf.item_count-1].x);
                    prints("\033[31m确定保存并退出?[Y] \033[m");
                    ch=igetkey();
                    if(toupper(ch)=='N')
                        continue;
                    return count;
                } else if (pos==conf.item_count-1 && count<max) { /* 添加 */
                    ret=modify_deny_reason(reason, &count, pos, 0, pts[pos-1]);
                    if (ret==0)
                        continue;
                    pos=ret;
                    break;
                } else { /* 修改移动及删除 */
                    i=1;
                    ret=0;
                    while (1) {
                        move(arg.items[pos-1].y, arg.items[pos-1].x);
                        prints("%s[E.修改]%s[M.移动]%s[D.删除] \033[33m%s\033[m",
                                i==1?"\033[32m":"\033[m", i==2?"\033[32m":"\033[m", i==3?"\033[32m":"\033[m", reason[pos-1]);
                        ch=igetkey();
                        if (ch==KEY_RIGHT || ch==KEY_TAB) {
                            i++;
                            if (i>3)
                                i=1;
                        } else if (ch==KEY_LEFT) {
                            i--;
                            if (i<1)
                                i=3;
                        } else if (toupper(ch)=='E') {
                            i=1;
                        } else if (toupper(ch)=='M') {
                            i=2;
                        } else if (toupper(ch)=='D') {
                            i=3;
                        } else if (ch=='\n'||ch=='\r') {
                            ret=modify_deny_reason(reason, &count, pos, i, pts[pos-1]);
                            break;
                        } else if (ch==KEY_ESC) {
                            break;
                        }
                    }
                    if (ret==0)
                        continue;
                    pos=ret;
                    break;
                }
            } else {/* ESC退出 */
                move(arg.items[conf.item_count-1].y, arg.items[conf.item_count-1].x);
                prints("\033[31m放弃保存并退出?[N] \033[m");
                ch=igetkey();
                if(toupper(ch)!='Y')
                    continue;
                return -1;
            }
        }
    }
}

int b_reason_edit() {
    char reason[MAXDENYREASON][STRLEN];
    int i, count;

#ifdef MEMBER_MANAGER
	if (!check_board_member_manager(&currmember, currboard, BMP_NOTE))
#else
    if (!chk_currBM(currBM, getCurrentUser())) 
#endif
        return 0;
    
    clear();

    for (i=0;i<MAXDENYREASON;i++)
        reason[i][0]='\0';
    count = get_deny_reason(currboard->filename, reason, MAXCUSTOMREASON);
    count = edit_deny_reason(reason, count, MAXCUSTOMREASON);
    if (count!=-1)
        save_deny_reason(currboard->filename, reason, count);
    return SHOW_REFRESH;
}

#ifdef TITLEKEYWORD
/*
 * 修改标题关键字
 */
int modify_title_key(char key[][8], int *count, int pos, int type, POINT pts)
{
    char buf[STRLEN];
    int i;
    
    if (type==0) { /* 添加 */
        getdata(pts.y, pts.x, "请输入关键字: ", buf, 7, DOECHO, NULL, true);
        remove_blank_ctrlchar(buf, buf, true, true, true);
        if (buf[0]=='\0')
            return 0;
        for (i=0;i<*count;i++) {
            if (strcasecmp(key[i], buf)==0) {
                char ans[2];
                getdata(pts.y, pts.x, "输入关键字已经存在", ans, 1, DOECHO, NULL, true);
                return 0;
            }   
        }   
        strcpy(key[pos-1], buf);
        (*count)++;
        return pos+1;
    } else if (type==1) { /* 修改 */
        strcpy(buf, key[pos-1]);
        getdata(pts.y+1, pts.x, "请输入关键字: ", buf, 7, DOECHO, NULL, false);
        remove_blank_ctrlchar(buf, buf, true, true, true);
        if (buf[0]=='\0')
            return 0;
        for (i=0;i<*count;i++) {
            if (strcasecmp(key[i], buf)==0 && i!=pos-1) {
                char ans[2];
                getdata(pts.y+1, pts.x, "输入关键字已经存在", ans, 1, DOECHO, NULL, true);
                return 0;
            }
        }
        strcpy(key[pos-1], buf);
        return pos;
    } else if (type==2) { /* 移动 */
        int newpos;
        getdata(pts.y+1, pts.x, "请输入新的位置: ", buf, 3, DOECHO, NULL, true);
        newpos = atoi(buf);
        if (newpos<=0 || newpos>(*count)) {
            getdata(pts.y+1, pts.x, "输入错误! ", buf, 1, DOECHO, NULL, true);
            return 0;
        } else if (newpos==pos)
            return 0;
        strcpy(buf, key[pos-1]);
        if (newpos>pos) {
            for (i=pos;i<newpos;i++)
                strcpy(key[i-1], key[i]);
        } else {
            for (i=pos-1;i>=newpos;i--)
                strcpy(key[i], key[i-1]);
        }
        strcpy(key[newpos-1], buf);
        return newpos;
    } else if (type==3) { /* 删除 */
        move(pts.y+1, pts.x);
        if (!askyn("确认删除", false))
            return 0;
        for (i=pos;i<(*count);i++) {
            strcpy(key[i-1], key[i]);
        }
        (*count)--;
        return pos;
    }
    return 0;
}

static int edit_titkey_key(struct _select_def *conf,int key)
{
    struct _simple_select_arg *arg=(struct _simple_select_arg*)conf->arg;
    int i;
    if (key==KEY_ESC) {
        return SHOW_QUIT;
    }
    for (i=0; i<conf->item_count; i++)
        if (toupper(key)==toupper(arg->items[i].hotkey)) {
            conf->new_pos=i+1;
            return SHOW_SELCHANGE;
        }
    return SHOW_CONTINUE;
}
static int titkey_show(struct _select_def *conf,int i)
{   
    struct _simple_select_arg *arg=(struct _simple_select_arg*)conf->arg;
    outs((char*)((arg->items[i-1]).data));
    return SHOW_CONTINUE;
}
static int edit_titkey_select(struct _select_def *conf)
{
    return SHOW_SELECT;
}
int edit_title_key(char key[][8], int count, int max)
{
    struct _select_item sel[max+2];
    struct _select_def conf;
    struct _simple_select_arg arg;
    POINT pts[max+2];
    char menustr[max+2][STRLEN];
    int i, ret, ch, pos, item_count;

    pos = 1;
    while(1) {
        /* 当count==max时，不出现“添加”选项 */
        item_count = (count<max)? count+2:count+1;
        for (i=0;i<item_count;i++) {
            sel[i].x = (i<10) ? 2 : 40;
            sel[i].y = (i<10) ? 3 + i : i - 7;
            sel[i].hotkey = (i<9) ? '1' + i : 'A' + i - 9;
            sel[i].type = SIT_SELECT;
            sel[i].data = menustr[i];
            if (i==item_count-1) {
                sprintf(menustr[i], "\033[31m[%c] %s\033[m", sel[i].hotkey, "保存并退出");
            } else if (i==item_count-2 && count<max)
                sprintf(menustr[i], "\033[33m[%c] %s\033[m", sel[i].hotkey, "添加标题关键字");
            else
                sprintf(menustr[i], "[%c] %s", sel[i].hotkey, key[i]);
            pts[i].x = sel[i].x;
            pts[i].y = sel[i].y;
        }
        sel[i].x = -1;
        sel[i].y = -1;
        sel[i].hotkey = -1;
        sel[i].type = 0;
        sel[i].data = NULL;

        arg.items = sel;
        arg.flag = SIF_SINGLE;
        bzero(&conf, sizeof(struct _select_def));
        conf.item_count = item_count;
        conf.item_per_page = MAXBOARDTITLEKEY + 2;
        conf.flag = LF_LOOP | LF_MULTIPAGE;
        conf.prompt = "◆";
        conf.item_pos = pts;
        conf.arg = &arg;
        conf.pos = pos;
        conf.show_data = titkey_show;
        conf.key_command = edit_titkey_key;
        conf.on_select = edit_titkey_select;

        move(0, 0);
        clrtoeol();
        prints("\033[32m修改标题关键字\033[m");
        while (1) {
            move(3, 0);
            clrtobot();
            conf.pos = pos;
            conf.flag=LF_LOOP;
            ret = list_select_loop(&conf);
            pos = conf.pos;
            if (ret==SHOW_SELECT) { /* 添加、修改或退出 */
                if (pos==conf.item_count) { /* 保存并退出 */
                    move(arg.items[conf.item_count-1].y, arg.items[conf.item_count-1].x);
                    prints("\033[31m确定保存并退出?[Y] \033[m");
                    ch=igetkey();
                    if(toupper(ch)=='N')
                        continue;
                    return count;
                } else if (pos==conf.item_count-1 && count<max) { /* 添加 */
                    ret=modify_title_key(key, &count, pos, 0, pts[pos-1]);
                    if (ret==0)
                        continue;
                    pos=ret;
                    break;
                } else { /* 修改移动及删除 */
                    i=1;
                    ret=0;
                    while (1) {
                        move(arg.items[pos-1].y, arg.items[pos-1].x);
                        prints("%s[E.修改]%s[M.移动]%s[D.删除] \033[33m%s\033[m",
                                i==1?"\033[32m":"\033[m", i==2?"\033[32m":"\033[m", i==3?"\033[32m":"\033[m", key[pos-1]);
                        ch=igetkey();
                        if (ch==KEY_RIGHT || ch==KEY_TAB) {
                            i++;
                            if (i>3)
                                i=1;
                        } else if (ch==KEY_LEFT) {
                            i--;
                            if (i<1)
                                i=3;
                        } else if (toupper(ch)=='E') {
                            i=1;
                        } else if (toupper(ch)=='M') {
                            i=2;
                        } else if (toupper(ch)=='D') {
                            i=3;
                        } else if (ch=='\n'||ch=='\r') {
                            ret=modify_title_key(key, &count, pos, i, pts[pos-1]);
                            break;
                        } else if (ch==KEY_ESC) {
                            break;
                        }
                    }
                    if (ret==0)
                        continue;
                    pos=ret;
                    break;
                }
            } else {/* ESC退出 */
                move(arg.items[conf.item_count-1].y, arg.items[conf.item_count-1].x);
                prints("\033[31m放弃保存并退出?[N] \033[m");
                ch=igetkey();
                if(toupper(ch)!='Y')
                    continue;
                return -1;
            }
        }
    }
}

int b_titkey_edit() {
    char key[MAXBOARDTITLEKEY][8];
    int i, count;

#ifdef MEMBER_MANAGER
	if (!check_board_member_manager(&currmember, currboard, BMP_NOTE))
#else
    if (!chk_currBM(currBM, getCurrentUser())) 
#endif	
        return 0;
    
    clear();
    
    for (i=0;i<MAXBOARDTITLEKEY;i++)
        key[i][0]='\0';
    count = get_title_key(currboard->filename, key, MAXBOARDTITLEKEY);
    count = edit_title_key(key, count, MAXBOARDTITLEKEY);
    if (count!=-1)
        save_title_key(currboard->filename, key, count);
    return SHOW_REFRESH;
}
#endif
