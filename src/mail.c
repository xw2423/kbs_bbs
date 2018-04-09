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
#include "read.h"

#define INTERNET_PRIVATE_EMAIL

int G_SENDMODE = false;
int gShowSize = false;
int cmpinames();                /* added by Leeward 98.04.10 */

extern char quote_user[];
struct _mail_list user_mail_list;

#define maxrecp 300

static int mail_reply(int ent,struct fileheader* fileinfo,char* direct);
static int mail_del(struct _select_def* conf, struct fileheader *fileinfo,void* extraarg);
static int do_mail_reply(struct _select_def* conf, struct fileheader *fileinfo,void* extraarg);
extern int do_cross(struct _select_def* conf,struct fileheader *fileinfo,void* extraarg);

int chkmail()
{
    static time_t lasttime = 0;
    static int ismail = 0;
    struct fileheader fh;
    struct stat st;
    int fd;
    int offset;
    long numfiles;
    unsigned char ch;
    char curmaildir[STRLEN];

    if (CHECK_UENT(uinfo.uid)) {
        uinfo.mailcheck = utmpshm->uinfo[ getSession()->utmpent - 1 ].mailcheck;
    } else
        uinfo.mailcheck = 0;

    if (uinfo.mailcheck & CHECK_MAIL) {
        return ismail;
    }

    utmpshm->uinfo[ getSession()->utmpent - 1 ].mailcheck |= CHECK_MAIL;
    uinfo.mailcheck |= CHECK_MAIL;

    setmailfile(curmaildir, getCurrentUser()->userid, DOT_DIR);

    if (!HAS_PERM(getCurrentUser(), PERM_BASIC)) {
        return 0;
    }
    /*
     * ylsdd 2001.4.23: 检测文件状态应该在get_mailnum，get_sum_records之前，否则岂不是
     * 要做大量无用的系统调用. 在这个改动中也把fstat改为stat了，节省一个open&close
     */
    if (stat(curmaildir, &st) < 0)
#ifdef ENABLE_REFER
        return (ismail = chkrefer()); // modified by windinsn, Jan 28, 2012, 增加@和回复提醒
#else
        return (ismail = 0);
#endif
    /*
    if (lasttime >= st.st_mtime)
        return ismail;
    */

    if (chkusermail(getCurrentUser()))
        return (ismail = 2);
    offset = (int)((char *) &(fh.accessed[0]) - (char *) &(fh));
    if ((fd = open(curmaildir, O_RDONLY)) < 0)
#ifdef ENABLE_REFER
        return (ismail = chkrefer()); // modified by windinsn, Jan 28, 2012, 增加@和回复提醒
#else
        return (ismail = 0);
#endif

    lasttime = st.st_mtime;
    numfiles = st.st_size;
    numfiles = numfiles / sizeof(fh);
    if (numfiles <= 0) {
        close(fd);
#ifdef ENABLE_REFER
        return (ismail = chkrefer()); // modified by windinsn, Jan 28, 2012, 增加@和回复提醒
#else
        return (ismail = 0);
#endif
    }
    /* 只判断最后一篇 */
    lseek(fd, (st.st_size - (sizeof(fh) - offset)), SEEK_SET);
    read(fd, &ch, 1);
    if (!(ch & FILE_READ)) {
        close(fd);
        return (ismail = 1);
    }
    close(fd);
#ifdef ENABLE_REFER
        return (ismail = chkrefer()); // modified by windinsn, Jan 28, 2012, 增加@和回复提醒
#else
        return (ismail = 0);
#endif
}

#ifdef ENABLE_REFER
/*
 * added by windinsn, Jan 28, 2012
 * 检查用户是否有新的 @提醒 或 回复提醒
 *
 * 0: 无
 * 3: 有新的 @
 * 4: 有新的回复
 * 5: 有新的短信, added by windinsn
 * 6: 有新的Like, added by windinsn, 2013-4-11
 */
int chkrefer() {
#ifdef ENABLE_NEW_MSG
    int ret;
    struct new_msg_handle handle;

    ret=0;
    new_msg_init(&handle, getCurrentUser());
    if (new_msg_open(&handle)==0) {
        ret=new_msg_check_new(&handle);
        new_msg_close(&handle);
    }

    if (ret==1)
        return 5;
#endif 
    /* 需要更新一下uinfo中的记录 */
    sync_refer_info(REFER_MODE_AT, 1);
    sync_refer_info(REFER_MODE_REPLY, 1);
#ifdef ENABLE_REFER_LIKE
    sync_refer_info(REFER_MODE_LIKE, 1);
#endif
    if (DEFINE(getCurrentUser(), DEF_REFER)&&check_refer_info(REFER_MODE_AT)>0/*chkrefer_dir(REFER_DIR)>0*/)
        return 3;
    if (DEFINE(getCurrentUser(), DEF_REPLY)&&check_refer_info(REFER_MODE_REPLY)>0/*chkrefer_dir(REPLY_DIR)>0*/)
        return 4;
#ifdef ENABLE_REFER_LIKE
    if (DEFINE(getCurrentUser(), DEF_REPLY)&&check_refer_info(REFER_MODE_LIKE)>0/*chkrefer_dir(LIKE_DIR)>0*/)
        return 6;
#endif

    return 0; 
}
int chkrefer_dir(char *filename) {
    char dir[STRLEN];
    struct stat st;
    int offset, fd, num;
    struct refer refer;
    unsigned char ch;

    sethomefile(dir, getCurrentUser()->userid, filename);
    if (stat(dir, &st)<0)
        return 0;

    offset=(int)((char *) &(refer.flag)-(char *) &(refer));
    if ((fd=open(dir, O_RDONLY))<0)
        return 0;
    num=st.st_size/sizeof(refer);
    if (num<=0) {
        close(fd);
        return 0;
    }
    
    lseek(fd, (st.st_size-(sizeof(refer)-offset)), SEEK_SET);
    read(fd, &ch, 1);
    close(fd);
    if (!(ch&FILE_READ))
        return num;
    return 0;
}
#endif

int get_mailnum(char* direct)
{
    struct fileheader fh;
    struct stat st;
    int fd;
    register int numfiles;

    if ((fd = open(direct, O_RDONLY)) < 0)
        return 0;
    fstat(fd, &st);
    numfiles = st.st_size;
    numfiles = numfiles / sizeof(fh);
    close(fd);
    return numfiles;
}


static int mailto(struct userec *uentp, void *arg)
{
    char filename[STRLEN];
    int mailmode = (POINTDIFF) arg;

    sprintf(filename, "etc/%s.mailtoall", getCurrentUser()->userid);
    if ((uentp->userlevel == PERM_BASIC && mailmode == 1) ||
            (!HAS_PERM(uentp, PERM_DENYMAIL) && mailmode == 2) || (uentp->userlevel & PERM_BOARDS && mailmode == 3) || (uentp->userlevel & PERM_CHATCLOAK && mailmode == 4)) {
        mail_file(getCurrentUser()->userid, filename, uentp->userid, save_title, 0, NULL);
    }
    return 1;
}

static int mailtoall(POINTDIFF mode)
{

    return apply_users(mailto, (void *) mode);
}

int mailall(void)
{
    char ans[4], ans4[4], ans2[4], fname[STRLEN], title[STRLEN];
    char doc[4][STRLEN], buf[STRLEN];
    char buf2[STRLEN], include_mode = 'Y';
    char buf3[STRLEN], buf4[STRLEN];
    int i, replymode = 0;       /* Post New UI */
    int gdataret;

    strcpy(title, "没主题");
    buf4[0] = '\0';
    modify_user_mode(SMAIL);
    clear();
    move(0, 0);
    sprintf(fname, "etc/%s.mailtoall", getCurrentUser()->userid);
    prints("你要寄给所有的：\n");
    prints("(0) 放弃\n");
    strcpy(doc[0], "(1) 未认证身份者");
    strcpy(doc[1], "(2) 已认证身份者");
    strcpy(doc[2], "(3) 有版主权限者");
    strcpy(doc[3], "(4) 智囊团成员");
    for (i = 0; i < 4; i++)
        prints("%s\n", doc[i]);
    while (1) {
        getdata(8, 0, "请输入模式 (0~4)? [0]: ", ans4, 2, DOECHO, NULL, true);

        move(9,0);
        clrtoeol();
        if (!askyn("真的要群信么", 0)) {
            clear();
            return -1;
        }
        if (ans4[0] - '0' >= 1 && ans4[0] - '0' <= 4) {
            sprintf(buf, "是否确定寄给%s (Y/N)? [N]: ", doc[ans4[0] - '0' - 1]);
            getdata(9, 0, buf, ans2, 2, DOECHO, NULL, true);
            if (ans2[0] != 'Y' && ans2[0] != 'y') {
                return -1;
            }
            in_mail = true;
            /*
             * Leeward 98.01.17 Prompt whom you are writing to
             */
            /*
             * strcpy(currentlookupuser->userid, doc[ans4[0]-'0'-1] + 4);
             */

            if (getCurrentUser()->signature > getSession()->currentmemo->ud.signum)
                getCurrentUser()->signature = 1;
            while (1) {
                sprintf(buf3, "引言模式 [\033[1m%c\033[m]", include_mode);
                move(t_lines - 4, 0);
                clrtoeol();
                prints("收信人: \033[1m%s\033[m\n", doc[ans4[0] - '0' - 1]);
                clrtoeol();
                prints("使用标题: \033[1m%-50s\033[m\n", (title[0] == '\0') ? "[正在设定标题]" : title);
                clrtoeol();
                if (getCurrentUser()->signature < 0)
                    prints("使用随机签名档     %s", (replymode) ? buf3 : "");
                else
                    prints("使用第 \033[1m%d\033[m 个签名档     %s", getCurrentUser()->signature, (replymode) ? buf3 : "");

                if (buf4[0] == '\0' || buf4[0] == '\n') {
                    move(t_lines - 1, 0);
                    clrtoeol();
                    gdataret = getdata(t_lines - 1, 0, "标题: ", buf4, 50, DOECHO, NULL, true);
                    if (gdataret == -1) return -2;
                    if ((buf4[0] == '\0' || buf4[0] == '\n')) {
                        buf4[0] = ' ';
                        continue;
                    }
                    strcpy(title, buf4);
                    continue;
                }
                move(t_lines - 1, 0);
                clrtoeol();
                /*
                 * Leeward 98.09.24 add: viewing signature(s) while setting post head
                 */
                sprintf(buf2, "按\033[1;32m0\033[m~\033[1;32m%d/V/L\033[m选/看/随机签名档%s，\033[1;32mT\033[m改标题，\033[1;32mEnter\033[m接受所有设定: ", getSession()->currentmemo->ud.signum,
                        (replymode) ? "，\033[1;32mY\033[m/\033[1;32mN\033[m/\033[1;32mR\033[m/\033[1;32mA\033[m改引言模式" : "");
                gdataret = getdata(t_lines - 1, 0, buf2, ans, 3, DOECHO, NULL, true);
                if (gdataret == -1) return -2;
                ans[0] = toupper(ans[0]);       /* Leeward 98.09.24 add; delete below toupper */
                if ((ans[0] - '0') >= 0 && ans[0] - '0' <= 9) {
                    if (atoi(ans) <= getSession()->currentmemo->ud.signum)
                        getCurrentUser()->signature = atoi(ans);
                } else if ((ans[0] == 'Y' || ans[0] == 'N' || ans[0] == 'A' || ans[0] == 'R') && replymode) {
                    include_mode = ans[0];
                } else if (ans[0] == 'T') {
                    buf4[0] = '\0';
                } else if (ans[0] == 'V') {     /* Leeward 98.09.24 add: viewing signature(s) while setting post head */
                    sethomefile(buf2, getCurrentUser()->userid, "signatures");
                    move(t_lines - 1, 0);
                    if (askyn("预设显示前三个签名档, 要显示全部吗", false) == true)
                        ansimore(buf2, 0);
                    else {
                        clear();
                        ansimore2(buf2, false, 0, 18);
                    }
                } else if (ans[0] == 'L') {
                    getCurrentUser()->signature = -1;
                } else {
                    extern char quote_title[120];

                    strncpy(save_title, title, STRLEN);
                    strncpy(quote_title, title, STRLEN);
                    break;
                }
            }
            do_quote(fname, include_mode, "", quote_user);
            if (vedit(fname, true, NULL, NULL,0) == -1) {
                in_mail = false;
                unlink(fname);
                clear();
                return -2;
            }
            move(t_lines - 1, 0);
            clrtoeol();
            prints("\033[32m\033[44m正在寄信件中，请稍候.....                                                        \033[m");
            mailtoall(ans4[0] - '0');
            move(t_lines - 1, 0);
            clrtoeol();
            unlink(fname);
            in_mail = false;
            return 0;
        } else
            in_mail = false;
        return 0;
    }
}

int m_internet(void)
{
    char receiver[STRLEN], title[STRLEN];
    int gdataret;

    modify_user_mode(SMAIL);
    gdataret = getdata(1, 0, "收信人: ", receiver, 70, DOECHO, NULL, true);
    if (gdataret == -1) return -2;

    gdataret = getdata(2, 0, "主题  : ", title, 70, DOECHO, NULL, true);
    if (gdataret == -1) return -2;

    if (!invalidaddr(receiver) && strchr(receiver, '@') && strlen(title) > 0) {
        clear();                /* Leeward 98.09.24fix a bug */
        switch (do_send(receiver, title, "")) { /* Leeward 98.05.11 adds "switch" */
            case -1:
                prints("收信者不正确\n");
                break;
            case -2:
                prints("取消发信\n");
                break;
            case -3:
                prints("'%s' 无法收信\n", receiver);
                break;
            case -4:
                clear();
                move(1, 0);
                prints("%s 信箱已满,无法收信\n", receiver);
                break;              /*Haohmaru.4.5.收信限制 */
            case -5:
                clear();
                move(1, 0);
                prints("%s 自杀中，不能收信\n", receiver);
                break;              /*Haohmaru.99.10.26.自杀者不能收信 */
            case -552:
                prints("\n\033[1m\033[33m信件超长（本站限定信件长度上限为 %d 字节），取消发信操作\033[m\033[m\n", MAXMAILSIZE);
                break;
            default:
                prints("信件已寄出\n");
        }
        pressreturn();

    } else {
        move(3, 0);
        prints("收信人或主题不正确, 请重新选取指令\n");
        pressreturn();
    }
    clear();
    return 0;
}


/* 返回值定义:
  *   -1  收信者不存在;  -2 取消发信;
  *   -3  无法收信   -4 信箱已满
  *   -5  自杀中无法收信    -552 信件超长?
  *    (错误代码统一定义才好，不然太混乱，维护很不爽。:(  )
  */
int do_send(char *userid, char *title, char *q_file)
{
    struct fileheader newmessage;
    struct stat st;
    char filepath[STRLEN], fname[STRLEN];
    char titlebuf[STRLEN], buf2[256], buf3[STRLEN], buf4[STRLEN];
    int replymode = 1;          /* Post New UI */
    char ans[4], include_mode = 'Y';

    int internet_mail = 0;
#ifdef INTERNET_PRIVATE_EMAIL
    char tmp_fname[256];
    int noansi;
#endif
    struct userec *user;
    extern char quote_title[120];
    int ret;
    int gdataret;
    char* upload = NULL;
    int savesent = HAS_MAILBOX_PROP(&uinfo, MBP_SAVESENTMAIL);

    if (!strchr(userid, '@')) {
        if (getuser(userid, &user) == 0)
            return -1;
#ifdef HAVE_USERSCORE
        /* 直接发信或版面回信需要进行积分检查 */
        if ((*q_file=='\0' || strncmp(q_file, "mail", 4)) && !sufficient_score_to_sendmail(getCurrentUser(), user->userid)) {
            prints("\033[1m\033[33m您积分不足，不能给 %s 发信。\033[m\n\n", user->userid);
            return -2;
        }
#endif
        /* fancyrabbit Sep 14 2007, fixed replied mail lost bug after modifying ID-case */
        sprintf(userid, user -> userid);
        ret = check_mail_perm(getCurrentUser(), user);

        switch (ret) {
            case 1:
                return -3;
            case 2:
                move(1, 0);
                prints("你的信箱容量超出上限, 无法发送信件。");
                pressreturn();
                return -2;
            case 3:
                return -4;
            case 4:  /* Leeward 98.04.10 */
                prints("\033[1m\033[33m很抱歉∶您无法给 %s 发信．因为 %s 拒绝接收您的信件．\033[m\033[m\n\n", userid,userid);
                return -2;
            case 5:
                prints("\033[1m\033[33m很抱歉∶您无法发信．因为 您被封禁了Mail权限。\n\033[m");
                return -2;
            case 6:
                /* 新人 */
                break;
        }
    }
#ifdef INTERNET_PRIVATE_EMAIL
    /*
     * I hate go to , but I use it again for the noodle code :-)
     */
    else {
        /*
         * if(!strstr(userid,"edu.tw")){
         * if(strstr(userid,"@bbs.ee.nthu."))
         * strcat(userid,"edu.tw");
         * else
         * strcat(userid,".edu.tw");}
         */
#ifdef HAVE_USERSCORE
        if (!sufficient_score_to_sendmail(getCurrentUser(), NULL)) {
            prints("\033[1m\033[33m您积分不足，不能给 %s 发信。\033[m\n\n", userid);
            return -2;
        }
#endif
        switch (check_mail_perm(getCurrentUser(), NULL)) {
            case 5:
            case 6:
                prints("\033[1m\033[33m很抱歉∶您无法发信．因为 您被封禁了Mail权限。\n\033[m");
                return -2;
            case 2:
                move(1, 0);
                prints("你的信箱容量超出上限, 无法发送信件。");
                pressreturn();
                return -2;
        }
        internet_mail = 1;
        modify_user_mode(IMAIL);
        buf4[0] = ' ';
        gettmpfilename(tmp_fname, "bbs-internet-gw");
        //sprintf(tmp_fname, "tmp/bbs-internet-gw/%05d", getpid());
        strcpy(filepath, tmp_fname);
        goto edit_mail_file;
    }
    /*
     * end of kludge for internet mail
     */
#endif

    setmailpath(filepath, userid);
    if (stat(filepath, &st) == -1) {
        if (mkdir(filepath, 0755) == -1)
            return -1;
    } else {
        if (!(st.st_mode & S_IFDIR))
            return -1;
    }

    memset(&newmessage, 0, sizeof(newmessage));
    GET_MAILFILENAME(fname, filepath);
    strcpy(newmessage.filename, fname);


    /*
     * strncpy(newmessage.title,title,STRLEN) ;
     */
    in_mail = true;
#if defined(MAIL_REALNAMES)
    sprintf(genbuf, "%s (%s)", getCurrentUser()->userid, getCurrentUser()->realname);
#else
    /*sprintf(genbuf,"%s (%s)",getCurrentUser()->userid,getCurrentUser()->username) ;*/
    strcpy(genbuf, getCurrentUser()->userid);        /* Leeward 98.04.14 */
#endif
    strncpy(newmessage.owner, genbuf, OWNER_LEN);
    newmessage.owner[OWNER_LEN - 1] = 0;

    setmailfile(filepath, userid, fname);

#ifdef INTERNET_PRIVATE_EMAIL
edit_mail_file:
#endif
    if (!title) {
        replymode = 0;
        buf4[0] = '\0';
        titlebuf[0] = '\0';
    } else {
        buf4[0] = ' ';
        strncpy(titlebuf, title, sizeof(titlebuf) - 1);
        titlebuf[sizeof(titlebuf) - 1] = '\0';
    }
    title = titlebuf;

    if (getCurrentUser()->signature > getSession()->currentmemo->ud.signum)
        getCurrentUser()->signature = (getSession()->currentmemo->ud.signum == 0) ? 0 : 1;
    while (1) {
        if (replymode) {
            sprintf(buf3, "     引言模式 [\033[1m%c\033[m]", include_mode);
        } else {
            buf3[0] = '\0';
        }
        if (internet_mail || buf4[0] == '\0' || buf4[0] == '\n') {
            buf2[0] = '\0';
        } else {
            sprintf(buf2, "     %s保存到发件箱[\033[1;32ms\033[m切换]", savesent ? "" : "不");
        }
        move(t_lines - 4, 0);
        clrtoeol();
        prints("收信人: \033[1m%s\033[m\n", userid);
        clrtoeol();
        prints("使用标题: \033[1m%-50s\033[m\n", (titlebuf[0] == '\0') ? "没主题" : titlebuf);
        clrtoeol();
        if (getCurrentUser()->signature < 0)
            prints("使用随机签名档%s%s", buf3, buf2);
        else
            prints("使用第 \033[1m%d\033[m 个签名档%s%s", getCurrentUser()->signature, buf3, buf2);

        if (buf4[0] == '\0' || buf4[0] == '\n') {
            move(t_lines - 1, 0);
            clrtoeol();
            strcpy(buf4, titlebuf);
            gdataret = getdata(t_lines - 1, 0, "标题: ", buf4, 79, DOECHO, NULL, false);
            if (gdataret == -1) return -2;
            if ((buf4[0] != '\0' && buf4[0] != '\n')) {
                strcpy(titlebuf, buf4);
            } else {
                buf4[0] = ' ';
            }
            continue;
        }
        move(t_lines - 1, 0);
        clrtoeol();
        /*
         * Leeward 98.09.24 add: viewing signature(s) while setting post head
         */
        sprintf(buf2, "按 \033[1;32m0\033[m~\033[1;32m%d/V/L\033[m选/看/随机签名档%s，\033[1;32mT\033[m标题，\033[1;32mQ\033[m取消，\033[1;32mEnter\033[m接受所有设定: ", getSession()->currentmemo->ud.signum,
                (replymode) ? "，\033[1;32mY\033[m/\033[1;32mN\033[m/\033[1;32mR\033[m/\033[1;32mA\033[m引言模式" : "");
        gdataret = getdata(t_lines - 1, 0, buf2, ans, 3, DOECHO, NULL, true);
        if (gdataret == -1) return -2;
        ans[0] = toupper(ans[0]);       /* Leeward 98.09.24 add; delete below toupper */
        if ((ans[0] - '0') >= 0 && ans[0] - '0' <= 9) {
            if (atoi(ans) <= getSession()->currentmemo->ud.signum)
                getCurrentUser()->signature = atoi(ans);
        } else if ((ans[0] == 'Y' || ans[0] == 'N' || ans[0] == 'A' || ans[0] == 'R') && replymode) {
            include_mode = ans[0];
        } else if (ans[0] == 'T') {
            buf4[0] = '\0';
        } else if (ans[0] == 'Q') {
            return -2;
        } else if (ans[0] == 'S') {
            savesent = !savesent;
        } else if (ans[0] == 'L') {
            getCurrentUser()->signature = -1;
        } else if (ans[0] == 'V') {     /* Leeward 98.09.24 add: viewing signature(s) while setting post head */
            sethomefile(buf2, getCurrentUser()->userid, "signatures");
            move(t_lines - 1, 0);
            if (askyn("预设显示前三个签名档, 要显示全部吗", false) == true)
                ansimore(buf2, 0);
            else {
                clear();
                ansimore2(buf2, false, 0, 18);
            }
        } else if (ans[0] == 'U'&&HAS_PERM(getCurrentUser(), PERM_SYSOP)) {
            chdir("tmp");
            if (upload != NULL) unlink(upload);
            upload = bbs_zrecvfile();
            chdir("..");
        } else {
            if (titlebuf[0] == '\0') {
                strcpy(titlebuf, "没主题");
            }
            strnzhcpy(newmessage.title, titlebuf, ARTICLE_TITLE_LEN);
            strncpy(save_title, newmessage.title, ARTICLE_TITLE_LEN - 1);
            save_title[ARTICLE_TITLE_LEN-1] = 0;
            break;
        }
    }

    do_quote(filepath, include_mode, q_file, quote_user);
    strncpy(quote_title, newmessage.title, ARTICLE_TITLE_LEN - 1);
    quote_title[ARTICLE_TITLE_LEN - 1] = 0;

#ifdef INTERNET_PRIVATE_EMAIL
    if (internet_mail) {
        int res, ch;

        if (vedit(filepath, false, NULL, NULL,0) == -1) {
            unlink(filepath);
            clear();
            return -2;
        }
        clear();
redo:
        prints("信件即将寄给 %s \n", userid);
        prints("标题为： %s \n", titlebuf);
        prints("确定要寄出吗? (Y/N) [Y]");
        ch = igetkey();
        switch (ch) {
            case KEY_REFRESH:
                move(3, 0);
                goto redo;
            case 'N':
            case 'n':
                prints("%c\n", 'N');
                prints("\n信件已取消...\n");
                res = -2;
                break;
            default: {
                /*
                 * uuencode or convert to big5 option -- Add by ming, 96.10.9
                 */
                char data[3];
                int isbig5;

                prints("%c\n", 'Y');
                if (askyn("是否备份给自己", false) == true)
                    mail_file(getCurrentUser()->userid, tmp_fname, getCurrentUser()->userid, save_title, 0, NULL);

                data[0] = '\0';
                prints("若您要将信件转寄到台湾请输入 Y 或 y\n");
                getdata(7, 0, "转成BIG5码? [N]: ", data, 2, DOECHO, 0, 0);
                if (data[0] == 'y' || data[0] == 'Y')
                    isbig5 = 1;
                else
                    isbig5 = 0;

                data[0] = '\0';
                getdata(8, 0, "过滤ANSI控制符? [Y]: ", data, 2, DOECHO, 0, 0);
                if (data[0] == 'N' || data[0] == 'n')
                    noansi = 0;
                else
                    noansi = 1;

                prints("请稍候, 信件传递中...\n");
#ifdef HAVE_USERSCORE
                if (!sufficient_score_to_sendmail(getCurrentUser(), NULL)) {
                    prints("\033[1m\033[33m很抱歉∶系统无法发出此信．因为您的积分不足．\033[m\033[m\n\n");
                    sprintf(save_title, "退信∶信件发送失败．");
                    mail_file(getCurrentUser()->userid, filepath, getCurrentUser()->userid, save_title, BBSPOST_MOVE, NULL);
                    return -2;
                }
#endif
                res = bbs_sendmail(tmp_fname, title, userid, isbig5, noansi,getSession());
                if (res) {
                    res = -1;
                } else {
                    newbbslog(BBSLOG_USER, "mailed %s %s", userid, title);
                }
                break;
            }
        }
        unlink(tmp_fname);
        return res;
    } else
#endif
    {
        if (vedit(filepath, true, NULL, NULL, 0) == -1) {
            unlink(filepath);
            clear();
            return -2;
        }

        if (upload) {
            char sbuf[PATHLEN];
            strcpy(sbuf,"tmp/");
            strcpy(sbuf+strlen(sbuf), upload);
            newmessage.attachment = add_attach(filepath, sbuf, upload);
        }

        clear();
        /*
         * if(!chkreceiver(userid))
         * {
         * prints("%s 信箱已满,无法收信",userid);
         * return -4;
         * }
         */

#ifdef HAVE_USERSCORE
        /* 直接发信或版面回信需要进行积分检查 */
        if ((*q_file=='\0' || strncmp(q_file, "mail", 4)) && !sufficient_score_to_sendmail(getCurrentUser(), userid)) {
            prints("\033[1m\033[33m很抱歉∶系统无法发出此信．因为您的积分不足．\033[m\033[m\n\n");
            sprintf(save_title, "退信∶信件发送失败．");
            mail_file(getCurrentUser()->userid, filepath, getCurrentUser()->userid, save_title, BBSPOST_MOVE, NULL);
            return -2;
        }
#endif
        if (false == canIsend2(getCurrentUser(), userid)) {  /* Leeward 98.04.10 */
            prints("\033[1m\033[33m很抱歉∶系统无法发出此信．因为 %s 拒绝接收您的信件．\033[m\033[m\n\n", userid);
            sprintf(save_title, "退信∶ %s 拒绝接收您的信件．", userid);
            mail_file(getCurrentUser()->userid, filepath, getCurrentUser()->userid, save_title, BBSPOST_MOVE, NULL);
            return -2;
        }
        /*
         * 加上保存到发件箱的确认，by flyriver, 2002.9.23
         */
        /*
         * Disabled by flyriver, 2003.1.5
         * Using the newly introduced mailbox properties.
         buf2[0] = '\0';
         getdata(1, 0, "保存信件到发件箱? [N]: ", buf2, 2, DOECHO, 0, 0);
         if (buf2[0] == 'y' || buf2[0] == 'Y')
         */
        if (savesent) {
            /*
             * backup mail to sent folder
             */
            mail_file_sent(userid, filepath, getCurrentUser()->userid, save_title, 0, getSession());
        }
        sprintf(buf2,"确定寄给%s? ", userid);
        if (askyn(buf2, true) == false)
            return -2;

        if (stat(filepath, &st) != -1) {
            newmessage.eff_size = st.st_size;
        } else {
            newmessage.eff_size = 0;
        }

        setmailfile(genbuf, userid, DOT_DIR);
        if (append_record(genbuf, &newmessage, sizeof(newmessage)) == -1)
            return -1;
        user->usedspace += newmessage.eff_size;
        setmailcheck(userid);

#ifdef AUTOREMAIL
        sethomefile(genbuf, userid, "autoremail");
        if (dashf(genbuf)) {
            sprintf(buf2, "[自动回复]%s的信件自动回复",userid);
            mail_file(userid, genbuf, getCurrentUser()->userid, buf2, BBSPOST_COPY, NULL);
        }
        setmailcheck(getCurrentUser()->userid);
#endif

        newbbslog(BBSLOG_USER, "mailed %s %s", userid, save_title);
        if (!strcasecmp(userid, "SYSOP"))
            updatelastpost(SYSMAIL_BOARD);
        return 0;
    }
}

