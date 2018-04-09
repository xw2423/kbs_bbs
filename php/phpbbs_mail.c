#include "php_kbs_bbs.h"

#ifdef ENABLE_REFER
void bbs_make_refer_array(zval *array, struct refer *refer) {
    add_assoc_string(array, "BOARD", refer->board, 1);
    add_assoc_string(array, "USER", refer->user, 1);
    add_assoc_string(array, "TITLE", refer->title, 1);
    add_assoc_long(array, "ID", refer->id);
    add_assoc_long(array, "GROUP_ID", refer->groupid);
    add_assoc_long(array, "RE_ID", refer->reid);
    add_assoc_long(array, "FLAG", refer->flag);
    add_assoc_long(array, "TIME", refer->time);
}
#endif

#ifdef ENABLE_NEW_MSG
void bbs_makr_new_msg_user_array(zval *array, struct new_msg_user *msg) {
	add_assoc_long(array, "ID", msg->id);
	add_assoc_long(array, "MSG_ID", msg->msg_id);
	add_assoc_string(array, "NAME", msg->name, 1);
	add_assoc_long(array, "COUNT", msg->count);
	add_assoc_long(array, "TIME", (&(msg->msg))->time);
	add_assoc_long(array, "HOST", (&(msg->msg))->host);
	add_assoc_long(array, "SIZE", (&(msg->msg))->size);
	add_assoc_long(array, "FLAG", (&(msg->msg))->flag);
	add_assoc_string(array, "KEY", (&(msg->msg))->key , 1);
	add_assoc_string(array, "USER", (&(msg->msg))->user , 1);
	add_assoc_string(array, "FROM", (&(msg->msg))->from , 1);
	add_assoc_string(array, "MSG", (&(msg->msg))->msg , 1);
	add_assoc_long(array, "ATTACHMENT_SIZE", (&(msg->attachment))->size);
	add_assoc_string(array, "ATTACHMENT_NAME", (&(msg->attachment))->name, 1);
	add_assoc_string(array, "ATTACHMENT_TYPE", (&(msg->attachment))->type, 1);
}
void bbs_make_new_msg_message_array(zval *array, struct new_msg_message *msg) {
    add_assoc_long(array, "ID", msg->id);
	add_assoc_long(array, "TIME", (&(msg->msg))->time);
	add_assoc_long(array, "HOST", (&(msg->msg))->host);
	add_assoc_long(array, "SIZE", (&(msg->msg))->size);
	add_assoc_long(array, "FLAG", (&(msg->msg))->flag);
	add_assoc_string(array, "KEY", (&(msg->msg))->key , 1);
	add_assoc_string(array, "USER", (&(msg->msg))->user , 1);
	add_assoc_string(array, "FROM", (&(msg->msg))->from , 1);
	add_assoc_string(array, "MSG", (&(msg->msg))->msg , 1);
	add_assoc_long(array, "ATTACHMENT_SIZE", (&(msg->attachment))->size);
	add_assoc_string(array, "ATTACHMENT_NAME", (&(msg->attachment))->name, 1);
	add_assoc_string(array, "ATTACHMENT_TYPE", (&(msg->attachment))->type, 1);
}
#endif

PHP_FUNCTION(bbs_checknewmail)
{
    char *userid;
    int userid_len;
    char qry_mail_dir[STRLEN];

    if (zend_parse_parameters(1 TSRMLS_CC, "s", &userid, &userid_len) != SUCCESS) {
        WRONG_PARAM_COUNT;
    }

    if (userid_len > IDLEN)
        userid[IDLEN]=0;

    setmailfile(qry_mail_dir, userid, DOT_DIR);

    RETURN_LONG(check_query_mail(qry_mail_dir, NULL));

}

PHP_FUNCTION(bbs_mail_get_limit)
{
    char *userid;
    int userid_len, space, num;
    struct userec *user;

    if (zend_parse_parameters(1 TSRMLS_CC, "s", &userid, &userid_len) != SUCCESS) {
        WRONG_PARAM_COUNT;
    }
    if (userid_len > IDLEN)
        RETURN_FALSE;
    if (!getuser(userid, &user))
        RETURN_FALSE;
    get_mail_limit(user, &space, &num);
    if (array_init(return_value) == FAILURE) {
        RETURN_FALSE;
    }
    add_assoc_long(return_value, "space", space);
    add_assoc_long(return_value, "num", num);
}

PHP_FUNCTION(bbs_mail_get_num)
{
    char *userid;
    int userid_len, total, newmail, full;
    char qry_mail_dir[STRLEN];

    if (zend_parse_parameters(1 TSRMLS_CC, "s", &userid, &userid_len) != SUCCESS) {
        WRONG_PARAM_COUNT;
    }

    if (userid_len > IDLEN)
        userid[IDLEN]=0;

    setmailfile(qry_mail_dir, userid, DOT_DIR);

    newmail = check_query_mail(qry_mail_dir, &total);
    full = (check_mail_perm(getCurrentUser(), NULL)==2) ? 1 : 0;
    if (array_init(return_value) == FAILURE) {
        RETURN_FALSE;
    }

    add_assoc_long(return_value, "total", total);
    add_assoc_bool(return_value, "newmail", newmail);
    add_assoc_bool(return_value, "full", full);
}

/**
 * get the number of one user's mail.
 * prototype:
 * bool bbs_getmailnum(string userid,long &total,long &unread);
 *
 * @return TRUE on success,
 *       FALSE on failure.
 *       and return total and unread in argument
 * @author KCN
 */
PHP_FUNCTION(bbs_getmailnum)
{
    zval *total, *unread;
    char *userid;
    int userid_len;
    struct fileheader x;
    char path[80];
    int totalcount = 0, unreadcount = 0;
    int ac = ZEND_NUM_ARGS();
    int fd;
    long oldtotal,oldunread;

    if (ac != 5 || zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "szzll", &userid, &userid_len, &total, &unread, &oldtotal, &oldunread) == FAILURE) {
        WRONG_PARAM_COUNT;
    }

    if (userid_len > IDLEN)
        WRONG_PARAM_COUNT;

    /*
     * check for parameter being passed by reference
     */
    if (!PZVAL_IS_REF(total) || !PZVAL_IS_REF(unread)) {
        zend_error(E_WARNING, "Parameter wasn't passed by reference");
        RETURN_FALSE;
    }

    if (!strcmp(userid, getCurrentUser()->userid) && oldtotal && getSession()->currentuinfo && !(getSession()->currentuinfo->mailcheck & CHECK_MAIL)) {
        totalcount = oldtotal;
        unreadcount = oldunread;
        ZVAL_LONG(total, totalcount);
        ZVAL_LONG(unread, unreadcount);
        RETURN_TRUE;
    }

    setmailfile(path, userid, DOT_DIR);
    fd = open(path, O_RDONLY);
    if (fd == -1)
        RETURN_FALSE;
    while (read(fd, &x, sizeof(x)) > 0) {
        totalcount++;
        if (!(x.accessed[0] & FILE_READ))
            unreadcount++;
    }
    close(fd);
    /*
     * make changes to the parameter
     */
    ZVAL_LONG(total, totalcount);
    ZVAL_LONG(unread, unreadcount);
    if (getSession()->currentuinfo)
        getSession()->currentuinfo->mailcheck |= CHECK_MAIL;
    RETURN_TRUE;
}

