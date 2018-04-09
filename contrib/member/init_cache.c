#include "bbs.h"

#ifndef MAX_MEMBERS
#define MAX_MEMBERS 400000
#endif

#ifndef MAX_MEMBER_TITLES
#define MAX_MEMBER_TITLES 10000
#endif

#ifndef MEMBERS_FILE
#define MEMBERS_FILE    ".MEMBERS"
#endif

#ifndef MEMBER_TITLES_FILE
#define MEMBER_TITLES_FILE ".MEMBER.TITLES"
#endif

int main(int argc, char **argv)
{
	int tid[MAX_MEMBER_TITLES*2];
	struct board_member_title *titles;
	struct board_member *members;
	MYSQL s;
	MYSQL_RES *res;
	MYSQL_ROW row;
	char sql[300];
	int i, index;
	struct userec *user;

	chdir(BBSHOME);
	resolve_boards();
	resolve_ucache();

	bzero(&tid, sizeof(int)*2*MAX_MEMBER_TITLES);
	titles=(struct board_member_title *)malloc(sizeof(struct board_member_title)*MAX_MEMBER_TITLES);
	if (NULL==titles) {
		printf("can not init. titles!\n");
		return -1;
	}
	members=(struct board_member *)malloc(sizeof(struct board_member)*MAX_MEMBERS);
	if (NULL==members) {
		printf("can not init. members!\n");
		free(titles);
		return -2;
	}

	bzero(titles, sizeof(struct board_member_title)*MAX_MEMBER_TITLES);
	bzero(members, sizeof(struct board_member)*MAX_MEMBERS);

	mysql_init(&s);
	my_connect_mysql(&s);
	sprintf(sql,"SELECT `id`, `board`, `name`, `serial`, `flag` FROM `board_title` ORDER BY `board`, `serial` ASC, `id` ASC");
	mysql_real_query(&s, sql, strlen(sql));
	res = mysql_store_result(&s);
	row = mysql_fetch_row(res);
	i=0;
	while (row!=NULL) {
		index=i+1;
		titles[i].id=index;
		strncpy(titles[i].board, row[1],STRLEN-2);
		strncpy(titles[i].name, row[2], STRLEN-2);
		titles[i].serial=atol(row[3]);
		titles[i].flag=atol(row[4]);

		tid[atol(row[0])]=index;

		printf("[TITLE]%s @ %s\n", titles[i].name, titles[i].board);
		i++;
		row = mysql_fetch_row(res);
	}
	mysql_free_result(res);

	sprintf(sql,"SELECT `board`, `user`, UNIX_TIMESTAMP(`time`), `status`, `manager`, `score`, `title`, `flag` FROM `board_user` ORDER BY `board`, `status`, `user`");	
	mysql_real_query(&s, sql, strlen(sql));
	res = mysql_store_result(&s);
	row = mysql_fetch_row(res);
	i=0;
	while(row!=NULL) {
		const struct boardheader *board;
		if (getuser(row[1], &user)&&getbid(row[0], &board)&&check_read_perm(user, board)) {
			strncpy(members[i].board, board->filename, STRLEN-2);
			strncpy(members[i].user, user->userid, IDLEN+1);
			members[i].time=atol(row[2]);
			members[i].status=atol(row[3]);
			strncpy(members[i].manager, row[4], IDLEN+1);
			members[i].score=atol(row[5]);
			index=atol(row[6]);
			if (index<2*MAX_MEMBER_TITLES)
				index=tid[index];
			else
				index=0;	
			members[i].title=index;
			members[i].flag=atol(row[7]);

			if (!valid_core_member(user->userid)) {
				if (BOARD_MEMBER_STATUS_MANAGER==members[i].status) {
					members[i].status=BOARD_MEMBER_STATUS_NORMAL;
				}
				members[i].flag=0;
			}
			printf("[MEMBER]%s @ %s # %d\n", members[i].user, members[i].board, members[i].title);
			i++;
		}

		row = mysql_fetch_row(res);
	}
	mysql_free_result(res);

	mysql_close(&s);
	printf("title file size: %lu\n", sizeof(struct board_member_title)*MAX_MEMBER_TITLES);
	i=substitute_record(MEMBER_TITLES_FILE, titles, MAX_MEMBER_TITLES * sizeof(struct board_member_title), 1, NULL, NULL);
	printf("save to file: %d\n", i);

	printf("member file size: %lu\n", sizeof(struct board_member)*MAX_MEMBERS);
	i=substitute_record(MEMBERS_FILE, members, MAX_MEMBERS * sizeof(struct board_member), 1, NULL, NULL);
	printf("save to file: %d\n", i);

	free(titles);
	free(members);
	return 0;
}
