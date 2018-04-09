/*
 * bm的操作函数
 */

#include "bbs.h"

/* 获取txt文件的内容，并返回字符串数组
 * 要确保result数组不大于maxcount，调用完成后caller需要free(*ptr)
 * return:
 *        0 文件不存在或错误
 *        count 字符串数
 */
int get_textfile_string(const char *file, char **ptr, char *result[], int maxcount)
{
    int fd;
    int count, size;
    struct stat st;
    char *p, *q;
    
    if ((fd=open(file, O_RDONLY))==-1)
        return 0;
    if (fstat(fd, &st)!=0 || st.st_size==0) {
        close(fd);
        return 0;
    }
    *ptr = (char *)malloc(st.st_size);
    read(fd, *ptr, st.st_size);
    count = 0;
    q = p = *ptr;
    if (*p!='#' && *p!='\n') /* 跳过注释行和长度为0的行 */
        result[count++] = p;
    size = st.st_size;
    while (size>0 && (p=(char *)memmem(p, size, "\n", 1))!=NULL) {
        *p = '\0';
        p++;
        if (*p == '\r')
            p++;
        size -= p-q;
        if (size<=0 || count>=maxcount)
            break;
        q = p;
        if (*p!='#' && *p!='\n') /* 跳过注释行和长度为0的行 */
            result[count++] = p;
    }
    /*
    for (i=1;i<st.st_size;i++) {
        if ((*ptr)[i] == '\n') {
            (*ptr)[i] = '\0';
            i++;
            count++;
            if (count>=maxcount)
                break;
            if (i==st.st_size)
                break;
            if ((*ptr)[i] == '\r') {
                i++;
                if (i==st.st_size)
                    break;
                result[count] = &((*ptr)[i]);
            } else {
                result[count] = &((*ptr)[i]);
            }
        }
    }
    */
    close(fd);
    return count;
}

/*
 * 获得封禁理由列表，denyreason在调用时定义
 * board为空时，获得系统封禁理由列表 
 */
int get_deny_reason(const char *board, char denyreason[][STRLEN], int max)
{
    char filename[STRLEN];
    char *ptr, *reason[MAXDENYREASON];
    int count, i;

    max = (max>MAXDENYREASON) ? MAXDENYREASON : max;
    if (board)
        setvfile(filename, board, "deny_reason");
    else
        sprintf(filename, "etc/deny_reason");
    count = get_textfile_string(filename, &ptr, reason, max);
    if (count > 0) {
        for (i=0;i<count;i++) {
            remove_blank_ctrlchar(reason[i], denyreason[i], true, true, true);
            denyreason[i][30] = '\0';
            //strcpy(denyreason[i], reason[i]);
        }
        free(ptr);
    }
    return count;
}

int save_deny_reason(const char *board, char denyreason[][STRLEN], int count)
{
    char filename[STRLEN];
    int i;
    FILE *fn;

    setvfile(filename, board, "deny_reason");

#ifdef BOARD_SECURITY_LOG
    char oldfilename[STRLEN];
    gettmpfilename(oldfilename, "old_deny_reason");
    f_cp(filename, oldfilename, 0); 
#endif

    if ((fn=fopen(filename, "w"))!=NULL) {
        for (i=0;i<count;i++)
            fprintf(fn, "%s\n", denyreason[i]);
        fclose(fn);
#ifdef BOARD_SECURITY_LOG
        board_file_report(board, "修改 <版面封禁理由>", oldfilename, filename);
        unlink(oldfilename);
#endif
    }
    return 0;
}

/* 从封禁文件读取的字符串中获得封禁理由, 与文件内容格式关系紧密 */
int get_denied_reason(const char *buf, char *reason)
{
    char genbuf[STRLEN*2], *p;

    strcpy(genbuf, buf);
    p = genbuf + IDLEN;
    genbuf[42] = '\0';
    remove_blank_ctrlchar(p, reason, true, true, false);
    return 0;
}

/* 从封禁文件读取的字符串中获得封禁操作ID, 与文件内容格式关系紧密 */
int get_denied_operator(const char *buf, char *opt)
{
    char genbuf[STRLEN*2], *p;

    strcpy(genbuf, buf);
    p = genbuf+43;
    p[IDLEN] = '\0';
    remove_blank_ctrlchar(p, opt, true, true, false);
    return 0;
}

#ifdef MANUAL_DENY
/* 从封禁文件读取的字符串中获得自动解封/手动解封选项，与文件内容格式关系紧密 */
int get_denied_freetype(const char *buf)
{
    return (!strncmp(buf+64, "解", 2));    
}
#endif

/* 从封禁文件读取的字符串中获取封禁时间 */
time_t get_denied_time(const char *buf)
{
    time_t denytime;
    char genbuf[STRLEN*2], *p;

    strcpy(genbuf, buf);
    if ((NULL != (p = strrchr(genbuf, '['))) && *(p+12)!='\n')
        sscanf(p + 12, "%lu", &denytime);
    else
        denytime = time(0) + 1;

    return denytime;
}

/* 从封禁文件读取的字符串中获取解封时间 */
time_t get_undeny_time(const char *buf)
{
    time_t undenytime;
    char genbuf[STRLEN*2], *p;

    strcpy(genbuf, buf);
    if (NULL != (p = strrchr(genbuf, '[')))
        sscanf(p + 1, "%lu", &undenytime);
    else
        undenytime = time(0) + 1;

    return undenytime;
}

/* 热点版面 */
int is_hot_board(const char *board)
{
    return seek_in_file("etc/hotboards", board, NULL);
}


/* 使用模板发布封禁公告
 * mode:
 *      0:添加
 *      1:修改原因
 *      2:修改时间
 */
