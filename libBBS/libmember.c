#include "bbs.h"

#ifndef LIB_MEMBER
#ifdef ENABLE_BOARD_MEMBER

#ifndef ENABLE_MEMBER_CACHE
#include <mysql.h>
#endif


#ifndef MAX_MEMBER_BOARD_ARTICLES
#define MAX_MEMBER_BOARD_ARTICLES 1000
#endif

#ifndef MIN_MEMBER_BOARD_ARTICLE_STAT
#define MIN_MEMBER_BOARD_ARTICLE_STAT 60
#endif

#ifndef MIN_MEMBER_STATUS_LOAD_TIME
#define MIN_MEMBER_STATUS_LOAD_TIME 600
#endif

int check_board_member_manager_by_name(struct board_member_status *status, char *name, int perm) {
    const struct boardheader *board;
    
    if (getbid(name, &board) <= 0)
        return 0;
        
    return check_board_member_manager(status, board, perm);
}

int check_board_member_manager(struct board_member_status *status, const struct boardheader *board, int perm) {
    if (NULL==board)
        return 0;
    if (chk_currBM(board->BM, getSession()->currentuser))
        return 1;
    if (!valid_core_member(getSession()->currentuser->userid))
        return 0;
        
    int bid, flag;
    struct board_member member;
    time_t now;
    
    now=time(NULL);    
    bid=getbid(board->filename, NULL);
    if (NULL==status || now - status->update_time > MIN_MEMBER_STATUS_LOAD_TIME || status->bid != bid)
        flag = -1;
    else
        flag = status->flag;
        
    if (flag != -1)
        return (flag&perm)?1:0;
        
    if (get_board_member(board->filename, getSession()->currentuser->userid, &member)<0)
        return 0;
        
    if (NULL!=status) {
        status->update_time=now;
        status->bid=bid;
        status->status=member.status;
        status->flag=member.flag;
    }
    
    if (member.status!=BOARD_MEMBER_STATUS_MANAGER)
        return 0;

    return (member.flag&perm)?1:0;
}

char *get_bmp_name(char *name, int bmp) {
    switch(bmp) {
        case BMP_DELETE:
            strcpy(name, "删文");
            break;
        case BMP_DENY:
            strcpy(name, "封禁");
            break;
        case BMP_SIGN:
            strcpy(name, "标记");
            break;
        case BMP_ANNOUNCE:
            strcpy(name, "精华区");
            break;
        case BMP_REFER:
            strcpy(name, "驻版提醒");
            break;
        case BMP_JUNK:
            strcpy(name, "查看删除区");
            break;
        case BMP_VOTE:
            strcpy(name, "投票管理");
            break;
        case BMP_RECOMMEND:
            strcpy(name, "置顶/不可RE/推荐");
            break;
        case BMP_RANGE:
            strcpy(name, "区段操作");
            break;
        case BMP_NOTE:
            strcpy(name, "备忘录/模板/版规/封禁理由/标题关键字");
            break;
        case BMP_THREAD:
            strcpy(name, "主题操作");
            break;
        case BMP_MODIFY:
            strcpy(name, "修改文章");
            break;
        case BMP_CLUB:
            strcpy(name, "俱乐部管理");
            break;
        case BMP_SELENE:
            strcpy(name, "收录被Like的文章");
            break;
        default:
            strcpy(name, "未定义");
    }
    
    return name;
}

char *get_member_status_name(char *name, int status) {
    switch(status){
        case BOARD_MEMBER_STATUS_CANDIDATE:
            strcpy(name, "待审批驻版用户");
            break;
        case BOARD_MEMBER_STATUS_NORMAL:
            strcpy(name, "驻版用户");
            break;
        case BOARD_MEMBER_STATUS_MANAGER:
            strcpy(name, "核心驻版用户");
            break;
        case BOARD_MEMBER_STATUS_BLACK:
            strcpy(name, "驻版黑名单");
            break;
        case BOARD_MEMBER_STATUS_NONE:
            strcpy(name, "非驻版用户");
            break;
        default:
            strcpy(name, "未定义");
    }
    
    return name;
}

int get_bmp_value(int index) {
    if (index<0||index>=BMP_COUNT)
        return 0;
    return 1<<index;
}

int board_member_log(struct board_member *member, char *title, char *log) {
    char path[STRLEN], buf[STRLEN];
    FILE *handle;
    
    gettmpfilename(path, "board.member.log");
    if ((handle = fopen(path, "w")) != NULL) { 
        if (NULL!=member)
            fprintf(handle, "操作者: %s\n\n用户: %s\n版面:%s\n\n", getSession()->currentuser->userid, member->user, member->board);
        
        fprintf(handle, "记录: \n%s\n\n", log);
        //fprintf(handle, "以下是个人资料");
        //getuinfo(handle, getSession()->currentuser);
        fclose(handle);
        
        if (NULL!=member)
            sprintf(buf, "%s#%s#%s", member->user, member->board, title);
        else
            strncpy(buf, title, sizeof(buf));
        post_file(getSession()->currentuser, "", path, BOARD_MEMBER_LOG_BOARD, buf, 0, 2, getSession());
        unlink(path);
    }
    
    return 0;
}

int delete_board_member_record(const char *name, const char *user_id) {
#ifdef ENABLE_MEMBER_CACHE
    int i;
    struct board_member member;
    
    i=get_member_index(name, user_id);
    if (i<=0)
        return -1;
    if (remove_member(i)<0)
        return -2;
    
    strncpy(member.user, user_id, IDLEN+1);
    strncpy(member.board, name, STRLEN);
    board_member_log(&member, "退出驻版", "退出驻版");    
    return 0;
#else
    MYSQL s;
    char sql[200];
    char my_name[STRLEN];
    char my_user_id[STRLEN];
    struct board_member member;
    
    if (!name[0])
        return -1;
    if (!user_id[0])
        return -2;
    if (0==strcmp(user_id, "guest"))
        return -3;    
    
    mysql_init(&s);
    if (!my_connect_mysql(&s)) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        return -4;
    }
    
    my_name[0]=0;
    my_user_id[0]=0;
    mysql_escape_string(my_name, name, strlen(name));
    mysql_escape_string(my_user_id, user_id, strlen(user_id));
    
    sprintf(sql,"DELETE FROM `board_user` WHERE `board`='%s' AND `user`='%s' LIMIT 1;", my_name, my_user_id);

    if (mysql_real_query(&s, sql, strlen(sql))) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        mysql_close(&s);
        return -5;
    }

    mysql_close(&s);
    
    strcpy(member.user, user_id);
    strcpy(member.board, name);
    board_member_log(&member, "退出驻版", "退出驻版");
    
    return 0;
#endif
}

int load_board_member_config(const char *name, struct board_member_config *config) {
    const struct boardheader *board;
    char path[PATHLEN];
    struct stat st;
    int fd;
    
    board=getbcache(name);
    if (0==board)
        return -1;
    if (board->flag&BOARD_GROUP)
        return -2;
    
    setbfile(path, board->filename, BOARD_MEMBER_CONFIG);
    if (stat(path, &st)<0) {
        config->approve=0;
        config->max_members=MEMBER_BOARD_MAX_DEFAULT;
        config->logins=0;
        config->posts=0;
        config->score=0;
        config->level=0;
        config->board_posts=0;
        config->board_origins=0;
        config->board_marks=0;
        config->board_digests=0;
        
        return 0;
    } 
    
    bzero(config, sizeof(struct board_member_config));
    if ((fd = open(path, O_RDONLY, 0644)) < 0) 
        return -3;
    
    read(fd, config, sizeof(struct board_member_config));
    close(fd);
    
    return 1;
}

int save_board_member_config(const char *name, struct board_member_config *config) {
    const struct boardheader *board;
    struct board_member_config old;
    char path[PATHLEN];
    char log[1024];
    char buf[STRLEN];
    char title[STRLEN];
    int fd;
    
    board=getbcache(name);
    if (0==board)
        return -1;
    if (board->flag&BOARD_GROUP)
        return -2;
    if (!HAS_PERM(getSession()->currentuser,PERM_SYSOP)&&!chk_currBM(board->BM,getSession()->currentuser))    
        return -3;
       
        load_board_member_config(board->filename, &old);
    setbfile(path, board->filename, BOARD_MEMBER_CONFIG);
    if ((fd = open(path, O_WRONLY | O_CREAT, 0644)) < 0)
        return -4;
    write(fd, config, sizeof(struct board_member_config));
    close(fd);
    
    sprintf(title, "更改驻版设置@%s", board->filename);
    sprintf(log, "版面: %s\n用户: %s\n\n",
        board->filename,
        getSession()->currentuser->userid
    );
    
    sprintf(buf, "审    批: %s -> %s%s\033[m\n", (old.approve>0)?"是":"否", (old.approve==config->approve)?"":"\033[1;31m", (config->approve>0)?"是":"否");
    strcat(log, buf);
    sprintf(buf, "最大用户: %d -> %s%d\033[m\n", old.max_members , (old.max_members==config->max_members)?"":"\033[1;31m", config->max_members);
    strcat(log, buf);
    sprintf(buf, "登 录 数: %d -> %s%d\033[m\n", old.logins , (old.logins==config->logins)?"":"\033[1;31m", config->logins);
    strcat(log, buf);
    sprintf(buf, "发 文 数: %d -> %s%d\033[m\n", old.posts , (old.posts==config->posts)?"":"\033[1;31m", config->posts);
    strcat(log, buf);
    sprintf(buf, "用户积分: %d -> %s%d\033[m\n", old.score , (old.score==config->score)?"":"\033[1;31m", config->score);
    strcat(log, buf);
    sprintf(buf, "用户等级: %d -> %s%d\033[m\n", old.level , (old.level==config->level)?"":"\033[1;31m", config->level);
    strcat(log, buf);
    sprintf(buf, "版面发文: %d -> %s%d\033[m\n", old.board_posts , (old.board_posts==config->board_posts)?"":"\033[1;31m", config->board_posts);
    strcat(log, buf);
    sprintf(buf, "版面原创: %d -> %s%d\033[m\n", old.board_origins , (old.board_origins==config->board_origins)?"":"\033[1;31m", config->board_origins);
    strcat(log, buf);
    sprintf(buf, "版面 M文: %d -> %s%d\033[m\n", old.board_marks , (old.board_marks==config->board_marks)?"":"\033[1;31m", config->board_marks);
    strcat(log, buf);
    sprintf(buf, "版面 G文: %d -> %s%d\033[m\n", old.board_digests , (old.board_digests==config->board_digests)?"":"\033[1;31m", config->board_digests);
    strcat(log, buf);
    
    board_member_log(NULL, title, log);
    return 0;
}

