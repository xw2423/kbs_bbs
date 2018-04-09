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
#include "vote.h"
#include "read.h"
extern int page, range;
struct votebal currvote;
struct votelimit currlimit;     /*Haohmaru.99.11.17.���ݰ���������������ж��Ƿ��ø�ʹ����ͶƱ */
char controlfile[STRLEN], limitfile[STRLEN];
unsigned int result[65];
int vnum;
int voted_flag;
FILE *sug;
static int mk_result(int num);
static int dele_vote(int num);
static int Show_Votes();
static int dump_vote_record(void);
static int write_vote_record(char *file, int mode);
static int sort_vote_record(void);
struct voterecord *vc;
int pos;
int cmpvuid(userid, uv)
char *userid;
struct ballot *uv;
{
    return !strncmp(userid, uv->uid,IDLEN);
}
int setvoteflag(char *bname, int flag)
{
    int pos;
    struct boardheader fh;

    pos = getboardnum(bname, &fh);
    if (pos) {
        if (flag == 0)
            fh.flag = fh.flag & ~BOARD_VOTEFLAG;
        else
            fh.flag = fh.flag | BOARD_VOTEFLAG;
        set_board(pos, &fh,NULL);
    }
    return 0;
}

void b_report(str)
char *str;
{
    char buf[STRLEN];

    sprintf(buf, "%s %s", currboard->filename, str);
    bbslog("user","%s",buf);
}

void setcontrolfile()
{
    setvfile(controlfile, currboard->filename, "control");
}