/**
 * get the number of one user's mail path.
 * prototype:
 * int bbs_getmailnum2(string path);
 *
 * @return the number
 * @author binxun
 */
PHP_FUNCTION(bbs_getmailnum2)
{
    char *path;
    int path_len;

    int ac = ZEND_NUM_ARGS();

    if (ac != 1 || zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "s", &path, &path_len) == FAILURE) {
        WRONG_PARAM_COUNT;
    }

    RETURN_LONG(getmailnum(path));
}


/**
 * Fetch a list of mails in one user's mail path file into an array.
 * prototype:
 * array bbs_getmails(char *filename,int start,int num);
 *
 * start - 0 based
 * @return array of loaded mails on success,
 *         -1  no mail
 *         FALSE on failure.
 * @author binxun
 */
PHP_FUNCTION(bbs_getmails)
{
    char *mailpath;
    int mailpath_len;
    int total, rows, i;
    long start,num;

    struct fileheader *mails;
    zval *element;
    char flags[2];              /* flags[0]: status
                                 * flags[1]: reply status
                                 */
    int ac = ZEND_NUM_ARGS();

    /*
     * getting arguments
     */
    if (ac != 3 || zend_parse_parameters(3 TSRMLS_CC, "sll", &mailpath, &mailpath_len,&start,&num) == FAILURE) {
        WRONG_PARAM_COUNT;
    }

    total = getmailnum(mailpath);
    if (!total)
        RETURN_LONG(-1);

    if (array_init(return_value) == FAILURE) {
        RETURN_FALSE;
    }

    if (start >= total)RETURN_FALSE;
    if (start + num > total)num = total - start;

    mails = emalloc(num * sizeof(struct fileheader));
    if (!mails)
        RETURN_FALSE;
    rows = get_records(mailpath, mails, sizeof(struct fileheader), start+1, num);//it is 1 -based
    if (rows == -1)
        RETURN_FALSE;
    for (i = 0; i < rows; i++) {
        MAKE_STD_ZVAL(element);
        array_init(element);
        if (mails[i].accessed[0] & FILE_READ) {
            if (mails[i].accessed[0] & FILE_MARKED)
                flags[0] = 'm';
            else
                flags[0] = ' ';
        } else {
            if (mails[i].accessed[0] & FILE_MARKED)
                flags[0] = 'M';
            else
                flags[0] = 'N';
        }
        if (mails[i].accessed[0] & FILE_REPLIED) {
            if (mails[i].accessed[0] & FILE_FORWARDED)
                flags[1] = 'A';
            else
                flags[1] = 'R';
        } else {
            if (mails[i].accessed[0] & FILE_FORWARDED)
                flags[1] = 'F';
            else
                flags[1] = ' ';
        }
        bbs_make_article_array(element, mails + i, flags, sizeof(flags));
        zend_hash_index_update(Z_ARRVAL_P(return_value), i, (void *) &element, sizeof(zval *), NULL);
    }
    efree(mails);

    if (getSession()->currentuinfo)
        getSession()->currentuinfo->mailcheck &= ~CHECK_MAIL;
}


/**
 * Get mail used space
 * @author stiger
 */
PHP_FUNCTION(bbs_getmailusedspace)
{
    RETURN_LONG(get_mailusedspace(getCurrentUser(),1)/1024);
}

/**
 * Whether save to sent box
 * @author atppp
 */
PHP_FUNCTION(bbs_is_save2sent)
{
    RETURN_LONG(HAS_MAILBOX_PROP(getSession()->currentuinfo, MBP_SAVESENTMAIL));
}




/*
 * bbs_can_send_mail ()
 * @author stiger
 */
PHP_FUNCTION(bbs_can_send_mail)
{
    long is_reply = 0;
    int ret;
    int ac = ZEND_NUM_ARGS();
    if (ac == 1) {
        if (zend_parse_parameters(1 TSRMLS_CC, "l", &is_reply) == FAILURE) {
            WRONG_PARAM_COUNT;
        }
    } else {
        if (ac != 0) {
            WRONG_PARAM_COUNT;
        }
    }
    ret = check_mail_perm(getCurrentUser(), NULL);
    if (ret > 0) {
        if (is_reply) {
            RETURN_LONG(ret == 6 ? 1 : 0);
        } else {
            RETURN_LONG(0);
        }
    } else {
        RETURN_LONG(1);
    }
}


/*
 * bbs_sufficient_score_sendmail()
 * @author jiangjun
 * 发送信件积分要求
 */
PHP_FUNCTION(bbs_sufficient_score_to_sendmail)
{
    char *userid;
    long ulen;
    int ac = ZEND_NUM_ARGS();

#ifdef HAVE_USERSCORE
    int ret;
    if (ac!=1 || zend_parse_parameters(1 TSRMLS_CC, "s", &userid, &ulen)==FAILURE) {
        WRONG_PARAM_COUNT;
    }
    if (strchr(userid, '@'))
        ret = sufficient_score_to_sendmail(getCurrentUser(), NULL);
    else
        ret = sufficient_score_to_sendmail(getCurrentUser(), userid);
    RETURN_LONG(ret);
#else
    RETURN_LONG(1);
#endif
}

/**
 * load mail list. user custom mailboxs.
 * prototype:
 * array bbs_loadmaillist(char *userid);
 *
 * @return array of loaded mails on success,
 *         -1 no mailbox
 *         FALSE on failure.
 * @author binxun
 */
PHP_FUNCTION(bbs_loadmaillist)
{
    char *userid;
    int userid_len;
    char buf[10];
    struct _mail_list maillist;

    struct userec *user;
    int i;
    zval *element;

    int ac = ZEND_NUM_ARGS();

    /*
     * getting arguments
     */
    if (ac != 1 || zend_parse_parameters(1 TSRMLS_CC, "s", &userid, &userid_len) == FAILURE) {
        WRONG_PARAM_COUNT;
    }
    if (userid_len > IDLEN)
        RETURN_FALSE;

    if (!getuser(userid, &user))
        RETURN_FALSE;
    load_mail_list(user, &maillist);

    if (maillist.mail_list_t < 0 || maillist.mail_list_t > MAILBOARDNUM) {      //no custom mail box
        RETURN_FALSE;
    }

    if (!maillist.mail_list_t)
        RETURN_LONG(-1);

    if (array_init(return_value) == FAILURE) {
        RETURN_FALSE;
    }

    for (i = 0; i < maillist.mail_list_t; i++) {
        MAKE_STD_ZVAL(element);
        array_init(element);
        sprintf(buf, ".%s", maillist.mail_list[i] + 30);
        //assign_maillist(element,maillist.mail_list[i],buf);
        add_assoc_string(element, "boxname", maillist.mail_list[i], 1);
        add_assoc_string(element, "pathname", buf, 1);
        zend_hash_index_update(Z_ARRVAL_P(return_value), i, (void *) &element, sizeof(zval *), NULL);

    }
}

/**
 * change mail list and save new for user custom mailboxs.
 * prototype:
 * int bbs_changemaillist(bool bAdd,char* userid,char* newboxname,int index); index--0 based
 *
 * @return
 *         0 ---- fail
 *         -1 ---- success
 *         >0 --- reach to max number!
 * @author binxun
 */