int get_user_max_member_boards(const struct userec *user) 
{
    int level, user_max;
    char buf[STRLEN];
    
#if defined(NEWSMTH) && !defined(SECONDSITE)
    level=uvaluetochar(buf, (struct userec *)user)*4;  
    user_max=(level>MEMBER_USER_MAX_DEFAULT)?level:MEMBER_USER_MAX_DEFAULT;
#else
    user_max=MEMBER_USER_MAX_DEFAULT;        
#endif    

    if (HAS_PERM(user, PERM_SYSOP))
        user_max += 20;
    else if (HAS_PERM(user, PERM_BMAMANGER))
        user_max += 10;
    else
        ;
        
    return user_max;    
}

/**
  * 用户申请成为某版的驻版用户
  * -1: guest不允许驻版
  * -2,-3: 版面错误
  * -4: 无版面权限
  * -6: 已经是该版的驻版用户
  * -7: 驻版申请已提交、等待审批中
  * -10: 登录数未达到要求
  * -11: 发文数未达到要求
  * -12: 积分未达到要求
  * -13: 用户等级未达到要求
  * -14: 在本版的发文数未达到要求
  * -15: 在本版的原创文章数未达到要求
  * -16: 在本版的M文章未达到要求
  * -17: 在本版的G文章未达到要求
  * -19: 本版驻版用户超出限制
  * -21: 所驻版面超过限制
  * 其他负数: 系统错误
  * 1: 提交审批，等待版主通过
  * 2: 成为驻版用户
  *
  * windinsn, 2012.8.12
  */
int join_board_member(const char *name) {
    const struct boardheader *board;
    struct board_member member;
    struct board_member_config config;
    int status, user_max, level, count, num;
    char buf[STRLEN];
#ifndef ENABLE_MEMBER_CACHE    
    MYSQL s;
    char sql[300];
#endif
    char log[1024];
    
    if (0==strcmp(getSession()->currentuser->userid, "guest"))
        return -1;
        
    board=getbcache(name);
    if (0==board)
        return -2;
    if (board->flag&BOARD_GROUP)
        return -3;
    if (!haspostperm(getSession()->currentuser, board->filename))
        return -4;
        
    status=get_board_member(board->filename, getSession()->currentuser->userid, &member);
    if (status < 0)
        return -5;
    if (status == BOARD_MEMBER_STATUS_NORMAL)
        return -6;
    if (status == BOARD_MEMBER_STATUS_CANDIDATE || status == BOARD_MEMBER_STATUS_MANAGER)
        return -7;
    if (status != BOARD_MEMBER_STATUS_NONE)
        return -8;
    
    if (load_board_member_config(board->filename, &config)<0)
        return -9;
    
    sprintf(log, "%s 在版面 %s 的详细信息\n\n", getSession()->currentuser->userid, board->filename);
    
    
    user_max=get_user_max_member_boards(getCurrentUser());
    
    count=0;
    if (config.max_members>0) {
        count=get_board_members(board->filename, 0);
        if (count<0)
            return -18;
        if (count>=config.max_members)
            return -19;
    }
    sprintf(buf, "版面用户数: %d / %d\n", count, config.max_members);    
    strcat(log, buf);
    
    count=get_member_boards(getSession()->currentuser->userid);
    if (count<0)
        return -20;
    if (count>=user_max)
        return -21;
    sprintf(buf, "用户版面数: %d / %d\n", count, user_max);    
    strcat(log, buf);
    
    status=(config.approve>0)?BOARD_MEMBER_STATUS_CANDIDATE:BOARD_MEMBER_STATUS_NORMAL;
    sprintf(buf, "\n是否需要审批: %s\n", (config.approve>0)?"是":"否");
#ifndef ENABLE_MEMBER_CACHE    
    mysql_init(&s);
    if (!my_connect_mysql(&s)) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        return -22;
    }
#endif    
    member.board[0]=0;
    member.user[0]=0;

#ifdef ENABLE_MEMBER_CACHE
    strncpy(member.board, board->filename, STRLEN);
    strncpy(member.user, getSession()->currentuser->userid, IDLEN+1);
#else    
    mysql_escape_string(member.board, board->filename, strlen(board->filename));
    mysql_escape_string(member.user, getSession()->currentuser->userid, strlen(getSession()->currentuser->userid));
#endif
    member.time=time(0);
    member.status=status;
    member.manager[0]=0;
    member.score=0;
    member.title=0;
    member.flag=0;
#ifdef ENABLE_MEMBER_CACHE
    if (add_member(&member)<0)
        return -23;
#else    
    sprintf(sql,"INSERT INTO `board_user` VALUES (\"%s\", \"%s\", FROM_UNIXTIME(%lu), %d, \"\", %u, %d, %u);", member.board, member.user, member.time, member.status, member.score, member.title, member.flag);
    if (mysql_real_query(&s, sql, strlen(sql))) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        mysql_close(&s);
        return -23;
    }
    
    mysql_close(&s);
#endif
    if (BOARD_MEMBER_STATUS_CANDIDATE==status) {
        // 向版主发信
        // TODO
    }
    
    board_member_log(&member, "加入驻版", log);
    return status;
}

int leave_board_member(const char *name) {
    return delete_board_member_record(name, getSession()->currentuser->userid);
}

int approve_board_member(const char *name, const char *user_id) {
    return set_board_member_status(name, user_id, BOARD_MEMBER_STATUS_NORMAL);
}

int black_board_member(const char *name, const char *user_id) {
    return set_board_member_status(name, user_id, BOARD_MEMBER_STATUS_BLACK);
}

int remove_board_member(const char *name, const char *user_id) {
    const struct boardheader *board;
    int ret;
    
    board=getbcache(name);
    if (0==board)
        return -1;
    if (!HAS_PERM(getSession()->currentuser,PERM_SYSOP)&&!chk_currBM(board->BM,getSession()->currentuser))    
        return -2;
    
    ret=delete_board_member_record(board->filename, user_id);
    if (ret<0)
        return ret-2;
    
    return 0;
}

int get_board_member(const char *name, const char *user_id, struct board_member *member) {
#ifdef ENABLE_MEMBER_CACHE
    return get_member_cache(name, user_id, member);
#else
    MYSQL s;
    MYSQL_RES *res;
    MYSQL_ROW row;
    char sql[300];
    char my_name[STRLEN];
    char my_user_id[STRLEN];
    int status;
    
    if (!user_id[0])
        return -1;
    if (0==strcmp(user_id, "guest"))
        return -2;
    if (!name[0])
        return -3;    
    
    mysql_init(&s);
    if (!my_connect_mysql(&s)) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        return -4;
    }
    
    my_name[0]=0;
    my_user_id[0]=0;
    mysql_escape_string(my_name, name, strlen(name));
    mysql_escape_string(my_user_id, user_id, strlen(user_id));
    
    sprintf(sql,"SELECT `board`, `user`, UNIX_TIMESTAMP(`time`), `status`, `manager`, `score`, `title`, `flag` FROM `board_user` WHERE `board`='%s' AND `user`='%s' LIMIT 1;", my_name, my_user_id);
    
    if (mysql_real_query(&s, sql, strlen(sql))) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        mysql_close(&s);
        return -5;
    }
    res = mysql_store_result(&s);
    row = mysql_fetch_row(res);
    
    if (NULL!=member) {
        bzero(member, sizeof(struct board_member));
        member->status=BOARD_MEMBER_STATUS_NONE;
        
        if (row != NULL) {
            strncpy(member->board, row[0], 32);
            strncpy(member->user, row[1], IDLEN+1);
            member->time=atol(row[2]);
            member->status=atol(row[3]);
            strncpy(member->manager, row[4], IDLEN+1);
            member->score=atol(row[5]);
            member->title=atol(row[6]);
            member->flag=atol(row[7]);
        }
        
        status=member->status;
    } else {
        status=BOARD_MEMBER_STATUS_NONE;
        
        if (row != NULL) {
            status=atol(row[3]);
        }
    }
    mysql_free_result(res);

    mysql_close(&s);
    
    if (!valid_core_member(user_id)) {
        if (BOARD_MEMBER_STATUS_MANAGER==status)
            status=BOARD_MEMBER_STATUS_NORMAL;
        if (NULL!=member) {
            if (BOARD_MEMBER_STATUS_MANAGER==member->status)
                member->status=BOARD_MEMBER_STATUS_NORMAL;
            member->flag=0;
        }
    }
    
    return status;
