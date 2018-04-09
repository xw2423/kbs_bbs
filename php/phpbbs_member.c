#include "php_kbs_bbs.h"

#ifdef ENABLE_BOARD_MEMBER
void bbs_make_board_member_config_array(zval *array, struct board_member_config *config) {
    add_assoc_long(array, "approve", config->approve);
    add_assoc_long(array, "max_members", config->max_members);
    add_assoc_long(array, "logins", config->logins);
    add_assoc_long(array, "posts", config->posts);
    add_assoc_long(array, "score", config->score);
    add_assoc_long(array, "level", config->level);
    add_assoc_long(array, "board_posts", config->board_posts);
    add_assoc_long(array, "board_origins", config->board_origins);
    add_assoc_long(array, "board_marks", config->board_marks);
    add_assoc_long(array, "board_digests", config->board_digests);
}
void bbs_make_board_member_array(zval *array, struct board_member *member) {
    add_assoc_string(array, "board", member->board, 1);
    add_assoc_string(array, "user", member->user, 1);
	add_assoc_long(array, "title", member->title);
    add_assoc_long(array, "time", member->time);
	add_assoc_long(array, "status", member->status);
	add_assoc_string(array, "manager", member->manager, 1);
	add_assoc_long(array, "score", member->score);
    add_assoc_long(array, "flag", member->flag);
}
void bbs_make_board_member_article_array(zval *array, struct member_board_article *article) {
	add_assoc_string(array, "board", article->board, 1);
	add_assoc_string(array, "filename", article->filename, 1);
	add_assoc_long(array, "id", article->id);
	add_assoc_long(array, "groupid", article->groupid);
	add_assoc_long(array, "reid", article->reid);
	add_assoc_long(array, "s_id", article->s_id);
	add_assoc_long(array, "s_groupid", article->s_groupid);
	add_assoc_long(array, "s_reid", article->s_reid);
	add_assoc_string(array, "owner", article->owner, 1);
	add_assoc_long(array, "eff_size", article->eff_size);
	add_assoc_long(array, "posttime", article->posttime);
	add_assoc_long(array, "attachment", article->attachment);
	add_assoc_string(array, "title", article->title, 1);
	add_assoc_long(array, "accessed_0", article->accessed[0] & 0xff);
	add_assoc_long(array, "accessed_1", article->accessed[1] & 0xff);
	add_assoc_long(array, "flag", article->flag);
}
void bbs_make_board_member_title_array(zval *array, struct board_member_title *title) {
	add_assoc_long(array, "id", title->id);
	add_assoc_string(array, "board", title->board, 1);
	add_assoc_string(array, "name", title->name, 1);
	add_assoc_long(array, "serial", title->serial);
	add_assoc_long(array, "flag", title->flag);
}
/**
  * bbs_load_board_member_config(string board_name, array &config);
  */