PHP_FUNCTION(bbs_changemaillist)
{
    char *boxname;
    int boxname_len;
    char *userid;
    int userid_len;
    zend_bool bAdd;
    long index;

    struct _mail_list maillist;
    char buf[10], path[PATHLEN];

    struct userec *user;
    int i;
    struct stat st;

    int ac = ZEND_NUM_ARGS();

    /*
     * getting arguments
     */
    if (ac != 4 || zend_parse_parameters(4 TSRMLS_CC, "bssl", &bAdd, &userid, &userid_len, &boxname, &boxname_len, &index) == FAILURE) {
        WRONG_PARAM_COUNT;
    }
    if (userid_len > IDLEN)
        RETURN_LONG(0);
    if (boxname_len > 29)
        boxname[29] = '\0';

    if (!getuser(userid, &user))
        RETURN_LONG(0);
    load_mail_list(user, &maillist);

    if (maillist.mail_list_t < 0 || maillist.mail_list_t > MAILBOARDNUM) {      //no custom mail box
        RETURN_LONG(0);
    }

    if (bAdd) {                 //add
        if (maillist.mail_list_t == MAILBOARDNUM)
            RETURN_LONG(MAILBOARDNUM);  //最大值了
        i = 0;
        while (1) {             //search for new mailbox path name
            i++;
            sprintf(buf, ".MAILBOX%d", i);
            setmailfile(path, getCurrentUser()->userid, buf);
            if (stat(path, &st) == -1)
                break;
        }
        f_touch(path);
        sprintf(buf, "MAILBOX%d", i);
        strcpy(maillist.mail_list[maillist.mail_list_t], boxname);
        strcpy(maillist.mail_list[maillist.mail_list_t] + 30, buf);
        maillist.mail_list_t += 1;
        save_mail_list(&maillist, getSession());
    } else {                    //delete
        if (index < 0 || index > maillist.mail_list_t - 1)
            RETURN_LONG(-1);
        sprintf(buf, ".%s", maillist.mail_list[index] + 30);
        setmailfile(path, getCurrentUser()->userid, buf);
        if (get_num_records(path, sizeof(struct fileheader)) != 0)
            RETURN_LONG(0);
        f_rm(path);
        for (i = index; i < maillist.mail_list_t - 1; i++)
            memcpy(maillist.mail_list[i], maillist.mail_list[i + 1], sizeof(maillist.mail_list[i]));
        maillist.mail_list_t--;
        save_mail_list(&maillist, getSession());
    }
    if (getSession()->currentuinfo)
        getSession()->currentuinfo->mailcheck &= ~CHECK_MAIL;
    RETURN_LONG(-1);
}





/**
 * Function: post a new mail
 *  rototype:
 * int bbs_postmail(string targetid,string title,string content,long sig, long backup);
 *
 *  @return the result
 *   0 -- success
 *  -1   index file failed to open
 *      -2   file/dir creation failed
 *      -3   receiver refuses
 *      -4   receiver reaches mail limit
 *      -5   send too frequently
 *      -6   receiver index append failed
 *      -7   sender index append failed
 *      -8   invalid renum
 *      -9   sender no permission
 *      -100 invalid user
 *  @author roy
 */
PHP_FUNCTION(bbs_postmail)
{
    char *recvID, *title, *content;
    char targetID[IDLEN+1];
    int  idLen, tLen,cLen;
    long backup,sig,renum;
    int ac = ZEND_NUM_ARGS();
    char mail_title[80];
    FILE *fp;
    char fname[PATHLEN], filepath[PATHLEN], sent_filepath[PATHLEN];
    struct fileheader header;
    struct stat st;
    struct userec *touser;      /*peregrine for updating used space */
    char *refname,*dirfname;
    int find=-1,fhcount=0,refname_len,dirfname_len,ret;

    if (ac == 5) { /* use this to send a new mail */
        if (zend_parse_parameters(5 TSRMLS_CC, "ss/s/ll", &recvID, &idLen,&title,&tLen,&content,&cLen,&sig,&backup) == FAILURE) {
            WRONG_PARAM_COUNT;
        }
        strncpy(targetID, recvID, sizeof(targetID));
        targetID[sizeof(targetID)-1] = '\0';
    } else if (ac == 7) { /* use this to reply a mail */
        if (zend_parse_parameters(7 TSRMLS_CC, "ssls/s/ll", &dirfname, &dirfname_len, &refname, &refname_len, &renum, &title, &tLen, &content, &cLen, &sig, &backup) == FAILURE) {
            WRONG_PARAM_COUNT;
        }
    } else {
        WRONG_PARAM_COUNT;
    }

    if (check_last_post_time(getSession()->currentuinfo)) {
        RETURN_LONG(-5); // 两次发文间隔过密, 请休息几秒后再试
    }

    /* read receiver's id from mail when replying, by pig2532 */
    if (ac == 7) {
        if (stat(dirfname, &st)==-1) {
            RETURN_LONG(-1);    /* error reading stat */
        }
        if ((renum<0)||(renum>=(st.st_size/sizeof(fileheader)))) {
            RETURN_LONG(-8);    /* no such mail to reply */
        }
        if ((fp = fopen(dirfname, "r+")) == NULL) {
            RETURN_LONG(-1);  /* error openning .DIR */
        }
        fseek(fp, sizeof(header) * renum, SEEK_SET);
        if (fread(&header, sizeof(header), 1, fp) > 0) { /* read fileheader by renum */
            if (strcmp(header.filename, refname) == 0) {
                find = renum;
            }
        }
        if (find == -1) {
            rewind(fp);
            while (true) { /* find the fileheader */
                if (fread(&header, sizeof(header), 1, fp) <= 0) {
                    break;
                }
                if (strcmp(header.filename, refname) == 0) {
                    find = fhcount;
                    break;
                }
                fhcount++;
            }
        }
        if (find == -1) { /* file not found */
            fclose(fp);
            RETURN_LONG(-8);
        } else {
            strncpy(targetID, header.owner, sizeof(targetID));
            targetID[sizeof(targetID)-1] = '\0';
            if (!(header.accessed[0] & FILE_REPLIED)) { /* set the replied flag */
                header.accessed[0] |= FILE_REPLIED;
                fseek(fp, sizeof(header) * find, SEEK_SET);
                fwrite(&header, sizeof(header), 1, fp);
            }
            fclose(fp);
        }
    }

    getuser(targetID, &touser);
    if (touser == NULL)
        RETURN_LONG(-100);//can't find user

    ret = check_mail_perm(getCurrentUser(), touser);
    switch (ret) {
        case 1:
            RETURN_LONG(-4);
            break;
        case 2:
            RETURN_LONG(-9);
            break;
        case 3:
            RETURN_LONG(-4);
            break;
        case 4:
            RETURN_LONG(-3);
            break;
        case 5:
            RETURN_LONG(-9);
            break;
        case 6:
            if (ac != 7) {
                RETURN_LONG(-9);
            }
            break;
        default:
            if (ret > 0) {
                RETURN_LONG(-10);
            }
    }

    strncpy(targetID, touser->userid, sizeof(targetID));
    targetID[sizeof(targetID)-1] = '\0';
    process_control_chars(title,NULL);
    if (title[0] == 0)
        strcpy(mail_title,"没主题");
    else
        strncpy(mail_title,title,79);
    mail_title[79]=0;

    bzero(&header, sizeof(header));
    strcpy(header.owner, getCurrentUser()->userid);
    strnzhcpy(header.title, mail_title, ARTICLE_TITLE_LEN);
    setmailpath(filepath, targetID);
    if (stat(filepath, &st) == -1) {
        if (mkdir(filepath, 0755) == -1)
            RETURN_LONG(-2);
    } else {
        if (!(st.st_mode & S_IFDIR))
            RETURN_LONG(-2);
    }
    if (GET_MAILFILENAME(fname, filepath) < 0)
        RETURN_LONG(-2);
    strcpy(header.filename, fname);
    setmailfile(filepath, targetID, fname);

    fp = fopen(filepath, "w");
    if (fp == NULL)
        RETURN_LONG(-2);
    write_header(fp, getCurrentUser(), 1, NULL, mail_title, 0, 0, getSession());
    if (cLen>0) {
        f_append(fp, unix_string(content));
    }
    getCurrentUser()->signature = sig;
    if (sig < 0) {
        struct userdata ud;
        read_userdata(getCurrentUser()->userid, &ud);
        if (ud.signum > 0) {
            sig = 1 + (int)(((double)ud.signum) * rand() / (RAND_MAX + 1.0));  //(rand() % ud.signum) + 1;
        } else sig = 0;
    }
    addsignature(fp, getCurrentUser(), sig);
    fputc('\n', fp);
    fclose(fp);

    if (stat(filepath, &st) != -1)
        header.eff_size = st.st_size;
    setmailfile(fname, targetID, ".DIR");
    if (append_record(fname, &header, sizeof(header)) == -1)
        RETURN_LONG(-6);
    touser->usedspace += header.eff_size;
    setmailcheck(targetID);

    /* 添加Log Bigman: 2003.4.7 */
    newbbslog(BBSLOG_USER, "mailed(www) %s %s", targetID, mail_title);

    if (backup) {
        strcpy(header.owner, targetID);
        setmailpath(sent_filepath, getCurrentUser()->userid);
        if (GET_MAILFILENAME(fname, sent_filepath) < 0) {
            RETURN_LONG(-7);
        }
        strcpy(header.filename, fname);
        setmailfile(sent_filepath, getCurrentUser()->userid, fname);

        f_cp(filepath, sent_filepath, 0);
        if (stat(sent_filepath, &st) != -1) {
            getCurrentUser()->usedspace += st.st_size;
            header.eff_size = st.st_size;
        } else {
            RETURN_LONG(-7);
        }
        header.accessed[0] |= FILE_READ;
        setmailfile(fname, getCurrentUser()->userid, ".SENT");
        if (append_record(fname, &header, sizeof(header)) == -1)
            RETURN_LONG(-7);
        newbbslog(BBSLOG_USER, "mailed(www) %s ", getCurrentUser()->userid);
    }
    RETURN_LONG(0);
}