#ifdef RECORD_DENY_FILE
int deny_announce(char *uident, const struct boardheader *bh, char *reason, int day, struct userec *operator, time_t time, int mode, const struct fileheader *fh, int filtermode)
#else
int deny_announce(char *uident, const struct boardheader *bh, char *reason, int day, struct userec *operator, time_t time, int mode)
#endif
{
    struct userec *lookupuser;
    char tmplfile[STRLEN], postfile[STRLEN], title[STRLEN], title1[STRLEN], timebuf[STRLEN];
    int bm=0;
#ifdef MEMBER_MANAGER
    int core_member=0;
    struct board_member member;
#endif	
#ifdef NEWSMTH
    int score = -1;
#endif

    gettmpfilename(postfile, "ann_deny.%d", getpid());
    sprintf(tmplfile, "etc/denypost_template");

    /* bm优先级最高，core次之，最后是站务 */
    if (HAS_PERM(operator, PERM_BOARDS) && chk_BM_instr(bh->BM, operator->userid))
        bm = 1;
    else {
#ifdef MEMBER_MANAGER
        bzero(&member, sizeof(struct board_member));
        if(get_board_member(bh->filename, operator->userid, &member)==BOARD_MEMBER_STATUS_MANAGER && member.flag&BMP_DENY)
			core_member = 1;
#endif
    }

    getuser(uident, &lookupuser);
    if (mode==0) {
        sprintf(title, "%s被取消在%s版的发文权限", uident, bh->filename);
        if (PERM_BOARDS & lookupuser->userlevel)
            sprintf(title1, "%s 封某版" NAME_BM " %s 在 %s", operator->userid, uident, bh->filename);
        else
            sprintf(title1, "%s 封 %s 在 %s", operator->userid, uident, bh->filename);
    } else {
        sprintf(title, "修改%s在%s版的发文权限封禁", uident, bh->filename);
        if (PERM_BOARDS & lookupuser->userlevel)
            sprintf(title1, "%s 修改某版" NAME_BM " %s 在 %s 的封禁", operator->userid, uident, bh->filename);
        else
            sprintf(title1, "%s 修改 %s 在 %s 的封禁", operator->userid, uident, bh->filename);
    }
    if (dashf(tmplfile)) {
        char daystr[4], opbuf[STRLEN];
        sprintf(daystr, "%d", day);
        if (bm)
            sprintf(opbuf, NAME_BM ":\x1b[4m%s\x1b[m", operator->userid);
#ifdef MEMBER_MANAGER
		else if (core_member)
			sprintf(opbuf, NAME_CORE_MEMBER ":\x1b[4m%s\x1b[m", operator->userid);
#endif
		else
            sprintf(opbuf, "%s" NAME_SYSOP_GROUP DENY_NAME_SYSOP "：\x1b[4m%s\x1b[m",  NAME_BBS_CHINESE, operator->userid);
        if (write_formatted_file(tmplfile, postfile, "ssssss",
                    uident, bh->filename, reason, daystr, opbuf, ctime_r(&time, timebuf))<0)
            return -1;
    } else {
        FILE *fn;
        fn = fopen(postfile, "w+");
        fprintf(fn, "由于 \x1b[4m%s\x1b[m 在 \x1b[4m%s\x1b[m 版的 \x1b[4m%s\x1b[m 行为，\n", uident, bh->filename, reason);
        fprintf(fn, DENY_BOARD_AUTOFREE " \x1b[4m%d\x1b[m 天。\n", day);
        /* day不允许0天，有需要的自行改造 */
        /*
        if (day)
            fprintf(fn, DENY_BOARD_AUTOFREE " \x1b[4m%d\x1b[m 天。\n", day);
        else
            fprintf(fn, DENY_BOARD_NOAUTOFREE "\n");
        */
#ifdef ZIXIA
        GetDenyPic(fn, DENYPIC, ndenypic, dpcount);
#endif 
        if (bm) {
            fprintf(fn, "                              " NAME_BM ":\x1b[4m%s\x1b[m\n", operator->userid);
        }
#ifdef MEMBER_MANAGER
		else if (core_member) {
			fprintf(fn, "                              " NAME_CORE_MEMBER ":\x1b[4m%s\x1b[m\n", operator->userid);
		}
#endif
		else {
            fprintf(fn, "                            %s" NAME_SYSOP_GROUP DENY_NAME_SYSOP "：\x1b[4m%s\x1b[m\n", NAME_BBS_CHINESE, operator->userid);
        }
        fprintf(fn, "                              %s\n", ctime_r(&time, timebuf));
        fclose(fn);
    }
#ifdef NEWSMTH
	post_file(operator, "", postfile, bh->filename, title, 0, 1 /*core_member?2:1*/, getSession());
#else
    post_file(operator, "", postfile, bh->filename, title, 0, 2, getSession());
#endif

/* 热点版面14d自动扣积分 */
#ifdef NEWSMTH
    if (mode!=1 && day>=14 && is_hot_board(bh->filename)) {
        char scoretitle[STRLEN], strbuf[STRLEN], filebuf[STRLEN];
        FILE *fn, *fn2;
        int tmp;
        time_t current=time(NULL);

        score = lookupuser->score_user>2000?2000:lookupuser->score_user;
        if (score>0) {
            tmp = AO_int_fetch_and_add(&(lookupuser->score_user), -score);

            /* 积分变更通知 */
            sprintf(scoretitle, "%s 版封禁自动扣分", bh->filename);
            score_change_mail(lookupuser, lookupuser->score_user+score, lookupuser->score_user, 0, 0, scoretitle);

            /* score公告 */
            sprintf(scoretitle, "[公告] 扣除 %s 积分 %d 分", lookupuser->userid, score);
            gettmpfilename(filebuf, "deny_score");
            if ((fn=fopen(filebuf, "w"))!=NULL) {
                fprintf(fn, "说明：\n"
                            "\t依据积分使用项目 008 号\n\n"
                            "附件：\n\n");
                /* 只能手动添加信头了 */
                fprintf(fn,"发信人: %s (自动发信系统), 信区: %s\n", DELIVER, bh->filename);
                fprintf(fn,"标  题: %s\n",title);
                fprintf(fn,"发信站: %s自动发信系统 (%24.24s)\n\n",BBS_FULL_NAME,ctime_r(&current, strbuf));

                if ((fn2=fopen(postfile, "r"))!=NULL) {
                    while (fgets(strbuf, 256, fn2)!=NULL)
                        fprintf(fn, "%s", strbuf);
                    fclose(fn2);
                }
                fclose(fn);
            }
            post_file(operator, "", filebuf, "Score", scoretitle, 0, 1, getSession());
            unlink(filebuf);
        }
    }
#endif

#ifdef RECORD_DENY_FILE
    if (fh) {
        char bname[STRLEN], filebuf[STRLEN], filestr[256];
        FILE *fn, *fn2;
        int size;
        
        fn = fopen(postfile, "r+");
        fseek(fn, 0, SEEK_END);
        if (filtermode==0)
            sprintf(bname, "%s", bh->filename);
        else if (filtermode==1)
            sprintf(bname, "%s", "Filter");
        else
            sprintf(bname, "%sFilter", bh->filename);
        setbfile(filebuf, bname, fh->filename);
        if (!dashf(filebuf)) {
            int i;
            char prefix[4]="MDJ", *p, ch;
            p = strrchr(filebuf, '/') + 1;
            ch = *p;
            for (i=0;i<3;i++) {
                if (ch == prefix[i])
                    continue;
                *p = prefix[i];
                if (dashf(filebuf))
                    break;
            }
        }
        if ((fn2=fopen(filebuf, "r"))==NULL) {
            fprintf(fn, "\033[1;31;45m系统问题, 无法显示全文, 请联系技术站务. \033[K\033[m\n");
        } else {
            fprintf(fn, "\033[1;33;45m以下是被封文章全文");
            if (filtermode)
                fprintf(fn, "(来源于\033[32m%s\033[33m过滤区)", filtermode==1?"系统":bh->filename);
            fprintf(fn, ":\033[K\033[m\n");
            while ((size=-attach_fgets(filestr,256,fn2))) {
                if (size<0)
                    fprintf(fn,"%s",filestr);
                else
                    put_attach(fn2,fn,size);
            }
            fclose(fn2);
            fprintf(fn, "\033[1;33;45m全文结束.\033[K\033[m\n");
        }
        fclose(fn);
    }
#endif

/* 热点版面14d自动扣积分 */
#ifdef NEWSMTH
    if (score>=0) {
        char strbuf[STRLEN];

        /* denypost标题 */
        sprintf(strbuf, " [-%d]", score);
        strcat(title1, strbuf);
    }
#endif
    post_file(operator, "", postfile, "denypost", title1, 0, -1, getSession());
#ifdef BOARD_SECURITY_LOG
    FILE *fn;
    fn = fopen(postfile, "w");
    fprintf(fn, "\033[33m封禁用户: \033[4;32m%s\033[m\n", uident);
    fprintf(fn, "\033[33m封禁原因: \033[4;32m%s\033[m\n", reason);
    fprintf(fn, "\033[33m封禁天数: \033[4;32m %d 天\033[m\n", day);
#ifdef NEWSMTH
    if (score)
        fprintf(fn, "\033[33m扣除积分: \033[4;32m%d\033[m\n", score);
#endif
    fclose(fn);
    sprintf(title, "%s封禁%s", mode?"修改":"", uident);
#ifdef RECORD_DENY_FILE
    board_security_report(postfile, operator, title, bh->filename, fh);
#else
    board_security_report(postfile, operator, title, bh->filename, NULL);
#endif
#endif
    unlink(postfile);

    return 0;
}