PHP_FUNCTION(bbs_load_board_member_config)
{
    char *name;
	int name_len;
	zval *array;
	struct board_member_config config;

	if (ZEND_NUM_ARGS()!=2 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz", &name, &name_len, &array)==FAILURE)
        WRONG_PARAM_COUNT;

    if (!PZVAL_IS_REF(array)) {
        zend_error(E_WARNING, "Parameter wasn't passed by reference");
        RETURN_FALSE;
    }
	
	if (0==strcmp(getCurrentUser()->userid, "guest"))
		RETURN_FALSE;
	
	if (load_board_member_config(name, &config)<0)
	    RETURN_FALSE;
	
	if (array_init(array) != SUCCESS)
	    RETURN_FALSE;
		
	bbs_make_board_member_config_array(array, &config);
	
	RETURN_TRUE;
}
/**
  * bbs_save_board_member_config(
        string board_name,
		long approve,
		long max_members,
		long logins,
		long posts,
		long score,
		long level,
		long board_posts,
		long board_origins,
		long board_marks,
		long board_digests
    )
  *    int approve;
	int max_members;
	
	int logins;
	int posts;
	int score;
	int level;
	
	int board_posts;
	int board_origins;
	int board_marks;
	int board_digests;
  */
PHP_FUNCTION(bbs_save_board_member_config)
{
    char *name;
	int name_len;
	long approve, max_members, logins, posts, score, level, board_posts, board_origins, board_marks, board_digests;
	struct board_member_config config;
	
	if (ZEND_NUM_ARGS()!=11 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sllllllllll", &name, &name_len, &approve, &max_members, &logins, &posts, &score, &level, &board_posts, &board_origins, &board_marks, &board_digests)==FAILURE)
        WRONG_PARAM_COUNT;
	
	if (0==strcmp(getCurrentUser()->userid, "guest"))
		RETURN_FALSE;
		
	if (load_board_member_config(name, &config)<0)
	    RETURN_FALSE;
		
	config.approve=(approve>0)?1:0;
	if (HAS_PERM(getSession()->currentuser,PERM_SYSOP)) 
	    config.max_members=(max_members>0)?max_members:0;
	config.logins=(logins>0)?logins:0;
	config.posts=(posts>0)?posts:0;
	config.score=(score>0)?score:0;
	config.level=(level>0)?level:0;
	config.board_posts=(board_posts>0)?board_posts:0;
	config.board_origins=(board_origins>0)?board_origins:0;
	config.board_marks=(board_marks>0)?board_marks:0;
	config.board_digests=(board_digests>0)?board_digests:0;
	
	if (save_board_member_config(name, &config)<0)
	    RETURN_FALSE;
	RETURN_TRUE;
}

/**
  * bbs_join_board_member(string board_name);
  */
PHP_FUNCTION(bbs_join_board_member)
{
    char *name;
	int name_len;
	
	if (ZEND_NUM_ARGS()!=1 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &name, &name_len)==FAILURE)
        WRONG_PARAM_COUNT;
		
	if (0==strcmp(getCurrentUser()->userid, "guest"))
		RETURN_LONG(-1);
		
	RETURN_LONG(join_board_member(name));	
}

PHP_FUNCTION(bbs_leave_board_member)
{
    char *name;
	int name_len;
	
	if (ZEND_NUM_ARGS()!=1 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &name, &name_len)==FAILURE)
        WRONG_PARAM_COUNT;
		
	if (0==strcmp(getCurrentUser()->userid, "guest"))
		RETURN_LONG(-1);
		
	RETURN_LONG(leave_board_member(name));	
}

PHP_FUNCTION(bbs_set_board_member_status)
{
	char *name, *id;
	int name_len, id_len;
	long status;
    const struct boardheader *board;
	struct userec *user;
	
	if (ZEND_NUM_ARGS()!=3 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ssl", &name, &name_len, &id, &id_len, &status)==FAILURE)
        WRONG_PARAM_COUNT;
		
	if (0==strcmp(getCurrentUser()->userid, "guest"))
		RETURN_LONG(-1);
		
	if (!getbid(name, &board)||board->flag&BOARD_GROUP)
		RETURN_LONG(-1);
		
	if (!check_read_perm(getCurrentUser(), board))
		RETURN_LONG(-2);

	if (id_len>IDLEN)
        RETURN_LONG(-3);
		
    if (!getuser(id, &user))
        RETURN_LONG(-3);
		
	RETURN_LONG(set_board_member_status(board->filename, user->userid, status));	
}

PHP_FUNCTION(bbs_remove_board_member)
{
    char *name, *id;
	int name_len, id_len;
	const struct boardheader *board;
	struct userec *user;
	
	if (ZEND_NUM_ARGS()!=2 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &name, &name_len, &id, &id_len)==FAILURE)
        WRONG_PARAM_COUNT;
		
	if (0==strcmp(getCurrentUser()->userid, "guest"))
		RETURN_LONG(-1);
		
	if (!getbid(name, &board)||board->flag&BOARD_GROUP)
		RETURN_LONG(-1);
		
	if (!check_read_perm(getCurrentUser(), board))
		RETURN_LONG(-2);

	if (id_len>IDLEN)
        RETURN_LONG(-3);
		
    if (!getuser(id, &user))
        RETURN_LONG(-3);
		
	RETURN_LONG(remove_board_member(board->filename, user->userid));	
}