#endif
}

int load_board_members(const char *board, struct board_member *member, int sort, int start, int num, int status) {
#ifdef ENABLE_MEMBER_CACHE
    return load_board_members_cache(board, member, sort, start, num, status);
#else
    MYSQL s;
    MYSQL_RES *res;
    MYSQL_ROW row;
    char sql[300];
    char qtmp[100];
    char my_board[STRLEN];
    int i;
    const struct boardheader *bh;
    struct userec *user;
    
    if (!board[0])
        return -1;
    if (!getbid(board, &bh)||bh->flag&BOARD_GROUP)
        return -1;
    
    mysql_init(&s);
    if (!my_connect_mysql(&s)) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        return -2;
    }
    
    my_board[0]=0;
    mysql_escape_string(my_board, bh->filename, strlen(bh->filename));
    
    sprintf(sql,"SELECT `board`, `user`, UNIX_TIMESTAMP(`time`), `status`, `manager`, `score`, `title`, `flag` FROM `board_user` WHERE `board`='%s' ORDER BY ", my_board);
    switch(sort) {
        case BOARD_MEMBER_SORT_TIME_DESC:
            strcpy(qtmp, " `time` DESC ");
            break;
        case BOARD_MEMBER_SORT_ID_ASC:
            strcpy(qtmp, " `user` ASC ");
            break;
        case BOARD_MEMBER_SORT_ID_DESC:
            strcpy(qtmp, " `user` DESC ");
            break;
        case BOARD_MEMBER_SORT_SCORE_DESC:
            strcpy(qtmp, " `score` DESC ");
            break;
        case BOARD_MEMBER_SORT_SCORE_ASC:
            strcpy(qtmp, " `score` ASC ");
            break;
        case BOARD_MEMBER_SORT_STATUS_ASC:
            strcpy(qtmp, " `status` ASC ");
            break;
        case BOARD_MEMBER_SORT_TITLE_ASC:
            strcpy(qtmp, " `title` ASC ");
            break;
        case BOARD_MEMBER_SORT_TITLE_DESC:
            strcpy(qtmp, " `title` DESC ");
            break;            
        case BOARD_MEMBER_SORT_STATUS_DESC:
            strcpy(qtmp, " `status` DESC ");
            break;        
        case BOARD_MEMBER_SORT_TIME_ASC:
        default:
            strcpy(qtmp, " `time` ASC ");
    }
    strcat(sql, qtmp);
    snprintf(qtmp, 99, " LIMIT %d,%d", start, num);
    strcat(sql, qtmp);
    
    if (mysql_real_query(&s, sql, strlen(sql))) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        mysql_close(&s);
        return -3;
    }
    res = mysql_store_result(&s);
    row = mysql_fetch_row(res);

    i=0;
    while (row != NULL) {
        if (!getuser(row[1], &user)||!check_read_perm(user,bh)) {
            delete_board_member_record(row[0], row[1]);
        } else {
            i++;
            if (i>num)
                break;
                
            strncpy(member[i-1].board, bh->filename, 32);
            strncpy(member[i-1].user, user->userid, IDLEN+1);
            member[i-1].time=atol(row[2]);
            member[i-1].status=atol(row[3]);
            strncpy(member[i-1].manager, row[4], IDLEN+1);
            member[i-1].score=atol(row[5]);
            member[i-1].title=atol(row[6]);
            member[i-1].flag=atol(row[7]);
            
            if (!valid_core_member(member[i-1].user)) {
                if (BOARD_MEMBER_STATUS_MANAGER==member[i-1].status)
                    member[i-1].status=BOARD_MEMBER_STATUS_NORMAL;
                member[i-1].flag=0;
            }
        }
        row = mysql_fetch_row(res);
    }
    mysql_free_result(res);

    mysql_close(&s);
    return i;
#endif
}

int load_member_boards(const char *user_id, struct board_member *member, int sort, int start, int num) {
#ifdef ENABLE_MEMBER_CACHE
    return load_member_boards_cache(user_id, member, sort, start, num);
#else
    MYSQL s;
    MYSQL_RES *res;
    MYSQL_ROW row;
    char sql[300];
    char qtmp[100];
    char my_user_id[STRLEN];
    int i;
    struct userec *user;
    
    if (!user_id[0])
        return -1;
    if (!getuser(user_id, &user))
        return -1;
    
    mysql_init(&s);
    if (!my_connect_mysql(&s)) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        return -2;
    }
    
    my_user_id[0]=0;
    mysql_escape_string(my_user_id, user->userid, strlen(user->userid));
    
    sprintf(sql,"SELECT `board`, `user`, UNIX_TIMESTAMP(`time`), `status`, `manager`, `score`, `title`, `flag` FROM `board_user` WHERE `user`='%s' ORDER BY ", my_user_id);
    switch(sort) {
        case MEMBER_BOARD_SORT_TIME_DESC:
            strcpy(qtmp, " `time` DESC ");
            break;
        case MEMBER_BOARD_SORT_BOARD_ASC:
            strcpy(qtmp, " `board` ASC ");
            break;
        case MEMBER_BOARD_SORT_BOARD_DESC:
            strcpy(qtmp, " `board` DESC ");
            break;
        case MEMBER_BOARD_SORT_SCORE_DESC:
            strcpy(qtmp, " `score` DESC ");
            break;
        case MEMBER_BOARD_SORT_SCORE_ASC:
            strcpy(qtmp, " `score` ASC ");
            break;
        case MEMBER_BOARD_SORT_STATUS_ASC:
            strcpy(qtmp, " `status` ASC ");
            break;
        case MEMBER_BOARD_SORT_STATUS_DESC:
            strcpy(qtmp, " `status` DESC ");
            break;
        case MEMBER_BOARD_SORT_TIME_ASC:
        default:
            strcpy(qtmp, " `time` ASC ");
    }
    strcat(sql, qtmp);
    snprintf(qtmp, 99, " LIMIT %d,%d", start, num);
    strcat(sql, qtmp);
    
    if (mysql_real_query(&s, sql, strlen(sql))) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        mysql_close(&s);
        return -3;
    }
    res = mysql_store_result(&s);
    row = mysql_fetch_row(res);

    i=0;
    while (row != NULL) {
            const struct boardheader *board;
        if (!getbid(row[0], &board)||board->flag&BOARD_GROUP||!check_read_perm(user,board)) {
            delete_board_member_record(row[0], row[1]);
        } else if (!check_read_perm(getCurrentUser(),board)) {
        
        } else {
            i++;
            if (i>num)
                break;
                
            strncpy(member[i-1].board, board->filename, 32);
            strncpy(member[i-1].user, user->userid, IDLEN+1);
            member[i-1].time=atol(row[2]);
            member[i-1].status=atol(row[3]);
            strncpy(member[i-1].manager, row[4], IDLEN+1);
            member[i-1].score=atol(row[5]);
            member[i-1].title=atol(row[6]);
            member[i-1].flag=atol(row[7]);
            
            if (!valid_core_member(member[i-1].user)) {
                if (BOARD_MEMBER_STATUS_MANAGER==member[i-1].status)
                    member[i-1].status=BOARD_MEMBER_STATUS_NORMAL;
                member[i-1].flag=0;
            }
        }
        row = mysql_fetch_row(res);
    }
    mysql_free_result(res);

    mysql_close(&s);
    return i;
#endif
}
int get_board_members(const char *board, int status) {
#ifdef ENABLE_MEMBER_CACHE
    return count_board_members_cache(board, status);
#else
    MYSQL s;
    MYSQL_RES *res;
    MYSQL_ROW row;
    char sql[300];
    char my_board[STRLEN];
    int i;

    if (!board[0])
        return -1;
    
    my_board[0]=0;
    mysql_escape_string(my_board, board, strlen(board));
    mysql_init(&s);

    if (! my_connect_mysql(&s)) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        return -2;
    }

    sprintf(sql,"SELECT COUNT(*) FROM `board_user` WHERE `board`='%s'", my_board);

    if (mysql_real_query(&s, sql, strlen(sql))) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        mysql_close(&s);
        return -3;
    }
    res = mysql_store_result(&s);
    row = mysql_fetch_row(res);

    i=0;
    if (row != NULL) {
        i=atoi(row[0]);
    }
    mysql_free_result(res);

    mysql_close(&s);
    return i;
#endif
}
int get_member_boards(const char *user_id) {
#ifdef ENABLE_MEMBER_CACHE
    return count_member_boards_cache(user_id);
#else
    MYSQL s;
    MYSQL_RES *res;
    MYSQL_ROW row;
    char sql[300];
    char my_user_id[STRLEN];
    int i;

    if (!user_id[0])
        return -1;
        
    my_user_id[0]=0;
    mysql_escape_string(my_user_id, user_id, strlen(user_id));
    mysql_init(&s);

    if (! my_connect_mysql(&s)) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        return -2;
    }

    sprintf(sql,"SELECT COUNT(*) FROM `board_user` WHERE `user`='%s'", my_user_id);

    if (mysql_real_query(&s, sql, strlen(sql))) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        mysql_close(&s);
        return -3;
    }
    res = mysql_store_result(&s);
    row = mysql_fetch_row(res);

    i=0;
    if (row != NULL) {
        i=atoi(row[0]);
    }
    mysql_free_result(res);

    mysql_close(&s);
    return i;