/* 使用模板发布封禁邮件
 * mode:
 *      0:添加
 *      1:修改
 */
int deny_mailuser(char *uident, const struct boardheader *bh, char *reason, int day, struct userec *operator, time_t time, int mode, int autofree)
{
    struct userec *lookupuser;
    char tmplfile[STRLEN], mailfile[STRLEN], title[STRLEN], timebuf[STRLEN];
    int bm=0, core_member=0;

#ifdef MEMBER_MANAGER
    struct board_member member;
#endif
	
    gettmpfilename(mailfile, "mail_deny.%d", getpid());
    sprintf(tmplfile, "etc/denymail_template");

    /* bm优先级最高，core次之，最后是站务 */
    if (HAS_PERM(operator, PERM_BOARDS) && chk_BM_instr(bh->BM, operator->userid))
        bm = 1;
    else {
#ifdef MEMBER_MANAGER
        bzero(&member, sizeof(struct board_member));
        if(get_board_member(bh->filename, operator->userid, &member)==BOARD_MEMBER_STATUS_MANAGER && member.flag&BMP_DENY)
            core_member = 1;
#endif
    }

    getuser(uident, &lookupuser);
    if (mode==0) {
        sprintf(title, "%s被取消在%s版的发文权限", uident, bh->filename);
    } else {
        sprintf(title, "修改%s在%s版的发文权限封禁", uident, bh->filename);
    }
    if (dashf(tmplfile)) {
        char sender[STRLEN], sitename[STRLEN], opfrom[STRLEN], daystr[16], opbuf[STRLEN], replyhint[STRLEN];
        sprintf(daystr, "\x1b[4m%d\x1b[m 天", day);
        if (!autofree)
            sprintf(replyhint, "，到期后请回复\n此信申请恢复权限");
        else
            replyhint[0] = '\0';
        if (!bm && !core_member) {
            sprintf(sender, "SYSOP (%s) ", NAME_SYSOP);
            sprintf(sitename, "%s (%24.24s)", BBS_FULL_NAME, ctime_r(&time, timebuf));
            sprintf(opfrom, "%s", NAME_BBS_ENGLISH);
            sprintf(opbuf, "%s" NAME_SYSOP_GROUP DENY_NAME_SYSOP "：\x1b[4m%s\x1b[m",NAME_BBS_CHINESE, operator->userid);
        } else {
            sprintf(sender, "%s ", operator->userid);
            sprintf(sitename, "%s (%24.24s)", BBS_FULL_NAME, ctime_r(&time, timebuf));
            sprintf(opfrom, "%s", SHOW_USERIP(operator, getSession()->fromhost));
#ifdef MEMBER_MANAGER
			if (core_member)
			sprintf(opbuf, NAME_CORE_MEMBER ":\x1b[4m%s\x1b[m", operator->userid);
			else
#endif			
            sprintf(opbuf, NAME_BM ":\x1b[4m%s\x1b[m", operator->userid);
        }
        if (write_formatted_file(tmplfile, mailfile, "ssssssssss",
                    sender, title, sitename, opfrom, bh->filename, reason, daystr, replyhint, opbuf, ctime_r(&time, timebuf))<0)
            return -1;
    } else {
        FILE *fn;
        fn = fopen(mailfile, "w+");
        if (!bm && !core_member) {
            fprintf(fn, "寄信人: SYSOP (%s) \n", NAME_SYSOP);
            fprintf(fn, "标  题: %s\n", title);
            fprintf(fn, "发信站: %s (%24.24s)\n", BBS_FULL_NAME, ctime_r(&time, timebuf));
            fprintf(fn, "来  源: %s\n", NAME_BBS_ENGLISH);
            fprintf(fn, "\n");
            fprintf(fn, "由于您在 \x1b[4m%s\x1b[m 版 \x1b[4m%s\x1b[m，我很遗憾地通知您， \n", bh->filename, reason);
            fprintf(fn, DENY_DESC_AUTOFREE " \x1b[4m%d\x1b[m 天", day);
            /* day不允许0天，有需要的自行改造 */
            /* if (day)
                fprintf(fn, DENY_DESC_AUTOFREE " \x1b[4m%d\x1b[m 天", day);
            else
                fprintf(fn, DENY_DESC_NOAUTOFREE);
            */
            if (!autofree)
                fprintf(fn, "，到期后请回复\n此信申请恢复权限。\n");
            fprintf(fn, "\n");
            fprintf(fn, "                            %s" NAME_SYSOP_GROUP DENY_NAME_SYSOP "：\x1b[4m%s\x1b[m\n", NAME_BBS_CHINESE, operator->userid);
            fprintf(fn, "                              %s\n", ctime_r(&time, timebuf));
        } else {
            fprintf(fn, "寄信人: %s \n", operator->userid);
            fprintf(fn, "标  题: %s\n", title);
            fprintf(fn, "发信站: %s (%24.24s)\n", BBS_FULL_NAME, ctime_r(&time, timebuf));
            fprintf(fn, "来  源: %s \n", SHOW_USERIP(operator, getSession()->fromhost));
            fprintf(fn, "\n");
            fprintf(fn, "由于您在 \x1b[4m%s\x1b[m 版 \x1b[4m%s\x1b[m，我很遗憾地通知您， \n", bh->filename, reason);
            fprintf(fn, DENY_DESC_AUTOFREE " \x1b[4m%d\x1b[m 天", day);
            /* day不允许0天，有需要的自行改造 */
            /* if (day)
                fprintf(fn, DENY_DESC_AUTOFREE " \x1b[4m%d\x1b[m 天", day);
            else
                fprintf(fn, DENY_DESC_NOAUTOFREE);
            */
            if (!autofree)
                fprintf(fn, "，到期后请回复\n此信申请恢复权限。\n");
#ifdef ZIXIA
            GetDenyPic(fn, DENYPIC, ndenypic, dpcount);
#endif 
            fprintf(fn, "\n");
#ifdef MEMBER_MANAGER
			if (core_member)
			fprintf(fn, "                              " NAME_CORE_MEMBER ":\x1b[4m%s\x1b[m\n", operator->userid);
			else
#endif				
            fprintf(fn, "                              " NAME_BM ":\x1b[4m%s\x1b[m\n", operator->userid);
            fprintf(fn, "                              %s\n", ctime_r(&time, timebuf));
        }
        fclose(fn);
    }
    if (lookupuser)
#ifdef NEWSMTH
        mail_file(DELIVER /*core_member?operator->userid:DELIVER*/, mailfile, uident, title, 0, NULL);
#else
        mail_file(operator->userid, mailfile, uident, title, 0, NULL);
#endif
    unlink(mailfile);

    return 0;
}