/**
 * mail a file from a user to another user.
 * prototype:
 * string bbs_mail_file(string srcid, string filename, string destid,
 *                        string title, int is_move)
 *
 * @return TRUE on success,
 *       FALSE on failure.
 * @author flyriver
 */
PHP_FUNCTION(bbs_mail_file)
{
    char *srcid;
    int srcid_len;
    char *filename;
    int filename_len;
    char *destid;
    int destid_len;
    char *title;
    int title_len;
    long is_move;
    int ac = ZEND_NUM_ARGS();

    if (ac != 5 || zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "ssssl", &srcid, &srcid_len, &filename, &filename_len, &destid, &destid_len, &title, &title_len, &is_move) == FAILURE) {
        WRONG_PARAM_COUNT;
    }
    if (mail_file(srcid, filename, destid, title, is_move, NULL) < 0)
        RETURN_FALSE;
    RETURN_TRUE;
}


/**
 * del mail
 * prototype:
 * int bbs_delmail(char* path,char* filename);
 *
 *  @return the result
 *   0 -- success, -1 -- mail don't exist
 *   -2 -- wrong parameter
 *  @author binxun
 */
PHP_FUNCTION(bbs_delmail)
{
    FILE *fp;
    struct fileheader f;
    struct userec *u = NULL;
    char dir[80];
    long result = 0;

    char* path;
    char* filename;
    int path_len,filename_len;
    int num = 0;

    int ac = ZEND_NUM_ARGS();

    if (ac != 2 || zend_parse_parameters(2 TSRMLS_CC, "ss", &path, &path_len,&filename,&filename_len) == FAILURE) {
        WRONG_PARAM_COUNT;
    }
    if (strncmp(filename, "M.", 2) || strstr(filename, ".."))
        RETURN_LONG(-2);

    u = getCurrentUser();

    sprintf(dir, "mail/%c/%s/%s", toupper(u->userid[0]),u->userid,path);
    fp = fopen(dir, "r");
    if (fp == 0)
        RETURN_LONG(-2);

    while (1) {
        if (fread(&f, sizeof(struct fileheader), 1, fp) <= 0)
            break;
        if (!strcmp(f.filename, filename)) {
            del_mail(num + 1, &f, dir);
            break;
        }
        num++;
    }
    fclose(fp);

    RETURN_LONG(result);
}


/*
 * set a mail had readed
 */
PHP_FUNCTION(bbs_setmailreaded)
{
    int ac = ZEND_NUM_ARGS();
    long num;
    char * dirname;
    int dirname_len;
    int total;
    struct fileheader fh;
    FILE *fp;

    if (ac != 2 || zend_parse_parameters(2 TSRMLS_CC, "sl", &dirname, &dirname_len, &num) == FAILURE) {
        WRONG_PARAM_COUNT;
    }

    total = get_num_records(dirname, sizeof(fh));

    if (total <= 0)
        RETURN_LONG(0);

    if (getSession()->currentuinfo)
        setmailcheck(getSession()->currentuinfo->userid);

    if (num >=0 && num < total) {
        if ((fp=fopen(dirname,"r+"))==NULL)
            RETURN_LONG(0);
        fseek(fp,sizeof(fh) * num,SEEK_SET);
        if (fread(&fh,sizeof(fh),1,fp) > 0) {
            if (fh.accessed[0] & FILE_READ) {
                fclose(fp);
                RETURN_LONG(0);
            } else {
                fh.accessed[0] |= FILE_READ;
                fseek(fp,sizeof(fh)*num,SEEK_SET);
                fwrite(&fh,sizeof(fh),1,fp);
                fclose(fp);
                RETURN_LONG(1);
            }
        }
        fclose(fp);
    }
    RETURN_LONG(0);
}


PHP_FUNCTION(bbs_domailforward)
{
    char *fname, *tit, *target1;
    char target[128];
    int ret,filename_len,tit_len,target_len;
    long big5,noansi;
    char title[512];
    struct userec *u;
    char mail_domain[STRLEN];
    char tmpfile[STRLEN];

    if (ZEND_NUM_ARGS() != 5 || zend_parse_parameters(5 TSRMLS_CC, "sssll", &fname, &filename_len, &tit, &tit_len, &target1, &target_len, &big5, &noansi) != SUCCESS) {
        WRONG_PARAM_COUNT;
    }

    strncpy(target, target1, 128);
    target[127]=0;

    if (target[0] == 0) {
        RETURN_ERROR(USER_NONEXIST);
    }


    snprintf(mail_domain, sizeof(mail_domain), "@%s", MAIL_BBSDOMAIN);
    if (strstr(target, mail_domain))
        strcpy(target, getCurrentUser()->userid);
    if (!strchr(target, '@')) {
        if (getuser(target,&u) == 0) {
            RETURN_ERROR(USER_NONEXIST);
        }
        ret = check_mail_perm(getCurrentUser(), u);
        if (ret) {
            RETURN_LONG(-ret);
        }
        big5=0;
        noansi=0;
    }

    if (!file_exist(fname))
        RETURN_LONG(-7);
    gettmpfilename(tmpfile, "mail.forward");
    f_cp(fname, tmpfile, 0);
    strcpy(fname, tmpfile);
    /* 添加转寄信头 */
    write_forward_header(fname, tit, NULL, DIR_MODE_MAIL);

    snprintf(title, 511, "%.50s(转寄)", tit);

    if (!strchr(target, '@')) {
        mail_file(getCurrentUser()->userid, fname, u->userid, title,0, NULL);
        unlink(fname);
        RETURN_LONG(1);
    } else {
        if (big5 == 1)
            conv_init(getSession());
        if (bbs_sendmail(fname, title, target, big5, noansi, getSession()) == 0) {
            unlink(fname);
            RETURN_LONG(1);
        } else {
            unlink(fname);
            RETURN_LONG(-10);
        }
    }
}

