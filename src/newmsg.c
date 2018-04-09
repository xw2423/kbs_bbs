#include "bbs.h"
#include "read.h"

#ifdef ENABLE_NEW_MSG
#ifndef NEW_MSG_FROM_TELNET
#ifdef SSHBBS
#define NEW_MSG_FROM_TELNET		"SSH"
#else
#define NEW_MSG_FROM_TELNET		"Telnet"
#endif
#endif

struct new_msg_user *new_msg_users_buffer=NULL;
struct new_msg_message *new_msg_messages_buffer=NULL;
struct new_msg_display_arg {
	struct new_msg_handle *handle;
	int size;
};

int new_msg_attachment_prompt(char *buf, struct new_msg_attachment *attachment) {
	char size[STRLEN];

	if (NULL!=attachment && attachment->name[0]!=0) {
		new_msg_show_size(size, attachment->size);
		sprintf(buf, "%s (%s)", attachment->name, size);
	}
	return 0;
}
int new_msg_letters_left(struct new_msg_info *msg) {
	return NEW_MSG_TEXT_LEN-strlen(msg->msg);
}
int new_msg_write_prompt(struct userec *incept, struct new_msg_info *msg, struct new_msg_attachment *attachment) {
	char buf[STRLEN];

	move(1, 0);
	clrtoeol();
	prints("发送信息给: \033[1;32m%s\033[m ,     还可以输入 \033[1;32m%d\033[m 字", incept->userid, new_msg_letters_left(msg));
	move(2, 0);
	clrtoeol();
	if (NULL==attachment || attachment->name[0]==0)
		prints("当前尚未上传附件");
	else {
		new_msg_attachment_prompt(buf, attachment);
		prints("附件: %s", buf);
	}
	get_telnet_sessionid(buf, getSession()->utmpent);
	move(3, 0);
	clrtoeol();
	prints("附件操作地址: \033[1;4mhttp://%s/bbsupload.php?sid=%s&type=msg\033[m", get_my_webdomain(0), buf);
	move(4, 0);
	clrtoeol();
	prints("\033[1;33m-----------------按 Ctrl+q 换行---------------------------\033[m");
	return 5;
}
int new_msg_flush_attachment(struct new_msg_attachment *attachment, char *path) {
	char attach_dir[MAXPATH], attach_info[MAXPATH];
	FILE *fp;
	char buf[256];
	struct stat st;
	char *ext;

	getattachtmppath(attach_dir, MAXPATH, getSession());
	snprintf(attach_info, MAXPATH, "%s/.index", attach_dir);
	bzero(attachment, sizeof(struct new_msg_attachment));	

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
				strncpy(attachment->name, name, NEW_MSG_ATTACHMENT_NAME_LEN);
				attachment->name[NEW_MSG_ATTACHMENT_NAME_LEN]=0;
				ext=strrchr(attachment->name, '.');
				strncpy(attachment->type, get_mime_type_from_ext(ext), NEW_MSG_ATTACHMENT_TYPE_LEN);
				attachment->size=st.st_size;
				strncpy(path, buf, PATHLEN);
				path[PATHLEN]=0;
				break;
			}			
		}
		fclose(fp);
	}	
	return 0;
}

int new_msg_remove_attachment_tmp() {
	struct new_msg_attachment attachment;
	char path[PATHLEN];

	new_msg_flush_attachment(&attachment, path);
	if (attachment.size>0 && attachment.name[0] != 0) {
		upload_del_file(attachment.name, getSession());
	}
	return 0;
}
                             