#ifdef TITLEKEYWORD
/*
 * 获得标题关键字列表，titlekey在调用时定义
 * board为空时，获得系统标题关键字
 */
int get_title_key(const char *board, char titlekey[][8], int max)
{
    char filename[STRLEN];
    char *ptr, *key[MAXTITLEKEY];
    int count, i;

    max = (max>MAXTITLEKEY) ? MAXTITLEKEY : max;
    if (board)
        setvfile(filename, board, "title_keyword");
    else
        sprintf(filename, "etc/title_keyword");
    count = get_textfile_string(filename, &ptr, key, max);
    if (count > 0) {
        for (i=0;i<count;i++) {
            remove_blank_ctrlchar(key[i], titlekey[i], true, true, true);
            titlekey[i][6] = '\0';
        }   
        free(ptr);
    }   
    return count;
}   

int save_title_key(const char *board, char titlekey[][8], int count)
{
    char filename[STRLEN];
    int i;
    FILE *fn;
    
    setvfile(filename, board, "title_keyword");

#ifdef BOARD_SECURITY_LOG
    char oldfilename[STRLEN];
    gettmpfilename(oldfilename, "old_title_keyword");
    f_cp(filename, oldfilename, 0);
#endif

    if ((fn=fopen(filename, "w"))!=NULL) {
        for (i=0;i<count;i++)
            fprintf(fn, "%s\n", titlekey[i]);
        fclose(fn);
        load_title_key(0, getbid(board, NULL), board);
#ifdef BOARD_SECURITY_LOG
        board_file_report(board, "修改 <标题关键字>", oldfilename, filename);
        unlink(oldfilename);
#endif
    }
    return 0;
}
#endif

#ifdef BOARD_SECURITY_LOG
struct post_report_arg {
    char *file;
    int id;
    int count;
    int bm;
    time_t tm;
};

/* 生成文章对应的操作记录索引，使用apply_record回调 */
int make_post_report(struct fileheader *fh, int idx, struct post_report_arg *pa){
    if (get_posttime(fh)<pa->tm)
        return QUIT;
    if (fh->o_id == pa->id) {
        /* 非版主不能查看除m/g之外的操作 */
        if (!pa->bm &&
                strncmp(fh->title, "标m ", 4) && strncmp(fh->title, "标g ", 4) && strncmp(fh->title, "去m ", 4) && strncmp(fh->title, "去g ", 4))
            return 0;
        append_record(pa->file, fh, sizeof(struct fileheader));
        pa->count++;
        return 1;
    }
    return 0;
}