PHP_FUNCTION(bbs_sendmail)
{
    char *fromid;
    int fromid_len;
    char *fname;
    int fname_len;
    char *title;
    int title_len;
    char *receiver;
    int receiver_len;
    long isbig5;
    long noansi;

    int ret;

    if (ZEND_NUM_ARGS() != 6 || zend_parse_parameters(6 TSRMLS_CC, "ssssll", &fromid, &fromid_len, &fname, &fname_len, &title, &title_len, &receiver, &receiver_len, &isbig5, &noansi) != SUCCESS) {
        WRONG_PARAM_COUNT;
    }
    struct userec *user, *saveuser;
    if (!getuser(fromid, &user)) {
        getuser("SYSOP", &user);
    }
    saveuser = getCurrentUser();
    setCurrentUser(user);
    ret = bbs_sendmail(fname, title, receiver, isbig5, noansi, getSession());
    setCurrentUser(saveuser);
    RETURN_LONG(ret);
}

/**
 * get the number of user's refer
 * prototype:
 * bool bbs_get_refer(string userid, long mode, long &total_num, long &new_num)
 *
 * @param string userid
 * @param long mode:
 *         1: refer
 *         2: reply
 *         others: invalid mode
 *
 * @return TRUE on success
 *         FALSE on failure.
 *         and return total and new numbers in arguments.
 *
 * @author: windinsn, Jan 29, 2012
 */
PHP_FUNCTION(bbs_get_refer) {
#ifdef ENABLE_REFER
    char *userid;
    int userid_len;
    long mode;
    zval *total_num, *new_num;
    char filename[STRLEN];
    struct userec *user;
    int total_count=0,new_count=0;

    MAKE_STD_ZVAL(total_num);
    MAKE_STD_ZVAL(new_num);

    if (ZEND_NUM_ARGS()!=4 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "slzz", &userid, &userid_len, &mode, &total_num, &new_num)==FAILURE)
        WRONG_PARAM_COUNT;

    /*if (!PZVAL_IS_REF(total_num)||!PZVAL_IS_REF(new_num)) {
        zend_error(E_WARNING, "Parameter wasn't passed by reference");
        RETURN_FALSE;
    }*/

    if (userid_len>IDLEN)
        RETURN_FALSE;

    if (!getuser(userid, &user))
        RETURN_FALSE;

	if (set_refer_file_from_mode(filename, mode)!=0) {
        zend_error(E_WARNING, "Invalid refer mode");
        RETURN_FALSE;
    }

    total_count=refer_get_count(user, filename);
    new_count=refer_get_new(user, filename);

    ZVAL_LONG(total_num, total_count);
    ZVAL_LONG(new_num, new_count);

    RETURN_TRUE;
#else
    RETURN_FALSE;
#endif
}
/**
 * Load user refers array
 * prototype:
 * int bbs_load_refer(string userid, long mode, long start, long count, array &refers);
 *
 * @param string userid
 * @param long mode:
 *                1: refer
 *                2: reply
 *                others: invalid mode
 * @param long start: start position, from 0
 * @param long count: elements count
 * @param array refers, element struct
 *        array (
 *            "BOARD":    board name
 *            "USER":     user id
 *            "TITLE":    article title
 *            "ID":       article id
 *            "GROUP_ID": article groupid
 *            "RE_ID":    article reid
 *            "FLAG":     some flags for refer:
 *                        0x01: read
 *            "TIME":     timestamp, refer time
 *        );
 *
 * @return
 *         -1: parameter error, parameter $refers should be passed by reference in php
 *         -2: invalid user id
 *         -3: system error
 *         >=0: element count
 *
 * @author: windinsn, Jan 29, 2012
 */
PHP_FUNCTION(bbs_load_refer) {
#ifdef ENABLE_REFER
    char *userid;
    int userid_len;
    long mode, start, count;
    zval *list, *element;
    struct userec *user;
    char filename[STRLEN];
    char buf[STRLEN];
    int total, num, i;
    struct refer *refers;

    MAKE_STD_ZVAL(list);
    if (ZEND_NUM_ARGS()!=5 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "slllz", &userid, &userid_len, &mode, &start, &count, &list)==FAILURE)
        WRONG_PARAM_COUNT;

    /*if (!PZVAL_IS_REF(list)) {
        zend_error(E_WARNING, "Parameter wasn't passed by reference");
        RETURN_LONG(-1);
    }*/

    if (userid_len>IDLEN)
        RETURN_LONG(-2);
    if (!getuser(userid, &user))
        RETURN_LONG(-2);

    if (set_refer_file_from_mode(filename, mode)!=0) {
        zend_error(E_WARNING, "Invalid refer mode");
        RETURN_FALSE;
    }

    total=refer_get_count(user, filename);
    if (total<=0)
        RETURN_LONG(0);

    start++;
    if (start<=0)
        start=1;

    if (count>total-start+1)
        count=total-start+1;
    if (count<=0)
        RETURN_LONG(0);

    if (array_init(list)!=SUCCESS)
        RETURN_LONG(-3);

    refers=emalloc(count*sizeof(struct refer));
    if (NULL==refers)
        RETURN_LONG(-4);

    sethomefile(buf, user->userid, filename);
    num=get_records(buf, refers, sizeof(struct refer), start, count);

    for (i=0; i<num; i++) {
        MAKE_STD_ZVAL(element);
        array_init(element);
#ifdef ENABLE_BOARD_MEMBER
        const struct boardheader *board;
        if ((board=getbcache((refers+i)->board))!=NULL && (board->flag&BOARD_MEMBER_READ)) {
            struct fileheader fh;
            bzero(&fh, sizeof(struct fileheader));
            /* 此处refers+i作者肯定不是当前用户，因此直接找该文章的主题文章，减少member_read_perm里相同的调用 */
            if (get_ent_from_id_ext(DIR_MODE_NORMAL, (refers+i)->groupid, board->filename, &fh)<=0 || !member_read_perm(board, &fh, user))
                strcpy((refers+i)->user, MEMBER_POST_OWNER);
        }
#endif
        bbs_make_refer_array(element, refers+i);
        zend_hash_index_update(Z_ARRVAL_P(list), i, (void *) &element, sizeof(zval*), NULL);
    }

    efree(refers);

    RETURN_LONG(num);
#else
    RETURN_FALSE;