int new_msg_do_send(struct userec *incept, struct new_msg_info *msg, struct new_msg_attachment *attachment, char *attachment_path) {
	struct new_msg_handle from;
	struct new_msg_handle to;
	char buf[STRLEN];
	int ret, i;

	ret=-1;
	new_msg_init(&from, getCurrentUser());
	new_msg_init(&to, incept);
	strncpy(msg->from, NEW_MSG_FROM_TELNET, NEW_MSG_FROM_LEN+1);

	if (new_msg_open(&from)<0)
		strcpy(buf, "初始化发件箱错误，请稍后重试或联系SYSOP");
	else if (new_msg_open(&to)<0) {
		strcpy(buf, "初始化对方收件箱错误，请稍后重试或联系SYSOP");
		new_msg_close(&from);
	} else {
		i=new_msg_send(&from, &to, msg, attachment, attachment_path);
		if (i>=0) {
			strcpy(buf, "信息发送成功");
			ret=0;
		} else switch(i) {
			case -4:
				strcpy(buf, "短信息内容不能为空");
				break;
			case -5:
				sprintf(buf, "短信内容过长 (%d/%d)", msg->size, NEW_MSG_MAX_SIZE);
				break;
			case -6:
				strcpy(buf, "未知的附件类型");
				break;
			case -7:
				strcpy(buf, "附件类型长度超出了系统限制");
				break;
			case -8:
				strcpy(buf, "缺少附件名称");
				break;
			case -9:
				strcpy(buf, "附件名称过长");
				break;
			case -10:
				strcpy(buf, "读取附件内容错误，请重新上传附件");
				break;
			case -11:
				sprintf(buf, "附件过大，不能超过%d", NEW_MSG_ATTACHMENT_MAX_SIZE);
				break;
			case -12:
				strcpy(buf, "信息发送失败，存储入对方收件箱时发生错误");
				break;
			case -13:
				strcpy(buf, "信息发送成功，但是未能存储本地发件箱");
				ret=1;
				break;
			case -1:
			case -2:
			case -3:
			default:
				strcpy(buf, "系统错误，请稍后重试或联系管理员");		
		}
		new_msg_close(&from);
		new_msg_close(&to);
	}

	if (ret != 0) {
		move(t_lines-2, 0);
		clrtoeol();
		prints("\033[1;31m%s\033[m", buf);
		pressanykey();
		move(t_lines-2, 0);
		clrtoeol();
	}

	return ret;
}
int new_msg_write(struct userec *incept, int mode) {
	struct new_msg_info msg;
	struct new_msg_attachment attachment;
	int row, i, edit;
	char buf[STRLEN], attachment_path[PATHLEN], attach_dir[MAXPATH];

	bzero(&msg, sizeof(struct new_msg_info));
	bzero(&attachment, sizeof(struct new_msg_attachment));
	i=new_msg_check(getCurrentUser(), incept);
	if (i!=0) {
		switch(i) {
			case -3:
				strcpy(buf, "您不能给自己发送短信");
				break;
			case -4:
				strcpy(buf, "您被封禁了Mail权限，无法发送短信");
				break;
			case -5:
				strcpy(buf, "您尚未通过注册，无法发送短信");
				break;	
			case -6:
				sprintf(buf, "%s 处于自杀状态或被封禁了Mail权限", incept->userid);
				break;
			case -7:
				sprintf(buf, "%s 拒收短信", incept->userid);
				break;
			case -8:
				sprintf(buf, "您的短信容量已用完 (%d/%d)", new_msg_get_size(getCurrentUser()), new_msg_get_capacity(getCurrentUser()));
				break;
			case -9:
				sprintf(buf, "%s 的短信容量已用完", incept->userid);
				break;
			case -10:
				strcpy(buf, "您不能给SYSOP发送短信息，若要联系请使用站内信");
				break;
			case -11:
				strcpy(buf, "您不能给guest发送短信息");
				break;
			case -12:
				strcpy(buf, "guest不能使用短信息功能，请登录后发送");
				break;
#ifdef HAVE_USERSCORE
            case -13:
                sprintf(buf, "您积分不足，不能给 %s 发送短消息", incept->userid);
                break;
#endif
			case -1:
			case -2:
			default:
				strcpy(buf, "系统错误，当前无法发送短信");
		}
		move(t_lines-2, 0);
		clrtoeol();
		prints("\033[1;31m     %s\033[m", buf);
		
		return 0;
	}

	msg.msg[0]=0;
	attachment.name[0]=0;
	clear();
	new_msg_flush_attachment(&attachment, attachment_path);
	row=new_msg_write_prompt(incept, &msg, &attachment);
	move(row, 0);
	edit=1;
	getattachtmppath(attach_dir, MAXPATH, getSession());

	while(1) {
		if (edit) {
			multi_getdata(row, 0, 79, NULL, msg.msg, NEW_MSG_TEXT_LEN, 12, 0, 0);
			edit=0;
			for (i=strlen(msg.msg)-1;i>=0;i--) {
				if (msg.msg[i]=='\r'||msg.msg[i]=='\n'||msg.msg[i]==' '||msg.msg[i]=='\t')
					msg.msg[i]=0;
				if (msg.msg[i]!=0)
					break;
			}
		}
		edit=0;
		new_msg_write_prompt(incept, &msg, &attachment);

		i=getdata(t_lines-1, 0, "操作提示: (Y)发送 (N)取消 (F)刷新附件 (E)再编辑  [Y]: ", buf, 2, DOECHO, NULL, 1);
		if (i==-1)
			return 0;
		if (buf[0]=='N' || buf[0]=='n') {
			if (msg.msg[0]==0) {
				new_msg_remove_attachment_tmp();
				return 0;
			}
			i=getdata(t_lines-1, 0, "您确定要取消发送吗? (Y/N) [N]: ", buf, 2, DOECHO, NULL, 1);
			if (i!=-1&&(buf[0]=='Y'||buf[0]=='y')) {
				new_msg_remove_attachment_tmp();
				return 0;
			}
		} else if (buf[0]=='F' || buf[0]=='f') {
			new_msg_flush_attachment(&attachment, attachment_path);
			continue;
		} else if (buf[0]=='E' || buf[0]=='e') {
			edit=1;
			continue;
		} else {
			if (msg.msg[0]==0) {
				move(t_lines-2, 0);
				clrtoeol();
				prints("信息内容为空，取消发送");
				return 0;
			}
			new_msg_flush_attachment(&attachment, attachment_path);
			if (attachment.name[0]==0 || attachment.size <= 0)
				i=new_msg_do_send(incept, &msg, NULL, NULL);
			else
				i=new_msg_do_send(incept, &msg, &attachment, attachment_path);

			if (i>=0) {
				new_msg_remove_attachment_tmp();
				if (!(mode&0x01)) {
					move(t_lines-2, 0);
					clrtoeol();
					prints("\033[1;33m        信息发送成功\033[m");
				}
				return 0;
			}
		}
	}
	return 0;
}
/**
 * mode
 * 0x01: 没有提示
 * 
 */