int m_send(char *userid)
{
    char uident[STRLEN];
    int oldmode;

    /*
     * 封禁Mail Bigman:2000.8.22
     */
    if (HAS_PERM(getCurrentUser(), PERM_DENYMAIL))
        return DONOTHING;

    oldmode = uinfo.mode;
    modify_user_mode(SMAIL);
    if (userid == NULL) {
        move(1, 0);
        clrtoeol();
        usercomplete("收信人： ", uident);
        if (uident[0] == '\0') {
            clear();
            modify_user_mode(oldmode);
            return 0;
        }
    } else
        strcpy(uident, userid);
    clear();
    switch (do_send(uident, NULL, "")) {
        case -1:
            prints("收信者不正确\n");
            break;
        case -2:
            prints("取消发信\n");
            break;
        case -3:
            prints("'%s' 无法收信\n", uident);
            break;
        case -4:
            clear();
            move(1, 0);
            prints("%s 信箱已满,无法收信\n", uident);
            break;                  /*Haohmaru.4.5.收信限制 */
        case -5:
            clear();
            move(1, 0);
            prints("%s 自杀中，不能收信\n", uident);
            break;                  /*Haohmaru.99.10.26.自杀者不能收信 */
        case -552:
            prints("\n\033[1m\033[33m信件超长（本站限定信件长度上限为 %d 字节），取消发信操作\033[m\033[m\n", MAXMAILSIZE);
            break;
        default:
            prints("信件已寄出\n");
    }
    pressreturn();
    modify_user_mode(oldmode);
    return 0;
}

extern int stuffmode;

int read_mail(struct fileheader *fptr)
{
    struct stat st;
    setmailfile(genbuf, getCurrentUser()->userid, fptr->filename);
    if (!lstat(genbuf, &st))
        if (S_ISLNK(st.st_mode))
            stuffmode = 1;
    ansimore_withzmodem(genbuf, false, fptr->title);
    stuffmode = 0;
    fptr->accessed[0] |= FILE_READ;
    return 0;
}

int del_mail(int ent, struct fileheader *fh, char *direct)
{
    char buf[PATHLEN];
    char *t;
    struct stat st;

    if (strstr(direct, ".DELETED")
            || HAS_MAILBOX_PROP(&uinfo, MBP_FORCEDELETEMAIL)) {
        strcpy(buf, direct);
        t = strrchr(buf, '/') + 1;
        strcpy(t, fh->filename);
        if (lstat(buf, &st) == 0 && S_ISREG(st.st_mode) && st.st_nlink == 1) {
            if (getCurrentUser()->usedspace > st.st_size)
                getCurrentUser()->usedspace -= st.st_size;
            else
                getCurrentUser()->usedspace = 0;
        }
    }

    strcpy(buf, direct);
    if ((t = strrchr(buf, '/')) != NULL)
        *t = '\0';
    if (!delete_record(direct, sizeof(*fh), ent, (RECORD_FUNC_ARG) cmpname, fh->filename)) {
        char filename[MAXPATH];
        sprintf(filename, "%s/%s", buf, fh->filename);
        if (strstr(direct, ".DELETED")
                || HAS_MAILBOX_PROP(&uinfo, MBP_FORCEDELETEMAIL)) {
            if (fh->filename[0]!=0)
                unlink(filename);
        } else {
            strcpy(buf, direct);
            t = strrchr(buf, '/') + 1;
            strcpy(t, ".DELETED");
            fh->accessed[sizeof(fh->accessed) - 1] = time(0) / (3600 * 24) % 100;
            append_record(buf, fh, sizeof(*fh));
        }
        setmailcheck(getCurrentUser()->userid);
        return 0;
    }
    return 1;
}

int mrd;
int delete_new_mail(struct fileheader *fptr, int idc, void *direct)
{
    if (fptr->accessed[1] & FILE_COMMEND) {
        del_mail(idc, fptr, (char*)direct);
        return 1;
    }
    return 0;
}

int read_new_mail(struct fileheader *fptr, int idc, char *direct)
{
    char done = false, delete_it;
    char fname[256];
    int gdataret;

    if (fptr->accessed[0])
        return 0;
    prints("读取 %s 寄来的 '%s' ?\n", fptr->owner, fptr->title);
    prints("(Yes, or No): ");
    gdataret = getdata(1, 0, "(Y)读取 (N)不读 (Q)离开 [Y]: ", genbuf, 3, DOECHO, NULL, true);
    if (gdataret == -1 || genbuf[0] == 'q' || genbuf[0] == 'Q') {
        clear();
        return QUIT;
    }
    if (genbuf[0] != 'y' && genbuf[0] != 'Y' && genbuf[0] != '\0') {
        clear();
        return 0;
    }
    read_mail(fptr);
    strcpy(fname, genbuf);
    mrd = 1;
    delete_it = false;
    while (!done) {
        move(t_lines - 1, 0);
        prints("(R)回信, (D)删除, (G)继续 ? [G]: ");
        switch (igetkey()) {
            case Ctrl('Y'):
                setmailfile(fname, getCurrentUser()->userid, fptr->filename);
                zsend_file(fname, fptr->title);
                break;
            case 'R':
            case 'r':

                /*
                 * 封禁Mail Bigman:2000.8.22
                 */
                if (HAS_PERM(getCurrentUser(), PERM_DENYMAIL)) {
                    clear();
                    move(3, 10);
                    prints("很抱歉,您目前没有Mail权限!");
                    pressreturn();
                    break;
                }

                mail_reply(idc, fptr,direct);
                /*
                 * substitute_record(currmaildir, fptr, sizeof(*fptr), dc);
                 */
                break;
            case 'D':
            case 'd':
                delete_it = true;
            default:
                done = true;
        }
        if (!done)
            ansimore(fname, false);     /* re-read */
    }
    if (delete_it) {
        clear();
        prints("Delete Message '%s' ", fptr->title);
        getdata(1, 0, "(Yes, or No) [N]: ", genbuf, 3, DOECHO, NULL, true);
        if (genbuf[0] == 'Y' || genbuf[0] == 'y') {     /* if not yes quit */
            fptr->accessed[1] |= FILE_COMMEND;
        }
    }
    if (substitute_record(direct, fptr, sizeof(*fptr), idc, (RECORD_FUNC_ARG) cmpname, fptr->filename))
        return -1;
    setmailcheck(getCurrentUser()->userid);
    clear();
    return 0;
}

int m_new(void)
{
    char direct[PATHLEN];
    clear();
    mrd = 0;
    modify_user_mode(RMAIL);
    setmailfile(direct, getCurrentUser()->userid, ".DIR");
    if (apply_record(direct, (APPLY_FUNC_ARG) read_new_mail, sizeof(struct fileheader), direct, 1, false) == -1) {
        clear();
        move(0, 0);
        prints("No new messages\n\n\n");
        return -1;
    }
    apply_record(direct, (APPLY_FUNC_ARG) delete_new_mail, sizeof(struct fileheader), direct, 1, true);
    /*
        if (delcnt) {
            while (delcnt--)
                delete_record(currmaildir, sizeof(struct fileheader), delmsgs[delcnt], NULL, NULL);
        }
    */
    clear();
    move(0, 0);
    if (mrd)
        prints("No more messages.\n\n\n");
    else
        prints("No new messages.\n\n\n");
    return -1;
}

void mailtitle(struct _select_def* conf)
{
    /*
     * Leeward 98.01.19 adds below codes for statistics
     */
    int MailSpace, numlimit;
    int UsedSpace = get_mailusedspace(getCurrentUser(), 0) / 1024;
    struct read_arg* arg=(struct read_arg*) conf->arg;

    get_mail_limit(getCurrentUser(), &MailSpace, &numlimit);
    showtitle("邮件选单    ", BBS_FULL_NAME);
    update_endline();
    move(1, 0);
    prints("离开[←,e]  选择[↑,↓]  阅读信件[→,r]  回信[R]  砍信／清除旧信[d,D]  求助[h]\033[m\n");
    /*
     * prints("\033[44m编号    %-20s %-49s\033[m\n","发信者","标  题") ;
     */
    if (0 != get_mailnum(arg->direct) && 0 == UsedSpace)
        UsedSpace = 1;
    else if (UsedSpace < 0)
        UsedSpace = 0;
    prints("\033[44m 编号    %-12s %6s  %-12s您的信箱上限容量%4dK，当前已用%4dK ", (strstr(arg->direct, ".SENT")) ? "收信者" : "发信者", "日  期", "标  题", MailSpace, UsedSpace);    /* modified by dong , 1998.9.19 */
    clrtoeol();
    prints("\n");
    resetcolor();
}

char *maildoent(char *buf, int num, struct fileheader *ent,struct fileheader* readfh,struct _select_def* conf)
{
    time_t filetime;
    char *date;
    char b2[512];
    char status, reply_status, attach;
    char *t;
    char c1[8];
    char c2[8];
    int same = false;

    filetime = get_posttime(ent);       /* 由文件名取得时间 */
    if (filetime > 740000000)
        date = ctime(&filetime) + 4;    /* 时间 -> 英文 */
    else
        /*
         * date = ""; char *类型变量, 可能错误, modified by dong, 1998.9.19
         */
    {
        date = ctime(&filetime) + 4;
        date = "";
    }

    if (DEFINE(getCurrentUser(), DEF_HIGHCOLOR)) {
        strcpy(c1, "\033[1;36m");
        strcpy(c2, "\033[1;33m");
    } else {
        strcpy(c1, "\033[36m");
        strcpy(c2, "\033[33m");
    }
    if (readfh&&isThreadTitle(readfh->title, ent->title))
        same = true;
    strncpy(b2, ent->owner, OWNER_LEN);
    ent->owner[OWNER_LEN - 1] = 0;
    if ((t = strchr(b2, ' ')) != NULL)
        *t = '\0';
    if (ent->accessed[0] & FILE_READ) {
        if (ent->accessed[0] & FILE_MARKED)
            status = 'm';
        else
            status = ' ';
    } else {
        if (ent->accessed[0] & FILE_MARKED)
            status = 'M';
        else
            status = 'N';
    }

    if (ent->accessed[1]&FILE_DEL)
        status='X';

    if (ent->accessed[0] & FILE_REPLIED) {
        if (ent->accessed[0] & FILE_FORWARDED)
            reply_status = 'A';
        else
            reply_status = 'R';
    } else {
        if (ent->accessed[0] & FILE_FORWARDED)
            reply_status = 'F';
        else
            reply_status = ' ';
    }
    if (ent->attachment > 0) attach = '@';
    else attach = ' ';
    /*
     * if (ent->accessed[0] & FILE_REPLIED)
     * reply_status = 'R';
     * else
     * reply_status = ' ';
     *//*
* * * * * added by alex, 96.9.7
*/
    if (!gShowSize) {
        if (!strncmp("Re:", ent->title, 3)) {
            sprintf(buf, " %s%3d\033[m %c%c %-12.12s %6.6s %c%s%.50s\033[m", same ? c1 : "", num, reply_status, status, b2, date, attach, same ? c1 : "", ent->title);
        } /* modified by dong, 1998.9.19 */
        else {
            sprintf(buf, " %s%3d\033[m %c%c %-12.12s %6.6s %c★ %s%.47s\033[m", same ? c2 : "", num, reply_status, status, b2, date, attach, same ? c2 : "", ent->title);
        }                           /* modified by dong, 1998.9.19 */
    } else {
        int titlelen, size = 0, i;
        char TITLE[ARTICLE_TITLE_LEN];
        char *sign;
        int is_original = strncmp("Re:", ent->title, 3);
        if (ent->eff_size > 0) size = (ent->eff_size-1) / 1024 + 1;

        titlelen = scr_cols > 80 ? scr_cols - 80 + 44 : 44;
        if (is_original) titlelen -= 3;
        if (titlelen > ARTICLE_TITLE_LEN) titlelen = ARTICLE_TITLE_LEN;
        strnzhcpy(TITLE, ent->title, titlelen);
        for (i=strlen(TITLE); i<titlelen-1; i++) TITLE[i] = ' ';
        TITLE[titlelen-1] = '\0';
        sign = same ? (is_original ? c2 : c1) : "";
        sprintf(buf, " %s%3d\033[m %c%c %-12.12s %6.6s %c%s%s%s%4dK\033[m", sign, num, reply_status, status, b2, date, attach, is_original?"★ ":"", sign, TITLE, size);
    }
    return buf;
}

#ifdef POSTBUG
extern int bug_possible;
#endif


int mail_read(struct _select_def* conf,struct fileheader *fileinfo,void* extraarg)
{
    char buf[512], notgenbuf[128];
    char *t;
    int readnext,readprev;
    char done = false, delete_it, replied;
    int ent=conf->pos;
    struct read_arg* arg=conf->arg;
    struct stat st;

    if (fileinfo==NULL)
        return DONOTHING;
    clear();
    readnext = false;
    readprev = false;
    setreadpost(conf,fileinfo);
    strcpy(buf, arg->direct);
    if ((t = strrchr(buf, '/')) != NULL)
        *t = '\0';
    sprintf(notgenbuf, "%s/%s", buf, fileinfo->filename);
    delete_it = replied = false;

    if (lstat(notgenbuf,&st) != -1) {
        if (S_ISLNK(st.st_mode)) {
            stuffmode = 1;
        }
    }

    while (!done) {
        ansimore(notgenbuf, false);
        move(t_lines - 1, 0);
        prints("(R)回信, (D)删除, (G)继续? [G]: ");
        switch (igetkey()) {
            case Ctrl('Y'):
                read_zsend(conf, fileinfo, NULL);
                break;
            case 'R':
            case 'r':

                /*
                 * 封禁Mail Bigman:2000.8.22
                 */
                if (HAS_PERM(getCurrentUser(), PERM_DENYMAIL)) {
                    clear();
                    move(3, 10);
                    prints("很抱歉,您目前没有Mail权限!");
                    pressreturn();
                    break;
                }
                replied = true;
                do_mail_reply(conf, fileinfo,NULL);
                break;
            case ' ':
            case 'j':
            case KEY_RIGHT:
            case KEY_DOWN:
            case KEY_PGDN:
                done = true;
                readnext = true;
                break;
                /* read prev mail  .  binxun 2003.5*/
            case KEY_UP:
                done = true;
                readprev = true;
                break;

            case Ctrl('D'):
                zsend_attach(ent, fileinfo, arg->direct);
                done=true;
                break;
            case 'D':
            case 'd':
                delete_it = true;
            default:
                done = true;

        }
    }
    stuffmode = 0;
    if (delete_it)
        return mail_del(conf, fileinfo,NULL);
    else if ((fileinfo->accessed[0] & FILE_READ) != FILE_READ) {
        fileinfo->accessed[0] |= FILE_READ;
        substitute_record(arg->direct, fileinfo, sizeof(*fileinfo), ent, (RECORD_FUNC_ARG) cmpname, fileinfo->filename);
        setmailcheck(getCurrentUser()->userid);
    }
    if (readnext == true)
        return READ_NEXT;

    /* read prev mail  .  binxun 2003.5*/
    if (readprev == true)
        return READ_PREV;

    if (!strcmp(fileinfo->owner,getCurrentUser()->userid))
        return DIRCHANGED;
    return FULLUPDATE;
}


/*ARGSUSED*/
static int mail_reply(int ent, struct fileheader *fileinfo,char* direct)
{
    char uid[STRLEN];
    char title[STRLEN];
    char q_file[STRLEN];
    char *t;

    if (fileinfo==NULL)
        return DONOTHING;
    clear();
    modify_user_mode(SMAIL);
    strncpy(uid, fileinfo->owner, OWNER_LEN);
    uid[OWNER_LEN - 1] = 0;
    if ((t = strchr(uid, ' ')) != NULL)
        *t = '\0';
    if (toupper(fileinfo->title[0]) != 'R' || fileinfo->title[1] != 'e' || fileinfo->title[2] != ':')
        strcpy(title, "Re: ");
    else
        title[0] = '\0';
    strncat(title, fileinfo->title, ARTICLE_TITLE_LEN - 5);

    setmailfile(q_file, getCurrentUser()->userid, fileinfo->filename);
    strncpy(quote_user, fileinfo->owner, IDLEN);
    quote_user[IDLEN] = 0;
    switch (do_send(uid, title, q_file)) {
        case -1:
            prints("无法投递\n");
            break;
        case -2:
            prints("取消回信\n");
            break;
        case -3:
            prints("'%s' 无法收信\n", uid);
            break;
        case -4:
            clear();
            move(1, 0);
            prints("%s 信箱已满,无法收信\n", uid);
            break;                  /*Haohmaru.4.5.收信限制 */
        case -5:
            clear();
            move(1, 0);
            prints("%s 自杀中，不能收信\n", uid);
            break;                  /*Haohmaru.99.10.26.自杀者不能收信 */
        default:
            prints("信件已寄出\n");
            fileinfo->accessed[0] |= FILE_REPLIED;  /*added by alex, 96.9.7 */
            substitute_record(direct, fileinfo, sizeof(*fileinfo), ent, (RECORD_FUNC_ARG) cmpname, fileinfo->filename);
    }
    pressreturn();
    return FULLUPDATE;
}