int make_post_report_dir(char *index, struct boardheader *bh, struct fileheader *fh, int bm) {
    char index_s[STRLEN];
    struct stat st;
    struct post_report_arg pa;
    
    setbdir(DIR_MODE_BOARD, index_s, bh->filename);
    if (!dashf(index_s) || stat(index_s, &st)==-1 || st.st_size==0)
        return 0;
    
    sprintf(index, "%s.%d[%d]", index_s, fh->id, getpid());
    bzero(&pa, sizeof(struct post_report_arg));
    pa.file = index;
    pa.id = fh->id;
    pa.bm = bm;
    pa.tm = get_posttime(fh);
    
    apply_record(index_s, (APPLY_FUNC_ARG)make_post_report, sizeof(struct fileheader), &pa, 0, 1);
    reverse_record(index, sizeof(struct fileheader));
    return pa.count;
}

/* 从删除区找到对应id的有效帖子，用于apply_record回调 */
int get_report_deleted_ent_by_id(struct fileheader *fh, int idx, struct post_report_arg *pa){
    if (fh->id == pa->id) {
        if (fh->filename[0])
            pa->count = idx;
        return QUIT;
    }
    return 0;
}

/* 根据操作记录原文ID号在删除区找原文 */
int get_report_deleted_ent(struct fileheader *fh, struct boardheader *bh) {
    char dir[STRLEN];
    struct post_report_arg pa;
    bzero(&pa, sizeof(struct post_report_arg));
    pa.id = fh->o_id;
    setbdir(DIR_MODE_DELETED, dir, bh->filename);
    /* 逆序从删除区找到最近一篇删除文章 */
    apply_record(dir, (APPLY_FUNC_ARG)get_report_deleted_ent_by_id, sizeof(struct fileheader), &pa, 0, 1);
    return pa.count;
}

/* 版面配置文件修改记录, 通过diff获得新旧文件差异
 */
void board_file_report(const char *board, char *title, char *oldfile, char *newfile)
{       
    FILE *fn, *fn2;
    char filename[STRLEN], buf[256];

    gettmpfilename(filename, "board_report_file", getpid());
    if((fn=fopen(filename, "w"))!=NULL){
        if(strstr(title, "删除")){
            fprintf(fn, "\033[1;33;45m删除文件信息备份\033[K\033[m\n");
    
            if((fn2=fopen(oldfile, "r"))!=NULL){
                while(fgets(buf, 256, fn2)!=NULL)
                    fputs(buf, fn);
                fclose(fn2);
                fprintf(fn, "\n");
            }
            fclose(fn);
        } else {
            char genbuf[STRLEN*2], filediff[STRLEN];
            gettmpfilename(filediff, "filediff", getpid());
            if(!dashf(oldfile)){
                f_touch(oldfile);
            }
            sprintf(genbuf, "diff -u %s %s > %s", oldfile, newfile, filediff);
            system(genbuf);
            fprintf(fn, "\033[1;33;45m修改文件信息备份\033[K\033[m\n");
        
            if((fn2=fopen(filediff, "r"))!=NULL){
                /* 跳过前2行 */
                fgets(buf, 256, fn2);fgets(buf, 256, fn2);
                while(fgets(buf, 256, fn2)!=NULL){
                    if(buf[0]=='-')
                        fprintf(fn, "\033[1;35m%s\033[m", buf);
                    else if(buf[0]=='+')
                        fprintf(fn, "\033[1;36m%s\033[m", buf);
                    else
                        fputs(buf, fn);
                }
                fclose(fn2);
                fprintf(fn, "\n");
            }
            fclose(fn);
            unlink(filediff);
        }
        board_security_report(filename, getCurrentUser(), title, board, NULL);
        unlink(filename);
    }
}

/* 获得版面特殊模式下最新id */
int board_last_index_id(const char *board, int mode)
{
    struct fileheader fh;
    char filename[STRLEN * 2];
    int count;

    setbdir(mode, filename, board);
    memset(&fh, 0, sizeof(struct fileheader));
    count = get_num_records(filename, sizeof(struct fileheader));
    if (count <= 0)
        return 0;
    get_record(filename, &fh, sizeof(struct fileheader), count);

    if (fh.id != 0)
        return fh.id;
    return 0;
}