int new_msg_do_compose(char *user_id, int mode) {
	int old_mode;
	char uident[STRLEN];
	struct userec *user;

	if (!HAS_PERM(getCurrentUser(),PERM_LOGINOK))
		return DONOTHING;
	if (HAS_PERM(getCurrentUser(), PERM_DENYMAIL))
		return DONOTHING;
	
	old_mode=uinfo.mode;
	modify_user_mode(SMAIL);

	if (NULL==user_id || !getuser(user_id, &user)) {
		move(1, 0);
		clrtoeol();
		usercomplete("发送至: ", uident);
		if (uident[0]=='\0' || !getuser(uident, &user)) {
			clear();
			modify_user_mode(old_mode);
			return 0;
		}
	}
	
	new_msg_write(user, mode);

	if (!(mode&0x01))
	pressanykey();
	modify_user_mode(old_mode);
	
	return 0;
}
int new_msg_alert(char *msg) {
	move(t_lines-2, 0);
	clrtoeol();
	prints("\033[1;32m        %s\033[m", msg);
	pressanykey();
	return 0;
}

int new_msg_show_user(struct new_msg_user *info) {
	char content[NEW_MSG_DISPLAY_MAX_CONTENT];
	char buf[STRLEN];
	int lines=1;

	if ((&(info->msg))->flag&NEW_MSG_MESSAGE_GROUP) {
		prints(" \033[1;31m多人会话: %s\033[m\n", info->name);
		lines ++;
	}
	prints("  %s%-12s\033[m   共\033[1;31m%d\033[m条记录    \033[1;36m%s 来自%s\033[m\n", 
		((&(info->msg))->flag&NEW_MSG_MESSAGE_SEND)?"发给\033[1;33m":"    \033[1;32m",
		(&(info->msg))->user,
		info->count,
		new_msg_show_time(buf, &(info->msg), 1),
		(&(info->msg))->from
	);
	lines+=new_msg_show_info(content, &(info->msg), 1, &(info->attachment));
	prints(content);
	return lines;
}
int new_msg_show_prepare() {
	return 0;
}
int new_msg_display_messages(struct new_msg_handle *handle, struct new_msg_user *info) {
	char path[PATHLEN];
	int key;
	int repeat=0;
	int ret=SHOW_REFRESH;
	int start, count, total;
	
	count=20;
	start=0;
	if (new_msg_dump(handle, info, 0, start, count)<0)
		return SHOW_CONTINUE;
		

	total=new_msg_get_user_messages(handle, (&(info->msg))->user);
	new_msg_dump_file(path, handle, info);
	if (
            !((&(info->msg))->flag&NEW_MSG_MESSAGE_READ)
          || (&(info->msg))->flag&NEW_MSG_MESSAGE_SEND
       ) {
		new_msg_read(handle, info);
		setmailcheck(getCurrentUser()->userid); 
	}
	
	clear();
	key=ansimore2(path, 0, 0, 0);
	if (!(key==KEY_RIGHT||key==KEY_PGUP||key==KEY_PGDN || key==' ' || key==KEY_UP||key==KEY_DOWN)&&(!(key>0)||!strchr("RrEexp", key)))
		key=igetkey();
	
	do {
		if (repeat)
			key=igetkey();
		repeat=0;
		switch(key) {
			case 's':
			case 'S':
			case 'w':
			case 'W':
			case 'r':
			case 'R':
				new_msg_do_compose((&(info->msg))->user, 0x01);
				ret=-1;
				break;
			case KEY_RIGHT:
			case KEY_DOWN:
			case KEY_PGDN:
			case ' ':
				if (start+count<total) {
					start+=count;
					if (new_msg_dump(handle, info, 0, start, count)<0)
						return SHOW_REFRESH;
					clear();
					key=ansimore2(path, 0, 0, 0);
					repeat=1;
				}
				break;
			case KEY_UP:
			case KEY_PGUP:
				if (start >= count) {
					start-=count;
					if (new_msg_dump(handle, info, 0, start, count)<0)
						return SHOW_REFRESH;
					clear();
					key=ansimore2(path, 0, 0, 0);
					repeat=1;
				}
				break;
		
		}
	} while (repeat);
	clear();
	unlink(path);
	if (-1==ret) {
		new_msg_display_messages(handle, info);
		ret=SHOW_DIRCHANGE;
	}
	return ret;
}
static int new_msg_display_show_data(struct _select_def *conf, int i) {
	int j;
	for (j=0;j<NEW_MSG_DISPLAY_LINES+3;j++) {
		move((&(conf->item_pos[i-conf->page_pos]))->y, 0);
		clrtoeol();
	}
	move((&(conf->item_pos[i-conf->page_pos]))->y, (&(conf->item_pos[i-conf->page_pos]))->x);
	new_msg_show_user(&(new_msg_users_buffer[i-conf->page_pos]));
	return SHOW_CONTINUE;
}
static int new_msg_display_show_title(struct _select_def *conf) {
	char buf[200], size_s[20], capacity_s[20];

	new_msg_show_size(size_s, new_msg_get_size(getCurrentUser()));
	new_msg_show_size(capacity_s, new_msg_get_capacity(getCurrentUser()));
	sprintf(buf, "回复[\033[1;32mr\033[m] 删除[\033[1;32md\033[m] 转寄[\033[1;32mF\033[m]   已用空间: \033[1;31m%s\033[m / 总空间: \033[1;31m%s\033[m", size_s, capacity_s);
	docmdtitle("[我的短信]", buf);
	update_endline();
	return 0;
}
static int new_msg_display_prekey(struct _select_def *conf, int *key) {
	switch(*key) {
		case 'q':
			*key=KEY_LEFT;
			break;
		case ' ':
			*key=KEY_PGDN;
			break;
	}
	return SHOW_CONTINUE;
}
static int new_msg_display_select(struct _select_def *conf) {
	int i;
	struct new_msg_display_arg *arg;
	arg=(struct new_msg_display_arg *)conf->arg;
	i=new_msg_display_messages(arg->handle, &(new_msg_users_buffer[conf->pos-conf->page_pos]));

	if (i==SHOW_DIRCHANGE) {
		conf->pos=1;
		conf->page_pos=1;
	}
	return i;
}
static int new_msg_display_getdata(struct _select_def *conf, int pos, int len) {
	int i;
	struct new_msg_display_arg *arg;

	arg=(struct new_msg_display_arg *)conf->arg;
	bzero(new_msg_users_buffer, sizeof(struct new_msg_user)*arg->size);
	i=new_msg_load_users(arg->handle, conf->page_pos-1, arg->size, new_msg_users_buffer);
	if (i<0)
		return SHOW_QUIT;
	if (i==0&&conf->pos>1) {
		conf->pos=1;
		i=new_msg_load_users(arg->handle, 0, arg->size, new_msg_users_buffer);
		if(i<0)
			return SHOW_QUIT;
	}

	clear();
	return SHOW_CONTINUE;
}
int new_msg_forward(struct new_msg_handle *handle, struct new_msg_user *info) {
	static char address[IDLEN+2];
	char buf[STRLEN], user_id[IDLEN+2], path[PATHLEN], title[ARTICLE_TITLE_LEN+1];
	int i;
	struct userec *user;
	
	if (strcmp("guest", getCurrentUser()->userid) == 0) {
		clear();
		move(3, 10);
		prints("很抱歉,想转寄文章请申请正式ID!");
		pressreturn();
		return -1;
	}

	if (HAS_PERM(getCurrentUser(), PERM_DENYMAIL)) {
		clear();
		move(3, 10);
		prints("很抱歉,您目前没有Mail权限!");
		pressreturn();
		return -2;
	}
	
	if (!HAS_PERM(getCurrentUser(), PERM_BASIC) || !HAS_PERM(getCurrentUser(),PERM_LOGINOK)) {
		return -3;
	}
	
	clear();
	if (address[0] == '\0') {
		strncpy(address, getCurrentUser()->userid, IDLEN+1);
	}
	
	prints("请直接按 Enter 接受括号内提示的用户, 或者输入其他用户ID\n");
	prints("把 %s 的短信记录转寄给:", info->name);
	sprintf(buf, "[%s]: ", address);
	i=getdata(3, 0, buf, user_id, IDLEN+2, DOECHO, NULL, true);
	if (-1==i)
		return -4;
	if (!user_id[0])
		strcpy(user_id, address);
	if (!getuser(user_id, &user)) {
		prints("未找到该用户");
		pressreturn();
		return -2;
	}	
	strcpy(address, user->userid);
	address[IDLEN]=0;
	sprintf(buf, "确定将短信记录寄给 %s 吗? (Y/N) [Y]: ", address);
	i=getdata(4, 0, buf, user_id, 3, DOECHO, NULL, true);
	if (i==-1 || user_id[0]=='n' || user_id[0]=='N')
		return 0;
	
	if (!HAS_PERM(getCurrentUser(), PERM_SYSOP) && user->userlevel & PERM_SUICIDE) {
		prints("%s 自杀中，不能收信\n", address);
		pressreturn();
		return -5;
	}
	if (!HAS_PERM(getCurrentUser(), PERM_SYSOP) && !(user->userlevel & PERM_BASIC)) {
		prints("%s 没有收信的权力，不能收信\n", address);
		pressreturn();
		return -5;
	}


	if (!HAS_PERM(getCurrentUser(), PERM_SYSOP) && chkusermail(user) >= 3) {
		prints("%s 信箱已满,无法收信\n", address);
		pressreturn();
		return -4;
	}

	if (false == canIsend2(getCurrentUser(), address)) {  
		prints("\033[1m\033[33m很抱歉∶系统无法转寄此信．因为 %s 拒绝接收您的信件．\033[m\033[m\n\n", address);
		pressreturn();
		return -4;
	}	
	
	prints("转寄信件给 %s, 请稍候....\n", address);
	sprintf(title, "%.50s记录(转寄)", info->name);
	if (new_msg_dump(handle, info, 0x01, 0, 0)<0) {
		prints("读取聊天记录发生错误，请稍后再试或联系SYSOP");
		pressreturn();
		return -2;	
	}
	
	new_msg_dump_file(path, handle, info);
	i=mail_file(getCurrentUser()->userid, path, user->userid, title, BBSPOST_COPY, NULL);
	if (i==0) {
		newbbslog(BBSLOG_USER, "forwarded msg record to %s", address);
		prints("短信记录转寄完成!\n");
	} else {
		prints("短信记录转寄失败!(code: %d)\n", i);
	}
	unlink(path);
	pressanykey();
	return 0;
}
static int new_msg_display_key(struct _select_def *conf, int key) {
	struct new_msg_display_arg *arg;
	struct new_msg_user *info;
	char buf[STRLEN], ans[4];
	int i;

	arg=(struct new_msg_display_arg *)conf->arg;
	info=&(new_msg_users_buffer[conf->pos-conf->page_pos]);
	switch(key) {
		case 'r':
		case 'R':
		case 's':
		case 'S':
		case 'w':
		case 'W':
			new_msg_do_compose((&(info->msg))->user, 0x01);	
			return SHOW_DIRCHANGE;
		case 'f':
		case 'F':
			new_msg_forward(arg->handle, info);
			return SHOW_REFRESH; 
		case 'd':
		case 'D':
			move(t_lines-1, 0);
			clrtoeol();
			ans[0]=0;
			sprintf(buf, "您确实要删除%s记录吗? (Y/N) [N]:", info->name);
			i=getdata(t_lines-1, 0, buf, ans, 3, DOECHO, NULL, true);
			if (i!=-1 && (ans[0]=='y' || ans[0]=='Y')) {
				new_msg_remove_user_messages(arg->handle, (&(info->msg))->user);
				conf->item_count=new_msg_get_users(arg->handle);	
				return SHOW_DIRCHANGE;
			} else
				return SHOW_REFRESH;

	}
	return SHOW_CONTINUE;
}
int new_msg_show_users() {
	struct _select_def group_conf;
	POINT *pts;
	int i, size, total, rows;
	struct new_msg_handle handle;
	struct new_msg_display_arg display_arg;
	
	new_msg_init(&handle, getCurrentUser());
	new_msg_open(&handle);

	size=(t_lines-1)/(NEW_MSG_DISPLAY_LINES+3);
	total=new_msg_get_users(&handle);
	new_msg_users_buffer=(struct new_msg_user *)malloc(sizeof(struct new_msg_user)*size);
	bzero(new_msg_users_buffer, sizeof(struct new_msg_user)*size);
	rows=new_msg_load_users(&handle, 0, size, new_msg_users_buffer);
	
	bzero(&group_conf, sizeof(struct _select_def));
	pts=(POINT *)malloc(sizeof(POINT)*size);
	bzero(pts, sizeof(POINT)*size);

	for (i=0;i<size;i++) {
		pts[i].x=0;
		pts[i].y=2+i*(NEW_MSG_DISPLAY_LINES+3);
	}

	group_conf.item_per_page=size;
	group_conf.flag = LF_VSCROLL | LF_BELL | LF_MULTIPAGE | LF_LOOP;
	group_conf.prompt = "◆";
	group_conf.item_pos = pts;
	group_conf.title_pos.x = 0;
	group_conf.title_pos.y = 0;
	group_conf.pos = 1;
	group_conf.page_pos = 1;

	group_conf.item_count = total;
	group_conf.show_data = new_msg_display_show_data;
	group_conf.show_title = new_msg_display_show_title;
	group_conf.pre_key_command = new_msg_display_prekey;
	group_conf.on_select = new_msg_display_select;
	group_conf.get_data = new_msg_display_getdata;
	group_conf.key_command = new_msg_display_key;

	display_arg.handle=&handle;
	display_arg.size=size;
	group_conf.arg=&display_arg;

	clear();
	list_select_loop(&group_conf);

	new_msg_close(&handle);
	free(new_msg_users_buffer);
	free(pts);

	new_msg_users_buffer=NULL;

	return 0;
}

/* 短信主函数，通过 mail 界面调用 */
int new_msg_main(void) {
	new_msg_show_users();
	return FULLUPDATE;
}

/* 发送一条短信 */
int new_msg_compose(void) {
	new_msg_do_compose(NULL, 0);
	return FULLUPDATE;
}

#endif