#endif
}

int load_board_member_request(const char *name, struct board_member_config *mine) {
    const struct boardheader *board;
    
    if (0==strcmp(getSession()->currentuser->userid, "guest"))
        return -1;
        
    board=getbcache(name);
    if (0==board)
        return -2;
    if (board->flag&BOARD_GROUP)
        return -3;
    if (!haspostperm(getSession()->currentuser, board->filename))
        return -4;
    
    mine->approve=0;
    mine->max_members=0;
    mine->logins=getSession()->currentuser->numlogins;
    mine->posts=getSession()->currentuser->numposts;
#if defined(NEWSMTH) && !defined(SECONDSITE)
    mine->score=getSession()->currentuser->score_user;
    
    char buf[8];
    mine->level=uvaluetochar(buf, getSession()->currentuser);
#else
    mine->score=0;
    mine->level=0;
#endif    
    mine->board_posts=board_regenspecial(board->filename, DIR_MODE_AUTHOR, getSession()->currentuser->userid);
    mine->board_origins=board_regenspecial(board->filename, DIR_MODE_ORIGIN_AUTHOR, getSession()->currentuser->userid);
    mine->board_marks=board_regenspecial(board->filename, DIR_MODE_MARK_AUTHOR, getSession()->currentuser->userid);
    mine->board_digests=board_regenspecial(board->filename, DIR_MODE_DIGEST_AUTHOR, getSession()->currentuser->userid);
    
    return 0;
}    

int is_board_member(const char *name, const char *user_id, struct board_member *member) {
    int status;
    
    status=get_board_member(name, user_id, member);
    return (status==BOARD_MEMBER_STATUS_NORMAL||status==BOARD_MEMBER_STATUS_MANAGER)?1:0;
}

int is_board_member_manager(const char *name, const char *user_id, struct board_member *member) {
    if (get_board_member(name, user_id, member)!=BOARD_MEMBER_STATUS_MANAGER)
        return 0;
    if (!valid_core_member(user_id))
        return 0;
    return 1;
}
    
int set_board_member_status(const char *name, const char *user_id, int status) {
    const struct boardheader *board;
    struct board_member member;
    int old;
    
    char old_name[STRLEN], new_name[STRLEN], mailfile[MAXPATH], title[STRLEN], timebuf[STRLEN];
    time_t now;
    FILE *fn;
    
#ifndef ENABLE_MEMBER_CACHE    
    MYSQL s;
    char sql[200];
    char my_name[STRLEN];
    char my_user_id[STRLEN];
    char my_manager_id[STRLEN];
#endif
    char buf[1024];
    
    board=getbcache(name);
    if (0==board)
        return -1;
    if (board->flag&BOARD_GROUP)
        return -2;    
    if (!HAS_PERM(getSession()->currentuser,PERM_SYSOP)&&!chk_currBM(board->BM,getSession()->currentuser))    
        return -3;
        
    old=get_board_member(board->filename, user_id, &member);    
    if (old==status)
        return 0;
    
    switch(status) {
        case BOARD_MEMBER_STATUS_NONE:
        case BOARD_MEMBER_STATUS_CANDIDATE:
        case BOARD_MEMBER_STATUS_NORMAL:
        case BOARD_MEMBER_STATUS_MANAGER:
        case BOARD_MEMBER_STATUS_BLACK:
            break;
        default:
            return -7;
    }
    
    if (BOARD_MEMBER_STATUS_MANAGER==status&&!valid_core_member(user_id))
        status=BOARD_MEMBER_STATUS_NORMAL;

#ifdef ENABLE_MEMBER_CACHE        
    member.status=status;
    update_member_cache(&member);
#else    
    mysql_init(&s);
    if (!my_connect_mysql(&s)) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        return -5;
    }
    
    my_name[0]=0;
    my_user_id[0]=0;
    my_manager_id[0]=0;
    mysql_escape_string(my_name, board->filename, strlen(board->filename));
    mysql_escape_string(my_user_id, member.user, strlen(member.user));
    mysql_escape_string(my_manager_id, getSession()->currentuser->userid, strlen(getSession()->currentuser->userid));
    
    sprintf(sql,"UPDATE `board_user` SET `time`=`time`, `status`=%d, `manager`=\"%s\" WHERE `board`='%s' AND `user`='%s' LIMIT 1;", status, my_manager_id, my_name, my_user_id);

    if (mysql_real_query(&s, sql, strlen(sql))) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        mysql_close(&s);
        return -6;
    }

    mysql_close(&s);
#endif    
    get_member_status_name(old_name, old);
    get_member_status_name(new_name, status);
    
    sprintf(buf, "原状态: %s (%d)\n新状态: %s (%d)", old_name, old, new_name, status);
    board_member_log(&member, "设置驻版状态", buf);
    
    now=time(NULL);
    sprintf(title, "[%s] 调整驻版状态", board->filename);
    gettmpfilename(mailfile, "set_member_status.%d", getpid());
    fn=fopen(mailfile, "w+");
    fprintf(fn, "寄信人: %s \n", getSession()->currentuser->userid);
    fprintf(fn, "标  题: %s\n", title);
    fprintf(fn, "发信站: %s (%24.24s)\n", BBS_FULL_NAME, ctime_r(&now, timebuf));
    fprintf(fn, "来  源: %s \n", SHOW_USERIP(getSession()->currentuser, getSession()->fromhost));
    fprintf(fn, "\n");
    fprintf(fn, "您在 \033[4m%s\033[m 版的驻版状态由 \033[4m%s\033[m 调整为 \033[4m%s\033[m。\n", board->filename, old_name, new_name);
    fprintf(fn, "                              " NAME_BM ":\x1b[4m%s\x1b[m\n", getSession()->currentuser->userid);
    fprintf(fn, "                              %s\n", ctime_r(&now, timebuf));
    fclose(fn);
    
    mail_file(DELIVER, mailfile, member.user, title, 0, NULL);
    
    unlink(mailfile);
    
    return 0;
}    

int set_board_member_flag(struct board_member *member) {
#ifndef ENABLE_MEMBER_CACHE
    MYSQL s;
    char my_name[STRLEN];
    char my_user_id[STRLEN];
    char my_manager_id[STRLEN];
    char sql[200];
#endif    
    char buf[1024];
    const struct boardheader *board;
    int i, flag, o_set, n_set, sysop;
    struct board_member old;
    char path[PATHLEN], name[STRLEN];
    FILE *handle;
    time_t time;
    
    board=getbcache(member->board);
    if (0==board)
        return -1;
    if (board->flag&BOARD_GROUP)
        return -2;    
    if (!HAS_PERM(getSession()->currentuser,PERM_SYSOP)&&!chk_currBM(board->BM,getSession()->currentuser))    
        return -3;
    if (get_board_member(board->filename, member->user, &old)<0)
        return -4;
    if (old.status != BOARD_MEMBER_STATUS_NORMAL && old.status != BOARD_MEMBER_STATUS_MANAGER)
        return -5;
    if (!valid_core_member(member->user)) 
        member->flag=0;
    
    member->status=BOARD_MEMBER_STATUS_NORMAL;
    for (i=0;i<BMP_COUNT;i++) {
        flag=get_bmp_value(i);
        if (member->flag&flag) {
            member->status=BOARD_MEMBER_STATUS_MANAGER;
            break;
        }
    }
    
    if (old.status == member->status && old.flag == member->flag)
        return 0;
 
#ifdef ENABLE_MEMBER_CACHE 
    update_member_cache(member);
#else
    my_name[0]=0;
    my_user_id[0]=0;
    my_manager_id[0]=0;
    mysql_escape_string(my_name, member->board, strlen(member->board));
    mysql_escape_string(my_user_id, member->user, strlen(member->user));
    mysql_escape_string(my_manager_id, getSession()->currentuser->userid, strlen(getSession()->currentuser->userid));
    
    mysql_init(&s);
    if (!my_connect_mysql(&s)) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        return -6;
    }
    
    sprintf(sql,"UPDATE `board_user` SET `time`=`time`, `flag`=%d, `status`=%d, `manager`=\"%s\" WHERE `board`='%s' AND `user`='%s' LIMIT 1;", member->flag, member->status, my_manager_id, my_name, my_user_id);

    if (mysql_real_query(&s, sql, strlen(sql))) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        mysql_close(&s);
        return -7;
    }

    mysql_close(&s);