#ifdef NEWSMTH
int b_rules_edit()
{
    char buf[STRLEN];
    char buf1[STRLEN];
    int aborted;
    int oldmode;
    struct stat st;
#ifdef BOARD_SECURITY_LOG
    char oldrules[STRLEN];
#endif

#ifdef MEMBER_MANAGER_0
	if (!check_board_member_manager(&currmember, currboard, BMP_NOTE)) {
#else
    if (!chk_currBM(currBM, getCurrentUser())) {
#endif
		return 0;
    }
    clear();
    makevdir(currboard->filename);
    setvfile(buf, currboard->filename, "rules");
    oldmode = uinfo.mode;
    modify_user_mode(EDITUFILE);
#ifdef BOARD_SECURITY_LOG
    gettmpfilename(oldrules, "old_rules");
    f_cp(buf, oldrules, 0);
#endif
    aborted = vedit(buf, false,NULL, NULL, 0);
    modify_user_mode(oldmode);
    if (aborted == -1) {
        pressreturn();
    } else {
        if (currboard->flag & BOARD_RULES) set_board_rule(currboard, 0);
        if (stat(buf, &st)==-1) return FULLUPDATE;
        sprintf(buf1, "%s�ύ%s�ΰ淽��:%ld", getCurrentUser()->userid, currboard->filename, st.st_mtime);
        post_file(getCurrentUser(), "", buf, "BoardRules", buf1, 0, 2, getSession());
        if (normal_board(currboard->filename))
            post_file(getCurrentUser(), "", buf, "BoardManager", buf1, 0, 2, getSession());
#ifdef BOARD_SECURITY_LOG
        board_file_report(currboard->filename, "�޸� <�ΰ淽��>", oldrules, buf);
#endif
        clear();
        move(3,0);
        prints("�Ѿ��ύ�µ��ΰ淽��,��ȴ���׼\n");
        pressreturn();
    }
#ifdef BOARD_SECURITY_LOG
    unlink(oldrules);
#endif
    return FULLUPDATE;
}
#endif

int b_notes_edit()
{
    char buf[STRLEN];
    char ans[4];
    int aborted;
    int oldmode;

#ifdef MEMBER_MANAGER
	if (!check_board_member_manager(&currmember, currboard, BMP_NOTE)) {
#else	
    if (!chk_currBM(currBM, getCurrentUser())) {
#endif
		return 0;
    }
    clear();
    makevdir(currboard->filename);
    setvfile(buf, currboard->filename, "notes");

#ifdef BOARD_SECURITY_LOG
    char oldfilename[STRLEN];
    gettmpfilename(oldfilename, "old_notes");
    f_cp(buf, oldfilename, 0); 
#endif

    getdata(1, 0, "(E)�༭ (D)ɾ�� ���������ı���¼? [E]: ", ans, 2,
            DOECHO, NULL, true);
    if (ans[0] == 'D' || ans[0] == 'd') {
        move(2, 0);
        if (askyn("���Ҫɾ�����������ı���¼", 0)) {
            move(3, 0);
            prints("����¼�Ѿ�ɾ��...\n");
            pressanykey();
#ifdef BOARD_SECURITY_LOG
            board_file_report(currboard->filename, "ɾ�� <����¼>", oldfilename, buf);
            unlink(oldfilename);
#endif
            my_unlink(buf);
            aborted = 1;
        } else
            aborted = -1;
    } else {
        oldmode = uinfo.mode;
        modify_user_mode(EDITUFILE);
        aborted = vedit(buf, false,NULL, NULL, 0);
#ifdef BOARD_SECURITY_LOG
        board_file_report(currboard->filename, "�޸� <����¼>", oldfilename, buf);
        unlink(oldfilename);
#endif
        modify_user_mode(oldmode);
    }
    if (aborted == -1) {
        pressreturn();
    } else {
        setvfile(buf, currboard->filename, "noterec");
        my_unlink(buf);
    }
    return FULLUPDATE;
}

#ifdef FLOWBANNER
void load_board_banner(const char * board)
{
    int i;
    FILE *fp;
    struct boardheader bh;
    const struct boardheader *obh;
    char filename[STRLEN * 2];
    char buf[512];
    sprintf(filename, "boards/%s/banner", board);
    obh = getbcache(board);
    memcpy(&bh, obh, sizeof(struct boardheader));
    i = 0;
    if (NULL != (fp = fopen(filename, "r"))) {
        while ((!feof(fp))&&(i<MAXBANNER)) {
            fgets(buf, 512, fp);
            buf[BANNERSIZE-1] = 0;
            strcpy(bh.banners[i], buf);
            if (*banner_filter(bh.banners[i])) i++;
        }
        fclose(fp);
    }
    bh.bannercount = i;
    set_board(getboardnum(board, NULL), &bh, NULL);
}

int b_banner_edit()
{
    char buf[STRLEN];
    char ans[4];
    int aborted;
    int oldmode;

#ifdef MEMBER_MANAGER
	if (!check_board_member_manager(&currmember, currboard, BMP_VOTE)) {
#else	
    if (!chk_currBM(currBM, getCurrentUser())) {
#endif
		return 0;
    }
    clear();
    sprintf(buf, "boards/%s/banner", currboard->filename);
    getdata(1, 0, "(E)�༭ (D)ɾ�� ����������������Ϣ? [E]: ", ans, 2,
            DOECHO, NULL, true);
    if (ans[0] == 'D' || ans[0] == 'd') {
        move(2, 0);
        if (askyn("���Ҫɾ������������������Ϣ", 0)) {
            move(3, 0);
            prints("������Ϣ�Ѿ�ɾ��...\n");
            pressanykey();
            my_unlink(buf);
            aborted = 1;
        } else
            aborted = -1;
    } else {
        oldmode = uinfo.mode;
        modify_user_mode(EDITUFILE);
        aborted = vedit(buf, false,NULL, NULL, 0);
        modify_user_mode(oldmode);
    }
    if (aborted == -1) {
        pressreturn();
    } else {
        load_board_banner(currboard->filename);
        setvfile(buf, currboard->filename, "noterec");
        my_unlink(buf);
    }
    return FULLUPDATE;
}
#endif

int b_sec_notes_edit(struct _select_def* conf,struct fileheader *fileinfo,void* extraarg)
{
    char buf[STRLEN];
    char ans[4];
    int aborted;

#ifdef MEMBER_MANAGER
	if (!check_board_member_manager(&currmember, currboard, BMP_VOTE)) {
#else	
    if (!chk_currBM(currBM, getCurrentUser())) {
#endif
		return 0;
    }
    clear();
    makevdir(currboard->filename);
    setvfile(buf, currboard->filename, "secnotes");
    getdata(1, 0, "(E)�༭ (D)ɾ�� �������������ܱ���¼? [E]: ", ans, 2,
            DOECHO, NULL, true);
    if (ans[0] == 'D' || ans[0] == 'd') {
        move(2, 0);
        if (askyn("���Ҫɾ���������������ܱ���¼", 0)) {
            move(3, 0);
            prints("���ܱ���¼�Ѿ�ɾ��...\n");
            pressanykey();
            my_unlink(buf);
            aborted = 1;
        } else
            aborted = -1;
    } else
        aborted = vedit(buf, false,NULL, NULL, 0);
    if (aborted == -1) {
        pressreturn();
    } else {
        setvfile(buf, currboard->filename, "noterec");
        my_unlink(buf);
    }
    return FULLUPDATE;
}

int b_jury_edit(struct _select_def* conf,struct fileheader *fileinfo,void* extraarg)
{                               /* stephen 2001.11.1: �༭�����ٲ����� */
    char buf[STRLEN];
    char ans[4];
    int aborted;

    if (!((HAS_PERM(getCurrentUser(), PERM_JURY)
            && HAS_PERM(getCurrentUser(), PERM_BOARDS))
            || HAS_PERM(getCurrentUser(), PERM_SYSOP))) {
        return 0;
    }
    clear();
    makevdir(currboard->filename);
    setvfile(buf, currboard->filename, "jury");
    getdata(1, 0, "(E)�༭ (D)ɾ�� ���������ٲ�ίԱ����? (C)ȡ�� [C]: ",
            ans, 2, DOECHO, NULL, true);
    if (ans[0] == 'D' || ans[0] == 'd') {
        move(2, 0);
        if (askyn("���Ҫɾ�����������ٲ�ίԱ����", 0)) {
            move(3, 0);
            prints("�ٲ�ίԱ�����Ѿ�ɾ��...\n");
            pressanykey();
            my_unlink(buf);
            aborted = 111;
        } else
            aborted = -1;
    } else if (ans[0] == 'E' || ans[0] == 'e')
        aborted = vedit(buf, false,NULL, NULL, 0);
    else {
        prints("ȡ��");
        aborted = -1;
    }
    if (aborted == -1) {
        pressreturn();
    } else {
        char secu[STRLEN];

        if (aborted == 111) {
            sprintf(secu, "ɾ�� %s ����ٲ�ίԱ����", currboard->filename);
            securityreport(secu, NULL, NULL, getSession());
            post_file(getCurrentUser(), "", buf, "JuryMail", secu, 0, 2, getSession());
        } else {
            sprintf(secu, "�޸� %s ����ٲ�ίԱ����", currboard->filename);
            securityreport(secu, NULL, NULL, getSession());
            post_file(getCurrentUser(), "", buf, "syssecurity", secu, 0, 2, getSession());
            post_file(getCurrentUser(), "", buf, "JuryMail", secu, 0, 2, getSession());
        }
        setvfile(buf, currboard->filename, "juryrec");
        my_unlink(buf);
    }
    return FULLUPDATE;
}

int b_suckinfile(fp, fname)
FILE *fp;
char *fname;
{
    char inbuf[256];
    FILE *sfp;

    if ((sfp = fopen(fname, "r")) == NULL)
        return -1;
    while (fgets(inbuf, sizeof(inbuf), sfp) != NULL)
        fputs(inbuf, fp);
    fclose(sfp);
    return 0;
}

int vote_close()
{
    time_t closetime;

    closetime = currvote.opendate + currvote.maxdays * 86400;
    if (closetime <= time(NULL)) {
        mk_result(vnum);
    }
    return 0;
}

int b_close(struct boardheader *fh,void* arg)
{
    int end;

    currboard= fh;
    currboardent=getbid(fh->filename,NULL);
    setcontrolfile();
    end = get_num_records(controlfile, sizeof(currvote));
    for (vnum = end; vnum >= 1; vnum--) {
        get_record(controlfile, &currvote, sizeof(currvote), vnum);
        vote_close();
    }
    return 0;
}

int b_closepolls()
{
    struct stat st;
    FILE *cfp;
    char buf[80];
    time_t now;
    struct boardheader* bh;
    int bid;

    now = time(NULL);
    strcpy(buf, "vote/lastpolling");
    if (stat(buf, &st) != -1 && st.st_mtime > now - 3600) {
        return 0;
    }
    move(t_lines - 1, 0);
    prints("�Բ���ϵͳ�ر�ͶƱ�У����Ժ�...");
    refresh();
    if ((cfp = fopen(buf, "w")) == NULL) {
        bbslog("3error","%s","lastpoll write error");
        return 0;
    }
    fprintf(cfp, "%s", ctime(&now));
    bh=currboard;
    bid=currboardent;
    fclose(cfp);
    apply_boards(b_close,NULL);
    currboard=bh;
    currboardent=bid;
    return 0;
}
int count_result(struct ballot *ptr, int idx, char *arg)
{
    int i;

    if (ptr == NULL) {
        if (sug != NULL) {
            fclose(sug);
            sug = NULL;
        }
        return 0;
    }
    if (ptr->msg[0][0] != '\0') {
        if (currvote.type == VOTE_ASKING) {
            fprintf(sug, "\033[44m%s ���������£�\033[m\n", ptr->uid);
        } else
            fprintf(sug, "\033[44m%s �Ľ������£�\033[m\n", ptr->uid);
        for (i = 0; i < 3; i++)
            fprintf(sug, "%s\n", ptr->msg[i]);
    }
    result[64]++;
    if (currvote.type == VOTE_ASKING) {
        return 0;
    }
    if (currvote.type != VOTE_VALUE) {
        for (i = 0; i < 32; i++) {
            if ((ptr->voted[0] >> i) & 1)
                (result[i])++;
            if ((ptr->voted[1] >> i) & 1)
                (result[32+i])++;
        }
    } else {
        result[63] += ptr->voted[0];
        result[(ptr->voted[0] * 10) / (currvote.maxtkt + 1)]++;
    }
    return 0;
}

int get_result_title()
{
    char buf[STRLEN];

    if (currlimit.numlogins || currlimit.numposts || currlimit.stay
            || currlimit.day) {
        fprintf(sug, "�� �˴�ͶƱ�������ʸ�Ϊ:\n");
        fprintf(sug, "1. ��վ��������� %d �� .\n", currlimit.numlogins);
        fprintf(sug, "2. ������Ŀ����� %d ƪ.\n", currlimit.numposts);
        fprintf(sug, "3. ��վ��ʱ������� %d Сʱ.\n", currlimit.stay);
        fprintf(sug, "4. ��վ����ʱ������� %d ��.\n", currlimit.day);
    }
    if (currvote.type < 1 || currvote.type > 5)
        currvote.type = 1;
    fprintf(sug, "�� ͶƱ�����ڣ�%.24s  ʱ����%d��  ���%s\n",
            ctime(&currvote.opendate), currvote.maxdays, vote_type[currvote.type - 1]);
    fprintf(sug, "�� ���⣺%s\n", currvote.title);
    if (currvote.type == VOTE_VALUE)
        fprintf(sug, "�� �˴�ͶƱ��ֵ���ɳ�����%d\n", currvote.maxtkt);
    if (currvote.type != VOTE_ASKING) {
        if (currvote.flag & VOTE_TRUE_FLAG)
            fprintf(sug, "�� �˴�ͶƱ��¼\033[1;31m�û�ͶƱ����%s\033[m\n", (currvote.flag & VOTE_IP_FLAG)? "������IP":"");
    }
    fprintf(sug, "\n�� Ʊѡ��Ŀ������\n\n");
    sprintf(buf, "vote/%s/desc.%lu", currboard->filename, currvote.opendate);
    b_suckinfile(sug, buf);
    return 0;
}
static int mk_result(int num)
{
    char fname[255], nname[255];
    char sugname[255];
    char title[255];
    int i;
    unsigned int total = 0;
    char record_file[256];  /* ��¼ͶƱͳ����Ϣ, ���ڼ���ͳ����ϸͶƱ��� */
    char vote_file[256]; /* ��¼��ϸ�������ӡ */

    setcontrolfile();
    sprintf(fname, "vote/%s/flag.%lu", currboard->filename, currvote.opendate);
    count_result(NULL, NULL, 0);
    sprintf(sugname, "vote/%s/tmp.%d", currboard->filename, (int)getpid());
    if ((sug = fopen(sugname, "w")) == NULL) {
        bbslog("3error","%s","open vote tmp file error");
        prints("Error: ����ͶƱ����...\n");
        pressanykey();
    }
    (void) memset(result, 0, sizeof(result));
    if (apply_record
            (fname, (APPLY_FUNC_ARG) count_result, sizeof(struct ballot), 0, 0,
             false) == -1) {
        bbslog("user","%s","Vote apply flag error");
    }
    fprintf(sug,
            "\033[44m\033[36m������������������������������ʹ����%s������������������������������\033[m\n\n\n",
            (currvote.type != VOTE_ASKING) ? "��������" : "�˴ε�����");
    fclose(sug);
    sug = NULL;
    sprintf(nname, "vote/%s/results", currboard->filename);
    if ((sug = fopen(nname, "w")) == NULL) {
        bbslog("user","%s","open vote newresult file error");
        prints("Error: ����ͶƱ����...\n");
    }
    /*    fprintf( sug, "** ͶƱ����춣�\033[1m%.24s\033[m  ���\033[1m%s\033[m\n", ctime( &currvote.opendate )
       ,vote_type[currvote.type-1]);
       fprintf( sug, "** ���⣺\033[1m%s\033[m\n",currvote.title);
       if(currvote.type==VOTE_VALUE)
       fprintf( sug, "** �˴�ͶƱ��ֵ���ɳ�����\033[1m%d\033[m\n\n" ,currvote.maxtkt);
       fprintf( sug, "** Ʊѡ��Ŀ������\n\n" );
       sprintf( buf, "vote/%s/desc.%d",currboard->filename,currvote.opendate );
       b_suckinfile( sug, buf ); */
    get_result_title();
    fprintf(sug, "** ͶƱ���:\n\n");
    if (currvote.type == VOTE_VALUE) {
        total = result[64];
        for (i = 0; i < 10; i++) {
            fprintf(sug,
                    "\033[1m  %4d\033[m �� \033[1m%4d\033[m ֮���� \033[1m%4d\033[m Ʊ  Լռ \033[1m%d%%\033[m\n",
                    (i * currvote.maxtkt) / 10 + ((i == 0) ? 0 : 1),
                    ((i + 1) * currvote.maxtkt) / 10, result[i]
                    , (result[i] * 100) / ((total <= 0) ? 1 : total));
        }
        fprintf(sug, "�˴�ͶƱ���ƽ��ֵ��: \033[1m%d\033[m\n",
                result[63] / ((total <= 0) ? 1 : total));
    } else if (currvote.type == VOTE_ASKING) {
        total = result[64];
    } else {
        for (i = 0; i < currvote.totalitems; i++) {
            total += result[i];
        }
        for (i = 0; i < currvote.totalitems; i++) {
            /*            fprintf(sug, "(%c) %-40s  %4d Ʊ  Լռ \033[1m%d%%\033[m\n",'A'+i,
               currvote.items[i], result[i] , (result[i]*100)/((total<=0)?1:total));
             */
            fprintf(sug, "(%c) %-40s  %4d Ʊ  Լռ \033[1m%d%%\033[m\n", (i<58)? 'A' + i : i - 25,
                    currvote.items[i], result[i],
                    (result[i] * 100) /
                    ((result[64] <= 0) ? 1 : result[64]));
        }
    }
    fprintf(sug, "\nͶƱ������ = \033[1m%d\033[m ��\n", result[64]);
    fprintf(sug, "ͶƱ��Ʊ�� =\033[1m %d\033[m Ʊ\n\n", total);
    if (currvote.type != VOTE_ASKING && (currvote.flag & VOTE_TRUE_FLAG)) {
        sprintf(record_file, "vote/%s/record.%d", currboard->filename, (int)getpid());
        fclose(sug);
        f_cp(nname, record_file, 0); /* ���ݼ�¼�ļ� */
        sug = fopen(nname, "r+");
        fseek(sug, 0, SEEK_END);
    }
    fprintf(sug,
            "\033[44m\033[36m������������������������������ʹ����%s������������������������������\033[m\n\n\n",
            (currvote.type != VOTE_ASKING) ? "��������" : "�˴ε�����");
    b_suckinfile(sug, sugname);
    unlink(sugname);
    fclose(sug);
    sug = NULL;
    if (currvote.type != VOTE_ASKING && (currvote.flag & VOTE_TRUE_FLAG)) {
        dump_vote_record();
        sprintf(vote_file, "vote/%s/vote_file", currboard->filename);
        f_cp(record_file, vote_file, 0);
        write_vote_record(vote_file, 0);
    }   
    sprintf(title, "[����] %s ͶƱ���: %s", currboard->filename, currvote.title);
    mail_file("deliver", nname, currvote.userid, title, 0, NULL);
    if (normal_board(currboard->filename)) {
        post_file(getCurrentUser(), "", nname, "vote", title, 0, 1, getSession());
    }
    sprintf(title, "[����] ͶƱ���: %s", currvote.title);
    post_file(getCurrentUser(), "", nname, currboard->filename, title, 0, 1, getSession());
    if (currvote.type != VOTE_ASKING && (currvote.flag & VOTE_TRUE_FLAG)) {
        sprintf(title, "[����] %s ͶƱ���(��ϸ): %s", currboard->filename, currvote.title);
        mail_file("deliver", vote_file, currvote.userid, title, 0, NULL);
        if (normal_board(currboard->filename)) {
            post_file(getCurrentUser(), "", vote_file, "vote", title, 0, 1, getSession());
        }
        sprintf(title, "[����] ͶƱ���(��ϸ): %s", currvote.title);
        post_file(getCurrentUser(), "", vote_file, currboard->filename, title, 0, 1, getSession());
        /* �����¼IP����IP���� */
        if (currvote.flag & VOTE_IP_FLAG) {
            f_cp(record_file, vote_file, 0);
            sort_vote_record();
            write_vote_record(vote_file, 1);
            sprintf(title, "[����] %s ͶƱ���(IP��): %s", currboard->filename, currvote.title);
            mail_file("deliver", vote_file, currvote.userid, title, 0, NULL);
            if (normal_board(currboard->filename)) {
                post_file(getCurrentUser(), "", vote_file, "vote", title, 0, 1, getSession());
            }
            sprintf(title, "[����] ͶƱ���(IP��): %s", currvote.title);
            post_file(getCurrentUser(), "", vote_file, currboard->filename, title, 0, 1, getSession());
        }
        unlink(record_file);
        unlink(vote_file);
        free(vc);
    }
    dele_vote(num);
    return 0;
}
int check_result(int num)
{
    char fname[STRLEN], nname[STRLEN];
    char sugname[STRLEN];
    char title[STRLEN];
    int i;
    unsigned int total = 0;

    setcontrolfile();
    sprintf(fname, "vote/%s/flag.%lu", currboard->filename, currvote.opendate);
    count_result(NULL, 0, 0);
    sprintf(sugname, "vote/%s/tmp.%d", currboard->filename, (int)getpid());
    if ((sug = fopen(sugname, "w")) == NULL) {
        bbslog("user","%s","open vote tmp file error");
        prints("Error: ���ͶƱ����...\n");
        pressanykey();
    }
    (void) memset(result, 0, sizeof(result));
    if (apply_record
            (fname, (APPLY_FUNC_ARG) count_result, sizeof(struct ballot), 0, 0,
             false) == -1) {
        bbslog("user","%s","Vote apply flag error");
    }
    fprintf(sug,
            "\033[44m\033[36m������������������������������ʹ����%s������������������������������\033[m\n\n\n",
            (currvote.type != VOTE_ASKING) ? "��������" : "�˴ε�����");
    fclose(sug);
    sug = NULL;
    sprintf(nname, "vote/%s/results", currboard->filename);
    if ((sug = fopen(nname, "w")) == NULL) {
        bbslog("user","%s","open vote newresult file error");
        prints("Error: ����ͶƱ����...\n");
    }
    get_result_title();
    fprintf(sug, "\n** ͶƱ���:\n");
    if (currvote.type == VOTE_VALUE) {
        total = result[64];
        for (i = 0; i < 10; i++) {
            fprintf(sug,
                    "\033[1m  %4d\033[m �� \033[1m%4d\033[m ֮���� \033[1m%4d\033[m Ʊ  Լռ \033[1m%d%%\033[m\n",
                    (i * currvote.maxtkt) / 10 + ((i == 0) ? 0 : 1),
                    ((i + 1) * currvote.maxtkt) / 10, result[i]
                    , (result[i] * 100) / ((total <= 0) ? 1 : total));
        }
        fprintf(sug, "�˴�ͶƱ���ƽ��ֵ��: \033[1m%d\033[m\n",
                result[63] / ((total <= 0) ? 1 : total));
    } else if (currvote.type == VOTE_ASKING) {
        total = result[64];
    } else {
        for (i = 0; i < currvote.totalitems; i++) {
            total += result[i];
        }
        for (i = 0; i < currvote.totalitems; i++) {
            fprintf(sug, "(%c) %-40s  %4d Ʊ  Լռ \033[1m%d%%\033[m\n", (i<58)? 'A' + i : i - 25,
                    currvote.items[i], result[i],
                    (result[i] * 100) /
                    ((result[64] <= 0) ? 1 : result[64]));
        }
    }
    fprintf(sug, "\nͶƱ������ = \033[1m%d\033[m ��\n", result[64]);
    fprintf(sug, "ͶƱ��Ʊ�� =\033[1m %d\033[m Ʊ\n\n", total);
    fprintf(sug,
            "\033[44m\033[36m������������������������������ʹ����%s������������������������������\033[m\n\n\n",
            (currvote.type != VOTE_ASKING) ? "��������" : "�˴ε�����");
    b_suckinfile(sug, sugname);
    unlink(sugname);
    fclose(sug);
    sug = NULL;
    /* �����ʱҲ�ɲ鿴��ϸ��¼ */
    if (currvote.type != VOTE_ASKING && (currvote.flag & VOTE_TRUE_FLAG)) {
        dump_vote_record();
        write_vote_record(nname, 0);
    }
    sprintf(title, "[���] %s ͶƱ���: %s", currboard->filename, currvote.title);
    mail_file(getCurrentUser()->userid, nname, getCurrentUser()->userid, title, BBSPOST_MOVE, NULL);
    return 0;
}

int get_vitems(bal)
struct votebal *bal;
{
    int num, oldnum=-1;
    char buf[STRLEN];

    for (num = 0; num < 64; num++)
        bal->items[num][0] = '\0';
    move(3, 0);
    prints("�����������ѡ����, �� ENTER ����趨.\n");

    while (1) {
        for (num = 0; num < 64; num++) {
            if (num == 32) {
                move(4, 0);
                clrtobot();
            }
            sprintf(buf, "%c) ", (num<58)? 'A' + num : num - 25);
            if (num<32)
                getdata((num % 16) + 4, (num / 16) * 40, buf, bal->items[num], 36,
                        DOECHO, NULL, false);
            else
                getdata((num % 16) + 4, ((num-32) / 16) * 40, buf, bal->items[num], 36,
                        DOECHO, NULL, false);
            if (strlen(bal->items[num]) == 0) {
                if (num != 0)
                    break;
                num = -1;
            }
        }
        if (oldnum != -1) {
            int i;
            for (i=num+1; i<oldnum; i++) {
                move((i % 16) + 4, (i<32)?(i / 16) * 40 : ((i - 32) / 16)* 40);
                prints("                                        ");
            }
        }
        oldnum = num;
        move(20, 0);
        getdata(21, 0, "��Ҫ�ٱ༭ѡ��������(Y/N)? [N]: ", buf, 2, DOECHO, NULL, true);
        if (buf[0] != 'Y' && buf[0] != 'y')
            break;
    }

    bal->totalitems = num;
    return num;
}

int vote_maintain(bname)
char *bname;
{
    char buf[STRLEN * 2];
    struct votebal *ball = &currvote;
    struct votelimit *v_limit = &currlimit;
    int aborted;

    setcontrolfile();
    if (!HAS_PERM(getCurrentUser(), PERM_SYSOP))
#ifdef MEMBER_MANAGER
		if (!check_board_member_manager(&currmember, currboard, BMP_VOTE)) {
#else	
        if (!chk_currBM(currBM, getCurrentUser())) {
#endif
			return 0;
        }
    stand_title("����ͶƱ��");
    makevdir(bname);
    for (;;) {
        getdata(2, 0,
                "(0)ȡ��, (1)�Ƿ�, (2)��ѡ, (3)��ѡ, (4)��ֵ, (5)�ʴ� ? [0] ",
                genbuf, 2, DOECHO, NULL, true);
        if (genbuf[0])
            genbuf[0] -= '0';
        if (genbuf[0] == 0) {
            prints("ȡ���˴�ͶƱ");
            refresh();
            sleep(1);
            return FULLUPDATE;
        }
        if (genbuf[0] < 1 || genbuf[0] > 5)
            continue;
        ball->type = (int) genbuf[0];
        break;
    }
    ball->opendate = time(NULL);
    prints("���κμ���ʼ�༭�˴� [ͶƱ������]: \n");
    pressanykey();
    setvfile(genbuf, bname, "desc");
    sprintf(buf, "%s.%lu", genbuf, ball->opendate);
    aborted = vedit(buf, false, NULL, NULL, 0);
    if (aborted) {
        clear();
        prints("ȡ���˴�ͶƱ\n");
        pressreturn();
        return FULLUPDATE;
    }
    clear();
    getdata(0, 0, "�˴�ͶƱ�������� (���ɣ���): ", buf, 4, DOECHO, NULL,
            true);
    if (*buf == '\n' || atoi(buf) == 0 || *buf == '\0')
        strcpy(buf, "1");
    ball->maxdays = atoi(buf);
    for (;;) {
        getdata(1, 0, "ͶƱ��ı���: ", ball->title, 61, DOECHO, NULL,
                true);
        if (strlen(ball->title) > 0)
            break;
        bell();
    }
    switch (ball->type) {
        case VOTE_YN:
            ball->maxtkt = 0;
            strcpy(ball->items[0], "�޳�  ���ǵģ�");
            strcpy(ball->items[1], "���޳ɣ����ǣ�");
            strcpy(ball->items[2], "û������������");
            ball->maxtkt = 1;
            ball->totalitems = 3;
            break;
        case VOTE_SINGLE:
            get_vitems(ball);
            ball->maxtkt = 1;
            break;
        case VOTE_MULTI:
            get_vitems(ball);
            for (;;) {
                getdata(21, 0, "һ������༸Ʊ? [1]: ", buf, 5, DOECHO, NULL,
                        true);
                ball->maxtkt = atoi(buf);
                if (ball->maxtkt <= 0)
                    ball->maxtkt = 1;
                if (ball->maxtkt > ball->totalitems)
                    continue;
                break;
            }
            break;
        case VOTE_VALUE:
            for (;;) {
                getdata(3, 0, "������ֵ��󲻵ó��� [100] : ", buf, 7, DOECHO,
                        NULL, true);
                ball->maxtkt = atoi(buf);
                if (ball->maxtkt <= 0)
                    ball->maxtkt = 100;
                break;
            }
            break;
        case VOTE_ASKING:
            /*                    getdata(3,0,"���ʴ�����������֮���� :",buf,3,DOECHO,NULL,true) ;
               ball->maxtkt = atof(buf) ;
               if(ball->maxtkt <= 0) ball->maxtkt = 10; */
            ball->maxtkt = 0;
            currvote.totalitems = 0;
            break;
    }
    //setvoteflag(bname, 1);
    clear();
    /* ������ֵ���ʴ�ʱ�����ڽ���м�¼�û�ͶƱ��Ϣ */
    ball->flag = 0;
    if (ball->type != VOTE_ASKING) {
        getdata(1, 0, "�Ƿ��¼�û���ϸͶƱ��Ϣ��[N]", buf, 3, DOECHO, NULL, true);
        if (buf[0] == 'y' || buf[0] == 'Y') {
            ball->flag |= VOTE_TRUE_FLAG;
            clear();
            if (HAS_PERM(getCurrentUser(), PERM_SYSOP) || HAS_PERM(getCurrentUser(), PERM_ADMIN)) {
                getdata(1, 0, "�Ƿ��¼�û�����IP��[N]", buf, 3, DOECHO, NULL, true);
                if (buf[0] == 'y' || buf[0] == 'Y')
                    ball->flag |= VOTE_IP_FLAG;
                clear();
            }
        }
    }
    /*Haohmaru.99.11.17.����ͶƱ����Ա������������ж��Ƿ��ø�ʹ����ͶƱ */
    if (HAS_PERM(getCurrentUser(), PERM_SYSOP)
            || HAS_PERM(getCurrentUser(), PERM_JURY)) {
        getdata(1, 0, "�Ƿ��ͶƱ�ʸ��������(Y/N) [Y]:", buf, 3, DOECHO,
                NULL, true);
        if (buf[0] != 'N' && buf[0] != 'n') {
            getdata(2, 0,
                    "���������վ����������(0Ϊû������),��վ��������:",
                    buf, 5, DOECHO, NULL, true);
            v_limit->numlogins = atoi(buf);
            getdata(3, 0,
                    "�������������Ŀ������(0Ϊû������),������Ŀ����:",
                    buf, 5, DOECHO, NULL, true);
            v_limit->numposts = atoi(buf);
            getdata(4, 0,
                    "���������վ��ʱ��������(0Ϊû������),��վ��ʱ������(Сʱ):",
                    buf, 5, DOECHO, NULL, true);
            v_limit->stay = atoi(buf);
            getdata(5, 0,
                    "���������վ����ʱ�������(0Ϊû������),��վ����ʱ�����(��):",
                    buf, 5, DOECHO, NULL, true);
            v_limit->day = atoi(buf);
        } else {
            v_limit->numlogins = 0;
            v_limit->numposts = 0;
            v_limit->stay = 0;
            v_limit->day = 0;
        }
    } else {
        v_limit->numlogins = 0;
        v_limit->numposts = 0;
        v_limit->stay = 0;
        v_limit->day = 0;
    }
    clear();
    sprintf(limitfile, "vote/%s/limit.%lu", bname, ball->opendate);
    if (append_record(limitfile, v_limit, sizeof(struct votelimit)) == -1) {
        prints("�������صĴ����޷�д�������ļ�����ͨ��վ��");
        b_report("Append limit file Error!!");
    }
    /*Haohmaru.99.10.26.add below 8 lines */
    getdata(1, 0, "ȷ������ͶƱ?[Y] :", buf, 3, DOECHO, NULL, true);
    if (buf[0] == 'N' || buf[0] == 'n') {
        clear();
        prints("ȡ���˴�ͶƱ\n");
        unlink(limitfile);
        pressreturn();
        return FULLUPDATE;
    }
    strcpy(ball->userid, getCurrentUser()->userid);
    if (append_record(controlfile, ball, sizeof(*ball)) == -1) {
        prints("�������صĴ����޷�����ͶƱ����ͨ��վ��");
        b_report("Append Control file Error!!");
    } else {
        char votename[STRLEN];
        int i;

        setvoteflag(bname, 1);
        b_report("OPEN VOTE");
        prints("ͶƱ���Ѿ������ˣ�\n");
        range++;
        gettmpfilename(votename, "votetmp");
        //sprintf(votename, "tmp/votetmp.%d", getpid());
        if ((sug = fopen(votename, "w")) != NULL) {
            get_result_title();
            if (ball->type != VOTE_ASKING && ball->type != VOTE_VALUE) {
                fprintf(sug, "\n��ѡ�����¡�\n");
                for (i = 0; i < ball->totalitems; i++) {
                    fprintf(sug, "(\033[1m%c\033[m) %-40s\n", 'A' + i,
                            ball->items[i]);
                }
            }
            fclose(sug);
            sug = NULL;
            if (normal_board(bname)) {
                sprintf(buf, "[֪ͨ] %s �ٰ�ͶƱ��%s", bname, ball->title);
                post_file(getCurrentUser(), "", votename, "vote", buf, 0, 1, getSession());
            }
            sprintf(buf, "[֪ͨ] �ٰ�ͶƱ��%s", ball->title);
            post_file(getCurrentUser(), "", votename, bname, buf, 0, 1, getSession());
#ifdef BOARD_SECURITY_LOG
            sprintf(buf, "����ͶƱ <%s>", ball->title);
            board_security_report(votename, getCurrentUser(), buf, currboard->filename, NULL);
#endif
            unlink(votename);
        }
    }
    pressreturn();
    return FULLUPDATE;
}
int vote_flag(char *bname, char val, int mode)
{
    char buf[STRLEN], flag;
    int fd, num, size;

    num = getSession()->currentuid - 1;
    switch (mode) {
        case 2:
            sprintf(buf, "Welcome.rec");    /*��վ�� Welcome ���� */
            break;
        case 1:
            setvfile(buf, bname, "noterec");        /*����������¼����� */
            break;
        default:
            return -1;
    }
    if (num >= MAXUSERS) {
        bbslog("user","%s","Vote Flag, Out of User Numbers");
        return -1;
    }
    if ((fd = open(buf, O_RDWR | O_CREAT, 0600)) == -1) {
        char buf[STRLEN];

        sprintf(buf, "%s Flag file open Error.", bname);
        bbslog("user","%s",buf);
        return -1;
    }
    writew_lock(fd, 0, SEEK_SET, 0);
    size = (int) lseek(fd, 0, SEEK_END);
    memset(buf, 0, sizeof(buf));
    while (size <= num) {
        write(fd, buf, sizeof(buf));
        size += sizeof(buf);
    }
    lseek(fd, num, SEEK_SET);
    read(fd, &flag, 1);
    if ((flag == 0 && val != 0)) {
        lseek(fd, num, SEEK_SET);
        write(fd, &val, 1);
    }
    un_lock(fd, 0, SEEK_SET, 0);
    close(fd);
    return flag;
}

int vote_check(bits)
int bits;
{
    int i, count;

    for (i = count = 0; i < 32; i++) {
        if ((bits >> i) & 1)
            count++;
    }
    return count;
}
struct _setperm_select {
    unsigned int pbits;
    unsigned int basic;
    unsigned int oldbits;
    struct dual_perm *dp;
};
int vote_select(struct _select_def *conf)
{
    int count;
    struct _setperm_select *arg = (struct _setperm_select *) conf->arg;
    struct dual_perm *dp = (struct dual_perm *) arg->dp;

    if (conf->pos == conf->item_count)
        return SHOW_QUIT;
    arg->pbits ^= (1 << (conf->pos - 1));
    if (dp) {
        int ch;
        ch = (dp->curr)?0:1;
        count = vote_check(arg->pbits) + vote_check(dp->value[ch]);
    } else
        count = vote_check(arg->pbits);
    if (count > currvote.maxtkt) {
        if (currvote.maxtkt == 1) {
            arg->pbits = (1 << (conf->pos - 1));
            return SHOW_REFRESH;
        } else
            arg->pbits ^= (1 << (conf->pos - 1));
        return SHOW_CONTINUE;
    }
    move(2, 0);
    clrtoeol();
    prints("���Ѿ�Ͷ�� %d Ʊ", count);
    return SHOW_REFRESHSELECT;
}
int showvoteitems(struct _select_def *conf, int i)
{
    char buf[STRLEN];
    struct _setperm_select *arg = (struct _setperm_select *) conf->arg;
    struct dual_perm *dp = (struct dual_perm *) arg->dp;
    int n=0;

    if (dp)
        n = dp->curr;
    i = i - 1;
    if (i == conf->item_count - 1) {
        prints("--�˳�--");
    } else {
        sprintf(buf, "%c.%2.2s%-36.36s", 'A' + i,
                ((arg->pbits >> i) & 1 ? "��" : "  "), currvote.items[i+32*n]);
        prints("%s", buf);
    }
    return SHOW_CONTINUE;
}

void show_voteing_title()
{
    time_t closedate;
    char buf[STRLEN];

    if (currvote.type != VOTE_VALUE && currvote.type != VOTE_ASKING)
        sprintf(buf, "��ͶƱ��: %d Ʊ", currvote.maxtkt);
    else
        buf[0] = '\0';
    closedate = currvote.opendate + currvote.maxdays * 86400;
    prints("ͶƱ��������: %24s  %s  %s\n",
           ctime(&closedate), buf, (voted_flag) ? "(�޸�ǰ��ͶƱ)" : "");
    prints("ͶƱ������: \033[1m%-50s\033[m����: \033[1m%s\033[m \n", currvote.title,
           vote_type[currvote.type - 1]);
    if (currvote.type != VOTE_ASKING) {
        if (currvote.flag & VOTE_TRUE_FLAG)
            prints("�˴�ͶƱ��¼\033[1;31m�û�ͶƱ����%s\033[m\n", (currvote.flag & VOTE_IP_FLAG)? "������IP":"");
    }
}

int getsug(uv)
struct ballot *uv;
{
    int i, line;

    move(0, 0);
    clrtobot();
    if (currvote.type == VOTE_ASKING) {
        show_voteing_title();
        line = 3;
        prints("��������������(����):\n");
    } else {
        line = 1;
        prints("����������������(����):\n");
    }
    move(line, 0);
    for (i = 0; i < 3; i++) {
        prints(": %s\n", uv->msg[i]);
    }
    for (i = 0; i < 3; i++) {
        getdata(line + i, 0, ": ", uv->msg[i], STRLEN - 2, DOECHO, NULL,
                false);
        if (uv->msg[i][0] == '\0')
            break;
    }
    return i;
}

int multivote(uv)
struct ballot *uv;
{
    int i;
    struct dual_perm dp;

    dp.value[0] = uv->voted[0];
    dp.value[1] = uv->voted[1];
    dp.curr = 0;
    dp.cancel = false;

    move(0, 0);
    show_voteing_title();
    if (currvote.totalitems<33)
        dp.value[0] = setperms(dp.value[0], 0, "ѡƱ", currvote.totalitems, showvoteitems, vote_select, NULL);
    else {
        i = 0;
        while (1) {
            dp.value[i] = setperms(dp.value[i], 0, "ѡƱ", i==0?32:currvote.totalitems-32, showvoteitems, vote_select, &dp);
            if (dp.cancel)
                return -1;
            if (i==dp.curr)
                break;
            i = dp.curr;
        }
    }
    if (uv->voted[0] == dp.value[0] && uv->voted[1] == dp.value[1])
        return -1;
    uv->voted[0] = dp.value[0];
    uv->voted[1] = dp.value[1];
    return 1;
}

int valuevote(uv)
struct ballot *uv;
{
    unsigned int chs;
    char buf[10];

    chs = uv->voted[0];
    move(0, 0);
    show_voteing_title();
    prints("�˴������ֵ���ܳ��� \033[1m%d\033[m", currvote.maxtkt);
    if (uv->voted[0] != 0)
        sprintf(buf, "%d", uv->voted[0]);
    else
        memset(buf, 0, sizeof(buf));
    do {
        getdata(4, 0, "������һ��ֵ? [0]: ", buf, 5, DOECHO, NULL, false);
        uv->voted[0] = abs(atoi(buf));
    } while ((int) uv->voted[0] > currvote.maxtkt && buf[0] != '\n'
             && buf[0] != '\0');
    if (buf[0] == '\n' || buf[0] == '\0' || uv->voted[0] == chs)
        return -1;
    return 1;
}
int user_vote(int num)
{
    char fname[STRLEN], bname[STRLEN];
    char buf[STRLEN];
    struct ballot uservote, tmpbal;
    struct votelimit userlimit;
    int votevalue;
    int aborted = false, pos;

    if (!haspostperm(getCurrentUser(),currboard->filename)
#ifdef HAVE_USERSCORE
            ||!check_score_level(getCurrentUser(),currboard)
#endif /* HAVE_USERSCORE */
       )
        return -1;
    move(t_lines - 2, 0);
    if (get_record(controlfile, &currvote, sizeof(struct votebal), num)<0) {
        move(0, 0);
        clear();
        prints("��ͶƱ�ѽ�������ѡ������ͶƱ");
        pressreturn();
        return -1;
    }
    sprintf(fname, "vote/%s/flag.%lu", currboard->filename, currvote.opendate);
    if ((pos =
                search_record(fname, &uservote, sizeof(uservote),
                              (RECORD_FUNC_ARG) cmpvuid,
                              getCurrentUser()->userid)) <= 0) {
        (void) memset(&uservote, 0, sizeof(uservote));
        voted_flag = false;
    } else {
        voted_flag = true;
    }
    strcpy(uservote.uid, getCurrentUser()->userid);
    sprintf(bname, "desc.%lu", currvote.opendate);
    setvfile(buf, currboard->filename, bname);
    ansimore(buf, true);
    move(0, 0);
    /*Haohmaru.99.11.17.���ݰ���������������ж��Ƿ��ø�ʹ����ͶƱ */
    clear();
    userlimit.numlogins = 0;
    userlimit.numposts = 0;
    userlimit.stay = 0;
    userlimit.day = 0;
    sprintf(limitfile, "vote/%s/limit.%lu", currboard->filename, currvote.opendate);
    get_record(limitfile, &userlimit, sizeof(struct votelimit), 1);
    if ((currvote.type <= 0) || (currvote.type > 5))
        currvote.type = 1;
    if ((getCurrentUser()->numposts < userlimit.numposts
            || getCurrentUser()->numlogins < userlimit.numlogins
            || getCurrentUser()->stay < userlimit.stay * 60 * 60
            || (time(NULL) - getCurrentUser()->firstlogin) <
            userlimit.day * 24 * 60 * 60)) {
        prints
        ("�Բ���,�㲻��������涨�Ĵ˴�ͶƱ��������,�޷��μ�ͶƱ,лл����,�´��ټ�! :)");
        pressanykey();
        return -1;
    }
    clrtobot();
    switch (currvote.type) {
        case VOTE_SINGLE:
        case VOTE_MULTI:
        case VOTE_YN:
            votevalue = multivote(&uservote);
            if (votevalue == -1)
                aborted = true;
            break;
        case VOTE_VALUE:
            votevalue = valuevote(&uservote);
            if (votevalue == -1)
                aborted = true;
            break;
        case VOTE_ASKING:
            uservote.voted[0] = 0;
            aborted = !getsug(&uservote);
            break;
    }
    clear();
    if (!dashf(buf)) {
        move(0, 0);
        clear();
        prints("��ͶƱ�ѽ�������ѡ������ͶƱ");
        pressreturn();
        return -1;
    }
    if (aborted == true) {
        prints("���� ��%s��ԭ���ĵ�ͶƱ��\n", currvote.title);
    } else {
        if (currvote.type != VOTE_ASKING)
            getsug(&uservote);
        pos =
            search_record(fname, &tmpbal, sizeof(tmpbal),
                          (RECORD_FUNC_ARG) cmpvuid, getCurrentUser()->userid);
        if (pos) {
            substitute_record(fname, &uservote, sizeof(uservote), pos, NULL, NULL);
        } else if (append_record(fname, &uservote, sizeof(uservote)) == -1) {
            move(2, 0);
            clrtoeol();
            prints("ͶƱʧ��! ��֪ͨվ���μ���һ��ѡ��ͶƱ\n");
            pressreturn();
        }
        prints("\n�Ѿ�����(��)Ͷ��Ʊ����...\n");
    }
    pressanykey();
    return 0;
}

void voteexp()
{
    clrtoeol();
    prints("\033[44m��� ����ͶƱ���� ������ %-40s��� ���� ����\033[m\n",
           "ͶƱ����", "");
}
int printvote(struct votebal *ent, int idx, int *i)
{
    struct ballot uservote;
    char buf[STRLEN + 80], *date;
    char flagname[STRLEN];
    int num_voted;

    if (ent == NULL) {
        move(2, 0);
        voteexp();
        *i = 0;
        return 0;
    }
    (*i)++;
    if (*i > page + BBS_PAGESIZE || *i > range)
        return QUIT;
    else if (*i <= page)
        return 0;
    sprintf(buf, "flag.%lu", ent->opendate);
    setvfile(flagname, currboard->filename, buf);
    if (search_record
            (flagname, &uservote, sizeof(uservote), (RECORD_FUNC_ARG) cmpvuid,
             getCurrentUser()->userid) <= 0) {
        voted_flag = false;
    } else
        voted_flag = true;
    num_voted = get_num_records(flagname, sizeof(struct ballot));
    date = ctime(&ent->opendate) + 4;
    if ((ent->type <= 0) || (ent->type > 5))
        ent->type = 1;
    sprintf(buf, " %s%3d %-12.12s %-6.6s %-40.40s%-4.4s %3d %5d\033[m\n",
            (voted_flag == false) ? "\033[1m" : "", *i, ent->userid, date,
            ent->title, vote_type[ent->type - 1], ent->maxdays, num_voted);
    /*
        sprintf(buf," %s%3d %-12.12s %-6.6s %-40.40s%-4.4s %3d  %4d\033[m\n",(voted_flag==false)?"\033[1m":"",*i,ent->userid,
                date,ent->title,vote_type[ent->type-1],ent->maxdays,num_voted);
    */
    prints("%s", buf);
    return 0;
}
static int dele_vote(int num)
{
    char buf[STRLEN];

    sprintf(buf, "vote/%s/flag.%lu", currboard->filename, currvote.opendate);
    unlink(buf);
    sprintf(buf, "vote/%s/desc.%lu", currboard->filename, currvote.opendate);
    unlink(buf);
    sprintf(buf, "vote/%s/limit.%lu", currboard->filename, currvote.opendate);    /*Haohmaru.99.11.18 */
    unlink(buf);
    if (delete_record(controlfile, sizeof(currvote), num, NULL, NULL) ==
            -1) {
        prints("����������֪ͨվ��....");
        pressanykey();
    }
    range--;
    if (get_num_records(controlfile, sizeof(currvote)) == 0) {
        setvoteflag(currboard->filename, 0);
    }
    return 0;
}

int vote_results(bname)
char *bname;
{
    char buf[STRLEN];

    setvfile(buf, bname, "results");
    if (ansimore(buf, true) == -1) {
        move(3, 0);
        prints("Ŀǰû���κ�ͶƱ�Ľ����\n");
        clrtobot();
        pressreturn();
    } else
        clear();
    return FULLUPDATE;
}

int b_vote_maintain(struct _select_def* conf,struct fileheader *fileinfo,void* extraarg)
{
    int ret;
    int oldmode;
#ifdef NEW_HELP
    int oldhelpmode = helpmode;
    helpmode = HELP_VOTE;
#endif

    oldmode = uinfo.mode;
    modify_user_mode(VOTING);
    ret =  vote_maintain(currboard->filename);
    modify_user_mode(oldmode);

#ifdef NEW_HELP
    helpmode = oldhelpmode;
#endif
    return ret;
}

int vote_title()
{
    docmdtitle("[ͶƱ���б�]",
               "\033[m�뿪[\033[1;32m��\033[m,\033[1;32me\033[m] ����[\033[1;32mh\033[m] ����ͶƱ[\033[1;32m��\033[m,\033[1;32mr <cr>\033[m] ��,��ѡ��[\033[1;32m��\033[m,\033[1;32m��\033[m] \033[1m������\033[m��ʾ��δͶƱ");
    move(2, 0);
    prints("\033[0;1;37;44m��� ����ͶƱ���� ������ ͶƱ����                                ��� ���� ����");
    clrtoeol();
    update_endline();
    return 0;
}

int vote_key(ch, allnum, pagenum)
int ch;
int allnum, pagenum;
{
    int deal = 0, ans;
    char buf[STRLEN];
#ifdef BOARD_SECURITY_LOG
    char sugname[STRLEN];
    int i;
#endif

    switch (ch) {
        case 'v':
        case 'V':
        case '\n':
        case '\r':
        case 'r':
        case KEY_RIGHT:
            user_vote(allnum + 1);
            deal = 1;
            break;
        case 'R':
            vote_results(currboard->filename);
            deal = 1;
            break;
        case 'H':
        case 'h':
            show_help("help/votehelp");
            deal = 1;
            break;
        case 'A':
        case 'a':
#ifdef MEMBER_MANAGER
			if (!check_board_member_manager(&currmember, currboard, BMP_VOTE)) 
#else		
            if (!chk_currBM(currBM, getCurrentUser()))
#endif
				return true;
            vote_maintain(currboard->filename);
            deal = 1;
            break;
        case 'O':
        case 'o':
#ifdef MEMBER_MANAGER
			if (!check_board_member_manager(&currmember, currboard, BMP_VOTE)) 
#else		
            if (!chk_currBM(currBM, getCurrentUser()))
#endif
				return true;
            clear();
            deal = 1;
            get_record(controlfile, &currvote, sizeof(struct votebal),
                       allnum + 1);
            prints("\033[31m����!!\033[m\n");
            prints("ͶƱ����⣺\033[1m%s\033[m\n", currvote.title);
            ans = askyn("��ȷ��Ҫ����������ͶƱ��", 0);
            if (ans != 1) {
                move(2, 0);
                prints("ȡ��ɾ���ж�\n");
                pressreturn();
                clear();
                break;
            }
#ifdef BOARD_SECURITY_LOG
            gettmpfilename(sugname, "vote_close");
            if ((sug = fopen(sugname, "w")) != NULL) {
                get_result_title();
                if (currvote.type != VOTE_ASKING && currvote.type != VOTE_VALUE) {
                    fprintf(sug, "\n��ѡ�����¡�\n");
                    for (i = 0; i < currvote.totalitems; i++) {
                        fprintf(sug, "(\033[1m%c\033[m) %-40s\n", 'A' + i,
                                currvote.items[i]);
                    }
                }
                fclose(sug);
            }
            sprintf(buf, "����ͶƱ <%s>", currvote.title);
            board_security_report(sugname, getCurrentUser(), buf, currboard->filename, NULL);
            unlink(sugname);
            sug = NULL;
#endif
            mk_result(allnum + 1);
            sprintf(buf, "�������ͶƱ %s", currvote.title);
            /* securityreport(buf, NULL, NULL, getSession()); */
            bbslog("user","%s",buf);
            break;
        case '@':
            if (!HAS_PERM(getCurrentUser(), PERM_SYSOP))
                return true;
#ifdef NEWSMTH
            if (!strcmp(currboard->filename, DISCUSS_BOARD))
                return true;
#endif
            clear();
            deal = 1;
            get_record(controlfile, &currvote, sizeof(struct votebal),
                       allnum + 1);
            prints("���ͶƱ��\x1b[1m%s\x1b[m\n", currvote.title);
            check_result(allnum + 1);
            break;
        case 'D':
        case 'd':
#ifdef MEMBER_MANAGER
			if (!check_board_member_manager(&currmember, currboard, BMP_VOTE)) {
#else
            if (!chk_currBM(currBM, getCurrentUser())) {
#endif
				return 1;
            }
            deal = 1;
            get_record(controlfile, &currvote, sizeof(struct votebal),
                       allnum + 1);
            clear();
            prints("\033[31m����!!\033[m\n");
            prints("ͶƱ����⣺\033[1m%s\033[m\n", currvote.title);
            ans = askyn("��ȷ��Ҫǿ�ƹر����ͶƱ��", 0);
            if (ans != 1) {
                move(2, 0);
                prints("ȡ��ɾ���ж�\n");
                pressreturn();
                clear();
                break;
            }
#ifdef BOARD_SECURITY_LOG
            gettmpfilename(sugname, "vote_delete");
            if ((sug = fopen(sugname, "w")) != NULL) {
                get_result_title();
                if (currvote.type != VOTE_ASKING && currvote.type != VOTE_VALUE) {
                    fprintf(sug, "\n��ѡ�����¡�\n");
                    for (i = 0; i < currvote.totalitems; i++) {
                        fprintf(sug, "(\033[1m%c\033[m) %-40s\n", 'A' + i,
                                currvote.items[i]);
                    }
                }
                fclose(sug);
            }
            sprintf(buf, "�ر�ͶƱ <%s>", currvote.title);
            board_security_report(sugname, getCurrentUser(), buf, currboard->filename, NULL);
            unlink(sugname);
            sug = NULL;
#endif
            sprintf(buf, "ǿ�ƹر�ͶƱ %s", currvote.title);
            /* securityreport(buf, NULL, NULL, getSession()); */
            bbslog("user","%s",buf);
            dele_vote(allnum + 1);
            break;
        default:
            return 0;
    }
    if (deal) {
        Show_Votes();
        vote_title();
    }
    return 1;
}
static int Show_Votes()
{
    int i;

    move(3, 0);
    clrtobot();
    i = 0;
    setcontrolfile();
    if (apply_record
            (controlfile, (APPLY_FUNC_ARG) printvote, sizeof(struct votebal),
             &i, 0, false) == -1) {
        prints("����û��ͶƱ�俪��....");
        pressreturn();
        return 0;
    }
    clrtobot();
    return 0;
}

int b_vote(struct _select_def* conf,struct fileheader *fileinfo,void* extraarg)
{
    int num_of_vote;
    int voting;
#ifdef NEW_HELP
    int oldhelpmode = helpmode;
#endif

#ifdef NEW_BOARD_ACCESS
    if (!HAS_PERM(getCurrentUser(), PERM_LOGINOK) || new_deny_me(&uinfo, currboardent, NBA_MODE_DENY))
#else
    if (!HAS_PERM(getCurrentUser(), PERM_LOGINOK) || deny_me(getCurrentUser()->userid, currboard->filename))
#endif /* NEW_BOARD_ACCESS */
        return 0;               /* Leeward 98.05.15 */
    setcontrolfile();
    num_of_vote = get_num_records(controlfile, sizeof(struct votebal));
    if (num_of_vote == 0) {
        move(3, 0);
        clrtobot();
        prints("��Ǹ, Ŀǰ��û���κ�ͶƱ���С�\n");
        pressreturn();
        setvoteflag(currboard->filename, 0);
        return FULLUPDATE;
    }
    setlistrange(num_of_vote);
    clear();
#ifdef NEW_HELP
    helpmode = HELP_VOTE;
#endif
    voting = choose(false, 0, vote_title, vote_key, Show_Votes, user_vote);
#ifdef NEW_HELP
    helpmode = oldhelpmode;
#endif
    clear();
    return /*user_vote( currboard->filename ) */ FULLUPDATE;
}

int m_vote(void)
{
    modify_user_mode(ADMIN);
    vote_maintain(DEFAULTBOARD);
    return 0;
}

int x_vote(void)
{
    struct boardheader* bh;
    int bid;
    modify_user_mode(XMENU);
    bh=currboard;
    bid=currboardent;
    b_vote(NULL,NULL,NULL);
    currboard=bh;
    currboardent=bid;
    return 0;
}

int x_results(void)
{
    modify_user_mode(XMENU);
    return vote_results(DEFAULTBOARD);
}

int get_vote_record(struct ballot *ptr, int idx, char *arg)
{
    int i; 
    char sv[2];
    struct userec *user;

    strcpy(vc[pos].userid, ptr->uid);
    if (getuser(ptr->uid, &user)) {
        strcpy(vc[pos].ip, user->lasthost);
        if (!(currvote.flag & VOTE_IP_FLAG)) {
            char * c;
            if ((c = strrchr(vc[pos].ip, '.'))!=NULL) {
                *(++c)='*';
                *(++c)='\0';
            }   
        }   
    } else
        strcpy(vc[pos].ip, "unknown");
    if (currvote.type == VOTE_VALUE) {  // ��¼����ͶƱ���
        sprintf(vc[pos].selectvalue, "%d", ptr->voted[0]);
    } else {
        for (i=0;i<32;i++) {
            if ((ptr->voted[0]>>i)&1) {
                sprintf(sv, "%c", 'A'+i);
                strcat(vc[pos].selectvalue, sv);
            }
        }
        for (i=0;i<32;i++) {
            if ((ptr->voted[1]>>i)&1) {
                sprintf(sv, "%c", i<26?'a'+i:i+7);
                strcat(vc[pos].selectvalue, sv);
            }
        }
    }
    pos ++;
    return 0;
}

static int dump_vote_record(void)
{
    char fname[255];
    int num;

    sprintf(fname, "vote/%s/flag.%lu", currboard->filename, currvote.opendate);

    num = get_num_records(fname, sizeof(struct ballot));
    vc = (struct voterecord *)malloc(num * sizeof(struct voterecord));
    memset(vc, 0, num * sizeof(struct voterecord));

    pos = 0;
    if (apply_record
            (fname, (APPLY_FUNC_ARG) get_vote_record, sizeof(struct ballot), 0, 0, false) == -1) {
        bbslog("user","%s","Vote apply flag error");
    }

    return 0;
}

static int sort_vote_record(void)
{
    int i, j, s;
    struct voterecord tc;

    for (i=0;i<pos;i++) {
        s = i;
        for (j=i+1;j<pos;j++) {
            if (ntohl(inet_addr(vc[s].ip)) > ntohl(inet_addr(vc[j].ip))) {
                s = j;
            }
        }
        memcpy(&tc, &vc[i], sizeof(struct voterecord));
        memcpy(&vc[i], &vc[s], sizeof(struct voterecord));
        memcpy(&vc[s], &tc, sizeof(struct voterecord));
    }
    return 0;
}

/* �ú���ʹ��ǰ��Ҫȷ���Ѿ�dump_vote_record */
/*
 * mode: 0: δ����
 *       1: ������
 */
static int write_vote_record(char *file, int mode)
{
    FILE *fd;
    int i;

    fd = fopen(file, "r+");
    fseek(fd, 0, SEEK_END);

    fprintf(fd, "\033[44m\033[36m������������������������������%s������������������������������\033[m\n\n\n",
            mode?"ʹ����IP��ַ����":"ʹ����ͶƱ������");
    for (i=0;i<pos;i++) {
        fprintf(fd, "%15s %20s      %-33s\n", vc[i].userid, vc[i].ip, vc[i].selectvalue);
    }
    fprintf(fd, "\n\n\033[44m\033[36m������������������������������%s������������������������������\033[m\n\n\n",
            mode?"ʹ����IP��ַ����":"ʹ����ͶƱ������");
    fclose(fd);

    return 0;
}