PHP_FUNCTION(bbs_get_board_member)
{
    char *name, *id;
	int name_len, id_len;
	const struct boardheader *board;
	struct userec *user;
	struct board_member member;
	zval *status;
	int ret;
	
	if (ZEND_NUM_ARGS()!=3 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ssz", &name, &name_len, &id, &id_len, &status)==FAILURE)
        WRONG_PARAM_COUNT;
	
	if (!PZVAL_IS_REF(status)) {
        zend_error(E_WARNING, "Parameter wasn't passed by reference");
        RETURN_LONG(-1);
    }
	
	if (id_len>IDLEN)
        RETURN_LONG(-1);
		
    if (!getuser(id, &user))
        RETURN_LONG(-1);
	
	if (!getbid(name, &board)||board->flag&BOARD_GROUP)
		RETURN_LONG(-2);
		
	if (!check_read_perm(user, board))
		RETURN_LONG(-3);
		
	ret=get_board_member(board->filename, user->userid, &member);
	if (ret<0)
		RETURN_LONG(-4);
		
	bbs_make_board_member_array(status, &member);
	RETURN_LONG(ret);
}

PHP_FUNCTION(bbs_load_board_members)
{
	char *name;
	int name_len, ret, i;
	long sort, start, count;
	zval *list, *element;
	const struct boardheader *board;
	struct board_member *members = NULL;
	
	if (ZEND_NUM_ARGS()!=5 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "slllz", &name, &name_len, &sort, &start, &count, &list)==FAILURE)
        WRONG_PARAM_COUNT;
	
	if (!PZVAL_IS_REF(list)) {
        zend_error(E_WARNING, "Parameter wasn't passed by reference");
        RETURN_LONG(-1);
    }
	
	if (!getbid(name, &board)||board->flag&BOARD_GROUP)
		RETURN_LONG(-1);
		
	if (!check_read_perm(getCurrentUser(), board))
		RETURN_LONG(-2);

	if (start <= 0)
		start=0;
	if (count<=0)
		RETURN_LONG(-3);
		
	members=emalloc(sizeof(struct board_member)*count);
	if (NULL==members)
		RETURN_LONG(-4);
		
	bzero(members, sizeof(struct board_member)*count);
	ret=load_board_members(board->filename, members, sort, start, count, 0);
	
	for (i=0;i<ret;i++) {
		MAKE_STD_ZVAL(element);
		array_init(element);
		bbs_make_board_member_array(element, members+i);
		zend_hash_index_update(Z_ARRVAL_P(list), i, (void *) &element, sizeof(zval*), NULL);
	}
	
	efree(members);
	
	if (ret<0)
		RETURN_LONG(-4);
		
	RETURN_LONG(ret);
}

PHP_FUNCTION(bbs_load_member_boards)
{
	char *id;
	int id_len, ret, i;
	long sort, start, count;
	zval *list, *element;
	struct userec *user;
	struct board_member *members = NULL;
	
	if (ZEND_NUM_ARGS()!=5 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "slllz", &id, &id_len, &sort, &start, &count, &list)==FAILURE)
        WRONG_PARAM_COUNT;
	
	if (!PZVAL_IS_REF(list)) {
        zend_error(E_WARNING, "Parameter wasn't passed by reference");
        RETURN_LONG(-1);
    }
	
	if (id_len>IDLEN)
        RETURN_LONG(-1);
    if (!getuser(id, &user))
        RETURN_LONG(-1);
		
	if (start <= 0)
		start=0;
	if (count<=0)
		RETURN_LONG(-2);
		
	members=emalloc(sizeof(struct board_member)*count);
	if (NULL==members)
		RETURN_LONG(-3);
		
	bzero(members, sizeof(struct board_member)*count);
	ret=load_member_boards(user->userid, members, sort, start, count);
	
	for (i=0;i<ret;i++) {
		MAKE_STD_ZVAL(element);
		array_init(element);
		bbs_make_board_member_array(element, members+i);
		zend_hash_index_update(Z_ARRVAL_P(list), i, (void *) &element, sizeof(zval*), NULL);
	}
	
	efree(members);
	
	if (ret<0)
		RETURN_LONG(-4);
		
	RETURN_LONG(ret);
}

PHP_FUNCTION(bbs_get_board_members)
{
	char *name;
	int name_len, count;
	const struct boardheader *board;
	
	if (ZEND_NUM_ARGS()!=1 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &name, &name_len)==FAILURE)
        WRONG_PARAM_COUNT;
		
	if (!getbid(name, &board)||board->flag&BOARD_GROUP)
		RETURN_LONG(-1);
		
	if (!check_read_perm(getCurrentUser(), board))
		RETURN_LONG(-2);
		
	count=get_board_members(board->filename, 0);
	if (count<0)
		RETURN_LONG(-3);
		
    RETURN_LONG(count);
}

PHP_FUNCTION(bbs_get_member_boards)
{
    char *id;
	int id_len, count;
	struct userec *user;
	
	if (ZEND_NUM_ARGS()!=1 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &id, &id_len)==FAILURE)
        WRONG_PARAM_COUNT;
	
	if (id_len>IDLEN)
        RETURN_LONG(-1);
    if (!getuser(id, &user))
        RETURN_LONG(-1);
		
	count=get_member_boards(user->userid);
	if (count<0)
		RETURN_LONG(-2);
		
    RETURN_LONG(count);
}