#endif
    
    gettmpfilename(path, "board.member.flag.log");
    if ((handle = fopen(path, "w")) != NULL) { 
        if ((HAS_PERM(getSession()->currentuser, PERM_SYSOP) || HAS_PERM(getSession()->currentuser, PERM_OBOARDS))
                && !chk_BM_instr(board->BM, getSession()->currentuser->userid))
            sysop = 1;
        else 
            sysop = 0;
        time=time(0);
        
        if (sysop)
        fprintf(handle, "  \033[1;33m版面\033[m: \033[1;32m%s\033[m                        \033[1;33m" NAME_BBS_CHINESE NAME_SYSOP_GROUP DENY_NAME_SYSOP "\033[m: \033[1;32m%s\033[m\n\n", board->filename, getSession()->currentuser->userid);
        else
        fprintf(handle, "  \033[1;33m版面\033[m: \033[1;32m%s\033[m                        \033[1;33m" NAME_BM "\033[m: \033[1;32m%s\033[m\n\n", board->filename, getSession()->currentuser->userid);
        fprintf(handle, "  \033[1;33m驻版用户\033[m: \033[1;31m%s\033[m\n\n", member->user);
        
        fprintf(handle, "\n  \033[1;33m原驻版身份\033[m: \033[1;32m%s\033[m\n", (old.status==BOARD_MEMBER_STATUS_MANAGER)?"核心驻版用户":"驻版用户");
        if (old.status == BOARD_MEMBER_STATUS_MANAGER) {
            for (i=0;i<BMP_COUNT;i++) {
                flag=get_bmp_value(i);
                get_bmp_name(name, flag);
                fprintf(handle, "      [%s] %s\n", (old.flag&flag)?"\033[1;32m*\033[m":" ", name);
            }
        }
        
        fprintf(handle, "\n  \033[1;33m新驻版身份\033[m: \033[1;32m%s\033[m\n", (member->status==BOARD_MEMBER_STATUS_MANAGER)?"核心驻版用户":"驻版用户");
        if (member->status == BOARD_MEMBER_STATUS_MANAGER) {
            for (i=0;i<BMP_COUNT;i++) {
                flag=get_bmp_value(i);
                get_bmp_name(name, flag);
                
                n_set=(member->flag&flag)?1:0;
                o_set=(old.flag&flag)?1:0;
                
                fprintf(handle, "      [%s] %s%s\033[m\n", (member->flag&flag)?"\033[1;32m*\033[m":" ", (n_set==o_set)?"":"\033[1;31m", name);
            }
        }
        
        if (sysop)
            fprintf(handle, "\n\n                            %s" NAME_SYSOP_GROUP DENY_NAME_SYSOP "：\x1b[4m%s\x1b[m\n", NAME_BBS_CHINESE, getSession()->currentuser->userid);
        else
            fprintf(handle, "\n\n                              " NAME_BM ":\x1b[4m%s\x1b[m\n", getSession()->currentuser->userid);
        
        fprintf(handle, "                              %s\n\n", ctime_r(&time, buf));
        fclose(handle);
        
        sprintf(buf, "调整 %s 的驻版权限", member->user);
        post_file(getSession()->currentuser, "", path, board->filename, buf, 0, 1, getSession());
        sprintf(buf, "调整 %s#%s 的驻版权限", member->user, board->filename);
        post_file(getSession()->currentuser, "", path, BOARD_MEMBER_LOG_BOARD, buf, 0, 2, getSession());        
        
        mail_file(DELIVER, path, member->user, buf, 0, NULL);
        
        unlink(path);
    } else {
        sprintf(buf, "权限: %d", member->flag);
        board_member_log(member, "设置驻版权限", buf);
    }
    
    update_board_member_manager_file(board);

#ifdef NEWSMTH
    set_member_manager_level(member->user);
#endif
    
    return 0;
}

#ifdef NEWSMTH
int set_member_manager_level(char *user_id) {
#ifndef ENABLE_MEMBER_CACHE
    MYSQL s;
    MYSQL_RES *res;
    MYSQL_ROW row;
    char my_user_id[STRLEN];
    char sql[512];
#endif
    struct userec *user;
    int count;
    
    if (!getuser(user_id, &user))
        return -1;

#ifdef ENABLE_MEMBER_CACHE        
    count=get_member_managers_cache(user->userid);
#else
    mysql_init(&s);
    if (! my_connect_mysql(&s)) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        return -2;
    }
    
    my_user_id[0]=0;
    mysql_escape_string(my_user_id, user->userid, strlen(user->userid));
    sprintf(sql,"SELECT COUNT(*) FROM `board_user` WHERE `user`='%s' AND `status`=%d", my_user_id, BOARD_MEMBER_STATUS_MANAGER);
    if (mysql_real_query(&s, sql, strlen(sql))) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        mysql_close(&s);
        return -3;
    }
    res = mysql_store_result(&s);
    row = mysql_fetch_row(res);
    count=0;
    if (row != NULL)
        count=atoi(row[0]);
    mysql_free_result(res);
    mysql_close(&s);
#endif

    if (HAS_PERM(user,PERM_MEMBER_MANAGER) && (count <= 0||!valid_core_member(user_id)))
        user->userlevel &= ~PERM_MEMBER_MANAGER;
    else if (!HAS_PERM(user,PERM_MEMBER_MANAGER) && count > 0)
        user->userlevel |= PERM_MEMBER_MANAGER;
    else
        return 0;

    return 0;
}
#endif

int update_board_member_manager_file(const struct boardheader *board) {
    struct stat st;
    char path[PATHLEN];
    
    setbfile(path, board->filename, BOARD_MEMBER_MANAGERS_FILE);
    if (stat(path, &st) >= 0)
        unlink(path);
        
    return 0;
}

int get_board_member_managers(const struct boardheader *board) {
#ifdef ENABLE_MEMBER_CACHE
    return count_member_board_managers_cache(board->filename);
#else
    MYSQL s;
    MYSQL_RES *res;
    MYSQL_ROW row;
    char sql[300];
    char my_board[STRLEN];
    int i;

    my_board[0]=0;
    mysql_escape_string(my_board, board->filename, strlen(board->filename));
    mysql_init(&s);

    if (! my_connect_mysql(&s)) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        return -1;
    }

    sprintf(sql,"SELECT COUNT(*) FROM `board_user` WHERE `board`='%s' AND `status`=%d", my_board, BOARD_MEMBER_STATUS_MANAGER);

    if (mysql_real_query(&s, sql, strlen(sql))) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        mysql_close(&s);
        return -2;
    }
    res = mysql_store_result(&s);
    row = mysql_fetch_row(res);

    i=0;
    if (row != NULL) {
        i=atoi(row[0]);
    }
    mysql_free_result(res);

    mysql_close(&s);
    return i;
#endif
}

int load_board_member_managers(const struct boardheader *board, struct board_member *members) {
#ifdef ENABLE_MEMBER_CACHE
    return load_board_member_managers_cache(board->filename, members);
#else
    MYSQL s;
    MYSQL_RES *res;
    MYSQL_ROW row;
    char sql[300];
    char my_board[STRLEN];
    int i;
    
    mysql_init(&s);
    if (!my_connect_mysql(&s)) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        return -1;
    }
    
    my_board[0]=0;
    mysql_escape_string(my_board, board->filename, strlen(board->filename));
    
    sprintf(sql,"SELECT `board`, `user`, UNIX_TIMESTAMP(`time`), `status`, `manager`, `score`, `title`, `flag` FROM `board_user` WHERE `board`='%s' AND `status`=%d ORDER BY `user` ASC", my_board, BOARD_MEMBER_STATUS_MANAGER);
    
    if (mysql_real_query(&s, sql, strlen(sql))) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        mysql_close(&s);
        return -2;
    }
    res = mysql_store_result(&s);
    row = mysql_fetch_row(res);

    i=0;
    while (row != NULL) {
        i++;
            
        strncpy(members[i-1].board, row[0], 32);
        strncpy(members[i-1].user, row[1], IDLEN+1);
        members[i-1].time=atol(row[2]);
        members[i-1].status=atol(row[3]);
        strncpy(members[i-1].manager, row[4], IDLEN+1);
        members[i-1].score=atol(row[5]);
        members[i-1].title=atol(row[6]);
        members[i-1].flag=atol(row[7]);

        if (!valid_core_member(members[i-1].user)) {
            members[i-1].status=BOARD_MEMBER_STATUS_NORMAL;
            members[i-1].flag=0;
        }
        
        row = mysql_fetch_row(res);
    }
    mysql_free_result(res);

    mysql_close(&s);
    return i;
#endif
}
int set_board_member_manager_file(const struct boardheader *board) {
    struct stat st;
    char path[PATHLEN], name[STRLEN];
    FILE *in;
    struct board_member *members=NULL;
    int total, i, j, k, flag;
    
    setbfile(path, board->filename, BOARD_MEMBER_MANAGERS_FILE);
    if (stat(path, &st) >= 0)
        return 0;
    
    if ((in=fopen(path, "w"))==NULL) 
        return -1;
    
    fprintf(in, "\033[1;33m版面名称\033[m: %s %s\n", board->filename, board->title+1);
    fprintf(in, "\033[1;33m版面版主\033[m: %s\n\n", board->BM);
    
    total=get_board_member_managers(board);
    if (total <= 0) {
        fprintf(in, "\033[1;31m本版尚无核心驻版用户\033[m");
        fclose(in);
        
        return 0;
    }
    
    members=(struct board_member *) malloc(sizeof(struct board_member) * total);
    bzero(members, sizeof(struct board_member) * total);
    
    if (load_board_member_managers(board, members)>0) {
        for (i=0; i<BMP_COUNT; i++) {
            flag=get_bmp_value(i);
            get_bmp_name(name, flag);
            fprintf(in, "\n具有 \033[1;31m%s\033[m 权限的用户\n", name);
            k=0;
            for (j=0; j<total; j++) {
                if (members[j].flag&flag) {
                    k++;
                    fprintf(in, " %3d. \033[1;32m%s\033[m\n", k, members[j].user);
                }
            }
            if (k==0)
            fprintf(in, "无\n");
        }
    }
    
    fprintf(in, "\n\n");
    
    free(members);
    members=NULL;
    
    fclose(in);
    
    return 0;
}