#endif
}
/**
 * read a refer record
 * prototype:
 * bool bbs_read_refer(string userid, long mode, long position);
 *
 * @param string userid
 * @param long mode:
 *                1: refer
 *                2: reply
 *                others: invalid mode
 * @param long position:
 *                refer index position, from 0
 *
 * @return TRUE on success
 *         FALSE on failure.
 *
 * @author: windinsn, Jan 29, 2012
 */
PHP_FUNCTION(bbs_read_refer)
{
#ifdef ENABLE_REFER
    char *userid;
    int userid_len;
    long mode, pos;
    char buf[STRLEN], path[STRLEN];
    struct userec *user;
    int total, offset, fd;
    struct refer refer;
    unsigned char ch;

    if (ZEND_NUM_ARGS()!=3 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sll", &userid, &userid_len, &mode, &pos)==FAILURE)
        WRONG_PARAM_COUNT;

    if (userid_len>IDLEN)
        RETURN_FALSE;
    if (!getuser(userid, &user))
        RETURN_FALSE;

   if (set_refer_file_from_mode(buf, mode)!=0) {
        zend_error(E_WARNING, "Invalid refer mode");
        RETURN_FALSE;
    }

    total=refer_get_count(user, buf);
    if (pos<0||pos>=total)
        RETURN_FALSE;

    offset=(int)((char *) &(refer.flag)-(char *) &(refer));
    sethomefile(path, user->userid, buf);
    if ((fd=open(path, O_RDWR))==-1)
        RETURN_FALSE;

    lseek(fd, pos*sizeof(refer)+offset, SEEK_SET);
    read(fd, &ch, 1);
    if (!(ch&FILE_READ)) {
        ch |= FILE_READ;
        lseek(fd, -1, SEEK_CUR);
        write(fd, &ch, 1);

        setmailcheck(user->userid);
    }
    close(fd);

    RETURN_TRUE;
#else
    RETURN_FALSE;
#endif
}
/**
 * Delete a refer element
 * bool bbs_delete_refer(string userid, long mode, long position);
 *
 * @param string userid
 * @param long mode:
 *                 1: refer
 *                 2: reply
 *                 others: invalid mode
 * @param long position:
 *                 refer index position, from 0
 *
 * @return TRUE on success, FALSE on failure
 *
 * @author: windinsn, Jan 29, 2012
 */
PHP_FUNCTION(bbs_delete_refer)
{
#ifdef ENABLE_REFER
    char *userid;
    int userid_len;
    long mode, pos;
    struct userec *user;
    int total;
    char buf[STRLEN], path[STRLEN];
    struct refer refer;

    if (ZEND_NUM_ARGS()!=3 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sll", &userid, &userid_len, &mode, &pos)==FAILURE)
        WRONG_PARAM_COUNT;

    if (userid_len>IDLEN)
        RETURN_FALSE;
    if (!getuser(userid, &user))
        RETURN_FALSE;

    if (set_refer_file_from_mode(buf, mode)!=0) {
        zend_error(E_WARNING, "Invalid refer mode");
        RETURN_FALSE;
    }

    total=refer_get_count(user, buf);
    if (pos<0||pos>=total)
        RETURN_FALSE;

    sethomefile(path, user->userid, buf);
    if (get_record(path, &refer, sizeof(refer), pos+1)!=0)
        RETURN_FALSE;

    if (0!=delete_record(path, sizeof(refer), pos+1, (RECORD_FUNC_ARG)refer_cmp, &refer))
        RETURN_FALSE;

    if (pos==total-1)
        setmailcheck(user->userid);

    RETURN_TRUE;
#else
    RETURN_FALSE;
#endif
}
/**
 * Read all refers
 * bool bbs_read_all_refer(string userid, long mode)
 *
 * @param string userid
 * @param long mode
 *              1: refer
 *              2: reply
 *              others: invalid mode
 *
 * @return TRUE on success, FALSE on failure
 *
 * @author: windinsn, Jan 29, 2012
 */
PHP_FUNCTION(bbs_read_all_refer)
{
#ifdef ENABLE_REFER
    char *userid;
    int userid_len;
    long mode;
    struct userec *user;
    char filename[STRLEN];
    char path[STRLEN];

    if (ZEND_NUM_ARGS()!=2 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &userid, &userid_len, &mode)==FAILURE)
        WRONG_PARAM_COUNT;

    if (userid_len>IDLEN)
        RETURN_FALSE;

    if (!getuser(userid, &user))
        RETURN_FALSE;

    if (set_refer_file_from_mode(filename, mode)!=0) {
        zend_error(E_WARNING, "Invalid refer mode");
        RETURN_FALSE;
    }

    sethomefile(path, user->userid, filename);
    refer_read_all(path);
    setmailcheck(user->userid);
    RETURN_TRUE;
#else
    RETURN_FALSE;
#endif
}

/**
 * Delete all refers
 * bool bbs_truncate_refer(string userid, long mode)
 *
 * @param string userid
 * @param long mode
 *              1: refer
 *              2: reply
 *              others: invalid mode
 *
 * @return TRUE on success, FALSE on failure
 *
 * @author: windinsn, Jan 29, 2012
 */
PHP_FUNCTION(bbs_truncate_refer)
{
#ifdef ENABLE_REFER
    char *userid;
    int userid_len;
    long mode;
    struct userec *user;
    char filename[STRLEN];
    char path[STRLEN];

    if (ZEND_NUM_ARGS()!=2 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &userid, &userid_len, &mode)==FAILURE)
        WRONG_PARAM_COUNT;

    if (userid_len>IDLEN)
        RETURN_FALSE;

    if (!getuser(userid, &user))
        RETURN_FALSE;

    if (set_refer_file_from_mode(filename, mode)!=0) {
        zend_error(E_WARNING, "Invalid refer mode");
        RETURN_FALSE;
    }

    sethomefile(path, user->userid, filename);
    refer_truncate(path);
    setmailcheck(user->userid);

    RETURN_TRUE;
#else
    RETURN_FALSE;
#endif
}

PHP_FUNCTION(bbs_new_msg_get_message)
{
#ifdef ENABLE_NEW_MSG
	long id;
	zval *message;
	struct new_msg_handle handle;
	struct new_msg_message msg;
	long i;

	if (ZEND_NUM_ARGS()!=2 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lz", &id, &message)==FAILURE)
        	WRONG_PARAM_COUNT;
	if (!PZVAL_IS_REF(message)) {
		zend_error(E_WARNING, "Parameter wasn't passed by reference");
		RETURN_LONG(-1);
	}

	if (strcasecmp(getCurrentUser()->userid, "guest")==0)
		RETURN_LONG(-2);

	new_msg_init(&handle, getCurrentUser());
	if (new_msg_open(&handle)<0)
		RETURN_LONG(-3);

	bzero(&msg, sizeof(struct new_msg_message));
	i=new_msg_get_message(&handle, id, &msg);
	new_msg_close(&handle);

	if (i<0)
		RETURN_LONG(-4);
	if (i==0)
		RETURN_LONG(0);

	if (array_init(message)!=SUCCESS)
		RETURN_LONG(-5);

	bbs_make_new_msg_message_array(message, &msg);
	RETURN_LONG(msg.id);
#else
	RETURN_FALSE;
#endif
}

PHP_FUNCTION(bbs_new_msg_get_attachment)
{
#ifdef ENABLE_NEW_MSG
	long id;
	struct new_msg_handle handle;
	struct new_msg_message msg;
	long i;
	char *output;
	int size;

	if (ZEND_NUM_ARGS()!=1 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &id)==FAILURE)
		WRONG_PARAM_COUNT;

	if (strcasecmp(getCurrentUser()->userid, "guest")==0)
		RETURN_FALSE;

	new_msg_init(&handle, getCurrentUser());
	if (new_msg_open(&handle)<0)
		RETURN_FALSE;

	bzero(&msg, sizeof(struct new_msg_message));
	i=new_msg_get_message(&handle, id, &msg);
	if (i<=0) {
		new_msg_close(&handle);
		RETURN_FALSE;
	}

	size=msg.attachment.size;
	if (size<=0) {
		new_msg_close(&handle);
		RETURN_FALSE;
	}

	output=(char *)emalloc(size);
	if (!output) {
		new_msg_close(&handle);
		RETURN_FALSE;
	}
	i=new_msg_get_attachment(&handle, msg.id, output, size);
	new_msg_close(&handle);

	if (i<0) {
		efree(output);
		RETURN_FALSE;
	}

	RETVAL_STRINGL(output, size, 0);
#else
	RETURN_FALSE;
#endif
}

