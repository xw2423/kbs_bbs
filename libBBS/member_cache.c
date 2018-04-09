/** 
  * 默认的驻版信息通过MySQL数据库存储，随着站点对驻版信息使用的增多，
  * 为了减少对数据库的依赖，并提高运行效率（数据库MyISAM表的效率你懂的...）
  * 改用共享内存存储驻版信息。
  * 驻版信息存储于 bbshome/.MEMBERS 文件
  * PS: 此部分代码请勿轻易改动
  *
  * windinsn, 2013-02-03
  */

#include "bbs.h"
#ifdef ENABLE_MEMBER_CACHE
#include "sys/socket.h"
#include "netinet/in.h"
#include <signal.h>
#include "member_cache.h"


int member_cache_node_cmp_type=0;

#ifndef USE_SEM_LOCK

static int member_cache_lock()
{
	int lockfd;
	char errbuf[STRLEN];
	lockfd = open(MEBERS_TMP_FILE, O_RDWR | O_CREAT, 0600);
	if (lockfd < 0) {
		bbslog("3system", "CACHE:lock member cache:%s", strerror_r(errno, errbuf, STRLEN));
		return -1;
	}
	writew_lock(lockfd, 0, SEEK_SET, 0);
	return lockfd;
}

static void member_cache_unlock(int fd)
{
	un_lock(fd, 0, SEEK_SET, 0);
	close(fd);
}
#else /* USE_SEM_LOCK */
static int member_cache_lock()
{
	lock_sem(MEMBER_CACHE_SEMLOCK);
	return 0;
}

static void member_cache_unlock(int fd)
{
	unlock_sem_check(MEMBER_CACHE_SEMLOCK);
}
#endif /* USE_SEM_LOCK */

void resolve_members () 
{
	int iscreate;

	iscreate = 0;
	if (membershm == NULL) {
		membershm = (struct MEMBER_CACHE *) attach_shm("MEMBER_CACHE_SHMKEY", 3702, sizeof(*membershm), &iscreate);
		if (iscreate) {		 /* shouldn't load .MEMBERS file in this place */
			bbslog("3system", "member cache daemon havn't startup");
			remove_shm("MEMBER_CACHE_SHMKEY",3702,sizeof(*membershm));
			return;
		}

	}
}