PHP_FUNCTION(bbs_get_user_max_boards)
{
	char *id;
	int id_len;
	struct userec *user;
	
	if (ZEND_NUM_ARGS()!=1 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &id, &id_len)==FAILURE)
        WRONG_PARAM_COUNT;
	
	if (id_len>IDLEN)
        RETURN_LONG(-1);
    if (!getuser(id, &user))
        RETURN_LONG(-1);
		
	RETURN_LONG(get_user_max_member_boards(user));
}

PHP_FUNCTION(bbs_load_board_member_request)
{
    char *name;
	int name_len;
	zval *array;
	struct board_member_config config;

	if (ZEND_NUM_ARGS()!=2 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz", &name, &name_len, &array)==FAILURE)
        WRONG_PARAM_COUNT;

    if (!PZVAL_IS_REF(array)) {
        zend_error(E_WARNING, "Parameter wasn't passed by reference");
        RETURN_FALSE;
    }
	
	if (0==strcmp(getCurrentUser()->userid, "guest"))
		RETURN_FALSE;
	
	if (load_board_member_request(name, &config)<0)
	    RETURN_FALSE;
	
	if (array_init(array) != SUCCESS)
	    RETURN_FALSE;
		
	bbs_make_board_member_config_array(array, &config);
	
	RETURN_TRUE;
}
/*
	return:
	-2: guest
	-3: 没有驻版
	-4: 准备驻版阅读DIR出错
	>=0 : 文章数目
*/
PHP_FUNCTION(bbs_load_board_member_articles)
{
	long start, count;
	char path[PATHLEN];
	int ret, total, i;
	struct stat st;
	zval *list, *element;
	struct member_board_article *articles;
	
	if (ZEND_NUM_ARGS()!=3 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "llz", &start, &count, &list)==FAILURE)
        WRONG_PARAM_COUNT;
	if (!PZVAL_IS_REF(list)) {
        zend_error(E_WARNING, "Parameter wasn't passed by reference");
        RETURN_LONG(-1);
    }
	
	if (0==strcmp(getCurrentUser()->userid, "guest"))
		RETURN_LONG(-2);
		
	set_member_board_article_dir(DIR_MODE_NORMAL, path, getCurrentUser()->userid);	
	ret=load_member_board_articles(path, DIR_MODE_NORMAL, getCurrentUser(), 0);
	if (-2==ret)
		RETURN_LONG(-3);
	if (ret<0)
		RETURN_LONG(-4);
	if (ret==0)
		RETURN_LONG(0);
		
	if (stat(path, &st)<0)
        RETURN_LONG(-4);
    
	total=st.st_size/sizeof(struct member_board_article);
	
	if (start<0)
		start=0;
	start = total-start-count;
	if (start<0)
		start=0;
	if (count>total-start)
        count=total-start;
    if (count<=0)
        RETURN_LONG(0);
	
	start++;
	if (array_init(list)!=SUCCESS)
        RETURN_LONG(-5);
	
	articles=emalloc(count*sizeof(struct member_board_article));
    if (NULL==articles)
        RETURN_LONG(-6);
	
	memset(articles, 0, count*sizeof(struct member_board_article));
	ret=get_records(path, articles, sizeof(struct member_board_article), start, count);

    for (i=ret-1; i>=0; i--) {
        MAKE_STD_ZVAL(element);
        array_init(element);
        bbs_make_board_member_article_array(element, articles+i);
        zend_hash_index_update(Z_ARRVAL_P(list), i, (void *) &element, sizeof(zval*), NULL);
    }

    efree(articles);
	
	RETURN_LONG(ret);
}

PHP_FUNCTION(bbs_get_board_member_articles)
{
	char path[PATHLEN];
	struct stat st;
	int ret;
	
	if (0==strcmp(getCurrentUser()->userid, "guest"))
		RETURN_LONG(-1);
	set_member_board_article_dir(DIR_MODE_NORMAL, path, getCurrentUser()->userid);	
	ret=load_member_board_articles(path, DIR_MODE_NORMAL, getCurrentUser(), 0);
	
	if (-2==ret)
		RETURN_LONG(-2);
	if (ret<0)
		RETURN_LONG(-3);
	if (ret==0)
		RETURN_LONG(0);
	
	if (stat(path, &st)<0)
        RETURN_LONG(-4);
    
	RETURN_LONG(st.st_size/sizeof(struct member_board_article));
}