PHP_FUNCTION(bbs_new_msg_get_new)
{
#ifdef ENABLE_NEW_MSG
	struct new_msg_handle handle;
	int ret;

	new_msg_init(&handle, getCurrentUser());
	if (new_msg_open(&handle)<0)
		RETURN_LONG(-1);

	ret=new_msg_get_new_count(&handle);
	new_msg_close(&handle);

	RETURN_LONG(ret);
#else
	RETURN_FALSE;
#endif
}

PHP_FUNCTION(bbs_new_msg_get_users)
{
#ifdef ENABLE_NEW_MSG
	struct new_msg_handle handle;
	int ret;

	new_msg_init(&handle, getCurrentUser());
	if (new_msg_open(&handle)<0)
		RETURN_LONG(-1);
	ret=new_msg_get_users(&handle);
	new_msg_close(&handle);

	RETURN_LONG(ret);
#else
	RETURN_FALSE;
#endif
}

PHP_FUNCTION(bbs_new_msg_load_users)
{
#ifdef ENABLE_NEW_MSG
	long start, count;
	int i, ret;
	zval *list, *element;
	struct new_msg_user *users;
	struct new_msg_handle handle;

	if (ZEND_NUM_ARGS()!=3 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "llz", &start, &count, &list)==FAILURE)
        WRONG_PARAM_COUNT;

	if (!PZVAL_IS_REF(list)) {
		zend_error(E_WARNING, "Parameter wasn't passed by reference");
		RETURN_LONG(-1);
	}

	if (count<=0)
		RETURN_LONG(-2);

	if (array_init(list)!=SUCCESS)
		RETURN_LONG(-3);

	users=(struct new_msg_user *)emalloc(sizeof(struct new_msg_user)*count);

	if (!users)
		RETURN_LONG(-4);

	bzero(users, sizeof(struct new_msg_user)*count);

	new_msg_init(&handle, getCurrentUser());

	if (new_msg_open(&handle)<0) {
		efree(users);
		RETURN_LONG(-2);
	}

	ret=new_msg_load_users(&handle, start, count, users);
	if (ret>0) {
		for (i=0;i<ret;i++) {
			MAKE_STD_ZVAL(element);
			array_init(element);
			bbs_makr_new_msg_user_array(element, users+i);
			zend_hash_index_update(Z_ARRVAL_P(list), i, (void *) &element, sizeof(zval*), NULL);
		}
	}
	efree(users);
	new_msg_close(&handle);

	if (ret<0)
		RETURN_LONG(-3);

	RETURN_LONG(ret);
#else
	RETURN_FALSE;
#endif
}

PHP_FUNCTION(bbs_new_msg_get_user_messages)
{
#ifdef ENABLE_NEW_MSG
	char *user_id;
	int user_id_len;
	struct userec *user;
	int ret;
	struct new_msg_handle handle;

	if (ZEND_NUM_ARGS()!=1 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &user_id, &user_id_len)==FAILURE)
		WRONG_PARAM_COUNT;

	if (user_id_len<=0 || user_id_len>IDLEN || !user_id[0])
		RETURN_LONG(-1);
	if (!getuser(user_id, &user))
		RETURN_LONG(-1);
	new_msg_init(&handle, getCurrentUser());
	if (new_msg_open(&handle)<0)
		RETURN_LONG(-2);

	ret=new_msg_get_user_messages(&handle, user->userid);
	new_msg_close(&handle);

	RETURN_LONG(ret);
#else
	RETURN_FALSE;
#endif
}

PHP_FUNCTION(bbs_new_msg_load_user_messages)
{
#ifdef ENABLE_NEW_MSG
	long start, count;
	int ret, i;
	zval *list, *element;
	char *user_id;
	int user_id_len;
	struct new_msg_message *messages;
	struct new_msg_handle handle;
	struct userec *user;

	if (ZEND_NUM_ARGS()!=4 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sllz", &user_id, &user_id_len, &start, &count, &list)==FAILURE)
		WRONG_PARAM_COUNT;

	if (!PZVAL_IS_REF(list)) {
		zend_error(E_WARNING, "Parameter wasn't passed by reference");
		RETURN_LONG(-1);
	}

	if (count<=0)
		RETURN_LONG(-2);

	if (array_init(list)!=SUCCESS)
		RETURN_LONG(-3);

	if (user_id_len<=0 || user_id_len>IDLEN || !user_id[0])
		RETURN_LONG(-4);

	if (!getuser(user_id, &user))
		RETURN_LONG(-4);

	messages=(struct new_msg_message *)emalloc(sizeof(struct new_msg_message)*count);
	if (!messages)
		RETURN_LONG(-5);
	bzero(messages, sizeof(struct new_msg_message)*count);

	new_msg_init(&handle, getCurrentUser());
	if (new_msg_open(&handle)<0) {
		efree(messages);
		RETURN_LONG(-6);
	}

	ret=new_msg_load_user_messages(&handle, user->userid, start, count, messages);
	if (ret>0) {
		for (i=0;i<ret;i++) {
			MAKE_STD_ZVAL(element);
			array_init(element);
			bbs_make_new_msg_message_array(element, messages+i);
			zend_hash_index_update(Z_ARRVAL_P(list), i, (void *) &element, sizeof(zval*), NULL);
		}
	}
	efree(messages);
	new_msg_close(&handle);

	if (ret<0)
		RETURN_LONG(-7);

	RETURN_LONG(ret);
#else
	RETURN_FALSE;
#endif
}

PHP_FUNCTION(bbs_new_msg_delete)
{
#ifdef ENABLE_NEW_MSG
	char *user_id;
	int user_id_len;
	struct userec *user;
	int ret;
	struct new_msg_handle handle;

	if (ZEND_NUM_ARGS()!=1 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &user_id, &user_id_len)==FAILURE)
		WRONG_PARAM_COUNT;

	if (user_id_len<=0 || user_id_len>IDLEN || !user_id[0])
		RETURN_LONG(-1);
	if (!getuser(user_id, &user))
		RETURN_LONG(-1);
	new_msg_init(&handle, getCurrentUser());
	if (new_msg_open(&handle)<0)
		RETURN_LONG(-2);

	ret=new_msg_remove_user_messages(&handle, user->userid);
	new_msg_close(&handle);

	if (ret<0)
		RETURN_LONG(-3);

	RETURN_LONG(0);
#else
	RETURN_FALSE;
#endif
}