void detach_members ()
{
	shmdt((void *)membershm);
	membershm=NULL;
}
int update_member_manager_flag(int uid) {
	struct userec *user;
	int count;
	
	user=getuserbynum(uid);
	if (NULL==user)
		return -1;
		
	if (HAS_PERM(user, PERM_NOZAP)) {
		if (HAS_PERM(user,PERM_MEMBER_MANAGER))
			user->userlevel &= ~PERM_MEMBER_MANAGER;
		return -2;
	}
	
	count=get_member_managers_cache(user->userid);
	
	if (count <= 0 && HAS_PERM(user,PERM_MEMBER_MANAGER))
		user->userlevel &= ~PERM_MEMBER_MANAGER;
	if (count > 0 && !HAS_PERM(user,PERM_MEMBER_MANAGER))
		user->userlevel |= PERM_MEMBER_MANAGER;
		
	return 0;
}
int is_valid_member(struct userec *user, const struct boardheader *board) {
	if (board->flag&BOARD_GROUP)
		return 0;
	if (!check_read_perm(user,board))
		return 0;
	return 1;
}
int is_null_member(struct MEMBER_CACHE_NODE *node) {
	if (node->bid==0 || node->uid==0)
		return 1;
	return 0;
}
int fill_member(struct board_member *member, struct MEMBER_CACHE_NODE *node) {
	struct userec *user;
	const struct boardheader *board;
	
	if (is_null_member(node))
		return -1;
		
	board=getboard(node->bid);
	if (NULL==board)
		return -2;
		
	user=getuserbynum(node->uid);	
	if (NULL==user)
		return -3;
	
	if (HAS_PERM(user, PERM_NOZAP) && BOARD_MEMBER_STATUS_MANAGER==node->status) {
		node->status=BOARD_MEMBER_STATUS_NORMAL;
		node->flag=0;
	}
	
	if (BOARD_MEMBER_STATUS_MANAGER!=node->status) {
		node->flag=0;
	}

	if (NULL==member)
		return 0;
	
	bzero(member, sizeof(struct board_member));
	strncpy(member->board, board->filename, STRLEN);
	strncpy(member->user, user->userid, IDLEN+1);
	member->time=node->time;
	member->score=node->score;
	member->flag=node->flag;
	member->title=node->title;
	member->manager[0]=0;
	member->status=node->status;
	
	return 0;
}
int is_null_member_title(struct MEMBER_CACHE_TITLE *title) {
	if (title->bid==0)
		return 1;
	if (!title->name[0])
		return 1;
	return 0;
}
int fill_member_title(struct board_member_title *title, struct MEMBER_CACHE_TITLE *t) {
	const struct boardheader *board;
	
	if (is_null_member_title(t))
		return -1;
	board=getboard(t->bid);
	if (NULL==board)
		return -2;
		
	if (NULL==title)
		return 0;
		
	bzero(title, sizeof(struct board_member_title));
	title->id=t->id;
	strncpy(title->board, board->filename, STRLEN);
	strncpy(title->name, t->name, STRLEN);
	title->serial=t->serial;
	title->flag=t->flag;
	
	return 0;
}
int find_null_member_title() {
	int i;
	
	for (i=0;i<MAX_MEMBER_TITLES;i++) {
		if (is_null_member_title(&(membershm->titles[i])))
			return i+1;
	}
	return 0;
}
int find_null_member() {
	int i;
	
	for (i=0; i<MAX_MEMBERS; i++) {
		if (is_null_member(&(membershm->nodes[i])))
			return i+1;
	}
	return 0;
}
int get_member_title_index(int id) {
	if (id<=0 || id>MAX_MEMBER_TITLES || membershm->title_count <= 0)
		return 0;
	
	if (is_null_member_title(&(membershm->titles[id-1])))
		return 0;
	return id;
}
struct MEMBER_CACHE_TITLE *get_member_title(int id) {
	int index=get_member_title_index(id);
	if (index<=0)
		return NULL;
		