int set_board_member_score(struct board_member *member, int type, int score) {
#ifdef ENABLE_MEMBER_CACHE
    if (type==0)
        member->score = score;
    else if (type>0)
        member->score += score;
    else
        member->score -= score;
        
    if (member->score < 0)
        member->score = 0;
        
    return update_member_cache(member);
#else
    MYSQL s;
    char my_name[STRLEN];
    char my_user_id[STRLEN];
    char sql[200];
    
    my_name[0]=0;
    my_user_id[0]=0;
    mysql_escape_string(my_name, member->board, strlen(member->board));
    mysql_escape_string(my_user_id, member->user, strlen(member->user));
    
    if (0==type)
        sprintf(sql,"UPDATE `board_user` SET `time`=`time`, `score`=%d WHERE `board`='%s' AND `user`='%s' LIMIT 1;", score, my_name, my_user_id);
    else
        sprintf(sql,"UPDATE `board_user` SET `time`=`time`, `score`=`score`%s%d WHERE `board`='%s' AND `user`='%s' LIMIT 1;", ((type>0)?"+":"-"),score, my_name, my_user_id);

    mysql_init(&s);
    if (!my_connect_mysql(&s)) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        return -1;
    }
    
    if (mysql_real_query(&s, sql, strlen(sql))) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        mysql_close(&s);
        return -2;
    }

    mysql_close(&s);
    
    return 0;
#endif
}

typedef int member_board_article_cmp_func(const void *, const void *);

int member_board_article_cmp(fileheader *a, fileheader *b) {
    return a->posttime-b->posttime;
//    return get_posttime(b) - get_posttime(a);
}

char *set_member_board_article_dir(enum BBS_DIR_MODE mode, char *buf, const char *userid) {
    const char *prefix;
    
    switch (mode) {
        case DIR_MODE_DIGEST:
            prefix="BMA_DIGEST";
            break;
        case DIR_MODE_THREAD:
            prefix="BMA_THREAD";
            break;
        case DIR_MODE_MARK:
            prefix="BMA_MARK";
            break;
        case DIR_MODE_WEB_THREAD:
            prefix="BMA_THREAD";
            break;
        case DIR_MODE_NORMAL:
        default:
            prefix="BMA_DIR";
    }
    
    sethomefile(buf, userid, prefix);
    return buf;
}

int load_member_board_articles(char *path, enum BBS_DIR_MODE mode, const struct userec *user, int force) {
    int total, i, j, offset, bid;
    struct board_member *members;
    struct fileheader *posts;
    struct fileheader *board_posts;
    int post_size, post_total, post_read;
    char dir[PATHLEN];
    int board_total, board_offset, board_num;
    int fd;
    struct stat st;
    struct member_board_article article;
    struct boardheader *bh;
    
    switch (mode) {
        case DIR_MODE_NORMAL:
        case DIR_MODE_DIGEST:
        case DIR_MODE_THREAD:
        case DIR_MODE_MARK:
        case DIR_MODE_WEB_THREAD:
            break;
        default:
            return -5;
    }
    
    if (stat(path, &st) >= 0 && !force && st.st_mtime > (time(NULL) - MIN_MEMBER_BOARD_ARTICLE_STAT)) 
        return (st.st_size / sizeof(struct member_board_article));
    
    total=get_member_boards(user->userid);
    if (total<0)
        return -1;
    if (total==0)
        return -2;
    
    members=(struct board_member *) malloc(sizeof(struct board_member) * total);
    bzero(members, sizeof(struct board_member) * total);
    total=load_member_boards(user->userid, members, 0, 0, total);
    if (total <= 0) {
        free(members);
        members=NULL;
        return -3;
    }
    
    offset=10;
    while (total>offset) 
        offset *= 10;
    
    post_size=sizeof(struct fileheader);
    posts=(struct fileheader *) malloc(post_size*(MAX_MEMBER_BOARD_ARTICLES*total));
    board_posts=(struct fileheader *) malloc(post_size*MAX_MEMBER_BOARD_ARTICLES);
    memset(posts, 0, post_size * (MAX_MEMBER_BOARD_ARTICLES*total));
    
    post_total=0;
    for (i=0; i<total;i++) {
        bid=getbid(members[i].board, NULL);
        if (!bid) continue;
        
        setbdir(mode,dir,members[i].board);
        board_total=get_num_records(dir, post_size);
        if (board_total > MAX_MEMBER_BOARD_ARTICLES) {
            board_offset=board_total-MAX_MEMBER_BOARD_ARTICLES+1;
            board_num=MAX_MEMBER_BOARD_ARTICLES;
        } else {
            board_offset=1;
            board_num=board_total;
        }
        memset(board_posts, 0, post_size * MAX_MEMBER_BOARD_ARTICLES);
        post_read=get_records(dir, board_posts, post_size, board_offset, board_num);
        if (post_read>0) {
            for (j=0;j<post_read;j++) {
                strncpy(posts[j+post_total].filename, board_posts[j].filename, FILENAME_LEN);
                posts[j+post_total].id=board_posts[j].id;
                posts[j+post_total].groupid=board_posts[j].groupid;
                posts[j+post_total].reid=board_posts[j].reid;
                strncpy(posts[j+post_total].owner, board_posts[j].owner, OWNER_LEN);
                posts[j+post_total].eff_size=board_posts[j].eff_size;
                posts[j+post_total].posttime=board_posts[j].posttime;
                posts[j+post_total].attachment=board_posts[j].attachment;
                strncpy(posts[j+post_total].title, board_posts[j].title, ARTICLE_TITLE_LEN);
                posts[j+post_total].accessed[0]=board_posts[j].accessed[0];
                posts[j+post_total].accessed[1]=board_posts[j].accessed[1];
                posts[j+post_total].o_id=board_posts[j].id*offset+i;
                posts[j+post_total].o_reid=board_posts[j].reid*offset+i;
                posts[j+post_total].o_groupid=board_posts[j].groupid*offset+i;
                posts[j+post_total].o_bid=bid;
            }
            post_total += post_read;
        }
    }
    
    free(board_posts);
    board_posts=NULL;

    if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0664))==-1) { 
        free(members);
        free(posts);
        members=NULL;
        posts=NULL;
        return -4;
    }
    
    writew_lock(fd, 0, SEEK_SET, 0);
    lseek(fd, 0, SEEK_END);

    i=0;
    j=0;
    memset(&article, 0, sizeof(struct member_board_article));    
    if (post_total > 0) {
        
        board_posts=(struct fileheader *) malloc(post_size*post_total);
        memset(board_posts, 0, post_size * post_total);
        
        while(j<post_total) {
            if (!posts[i].o_bid) break;
            strncpy(board_posts[j].filename, posts[i].filename, FILENAME_LEN);
                        board_posts[j].id=posts[i].id;
                        board_posts[j].groupid=posts[i].groupid;
                        board_posts[j].reid=posts[i].reid;
                        strncpy(board_posts[j].owner, posts[i].owner, OWNER_LEN);
                        board_posts[j].eff_size=posts[i].eff_size;
                        board_posts[j].posttime=posts[i].posttime;
                        board_posts[j].attachment=posts[i].attachment;
                        strncpy(board_posts[j].title, posts[i].title, ARTICLE_TITLE_LEN);
                        board_posts[j].accessed[0]=posts[i].accessed[0];
                        board_posts[j].accessed[1]=posts[i].accessed[1];
                        board_posts[j].o_id=posts[i].o_id;
                        board_posts[j].o_reid=posts[i].o_reid;
                        board_posts[j].o_groupid=posts[i].o_groupid;
                        board_posts[j].o_bid=posts[i].o_bid;    
            j++;
            i++;
        }

        qsort(board_posts, post_total, post_size, (member_board_article_cmp_func *) member_board_article_cmp);

        if (post_total <= MAX_MEMBER_BOARD_ARTICLES)
            i=0;
        else
            i=post_total-MAX_MEMBER_BOARD_ARTICLES;
        
        while (i<post_total) {    
            if (!board_posts[i].o_bid) break;
            bh=(struct boardheader *)getboard(board_posts[i].o_bid);
            if (!bh) { i++; continue; }
            
            strncpy(article.board, bh->filename, STRLEN);
            strncpy(article.filename, board_posts[i].filename, FILENAME_LEN);
            article.id=board_posts[i].id;
            article.groupid=board_posts[i].groupid;
            article.reid=board_posts[i].reid;
            article.s_id=board_posts[i].o_id;
            article.s_groupid=board_posts[i].o_groupid;
            article.s_reid=board_posts[i].o_reid;
            strncpy(article.owner, board_posts[i].owner, OWNER_LEN);
#ifdef ENABLE_BOARD_MEMBER
            if (!member_read_perm(bh, &board_posts[i], (struct userec*)user))
                strncpy(article.owner, MEMBER_POST_OWNER, OWNER_LEN);
#endif
            article.eff_size=board_posts[i].eff_size;
            article.posttime=board_posts[i].posttime;
            article.attachment=board_posts[i].attachment;
            strncpy(article.title, board_posts[i].title, ARTICLE_TITLE_LEN);
            article.accessed[0]=board_posts[i].accessed[0];
            article.accessed[1]=board_posts[i].accessed[1];
            safewrite(fd, &article, sizeof(struct member_board_article));
            i++;
        }

        free(board_posts);
        board_posts=NULL;
    }

    un_lock(fd, 0, SEEK_SET, 0);
    close(fd);
    
    free(members);
    free(posts);
    members=NULL;
    posts=NULL;
    
    return j;
}