static int do_mail_reply(struct _select_def* conf, struct fileheader *fileinfo,void* extraarg)
{
    int ent=conf->pos;
    struct read_arg* arg=conf->arg;
    if (fileinfo==NULL)
        return DONOTHING;
    mail_reply(ent,fileinfo,arg->direct);
    if (strcmp(fileinfo->owner,getCurrentUser()->userid))
        return  DONOTHING;
    return DIRCHANGED;
}

static int mail_showsize(struct _select_def* conf, struct fileheader *fileinfo,void* extraarg)
{
    gShowSize = !gShowSize;
    return FULLUPDATE;
}

static int mail_del(struct _select_def* conf, struct fileheader *fileinfo,void* extraarg)
{
    int ent=conf->pos;
    struct read_arg* arg=conf->arg;
    if (fileinfo==NULL)
        return DONOTHING;
    if (arg->mode == DIR_MODE_SUPERFITER)
        return DONOTHING;
    clear();
    prints("删除此信件 '%s' ", fileinfo->title);
    getdata(1, 0, "(Yes, or No) [N]: ", genbuf, 2, DOECHO, NULL, true);
    if (genbuf[0] != 'Y' && genbuf[0] != 'y') { /* if not yes quit */
        move(2, 0);
        prints("取消删除\n");
        pressreturn();
        clear();
        return FULLUPDATE;
    }

    if (del_mail(ent, fileinfo, arg->direct) == 0) {
        return DIRCHANGED;
    }
    move(2, 0);
    prints("删除失败\n");
    pressreturn();
    clear();
    return FULLUPDATE;
}

/*added by bad 03-2-10*/
static int mail_edit(struct _select_def* conf, struct fileheader *fileinfo,void* extraarg)
{
    char buf[512];
    char *t;
    long eff_size;
    long attachpos;
    struct stat st;
    int before = 0;
    int ent=conf->pos;
    struct read_arg* arg=conf->arg;

    if (fileinfo==NULL)
        return DONOTHING;
    strcpy(buf, arg->direct);
    if ((t = strrchr(buf, '/')) != NULL)
        *t = '\0';

    sprintf(genbuf, "%s/%s", buf, fileinfo->filename);
    if (lstat(genbuf,&st) != -1) {
        if (S_ISLNK(st.st_mode))
            return DONOTHING;
    } else
        return DONOTHING;

    if (stat(genbuf,&st) != -1) {
        mode_t rwmode = S_IRUSR | S_IWUSR ;
        if ((st.st_mode & rwmode) != rwmode)
            return DONOTHING;
        before = st.st_size;
    } else
        return DONOTHING;

    modify_user_mode(EDIT);
    clear();
    attachpos = fileinfo->attachment;
    if (vedit_post(genbuf, false, &eff_size,&attachpos, 0) != -1) {
        if (ADD_EDITMARK)
            add_edit_mark(genbuf, 1, /*NULL*/ fileinfo->title,getSession());

        if (stat(genbuf,&st) != -1) {
            int diff = before - st.st_size;
            if (diff <= 0 || getCurrentUser()->usedspace > diff)
                getCurrentUser()->usedspace -= diff;
            else
                getCurrentUser()->usedspace = 0;
            fileinfo->eff_size = st.st_size;
        } else {
            fileinfo->eff_size = 0;
        }
        fileinfo->attachment=attachpos;
        substitute_record(arg->direct, fileinfo, sizeof(*fileinfo), ent, (RECORD_FUNC_ARG) cmpname, fileinfo->filename);
    }

    newbbslog(BBSLOG_USER, "edited mail '%s' ", fileinfo->title);
    return FULLUPDATE;
}

static int mail_edit_title(struct _select_def* conf, struct fileheader *fileinfo,void* extraarg)
{
    char buf[STRLEN];
    char tmp[STRLEN*2];
    char genbuf[1024];
    char * t = NULL;
    int ent=conf->pos;
    struct read_arg* arg=conf->arg;
    struct stat st;

    if (fileinfo==NULL)
        return DONOTHING;
    if (arg->mode == DIR_MODE_SUPERFITER)
        return DONOTHING;
    setmailfile(genbuf,getCurrentUser()->userid,fileinfo->filename);
    if (lstat(genbuf,&st)==-1||S_ISLNK(st.st_mode))
        return DONOTHING;
    strcpy(buf,fileinfo->title);
    getdata(t_lines-1,0,"新信件标题:",buf,50,DOECHO,NULL,false);

    if (buf[0] && strcmp(buf,fileinfo->title)) {
        process_control_chars(buf,NULL);
        strnzhcpy(fileinfo->title, buf, ARTICLE_TITLE_LEN);

        strcpy(tmp,arg->direct);
        if ((t = strrchr(tmp,'/')) != NULL)*t='\0';
        sprintf(genbuf,"%s/%s",tmp,fileinfo->filename);
        add_edit_mark(genbuf,3,buf,getSession()); /* 3 means edit mail and title */
        substitute_record(arg->direct, fileinfo, sizeof(*fileinfo), ent, (RECORD_FUNC_ARG) cmpname, fileinfo->filename);
        newbbslog(BBSLOG_USER, "edited mail '%s' ", fileinfo->title);
    }
    return FULLUPDATE;
}

/** Added by netty to handle mail to 0Announce */
int mail_to_tmp(struct _select_def* conf, struct fileheader *fileinfo,void* extraarg)
{
    char buf[STRLEN];
    char *p;
    char fname[STRLEN];
    char board[STRLEN];
    char ans[STRLEN];
    struct read_arg* arg=conf->arg;

    if (fileinfo==NULL)
        return DONOTHING;
    if (!HAS_PERM(getCurrentUser(), PERM_BOARDS)) {
        return DONOTHING;
    }
    strncpy(buf, arg->direct, sizeof(buf));
    if ((p = strrchr(buf, '/')) != NULL)
        *p = '\0';
    sprintf(fname, "%s/%s", buf, fileinfo->filename);
    sprintf(genbuf, "将信件存入暂存档,确定吗?(Y/N) [N]: ");
    a_prompt(-1, genbuf, ans);
    if (ans[0] == 'Y' || ans[0] == 'y') {
        sprintf(board, "tmp/bm.%s", getCurrentUser()->userid);
        if (dashf(board)) {
            sprintf(genbuf, "要附加在旧暂存档之后吗?(Y/N) [N]: ");
            a_prompt(-1, genbuf, ans);
            if (ans[0] == 'Y' || ans[0] == 'y') {
                sprintf(genbuf, "/bin/cat %s >> tmp/bm.%s", fname, getCurrentUser()->userid);
                system(genbuf);
            } else {
                /*
                 * sprintf( genbuf, "/bin/cp -r %s  tmp/bm.%s", fname , getCurrentUser()->userid );
                 */
                sprintf(genbuf, "tmp/bm.%s", getCurrentUser()->userid);
                f_cp(fname, genbuf, 0);
            }
        } else {
            sprintf(genbuf, "tmp/bm.%s", getCurrentUser()->userid);
            f_cp(fname, genbuf, 0);
        }
        sprintf(genbuf, " 已将该文章存入暂存档, 请按 ENTER 键以继续 << ");
        a_prompt(-1, genbuf, ans);
    }
    clear();
    return FULLUPDATE;
}


int mail_forward_internal(struct _select_def* conf, struct fileheader *fh, char* direct, int inmail)
{
    char buf[STRLEN];
    char *p;
    struct fileheader *fileinfo;

//    int ent=conf->pos;
//    struct read_arg* arg=conf->arg;
    if (fh==NULL)
        return DONOTHING;

    if (strcmp("guest", getCurrentUser()->userid) == 0) {
        clear();
        move(3, 10);
        prints("很抱歉,想转寄文章请申请正式ID!");
        pressreturn();
        return FULLUPDATE;
    }

    /*
     * 封禁Mail Bigman:2000.8.22
     */
    if (HAS_PERM(getCurrentUser(), PERM_DENYMAIL)) {
        clear();
        move(3, 10);
        prints("很抱歉,您目前没有Mail权限!");
        pressreturn();
        return FULLUPDATE;
    }

#ifdef ENABLE_BOARD_MEMBER
    if (!inmail) {
        int ret;
        clear();
        ret = member_read_perm(currboard, fh, getCurrentUser());
        if (!ret) {
            move(3, 10);
            prints("本版为驻版可读，非本版驻版用户不能转寄文章！");
            move(4, 10);
            prints("详情请联系本版版主。");
            pressreturn();
            return FULLUPDATE;
        } else if (ret==1) {
            char ans[4];
            getdata(3, 4, "\033[1;31m本版为驻版可读，转寄可能泄露文章内容。确定转寄? [y/N]: \033[m", ans, 2, DOECHO, NULL, true);
            if (toupper(ans[0]) != 'Y')
                return FULLUPDATE;
        }
    }
#endif
    if (!HAS_PERM(getCurrentUser(), PERM_BASIC) || !HAS_PERM(getCurrentUser(),PERM_LOGINOK)) {
        return DONOTHING;
    }
    strncpy(buf, direct, sizeof(buf));
    if ((p = strrchr(buf, '/')) != NULL)
        *p = '\0';
    clear();
    /* 防止doforward修改fh内容 */
    fileinfo=(struct fileheader *)malloc(sizeof(struct fileheader));
    memcpy(fileinfo, fh, sizeof(struct fileheader));
    switch (doforward(conf, buf, fileinfo)) {
        case 0:
            prints("文章转寄完成!\n");
            if (inmail) {
                fh->accessed[0] |= FILE_FORWARDED;        /*added by alex, 96.9.7 */
                substitute_record(direct, fh, sizeof(*fh), conf->pos, (RECORD_FUNC_ARG) cmpname, fh->filename);
            }
            break;
        case -1:
            prints("Forward failed: system error.\n");
            break;
        case -2:
            prints("Forward failed: missing or invalid address.\n");
            break;
        case -552:
            prints
            ("\n\033[1m\033[33m信件超长（本站限定信件长度上限为 %d 字节），取消转寄操作\033[m\033[m\n\n请告知收信人（也许就是您自己吧:PP）：\n\n*1* 使用 \033[1m\033[33mWWW\033[m\033[m 方式访问本站，随时可以保存任意长度的文章到自己的计算机；\n*2* 使用 \033[1m\033[33mpop3\033[m\033[m 方式从本站用户的信箱取信，没有任何长度限制。\n*3* 如果不熟悉本站的 WWW 或 pop3 服务，请阅读 \033[1m\033[33mAnnounce\033[m\033[m 版有关公告。\n",
             MAXMAILSIZE);
            break;
        default:
            prints("取消转寄...\n");
    }
    free(fileinfo);
    pressreturn();
    clear();
    return FULLUPDATE;
}

/* 定义挪到libBBS中去 */
#if 0
/* 生成同主题文件，格式与合集类似，用于同主题转寄 */
int user_thread_save(const char *board, struct fileheader *fileinfo, int no_ref, char *userid)
{
    FILE *inf, *outf;
    char qfile[STRLEN], filepath[STRLEN], genbuf[PATHLEN];
    char buf[256];
    struct fileheader savefileheader;
    char userinfo[STRLEN],posttime[STRLEN];
    char* t;

    sprintf(qfile, "boards/%s/%s", board, fileinfo->filename);
    gettmpfilename(filepath, "ut", getpid());
    outf = fopen(filepath, "a");
    if (*qfile != '\0' && (inf = fopen(qfile, "r")) != NULL) {
        fgets(buf, 256, inf);

        t = strrchr(buf,')');
        if (t) {
            *(t+1)='\0';
            memcpy(userinfo,buf+8,STRLEN);
        } else strcpy(userinfo,"未知发信人");
        fgets(buf, 256, inf);
        fgets(buf, 256, inf);
        t = strrchr(buf,')');
        if (t) {
            *(t+1)='\0';
            if (NULL!=(t = strchr(buf,'(')))
                memcpy(posttime,t,STRLEN);
            else
                strcpy(posttime,"未知时间");
        } else
            strcpy(posttime,"未知时间");

        fprintf(outf, "\033[0;1;32m☆─────────────────────────────────────☆\033[0;37m\n");
        fprintf(outf, "  \033[0;1;32m %s \033[0;1;37m于 \033[0;1;36m %s \033[0;1;37m  在\n", userinfo, posttime);
        fprintf(outf, "  \033[0;1;32m【\033[33;4m%s\033[0;32m】\033[0;1;37m 的大作中提到:\033[m\n", fileinfo->title);

        fprintf(outf,"\n");
        while (fgets(buf, 256, inf) != NULL)
            if (buf[0] == '\n')
                break;

        while (fgets(buf, 256, inf) != NULL) {
            /*结束*/
            if (!strcmp(buf,"--\n"))
                break;
            /*引文*/
            if (no_ref&&(strstr(buf,": ")==buf||(strstr(buf,"【 在")==buf&&strstr(buf,") 的大作中提到: 】"))))
                continue;
            /*来源和修改信息*/
            if ((strstr(buf,"\033[m\033")==buf&&strstr(buf,"※ 来源:·")==buf+10)
                    ||(strstr(buf,"\033[36m※ 修改:·")==buf))
                continue;
            fprintf(outf, "%s", buf);
        }
        fprintf(outf, "\n\n");
        fclose(inf);
    }

    fclose(outf);
    memcpy(&savefileheader,fileinfo,sizeof(savefileheader));
    savefileheader.attachment=0;
    if (fileinfo->attachment) {
        int fsrc,fdst;
        char *src = (char *) malloc(BLK_SIZ);
        if ((fsrc = open(qfile, O_RDONLY)) >= 0) {
            lseek(fsrc,fileinfo->attachment-1,SEEK_SET);
            gettmpfilename(genbuf, "ut.attach", getpid());
            if ((fdst=open(genbuf,O_WRONLY | O_CREAT | O_APPEND, 0600)) >= 0) {
                long ret;
                do {
                    ret = read(fsrc, src, BLK_SIZ);
                    if (ret <= 0)
                        break;
                } while (write(fdst, src, ret) > 0);
                close(fdst);
            }
            close(fsrc);
        }
        free(src);
    }

    return 1;
}
#endif
/* 获得同主题第一篇文章标题 */
static int get_thread_title(struct _select_def* conf, struct fileheader* fh, int ent, void *title)
{
    strcpy((char*)title, fh->title);
    return APPLY_QUIT;
}

/*用户同主题函数，用于apply_thread的回调函数, 用于同主题转寄, added by jiangjun, 20110720 */
static int user_thread_mail(struct _select_def* conf, struct fileheader* fh,int ent, void* extraarg)
{
    struct read_arg* arg=(struct read_arg*)conf->arg;
    int no_ref=0, ret=APPLY_CONTINUE;

#ifdef ENABLE_BOARD_MEMBER
    if (!member_read_perm(currboard, fh, getCurrentUser()))
        return ret;
#endif
    if (extraarg)
        no_ref = *((int *)extraarg);
    conf->pos=ent;
    if (arg->writearg) {
        arg->writearg->ent=ent;
    }
    /* 读取并保存同主题的正文和回复 */
    user_thread_save(currboard->filename, fh, no_ref, 0);

    return ret;
}

/* 生成同主题转寄的临时文件 */
int get_thread_forward(struct _select_def* conf, struct fileheader* fh, char *title, void* extraarg)
{
    struct read_arg* arg=conf->arg;
    struct write_dir_arg dirarg;
    int ent;
    char ut_file[STRLEN], ut_attach[STRLEN];
    FILE *fn;

    ent=conf->pos;
    init_write_dir_arg(&dirarg);
    dirarg.fd=arg->fd;
    dirarg.needlock=false;
    arg->writearg=&dirarg;
    writew_lock(arg->fd, 0, SEEK_SET, 0);

    /*走到第一篇*/
    conf->new_pos=0;
    apply_thread(conf,fh,fileheader_thread_read,false,false,(void*)SR_FIRST);
    if (conf->new_pos!=0)
        conf->pos=conf->new_pos;
    /* 获得第一篇的标题, 因为已经走到第一篇 */
    apply_thread(conf,fh,get_thread_title,true,false,(void*)title);
    /* 清空临时文件, 防止意外 */
    gettmpfilename(ut_file, "ut", getpid());
    unlink(ut_file);
    gettmpfilename(ut_attach, "ut.attach", getpid());
    unlink(ut_attach);

    if ((fn=fopen(ut_file, "w"))==NULL)
        return -1;
    //fprintf(fn, "\033[0;1;33m【以下内容由 \033[32m%s\033[33m 转寄，原文发表于 \033[32m%s\033[33m 版】\n\n", getCurrentUser()->userid, currboard->filename);
    fclose(fn);

    apply_thread(conf,fh,user_thread_mail,true,true,extraarg);
    if (dashf(ut_attach)) {
        f_catfile(ut_attach, ut_file);
        unlink(ut_attach);
    }
    un_lock(arg->fd, 0, SEEK_SET, 0);
    free_write_dir_arg(&dirarg);
    arg->writearg=NULL;
    conf->pos=ent;
    return 0;
}

int mail_forward(struct _select_def* conf, struct fileheader *fileinfo,void* extraarg)
{
    int inmail=(((struct read_arg*)conf -> arg) -> mode == DIR_MODE_MAIL);
    return mail_forward_internal(conf, fileinfo, ((struct read_arg*)conf->arg)->direct, inmail);
}


int mail_mark(struct _select_def* conf, struct fileheader *fileinfo,void* extraarg)
{
    int ent=conf->pos;
    struct read_arg* arg=conf->arg;
    if (fileinfo==NULL)
        return DONOTHING;
    if (arg->mode == DIR_MODE_SUPERFITER)
        return DONOTHING;
    if (fileinfo->accessed[0] & FILE_MARKED)
        fileinfo->accessed[0] &= ~FILE_MARKED;
    else
        fileinfo->accessed[0] |= FILE_MARKED;
    substitute_record(arg->direct, fileinfo, sizeof(*fileinfo), ent, (RECORD_FUNC_ARG) cmpname, fileinfo->filename);
    return (PARTUPDATE);
}

int mail_token(struct _select_def *conf,struct fileheader *file,void *varg)
{
    struct read_arg *arg=(struct read_arg*)conf->arg;
    if (!file)
        return DONOTHING;
    if (arg->mode == DIR_MODE_SUPERFITER)
        return DONOTHING;
    file->accessed[1]^=FILE_DEL;
    substitute_record(arg->direct,file,sizeof(struct fileheader),conf->pos, (RECORD_FUNC_ARG) cmpname, file->filename);
    return PARTUPDATE;
}

int mail_move(struct _select_def* conf, struct fileheader *fileinfo,void* extraarg)
{
    struct _select_item *sel;
    int i;
    char buf[PATHLEN];
    char menu_char[4][10] = {"I) 收件箱","P) 发件箱","J) 垃圾箱","Q) 退出"};//etnlegend,2005.03.27,加上发件箱&修正显示
    int ent=conf->pos;
    struct read_arg* arg=conf->arg;

    if (fileinfo==NULL)
        return DONOTHING;
    if (arg->mode == DIR_MODE_SUPERFITER)
        return DONOTHING;
    clear();
    move(4,4);
    prints("\033[1;32m请选择移动到哪个邮箱\033[m");
    sel=(struct _select_item*)malloc(sizeof(struct _select_item)*(user_mail_list.mail_list_t+5));
    sel[0].x=4;
    sel[0].y=5;
    sel[0].hotkey='I';
    sel[0].type=SIT_SELECT;
    sel[0].data=menu_char[0];//收件箱
    sel[1].x=4;
    sel[1].y=6;
    sel[1].hotkey='P';
    sel[1].type=SIT_SELECT;
    sel[1].data=menu_char[1];//发件箱
    sel[2].x=4;
    sel[2].y=7;
    sel[2].hotkey='J';
    sel[2].type=SIT_SELECT;
    sel[2].data=menu_char[2];//垃圾箱
    for (i=0; i<((user_mail_list.mail_list_t>12)?(user_mail_list.mail_list_t-1)/2:user_mail_list.mail_list_t); i++) {//分栏判断
        sel[i+3].x=4;
        sel[i+3].y=i+8;
        sel[i+3].hotkey=user_mail_list.mail_list[i][0];
        sel[i+3].type=SIT_SELECT;
        sel[i+3].data=(void*)user_mail_list.mail_list[i];//自定义邮箱
    }
    if (i!=user_mail_list.mail_list_t)//需要分栏
        for (; i<user_mail_list.mail_list_t; i++) {
            sel[i+3].x=44;
            sel[i+3].y=5+(i-(user_mail_list.mail_list_t-1)/2);
            sel[i+3].hotkey=user_mail_list.mail_list[i][0];
            sel[i+3].type=SIT_SELECT;
            sel[i+3].data=(void*)user_mail_list.mail_list[i];
        }
    sel[user_mail_list.mail_list_t+3].x=((i>12)?44:4);//i=user_mail_list.mail_list_t
    sel[user_mail_list.mail_list_t+3].y=((i>12)?(6+i/2):(i+8));
    sel[user_mail_list.mail_list_t+3].hotkey='Q';
    sel[user_mail_list.mail_list_t+3].type=SIT_SELECT;
    sel[user_mail_list.mail_list_t+3].data=menu_char[3];//退出...
    sel[user_mail_list.mail_list_t+4].x=-1;
    sel[user_mail_list.mail_list_t+4].y=-1;
    sel[user_mail_list.mail_list_t+4].hotkey=-1;
    sel[user_mail_list.mail_list_t+4].type=0;
    sel[user_mail_list.mail_list_t+4].data=NULL;
    i=simple_select_loop(sel,SIF_NUMBERKEY|SIF_SINGLE|SIF_ESCQUIT,0,6,NULL)-1;
    if (!(i<0)&&i<user_mail_list.mail_list_t+3) {
        switch (i) {
            case 0:setmailfile(buf,getCurrentUser()->userid,".DIR"); break;
            case 1:setmailfile(buf,getCurrentUser()->userid,".SENT"); break;
            case 2:setmailfile(buf,getCurrentUser()->userid,".DELETED"); break;
            default:
                setmailpath(buf,getCurrentUser()->userid);
                strcat(strcat(buf,"/."),user_mail_list.mail_list[i-3]+30);
        }
        if (strcmp(buf,arg->direct))
            if (!delete_record(arg->direct,sizeof(*fileinfo),ent,(RECORD_FUNC_ARG)cmpname,fileinfo->filename))
                /* 移到垃圾箱时更新access[3] */
                if (strstr(buf, ".DELETED")==0)
                    fileinfo->accessed[sizeof(fileinfo->accessed) - 1] = time(0) / (3600 * 24) % 100;
                append_record(buf,fileinfo,sizeof(*fileinfo));
    }
    free(sel);
    return (DIRCHANGED);
}