	return &(membershm->titles[index-1]);
}
int get_board_member_title_cache(const char *board, int id, struct board_member_title *title) {
	int bid;
	struct MEMBER_CACHE_TITLE *t;
	
	if (NULL!=title)
		bzero(title, sizeof(struct board_member_title));

	bid=getbid(board, NULL);
	if (bid<=0)
		return -1;
	t=get_member_title(id);
	if (NULL==t)
		return 0;
	if (t->bid != bid)
		return 0;
	if (NULL!=title)
		fill_member_title(title, t);
	
	return 1;
}
struct MEMBER_CACHE_NODE *get_member_by_index(int index) {
	if (index<=0 || index>MAX_MEMBERS)
		return NULL;
	if (is_null_member(&(membershm->nodes[index-1])))
		return NULL;
	return &(membershm->nodes[index-1]);
}
int get_member_cache(const char *name, const char *user_id, struct board_member *member) {
	int index;
	struct MEMBER_CACHE_NODE *node;

	if (NULL!=member) {
		bzero(member, sizeof(struct board_member));
		member->status=BOARD_MEMBER_STATUS_NONE;
	}
	
	index=get_member_index(name, user_id);
	if (index<=0)
		return index;
	
	node=get_member_by_index(index);
	if (NULL!=member)
		fill_member(member, node);
	
	return node->status;
}
int query_board_member_title_cache(const char *board, char *name, struct board_member_title *title) {
	int bid, i;
	struct MEMBER_CACHE_TITLE *t;
	
	bid=getbid(board, NULL);
	if (bid<=0)
		return -1;
	i=membershm->board_titles[bid-1];
	if (i<=0)
		return 0;
	while (i>0) {
		t=&(membershm->titles[i-1]);
		if (is_null_member_title(t))
			return 0;
		if (t->bid != bid)
			return 0;
		if (strcasecmp(t->name, name)==0) {
			if (NULL!=title) fill_member_title(title, t);
			return 1;
		}
		i=t->next;
	}
	return 0;
}
int set_board_member_title_cache(struct board_member *member) {
	int i;
	struct MEMBER_CACHE_NODE *node;
	
	i=get_member_index(member->board, member->user);
	if (i<=0)
		return 0;
	node=get_member_by_index(i);
	if (NULL==node)
		return 0;
	if (member->title>0 && get_board_member_title_cache(member->board, member->title, NULL)<=0)
		return 0;
	node->title=member->title;
	return 1;
}
int set_member_title_cache(struct board_member_title *title) {
	struct MEMBER_CACHE_TITLE *t;
	int bid;
	
	bid=getbid(title->board, NULL);
	if (bid<=0)
		return -1;
	t=get_member_title(title->id);
	if (NULL==t)
		return -2;
	if (t->bid != bid)
		return -3;
	
	strncpy(t->name, title->name, STRLEN);
	t->serial=title->serial;
	t->flag=title->flag;
	
	return 0;
}
int get_member_index(const char *name, const char *user_id) {
	int uid, bid, i;
	struct MEMBER_CACHE_NODE *node;
	struct userec *user;
	const struct boardheader *board;
	
	if (strcasecmp("guest", user_id)==0)
		return -1;
	
	uid=getuser(user_id, &user);
	if (uid<=0)
		return -2;
		
	bid=getbid(name, &board);
	if (bid<=0)
		return -3;
		
	i=membershm->users[uid-1];
	if (i==0)
		return 0;
		
	while (i>0) {
		node=&(membershm->nodes[i-1]);
		if (is_null_member(node))
			return 0;
		if (node->uid!=uid)
			return 0;
		if (node->bid==bid) {
			if (!is_valid_member(user, board))
				break;
			else
				return i;
		}
		i=node->user_next;
	}
	
	if (i>0)
		remove_member(i);
	
	return 0;
}
int get_board_titles_cache(const char *name, struct MEMBER_TITLE_CONTAINER *titles, int size)
{
	int bid, i, ret;
	struct MEMBER_CACHE_TITLE *title;
	
	bid=getbid(name, NULL);
	if (bid<=0)
		return -1;
		
	i=membershm->board_titles[bid-1];
	if (i==0)
		return 0;
		
	ret=0;
	if (NULL!=titles && size>0)
		bzero(titles, sizeof(struct MEMBER_TITLE_CONTAINER)*size);
		
	while (i>0) {
		title=&(membershm->titles[i-1]);
		if (is_null_member_title(title))
			break;
		if (title->bid != bid)
			break;
		if (NULL!=titles && size>0 && ret<size)
			titles[ret].title=&(membershm->titles[i-1]);
			
		i=title->next;
		ret ++;
	}
	
	return ret;
}
int count_board_titles_cache(const char *name) {
	return get_board_titles_cache(name, NULL, 0);
}
int get_board_members_cache(const char *name, struct MEMBER_CACHE_CONTAINER *members, int size, int status)
{
	int bid, i, ret, j;
	struct MEMBER_CACHE_NODE *node;
	const struct boardheader *board;
	
	bid=getbid(name, &board);
	if (bid<=0)
		return -1;
		
	i=membershm->boards[bid-1];
	if (i==0)
		return 0;
		
	ret=0;
	if (NULL!=members && size>0)
		bzero(members, sizeof(struct MEMBER_CACHE_CONTAINER)*size);
	
	while (i>0) {
		node=&(membershm->nodes[i-1]);
		if (is_null_member(node))
			break;
		if (node->bid != bid)
			break;
		
		if (is_valid_member(getuserbynum(node->uid), board)) {
			if (status==0||status==node->status) {
				if (NULL!=members && size>0 && ret<size)
					members[ret].node=&(membershm->nodes[i-1]);
				ret++;
			}
			i=node->board_next;
		} else {
			j=node->board_next;
			remove_member(i);
			i=j;
		}
	}
	return ret;
}
int count_board_members_cache(const char *name, int status) {
	return get_board_members_cache(name, NULL, 0, status);
}
int add_member_title(struct board_member_title *title) {
	struct MEMBER_CACHE_TITLE *t, *p;
	int bid, i, index, fd;
	
	if (membershm->title_count >= MAX_MEMBER_TITLES) {
		bbslog("3system", "BBS board member titles records full.");
		return -1;
	}
	
	if (!title->board[0] || !title->name[0])
		return -2;
		
	bid=getbid(title->board, NULL);
	
	if (bid==0)
		return -3;
	
	fd=member_cache_lock();
	index=find_null_member_title();
	if (index==0) {
		member_cache_unlock(fd);
		return -4;
	}
		
	t=&(membershm->titles[index-1]);
	bzero(t, sizeof(struct MEMBER_CACHE_TITLE));
	t->id=index;
	t->bid=bid;
	strncpy(t->name, title->name, STRLEN);
	t->serial=title->serial;
	t->flag=title->flag;
	t->next=0;
	
	i=membershm->board_titles[bid-1];
	if (i==0) {
		membershm->board_titles[bid-1]=index;
	} else {
		while (i!=0) {
			p=&(membershm->titles[i-1]);
			i=p->next;
			if (i==0) {
				p->next=index;
			}
		}
	}
	
	membershm->title_count ++;
	member_cache_unlock(fd);
	return 0;
}
int add_member(struct board_member *member) {
	struct MEMBER_CACHE_NODE *node, *p;
	int uid, bid, i, index, fd;
	struct userec *user;
	const struct boardheader *board;
	
	if (membershm->member_count >= MAX_MEMBERS) {
		bbslog("3system", "BBS board members records full.");
		return -1;
	}
	
	if (!member->user[0] || !member->board[0])
		return -2;
	
	uid=getuser(member->user, &user);
	bid=getbid(member->board, &board);
	
	if (uid==0 || bid==0)
		return -3;
		
	if (!is_valid_member(user, board))
		return -4;
		
	if (get_member_index(member->board, member->user)>0)
		return -5;
	
	fd=member_cache_lock();
	index=find_null_member();
	if (index==0) {
		member_cache_unlock(fd);
		return -6;
	}
	node=&(membershm->nodes[index-1]);
	bzero(node, sizeof(struct MEMBER_CACHE_NODE));
	node->bid=bid;
	node->uid=uid;
	node->time=member->time;
	node->status=member->status;
	node->score=member->score;
	node->flag=member->flag;
	node->title=member->title;
	node->user_next=0;
	node->board_next=0;
	
	i=membershm->users[uid-1];
	if (i==0) {
		membershm->users[uid-1]=index;
	} else {
		while (i!=0) {
			p=&(membershm->nodes[i-1]);
			i=p->user_next;
			if (i==0) {
				p->user_next=index;
			}
		}
	}
	
	i=membershm->boards[bid-1];
	if (i==0) {
		membershm->boards[bid-1]=index;
	} else {
		while (i!=0) {
			p=&(membershm->nodes[i-1]);
			i=p->board_next;
			if (i==0) {
				p->board_next=index;
			}
		}
	}
	
	membershm->member_count ++;
	member_cache_unlock(fd);
	return 0;	
}
int remove_member_title(int index) {
	struct MEMBER_CACHE_TITLE *title, *t;
	struct MEMBER_CACHE_NODE *p;
	int i, son, fd;
	
	title=get_member_title(index);
	if (NULL==title)
		return -1;
	
	fd=member_cache_lock();
	i=membershm->board_titles[title->bid-1];
	son=title->next;
	
	if (i==index || i==0)
		membershm->board_titles[title->bid-1]=son;
	else {
		while (i!=index && i!=0) {
			t=&(membershm->titles[i-1]);
			i=t->next;
			if (i==index || i==0)
				t->next=son;
		}
	}
	
	i=membershm->boards[title->bid-1];
	while (i>0) {
		p=&(membershm->nodes[i-1]);
		if (is_null_member(p))
			break;
		if (p->bid != title->bid)
			break;
		if (p->title == title->id)
			p->title = 0;
		i=p->board_next;
	}
	
	bzero(title, sizeof(struct MEMBER_CACHE_TITLE));
	membershm->title_count --;
	member_cache_unlock(fd);
	
	return 0;
}