PHP_FUNCTION(bbs_get_board_member_titles)
{
	char *name;
	int name_len, count;
	const struct boardheader *board;
	
	if (ZEND_NUM_ARGS()!=1 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &name, &name_len)==FAILURE)
        WRONG_PARAM_COUNT;
		
	if (!getbid(name, &board)||board->flag&BOARD_GROUP)
		RETURN_LONG(-1);
		
	if (!check_read_perm(getCurrentUser(), board))
		RETURN_LONG(-2);
		
	count=get_board_member_titles(board->filename);
	if (count<0)
		RETURN_LONG(-3);
		
    RETURN_LONG(count);	
}

PHP_FUNCTION(bbs_load_board_member_titles)
{
	char *name;
	int name_len, ret, i, count;
	zval *list, *element;
	const struct boardheader *board;
	struct board_member_title *titles = NULL;
	
	if (ZEND_NUM_ARGS()!=2 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz", &name, &name_len, &list)==FAILURE)
        WRONG_PARAM_COUNT;
	
	if (!PZVAL_IS_REF(list)) {
        zend_error(E_WARNING, "Parameter wasn't passed by reference");
        RETURN_LONG(-1);
    }
	
	if (!getbid(name, &board)||board->flag&BOARD_GROUP)
		RETURN_LONG(-1);
		
	if (!check_read_perm(getCurrentUser(), board))
		RETURN_LONG(-2);

	count=get_board_member_titles(board->filename);
	if (count<0)
		RETURN_LONG(-3);
	if (count==0)
		RETURN_LONG(0);
		
	titles=emalloc(sizeof(struct board_member_title)*count);
	if (NULL==titles)
		RETURN_LONG(-4);
		
	bzero(titles, sizeof(struct board_member_title)*count);
	ret=load_board_member_titles(board->filename, titles);
	
	for (i=0;i<ret;i++) {
		MAKE_STD_ZVAL(element);
		array_init(element);
		bbs_make_board_member_title_array(element, titles+i);
		zend_hash_index_update(Z_ARRVAL_P(list), titles[i].id, (void *) &element, sizeof(zval*), NULL);
	}
	
	efree(titles);
	
	if (ret<0)
		RETURN_LONG(-5);
		
	RETURN_LONG(ret);
}

PHP_FUNCTION(bbs_get_board_member_title) 
{
	char *name;
	int name_len;
	long id;
	const struct boardheader *board;
	struct board_member_title title;
	zval *element;
	int ret;
	
	if (ZEND_NUM_ARGS()!=3 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "slz", &name, &name_len, &id, &element)==FAILURE)
        WRONG_PARAM_COUNT;
	
	if (!PZVAL_IS_REF(element)) {
        zend_error(E_WARNING, "Parameter wasn't passed by reference");
        RETURN_LONG(-1);
    }
	
	if (!getbid(name, &board)||board->flag&BOARD_GROUP)
		RETURN_LONG(-2);
		
	if (!check_read_perm(getCurrentUser(), board))
		RETURN_LONG(-3);
		
	ret=get_board_member_title(board->filename, id, &title);
	if (ret<=0)
		RETURN_LONG(-4);
		
	bbs_make_board_member_title_array(element, &title);
	RETURN_LONG(ret);
}

PHP_FUNCTION(bbs_get_bmp_value)
{
	long index;
	
	if (ZEND_NUM_ARGS()!=1 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &index)==FAILURE)
        WRONG_PARAM_COUNT;
		
	RETURN_LONG(get_bmp_value(index));
}

PHP_FUNCTION(bbs_get_bmp_name)
{
	char name[STRLEN];
	long flag;
	
	if (ZEND_NUM_ARGS()!=1 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &flag)==FAILURE)
        WRONG_PARAM_COUNT;
		
	get_bmp_name(name, flag);
	RETURN_STRING(name, 1);
}