int mailreadhelp(struct _select_def* conf,void* data,void* extraarg)
{
    show_help("help/mailreadhelp");
    return FULLUPDATE;
}

int mail_add_ignore(struct _select_def *conf,struct fileheader *fh,void *arg)
{
#define MAIL_ADD_IGNORE_ITEMSZ ((IDLEN+1)*sizeof(char))
#define MAIL_ADD_IGNORE_ECHO(s) do{prints("\033[1;32m%s\033[0;33m<ENTER>\033[m",s);WAIT_RETURN;}while(0)
#define MAIL_ADD_IGNORE_RETURN do{saveline(t_lines-2,1,linebuf);move(y,x);return DONOTHING;}while(0)
    struct stat st;
    char linebuf[512],buf[64],ans[4];
    int fd,y,x,ret;
    void *map;
    const void *p;
    if (!fh||!searchuser(fh->owner)||!strcasecmp(fh->owner,getCurrentUser()->userid))
        return DONOTHING;
    getyx(&y,&x);
    saveline(t_lines-2,0,linebuf);
    move(t_lines-2,0); clrtoeol();
    sprintf(buf,"\033[1;32m添加用户 %s 到黑名单 [y/N]: \033[m",fh->owner);
    getdata(t_lines-2,0,buf,ans,2,DOECHO,NULL,true);
    if (!(ans[0]=='y'||ans[0]=='Y'))
        MAIL_ADD_IGNORE_RETURN;
    move(t_lines-2,0); clrtoeol();
    sethomefile(buf,getCurrentUser()->userid,"ignores");
    if (!stat(buf,&st)&&S_ISREG(st.st_mode)&&st.st_size) {
        if (!((ret=st.st_size/MAIL_ADD_IGNORE_ITEMSZ)<MAX_IGNORE)) {
            MAIL_ADD_IGNORE_ECHO("已达到黑名单上限...");
            MAIL_ADD_IGNORE_RETURN;
        }
        if (st.st_size%MAIL_ADD_IGNORE_ITEMSZ)
            truncate(buf,ret*MAIL_ADD_IGNORE_ITEMSZ);
        if ((fd=open(buf,O_RDONLY,0644))==-1)
            MAIL_ADD_IGNORE_RETURN;
        map=mmap(NULL,st.st_size,PROT_READ,MAP_SHARED,fd,0);
        close(fd);
        if (map==MAP_FAILED)
            MAIL_ADD_IGNORE_RETURN;
        for (p=map; ret>0; vpm(p,MAIL_ADD_IGNORE_ITEMSZ),ret--)
            if (!strcmp(p,fh->owner))
                break;
        munmap(map,st.st_size);
        if (ret) {
            MAIL_ADD_IGNORE_ECHO("该用户已经在当前黑名单中...");
            MAIL_ADD_IGNORE_RETURN;
        }
    }
    if ((fd=open(buf,O_WRONLY|O_CREAT|O_APPEND,0644))==-1)
        MAIL_ADD_IGNORE_RETURN;
    ret=write(fd,fh->owner,MAIL_ADD_IGNORE_ITEMSZ);
    close(fd);
    if (ret!=-1)
        MAIL_ADD_IGNORE_ECHO("该用户已经被添加到黑名单!");
    MAIL_ADD_IGNORE_RETURN;
#undef MAIL_ADD_IGNORE_ITEMSZ
#undef MAIL_ADD_IGNORE_ECHO
#undef MAIL_ADD_IGNORE_RETURN
}

int change_mail_mode(struct _select_def *conf,struct fileheader *fh,int mode)
{
    char ans[4];

    if (!mode) {
        move(t_lines-2,0);
        clrtoeol();
        prints("%s","切换模式到: 0)取消 ");
        move(t_lines-1,0);
        clrtoeol();
        if (getdata(t_lines-1,12,"7)超级文章选择 [7]: ",ans,2,DOECHO,NULL,true)==-1)
            return FULLUPDATE;
        switch (ans[0]) {
            case '0':
                return FULLUPDATE;
                /*
                case '1':
                    mode=DIR_MODE_DIGEST;
                    break;
                case '2':
                    mode=DIR_MODE_THREAD;
                    break;
                case '3':
                    mode=DIR_MODE_MARK;
                    break;
                case '4':
                    mode=DIR_MODE_ORIGIN;
                    buf[0]=0;
                    break;
                case '5':
                    mode=DIR_MODE_AUTHOR;
                    move(t_lines-2,0);
                    clrtoeol();
                    move(t_lines-1,0);
                    clrtoeol();
                    strcpy(buf,fh->owner);
                    getdata(t_lines-1,0,"您希望查找哪位用户的文章: ",buf,IDLEN+2,DOECHO,NULL,false);
                    if(!buf[0])
                        return FULLUPDATE;
                    break;
                case '6':
                    mode=DIR_MODE_TITLE;
                    move(t_lines-2,0);
                    clrtoeol();
                    move(t_lines-1,0);
                    clrtoeol();
                    strcpy(buf,title);
                    getdata(t_lines-1,0,"您希望查找的标题关键字: ",buf,32,DOECHO,NULL,false);
                    if(!buf[0])
                        return FULLUPDATE;
                    strcpy(title,buf);
                    break;
                */
            case '7':
                mode=DIR_MODE_SUPERFITER;
                break;
            default:
                mode=DIR_MODE_NORMAL;
                break;
        }
    }
    switch (mode) {
        case DIR_MODE_NORMAL:
            /*
            case DIR_MODE_DIGEST:
                return digest_mode(conf,fh,NULL);
            case DIR_MODE_THREAD:
                return title_mode(conf,fh,NULL);
            case DIR_MODE_MARK:
                return marked_mode(conf,fh,NULL);
            case DIR_MODE_ORIGIN:
            case DIR_MODE_AUTHOR:
            case DIR_MODE_TITLE:
                return search_mode(conf,fh,mode,buf);
            */
        case DIR_MODE_SUPERFITER:
            return super_filter(conf,fh,(int*)1);
        default:
            break;
    }
    return DIRCHANGED;
}

struct key_command mail_comms[] = {
    {'s', (READ_KEY_FUNC)mail_showsize,NULL},
    {'d', (READ_KEY_FUNC)mail_del,NULL},
    {'D',(READ_KEY_FUNC)delete_range,NULL},
//added by bad 03-2-10
    {'E', (READ_KEY_FUNC)mail_edit,NULL},
    {'T', (READ_KEY_FUNC)mail_edit_title,NULL},
    {'r', (READ_KEY_FUNC)mail_read,NULL},
    {'R', (READ_KEY_FUNC)do_mail_reply,NULL},
    {'m', (READ_KEY_FUNC)mail_mark,NULL},
    {'t',(READ_KEY_FUNC)mail_token,NULL},
    {'M', (READ_KEY_FUNC)mail_move,NULL},
    {'i', (READ_KEY_FUNC)mail_to_tmp,NULL},
    {'F', (READ_KEY_FUNC)mail_forward,NULL},
    {Ctrl('P'), (READ_KEY_FUNC)m_sendnull,NULL},
    /*
     * Added by ming, 96.10.9
     */
    {'a', (READ_KEY_FUNC)auth_search,(void*)false},
    {'A', (READ_KEY_FUNC)auth_search,(void*)true},
    {'/', (READ_KEY_FUNC)title_search,(void*)false},
    {'?', (READ_KEY_FUNC)title_search,(void*)true},
#if 0
    {']', (READ_KEY_FUNC)thread_read,(void*)SR_NEXT},
    {'[', (READ_KEY_FUNC)thread_read,(void*)SR_PREV},
    {'\\', (READ_KEY_FUNC)thread_read,(void*)SR_LAST},
    {'=', (READ_KEY_FUNC)thread_read,(void*)SR_FIRST},
#endif
    {Ctrl('N'), (READ_KEY_FUNC)thread_read,(void*)SR_FIRSTNEW},
    {'z', (READ_KEY_FUNC)read_sendmsgtoauthor,NULL},

    {Ctrl('A'), (READ_KEY_FUNC)read_showauthor,NULL},
    {'~',(READ_KEY_FUNC)read_authorinfo,NULL},
    {Ctrl('W'), (READ_KEY_FUNC)read_showauthorBM,NULL},
    {Ctrl('O'), (READ_KEY_FUNC)read_addauthorfriend,NULL},

    {Ctrl('Y'), (READ_KEY_FUNC)read_zsend,NULL},
    {Ctrl('C'), (READ_KEY_FUNC)do_cross,NULL},
    {Ctrl('G'), (READ_KEY_FUNC)change_mail_mode,(void*)0},   /* bad : 2002.8.8 add marked mode */

#ifdef PERSONAL_CORP
    {'y', (READ_KEY_FUNC)read_importpc,NULL},
#endif
    {Ctrl('C'), (READ_KEY_FUNC)do_cross,NULL},

    {'h', (READ_KEY_FUNC)mailreadhelp,NULL},

    {Ctrl('D'),(READ_KEY_FUNC)mail_add_ignore,NULL},//etnlegend,2005.09.21,信件列表状态添加用户到黑名单

    {',', (READ_KEY_FUNC)read_splitscreen,NULL},

    {'\'',(READ_KEY_FUNC)post_search,(void*)2UL},
    {'\"',(READ_KEY_FUNC)post_search,(void*)3UL},

    {'\0', NULL},
};

int m_read(void)
{
    char curmaildir[STRLEN];
    int returnmode=CHANGEMODE;
    int oldmode;

    oldmode = uinfo.mode;
    modify_user_mode(MAIL);
#ifdef NEW_HELP
    int oldhelpmode = helpmode;
#endif

    setmailfile(curmaildir, getCurrentUser()->userid, DOT_DIR);
    in_mail = true;
#ifdef NEW_HELP
    helpmode = HELP_MAIL;
#endif
    while (returnmode==CHANGEMODE) {
        returnmode = new_i_read(DIR_MODE_MAIL, curmaildir, mailtitle, (READ_ENT_FUNC) maildoent, &mail_comms[0], sizeof(struct fileheader));
        setmailfile(curmaildir, getCurrentUser()->userid, DOT_DIR);
    }
#ifdef NEW_HELP
    helpmode = oldhelpmode;
#endif
    in_mail = false;
    modify_user_mode(oldmode);
    return FULLUPDATE /* 0 */ ;
}

int g_send(void)
{
    char uident[13], tmp[3];
    int cnt, i, n, fmode = false;
    char maillists[STRLEN];
    struct userec *lookupuser;
    struct user_info *u;
    int gdataret;

    /*
     * 封禁Mail Bigman:2000.8.22
     */
    if (HAS_PERM(getCurrentUser(), PERM_DENYMAIL))
        return DONOTHING;

    modify_user_mode(SMAIL);
    clear();
    sethomefile(maillists, getCurrentUser()->userid, "maillist");
    cnt = listfilecontent(maillists);
    while (1) {
        if (cnt > maxrecp - 10) {
            move(2, 0);
            prints("目前限制寄信给 \033[1m%d\033[m 人", maxrecp);
        }
        gdataret = getdata(0, 0, "(A)增加 (D)删除 (I)引入好友 (C)清除目前名单 (E)放弃 (S)寄出? [S]： ", tmp, 2, DOECHO, NULL, true);
        if (gdataret == -1) {
            cnt = 0;
            break;
        }
        if (tmp[0] == '\n' || tmp[0] == '\0' || tmp[0] == 's' || tmp[0] == 'S') {
            break;
        }
        if (tmp[0] == 'a' || tmp[0] == 'd' || tmp[0] == 'A' || tmp[0] == 'D') {
            move(1, 0);
            if (tmp[0] == 'a' || tmp[0] == 'A')
                usercomplete("请依次输入使用者代号(只按 ENTER 结束输入): ", uident);
            else
                namecomplete("请依次输入使用者代号(只按 ENTER 结束输入): ", uident);
            move(1, 0);
            clrtoeol();
            if (uident[0] == '\0')
                continue;
            if (!getuser(uident, &lookupuser)) {
                move(2, 0);
                prints("这个使用者代号是错误的.\n");
                continue;
            } else
                strcpy(uident, lookupuser->userid);
        }
        switch (tmp[0]) {
            case 'A':
            case 'a':
                if (!(lookupuser->userlevel & PERM_BASIC)) {
                    move(2, 0);
                    prints("信件无法被寄给: \033[1m%s\033[m\n", lookupuser->userid);
                    break;
                } else if (seek_in_file(maillists, uident, NULL)) {
                    move(2, 0);
                    prints("已经列为收件人之一 \n");
                    break;
                }
                addtofile(maillists, uident);
                cnt++;
                break;
            case 'E':
            case 'e':
                cnt = 0;
                break;
            case 'D':
            case 'd': {
                if (seek_in_file(maillists, uident, NULL)) {
                    del_from_file(maillists, uident);
                    cnt--;
                }
                break;
            }
            case 'I':
            case 'i':
                n = 0;
                clear();
                u = get_utmpent(getSession()->utmpent);
                for (i = cnt; i < maxrecp && n < u->friendsnum; i++) {
                    int key;

                    move(2, 0);
                    prints("%s\n", getuserid2(u->friends_uid[n]));
                    move(4, 0);
                    clrtoeol();
                    move(3, 0);
                    n++;
                    if (!fmode) {
                        prints("(A)剩下的全部加入 (Y)加入 (N)不加入 (Q)结束? [Y]:");
                        /*
                         * TODO: add KEY_REFRESH support
                         */
                        key = igetkey();
                    } else
                        key = 'Y';
                    if (key == 'q' || key == 'Q')
                        break;
                    if (key == 'A' || key == 'a') {
                        fmode = true;
                        key = 'Y';
                    }
                    if (key == '\0' || key == '\n' || key == 'y' || key == 'Y' || '\r' == key) {
                        struct userec *lookupuser;
                        char *errstr;
                        char *touserid = getuserid2(u->friends_uid[n - 1]);

                        errstr = NULL;
                        if (!touserid) {
                            errstr = "这个使用者代号是错误的.\n";
                        } else {
                            strcpy(uident, getuserid2(u->friends_uid[n - 1]));
                            if (!getuser(uident, &lookupuser)) {
                                errstr = "这个使用者代号是错误的.\n";
                            } else if (!(lookupuser->userlevel & PERM_BASIC)) {
                                errstr = "信件无法被寄给他\n";
                            } else if (seek_in_file(maillists, uident, NULL)) {
                                i--;
                                continue;
                            }
                        }
                        if (errstr) {
                            if (fmode != true) {
                                move(4, 0);
                                prints(errstr);
                                pressreturn();
                            }
                            i--;
                            continue;
                        }
                        addtofile(maillists, uident);
                        cnt++;
                    }
                }
                fmode = false;
                clear();
                break;
            case 'C':
            case 'c':
                unlink(maillists);
                cnt = 0;
                break;
        }
        if (tmp[0] == 'e' || tmp[0] == 'E')
            break;
        move(5, 0);
        clrtobot();
        if (cnt > maxrecp)
            cnt = maxrecp;
        move(3, 0);
        clrtobot();
        listfilecontent(maillists);
    }
    if (cnt > 0) {
        G_SENDMODE = 2;
        switch (do_gsend(NULL, NULL, cnt)) {
            case -1:
                prints("信件目录错误\n");
                break;
            case -2:
                prints("取消发信\n");
                break;
            case -4:
                prints("信箱已经超出限额\n");
                break;
            default:
                prints("信件已寄出\n");
        }
        G_SENDMODE = 0;
        pressreturn();
    }
    return 0;
}

/*Add by SmallPig*/
int do_gsend(char *userid[], char *title, int num)
{
    struct stat st;
    char buf2[256], buf3[STRLEN], buf4[STRLEN];
    int replymode = 1;          /* Post New UI */
    char ans[4], include_mode = 'Y';
    char filepath[STRLEN], tmpfile[STRLEN];
    int cnt;
    FILE *mp=NULL;
    extern char quote_title[120];
    int oldmode;

    /*
     * 添加在好友寄信时的发信上限限制 Bigman 2000.12.11
     */
    if (chkusermail(getCurrentUser()) >= 2) {
        move(1, 0);
        prints("你的信箱已经超出限额，无法转寄信件。\n");
        pressreturn();
        return -4;
    }

    in_mail = true;
    oldmode = uinfo.mode;
    modify_user_mode(SMAIL);
#if defined(MAIL_REALNAMES)
    sprintf(genbuf, "%s (%s)", getCurrentUser()->userid, getCurrentUser()->realname);
#else
    /*
     * sprintf(genbuf,"%s (%s)",getCurrentUser()->userid,getCurrentUser()->username) ;
     */
    strcpy(genbuf, getCurrentUser()->userid);        /* Leeward 98.04.14 */
#endif
    move(1, 0);
    clrtoeol();
    if (!title) {
        replymode = 0;
        title = "没主题";
        buf4[0] = '\0';
    } else
        buf4[0] = ' ';

    gettmpfilename(tmpfile, "bbs-gsend");
    //sprintf(tmpfile, "tmp/bbs-gsend/%05d", getpid());
    /*
     * Leeward 98.01.17 Prompt whom you are writing to
     * if (1 == G_SENDMODE)
     * strcpy(lookupuser->userid, "好友名单");
     * else if (2 == G_SENDMODE)
     * strcpy(lookupuser->userid, "寄信名单");
     * else
     * strcpy(lookupuser->userid, "多位网友");
     */

    if (getCurrentUser()->signature > getSession()->currentmemo->ud.signum)
        getCurrentUser()->signature = 1;
    while (1) {
        sprintf(buf3, "引言模式 [\033[1m%c\033[m]", include_mode);
        move(t_lines - 3, 0);
        clrtoeol();
        prints("使用标题: \033[1m%-50s\033[m\n", (title[0] == '\0') ? "[正在设定标题]" : title);
        clrtoeol();
        if (getCurrentUser()->signature < 0)
            prints("使用随机签名档     %s", (replymode) ? buf3 : "");
        else
            prints("使用第 \033[1m%d\033[m 个签名档     %s", getCurrentUser()->signature, (replymode) ? buf3 : "");

        if (buf4[0] == '\0' || buf4[0] == '\n') {
            move(t_lines - 1, 0);
            clrtoeol();
            getdata(t_lines - 1, 0, "标题: ", buf4, 50, DOECHO, NULL, true);
            if ((buf4[0] == '\0' || buf4[0] == '\n')) {
                buf4[0] = ' ';
                continue;
            }
            title = buf4;
            continue;
        }
        move(t_lines - 1, 0);
        clrtoeol();
        /*
         * Leeward 98.09.24 add: viewing signature(s) while setting post head
         */
        sprintf(buf2, "按\033[1;32m0\033[m~\033[1;32m%d/V/L\033[m选/看/随机签名档%s，\033[1;32mT\033[m改标题，\033[1;32mEnter\033[m接受所有设定: ", getSession()->currentmemo->ud.signum,
                (replymode) ? "，\033[1;32mY\033[m/\033[1;32mN\033[m/\033[1;32mR\033[m/\033[1;32mA\033[m改引言模式" : "");
        getdata(t_lines - 1, 0, buf2, ans, 3, DOECHO, NULL, true);
        ans[0] = toupper(ans[0]);       /* Leeward 98.09.24 add; delete below toupper */
        if ((ans[0] - '0') >= 0 && ans[0] - '0' <= 9) {
            if (atoi(ans) <= getSession()->currentmemo->ud.signum)
                getCurrentUser()->signature = atoi(ans);
        } else if ((ans[0] == 'Y' || ans[0] == 'N' || ans[0] == 'A' || ans[0] == 'R') && replymode) {
            include_mode = ans[0];
        } else if (ans[0] == 'T') {
            buf4[0] = '\0';
        } else if (ans[0] == 'V') {     /* Leeward 98.09.24 add: viewing signature(s) while setting post head */
            sethomefile(buf2, getCurrentUser()->userid, "signatures");
            move(t_lines - 1, 0);
            if (askyn("预设显示前三个签名档, 要显示全部吗", false) == true)
                ansimore(buf2, 0);
            else {
                clear();
                ansimore2(buf2, false, 0, 18);
            }
        } else if (ans[0] == 'L') {
            getCurrentUser()->signature = -1;
        } else {
            strncpy(save_title, title, STRLEN);
            break;
        }
    }

    /*
     * Bigman:2000.8.13 群体发信为什么要引用文章呢
     */
    /*
     * do_quote( tmpfile,include_mode );
     */

    strcpy(quote_title, save_title);
    if (vedit(tmpfile, true, NULL, NULL,0) == -1) {
        unlink(tmpfile);
        clear();
        modify_user_mode(oldmode);
        return -2;
    }
    clear();
    if (G_SENDMODE == 2) {
        char maillists[STRLEN];

        sethomefile(maillists, getCurrentUser()->userid, "maillist");
        if ((mp = fopen(maillists, "r")) == NULL) {
            modify_user_mode(oldmode);
            return -3;
        }
    }

    for (cnt = 0; cnt < num; cnt++) {
        char uid[13];
        char buf[STRLEN];
        struct userec *user;

        if (G_SENDMODE == 1)
            getuserid(uid, get_utmpent(getSession()->utmpent)->friends_uid[cnt]);
        else if (G_SENDMODE == 2) {
            if (fgets(buf, STRLEN, mp) != NULL) {
                if (strtok(buf, " \n\r\t") != NULL)
                    strcpy(uid, buf);
                else
                    continue;
            } else {
                cnt = num;
                continue;
            }
        } else
            strcpy(uid, userid[cnt]);
        setmailpath(filepath, uid);
        if (stat(filepath, &st) == -1) {
            if (mkdir(filepath, 0755) == -1) {
                if (G_SENDMODE == 2)
                    fclose(mp);
                modify_user_mode(oldmode);
                return -1;
            }
        } else {
            if (!(st.st_mode & S_IFDIR)) {
                if (G_SENDMODE == 2)
                    fclose(mp);
                modify_user_mode(oldmode);
                return -1;
            }
        }

        if (getuser(uid, &user) == 0) {
            prints("找不到用户%s,请按 Enter 键继续向其他人发信...", uid);
            pressreturn();
            clear();
        } else if (user->userlevel & PERM_SUICIDE) {
            prints("%s 自杀中，不能收信，请按 Enter 键继续向其他人发信...", uid);
            pressreturn();
            clear();
        } else if (!(user->userlevel & PERM_BASIC)) {
            prints("%s 没有收信的权力，不能收信，请按 Enter 键继续向其他人发信...", uid);
            pressreturn();
            clear();
        } else if (chkusermail(user) >= 3) { /*Haohamru.99.4.05 */
            prints("%s 信箱已满,无法收信,请按 Enter 键继续向其他人发信...", uid);
            pressreturn();
            clear();
        } else /* 修正好友发信的错误 Bigman 2000.9.8 */ if (false == canIsend2(getCurrentUser(), uid)) {     /* Leeward 98.04.10 */
            char tmp_title[STRLEN], save_title_bak[STRLEN];

            prints("\033[1m\033[33m很抱歉∶系统无法向 %s 发出此信．因为 %s 拒绝接收您的信件．\n\n请按 Enter 键继续向其他人发信...\033[m\033[m\n\n", uid, uid);
            pressreturn();
            clear();
            strcpy(save_title_bak, save_title);
            sprintf(tmp_title, "退信∶ %s 拒绝接收您的信件．", uid);
            mail_file(getCurrentUser()->userid, tmpfile, getCurrentUser()->userid, tmp_title, 0, NULL);
            strcpy(save_title, save_title_bak);
        } else {
            mail_file(getCurrentUser()->userid, tmpfile, uid, save_title, 0, NULL);
#ifdef AUTOREMAIL
            sethomefile(genbuf, uid, "autoremail");
            if (dashf(genbuf)) {
                sprintf(buf2, "[自动回复]%s的信件自动回复",uid);
                mail_file(uid, genbuf, getCurrentUser()->userid, buf2, BBSPOST_COPY, NULL);
            }
            setmailcheck(getCurrentUser()->userid);
#endif
        }
    }
    mail_file_sent(".group", tmpfile, getCurrentUser()->userid, save_title, 0, getSession());
    unlink(tmpfile);
    if (G_SENDMODE == 2)
        fclose(mp);
    modify_user_mode(oldmode);
    return 0;
}