PHP_FUNCTION(bbs_new_msg_forward)
{
#ifdef ENABLE_NEW_MSG
	char *user_id, *to_id;
	int user_id_len, to_id_len;
	struct userec *user, *to;
	struct new_msg_handle handle;
	struct new_msg_user info;
	char title[100], path[PATHLEN];
	int ret;

	if (ZEND_NUM_ARGS()!=2 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &user_id, &user_id_len, &to_id, &to_id_len)==FAILURE)
		WRONG_PARAM_COUNT;

	if (user_id_len<=0 || user_id_len>IDLEN || !user_id[0])
		RETURN_LONG(-1);
	if (to_id_len<=0 || to_id_len>IDLEN || !to_id[0])
		RETURN_LONG(-2);
	if (!getuser(user_id, &user))
		RETURN_LONG(-1);
	if (!getuser(to_id, &to))
		RETURN_LONG(-2);

	if (!HAS_PERM(getCurrentUser(), PERM_SYSOP) && to->userlevel & PERM_SUICIDE)
		RETURN_LONG(-3);

	if (!HAS_PERM(getCurrentUser(), PERM_SYSOP) && !(to->userlevel & PERM_BASIC))
		RETURN_LONG(-4);

	if (!HAS_PERM(getCurrentUser(), PERM_SYSOP) && chkusermail(to) >= 3)
		RETURN_LONG(-5);

	if (false == canIsend2(getCurrentUser(), to->userid))
		RETURN_LONG(-6);

	new_msg_init(&handle, getCurrentUser());
	if (new_msg_open(&handle)<0)
		RETURN_LONG(-7);

	bzero(&info, sizeof(struct new_msg_user));
	strncpy(info.msg.user, user->userid, IDLEN+1);
	sprintf(info.name, "与 %s 的对话", user->userid);
	if (new_msg_dump(&handle, &info, 0x01, 0, 0)<0)
		ret=-8;
	else {
		sprintf(title, "与 %s 的短信记录(转寄)", user->userid);
		new_msg_dump_file(path, &handle, &info);
		ret=mail_file(getCurrentUser()->userid, path, to->userid, title, BBSPOST_COPY, NULL);
		if (ret!=0)
			ret=-9;
	}

	new_msg_close(&handle);
	RETURN_LONG(0);
#else
	RETURN_FALSE;
#endif
}

PHP_FUNCTION(bbs_new_msg_read)
{
#ifdef ENABLE_NEW_MSG
	char *user_id;
	int user_id_len;
	struct userec *user;
	struct new_msg_handle handle;
	struct new_msg_user info;

	if (ZEND_NUM_ARGS()!=1 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &user_id, &user_id_len)==FAILURE)
		WRONG_PARAM_COUNT;

	if (user_id_len<=0 || user_id_len>IDLEN || !user_id[0])
		RETURN_LONG(-1);
	if (!getuser(user_id, &user))
		RETURN_LONG(-1);
	new_msg_init(&handle, getCurrentUser());
	if (new_msg_open(&handle)<0)
		RETURN_LONG(-2);

	bzero(&info, sizeof(struct new_msg_user));
	strncpy(info.msg.user, user->userid, IDLEN+1);
	info.msg.flag=0;
	new_msg_read(&handle, &info);

	new_msg_close(&handle);

	RETURN_LONG(0);
#else
	RETURN_FALSE;
#endif
}

PHP_FUNCTION(bbs_new_msg_send)
{
#ifdef ENABLE_NEW_MSG
	char *user_id, *content, *send_type;
	int user_id_len, content_len, send_type_len;
	struct userec *user;
	int ret;
	struct new_msg_handle from;
	struct new_msg_handle to;
	struct new_msg_info msg;
	struct new_msg_attachment attachment;
	char attach_dir[MAXPATH], attach_info[MAXPATH], path[PATHLEN+1];
	FILE *fp;
	char buf[256];
	struct stat st;
	char *ext;

	if (ZEND_NUM_ARGS()!=3 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sss", &user_id, &user_id_len, &content, &content_len, &send_type, &send_type_len)==FAILURE)
			WRONG_PARAM_COUNT;

	if (user_id_len<=0 || user_id_len>IDLEN || !user_id[0])
		RETURN_LONG(-2);
	if (!getuser(user_id, &user))
		RETURN_LONG(-2);
	if (content_len<=0 || !content[0])
		RETURN_LONG(-24);
	if (content_len>NEW_MSG_MAX_SIZE)
		RETURN_LONG(-25);

	ret=new_msg_check(getCurrentUser(), user);
	if (ret<0)
		RETURN_LONG(ret);

	new_msg_init(&from, getCurrentUser());
	new_msg_init(&to, user);

	if (new_msg_open(&from)<0)
		RETURN_LONG(-11);
	if (new_msg_open(&to)<0) {
		new_msg_close(&from);
		RETURN_LONG(-12);
	}

	bzero(&msg, sizeof(struct new_msg_info));
	bzero(&attachment, sizeof(struct new_msg_attachment));

	if (send_type_len<=0 || !send_type[0])
		strcpy(msg.from, "WWW");
	else
		strncpy(msg.from, send_type, NEW_MSG_FROM_LEN+1);
	strncpy(msg.msg, content, content_len);

	/* attachment */
	getattachtmppath(attach_dir, MAXPATH, getSession());
	snprintf(attach_info, MAXPATH, "%s/.index", attach_dir);
    if ((fp=fopen(attach_info, "r"))!=NULL) {
            while(fgets(buf, 256, fp)) {
                    char *name;
                    char *ptr;

                    name=strchr(buf, ' ');
                    if (name==NULL)
                            continue;
                    *name=0;
                    name++;
                    ptr=strchr(name, '\n');
                    if (ptr)
                            *ptr=0;

                    if (stat(buf, &st)!=-1&&S_ISREG(st.st_mode)) {
                            strncpy(attachment.name, name, NEW_MSG_ATTACHMENT_NAME_LEN);
                            attachment.name[NEW_MSG_ATTACHMENT_NAME_LEN]=0;
                            ext=strrchr(attachment.name, '.');
                            strncpy(attachment.type, get_mime_type_from_ext(ext), NEW_MSG_ATTACHMENT_TYPE_LEN);
                            attachment.size=st.st_size;
                            strncpy(path, buf, PATHLEN);
                            path[PATHLEN]=0;
                            break;
                    }
            }
            fclose(fp);
    }

    if (attachment.name[0]==0 || attachment.size<=0)
    	ret=new_msg_send(&from, &to, &msg, NULL, NULL);
    else {
    	ret=new_msg_send(&from, &to, &msg, &attachment, path);
    	if (ret>=0) {
    		upload_del_file(attachment.name, getSession());
    	}
    }

	new_msg_close(&from);
	new_msg_close(&to);

	if (ret<0)
		RETURN_LONG(ret-20);

	RETURN_LONG(ret);
#else
	RETURN_FALSE;
#endif
}

PHP_FUNCTION(bbs_new_msg_get_capacity)
{
#ifdef ENABLE_NEW_MSG
	RETURN_LONG(new_msg_get_capacity(getCurrentUser()));
#else
	RETURN_LONG(0);
#endif
}
PHP_FUNCTION(bbs_new_msg_get_size)
{
#ifdef ENABLE_NEW_MSG
	RETURN_LONG(new_msg_get_size(getCurrentUser()));
#else
	RETURN_LONG(0);
#endif
}