int remove_member(int index) {
	struct MEMBER_CACHE_NODE *node, *p;
	int bid, uid, i, son, fd;

	node=get_member_by_index(index);
	if (NULL==node)
		return -1;
	
	bid=node->bid;
	uid=node->uid;
	
	fd=member_cache_lock();
	i=membershm->users[uid-1];
	son=node->user_next;
	if (i==index || i==0)
		membershm->users[uid-1]=son;
	else {
		while(i!=index && i!=0) {
			p=&(membershm->nodes[i-1]);
			i=p->user_next;
			if (i==index || i==0) {
				p->user_next=son;
			}
		}
	}
	
	i=membershm->boards[bid-1];
	son=node->board_next;
	if (i==index || i==0)
		membershm->boards[bid-1]=son;
	else {
		while (i!=index && i!=0) {
			p=&(membershm->nodes[i-1]);
			i=p->board_next;
			if (i==index || i==0) {
				p->board_next=son;
			}
		}
	}
	
	if (node->status==BOARD_MEMBER_STATUS_MANAGER)
		update_member_manager_flag(node->uid);
		
	bzero(node, sizeof(struct MEMBER_CACHE_NODE));
	membershm->member_count --;
	member_cache_unlock(fd);
	
	return 0;
}

void load_member_titles() {
	int fd;
	char errbuf[STRLEN];
	struct board_member_title *titles;
	int i;

	titles=(struct board_member_title *)malloc(sizeof(struct board_member_title)*MAX_MEMBER_TITLES);
	if (NULL==titles) {
		bbslog("3system", "can not malloc member titles!");
		exit (-1);
	}
	bzero(titles, sizeof(struct board_member_title)*MAX_MEMBER_TITLES);	

	membershm->title_count=0;

	if ((fd=open(MEMBER_TITLES_FILE, O_RDWR | O_CREAT, 0644))==-1) {
		free(titles);
		bbslog("3system", "can not open member titles file: %s", strerror_r(errno, errbuf, STRLEN));
		exit(-1);
	}
	ftruncate(fd, MAX_MEMBER_TITLES * sizeof(struct board_member_title));
	close(fd);

	if (get_records(MEMBER_TITLES_FILE, titles, sizeof(struct board_member_title), 1, MAX_MEMBER_TITLES) != MAX_MEMBER_TITLES) {
		free(titles);
		bbslog("3system", "error member title file!");
		exit(-1);
	}

	for (i=0;i<MAX_MEMBER_TITLES;i++) {
		add_member_title(&titles[i]);
	}
	
	free(titles);
	bbslog("3system", "CACHE:reload member title cache for %d records", membershm->title_count);
}