/*Add by SmallPig*/
int ov_send(void)
{
    int all, i;
    struct user_info *u;

    /*
     * 封禁Mail Bigman:2000.8.22
     */
    if (HAS_PERM(getCurrentUser(), PERM_DENYMAIL))
        return DONOTHING;

    modify_user_mode(SMAIL);
    move(1, 0);
    clrtobot();
    move(2, 0);
    u = get_utmpent(getSession()->utmpent);
    prints("寄信给好友名单中的人，目前本站限制仅可以寄给 \033[1m%d\033[m 位。\n", maxrecp);
    if (u->friendsnum <= 0) {
        prints("你并没有设定好友。\n");
        pressanykey();
        clear();
        return 0;
    } else {
        prints("名单如下：\n");
    }
    G_SENDMODE = 1;
    all = (u->friendsnum >= maxrecp) ? maxrecp : u->friendsnum;
    for (i = 0; i < all; i++) {
        char *userid;

        userid = getuserid2(u->friends_uid[i]);
        if (!userid)
            prints("\x1b[1;32m%-12s\x1b[m ", u->friends_uid[i]);
        else
            prints("%-12s ", userid);
        if ((i + 1) % 6 == 0)
            prints("\n");
    }
    pressanykey();
    switch (do_gsend(NULL, NULL, all)) {
        case -1:
            prints("信件目录错误\n");
            break;
        case -2:
            prints("信件取消\n");
            break;
        case -4:
            prints("信箱已经超出限额\n");
            break;
        default:
            prints("信件已寄出\n");
    }
    pressreturn();
    G_SENDMODE = 0;
    return 0;
}

int in_group(uident, cnt)
char uident[maxrecp][STRLEN];
int cnt;
{
    int i;

    for (i = 0; i < cnt; i++)
        if (!strcmp(uident[i], uident[cnt])) {
            return i + 1;
        }
    return 0;
}

int doforward(struct _select_def* conf, char *direct, struct fileheader *fh)
{
    static char address[STRLEN];
    char fname[STRLEN];
    char receiver[STRLEN];
    char title[STRLEN];
    int return_no;
    char tmp_buf[200];
    int y = 5;
    int noansi, mailout = 0;
    char *ptrX;
    int gdataret;

    clear();
    if (address[0] == '\0') {
//        strncpy(address, curruserdata.email, STRLEN);
        strncpy(address, getSession()->currentmemo->ud.email, STRLEN);
//        if (strstr(curruserdata.email, "@" MAIL_BBSDOMAIN) || strlen(curruserdata.email) == 0) {
        snprintf(tmp_buf, sizeof(tmp_buf), "@%s", MAIL_BBSDOMAIN);
        if (strstr(getSession()->currentmemo->ud.email, tmp_buf) || strlen(getSession()->currentmemo->ud.email) == 0) {
            strcpy(address, getCurrentUser()->userid);
        }
    }

    prints("请直接按 Enter 接受括号内提示的地址, 或者输入其他地址\n");
    prints("(如要转信到自己的BBS信箱,请直接输入你的ID作为地址即可)\n");
    prints("把 %s 的《%s》转寄给:", fh->owner, fh->title);
    sprintf(genbuf, "[%s]: ", address);
    gdataret = getdata(3, 0, genbuf, receiver, 70, DOECHO, NULL, true);
    if (gdataret == -1)
        return 1;
    if (receiver[0] != '\0') {
        strncpy(address, receiver, STRLEN);
    }
    /*
     * 确认地址是否正确 added by dong, 1998.10.1
     */
#ifdef HAVE_USERSCORE
    if (!sufficient_score_to_sendmail(getCurrentUser(), address)) {
        prints("\033[1m\033[33m您积分不足，不能给 %s 发信。\033[m\n\n", address);
        return -7;
    }
#endif
    sprintf(genbuf, "确定将文章寄给 %s 吗? (Y/N) [Y]: ", address);
    gdataret = getdata(3, 0, genbuf, receiver, 3, DOECHO, NULL, true);
    if (gdataret == -1 || receiver[0] == 'n' || receiver[0] == 'N')
        return 1;
    strncpy(receiver, address, STRLEN);

    if (invalidaddr(receiver))
        return -2;

    /*
     * ptrX = strstr(receiver, ".bbs@smth.org");
     * @smth.org @zixia.net 取到前面的用户即可
     */
    ptrX = strstr(receiver, email_domain());

    /*
     * disable by KCN      if (!ptrX) ptrX = strstr(receiver, ".bbs@");
     */
    if (ptrX && (ptrX > receiver) && '@' == *(ptrX - 1))
        *(ptrX - 1) = 0;


    mailout = (strstr(receiver, "@") || strstr(receiver, "."));
    if (!mailout) {
        /* F 到站外不检查信件是否超额 atppp 20060424 */
        if (chkusermail(getCurrentUser()) >= 2) {
            move(1, 0);
            prints("你的信箱已经超出限额，无法转寄信件。\n");
            pressreturn();
            return -4;
        }
        if (HAS_PERM(getCurrentUser(), PERM_DENYMAIL)) {
            prints("你尚无权限转寄信件给站内其它用户。");
            pressreturn();
            return -22;
        }
    }

    gettmpfilename(fname, "forward");
    //sprintf(fname, "tmp/forward/%s.%05d", getCurrentUser()->userid, getpid());
    /*
     * sprintf( tmp_buf, "cp %s/%s %s",
     * direct, fh->filename, fname);
     */
    /* 添加普通模式下同主题转寄功能, jiangjun, 2011-07-23 */
    while (1) {
        if (conf!=NULL && ((struct read_arg*)conf->arg)->mode==DIR_MODE_NORMAL) {
            if (askyn("是否以合集方式转寄同主题所有文章", 0) == 1) {
                int no_ref;
                no_ref=!askyn("是否保留引文", 1);
                get_thread_forward(conf, fh, title, &no_ref);
                gettmpfilename(tmp_buf, "ut", getpid());
                //get_effsize_attach(tmp_buf, &fh->attachment);
                f_mv(tmp_buf, fname);
                strcat(title, "(合集转寄)");
                if (!strncmp(title, "Re: ", 4))
                    strcpy(title, title+4);
                break;
            }
        }
        sprintf(tmp_buf, "%s/%s", direct, fh->filename);
        f_cp(tmp_buf, fname, 0);
        sprintf(title, "%.50s(转寄)", fh->title);   /*Haohmaru.00.05.01,moved here */
        break;
    }
    if (askyn("是否修改文章内容", 0) == 1) {
        int oldmode = uinfo.mode;
        long attachpos = fh->attachment;
        modify_user_mode(SMAIL);
        /* fancyrabbit Jun 5 2007 取消修改增加提示 */
        if (vedit(fname, false, NULL, &attachpos, 0) == -1) {
            if (!askyn("取消修改, 是否仍然转寄?", 0)) {
                unlink(fname);
                modify_user_mode(oldmode);
                return 1;
            }
        } else {
            fh->attachment = attachpos;
            if (ADD_EDITMARK)
                add_edit_mark(fname, 1, fh->title,getSession());
        }
        modify_user_mode(oldmode);
        y = 2;
        newbbslog(BBSLOG_USER, "修改被转贴的文章或信件: %s", title);    /*Haohmaru.00.05.01 */
        /*
         * clear();
         */
    }

    int mode; /*-1:精华区转寄, 其他: BBS_DIR_MODE */
    char board[STRLEN];
    strcpy(board, currboard->filename);
    if (conf) {
        mode = ((struct read_arg*)conf->arg)->mode;
    } else {
        mode = -1;
        /* 获得精华区文件所在版面 */
        char *t, buf[STRLEN];
        int i;
        strncpy(buf, direct, strlen(direct));
        t = buf;
        for (i=0;i<3;i++) {
            t=strchr(t, '/')+1;
        }
        strcpy(board, t);
        if ((t=strchr(board, '/'))!=NULL)
            *t = '\0';
    }
    write_forward_header(fname, fh->title, board, mode);
    get_effsize_attach(fname, &fh->attachment);

    /* 如果带有附件，选择是否连同附件一起转寄, jiangjun, 20110708 */
    if (fh->attachment) {
        if (askyn("您转寄的文章包含附件，是否连同附件一起转寄", 1) == 0) {
            FILE *fin, *fout;
            char buf[256];
            int size;
            gettmpfilename(tmp_buf, "forward.no.attach");
            if (!(fin=fopen(fname,"r")))
                return -1;
            if (!(fout=fopen(tmp_buf,"w"))) {
                fclose(fin);
                unlink(fname);
                return -1;
            }
            while ((size=-attach_fgets(buf,256,fin))) {
                if (size<0)
                    fprintf(fout,"%s",buf);
                else
                    break;
            }
            fclose(fin);
            fclose(fout);
            f_mv(tmp_buf, fname);
            fh->attachment = 0;
        }
    }



    if (!mailout) {     /* sending local file need not uuencode or convert to big5... */
        struct userec *lookupuser;

        prints("转寄信件给 %s, 请稍候....\n", receiver);

        return_no = getuser(receiver, &lookupuser);
        if (return_no == 0) {
            return_no = 1;
            prints("使用者找不到...\n");
        } else {                /* 查完后应该使用lookupuser中的内容,保证大小写正确  period 2000-12-13 */
            strncpy(receiver, lookupuser->userid, IDLEN + 1);
            receiver[IDLEN] = 0;

            /*
             * if(!chkreceiver(receiver,NULL))Haohamru.99.4.05
             * FIXME NULL -> lookupuser，在 zixia.net 上是这么改的... 有没有问题？
             */
            if (!HAS_PERM(getCurrentUser(), PERM_SYSOP) && lookupuser->userlevel & PERM_SUICIDE) {
                prints("%s 自杀中，不能收信\n", receiver);
                return -5;
            }
            if (!HAS_PERM(getCurrentUser(), PERM_SYSOP) && !(lookupuser->userlevel & PERM_BASIC)) {
                prints("%s 没有收信的权力，不能收信\n", receiver);
                return -5;
            }


            if (!HAS_PERM(getCurrentUser(), PERM_SYSOP) && chkusermail(lookupuser) >= 3) {        /*Haohamru.99.4.05 */
                prints("%s 信箱已满,无法收信\n", receiver);
                return -4;
            }

            if (false == canIsend2(getCurrentUser(), receiver)) {    /* Leeward 98.04.10 */
                prints("\033[1m\033[33m很抱歉∶系统无法转寄此信．因为 %s 拒绝接收您的信件．\033[m\033[m\n\n", receiver);
                sprintf(title, "退信∶ %s 拒绝接收您的信件．", receiver);
                mail_file(getCurrentUser()->userid, fname, getCurrentUser()->userid, title, BBSPOST_COPY, NULL);
                return -4;
            }
            return_no = mail_file(getCurrentUser()->userid, fname, lookupuser->userid, title, BBSPOST_COPY, fh);
            /* fancyrabbit Jun 5 2007 转寄信件保存到发件箱，F 判断太多就不判断了，按设置来吧 ...*/
            if (HAS_MAILBOX_PROP(&uinfo, MBP_SAVESENTMAIL) && strcmp(lookupuser -> userid, getCurrentUser() -> userid))
                mail_file_sent(lookupuser -> userid, fname, getCurrentUser()->userid, title, BBSPOST_COPY, getSession());
#ifdef AUTOREMAIL
            sethomefile(genbuf, lookupuser->userid, "autoremail");
            if (dashf(genbuf)) {
                char buf2[256];
                sprintf(buf2, "[自动回复]%s的信件自动回复",lookupuser->userid);
                mail_file(lookupuser->userid, genbuf, getCurrentUser()->userid, buf2, BBSPOST_COPY, NULL);
            }
            setmailcheck(getCurrentUser()->userid);
#endif
        }
    } else {
        /*
         * Add by ming, 96.10.9
         */
        char data[3];
        int isbig5;

#ifdef NEWSMTH
        char *ptr;
        ptr = strchr(receiver, '@');
#ifdef SECONDSITE
        if (strcasecmp(ptr, "@bbs.newsmth.net") == 0)
#else
        if (strcasecmp(ptr, "@2.newsmth.net") == 0)
#endif
        {
            noansi = 0;
            isbig5 = 0;
        } else {
#endif /* NEWSMTH */
            data[0] = 0;
            prints("若您要将信件转寄到台湾请输入 Y 或 y\n");
            getdata(7, 0, "转成BIG5码? [N]: ", data, 2, DOECHO, 0, 0);
            if (data[0] == 'y' || data[0] == 'Y')
                isbig5 = 1;
            else
                isbig5 = 0;

            data[0] = 0;
            getdata(8, 0, "过滤ANSI控制符? [Y]: ", data, 2, DOECHO, 0, 0);
            if (data[0] == 'n' || data[0] == 'N')
                noansi = 0;
            else
                noansi = 1;
#ifdef NEWSMTH
        }
#endif

        prints("转寄信件给 %s, 请稍候....\n", receiver);

        return_no = bbs_sendmail(fname, title, receiver, isbig5, noansi,getSession());
        if (return_no) return_no = -2;
    }
    if (return_no==0)
        newbbslog(BBSLOG_USER, "forwarded file to %s", receiver);
    unlink(fname);
    return (return_no);
}




struct command_def {
    char *prompt;
    int permission;
    int (*func)();
    void *arg;
};

const static char *mail_sysbox[] = {
    ".DIR",
    ".SENT",
    ".DELETED"
};

const static char *mail_sysboxtitle[] = {
    "I)收件箱",
    "P)发件箱",
    "J)垃圾箱",
};

static int m_clean()
{
    char buf[40];
    int num;
    int savemode = uinfo.mode;

    move(0, 0);
    uinfo.mode = RMAIL;
    setmailfile(buf, getCurrentUser()->userid, mail_sysbox[1]);
    num = get_num_records(buf, sizeof(struct fileheader));
    if (num && askyn("清除发件箱么?", 0))
        delete_range_base(getCurrentUser()->userid,mail_sysbox[1],mail_sysbox[2],1,num,
                          (DELETE_RANGE_BASE_MODE_MAIL|DELETE_RANGE_BASE_MODE_RANGE),NULL,NULL);
    move(0, 0);
    setmailfile(buf, getCurrentUser()->userid, mail_sysbox[2]);
    num = get_num_records(buf, sizeof(struct fileheader));
    if (num && askyn("清除垃圾箱么?", 0))
        delete_range_base(getCurrentUser()->userid,mail_sysbox[2],NULL,1,num,
                          (DELETE_RANGE_BASE_MODE_MAIL|DELETE_RANGE_BASE_MODE_RANGE),NULL,NULL);
    uinfo.mode = savemode;
    return 0;
}

int m_sendnull(void)
{
    if (HAS_PERM(getCurrentUser(),PERM_LOGINOK))
        m_send(NULL);
    return FULLUPDATE;
}

const static struct command_def mail_cmds[] = {
    {"N) 览阅新信件", 0, m_new, NULL},
    {"R) 览阅全部信件", 0, m_read, NULL},
#ifdef ENABLE_REFER
    {"K) @我的文章", 0, refer_at, NULL},
    {"L) 回复我的文章", 0, refer_reply, NULL},
#ifdef ENABLE_REFER_LIKE
    {"B) Like我的文章", 0, refer_like, NULL},
#endif /* ENABLE_REFER_LIKE */
#endif
// new msg system, added by windinsn, Jan 21, 2013
#ifdef ENABLE_NEW_MSG
    {"T) 短信", 0, new_msg_main, NULL},
    {"W) 发送短信", 0, new_msg_compose, NULL},
#endif
    {"S) 寄信", PERM_LOGINOK, m_sendnull, NULL},
#ifdef MAILOUT
    {"I) 发送站外信件", PERM_LOGINOK, m_internet, NULL},
#endif
    {"G) 群体信件选单", PERM_LOGINOK, set_mailgroup_list, NULL},
    /*
     * {"O)┌设定好友名单", 0, t_override, NULL},
     */
    {"F) 寄信给所有好友", PERM_LOGINOK, ov_send, NULL},
    {"C) 清空备份的邮箱", 0, m_clean, NULL},
    {"X) 设置邮箱选项", 0, set_mailbox_prop, NULL},
    {"M) 寄信给所有人", PERM_SYSOP, mailall, NULL},
};

struct mail_proc_arg {
    int leftpos;
    int rightpos;
    int cmdnum;
    int sysboxnum;
    int numbers;
    int tmpnum;
    int flag;

    int cmdptr[sizeof(mail_cmds) / sizeof(struct command_def)];
};

static int maillist_refresh(struct _select_def *conf)
{
    int i;

    clear();
    docmdtitle("[处理信笺选单]",
               "主选单[\x1b[1;32m←\x1b[0;37m,\x1b[1;32me\x1b[0;37m] 进入[\x1b[1;32mEnter\x1b[0;37m] 选择[\x1b[1;32m↑\x1b[0;37m,\x1b[1;32m↓\x1b[0;37m] 左右切换[\x1b[1;32mTab\x1b[m] 添加[\x1b[1;32ma\x1b[0;37m] 改名[\x1b[1;32mT\x1b[0;37m] 删除[\x1b[1;32md\x1b[0;37m]\x1b[m");
    update_endline();

    move(2, 0);
    prints("%s", "\x1b[1;44;37m──功能选单─────────────┬────自定义邮箱───────");
    for (i=0; i<scr_cols/2-36; i++)
        prints("─");
    for (i = 3; i < scr_lns - 1; i++) {
        move(i, 38);
        prints("%s", "\x1b[1;44;37m│\x1b[m");
    }
    move(17, 0);
    prints("%s", "\x1b[1;44;37m──系统预定义邮箱──────────┤\x1b[m");

    if (user_mail_list.mail_list_t == 0) {
        move(14, 46);
        prints("%s", "无自定义邮箱");
    }
    return SHOW_CONTINUE;
}
static int maillist_show(struct _select_def *conf, int pos)
{
    struct mail_proc_arg *arg = (struct mail_proc_arg *) conf->arg;
    char buf[80];

    if (pos <= arg->cmdnum) {
        /*
         * 邮箱命令
         */
        outs(mail_cmds[arg->cmdptr[pos - 1]].prompt);
    } else if (pos <= arg->sysboxnum + arg->cmdnum) {
        int sel;

        sel = pos - arg->cmdnum - 1;
        if (arg->flag)
            outs(mail_sysbox[sel]+1);
        else
            outs(mail_sysboxtitle[sel]);

        setmailfile(buf, getCurrentUser()->userid, mail_sysbox[sel]);
        prints("(%d)", getmailnum(buf));
    } else {
        /*
         * 自定义邮箱
         */
        int sel;
        char dirbstr[60];

        sel = pos - arg->cmdnum - arg->sysboxnum;
        if (sel < 10)
            outc(' ');
        sprintf(dirbstr, ".%s", user_mail_list.mail_list[sel - 1] + 30);
        if (arg->flag)
            prints("%d) %s", sel, user_mail_list.mail_list[sel - 1] + 30);
        else
            prints("%d) %s", sel, user_mail_list.mail_list[sel - 1]);
        setmailfile(buf, getCurrentUser()->userid, dirbstr);
        prints("(%d)", getmailnum(buf));
    }
    return SHOW_CONTINUE;
}

static int maillist_onselect(struct _select_def *conf)
{
    struct mail_proc_arg *arg = (struct mail_proc_arg *) conf->arg;
    char buf[20];
    char curmaildir[STRLEN];
    int returnmode=CHANGEMODE;
#ifdef NEW_HELP
    int oldhelpmode = helpmode;
#endif

    if (conf->pos <= arg->cmdnum) {
        /*
         * 邮箱命令
         */
        (*mail_cmds[arg->cmdptr[conf->pos - 1]].func)();
    } else if (conf->pos <= arg->sysboxnum + arg->cmdnum) {
        int sel;

        sel = conf->pos - arg->cmdnum - 1;
        setmailfile(curmaildir,getCurrentUser()->userid, mail_sysbox[sel]);
        in_mail = true;
#ifdef NEW_HELP
        helpmode = HELP_MAIL;
#endif
        while (returnmode==CHANGEMODE) {
            returnmode = new_i_read(DIR_MODE_MAIL, curmaildir, mailtitle, (READ_ENT_FUNC) maildoent, &mail_comms[0], sizeof(struct fileheader));
            setmailfile(curmaildir, getCurrentUser()->userid, mail_sysbox[sel]);
        }
#ifdef NEW_HELP
        helpmode = oldhelpmode;
#endif
        in_mail = false;
        /*
         * 系统邮箱
         */
    } else {
        /*
         * 自定义邮箱
         */
        int sel;

        sel = conf->pos - arg->sysboxnum - arg->cmdnum - 1;
        sprintf(buf, ".%s", user_mail_list.mail_list[sel] + 30);
        setmailfile(curmaildir, getCurrentUser()->userid, buf);
        in_mail = true;
#ifdef NEW_HELP
        helpmode = HELP_MAIL;
#endif
        while (returnmode==CHANGEMODE) {
            returnmode = new_i_read(DIR_MODE_MAIL, curmaildir, mailtitle, (READ_ENT_FUNC) maildoent, &mail_comms[0], sizeof(struct fileheader));
            setmailfile(curmaildir, getCurrentUser()->userid, buf);
        }
#ifdef NEW_HELP
        helpmode = oldhelpmode;
#endif
        in_mail = false;
    }
    modify_user_mode(MAIL);
    return SHOW_REFRESH;
}

static int maillist_prekey(struct _select_def *conf, int *command)
{
    struct mail_proc_arg *arg = (struct mail_proc_arg *) conf->arg;

    /*
     * 如果是左键并且到了左边
     */
    if (*command == KEY_RIGHT) {
        if ((user_mail_list.mail_list_t == 0) || (conf->pos > arg->cmdnum + arg->sysboxnum))
            *command = '\n';
        else
            *command = '\t';
    }
    if (*command == KEY_LEFT) {
        if ((conf->pos <= arg->cmdnum + arg->sysboxnum))
            return SHOW_QUIT;
        else {
            *command = '\t';
            return SHOW_CONTINUE;
        }
    }

    if (*command == 'e')
        return SHOW_QUIT;
    update_endline();
    if (!isdigit(*command))
        arg->tmpnum = -1;
    return SHOW_CONTINUE;
}

