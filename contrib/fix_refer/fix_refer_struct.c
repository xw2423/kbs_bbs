#include "bbs.h"
#include "stdio.h"

struct refer_ugly {
    char board[IDLEN+6];
    char user[IDLEN+1];
    char title[ARTICLE_TITLE_LEN];
    unsigned int id, groupid, reid, flag;
    time_t time;
};

struct refer_pretty {
	char board[STRLEN];
	char user[IDLEN+1];
	char title[ARTICLE_TITLE_LEN];
	unsigned int id, groupid, reid, flag;
	time_t time;
};


int conv_refer_dir(struct userec *user, char *filename) {
	char buf[STRLEN];
	char path[STRLEN];
	struct stat st;
	FILE *in, *out;
	struct refer_ugly ugly;
	struct refer_pretty pretty;	
	char cmd[1024];

	sethomefile(buf, user->userid, filename);
	if(stat(buf, &st)<0)
		return 0;

	memset(&pretty, 0, sizeof(struct refer_pretty));

	in=fopen(buf, "r");
	if(!in) {
		printf("can not open dir file %s\n", buf);
		return 0;
	}
	sprintf(path, "%s.new", buf);
	out=fopen(path, "w");
	while(fread(&ugly, sizeof(struct refer_ugly), 1, in)>0) {
		strcpy(pretty.board, ugly.board);
		strcpy(pretty.user, ugly.user);
		strcpy(pretty.title, ugly.title);
		pretty.id=ugly.id;
		pretty.groupid=ugly.groupid;
		pretty.reid=ugly.reid;
		pretty.flag=ugly.flag;	
		pretty.time=ugly.time;

		fwrite(&pretty, sizeof(struct refer_pretty), 1, out);

	}
	fclose(in);
	fclose(out);

	sprintf(cmd, "cp %s %s.old", buf, buf);
	//printf("%s\n", cmd);
	system(cmd);
	sprintf(cmd, "cp %s %s", path, buf);
	//printf("%s\n", cmd);
	system(cmd);

	//printf("fix refer dir: %s\n", buf);	
	return 0;
}

int conv_refer_file(struct userec *user, void *arg) {
	if(user==NULL||user->userid[0]=='\0')
		return 0;

	conv_refer_dir(user, REFER_DIR);
	conv_refer_dir(user, REPLY_DIR);
	printf("fix user refer:%s\n", user->userid);
	return 0;
}

int main(int argc, char* argv[]) {
	chdir(BBSHOME);
	resolve_ucache();
	apply_users(conv_refer_file, NULL);

	return 0;	
}