int load_members ()
{
	int iscreate;
	int fd;
	int member_file_fd;
	char errbuf[STRLEN];
	struct board_member *members;
	int i;

	membershm = (struct MEMBER_CACHE *) attach_shm("MEMBER_CACHE_SHMKEY", 3702, sizeof(*membershm), &iscreate);   /*attach to member cache shm */
	if (!iscreate) {
		bbslog("3system", "load a exitist member cache shm!");
	} else {
		fd=member_cache_lock();
		bzero(membershm, sizeof(struct MEMBER_CACHE));
		member_cache_unlock(fd);
		
		load_member_titles();
		if ((member_file_fd = open(MEMBERS_FILE, O_RDWR | O_CREAT, 0644)) == -1) {
			bbslog("3system", "Can't open " MEMBERS_FILE " file %s", strerror_r(errno, errbuf, STRLEN));
			exit(-1);
		}
		ftruncate(member_file_fd, MAX_MEMBERS * sizeof(struct board_member));
		close(member_file_fd);

		members=(struct board_member *)malloc(sizeof(struct board_member)*MAX_MEMBERS);
		if (NULL==members) {
			bbslog("3system", "can not malloc members!");
			exit(-1);
		}
		bzero(members, sizeof(struct board_member)*MAX_MEMBERS);
		if (get_records(MEMBERS_FILE, members, sizeof(struct board_member), 1, MAX_MEMBERS) != MAX_MEMBERS) {
			bbslog("3system", "error members file!");
			free(members);
			exit(-1);
		}

		membershm->member_count=0;
		for (i=0;i<MAX_MEMBERS;i++) {
			add_member(&members[i]);
		}
		
		free(members);
		bbslog("3system", "CACHE:reload member cache for %d records", membershm->member_count);
	}
	return 0;
}