static int maillist_key(struct _select_def *conf, int command)
{
    struct mail_proc_arg *arg = (struct mail_proc_arg *) conf->arg;
    int i;

    if (command == '\t') {
        if (conf->pos <= arg->cmdnum + arg->sysboxnum) {
            /*
             * 左边
             */
            if (!user_mail_list.mail_list_t)
                return SHOW_CONTINUE;
            arg->leftpos = conf->pos;
            conf->new_pos = arg->rightpos;
        } else {
            arg->rightpos = conf->pos;
            conf->new_pos = arg->leftpos;
        }
        return SHOW_SELCHANGE;
    }
    if (toupper(command) == 'H') {
        mailreadhelp(NULL,NULL,NULL);
        return SHOW_REFRESH;
    }

    if (toupper(command) == 'Z') {
        arg->flag=!arg->flag;
        return SHOW_REFRESH;
    }
    if (toupper(command) == 'A') {
        char bname[STRLEN], buf[PATHLEN];
        int i = 0, y, x;
        struct stat st;

        if (!HAS_PERM(getCurrentUser(), PERM_LOGINOK))
            return SHOW_CONTINUE;
        if (user_mail_list.mail_list_t >= MAILBOARDNUM) {
            move(2, 0);
            clrtoeol();
            prints("邮箱数已经达上限(%d)！", MAILBOARDNUM);
            pressreturn();
            return SHOW_REFRESH;
        }
        move(0, 0);
        clrtoeol();
        getdata(0, 0, "输入自定义邮箱显示中文名: ", buf, 30, DOECHO, NULL, true);
        if (buf[0] == 0) {
            return SHOW_REFRESH;
        }
        strncpy(user_mail_list.mail_list[user_mail_list.mail_list_t], buf, 29);
        move(0, 0);
        clrtoeol();
        while (1) {
            i++;
            sprintf(bname, ".MAILBOX%d", i);
            setmailfile(buf, getCurrentUser()->userid, bname);
            if (stat(buf, &st) == -1)
                break;
        }
        sprintf(bname, "MAILBOX%d", i);
        f_touch(buf);
        strncpy(user_mail_list.mail_list[user_mail_list.mail_list_t] + 30, bname, 9);
        user_mail_list.mail_list_t++;
        save_mail_list(&user_mail_list,getSession());
        x = 0;

        y = 3 + (20 - user_mail_list.mail_list_t) / 2;
        arg->numbers++;
        conf->item_count = arg->numbers;
        conf->item_per_page = arg->numbers;
        for (i = arg->cmdnum + arg->sysboxnum; i < arg->cmdnum + arg->sysboxnum + user_mail_list.mail_list_t; i++) {
            conf->item_pos[i].x = 44;
            conf->item_pos[i].y = y + i - arg->cmdnum - arg->sysboxnum;
        }
        return SHOW_REFRESH;
    }
    if (toupper(command) == 'D') {
        char bname[STRLEN], buf[PATHLEN];
        int p = 1, i, j;
        char ans[2];
        int y;

        if (!HAS_PERM(getCurrentUser(), PERM_LOGINOK))
            return SHOW_CONTINUE;
        if (conf->pos <= arg->cmdnum + arg->sysboxnum)
            return SHOW_CONTINUE;
        move(0, 0);
        clrtoeol();
        sprintf(bname, ".%s", user_mail_list.mail_list[conf->pos - arg->cmdnum - arg->sysboxnum - 1] + 30);
        setmailfile(buf, getCurrentUser()->userid, bname);
        if (get_num_records(buf, sizeof(struct fileheader)) != 0) {
            prints("目录非空，请先删除里面的信件。");
            pressreturn();
            return SHOW_REFRESH;
        }
        getdata(0, 0, "确认删除整个目录？(y/N)", ans, 2, DOECHO, NULL, true);
        p = ans[0] == 'Y' || ans[0] == 'y';
        if (p) {
            f_rm(buf);
            p = conf->pos - arg->cmdnum - arg->sysboxnum - 1;
            for (j = p; j < user_mail_list.mail_list_t - 1; j++)
                memcpy(user_mail_list.mail_list[j], user_mail_list.mail_list[j + 1], sizeof(user_mail_list.mail_list[j]));
            user_mail_list.mail_list_t--;
            save_mail_list(&user_mail_list,getSession());
            y = 3 + (20 - user_mail_list.mail_list_t) / 2;
            arg->numbers--;
            conf->item_count = arg->numbers;
            conf->item_per_page = arg->numbers;
            if (conf->pos > arg->numbers)
                conf->pos = arg->numbers;
            for (i = arg->cmdnum + arg->sysboxnum; i < arg->cmdnum + arg->sysboxnum + user_mail_list.mail_list_t; i++) {
                conf->item_pos[i].x = 44;
                conf->item_pos[i].y = y + i - arg->cmdnum - arg->sysboxnum;
            }
            return SHOW_REFRESH;
        }
        return SHOW_REFRESH;
    }
    if (command == 'T') {
        int i;
        char bname[STRLEN];

        if (!HAS_PERM(getCurrentUser(), PERM_LOGINOK))
            return SHOW_CONTINUE;
        if (conf->pos <= arg->cmdnum + arg->sysboxnum)
            return SHOW_CONTINUE;
        move(0, 0);
        clrtoeol();
        i = conf->pos - arg->cmdnum - arg->sysboxnum - 1;
        strcpy(bname, user_mail_list.mail_list[i]);
        getdata(0, 0, "输入信箱中文名: ", bname, 30, DOECHO, NULL, false);
        if (bname[0]) {
            strcpy(user_mail_list.mail_list[i], bname);
            save_mail_list(&user_mail_list,getSession());
            return SHOW_REFRESH;
        }
        return SHOW_REFRESH;
    }

    for (i = 0; i < arg->cmdnum; i++)
        if (toupper(command) == mail_cmds[i].prompt[0]) {
            conf->new_pos = i + 1;
            return SHOW_SELCHANGE;
        }
    for (i = 0; i < arg->sysboxnum; i++)
        if (toupper(command) == mail_sysboxtitle[i][0]) {
            conf->new_pos = i + arg->cmdnum + 1;
            return SHOW_SELCHANGE;
        }
    if (isdigit(command)) {
        int num;

        num = command - '0';
        if ((arg->tmpnum == -1) || (num + arg->tmpnum * 10 > user_mail_list.mail_list_t)) {
            arg->tmpnum = command - '0';
            if (arg->tmpnum <= user_mail_list.mail_list_t) {
                conf->new_pos = arg->tmpnum + arg->cmdnum + arg->sysboxnum;
                return SHOW_SELCHANGE;
            }
        } else {
            conf->new_pos = arg->tmpnum * 10 + arg->cmdnum + arg->sysboxnum + num;
            arg->tmpnum = command - '0';
            return SHOW_SELCHANGE;
        }
    }
    return SHOW_CONTINUE;
}

int MailProc(void)
{
    struct _select_def maillist_conf;
    struct mail_proc_arg arg;
    POINT *pts;
    int i;
    int y;
    int oldmode;

    oldmode = uinfo.mode;
    modify_user_mode(MAIL);
    clear();
    bzero(&arg,sizeof(arg));
    arg.tmpnum = -1;
    arg.cmdnum = 0;
    for (i = 0; i < sizeof(mail_cmds) / sizeof(struct command_def); i++) {
        if (HAS_PERM(getCurrentUser(), mail_cmds[i].permission)) {
            arg.cmdptr[arg.cmdnum] = i;
            arg.cmdnum++;
        }
    }
    arg.sysboxnum = sizeof(mail_sysbox) / sizeof(char *);
    arg.numbers = user_mail_list.mail_list_t + arg.cmdnum + arg.sysboxnum;
    arg.leftpos = 2;
    arg.rightpos = arg.cmdnum + arg.sysboxnum + 1;
    pts = (POINT *) malloc(sizeof(POINT) * (arg.numbers + MAILBOARDNUM));

    /*
     * 计算邮箱命令地位置
     */
    y = 2 + (17 - arg.cmdnum) / 2;
    for (i = 0; i < arg.cmdnum; i++) {
        pts[i].x = 6;
        pts[i].y = y + i;
    }

    y = 18 + (5 - arg.sysboxnum) / 2;
    /*
     * 计算系统邮箱地位置
     */
    for (; i < arg.cmdnum + arg.sysboxnum; i++) {
        pts[i].x = 6;
        pts[i].y = y + i - arg.cmdnum;
    }
    /*
     * 计算自定义邮箱地位置
     */

    y = 3 + (20 - user_mail_list.mail_list_t) / 2;
    for (; i < arg.cmdnum + arg.sysboxnum + user_mail_list.mail_list_t; i++) {
        pts[i].x = 44;
        pts[i].y = y + i - arg.cmdnum - arg.sysboxnum;
    }
    bzero((char *) &maillist_conf, sizeof(struct _select_def));
    maillist_conf.item_count = arg.numbers;
    maillist_conf.item_per_page = arg.numbers;
    maillist_conf.flag = LF_BELL | LF_LOOP;     /*|LF_HILIGHTSEL; */
    maillist_conf.prompt = "◆";
    maillist_conf.item_pos = pts;
    maillist_conf.arg = &arg;
    maillist_conf.title_pos.x = 1;
    maillist_conf.title_pos.y = 6;

#ifdef ENABLE_REFER
/**
 *  通过用户的提示状态定位光标位置
 *  added by windinsn, Jan 29, 2012
 */
    switch(chkmail()) {
        case 2: 
            maillist_conf.pos=8;
            break;
        case 1:
            maillist_conf.pos=2;
            break;
        case 3:
            maillist_conf.pos=3;
            break;
        case 4:
            maillist_conf.pos=4;
            break;
#ifdef ENABLE_NEW_MSG
        case 5:
            maillist_conf.pos=6;
            break;
#endif
#ifdef ENABLE_REFER_LIKE
        case 6:
            maillist_conf.pos=5;
            break;
#endif
        default:
            maillist_conf.pos=2;
    }    
#else
    maillist_conf.pos = 2;
#endif

    maillist_conf.on_select = maillist_onselect;
    maillist_conf.show_data = maillist_show;
    maillist_conf.pre_key_command = maillist_prekey;
    maillist_conf.key_command = maillist_key;
    maillist_conf.show_title = maillist_refresh;

    list_select_loop(&maillist_conf);
    free(pts);
    modify_user_mode(oldmode);
    return 0;
}

typedef struct {
    unsigned int prop;
    unsigned int oldprop;
} mailbox_prop_arg;

static int set_mailbox_prop_select(struct _select_def *conf)
{
    mailbox_prop_arg *arg = (mailbox_prop_arg *) conf->arg;

    if (conf->pos == conf->item_count)
        return SHOW_QUIT;
    arg->prop ^= (1 << (conf->pos - 1));
    return SHOW_REFRESHSELECT;
}

static int set_mailbox_prop_show(struct _select_def *conf, int i)
{
    mailbox_prop_arg *arg = (mailbox_prop_arg *) conf->arg;

    i = i - 1;
    if (i == conf->item_count - 1) {
        prints("%c. 退出 ", 'A' + i);
    } else {
        if ((arg->prop & (1 << i)) != (arg->oldprop & (1 << i)))
            prints("%c. %-50s \033[31;1m%3s\033[m", 'A' + i, mailbox_prop_str[i], ((arg->prop >> i) & 1 ? "ON" : "OFF"));
        else
            prints("%c. %-50s \033[37;0m%3s\033[m", 'A' + i, mailbox_prop_str[i], ((arg->prop >> i) & 1 ? "ON" : "OFF"));
    }
    return SHOW_CONTINUE;
}

static int set_mailbox_prop_key(struct _select_def *conf, int key)
{
    int sel;

    if (key == Ctrl('Q'))
        return SHOW_QUIT;
    if (key == Ctrl('A')) {
        mailbox_prop_arg *arg = (mailbox_prop_arg *) conf->arg;

        arg->prop = arg->oldprop;
        return SHOW_QUIT;
    }
    if (key <= 'z' && key >= 'a')
        sel = key - 'a';
    else
        sel = key - 'A';
    if (sel >= 0 && sel < (conf->item_count)) {
        conf->new_pos = sel + 1;
        return SHOW_SELCHANGE;
    }
    return SHOW_CONTINUE;
}

/**
 * Setting getCurrentUser()'s mailbox properties.
 *
 * @authur flyriver
 */
int set_mailbox_prop()
{
    struct _select_def proplist_conf;
    mailbox_prop_arg arg;
    POINT *pts;
    int i;

    clear();
    move(0, 0);
    prints("设定邮箱属性，\033[1;32mCtrl+Q\033[m退出，\033[1;32mCtrl+A\033[m放弃修改退出.\n");
    arg.prop = load_mailbox_prop(getCurrentUser()->userid);
    arg.oldprop = arg.prop;
    pts = (POINT *) malloc(sizeof(POINT) * (MBP_NUMS + 1));
    for (i = 0; i < MBP_NUMS + 1; i++) {
        pts[i].x = 2;
        pts[i].y = i + 2;
    }
    bzero(&proplist_conf, sizeof(struct _select_def));
    proplist_conf.item_count = MBP_NUMS + 1;
    proplist_conf.item_per_page = MBP_NUMS + 1;
    proplist_conf.flag = LF_BELL | LF_LOOP;
    proplist_conf.prompt = "◆";
    proplist_conf.item_pos = pts;
    proplist_conf.arg = &arg;
    proplist_conf.title_pos.x = 1;
    proplist_conf.title_pos.y = 2;
    proplist_conf.pos = MBP_NUMS + 1;

    proplist_conf.on_select = set_mailbox_prop_select;
    proplist_conf.show_data = set_mailbox_prop_show;
    proplist_conf.key_command = set_mailbox_prop_key;

    list_select_loop(&proplist_conf);
    free(pts);
    uinfo.mailbox_prop = update_mailbox_prop(getCurrentUser()->userid, arg.prop);
    store_mailbox_prop(getCurrentUser()->userid);

    return 0;
}

typedef struct {
    mailgroup_list_t *mgl;
    int entry;
    mailgroup_t *users;
} mailgroup_arg;

const static struct key_translate mail_key_table[]= {
    {'$',KEY_END},
    {'q',KEY_LEFT},
    {'e',KEY_LEFT},
    {'k',KEY_UP},
    {'j',KEY_DOWN},
    {'N',KEY_PGDN},
    {Ctrl('F'),KEY_PGDN},
    {' ',KEY_PGDN},
    {'p',KEY_PGDN},
    {Ctrl('B'),KEY_PGUP},
    {-1,-1}
};

static int set_mailgroup_select(struct _select_def *conf)
{
    mailgroup_arg *arg = (mailgroup_arg *) conf->arg;
    int oldmode;

    oldmode = uinfo.mode;
    t_query(arg->users[conf->pos - 1].id);
    modify_user_mode(oldmode);

    return SHOW_REFRESH;
}

static int set_mailgroup_show(struct _select_def *conf, int i)
{
    mailgroup_arg *arg = (mailgroup_arg *) conf->arg;

    prints(" %3d  %-12s  %-14s", i, arg->users[i - 1].id, arg->users[i - 1].exp);
    return SHOW_CONTINUE;
}

static int set_mailgroup_key(struct _select_def *conf, int key)
{
    mailgroup_arg *arg = (mailgroup_arg *) conf->arg;
    int oldmode;

    switch (key) {
        case 'a':                  /* add new user */
            if (arg->mgl->groups[arg->entry].users_num < MAX_MAILGROUP_USERS) {
                mailgroup_t user;

                bzero(&user, sizeof(user));
                clear();
                move(1, 0);
                usercomplete("请输入要增加的用户代号: ", user.id);
                if (user.id[0] != '\0') {
                    if (searchuser(user.id) <= 0) {
                        move(2, 0);
                        prints(MSG_ERR_USERID);
                        pressanykey();
                    } else {
                        move(2, 0);
                        getdata(2, 0, "请输入用户说明: ", user.exp, sizeof(user.exp), DOECHO, NULL, true);
                        add_mailgroup_user(arg->mgl, arg->entry, arg->users, &user);
                    }
                }
                return SHOW_DIRCHANGE;
            }
            break;
        case 'd':                  /* delete existed user */
            if (arg->mgl->groups[arg->entry].users_num > 0) {
                char ans[3];

                getdata(t_lines - 1, 0, "确实要从组中删除该用户吗(Y/N)? [N]: ", ans, sizeof(ans), DOECHO, NULL, true);
                if (ans[0] == 'Y' || ans[0] == 'y') {
                    delete_mailgroup_user(arg->mgl, arg->entry, arg->users, conf->pos - 1);
                }
                return SHOW_DIRCHANGE;
            }
            break;
        case 'T':                  /* modify existed user */
            if (arg->mgl->groups[arg->entry].users_num > 0) {
                mailgroup_t user;

                memcpy(&user, &(arg->users[conf->pos - 1]), sizeof(user));
                getdata(0, 0, "请输入新用户说明: ", user.exp, sizeof(user.exp), DOECHO, NULL, true);
                if (strlen(user.exp) > 0)
                    modify_mailgroup_user(arg->users, conf->pos - 1, &user);
                return SHOW_DIRCHANGE;
            }
            break;
        case 'm':                  /* send mail to a user */
            if (arg->mgl->groups[arg->entry].users_num > 0) {
                oldmode = uinfo.mode;
                modify_user_mode(FRIEND);   /* FIXME: A temporary workaround for
                                         * the buggy m_send() function. */
                m_send(arg->users[conf->pos - 1].id);
                modify_user_mode(oldmode);
                return SHOW_REFRESH;
            }
            break;
        case 'z':                  /* send message to a user */
            if (arg->mgl->groups[arg->entry].users_num > 0) {
                struct user_info *uin;

                if (!HAS_PERM(getCurrentUser(), PERM_PAGE))
                    break;
                oldmode = uinfo.mode;
                clear();
                uin = (struct user_info *) t_search(arg->users[conf->pos - 1].id, 0);
                if (!uin || !canmsg(getCurrentUser(), uin))
                    do_sendmsg(NULL, NULL, 0);
                else {
                    strcpy(getSession()->MsgDesUid, uin->userid);
                    do_sendmsg(uin, NULL, 0);
                }
                modify_user_mode(oldmode);
                return SHOW_REFRESH;
            }
            break;
        case Ctrl('Z'):
            oldmode = uinfo.mode;
            r_lastmsg();
            modify_user_mode(oldmode);
            return SHOW_REFRESH;
        case 'L':
        case 'l':
            oldmode = uinfo.mode;
            show_allmsgs();
            modify_user_mode(oldmode);
            return SHOW_REFRESH;
        case 'W':
        case 'w':
            oldmode = uinfo.mode;
            if (!HAS_PERM(getCurrentUser(), PERM_PAGE))
                break;
            s_msg();
            modify_user_mode(oldmode);
            return SHOW_REFRESH;
        case Ctrl('A'):
            clear();
            t_query(arg->users[conf->pos - 1].id);
            return SHOW_REFRESH;
        case 'u':
            oldmode = uinfo.mode;
            clear();
            modify_user_mode(QUERY);
            t_query(NULL);
            modify_user_mode(oldmode);
            clear();
            return SHOW_REFRESH;
    }

    return SHOW_CONTINUE;
}

static int set_mailgroup_refresh(struct _select_def *conf)
{
    clear();
    docmdtitle("[设置群体信件组]",
               "退出[\x1b[1;32m←\x1b[0;37m,\x1b[1;32me\x1b[0;37m] 进入[\x1b[1;32mEnter\x1b[0;37m] 选择[\x1b[1;32m↑\x1b[0;37m,\x1b[1;32m↓\x1b[0;37m] 添加[\x1b[1;32ma\x1b[0;37m] 修改说明[\x1b[1;32mT\x1b[0;37m] 删除[\x1b[1;32md\x1b[0;37m]\x1b[m 发信[\x1b[1;32mm\x1b[0;37m]\x1b[m");
    move(2, 0);
    prints("\033[0;1;37;44m  %4s  %-12s  %-58s", "编号", "用户代号", "用户说明");
    clrtoeol();
    update_endline();
    return SHOW_CONTINUE;
}

static int set_mailgroup_getdata(struct _select_def *conf, int pos, int len)
{
    mailgroup_arg *arg = (mailgroup_arg *) conf->arg;

    conf->item_count = arg->mgl->groups[arg->entry].users_num;

    return SHOW_CONTINUE;
}

static int init_mailgroup(mailgroup_list_t * mgl, int entry, mailgroup_t * users)
{
    mailgroup_t user;
    int ret = 0;

    clear();
    move(0, 0);
    prints("初始化群体信件组用户向导\n");
    bzero(&user, sizeof(user));
    move(1, 0);
    usercomplete("请输入要增加的用户代号: ", user.id);
    if (user.id[0] != '\0') {
        if (searchuser(user.id) <= 0) {
            move(2, 0);
            prints(MSG_ERR_USERID);
            pressanykey();
        } else {
            move(2, 0);
            getdata(2, 0, "请输入用户说明: ", user.exp, sizeof(user.exp), DOECHO, NULL, true);
            add_mailgroup_user(mgl, entry, users, &user);
            move(3, 0);
            prints("初始化完成!\n");
            pressanykey();
            ret = 1;
        }
    }
    return ret;
}

/**
 * Setting getCurrentUser()'s mailgroup.
 *
 * @authur flyriver
 */
int set_mailgroup(mailgroup_list_t * mgl, int entry, mailgroup_t * users)
{
    struct _select_def group_conf;
    mailgroup_arg arg;
    POINT *pts;
    int i;

    arg.mgl = mgl;
    arg.entry = entry;
    arg.users = users;

    bzero(&group_conf, sizeof(struct _select_def));
    group_conf.item_count = load_mailgroup(getCurrentUser()->userid, mgl->groups[entry].group_name, users, mgl->groups[entry].users_num);
    if (group_conf.item_count == 0) {
        group_conf.item_count = init_mailgroup(mgl, entry, users);
        if (group_conf.item_count == 0)
            return -1;
    }

    clear();
    //TODO: 窗口大小动态改变的情况？这里有bug
    pts = (POINT *) malloc(sizeof(POINT) * BBS_PAGESIZE);
    for (i = 0; i < BBS_PAGESIZE; i++) {
        pts[i].x = 2;
        pts[i].y = i + 3;
    }
    group_conf.key_table = (struct key_translate*)mail_key_table;
    group_conf.item_per_page = BBS_PAGESIZE;
    /*
     * 加上 LF_VSCROLL 才能用 LEFT 键退出
     */
    group_conf.flag = LF_VSCROLL | LF_BELL | LF_LOOP | LF_MULTIPAGE;
    group_conf.prompt = "◆";
    group_conf.item_pos = pts;
    group_conf.arg = &arg;
    group_conf.title_pos.x = 0;
    group_conf.title_pos.y = 0;
    group_conf.pos = 1;         /* initialize cursor on the first mailgroup */
    group_conf.page_pos = 1;    /* initialize page to the first one */

    group_conf.on_select = set_mailgroup_select;
    group_conf.show_data = set_mailgroup_show;
    group_conf.key_command = set_mailgroup_key;
    group_conf.show_title = set_mailgroup_refresh;
    group_conf.get_data = set_mailgroup_getdata;

    list_select_loop(&group_conf);
    store_mailgroup(getCurrentUser()->userid, mgl->groups[entry].group_name, users, mgl->groups[entry].users_num);
    free(pts);

    return 0;
}