/* 版面安全记录，在DIR_MODE_BOARD中添加文章, jiangjun, 20100610 */
int board_security_report(const char *filename, struct userec *user, const char *title, const char *bname, const struct fileheader *xfh)
{       
    struct fileheader fh;
    struct boardheader bh;
    FILE *fin, *fout;
    char buf[STRLEN], bufcp[READ_BUFFER_SIZE], timebuf[STRLEN];
    int mode, fd;
    time_t now;
    size_t size;
            
    bzero(&fh, sizeof(struct fileheader));
    setbpath(buf, bname);
    if (!getboardnum(bname, &bh))
        return 1;
    if (GET_POSTFILENAME(fh.filename, buf))
        return 2;
    POSTFILE_BASENAME(fh.filename)[0] = 'B';
    set_posttime(&fh);
    
    mode = 0;
    if (user == NULL)
        mode = 1;  //自动发信
    if (mode)
        memcpy(fh.owner, DELIVER, OWNER_LEN);
    else
        memcpy(fh.owner, user -> userid, OWNER_LEN);
    fh.owner[OWNER_LEN - 1] = 0;

    if (xfh) {
        if (strlen(xfh->title)>40) {
            strnzhcpy(buf, xfh->title, 38);
            strcat(buf, "..");
        } else
            strcpy(buf, xfh->title);
        sprintf(fh.title, "%s <%s>", title, buf);
    } else
        strnzhcpy(fh.title, title, ARTICLE_TITLE_LEN);

    setbfile(buf, bname, fh.filename);
    if (!(fout = fopen(buf, "w")))
        return 3;

    if (mode) {
        now = time(NULL);
        fprintf(fout, "发信人: "DELIVER" (自动发信系统), 信区: %s版安全记录\n", bname);
        fprintf(fout, "标  题: %s\n", fh.title);
        fprintf(fout, "发信站: %s自动发信系统 (%24.24s)\n\n", BBS_FULL_NAME, ctime_r(&now, timebuf));
    } else {
        now = time(NULL);
        fprintf(fout, "发信人: %s (%s), 信区: %s版安全记录\n", user->userid, user->username, bname);
        fprintf(fout, "标  题: %s\n", fh.title);
        fprintf(fout, "发信站: %s (%24.24s)\n\n", BBS_FULL_NAME, ctime_r(&now, timebuf));
    }
    fprintf(fout, "\033[36m版面管理安全记录\033[m\n");
    fprintf(fout, "\033[33m记录原因: \033[32m%s %s\033[m\n", (mode)?"":user->userid, fh.title);
    if (!mode)
        fprintf(fout, "\033[33m用户来源: \033[32m%s\033[m\n", getSession()->fromhost);
    if (filename != NULL) {
        if (!(fin = fopen(filename, "r"))) {
            fprintf(fout, "\n\033[45;31m系统错误，无法记录相关详细信息\033[K\033[m\n");
        } else {
            fprintf(fout, "\n\033[36m本次操作附加信息\033[m\n");
            while (true) {
                size = fread(bufcp, 1, READ_BUFFER_SIZE, fin);
                if (size == 0)
                    break;
                fwrite(bufcp, size, 1, fout);
            }
            fclose(fin);
        }
    }
    if (xfh) {
        time_t t=get_posttime(xfh);
        fprintf(fout, "\n\033[36m本次操作对应文章信息\033[m\n");
        fprintf(fout, "\033[33m文章标题: \033[4;32m%s\033[m\n", xfh->title);
        fprintf(fout, "\033[33m文章作者: \033[4;32m%s\033[m\n", xfh->owner);
        fprintf(fout, "\033[33m文章ID号: \033[4;32m%d\033[m\n", xfh->id);
        fprintf(fout, "\033[33m发表时间: \033[4;32m%24.24s\033[m\n", ctime_r(&t, timebuf));
    }
    fclose(fout);
    fh.eff_size = get_effsize_attach(buf, &fh.attachment);
    setbdir(DIR_MODE_BOARD, buf, bname);
    if ((fd = open(buf, O_WRONLY|O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) == -1) {
        return 5;
    }
    writew_lock(fd, 0, SEEK_SET, 0);
    fh.id = board_last_index_id(bname, DIR_MODE_BOARD) + 1;
    fh.groupid = fh.id;
    fh.reid = fh.id;
    if (xfh)
        fh.o_id = xfh->id;
    else
        fh.o_id = 0;
    lseek(fd, 0, SEEK_END);
    if (safewrite(fd, &fh, sizeof(struct fileheader)) == -1) {
        un_lock(fd, 0, SEEK_SET, 0);
        close(fd);
        setbfile(buf, bname, fh.filename);
        unlink(buf);
        return 6;
    }
    un_lock(fd, 0, SEEK_SET, 0);
    close(fd);
    return 0;
}
#endif

#ifdef HAVE_USERSCORE
/* 版面积分变更记录 */
int board_score_change_report(struct userec *user, const char *bname, int os, int ns, char *title, struct fileheader *xfh)
{
    struct fileheader fh;
    struct boardheader bh;
    FILE *fn;
    char buf[STRLEN], timebuf[STRLEN];
    int mode, ds, fd;
    time_t now;

    bzero(&fh, sizeof(struct fileheader));
    setbpath(buf, bname);     
    if (!getboardnum(bname, &bh)) 
        return 1;
    if (GET_POSTFILENAME(fh.filename, buf))
        return 2;
    POSTFILE_BASENAME(fh.filename)[0] = 'B';
    set_posttime(&fh);

    mode = 0;
    if (user == NULL)
        mode = 1;  //自动发信
    if (mode) 
        memcpy(fh.owner, DELIVER, OWNER_LEN);
    else
        memcpy(fh.owner, user -> userid, OWNER_LEN);
    fh.owner[OWNER_LEN - 1] = 0;

    if (xfh) {
        if (strlen(xfh->title)>40) {
            strnzhcpy(buf, xfh->title, 38);
            strcat(buf, "..");
        } else
            strcpy(buf, xfh->title);
        sprintf(fh.title, "%s <%s>", title, buf);
    } else
        strnzhcpy(fh.title, title, ARTICLE_TITLE_LEN);

    setbfile(buf, bname, fh.filename);
    if (!(fn = fopen(buf, "w")))
        return 3;

    if (mode) {
        now = time(NULL);
        fprintf(fn, "发信人: "DELIVER" (自动发信系统), 信区: %s版积分变更记录\n", bname);
        fprintf(fn, "标  题: %s\n", fh.title);
        fprintf(fn, "发信站: %s自动发信系统 (%24.24s)\n\n", BBS_FULL_NAME, ctime_r(&now, timebuf));
    } else {
        now = time(NULL);
        fprintf(fn, "发信人: %s (%s), 信区: %s版积分变更记录\n", user->userid, user->username, bname);
        fprintf(fn, "标  题: %s\n", fh.title);
        fprintf(fn, "发信站: %s (%24.24s)\n\n", BBS_FULL_NAME, ctime_r(&now, timebuf));
    }
    ds = ns - os;
    fprintf(fn, "\033[36m版面积分变更记录\033[m\n");
    fprintf(fn, "\033[33m版面积分: \033[33m%-8d\033[m -> \033[32m%8d\t\t%s%d\033[m\n", os, ns, (ds>0)?"\033[31m↑":"\033[36m↓", abs(ds));
    fprintf(fn, "\033[33m记录原因: \033[32m%s%s\033[m\n", (mode)?"":user->userid, fh.title);

    if (xfh) {
        time_t t=get_posttime(xfh); 
        fprintf(fn, "\n\033[36m本次操作对应文章信息\033[m\n");
        fprintf(fn, "\033[33m文章标题: \033[4;32m%s\033[m\n", xfh->title);
        fprintf(fn, "\033[33m文章作者: \033[4;32m%s\033[m\n", xfh->owner);
        fprintf(fn, "\033[33m文章ID号: \033[4;32m%d\033[m\n", xfh->id);
        fprintf(fn, "\033[33m发表时间: \033[4;32m%24.24s\033[m\n", ctime_r(&t, timebuf));
    } 
    fclose(fn);
    fh.eff_size = get_effsize_attach(buf, &fh.attachment);
    setbdir(DIR_MODE_SCORE, buf, bname);
    if ((fd = open(buf, O_WRONLY|O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) == -1) {
        return 5;
    }
    writew_lock(fd, 0, SEEK_SET, 0);
    fh.id = board_last_index_id(bname, DIR_MODE_SCORE) + 1;
    fh.groupid = fh.id;
    fh.reid = fh.id;
    if (xfh)
        fh.o_id = xfh->id;
    else
        fh.o_id = 0;
    lseek(fd, 0, SEEK_END);
    if (safewrite(fd, &fh, sizeof(struct fileheader)) == -1) {
        un_lock(fd, 0, SEEK_SET, 0);
        close(fd);
        setbfile(buf, bname, fh.filename);
        unlink(buf);
        return 6;
    }
    un_lock(fd, 0, SEEK_SET, 0);
    close(fd);
    return 0;
}

#ifdef NEWSMTH
#define AWARD_TYPE_BOARD 1
#define AWARD_TYPE_USER  2
/* 积分奖励系统记录 */
int score_award_report(struct boardheader *bh, struct userec *opt, struct userec *user, struct fileheader *fh, int score, int mode)
{
    char file[STRLEN], title[STRLEN], timebuf[STRLEN];
    FILE *fn;
    time_t t;

    gettmpfilename(file, "score_award_report");
    if ((fn=fopen(file, "w"))!=NULL) {
        fprintf(fn, "\033[36m积分奖励记录\033[m\n");
        fprintf(fn, "\033[33m来源版面: \033[32m%s\n", bh->filename);
        fprintf(fn, "\033[33m奖励类型: \033[32m%s%s\n", (mode==AWARD_TYPE_BOARD)?"版面":"个人", score>0?"奖励":"扣还");
        fprintf(fn, "\033[33m版面积分: \033[33m%-8d\033[m -> \033[32m%8d\t\t%s%d\033[m\n", (mode==AWARD_TYPE_BOARD)?bh->score+score:bh->score-score/5,
                bh->score, (score<0||mode==AWARD_TYPE_USER)?"\033[31m↑":"\033[36m↓", (mode==AWARD_TYPE_BOARD)?abs(score):score/5);

        fprintf(fn, "\n\033[36m奖励者基本信息\033[m\n");
        fprintf(fn, "\033[33m操作用户: \033[32m%s\n", opt->userid);
        fprintf(fn, "\033[33m登录地址: \033[32m%s\n", opt->lasthost);
        if (mode==AWARD_TYPE_USER)
            fprintf(fn, "\033[33m积分变动: \033[33m%-8d\033[m -> \033[32m%8d\t\t%s%d\033[m\n", opt->score_user+score, opt->score_user, "\033[36m↓", score);

        fprintf(fn, "\n\033[36m被奖励者基本信息\033[m\n");
        fprintf(fn, "\033[33m操作用户: \033[32m%s\n", user->userid);
        fprintf(fn, "\033[33m登录地址: \033[32m%s\n", user->lasthost);
        fprintf(fn, "\033[33m积分变动: \033[33m%-8d\033[m -> \033[32m%8d\t\t%s%d\033[m\n", (mode==AWARD_TYPE_BOARD)?user->score_user-score:user->score_user-score*4/5,
                user->score_user, (score>0)?"\033[31m↑":"\033[36m↓", (mode==AWARD_TYPE_BOARD)?abs(score):score*4/5);

        t=get_posttime(fh);
        fprintf(fn, "\n\033[36m本次操作对应文章信息\033[m\n");
        fprintf(fn, "\033[33m文章标题: \033[4;32m%s\033[m\n", fh->title);
        fprintf(fn, "\033[33m文章ID号: \033[4;32m%d\033[m\n", fh->id);
        fprintf(fn, "\033[33m发表时间: \033[4;32m%24.24s\033[m\n", ctime_r(&t, timebuf));

        fclose(fn);

        sprintf(title, "[%s%s][%s][%d][%s]", (mode==AWARD_TYPE_BOARD)?"版面":"个人", score>0?"奖励":"扣还", bh->filename, abs(score), user->userid);
        post_file(opt, "", file, "ScoreAwardLog", title, 0, 2, getSession());
        unlink(file);
    }
    return 0;
}

#endif

/* 获取文章对应的积分奖励记录文件 */
void setsfile(char *file, char *board, char *filename)
{
    char buf[STRLEN];

    strcpy(buf, filename);
    POSTFILE_BASENAME(buf)[0]='A';
    setbfile(file, board, buf);
}

/* 查询符合条件的积分奖励记录，使用apply_record回调 */
int get_award_score(struct score_award_arg *pa, int idx, struct score_award_arg *sa)
{
    if (sa->bm) {
        if (pa->bm)
            sa->score += pa->score;
    } else {
        if (!pa->bm && !strcmp(pa->userid, sa->userid))
            sa->score += pa->score;
    }
    return 0;
}

/* 最多可奖励的积分 */
int max_award_score(struct boardheader *bh, struct userec *user, struct fileheader *fh, int bm)
{
    int max;
    struct score_award_arg sa;
    char file[STRLEN];

    max = bm?MAX_BOARD_AWARD_SCORE:MAX_USER_AWARD_SCORE;

    bzero(&sa, sizeof(struct score_award_arg));
    strcpy(sa.userid, user->userid);
    sa.bm = bm;

    setsfile(file, bh->filename, fh->filename);
    apply_record(file, (APPLY_FUNC_ARG)get_award_score, sizeof(struct score_award_arg), &sa, 0, 0);
    max = max - sa.score;

    return max;
}

/* 查询积分奖励总数，使用apply_record回调 */
int get_all_award_score(struct score_award_arg *pa, int idx, struct score_award_arg *sa)
{
    if (sa->bm) {
        if (pa->bm)
            sa->score += pa->score;
    } else
        sa->score += pa->score;
    return 0;
}

/* 获得积分奖励总数 */
int all_award_score(struct boardheader *bh, struct fileheader *fh, int bm)
{
    struct score_award_arg sa;
    char file[STRLEN];

    bzero(&sa, sizeof(struct score_award_arg));
    sa.bm = bm;
    setsfile(file, bh->filename, fh->filename);
    apply_record(file, (APPLY_FUNC_ARG)get_all_award_score, sizeof(struct score_award_arg), &sa, 0, 0);

    return sa.score;
}

/* 版面积分奖励/扣除记录 */
int add_award_record(struct boardheader *bh, struct userec *opt, struct fileheader *fh, int score, int bm)
{
    struct score_award_arg sa;
    char file[STRLEN];

    bzero(&sa, sizeof(struct score_award_arg));
    setsfile(file, bh->filename, fh->filename);
    strcpy(sa.userid, opt->userid);
    sa.score = score;
    sa.t = time(0);
    sa.bm = bm;

    append_record(file, &sa, sizeof(struct score_award_arg));
    return 0;
}

/* 检查能否发积分 */
int can_award_score(struct userec *user, int bm)
{
    unsigned int lvl=user->userlevel;
    if (!(lvl & (PERM_CHAT)) && !(lvl & (PERM_PAGE)) && !(lvl & (PERM_POST)) && !(lvl &(PERM_LOGINOK)))
        return 0;
    if (user->score_user<2000 && !bm)
        return 0;
    return 1;
}

void bcache_setreadonly(int readonly);

/* 奖励用户个人积分 */
int award_score_from_user(struct boardheader *bh, struct userec *from, struct userec *user, struct fileheader *fh, int score)
{
    char buf[STRLEN*2];
    int tmp;

    if ((int)(from->score_user) < score)
        return -1;

    tmp = AO_int_fetch_and_add(&(from->score_user), -score);
    tmp = AO_int_fetch_and_add(&(user->score_user), score * 8 / 10);
    bcache_setreadonly(0);
    tmp = AO_int_fetch_and_add(&(bh->score), score / 5);
    bcache_setreadonly(1);

    sprintf(buf, "%s 版 %s 奖励个人积分 <%s>", bh->filename, from->userid, fh->title);
    score_change_mail(user, user->score_user-score*8/10, user->score_user, 0, 0, buf);

    sprintf(buf, "奖励 %s 版 %s 积分 <%s>", bh->filename, user->userid, fh->title);
    score_change_mail(from, from->score_user+score, from->score_user, 0, 0, buf);

    sprintf(buf, "奖励%s %d用户积分", user->userid, score);
    board_score_change_report(from, bh->filename, bh->score-score/5, bh->score, buf, fh);

    add_award_record(bh, from, fh, score, 0);

#ifdef NEWSMTH
    score_award_report(bh, from, user, fh, score, AWARD_TYPE_USER);
#endif
    return 0;
}

/* 奖励用户版面积分 */
int award_score_from_board(struct boardheader *bh, struct userec *opt, struct userec *user, struct fileheader *fh, int score)
{
    char buf[STRLEN*2];
    int insufficient=0, os=score, tmp;

    if ((int)(bh->score) < score)
        return -1;

    if ((int)user->score_user + score < 0) {
        score = -((int)user->score_user);
        insufficient = 1;
    }

    tmp = AO_int_fetch_and_add(&(user->score_user), score);
    bcache_setreadonly(0);
    tmp = AO_int_fetch_and_add(&(bh->score), -score);
    bcache_setreadonly(1);

    sprintf(buf, "%s 版 %s %s版面积分 <%s>", bh->filename, opt->userid, score>0?"奖励":"扣还", fh->title);
    score_change_mail(user, user->score_user-score, user->score_user, 0, 0, buf);

#ifdef BOARD_SECURITY_LOG
    char tmpfile[STRLEN];
    FILE *fn;
    gettmpfilename(tmpfile, "award_score");
    if ((fn = fopen(tmpfile, "w"))!=NULL) {
        fprintf(fn, "\033[33m版面积分: %8d\033[m -> \033[32m%-8d\t\t%s%d\033[m\n", bh->score+score, bh->score, score<0?"\033[31m↑":"\033[36m↓", abs(score));
        if (insufficient) {
            fprintf(fn, "  输入积分: \033[32m%d\033[m\n", os);
            fprintf(fn, "  扣还积分: \033[31m%d\033[m\n", abs(score));
        }
        fclose(fn);
    }
    sprintf(buf, "%s%d积分", score>0?"奖励":"扣还", abs(score));
    board_security_report(tmpfile, opt, buf, bh->filename, fh);
    unlink(tmpfile);
#endif

    sprintf(buf, "%s%s %d版面积分", score>0?"奖励":"扣还", user->userid, abs(score));
    board_score_change_report(opt, bh->filename, bh->score+score, bh->score, buf, fh);

    add_award_record(bh, opt, fh, score, 1);
#ifdef NEWSMTH
    score_award_report(bh, opt, user, fh, score, AWARD_TYPE_BOARD);
#endif
    return 0;
}

int add_award_mark(struct boardheader *bh, struct fileheader *fh)
{
    FILE *fin, *fout;
    char buf[256], fname[STRLEN], outfile[STRLEN];
    int score, bscore, asize, added=0;

    setbfile(fname, bh->filename, fh->filename);
    if ((fin = fopen(fname, "rb")) == NULL)
        return 0;
    gettmpfilename(outfile, "awardmark", getpid());
    if ((fout = fopen(outfile, "w")) == NULL) {
        fclose(fin);
        return 0;
    }
    while ((asize = -attach_fgets(buf, 256, fin)) != 0) {
        if (asize<0) {
            if (!added && !strncmp(buf, "发信站: ", 8)) {
                char *p;
                if ((p=strstr(buf, "  \033[1;31m[累计积分奖励:"))!=NULL)
                    *p = '\0';
                else if (buf[strlen(buf)-1] == '\n')
                    buf[strlen(buf)-1] = '\0';
                score = all_award_score(bh, fh, 0);
                bscore = all_award_score(bh, fh, 1);
                fprintf(fout, "%s  \033[1;31m[累计积分奖励: %d/%d]\033[m\n", buf, bscore, score-bscore);
                added = 1;
            } else
                fputs(buf, fout);
        } else
            put_attach(fin, fout, asize);
    }

    fclose(fin);
    fclose(fout);

    f_cp(outfile, fname, O_TRUNC);
    unlink(outfile);
    
    return 1;
}
#endif