int flush_member_board_articles(int mode, const struct userec *user, int force) {
    char path[PATHLEN];
    struct stat st;
    
    set_member_board_article_dir(mode, path, user->userid);
    if (!force && stat(path, &st) >= 0 && st.st_mtime > (time(NULL) - MIN_MEMBER_BOARD_ARTICLE_STAT)) 
        return 0;
    
    load_member_board_articles(path, mode, user, force);
    return 1;
}

int get_board_member_titles(const char *board) {
#ifdef ENABLE_MEMBER_CACHE
    return count_board_titles_cache(board);
#else
    MYSQL s;
    MYSQL_RES *res;
    MYSQL_ROW row;
    char sql[300];
    char my_board[STRLEN];
    int i;

    if (!board[0])
        return -1;
    
    my_board[0]=0;
    mysql_escape_string(my_board, board, strlen(board));
    mysql_init(&s);

    if (! my_connect_mysql(&s)) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        return -2;
    }

    sprintf(sql,"SELECT COUNT(*) FROM `board_title` WHERE `board`='%s'", my_board);

    if (mysql_real_query(&s, sql, strlen(sql))) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        mysql_close(&s);
        return -3;
    }
    res = mysql_store_result(&s);
    row = mysql_fetch_row(res);

    i=0;
    if (row != NULL) {
        i=atoi(row[0]);
    }
    mysql_free_result(res);

    mysql_close(&s);
    return i;
#endif
}

int load_board_member_titles(const char *board, struct board_member_title *titles) {
#ifdef ENABLE_MEMBER_CACHE
    return load_board_titles_cache(board, titles);
#else
    MYSQL s;
    MYSQL_RES *res;
    MYSQL_ROW row;
    char sql[300];
    char my_board[STRLEN];
    int i;
    
    if (!board[0])
        return -1;
    
    mysql_init(&s);
    if (!my_connect_mysql(&s)) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        return -2;
    }
    
    my_board[0]=0;
    mysql_escape_string(my_board, board, strlen(board));
    
    sprintf(sql,"SELECT `id`, `board`, `name`, `serial`, `flag` FROM `board_title` WHERE `board`='%s' ORDER BY `serial` ASC, `id` ASC", my_board);
    
    if (mysql_real_query(&s, sql, strlen(sql))) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        mysql_close(&s);
        return -3;
    }
    res = mysql_store_result(&s);
    row = mysql_fetch_row(res);

    i=0;
    while (row != NULL) {
        i++;
        
        titles[i-1].id=atol(row[0]);
        strncpy(titles[i-1].board, row[1], STRLEN-2);
        strncpy(titles[i-1].name, row[2], STRLEN-2);
        titles[i-1].serial=atol(row[3]);
        titles[i-1].flag=atol(row[4]);        
        
        row = mysql_fetch_row(res);
    }
    mysql_free_result(res);

    mysql_close(&s);
    return i;
#endif    
}

int get_board_member_title(const char *board, int id, struct board_member_title *title) {
#ifdef ENABLE_MEMBER_CACHE
    return get_board_member_title_cache(board, id, title);
#else
    MYSQL s;
    MYSQL_RES *res;
    MYSQL_ROW row;
    char sql[500];
    char my_board[STRLEN];
    int not_found=1;
    
    if (!board[0])
        return -1;
    if (id<=0)
        return -2;    
    
    mysql_init(&s);
    if (!my_connect_mysql(&s)) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        return -3;
    }
    
    my_board[0]=0;
    mysql_escape_string(my_board, board, strlen(board));
    
    sprintf(sql,"SELECT `id`, `board`, `name`, `serial`, `flag` FROM `board_title` WHERE `id`=%d AND `board`='%s'", id, my_board);
    
    if (mysql_real_query(&s, sql, strlen(sql))) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        mysql_close(&s);
        return -4;
    }
    res = mysql_store_result(&s);
    row = mysql_fetch_row(res);
    
    if (NULL != row) {
        not_found=0;
            
        if (NULL != title) {
            bzero(title, sizeof(struct board_member_title));

            title->id=atol(row[0]);
            strncpy(title->board, row[1], STRLEN-2);
            strncpy(title->name, row[2], STRLEN-2);
            title->serial=atol(row[3]);
            title->flag=atol(row[4]);
        }        
    }

    mysql_free_result(res);
    mysql_close(&s);

    if (not_found)
        return 0;
        
    return 1;
#endif
}

int query_board_member_title(const char *board, char *name, struct board_member_title *title) {
#ifdef ENABLE_MEMBER_CACHE
    return query_board_member_title_cache(board, name, title);
#else
    MYSQL s;
    MYSQL_RES *res;
    MYSQL_ROW row;
    char sql[500];
    char my_board[STRLEN];
    char my_name[STRLEN];    
    int not_found=1;
    
    if (!board[0])
        return -1;
    if (!name[0])
        return -2;    
    
    mysql_init(&s);
    if (!my_connect_mysql(&s)) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        return -3;
    }
    
    my_board[0]=0;
    my_name[0]=0;
    mysql_escape_string(my_board, board, strlen(board));
    mysql_escape_string(my_name, name, strlen(name));
    
    sprintf(sql,"SELECT `id`, `board`, `name`, `serial`, `flag` FROM `board_title` WHERE `name`='%s' AND `board`='%s'", my_name, my_board);
    
    if (mysql_real_query(&s, sql, strlen(sql))) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        mysql_close(&s);
        return -4;
    }
    res = mysql_store_result(&s);
    row = mysql_fetch_row(res);
    
    if (NULL != row) {
        not_found=0;
            
        if (NULL != title) {
            bzero(title, sizeof(struct board_member_title));

            title->id=atol(row[0]);
            strncpy(title->board, row[1], STRLEN-2);
            strncpy(title->name, row[2], STRLEN-2);
            title->serial=atol(row[3]);
            title->flag=atol(row[4]);
        }        
    }

    mysql_free_result(res);
    mysql_close(&s);

    if (not_found)
        return 0;
        
    return 1;
#endif
}

int set_board_member_title(struct board_member *member) {
#ifndef ENABLE_MEMBER_CACHE
    MYSQL s;
    char my_name[STRLEN];
    char my_user_id[STRLEN];
    char my_manager_id[STRLEN];
    char sql[200];
#endif
    char buf[1024];
    const struct boardheader *board;
    int sysop;
    struct board_member old;
    struct board_member_title old_title;
    struct board_member_title new_title;
    char path[PATHLEN];
    FILE *handle;
    time_t time;
    
    board=getbcache(member->board);
    if (0==board)
        return -1;
    if (board->flag&BOARD_GROUP)
        return -2;    
    if (!HAS_PERM(getSession()->currentuser,PERM_SYSOP)&&!chk_currBM(board->BM,getSession()->currentuser))    
        return -3;
    if (get_board_member(board->filename, member->user, &old)<0)
        return -4;
    if (old.status != BOARD_MEMBER_STATUS_NORMAL && old.status != BOARD_MEMBER_STATUS_MANAGER)
        return -5;
    
    if (old.title != 0 && get_board_member_title(board->filename, old.title, &old_title)<=0)
        old.title=0;
        
    if (member->title !=0 && get_board_member_title(board->filename, member->title, &new_title)<=0)
        return -6;
    
    if (old.title == member->title)
        return 0;
#ifdef ENABLE_MEMBER_CACHE        
    set_board_member_title_cache(member);
#else
    my_name[0]=0;
    my_user_id[0]=0;
    my_manager_id[0]=0;
    mysql_escape_string(my_name, member->board, strlen(member->board));
    mysql_escape_string(my_user_id, member->user, strlen(member->user));
    mysql_escape_string(my_manager_id, getSession()->currentuser->userid, strlen(getSession()->currentuser->userid));
    
    mysql_init(&s);
    if (!my_connect_mysql(&s)) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        return -7;
    }
    
    sprintf(sql,"UPDATE `board_user` SET `time`=`time`, `title`=%d, `manager`=\"%s\" WHERE `board`='%s' AND `user`='%s' LIMIT 1;", member->title, my_manager_id, my_name, my_user_id);

    if (mysql_real_query(&s, sql, strlen(sql))) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        mysql_close(&s);
        return -8;
    }

    mysql_close(&s);