typedef struct {
    mailgroup_list_t mail_group;
    mailgroup_t users[MAX_MAILGROUP_USERS];
} mailgroup_list_arg;

static int set_mailgroup_list_select(struct _select_def *conf)
{
    mailgroup_list_arg *arg = (mailgroup_list_arg *) conf->arg;

    bzero(arg->users, sizeof(mailgroup_t) * MAX_MAILGROUP_USERS);
    set_mailgroup(&(arg->mail_group), conf->pos - 1, arg->users);

    return SHOW_REFRESH;
}

static int set_mailgroup_list_show(struct _select_def *conf, int i)
{
    mailgroup_list_arg *arg = (mailgroup_list_arg *) conf->arg;

    prints("  %2d  %-40s  %3d", i, arg->mail_group.groups[i - 1].group_desc, arg->mail_group.groups[i - 1].users_num);
    return SHOW_CONTINUE;
}

static int set_mailgroup_list_key(struct _select_def *conf, int key)
{
    mailgroup_list_arg *arg = (mailgroup_list_arg *) conf->arg;
    int oldmode;

    switch (key) {
        case 'a':                  /* add new mailgroup */
            if (arg->mail_group.groups_num < MAX_MAILGROUP_NUM) {
                mailgroup_list_item item;
                char ans[3];
                char filename[STRLEN];
                int y = 0;
                int initialized = 0;

                clear();
                sethomefile(filename, getCurrentUser()->userid, "friends");
                if (dashf(filename)) {
                    getdata(y, 0, "是否导入好友名单(Y/N)? [Y]: ", ans, sizeof(ans), DOECHO, NULL, true);
                    y++;
                    if (ans[0] == '\0' || ans[0] == 'Y' || ans[0] == 'y') {
                        move(y, 0);
                        prints("导入好友名单... ");
                        import_friends_mailgroup(getCurrentUser()->userid, &(arg->mail_group));
                        initialized++;
                        prints("[\033[0;1;32m成功\033[m]\n");
                        y++;
                    }
                }
                if (initialized == 0) {
                    bzero(&item, sizeof(item));
                    getdata(y, 0, "请输入新群体信件组的名称: ", item.group_desc, sizeof(item.group_desc), DOECHO, NULL, true);
                    add_mailgroup_item(getCurrentUser()->userid, &(arg->mail_group), &item);
                }
                pressanykey();
                return SHOW_DIRCHANGE;
            }
            break;
        case 'd':                  /* delete existed mailgroup */
            if (arg->mail_group.groups_num > 0) {
                char ans[3];

                getdata(t_lines - 1, 0, "确实要删除该群体信件组吗(Y/N)? [N]: ", ans, sizeof(ans), DOECHO, NULL, true);
                if (ans[0] == 'Y' || ans[0] == 'y') {
                    delete_mailgroup_item(getCurrentUser()->userid, &(arg->mail_group), conf->pos - 1);
                    if (conf->item_count == 0) {
                        add_default_mailgroup_item(getCurrentUser()->userid, &(arg->mail_group),getSession());
                    }
                }
                return SHOW_DIRCHANGE;
            }
            break;
        case 'T':                  /* modify existed mailgroup */
            if (arg->mail_group.groups_num > 0) {
                mailgroup_list_item item;

                memcpy(&item, &(arg->mail_group.groups[conf->pos - 1]), sizeof(item));
                getdata(0, 0, "请输入新群体信件组的名称: ", item.group_desc, sizeof(item.group_desc), DOECHO, NULL, true);
                if (strlen(item.group_desc) > 0)
                    modify_mailgroup_item(getCurrentUser()->userid, &(arg->mail_group), conf->pos - 1, &item);
                return SHOW_DIRCHANGE;
            }
            break;
        case 'm':
            if (arg->mail_group.groups_num > 0 && arg->mail_group.groups[conf->pos - 1].users_num > 0) {
                char **mg_users;
                int cnt;
                int i;

                if (HAS_PERM(getCurrentUser(), PERM_DENYMAIL)) {
                    break;
                }

                cnt = arg->mail_group.groups[conf->pos - 1].users_num;
                mg_users = (char **) malloc(cnt * sizeof(char *));
                if (mg_users == NULL)
                    break;
                load_mailgroup(getCurrentUser()->userid, arg->mail_group.groups[conf->pos - 1].group_name, arg->users, cnt);
                for (i = 0; i < cnt; i++)
                    mg_users[i] = arg->users[i].id;
                clear();
                G_SENDMODE = 0;
                switch (do_gsend(mg_users, NULL, cnt)) {
                    case -1:
                        prints("信件目录错误\n");
                        break;
                    case -2:
                        prints("取消发信\n");
                        break;
                    case -4:
                        prints("信箱已经超出限额\n");
                        break;
                    default:
                        prints("信件已寄出\n");
                }
                free(mg_users);
                pressreturn();
                return SHOW_REFRESH;
            }
            break;
        case Ctrl('Z'):
            oldmode = uinfo.mode;
            r_lastmsg();
            modify_user_mode(oldmode);
            return SHOW_REFRESH;
        case 'L':
        case 'l':
            oldmode = uinfo.mode;
            show_allmsgs();
            modify_user_mode(oldmode);
            return SHOW_REFRESH;
        case 'W':
        case 'w':
            oldmode = uinfo.mode;
            if (!HAS_PERM(getCurrentUser(), PERM_PAGE))
                break;
            s_msg();
            modify_user_mode(oldmode);
            return SHOW_REFRESH;
        case 'u':
            oldmode = uinfo.mode;
            clear();
            modify_user_mode(QUERY);
            t_query(NULL);
            modify_user_mode(oldmode);
            clear();
            return SHOW_REFRESH;
    }

    return SHOW_CONTINUE;
}

static int set_mailgroup_list_refresh(struct _select_def *conf)
{
    clear();
    docmdtitle("[群体信件选单]",
               "退出[\x1b[1;32m←\x1b[0;37m,\x1b[1;32me\x1b[0;37m] 进入[\x1b[1;32mEnter\x1b[0;37m] 选择[\x1b[1;32m↑\x1b[0;37m,\x1b[1;32m↓\x1b[0;37m] 添加[\x1b[1;32ma\x1b[0;37m] 改名[\x1b[1;32mT\x1b[0;37m] 删除[\x1b[1;32md\x1b[0;37m]\x1b[m 发群体信[\x1b[1;32mm\x1b[0;37m]\x1b[m");
    move(2, 0);
    prints("\033[0;1;37;44m  %4s  %-40s %-31s", "编号", "群体信件组名称", "人数");
    clrtoeol();
    update_endline();
    return SHOW_CONTINUE;
}

static int set_mailgroup_list_getdata(struct _select_def *conf, int pos, int len)
{
    mailgroup_list_arg *arg = (mailgroup_list_arg *) conf->arg;

    conf->item_count = arg->mail_group.groups_num;

    return SHOW_CONTINUE;
}

static int init_mailgroup_list(mailgroup_list_t * mgl)
{
    char filename[STRLEN];
    char ans[3];
    int y = 2;
    int initialized = 0;

    move(0, 0);
    prints("初始化群体信件分组向导\n");
    sethomefile(filename, getCurrentUser()->userid, "maillist");
    if (dashf(filename)) {
        getdata(y, 0, "是否导入老版本的群体信件组(Y/N)? [Y]: ", ans, sizeof(ans), DOECHO, NULL, true);
        if (ans[0] == '\0' || ans[0] == 'Y' || ans[0] == 'y') {
            y++;
            move(y, 0);
            prints("导入老版本的群体信件组... ");
            import_old_mailgroup(getCurrentUser()->userid, mgl);
            unlink(filename);
            initialized++;
            prints("[\033[0;1;32m成功\033[m]\n");
            y++;
        }
    }
    sethomefile(filename, getCurrentUser()->userid, "friends");
    if (dashf(filename)) {
        getdata(y, 0, "是否导入好友名单(Y/N)? [Y]: ", ans, sizeof(ans), DOECHO, NULL, true);
        if (ans[0] == '\0' || ans[0] == 'Y' || ans[0] == 'y') {
            y++;
            move(y, 0);
            prints("导入好友名单... ");
            import_friends_mailgroup(getCurrentUser()->userid, mgl);
            initialized++;
            prints("[\033[0;1;32m成功\033[m]\n");
            y++;
        }
    }
    if (initialized == 0) {
        add_default_mailgroup_item(getCurrentUser()->userid, mgl,getSession());
        initialized++;
    }
    move(y, 0);
    prints("初始化完成！\n");
    pressanykey();
    return initialized;
}

/**
 * Setting getCurrentUser()'s mailgroup lists.
 *
 * @authur flyriver
 */
int set_mailgroup_list()
{
    struct _select_def grouplist_conf;
    mailgroup_list_arg *arg;
    POINT *pts;
    int i;
    int oldmode;

    clear();
    arg = (mailgroup_list_arg *) malloc(sizeof(mailgroup_list_arg));
    if (arg == NULL)
        return -1;
    oldmode = uinfo.mode;
    modify_user_mode(MAIL);
    pts = (POINT *) malloc(sizeof(POINT) * BBS_PAGESIZE);
    for (i = 0; i < BBS_PAGESIZE; i++) {
        pts[i].x = 2;
        pts[i].y = i + 3;
    }
    bzero(&grouplist_conf, sizeof(struct _select_def));
    grouplist_conf.item_count = load_mailgroup_list(getCurrentUser()->userid, &(arg->mail_group));
    if (grouplist_conf.item_count == 0) {
        grouplist_conf.item_count = init_mailgroup_list(&(arg->mail_group));
        clear();
    }
    grouplist_conf.item_per_page = BBS_PAGESIZE;
    /*
     * 加上 LF_VSCROLL 才能用 LEFT 键退出
     */
    grouplist_conf.key_table=(struct key_translate*)mail_key_table;
    grouplist_conf.flag = LF_NUMSEL | LF_VSCROLL | LF_BELL | LF_LOOP | LF_MULTIPAGE;
    grouplist_conf.prompt = "◆";
    grouplist_conf.item_pos = pts;
    grouplist_conf.arg = arg;
    grouplist_conf.title_pos.x = 0;
    grouplist_conf.title_pos.y = 0;
    grouplist_conf.pos = 1;     /* initialize cursor on the first mailgroup */
    grouplist_conf.page_pos = 1;        /* initialize page to the first one */

    grouplist_conf.on_select = set_mailgroup_list_select;
    grouplist_conf.show_data = set_mailgroup_list_show;
    grouplist_conf.key_command = set_mailgroup_list_key;
    grouplist_conf.show_title = set_mailgroup_list_refresh;
    grouplist_conf.get_data = set_mailgroup_list_getdata;

    list_select_loop(&grouplist_conf);
    store_mailgroup_list(getCurrentUser()->userid, &(arg->mail_group));
    free(arg);
    free(pts);
    modify_user_mode(oldmode);

    return 0;
}

#ifdef ENABLE_REFER
#define REFER_ACTION_AUTH_SEARCH_PREV    1
#define REFER_ACTION_AUTH_SEARCH_NEXT    2
#define REFER_ACTION_TITLE_SEARCH_PREV   3
#define REFER_ACTION_TITLE_SEARCH_NEXT   4
#define REFER_ACTION_BOARD_SEARCH_PREV   5
#define REFER_ACTION_BOARD_SEARCH_NEXT   6
#define REFER_ACTION_SEND_MSG_TO_AUTH    7
#define REFER_ACTION_SHOW_AUTH           8
#define REFER_ACTION_AUTH_INFO           9
#define REFER_ACTION_AUTH_BM             10
#define REFER_ACTION_ADD_FRIEND          11
#define REFER_ACTION_ADD_IGNORE          12

char refer_title_bar[STRLEN];