PHP_FUNCTION(bbs_set_board_member_title)
{
	char *id, *name;
	int id_len, name_len, status;
	long title_id;
	const struct boardheader *board;
	struct userec *user;
	struct board_member member;
	
	if (ZEND_NUM_ARGS()!=3 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ssl", &id, &id_len, &name, &name_len, &title_id)==FAILURE)
        WRONG_PARAM_COUNT;
		
	if (!getbid(name, &board)||board->flag&BOARD_GROUP)
		RETURN_LONG(-1);
	
	if (id_len>IDLEN)
        RETURN_LONG(-2);
		
    if (!getuser(id, &user))
        RETURN_LONG(-2);

	status=get_board_member(board->filename, user->userid, &member);
	if (status != BOARD_MEMBER_STATUS_NORMAL && status != BOARD_MEMBER_STATUS_MANAGER)
		RETURN_LONG(-3);
		
	if (member.title == title_id)
		RETURN_LONG(0);
		
	member.title=title_id;	
	RETURN_LONG(set_board_member_title(&member));
}

PHP_FUNCTION(bbs_create_board_member_title)
{
	char *name, *title;
	int name_len, title_len;
	long serial;
	const struct boardheader *board;
	
	if (ZEND_NUM_ARGS()!=3 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ssl", &name, &name_len, &title, &title_len, &serial)==FAILURE)
        WRONG_PARAM_COUNT;
		
	if (!getbid(name, &board)||board->flag&BOARD_GROUP)
		RETURN_LONG(-1);
		
	if (title_len<=0)
		RETURN_LONG(-2);
		
	if (serial<=0)
		serial=get_board_member_titles(board->filename);
		
	RETURN_LONG(create_board_member_title(board->filename, title, serial));
}

PHP_FUNCTION(bbs_remove_board_member_title)
{
	char *name;
	int name_len;
	long id;
	const struct boardheader *board;
	struct board_member_title title;
	
	if (ZEND_NUM_ARGS()!=2 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &name, &name_len, &id)==FAILURE)
        WRONG_PARAM_COUNT;
		
	if (!getbid(name, &board)||board->flag&BOARD_GROUP)
		RETURN_LONG(-1);
	if (get_board_member_title(board->filename, id, &title)<=0)
		RETURN_LONG(-2);
	RETURN_LONG(remove_board_member_title(&title));
}

PHP_FUNCTION(bbs_modify_board_member_title)
{
	char *name, *title_name;
	int name_len, title_name_len;
	long title_id, title_serial, title_flag;
	const struct boardheader *board;
	struct board_member_title title;
	
	if (ZEND_NUM_ARGS()!=5 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "slsll", &name, &name_len, &title_id, &title_name, &title_name_len, &title_serial, &title_flag)==FAILURE)
        WRONG_PARAM_COUNT;
		
	if (!getbid(name, &board)||board->flag&BOARD_GROUP)
		RETURN_LONG(-1);
	if (get_board_member_title(board->filename, title_id, &title)<=0)
		RETURN_LONG(-2);
	if (title_name_len<=0)
		RETURN_LONG(-3);
	strncpy(title.name, title_name, STRLEN-2);
	title.serial=title_serial;
	title.flag=title_flag;
	
	RETURN_LONG(modify_board_member_title(&title));
}

PHP_FUNCTION(bbs_view_member_managers)
{
	char *name;
	int name_len;
	const struct boardheader *board;
	char path[PATHLEN];
	
	if (ZEND_NUM_ARGS()!=1 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &name, &name_len)==FAILURE)
        WRONG_PARAM_COUNT;
		
	if (!getbid(name, &board)||board->flag&BOARD_GROUP)
		RETURN_FALSE;
	if (set_board_member_manager_file(board)<0)
		RETURN_FALSE;
	setbfile(path, board->filename, BOARD_MEMBER_MANAGERS_FILE);
	RETURN_STRING(path, 1);
}

#if defined(NEWSMTH) && !defined(SECONDSITE)
#include "phpar.c"
#else
PHP_FUNCTION(bbs_load_ar_timeline)
{
	RETURN_FALSE;
}
PHP_FUNCTION(bbs_load_ar_attachment)
{
	RETURN_FALSE;
}
#endif

PHP_FUNCTION(bbs_member_post_perm)
{
    long user_num, boardnum;
    struct userec *user;
    const struct boardheader *bh;

    if (zend_parse_parameters(2 TSRMLS_CC, "ll", &user_num, &boardnum) != SUCCESS)
        WRONG_PARAM_COUNT;
    user = getuserbynum(user_num);
    if (user == NULL)
        RETURN_LONG(0);
    bh=getboard(boardnum);
    if (bh==0) {
        RETURN_LONG(0);
    }
    RETURN_LONG(member_post_perm(bh, user));
}

#endif // ENABLE_BOARD_MEMBER