#endif    
    gettmpfilename(path, "board.member.title.log");
    if ((handle = fopen(path, "w")) != NULL) { 
        if ((HAS_PERM(getSession()->currentuser, PERM_SYSOP) || HAS_PERM(getSession()->currentuser, PERM_OBOARDS))
                && !chk_BM_instr(board->BM, getSession()->currentuser->userid))
            sysop = 1;
        else 
            sysop = 0;
        time=time(0);
        
        if (sysop)
        fprintf(handle, "  \033[1;33m版面\033[m: \033[1;32m%s\033[m                        \033[1;33m" NAME_BBS_CHINESE NAME_SYSOP_GROUP DENY_NAME_SYSOP "\033[m: \033[1;32m%s\033[m\n\n", board->filename, getSession()->currentuser->userid);
        else
        fprintf(handle, "  \033[1;33m版面\033[m: \033[1;32m%s\033[m                        \033[1;33m" NAME_BM "\033[m: \033[1;32m%s\033[m\n\n", board->filename, getSession()->currentuser->userid);
        fprintf(handle, "  \033[1;33m驻版用户\033[m: \033[1;31m%s\033[m\n\n", member->user);
        
        fprintf(handle, "\n  \033[1;33m原驻版称号\033[m: \033[1;32m%s\033[m\n", (old.title==0)?"无":old_title.name);
        fprintf(handle, "\n  \033[1;33m新驻版称号\033[m: \033[1;32m%s\033[m\n", (member->title==0)?"无":new_title.name);
        
        if (sysop)
            fprintf(handle, "\n\n                            %s" NAME_SYSOP_GROUP DENY_NAME_SYSOP "：\x1b[4m%s\x1b[m\n", NAME_BBS_CHINESE, getSession()->currentuser->userid);
        else
            fprintf(handle, "\n\n                              " NAME_BM ":\x1b[4m%s\x1b[m\n", getSession()->currentuser->userid);
        
        fprintf(handle, "                              %s\n\n", ctime_r(&time, buf));
        fclose(handle);
        
        sprintf(buf, "调整 %s 的驻版称号", member->user);
        post_file(getSession()->currentuser, "", path, board->filename, buf, 0, 1, getSession());
        sprintf(buf, "调整 %s#%s 的驻版称号", member->user, board->filename);
        post_file(getSession()->currentuser, "", path, BOARD_MEMBER_LOG_BOARD, buf, 0, 2, getSession());        
        
        mail_file(DELIVER, path, member->user, buf, 0, NULL);
        
        unlink(path);
    } else {
        sprintf(buf, "称号: %d", member->title);
        board_member_log(member, "设置驻版称号", buf);
    }
        
    return 0;
}

int create_board_member_title(const char *board_name, char *name, int serial) {
#ifdef ENABLE_MEMBER_CACHE
    struct board_member_title title;
#else
    MYSQL s;
    char my_name[STRLEN];
    char my_board[STRLEN];
    char sql[500];
#endif
    char buf[300];
    const struct boardheader *board;
    
    board=getbcache(board_name);
    if (0==board)
        return -1;
    if (board->flag&BOARD_GROUP)
        return -2;    
    if (!HAS_PERM(getSession()->currentuser,PERM_SYSOP)&&!chk_currBM(board->BM,getSession()->currentuser))    
        return -3;
    
    if (!name[0])
        return -4;
    
    if (query_board_member_title(board->filename, name, NULL) != 0)
        return -5;
#ifdef ENABLE_MEMBER_CACHE    
    bzero(&title, sizeof(struct board_member_title));
    strncpy(title.board, board->filename, STRLEN);
    strncpy(title.name, name, STRLEN);
    title.serial=serial;
    title.flag=0;
    if (add_member_title(&title)<0)
        return -6;
#else
    my_name[0]=0;
    my_board[0]=0;
    mysql_escape_string(my_name, name, strlen(name));
    mysql_escape_string(my_board, board->filename, strlen(board->filename));
    
    mysql_init(&s);
    if (!my_connect_mysql(&s)) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        return -6;
    }
    sprintf(sql, "INSERT INTO `board_title` (`board`, `name`, `serial`, `flag`) VALUES (\"%s\", \"%s\", %d, %d);", my_board, my_name, serial, 0);
    if (mysql_real_query(&s, sql, strlen(sql))) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        mysql_close(&s);
        return -7;
    }

    mysql_close(&s);
#endif

    sprintf(buf, "增加驻版称号#%s: %s", board->filename, name);
    board_member_log(NULL, buf, buf);
    
    return 0;
}

int remove_board_member_title(struct board_member_title *title) {
#ifndef ENABLE_MEMBER_CACHE
    MYSQL s;
    char my_board[STRLEN], sql[500];
#endif
    const struct boardheader *board;
    char buf[300];
    
    board=getbcache(title->board);
    if (0==board)
        return -1;
    if (board->flag&BOARD_GROUP)
        return -2;    
    if (!HAS_PERM(getSession()->currentuser,PERM_SYSOP)&&!chk_currBM(board->BM,getSession()->currentuser))    
        return -3;
        
    if (title->id <= 0)
        return -4;

#ifdef ENABLE_MEMBER_CACHE
    if (remove_member_title(title->id)<0)
        return -5;
#else        
    my_board[0]=0;    
    mysql_escape_string(my_board, board->filename, strlen(board->filename));    
    sprintf(sql, "DELETE FROM `board_title` WHERE `id`=%d AND `board`='%s';", title->id, my_board);
    mysql_init(&s);
    if (!my_connect_mysql(&s)) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        return -5;
    }
    
    if (mysql_real_query(&s, sql, strlen(sql))) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        mysql_close(&s);
        return -6;
    }
    
    sprintf(sql, "UPDATE `board_user` SET `time`=`time`, `title`=0 WHERE `title`=%d AND `board`='%s';", title->id, my_board);
    if (mysql_real_query(&s, sql, strlen(sql))) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        mysql_close(&s);
        return -7;
    }
    
    mysql_close(&s);
#endif    
    sprintf(buf, "删除驻版称号#%s: [%d]%s", board->filename, title->id, title->name);
    board_member_log(NULL, buf, buf);
    
    return 0;
}

int modify_board_member_title(struct board_member_title *title) {
#ifndef ENABLE_MEMBER_CACHE
    MYSQL s;
    char my_board[STRLEN];
    char my_name[STRLEN];
    char sql[500];
#endif
    const struct boardheader *board;
    char buf[300];
    
    board=getbcache(title->board);
    if (0==board)
        return -1;
    if (board->flag&BOARD_GROUP)
        return -2;    
    if (!HAS_PERM(getSession()->currentuser,PERM_SYSOP)&&!chk_currBM(board->BM,getSession()->currentuser))    
        return -3;
        
    if (title->id <= 0)
        return -4;

    if (!title->name[0])
        return -5;
#ifdef ENABLE_MEMBER_CACHE    
    if (set_member_title_cache(title)<0)
        return -6;
#else    
    my_board[0]=0;
    my_name[0]=0;
    mysql_escape_string(my_board, board->filename, strlen(board->filename));    
    mysql_escape_string(my_name, title->name, strlen(title->name));
    
    mysql_init(&s);
    if (!my_connect_mysql(&s)) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        return -6;
    }

    sprintf(sql, "UPDATE `board_title` SET `name`=\"%s\", serial=%d, flag=%d WHERE `id`=%d AND `board`='%s';", my_name, title->serial, title->flag, title->id, my_board);
    if (mysql_real_query(&s, sql, strlen(sql))) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        mysql_close(&s);
        return -7;
    }
    
    mysql_close(&s);
#endif    
    sprintf(buf, "更改驻版称号#%s: [%d]%s", board->filename, title->id, title->name);
    board_member_log(NULL, buf, buf);
    
    return 0;
}
    
int valid_core_member(const char *user_id)
{
    struct userec *user;
    
    if (!getuser(user_id, &user))
        return 0;
    if (HAS_PERM(user, PERM_NOZAP))
        return 0;
    
    return 1;
}    

/*
  检查用户是否可读驻版可读版面的帖子
  1: 可读
  0: 不可读
  -1: 非驻版可读的版面
  */
int member_read_perm(const struct boardheader *bh, struct fileheader *fh, struct userec *user) {
    struct board_member member;
    struct fileheader orig;
    int status;

    if (!(bh->flag & BOARD_MEMBER_READ))
        return -1;
    if (chk_currBM(bh->BM, user))
        return 1;
    if (fh!=NULL && (POSTFILE_BASENAME(fh->filename)[0]=='Z' || is_top(fh, bh->filename) || isowner(user, fh))) /* 置顶或作者 */
        return 1;
    bzero(&orig, sizeof(struct fileheader));
    if (fh!=NULL && fh->id!=fh->groupid && get_ent_from_id_ext(DIR_MODE_NORMAL, fh->groupid, bh->filename, &orig)>0 && isowner(user, &orig)) /* 楼主 */
        return 1;

    bzero(&member, sizeof(struct board_member));
    status = get_board_member(bh->filename, user->userid, &member);
    if (status==BOARD_MEMBER_STATUS_NORMAL || status==BOARD_MEMBER_STATUS_MANAGER)
        return 1;

    return 0;
}

/*
  检察用户是否有驻版可写版面的写权限
  1: 可写
  0: 不可写
  -1:非驻版可写版面
  */
int member_post_perm(const struct boardheader *bh, struct userec *user) {
    struct board_member member;
    int status;

    if (!(bh->flag & BOARD_MEMBER_POST))
        return -1;
    if (chk_currBM(bh->BM, user))
        return 1;

    bzero(&member, sizeof(struct board_member));
    status = get_board_member(bh->filename, user->userid, &member);
    if (status==BOARD_MEMBER_STATUS_NORMAL || status==BOARD_MEMBER_STATUS_MANAGER)
        return 1;

    return 0;
}
#endif
#endif 