static int flush_member_cache_members() {
	struct board_member *members;
	int i, ret;

	members=(struct board_member *)malloc(MAX_MEMBERS * sizeof(struct board_member));
	if (NULL==members)
		return -1;
		
	bzero(members, 	MAX_MEMBERS * sizeof(struct board_member));
	for (i=0; i<MAX_MEMBERS;i++) {
		fill_member(&members[i], &(membershm->nodes[i]));
	}
		
	ret=substitute_record(MEMBERS_FILE, members, MAX_MEMBERS * sizeof(struct board_member), 1, NULL, NULL);
	free(members);

	return ret;
}
static int flush_member_cache_titles() {
	struct board_member_title *titles;
	int i, ret;
	
	bbslog("3system", "flush member cache: titles BEGIN");
	
	titles=(struct board_member_title *)malloc(MAX_MEMBER_TITLES * sizeof(struct board_member_title));
	if (NULL==titles)
		return -1;
		
	bzero(titles, MAX_MEMBER_TITLES * sizeof(struct board_member_title));
	for (i=0;i<MAX_MEMBER_TITLES;i++) {
		fill_member_title(&titles[i], &(membershm->titles[i]));
	}
		
	ret=substitute_record(MEMBER_TITLES_FILE, titles, MAX_MEMBER_TITLES * sizeof(struct board_member_title), 1, NULL, NULL);
	free(titles);

	bbslog("3system", "flush member cache: titles");

	return ret;
}
int flush_member_cache () {
	flush_member_cache_members();
	flush_member_cache_titles();
	return 0;
}
typedef int member_cache_title_cmp_func(const void *, const void *);
int member_cache_title_cmp(struct MEMBER_TITLE_CONTAINER *a, struct MEMBER_TITLE_CONTAINER *b) {
	return a->title->serial - b->title->serial;
}
typedef int member_cache_node_cmp_func(const void *, const void *);
int member_cache_node_cmp(struct MEMBER_CACHE_CONTAINER *a, struct MEMBER_CACHE_CONTAINER *b) {
	struct userec *u1, *u2;
	const struct boardheader *b1, *b2;
	switch(member_cache_node_cmp_type) {
		case BOARD_MEMBER_SORT_TIME_ASC+100:
		case MEMBER_BOARD_SORT_TIME_ASC:
			return a->node->time - b->node->time;
		case BOARD_MEMBER_SORT_TIME_DESC+100:
		case MEMBER_BOARD_SORT_TIME_DESC:
			return b->node->time - a->node->time;
		case BOARD_MEMBER_SORT_ID_ASC+100:
		case BOARD_MEMBER_SORT_ID_DESC+100:
			u1=getuserbynum(a->node->uid);
			u2=getuserbynum(b->node->uid);
			if (u1==NULL || u2==NULL)
				return 0;
			if (BOARD_MEMBER_SORT_ID_ASC+100==member_cache_node_cmp_type)
				return strcasecmp(u1->userid, u2->userid);
			else
				return strcasecmp(u2->userid, u1->userid);
		case BOARD_MEMBER_SORT_SCORE_DESC+100:
		case MEMBER_BOARD_SORT_SCORE_DESC:
			return b->node->score - a->node->score;
		case BOARD_MEMBER_SORT_SCORE_ASC+100:
		case MEMBER_BOARD_SORT_SCORE_ASC:
			return a->node->score - b->node->score;
		case BOARD_MEMBER_SORT_STATUS_ASC+100:
		case MEMBER_BOARD_SORT_STATUS_ASC:
			return a->node->status - b->node->status;
		case BOARD_MEMBER_SORT_STATUS_DESC+100:
		case MEMBER_BOARD_SORT_STATUS_DESC:
			return b->node->status - a->node->status;
		case BOARD_MEMBER_SORT_TITLE_ASC+100:
			return a->node->title - b->node->title;
		case BOARD_MEMBER_SORT_TITLE_DESC+100:
			return b->node->title - a->node->title;
		case MEMBER_BOARD_SORT_BOARD_ASC:
		case MEMBER_BOARD_SORT_BOARD_DESC:
			b1=getboard(a->node->bid);
			b2=getboard(b->node->bid);
			if (b1==NULL || b2==NULL)
				return 0;
			if (MEMBER_BOARD_SORT_BOARD_ASC==member_cache_node_cmp_type)
				return strcasecmp(b1->filename, b2->filename);
			else
				return strcasecmp(b2->filename, b1->filename);
	}
	return 0;
}
int load_board_titles_cache(const char *board, struct board_member_title *titles)
{
	int total, i;
	struct MEMBER_TITLE_CONTAINER *container;
	
	total=get_board_titles_cache(board, NULL, 0);
	if (total < 0)
		return -1;
	if (total==0)
		return 0;
	
	container=(struct MEMBER_TITLE_CONTAINER *)malloc(sizeof(struct MEMBER_TITLE_CONTAINER)*total);
	if (NULL==container)
		return -2;
	get_board_titles_cache(board, container, total);
	
	qsort(container, total, sizeof(struct MEMBER_TITLE_CONTAINER), (member_cache_title_cmp_func *) member_cache_title_cmp);
	for (i=0; i<total; i ++) {
		fill_member_title(&titles[i], container[i].title);
	}
	free(container);
	
	return total;
}
int load_board_members_cache(const char *board, struct board_member *members, int sort, int start, int num, int status)
{
	int total, i, j;
	struct MEMBER_CACHE_CONTAINER *container;
	
	total=get_board_members_cache(board, NULL, 0, status);
	if (start<0 || start>=total)
		return 0;
	
	container=(struct MEMBER_CACHE_CONTAINER *)malloc(sizeof(struct MEMBER_CACHE_CONTAINER)*total);
	if (NULL==container)
		return -2;
	get_board_members_cache(board, container, total, status);
	
	switch(sort) {
		case BOARD_MEMBER_SORT_TIME_DESC:
		case BOARD_MEMBER_SORT_ID_ASC:
		case BOARD_MEMBER_SORT_ID_DESC:
		case BOARD_MEMBER_SORT_SCORE_DESC:
		case BOARD_MEMBER_SORT_SCORE_ASC:
		case BOARD_MEMBER_SORT_STATUS_ASC:
		case BOARD_MEMBER_SORT_TITLE_ASC:
		case BOARD_MEMBER_SORT_TITLE_DESC:
		case BOARD_MEMBER_SORT_STATUS_DESC:
			member_cache_node_cmp_type=sort;
			break;
		case BOARD_MEMBER_SORT_TIME_ASC:
		default:
			member_cache_node_cmp_type=BOARD_MEMBER_SORT_TIME_ASC;
	}
	member_cache_node_cmp_type += 100;
	
	qsort(container, total, sizeof(struct MEMBER_CACHE_CONTAINER), (member_cache_node_cmp_func *) member_cache_node_cmp);
	j=0;
	for (i=start; i<start+num && i<total;i++) {
		fill_member(&members[j], container[i].node);
		j++;
	}
	free(container);
	return j;
}
int get_member_boards_cache(const char *user_id, struct MEMBER_CACHE_CONTAINER *members, int size)
{
	int uid, i, ret, j;
	struct MEMBER_CACHE_NODE *node;
	struct userec *user;
	
	if (strcasecmp("guest", user_id)==0)
		return 0;
		
	uid=getuser(user_id, &user);
	if (uid<=0)
		return -1;
		
	i=membershm->users[uid-1];
	if (i==0)
		return 0;
		
	ret=0;
	if (NULL!=members && size>0)
		bzero(members, sizeof(struct MEMBER_CACHE_CONTAINER)*size);
	
	while (i>0) {
		node=&(membershm->nodes[i-1]);
		if (is_null_member(node))
			break;
		if (node->uid != uid)
			break;
		
		if (is_valid_member(user, getboard(node->bid))) {
			if (NULL!=members && size>0 && ret<size)	
				members[ret].node=&(membershm->nodes[i-1]);
				
			i=node->user_next;
			ret ++;
		} else {
			j=node->user_next;
			remove_member(i);
			i=j;
		}
	}
	return ret;
}
int count_member_boards_cache(const char *user_id) {
	return get_member_boards_cache(user_id, NULL, 0);
}
int load_member_boards_cache(const char *user_id, struct board_member *members, int sort, int start, int num) 
{
	int total, i, j;
	struct MEMBER_CACHE_CONTAINER *container;
	
	total=get_member_boards_cache(user_id, NULL, 0);
	if (start<0 || start>=total)
		return -1;
	
	container=(struct MEMBER_CACHE_CONTAINER *)malloc(sizeof(struct MEMBER_CACHE_CONTAINER)*total);
	if (NULL==container)
		return -2;
	get_member_boards_cache(user_id, container, total);
	
	switch(sort) {
		case MEMBER_BOARD_SORT_TIME_DESC:
		case MEMBER_BOARD_SORT_BOARD_ASC:
		case MEMBER_BOARD_SORT_BOARD_DESC:
		case MEMBER_BOARD_SORT_SCORE_DESC:
		case MEMBER_BOARD_SORT_SCORE_ASC:
		case MEMBER_BOARD_SORT_STATUS_ASC:
		case MEMBER_BOARD_SORT_STATUS_DESC:
			member_cache_node_cmp_type=sort;
			break;
		case MEMBER_BOARD_SORT_TIME_ASC:
		default:
			member_cache_node_cmp_type=MEMBER_BOARD_SORT_TIME_ASC;
	}
	
	qsort(container, total, sizeof(struct MEMBER_CACHE_CONTAINER), (member_cache_node_cmp_func *) member_cache_node_cmp);
	j=0;
	for (i=start; i<start+num && i<total;i++) {
		fill_member(&members[j], container[i].node);
		j++;
	}
	free(container);
	return j;
}
int update_member_cache(struct board_member *member) {
	struct MEMBER_CACHE_NODE *node;
	int index;
	
	index=get_member_index(member->board, member->user);
	if (index <= 0)
		return -1;
	
	node=get_member_by_index(index);
	node->flag = member->flag;
	node->status = member->status;
	node->score = member->score;
	
	return 0;
}
int get_member_managers_cache(char *user_id) {
	int uid, i, ret, j;
	struct MEMBER_CACHE_NODE *node;
	struct userec *user;
	
	if (strcasecmp("guest", user_id)==0)
		return 0;
		
	uid=getuser(user_id, &user);
	if (uid<=0)
		return 0;
		
	i=membershm->users[uid-1];
	if (i==0)
		return 0;
		
	ret=0;
	while (i>0) {
		node=&(membershm->nodes[i-1]);
		if (is_null_member(node))
			break;
		if (node->uid != uid)
			break;
		if (is_valid_member(user, getboard(node->bid))) {	
			if (node->status==BOARD_MEMBER_STATUS_MANAGER)
				ret ++;
				
			i=node->user_next;
		} else {
			j=node->user_next;
			remove_member(i);
			i=j;
		}
	}
	return ret;
}
int get_member_board_managers_cache(const char *name, struct MEMBER_CACHE_CONTAINER *members, int size) {
	int bid, i, ret, j;
	struct MEMBER_CACHE_NODE *node;
	const struct boardheader *board;
	struct userec *user;
	
	bid=getbid(name, &board);
	if (bid<=0)
		return -1;
		
	i=membershm->boards[bid-1];
	if (i==0)
		return 0;
		
	ret=0;
	if (NULL!=members && size>0)
		bzero(members, sizeof(struct MEMBER_CACHE_CONTAINER)*size);
	
	while (i>0) {
		node=&(membershm->nodes[i-1]);
		if (is_null_member(node))
			break;
		if (node->bid != bid)
			break;
		user=getuserbynum(node->uid);
		if (is_valid_member(user, board)) {
			if (node->status==BOARD_MEMBER_STATUS_MANAGER) {	
				if (NULL!=members && size>0 && ret<size)
					members[ret].node=&(membershm->nodes[i-1]);
				ret++;
			}
			i=node->board_next;
		} else {
			j=node->board_next;
			remove_member(i);
			i=j;
		}
	}
	return ret;	
}
int count_member_board_managers_cache(const char *name) {
	return get_member_board_managers_cache(name, NULL, 0);
}
int load_board_member_managers_cache(const char *name, struct board_member *members) {
	int total, i;
	struct MEMBER_CACHE_CONTAINER *container;
	
	total=get_member_board_managers_cache(name, NULL, 0);
	if (total<=0)
		return 0;
	container=(struct MEMBER_CACHE_CONTAINER *)malloc(sizeof(struct MEMBER_CACHE_CONTAINER)*total);
	if (NULL==container)
		return -2;
	get_member_board_managers_cache(name, container, total);
	for (i=0;i<total;i++) {
		fill_member(&members[i], container[i].node);
	}
	free(container);
	return total;
}
#endif