int refer_read(struct _select_def* conf, struct refer *refer, void* extraarg) {
    int save_currboardent, save_uinfo_currentboard, fd, num, retnum, key, repeat, ret;
    char buf[PATHLEN];
    struct boardheader *board;
    fileheader_t article[1];
    struct read_arg *arg;

    if (refer==NULL)
        return DONOTHING;

    arg=(struct read_arg*)conf->arg;
    if (!(refer->flag&FILE_READ)) {
        int ent=conf->pos;
        refer->flag |= FILE_READ;
        substitute_record(arg->direct, refer, sizeof(*refer), ent, (RECORD_FUNC_ARG) refer_cmp, refer);

        if (ent>=arg->filecount)
            setmailcheck(getCurrentUser()->userid); 
    }

    board=(struct boardheader *)getbcache(refer->board);
    if (0==board||board->flag&BOARD_GROUP||!check_read_perm(getCurrentUser(), board)) {
        clear();
        prints("指定版面不存在...");
        pressreturn();
        return FULLUPDATE;
    }

    setbdir(DIR_MODE_NORMAL, buf, board->filename);
    if ((fd=open(buf, O_RDWR, 0644))<0) {
        clear();
        prints("无法打开版面文章列表...");
        pressreturn();
        return FULLUPDATE;
    }
    retnum=get_records_from_id(fd, refer->id, article, 1, &num);
    close(fd);
    if (0==retnum) {
        clear();
        prints("文章不存在，可能已被删除...");
        pressreturn();
        return FULLUPDATE;
    }
#ifdef ENABLE_BOARD_MEMBER
    if (!member_read_perm(board, article, getCurrentUser())) {
        clear();
        move(3, 10);
        prints("该版为驻版可读，非该版驻版用户不能查看版面文章！");
        move(4, 10);
        prints("详情请联系该版版主。");
        pressanykey();
        return FULLUPDATE;
    }
#endif

    setbfile(buf, board->filename, article->filename);
#ifdef NOREPLY
    key=ansimore_withzmodem(buf, true, article->title); 
#else
    key=ansimore_withzmodem(buf, false, article->title); 
#endif    
    
    save_currboardent=currboardent;
    save_uinfo_currentboard=uinfo.currentboard;

    currboardent=getbid(board->filename, NULL);
    currboard=board;
    uinfo.currentboard=currboardent;

#ifdef HAVE_BRC_CONTROL
    brc_initial(getCurrentUser()->userid, board->filename, getSession());
    brc_add_read(refer->id, currboardent, getSession());
#endif
    
    if (arg->readdata==NULL)
        arg->readdata=malloc(sizeof(struct refer));
    memcpy(arg->readdata, refer, sizeof(struct refer));

    ret=FULLUPDATE;

#ifndef NOREPLY
    move(t_lines-1, 0);

    prints("\033[44m\033[36m[通知模式] \033[32m[阅读文章]\033[33m 结束Q,| 上一篇 | 下一篇<空格>, | 同主题^x,p ");

    clrtoeol();
    resetcolor();


    if (!(key==KEY_RIGHT||key==KEY_PGUP||key==KEY_UP||key==KEY_DOWN)&&(!(key>0)||!strchr("RrEexp", key)))
        key=igetkey();

    repeat=0;
    do {
        if (repeat)
            key=igetkey();
    
        repeat=0;
        switch(key) {
            case KEY_LEFT:
            case 'Q':
            case 'q':
            case KEY_REFRESH:
                break;
            case 'R':
            case 'r':
            case 'Y':
            case 'y':
                clear();
                move(5,0);
                if(board->flag&BOARD_NOREPLY) {
                    prints("\t\t\033[1;33m%s\033[0;33m<Enter>\033[m","该版面已设置为不可回复文章...");
                    WAIT_RETURN;
                } else if (article->accessed[1]&FILE_READ && !HAS_PERM(getCurrentUser(), PERM_SYSOP)) {
                    prints("\t\t\033[1;33m%s\033[0;33m<Enter>\033[m","本文已设置为不可回复, 请勿试图讨论...");
                    WAIT_RETURN;
                } else
                    do_reply(conf, article);
                
                ret=DIRCHANGED;
                break;
            case Ctrl('R'):
                post_reply(conf, article, NULL);
                break; 
            case Ctrl('A'):
                clear();
                read_showauthor(conf,article,NULL);
                ret=READ_NEXT;
                break;
            case Ctrl('Z'):
            case 'H':
                r_lastmsg();
                break;
            case 'Z':
            case 'z':
                if (HAS_PERM(getCurrentUser(), PERM_PAGE)) {
                    read_sendmsgtoauthor(conf, article, NULL);
                    ret=READ_NEXT;
                }
                break;
            case 'u':
                clear();
                modify_user_mode(QUERY);
                t_query(NULL);
                break;
            case 'L':
                if (uinfo.mode==LOOKMSGS)
                    ret=DONOTHING;
                else
                    show_allmsgs();
                break;
            case 'o':
            case 'O':
                if (HAS_PERM(getCurrentUser(), PERM_BASIC)) {
                    t_friends();
                }
                break;
            case 'U':
                ret=board_query();
                break;
            case Ctrl('O'):
                clear();
                read_addauthorfriend(conf, article, NULL);
                ret=READ_NEXT;
                break;
            case '~':
                ret=read_authorinfo(conf, article, NULL);
                break;
            case Ctrl('W'):
                ret=read_showauthorBM(conf, article, NULL);
                break;  
            case KEY_RIGHT:
            case KEY_DOWN:
            case KEY_PGDN:
                ret=READ_NEXT;
                break;
            case KEY_UP:
                ret=READ_PREV;
                break;
            case 's':
            case 'S':
                savePos(DIR_MODE_NORMAL, NULL, num, board);
                ReadBoard();
                break;
            case '!':
                Goodbye();
                break;
        }
    } while(repeat);


#endif /* NOREPLY */

    uinfo.currentboard=save_uinfo_currentboard;
    currboardent=save_currboardent;
    currboard=((struct boardheader*)getboard(save_currboardent));

#ifdef HAVE_BRC_CONTROL
    if (currboard) {
        brc_initial(getCurrentUser()->userid, currboard->filename, getSession());
    }
#endif

    if (ret==FULLUPDATE&&arg->oldpos!=0) {
        conf->new_pos=arg->oldpos;
        arg->oldpos=0;
        list_select_add_key(conf, KEY_REFRESH);
        arg->readmode=READ_NORMAL;
        return SELCHANGE;
    }

    return ret;
}
int refer_board(struct _select_def* conf, struct refer *refer, void* extraarg) {
    int bid, save_currboardent, save_uinfo_currentboard, fd, pos;
    struct boardheader *board;
    fileheader_t article[1];
    char buf[STRLEN];

    if (refer==NULL)
        return DONOTHING;

    board=(struct boardheader *)getbcache(refer->board);
    if (0==board||board->flag&BOARD_GROUP||!check_read_perm(getCurrentUser(), board)) {
        clear();
        prints("指定版面不存在...");
        pressreturn();
        return FULLUPDATE;
    }    
    
    bid=getbnum_safe(board->filename, getSession(), 1);
    
    save_currboardent=currboardent;
    save_uinfo_currentboard=uinfo.currentboard;

    board_setcurrentuser(uinfo.currentboard, -1);
    uinfo.currentboard=bid;
    UPDATE_UTMP(currentboard, uinfo);
    board_setcurrentuser(uinfo.currentboard, 1);
    currboardent=bid;
    currboard=board;
    uinfo.currentboard=currboardent;

#ifdef HAVE_BRC_CONTROL
    brc_initial(getCurrentUser()->userid, board->filename,getSession());
#endif

    setbdir(DIR_MODE_NORMAL, buf, board->filename);
    pos=-1;
    if ((fd=open(buf, O_RDWR, 0644))>=0) {
        get_records_from_id(fd, refer->id, article, 1, &pos);
        close(fd);
    } 
    
    if (pos>0) {
        savePos(DIR_MODE_NORMAL, NULL, pos, board);
    }

    move(0,0);
    clrtoeol();
    move(1,0);
    clrtoeol();

    ReadBoard();

    
    uinfo.currentboard=save_uinfo_currentboard;
    currboardent=save_currboardent;
    currboard=((struct boardheader*)getboard(save_currboardent));
    UPDATE_UTMP(currentboard, uinfo);
    board_setcurrentuser(uinfo.currentboard, 1);
    
#ifdef HAVE_BRC_CONTROL
    brc_initial(getCurrentUser()->userid, currboard->filename, getSession());
#endif

    return DOQUIT;
}
int refer_del(struct _select_def* conf, struct refer *refer, void* extraarg) {
    int ent=conf->pos;
    struct read_arg* arg=conf->arg;

    if (refer==NULL)
        return DONOTHING;
    if (arg->mode!=DIR_MODE_REFER)
        return DONOTHING;
    if (refer_remove(arg->direct, ent, refer)==0)
        return DIRCHANGED;

    clear();
    move(2,0);
    prints("删除失败\n");
    pressreturn();
    clear();
    return FULLUPDATE;
}
#define KEY_BS 8
int get_field_input_number(int y, int x, int num, int total, int fw){
    char buf[STRLEN];
    int ch;

    while(1) {
        if (num)
            sprintf(buf, "%*d", fw, num);
        else
            sprintf(buf, "%*s", fw, " ");
        move(y, x+1);
        prints("\033[32;41;4m%s\033[m", buf);
        move(y, x+fw);
        ch = igetkey();
        if (ch==KEY_BS || ch==KEY_DEL)
            num = num / 10;
        else if (ch=='\r')
            break;
        else if (ch<'0' || ch>'9')
            continue;
        else {
           sprintf(buf, "%d", num);
           if (num==0 || strlen(buf)<fw)
               num = num * 10 + ch - '0';
        }
    }
    if (num==0 || num>total)
        return -1;
    return num;
}
#undef KEY_BS
int select_refer_del_range(int total, int fw, int *from, int *to){
    char buf[STRLEN];
    int start, end;

    sprintf(buf, "[ \033[32;41;4m%*s\033[m - \033[32;41;4m%*s\033[m ]", fw, " ", fw, " ");
    move(5, 21);
    prints(buf);

    start = end = 0;
    if ((start=get_field_input_number(5, 22, start, total, fw))<0)
        return -1;
    if ((end=get_field_input_number(5, 25+fw, end, total, fw))<0 || end<start)
        return -1;
    *from = start;
    *to = end;
    return 0;
}
int refer_range_del(struct _select_def* conf, struct refer *refer, void* extraarg) {
    int fw, ch, sel, newsel, ent, total, from, to;
    struct read_arg *arg=conf->arg;
    char buf[STRLEN];

    if (refer==NULL)
        return DONOTHING;
    if (arg->mode!=DIR_MODE_REFER)
        return DONOTHING;

    ent = conf->pos;
    total = arg->filecount;
    from = to = 0;
    newsel = sel = 0;

    move(0,0);
    clrtobot();
    prints("\033[1;32m[区段删除选单] \033[33m<Enter>键选择/<Esc/Q/←>键退出\033[m");

    sprintf(buf, "%d", total);
    fw = strlen(buf);

    sprintf(buf, "[\033[36m0\033[m] 全部         [ \033[31m%*d\033[m - \033[31m%*d\033[m ]", fw, 1, fw, total);
    move(2, 4);prints(buf);
    sprintf(buf, "[\033[36m1\033[m] 当前位置向前 [ \033[31m%*d\033[m - \033[31m%*d\033[m ]", fw, 1, fw, ent);
    move(3, 4);prints(buf);
    sprintf(buf, "[\033[36m2\033[m] 当前位置向后 [ \033[31m%*d\033[m - \033[31m%*d\033[m ]", fw, ent, fw, total);
    move(4, 4);prints(buf);
    sprintf(buf, "[\033[36m3\033[m] 指定删除区域");
    move(5, 4);prints(buf);
    move(sel+2, 2);prints("◇");

    while(1) {
        ch = igetkey();
        switch (ch) {
            case KEY_UP:
                newsel--;
                if (newsel<0)
                    newsel = 3;
                break;
            case KEY_DOWN:
                newsel++;
                if (newsel>3)
                    newsel = 0;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
                newsel = ch - '0';
                break;
            case KEY_ESC:
            case KEY_LEFT:
            case 'q':
            case 'Q':
                return FULLUPDATE;
            case '\r':
                if (sel == 0) {
                    from = 1;
                    to = total;
                } else if (sel == 1) {
                    from = 1;
                    to = ent;
                } else if (sel == 2) {
                    from = ent;
                    to = total;
                } else {
                    if (select_refer_del_range(total, fw, &from, &to)<0) {
                        prompt_return("指定区域输入错误", 2, 0);
                        move(5, 21);clrtobot();move(5, 4);
                        break;
                    }
                }
                move(7, 4);
                sprintf(buf, "区段删除 \033[31m%d\033[m - \033[31m%d\033[m，确认操作", from, to);
                if (askyn(buf, 0)==1) {
                    if (range_delete_records(arg->direct, sizeof(struct refer), from, to, NULL)!=0)
                        return prompt_return("删除失败", 2, 0);
                    prompt_return("删除完成", 0, 1);
                } else
                    prompt_return("放弃删除", 1, 1);
                return DIRCHANGED;
            default:
                break;
        }
        if (sel!=newsel) {
            move(sel+2, 2);prints("  ");
            sel = newsel;
            move(sel+2, 2);prints("◇");
        }
    }
    return FULLUPDATE;
}
int refer_set_read(struct _select_def* conf, struct refer *refer, void* extraarg) {
    struct read_arg *arg=conf->arg;

    if (refer==NULL)
        return DONOTHING;
    if (arg->mode!=DIR_MODE_REFER)
        return DONOTHING;

    refer_read_all(arg->direct);
    setmailcheck(getCurrentUser()->userid);
    return FULLUPDATE;
}
int refer_action(struct _select_def* conf, struct refer *refer, int act) {
    if (refer==NULL)
        return DONOTHING;

    int ret=FULLUPDATE;
    struct user_info *user;  
    char buf[STRLEN];
    char pmt[STRLEN];
    bool up=false;
    int search_mode=0;
    int search_length=STRLEN;
    static char title[STRLEN];

    switch(act) {
        case REFER_ACTION_SEND_MSG_TO_AUTH:
            if (HAS_PERM(getCurrentUser(), PERM_PAGE)) {
                clear();
                user=(struct user_info *)t_search(refer->user, false);
                if (!user||!canmsg(getCurrentUser(), user))
                    do_sendmsg(NULL, NULL, 0);
                else {
                    strncpy(getSession()->MsgDesUid, user->userid, 20);
                    do_sendmsg(user, NULL, 0);
                }
            } else 
                return DONOTHING;
            break;
        case REFER_ACTION_SHOW_AUTH:
            t_query(refer->user);
            break;
        case REFER_ACTION_AUTH_INFO:
#ifdef HAVE_STRICT_USERINFO
#define RAI_PERM    (PERM_ADMIN)
#else /* HAVE_STRICT_USERINFO */
#define RAI_PERM    (PERM_SYSOP|PERM_ADMIN)
#endif /* HAVE_STRICT_USERINFO */
             if (HAS_PERM(getCurrentUser(),RAI_PERM)) {
                 modify_userinfo(searchuser(refer->user),(HAS_PERM(getCurrentUser(),PERM_ADMIN)?1:2));
             }
#undef RAI_PERM            
            break;
        case REFER_ACTION_ADD_FRIEND:
            clear();
            if (strcmp("guest", getCurrentUser()->userid))
                addtooverride(refer->user);
            break;
        case REFER_ACTION_AUTH_SEARCH_PREV:
        case REFER_ACTION_AUTH_SEARCH_NEXT:
        case REFER_ACTION_TITLE_SEARCH_PREV:
        case REFER_ACTION_TITLE_SEARCH_NEXT:
        case REFER_ACTION_BOARD_SEARCH_PREV:
        case REFER_ACTION_BOARD_SEARCH_NEXT:
            if (REFER_ACTION_AUTH_SEARCH_PREV==act||REFER_ACTION_TITLE_SEARCH_PREV==act||REFER_ACTION_BOARD_SEARCH_PREV==act) 
                up=true;
            switch (act) {
                case REFER_ACTION_AUTH_SEARCH_PREV:
                case REFER_ACTION_AUTH_SEARCH_NEXT:
                    strcpy(buf, refer->user);
                    snprintf(pmt, STRLEN, "%s搜寻作者: ", up?"↑" : "↓");
                    search_length=IDLEN+1;
                    search_mode=1;
                    break;
                case REFER_ACTION_TITLE_SEARCH_PREV:
                case REFER_ACTION_TITLE_SEARCH_NEXT:
                    strncpy(buf, title, STRLEN);
                    snprintf(pmt, STRLEN, "%s搜寻标题: ", up ? "↑" : "↓");
                    search_length=STRLEN-1;
                    search_mode=2;
                    break;
                case REFER_ACTION_BOARD_SEARCH_PREV:
                case REFER_ACTION_BOARD_SEARCH_NEXT:
                    strcpy(buf, refer->board);
                    snprintf(pmt, STRLEN, "%s搜寻版面: ", up ? "↑" : "↓");
                    search_length=STRLEN-1;
                    search_mode=3;
                    break;
            }

            move(t_lines-1, 0);
            clrtoeol();
            getdata(t_lines-1, 0, pmt, buf, search_length, DOECHO, NULL, false);
            if (buf[0]!='\0') {
                if (REFER_ACTION_TITLE_SEARCH_PREV==act||REFER_ACTION_TITLE_SEARCH_NEXT==act)
                    strcpy(title, buf);
                if (1==refer_search(conf, buf, up, search_mode))
                    ret=SELCHANGE;
            } 
            conf->show_endline(conf);
            break;
    } 

    return ret;
}
int refer_search(struct _select_def* conf, char *query, bool up, int mode) {
    char ptr[STRLEN];
    char *ptr2;
    int now, match=0;
    char upper_query[STRLEN];
    int init;
    size_t bm_search[256];
    
    char *data;
    struct refer *pFh, *pFh1;
    off_t size;
    struct read_arg *arg=(struct read_arg *)conf->arg;
    int ln, i;

    for (ln=0;(ln<STRLEN)&&(query[ln]!=0);ln++);
    for (i=0;i<ln;i++) {
        upper_query[i]=toupper(query[i]);
        if (upper_query[i]=='\0') upper_query[i]='\1';
    }
    upper_query[ln]='\0';

    if (*query=='\0')
        return 0;
    init=false;
    now=conf->pos;

    match=0;
    BBS_TRY {
        if (safe_mmapfile_handle(arg->fd, PROT_READ, MAP_SHARED, &data, &size)==0)
            BBS_RETURN(0);
        pFh=(struct refer*)data;
        arg->filecount=size/sizeof(struct refer);
        if (now>arg->filecount)
            now=arg->filecount;

        if (now<=arg->filecount) {
            pFh1=pFh+now-1;
            while (1) {
                if (!up) {
                    if (++now>arg->filecount)
                        break;
                    pFh1++;
                } else {
                    if (--now<1)
                        break;
                    pFh1--;
                }
                if (now>arg->filecount)
                    break;
                if (mode==1) { // search auth
                    strncpy(ptr, pFh1->user, STRLEN-1);
                    if (!strcasecmp(ptr, upper_query)) {
                        match=1;
                        break;
                    }
                } else if (mode==2) { // search title
                    strncpy(ptr, pFh1->title, ARTICLE_TITLE_LEN-1);
                    ptr[ARTICLE_TITLE_LEN-1]=0;
                    ptr2=ptr;
                     
                    if ((*ptr=='R'||*ptr=='r')&&(*(ptr+1)=='E'||*(ptr+1)=='e')&&(*(ptr+2)==':')&&(*(ptr+3)==' '))
                        ptr2=ptr+4;
                    if (bm_strcasestr_rp(ptr2, query, bm_search, &init)) {
                        match=1;
                        break;
                    }     
                } else if (mode==3) { // search board
                   strncpy(ptr, pFh1->board, STRLEN-1);
                   if (!strcasecmp(ptr, upper_query)) {
                       match=1;
                       break;
                   }
                }
            }
        }
    }
    BBS_CATCH {
        match=0;
    }

    BBS_END;
    end_mmapfile(data, size, -1);
    move(t_lines-1, 0);
    clrtoeol();
    if (match) {
        conf->new_pos=now;
        return 1;
    }

    return 0;
}
struct key_command refer_comms[]={
    {'r', (READ_KEY_FUNC)refer_read, NULL},
    {'s', (READ_KEY_FUNC)refer_board, NULL},
    {'d', (READ_KEY_FUNC)refer_del, NULL},
    {'D', (READ_KEY_FUNC)refer_range_del, NULL},
    {'f', (READ_KEY_FUNC)refer_set_read, NULL},
    {',', (READ_KEY_FUNC)read_splitscreen,NULL},
    {'a', (READ_KEY_FUNC)refer_action, (void*)REFER_ACTION_AUTH_SEARCH_NEXT},    
    {'A', (READ_KEY_FUNC)refer_action, (void*)REFER_ACTION_AUTH_SEARCH_PREV},    
    {'/', (READ_KEY_FUNC)refer_action, (void*)REFER_ACTION_TITLE_SEARCH_NEXT},    
    {'?', (READ_KEY_FUNC)refer_action, (void*)REFER_ACTION_TITLE_SEARCH_PREV},    
    {'\'', (READ_KEY_FUNC)refer_action, (void*)REFER_ACTION_BOARD_SEARCH_NEXT},    
    {'\"', (READ_KEY_FUNC)refer_action, (void*)REFER_ACTION_BOARD_SEARCH_PREV},    
    {'z', (READ_KEY_FUNC)refer_action, (void*)REFER_ACTION_SEND_MSG_TO_AUTH},    
    {Ctrl('A'), (READ_KEY_FUNC)refer_action, (void*)REFER_ACTION_SHOW_AUTH},    
    {'~', (READ_KEY_FUNC)refer_action, (void*)REFER_ACTION_AUTH_INFO},    
//    {Ctrl('W'), (READ_KEY_FUNC)refer_action, (void*)REFER_ACTION_AUTH_BM},    
    {Ctrl('O'), (READ_KEY_FUNC)refer_action, (void*)REFER_ACTION_ADD_FRIEND},    
//    {Ctrl('D'), (READ_KEY_FUNC)refer_action, (void*)REFER_ACTION_ADD_IGNORE},    
    {'\n', NULL},
};
void refer_title(struct _select_def* conf) {
    
    showtitle(refer_title_bar, BBS_FULL_NAME);
    update_endline();
    move(1, 0);
    prints("离开[←,e] 选择[↑,↓] 阅读[→,r] 版面[s] 删除[d] 标题[?,/] 作者[a,A] 寻版[\',\"]\033[m\n");
    prints("\033[44m  编号   发布者       日期    讨论区名称   主题");
    clrtoeol();
    prints("\n");
    resetcolor();
}
char *referdoent(char *buf, int num, struct refer *ent, struct refer *readfh, struct _select_def* conf) {
    char *date;
    char c1[8],c2[8],user[IDLEN+2];
    int same=false, orig=0;

    date=ctime(&ent->time)+4;
    if (DEFINE(getCurrentUser(), DEF_HIGHCOLOR)) {
        strcpy(c1, "\033[1;33m");
        strcpy(c2, "\033[1;36m");
    } else {
        strcpy(c1, "\033[33m");
        strcpy(c2, "\033[36m");
    }
    if (readfh&&0==strncasecmp(ent->board, readfh->board, STRLEN-1)&&ent->groupid==readfh->groupid)
        same=true;
    if (strncmp(ent->title, "Re: ", 4))
        orig=1;

    strcpy(user, ent->user);
#ifdef ENABLE_BOARD_MEMBER
    const struct boardheader *board;
    if ((board=getbcache(ent->board))!=NULL && (board->flag&BOARD_MEMBER_READ)) {
        struct fileheader fh;
        bzero(&fh, sizeof(struct fileheader));
        /* 此处ent作者肯定不是当前用户，因此直接找该文章的主题文章，减少member_read_perm里相同的调用 */
        if (get_ent_from_id_ext(DIR_MODE_NORMAL, ent->groupid, board->filename, &fh)<=0 || !member_read_perm(board, &fh, getCurrentUser()))
            strcpy(user, MEMBER_POST_OWNER);
    }
#endif
    sprintf(buf, " %s%4d %s %-12.12s %6.6s  %-12.12s %s%s\033[m", same?(ent->id==ent->groupid?c1:c2):"", num, (ent->flag&FILE_READ)?" ":"*", user, date, ent->board, orig?FIRSTARTICLE_SIGN" ":"", ent->title);

    return buf;
}
int refer_at(void) {
    sprintf(refer_title_bar, "[@我的文章]");
    return refer_list(REFER_DIR, REFER_MODE_AT);
}
int refer_reply(void) {
    sprintf(refer_title_bar, "[回复我的文章]");
    return refer_list(REPLY_DIR, REFER_MODE_REPLY);
}
#ifdef ENABLE_REFER_LIKE
int refer_like(void) {
    sprintf(refer_title_bar, "[Like我的文章]");
    return refer_list(LIKE_DIR, REFER_MODE_LIKE);
}
#endif
int refer_list(char filename[STRLEN], int mode) {
    char dir[255];
    int oldmode;
    int returnmode=CHANGEMODE;
    struct stat st;

    sethomefile(dir, getCurrentUser()->userid, filename);
    if (stat(dir, &st)<0) {
        clear();
        move(9, 0);
        prints("\t\t\033[1;33m目前没有任何提醒!\033[m");
        pressreturn();
        return FULLUPDATE;
    }

    oldmode=uinfo.mode;
    modify_user_mode(REFER);

    in_mail=true;
    while(returnmode==CHANGEMODE) {
        sync_refer_info(mode, 0);
        returnmode=new_i_read(DIR_MODE_REFER, dir, refer_title, (READ_ENT_FUNC)referdoent, &refer_comms[0], sizeof(struct refer));
        sethomefile(dir, getCurrentUser()->userid, filename);
        load_refer_info(mode, 0);
    }
    in_mail=false;
    modify_user_mode(oldmode);     

    return DOQUIT;
}


/*
 * 将refer信息放入uinfo中，jiangjun，20120212
 */

void init_refer_info(int mode)
{
    uinfo.refer_head[mode-1] = NULL;
    uinfo.ri_loadedtime[mode-1] = 0;
    uinfo.ri_updatetime[mode-1] = 0;
    return;
}

void free_refer_info(struct refer_info **ri)
{
    if (*ri) {
        if ((*ri)->next)
            free_refer_info(&((*ri)->next));
        free(*ri);
        *ri = NULL;
    }
    return;
}

void clear_refer_info(int mode)
{
    free_refer_info(&(uinfo.refer_head[mode-1]));
}

/* 读入文件信息至uinfo，如果文件未更新，则不更新uinfo
   改进：只load所有未读记录，并将最后一篇放到refer_head（不管是否已读） */
int load_refer_info(int mode, int init)
{
    int i, count;
    char buf[STRLEN], filename[STRLEN];
    struct refer *rf;
    struct refer_info *p;
    char *ptr;
    off_t size;

    if (set_refer_file_from_mode(buf, mode)!=0)
        return -1;

    sethomefile(filename, getCurrentUser()->userid, buf);
    if (init)
        init_refer_info(mode);
    else
        clear_refer_info(mode);
    /* 使用mmap读取文件 */
    BBS_TRY {
        if (!safe_mmapfile(filename, O_RDONLY, PROT_READ, MAP_SHARED, &ptr, &size, NULL))
            BBS_RETURN(-1);
        count = size / sizeof(struct refer);
        rf = (struct refer *)ptr;
        for (i=0;i<count;i++) {
            if (rf[i].flag&FILE_READ && i<count-1)
                continue;
            p = (struct refer_info*)malloc(sizeof(struct refer_info));
            strcpy(p->board, rf[i].board);
            p->id = rf[i].id;
            p->flag = rf[i].flag;
            p->next = uinfo.refer_head[mode-1];
            uinfo.refer_head[mode-1] = p;
        }
    }
    BBS_CATCH {
    }
    BBS_END;
    end_mmapfile(ptr, size, -1); 
#if 0
    count = st.st_size / sizeof(struct refer); //get_num_records(filename, sizeof(struct refer));
    if (count<=0)
        return -1;
    if ((fd=open(filename, O_RDONLY))<0)
        return -1;
    rf = (struct refer*)malloc(count * sizeof(struct refer));
    read(fd, rf, count * sizeof(struct refer));
    for (i=0;i<count;i++){
        if ((bid = getbid(rf[i].board, NULL))==0 || (rf[i].flag&FILE_READ && i<count-1)) 
            continue;
        p = (struct refer_info*)malloc(sizeof(struct refer_info));
        p->bid = bid;
        p->id = rf[i].id;
        p->flag = rf[i].flag;
        p->next = uinfo.refer_head[mode-1];
        uinfo.refer_head[mode-1] = p;
    }
    close(fd);
    free(rf);
#endif
    uinfo.ri_updatetime[mode-1] = uinfo.ri_loadedtime[mode-1] = time(0);
    return 0;
}

/* 从uinfo中获得对应的refer状态 */
int get_refer_info(struct refer *rf, int mode)
{
    struct refer_info *p;

    p = uinfo.refer_head[mode-1];
    while(p!=NULL) {
        if (strcasecmp(p->board,rf->board)==0 && p->id == rf->id) {
            rf->flag |= p->flag;
            if (p->next==NULL) /* 最后一个了 */
                return 2;
            return 1;
        }
        p = p->next;
    }
    return 0;
}

/* 同步uinfo与refer记录文件
   首先将uinfo的refer信息写回文件，写之前如果文件被更新，则写入后重新load
   如果updatetime==loadedtime，表示uinfo未更新，则不用写入
   reload为0时，强制不更新uinfo，用于读取refer列表前更新文件 */
int sync_refer_info(int mode, int reload)
{
    int i, count;
    char buf[STRLEN], filename[STRLEN];
    struct refer *rf;
    struct stat st;
    char *ptr;
    off_t size;

    if (set_refer_file_from_mode(buf, mode)!=0)
        return -1;
    sethomefile(filename, getCurrentUser()->userid, buf);
    if (stat(filename, &st)==-1)
        return -1;
    //if (reload && uinfo.ri_loadedtime[mode-1]>=st.st_mtime)
    //    reload = 0;

    if (uinfo.ri_updatetime[mode-1]>uinfo.ri_loadedtime[mode-1]) {
        /* 使用mmap同步写入，不用再判断记录位置啥的了 */
        BBS_TRY {
            if (!safe_mmapfile(filename, O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED, &ptr, &size, NULL))
                BBS_RETURN(-1);
            count = size / sizeof(struct refer);
            rf = (struct refer *)ptr;
            for (i=count-1;i>=0;i--) {
                if (!(rf[i].flag & FILE_READ) && get_refer_info(&rf[i], mode)==2)
                    break;
            }
        }
        BBS_CATCH {
        }
        BBS_END;
        end_mmapfile(ptr, size, -1);
#if 0
        count = get_num_records(filename, sizeof(struct refer));
        if (count<=0)
            return -1;
        if ((fd=open(filename, O_RDWR))<0)
            return -1;

        rf = (struct refer*)malloc(count * sizeof(struct refer));
        read(fd, rf, count * sizeof(struct refer));
        for (i=count-1;i>=0;i--) { /* 逆序寻找，达到最后一个时就不再往前找了 */
            if (!(rf[i].flag & FILE_READ) && get_refer_info(&rf[i], mode)==2)
                break;
        }
        /* 万一最后一个被双登的读掉了，就gg了，因此... */
        i=(i<0)?0:i;
        /* 只写入查找过的rf记录 */
        lseek(fd, i*sizeof(struct refer), SEEK_SET);
        safewrite(fd, &(rf[i]), (count-i) * sizeof(struct refer));
        close(fd);
        free(rf);
#endif
        uinfo.ri_updatetime[mode-1] = uinfo.ri_loadedtime[mode-1] = time(0); /* 防止由于sync引起文件时间改变而导致重新load */
    }
    if (reload)
        load_refer_info(mode, 0);
    return 0;
}

int set_refer_info(char *board, int id, int mode)
{
    struct refer_info *p;

    p = uinfo.refer_head[mode-1];
    while (p!=NULL) {
        if (strcasecmp(p->board, board)==0 && p->id==id && !(p->flag&FILE_READ)) {
            p->flag |= FILE_READ;
            uinfo.ri_updatetime[mode-1]=time(0);
            if(p==uinfo.refer_head[mode-1])
                setmailcheck(getCurrentUser()->userid);
            return 0;
        }
        p = p->next;
    }
    return 0;
}

int set_board_refer_info(char *board, int mode)
{
    struct refer_info *p;
    int found=0, ishead=0;

    p = uinfo.refer_head[mode-1];
    while (p!=NULL) {
        if (strcasecmp(p->board, board)==0 && !(p->flag&FILE_READ)) {
            p->flag |= FILE_READ;
            found = 1;
            if (p==uinfo.refer_head[mode-1])
                ishead = 1;
        }
        p = p->next;
    }
    if (found) {
        uinfo.ri_updatetime[mode-1] = time(0);
        if (ishead)
            setmailcheck(getCurrentUser()->userid);
    }
    return 0;
}

/* 从uinfo中检查refer状态，如果文件更新过，写入后重新load */
int check_refer_info(int mode)
{
    /* 这部分检查挪到调用之前，因为可能因为检查AT而遗漏REPLY */
    /*
    char buf[STRLEN], filename[STRLEN];
    struct stat st;

    if (set_refer_file_from_mode(buf, mode)!=0)
        return -1;
    sethomefile(filename, getCurrentUser()->userid, buf);
    if (stat(filename, &st)==-1)
        return -1;
    if (uinfo.ri_loadedtime[mode-1]<st.st_mtime)
        sync_refer_info(mode, 1);
    */
    if (uinfo.refer_head[mode-1]) {
        if (uinfo.refer_head[mode-1]->flag & FILE_READ)
            return 0;
        else
            return 1;
    }
    return 0;
}
#endif

