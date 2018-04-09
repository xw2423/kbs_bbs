#include "bbs.h"
#include "md5.h"
#include "sqlite3.h"

/* new msg system, windinsn, Jan 21,2013 */
#ifdef ENABLE_NEW_MSG
#ifndef NEW_MSG_LIB
#define NEW_MSG_LIB

#define NEW_MSG_SQL_SELECT_MESSAGE_FULL "[id],[key],[user],strftime('%s',[time], 'utc'),[host],[from],[msg],[msg_size],[flag],[attachment_type],[attachment_size],[attachment_name]"
#define NEW_MSG_SQL_SELECT_USER_FULL "[id], [msg_id],[key], [user], strftime('%s',[time], 'utc'), [host], [from], [msg], [msg_size],[flag],[attachment_type],[attachment_size], [attachment_name],  [count], [name]"
#define NEW_MSG_SQL_SELECT_MEMBER_FULL "[id], [key], [user], [flag]"

#define NEW_MSG_BORDER_TOP_LEFT        "┏"
#define NEW_MSG_BORDER_TOP_RIGHT       "┓"
#define NEW_MSG_BORDER_BOTTOM_LEFT     "┗"
#define NEW_MSG_BORDER_BOTTOM_RIGHT    "┛"
#define NEW_MSG_BORDER_HOR             "━"
#define NEW_MSG_BORDER_VER             "┃"

int new_msg_show_size(char *buf, int size) {	
	if (size > 1024*1024)		
		sprintf(buf, "%dM", size/1024/1024);	
	else if (size > 1024)		
		sprintf(buf, "%dK", size/1024);	
	else		
		sprintf(buf, "%dbyte", size);	
	return 0;
}

unsigned long new_msg_ip2long(const char *ip)
{
	unsigned long int addr;
	int len=sizeof(ip);
	if (len==0||(addr=inet_addr(ip))==INADDR_NONE) {
		if (len==sizeof("255.255.255.255")-1 && !memcmp(ip, "255.255.255.255", sizeof("255.255.255.255")-1))
			return 0xFFFFFFFF;
		return 0;
	}	
	return ntohl(addr);
}

typedef int new_msg_member_cmp_func (const void *, const void *);
int new_msg_member_cmp(struct new_msg_member *a, struct new_msg_member *b) {
	return strcasecmp(a->user, b->user);
}

int new_msg_set_key(struct new_msg_info *msg, struct new_msg_member *members, int count) {
	char str[(IDLEN+1)*NEW_MSG_MAX_MEMBERS+1];
	char ret[33];
	unsigned char out[33];
	int i;
	MD5_CTX md5;

	str[0]=0;
	ret[0]=0;

	qsort(members, count, sizeof(struct new_msg_member), (new_msg_member_cmp_func *) new_msg_member_cmp);
	for (i=0;i<count;i++) {
		strcat(str, members[i].user);
		if (i<count-1) strcat(str, ",");
	}

	MD5Init(&md5);
	MD5Update(&md5, (unsigned char *) str, strlen(str));
	MD5Final((unsigned char*)ret, &md5);
	to64frombits(out, (unsigned char *)ret, 32);
	strncpy(msg->key, (char *)out, 	NEW_MSG_KEY_LEN+1);

	return 0;
}

/**
  * 获取数据库中的某些数值
  * @param int type:   1 -> COUNT
  *                    2 -> SUM
  * @param void arg:   type==2 时为求和项 (char *)
  */
int new_msg_query_number(struct new_msg_handle *handle, int type, void *arg, char *table, char *where) {
	char *sql;
	int size, count;
	sqlite3_stmt  *stmt=NULL;
	char *fields=NULL;
	
	size=100+strlen(table);
	if (NULL!=where)
		size += strlen(where);
	if (NULL!=arg) {
		fields=(char *)arg;
		size += strlen(fields);
	}
	
	sql=(char *)malloc(size);
	if (!sql)
		return -1;
	bzero(sql, size);
	
	switch(type) {
		case 1:
			sprintf(sql, "SELECT COUNT(*) FROM [%s] ", table);
			break;
		case 2:
			if (!fields || !fields[0]) {
				free(sql);
				return -3;
			}
			sprintf(sql, "SELECT SUM(%s) FROM [%s] ", fields, table);
			break;
		default:
			free(sql);
			return -2;
	}
	
	if (NULL!=where) {
		strcat(sql, " WHERE ");
		strcat(sql, where);
	}
		
	if (SQLITE_OK != sqlite3_prepare(handle->db, sql, strlen(sql), &stmt, NULL)) {
		if (stmt)
			sqlite3_finalize(stmt);
		free(sql);
		return -4;
	}
	
	if (SQLITE_ROW == sqlite3_step(stmt)) 
		count=sqlite3_column_int(stmt, 0);
	else
		count=0;
	
	sqlite3_finalize(stmt);
	free(sql);
	
	return count;
}

int new_msg_count(struct new_msg_handle *handle, char *table, char *where) {
	return new_msg_query_number(handle, 1, NULL, table, where);
}

int new_msg_sum(struct new_msg_handle *handle, char *fields, char *table, char *where) {
	return new_msg_query_number(handle, 2, fields, table, where);
}

int new_msg_delete(struct new_msg_handle *handle, char *table, char *where) {
	char *sql;
	int size, ret;
	char *errmsg=NULL; 
	
	size=50+strlen(table);
	if (NULL!=where)
		size += strlen(where);
	
	sql=(char *)malloc(size);
	if (!sql)
		return -1;
	bzero(sql, size);
	if (NULL==where)
		sprintf(sql, "DELETE FROM [%s];", table);
	else
		sprintf(sql, "DELETE FROM [%s] WHERE %s;", table, where);
		
	if (SQLITE_OK==sqlite3_exec(handle->db, sql, NULL, NULL, &errmsg)) {
		ret=sqlite3_changes(handle->db);
		if (ret>0) sqlite3_exec(handle->db, "vacuum;", NULL, NULL, &errmsg);
	} else
		ret=-2;
	
	free(sql);
	return ret;
}

int new_msg_fill_msg(struct new_msg_info *msg, sqlite3_stmt  *stmt, int offset) {
	int pos;

	pos=offset;
	strncpy(msg->key, (char *)sqlite3_column_text(stmt, pos++), NEW_MSG_KEY_LEN+1);
	strncpy(msg->user, (char *)sqlite3_column_text(stmt, pos++), IDLEN+1);
	msg->time=(time_t) sqlite3_column_int64(stmt, pos++);
	msg->host=(unsigned long) sqlite3_column_int64(stmt, pos++);
	strncpy(msg->from, (char *)sqlite3_column_text(stmt, pos++), NEW_MSG_FROM_LEN+1);
	strncpy(msg->msg, (char *)sqlite3_column_text(stmt, pos++), NEW_MSG_TEXT_LEN+1);
	msg->size=(unsigned int)sqlite3_column_int(stmt, pos++);
	msg->flag=(unsigned long) sqlite3_column_int64(stmt, pos++);
	return pos;
}

int new_msg_fill_attachment(struct new_msg_attachment *attachment, sqlite3_stmt  *stmt, int offset) {
	int pos;

	pos=offset;
	strncpy(attachment->type, (char *)sqlite3_column_text(stmt, pos++), NEW_MSG_ATTACHMENT_TYPE_LEN+1);
	attachment->size=(unsigned int)sqlite3_column_int(stmt, pos++);
	strncpy(attachment->name, (char *)sqlite3_column_text(stmt, pos++), NEW_MSG_ATTACHMENT_NAME_LEN+1);
	return pos;
}

int new_msg_fill_user(struct new_msg_user *info, sqlite3_stmt  *stmt) {
	int offset=0;

	info->id=(long)sqlite3_column_int64(stmt, offset++);
	info->msg_id=(long)sqlite3_column_int64(stmt, offset++);
	offset=new_msg_fill_msg(&(info->msg), stmt, offset);
	offset=new_msg_fill_attachment(&(info->attachment), stmt, offset);
	info->count=(unsigned int)sqlite3_column_int64(stmt, offset++);
	strncpy(info->name, (char *)sqlite3_column_text(stmt, offset++), NEW_MSG_NAME_LEN);
	(&(info->attachment))->id=info->msg_id;
	return 0;
}

int new_msg_fill_message(struct new_msg_message *message, sqlite3_stmt  *stmt) {
	int offset=0;

	message->id=(long) sqlite3_column_int64(stmt, offset++);
	offset=new_msg_fill_msg(&(message->msg), stmt, offset);
	if ((&(message->msg))->flag&NEW_MSG_MESSAGE_ATTACHMENT)
		offset=new_msg_fill_attachment(&(message->attachment), stmt, offset);
	else {
		(&(message->attachment))->name[0]=0;
		(&(message->attachment))->type[0]=0;
		(&(message->attachment))->size=0;
	}
	(&(message->attachment))->id=message->id;
	return 0;
}

int new_msg_init(struct new_msg_handle *handle, struct userec *user) {
	bzero(handle, sizeof(struct new_msg_handle));
	strncpy(handle->user, user->userid, IDLEN+1);
	handle->flag=0;
	return 0; 
}

int new_msg_open(struct new_msg_handle *handle) {
	struct userec *user;
	struct stat st;
	char path[PATHLEN];
	
	if (handle->flag&NEW_MSG_HANDLE_OK)
		return 0;
	
	if (!handle->user[0])
		return -1;
	if (!getuser(handle->user, &user))
		return -1;
	if (strcasecmp(user->userid, "guest")==0)
		return -2;
	if (strcasecmp(user->userid, "SYSOP")==0)
		return -3;	
	strncpy(handle->user, user->userid, IDLEN+1);
	sethomefile(path, handle->user, NEW_MSG_DB);
	
	if (stat(path, &st)<0 && f_cp(NEW_MSG_INIT_DB, path, 0)!=0)
		return -3;
		
	if (SQLITE_OK!=sqlite3_open(path, &handle->db))
		return -4;
		
	handle->flag |= NEW_MSG_HANDLE_OK;	
		
	return 0;
}

int new_msg_close(struct new_msg_handle *handle) {
	if (NULL != handle->db && SQLITE_OK!=sqlite3_close(handle->db))
		return -1;
	
	handle->flag &= ~NEW_MSG_HANDLE_OK;
	return 0;
}

int new_msg_get_capacity(struct userec *user) {
	if (HAS_PERM(user, PERM_SYSOP))
		return 0;
		
	return NEW_MSG_CAPACITY;
}

#ifdef HAVE_USERSCORE
/* 积分低于2k，不允许给非粉丝发短信息 */
int sufficient_score_to_sendnewmsg(struct userec *fromuser, struct userec *touser) {
    char path[STRLEN];

    if (HAS_PERM(fromuser, PERM_BMAMANGER) || HAS_PERM(fromuser, PERM_SYSOP)
            || HAS_PERM(fromuser, PERM_ADMIN) || HAS_PERM(fromuser, PERM_JURY)
            || fromuser->score_user>=publicshm->sendmailscorelimit)
        return 1;
    if (!touser)
        return 0;
    if (strcasecmp(touser->userid, "SYSOP") && strcasecmp(touser->userid, "Arbitrator")) {
        sethomefile(path, touser->userid, "friends");
        if (!search_record(path, NULL, sizeof(struct friends), (RECORD_FUNC_ARG)cmpfnames, fromuser->userid))
            return 0;
    }
    return 1;
}
#endif
/**
  * 检查发送权限
  * @return 0: 没有问题
  *        -1: 找不到发件人
  *        -2: 找不到收件人
  *        -3: 发件人、收件人为同一用户
  *        -4: 发件人封禁mail权限
  *        -5: 发件人没有通过注册
  *        -6: 收件人自杀或者封禁mail权限
  *        -7: 收件人拒收   
  *        -8: 发件人超容， 短信息容量为 new_msg_get_capacity(from)
  *        -9: 收件人超容， 短信息容量为 new_msg_get_capacity(to)
  *        -10: 不能发给SYSOP
  *        -11: 不能发给guest
  *        -12: guest不能发
  *        -13: 积分不足 //#ifdef HAVE_USERSCORE
  *        
  */
int new_msg_check(struct userec *from, struct userec *to) {
	int size, used;
	
	if (strcasecmp(to->userid, "SYSOP")==0)
		return -10;
	if (strcasecmp(to->userid, "guest")==0)
		return -11;
	if (strcasecmp(from->userid, "guest")==0)
		return -12;

	if (!from || !from->userid[0])
		return -1;
	if (!to || !to->userid[0])
		return -2;
		
	if (!strcasecmp(from->userid, to->userid))
		return -3;
		
	if (HAS_PERM(from, PERM_DENYMAIL))
		return -4;	
	if (!HAS_PERM(from, PERM_LOGINOK))
		return -5;
		
	if (HAS_PERM(from, PERM_SYSOP))
		return 0;
		
	if (to->userlevel & PERM_SUICIDE)
		return -6;
	if (!(to->userlevel & PERM_BASIC))
		return -6;
	if (!canIsend2(from, to->userid))
		return -7;
	
#ifdef HAVE_USERSCORE
    if (!sufficient_score_to_sendnewmsg(from, to))
        return -13;
#endif
	size=new_msg_get_capacity(from);
	if (size!=0) {
		used=new_msg_get_size(from);
		if (used >= size)
			return -8;
	}
	
	size=new_msg_get_capacity(to);
	if (size!=0) {
		used=new_msg_get_size(to);
		if (used >= size)
			return -9;
	}
	
	return 0;
}

long new_msg_user_info(struct new_msg_handle *handle, char *user_id, struct new_msg_user *info) {
	char sql[300];
	sqlite3_stmt  *stmt=NULL;
	long id;
	
	if (NULL==info) {
		sprintf(sql, "SELECT [id] FROM [%s] WHERE [user]='%s' LIMIT 1;",
			NEW_MSG_TABLE_USER,
			user_id
		);
	} else {
		sprintf(sql, "SELECT %s FROM [%s] WHERE [user]='%s' LIMIT 1",
			NEW_MSG_SQL_SELECT_USER_FULL,
			NEW_MSG_TABLE_USER,
			user_id
		);
	}
	
	if (SQLITE_OK != sqlite3_prepare(handle->db, sql, strlen(sql), &stmt, NULL)) {
		if (stmt)
			sqlite3_finalize(stmt);
		
		return -1;
	}
	
	if (SQLITE_ROW == sqlite3_step(stmt)) {
		id=(long) sqlite3_column_int64(stmt, 0);
		if (NULL!=info && new_msg_fill_user(info, stmt)<0) {
			sqlite3_finalize(stmt);
			return -2;
		}
	} else {
		id=0;
	}
	sqlite3_finalize(stmt);
	
	return id;
}

int new_msg_get_user_messages(struct new_msg_handle *handle, char *user_id) {
	char buf[STRLEN];
	
	sprintf(buf, " [user]='%s' ", user_id);
	return new_msg_count(handle, NEW_MSG_TABLE_MESSAGE, buf);
}

int new_msg_update_user(struct new_msg_handle *handle, char *user_id, struct new_msg_message *record) {
	struct new_msg_message *message;
	struct new_msg_message query;
	int size, count;
	long id;
	char *sql;
	sqlite3_stmt  *stmt=NULL;
	char name[NEW_MSG_NAME_LEN];
	
	if (NULL==record || record->id<=0 || strcmp(user_id, (&(record->msg))->user)!=0) {
		id=new_msg_last_user_message(handle, user_id, &query);
		if (id<0) {
			return -1;
		}
		if (id==0) {
			message=NULL;
		} else {
			message=&query;
		}
	} else {
		message=record;
	}

	id=new_msg_user_info(handle, user_id, NULL);
	if (id<0) {
		return -2;
	}
	
	count=new_msg_get_user_messages(handle, user_id);
	if (count<0) {
		return -3;
	}
	
	size=1024;
	if (NULL!=message)
		size+=(&(message->msg))->size*2;
		
	sql=(char *)malloc(size);
	
	if (!sql) {
		return -4;
	}
	
	bzero(sql, size);
	
	if (count>0 && NULL!=message && id==0) {
		// 有message记录，无user记录
		sprintf(sql, "INSERT INTO [%s] ([msg_id], [user], [time], [host], [from], [msg], [msg_size], [count], [flag], [attachment_type], [attachment_size], [attachment_name], [key], [name]) VALUES (%ld, '%s', datetime(%lu, 'unixepoch', 'localtime'), %lu, ?, ?, %u, %u, %lu, ?, %d, ?, ?, ?);",
			NEW_MSG_TABLE_USER,
			message->id, 
			(&(message->msg))->user,
			(&(message->msg))->time,
			(&(message->msg))->host,
			(&(message->msg))->size,
			count,
			(&(message->msg))->flag,
			(&(message->attachment))->size
		);
	} else if (count>0 && NULL!=message && id>0) {
		// 有message记录和user记录
		sprintf(sql, "UPDATE [%s] SET [msg_id]=%ld, [user]='%s', [time]=datetime(%lu, 'unixepoch', 'localtime'), [host]=%lu, [from]=?, [msg]=?, [msg_size]=%u, [count]=%u, [flag]=%lu, [attachment_type]=?, [attachment_size]=%d, [attachment_name]=?, [key]=?, [name]=? WHERE [id]=%ld;",
			NEW_MSG_TABLE_USER,
			message->id, 
			(&(message->msg))->user,
			(&(message->msg))->time,
			(&(message->msg))->host,
			(&(message->msg))->size,
			count,
			(&(message->msg))->flag,
			(&(message->attachment))->size,
			id
		);
	} else if ((count==0 || NULL==message) && id>0) {
		// 无message记录，有user记录
		sprintf(sql, "DELETE FROM [%s] WHERE [id]=%ld LIMIT 1;", 
			NEW_MSG_TABLE_USER,
			id
		);
	} else {
		// 会出现么?
		free(sql);
		return -6;
	}
	
	if (SQLITE_OK != sqlite3_prepare(handle->db, sql, size, &stmt, NULL)) {
		if (stmt)
			sqlite3_finalize(stmt);
		free(sql);
		return -7;
	}
	
	if (count>0 && NULL!=message) {
		// 有message记录
		strncpy(name, message->name, NEW_MSG_NAME_LEN+1);
		sqlite3_bind_text(stmt, 1, (&(message->msg))->from, strlen((&(message->msg))->from), NULL);
		sqlite3_bind_text(stmt, 2, (&(message->msg))->msg, strlen((&(message->msg))->msg), NULL);
		sqlite3_bind_text(stmt, 3, (&(message->attachment))->type, strlen((&(message->attachment))->type), NULL);
		sqlite3_bind_text(stmt, 4, (&(message->attachment))->name, strlen((&(message->attachment))->name), NULL);
		sqlite3_bind_text(stmt, 5, (&(message->msg))->key, strlen((&(message->msg))->key), NULL);
		sqlite3_bind_text(stmt, 6, name, strlen(name), NULL);
	}
	
	if (SQLITE_DONE != sqlite3_step(stmt)) {
		sqlite3_finalize(stmt);
		free(sql);
		return -8;
	}
	
	sqlite3_finalize(stmt);
	free(sql);
	
	return 0;
}

long new_msg_last_user_message(struct new_msg_handle *handle, char *user_id, struct new_msg_message *message) {
	char sql[300];
	sqlite3_stmt  *stmt=NULL;
	long id;
	
	if (NULL==message) {
		sprintf(sql, "SELECT [id] FROM [%s] WHERE [user]='%s' ORDER BY [time] DESC LIMIT 1;",
			NEW_MSG_TABLE_MESSAGE,
			user_id
		);
	} else {
		sprintf(sql, "SELECT %s FROM [%s] WHERE [user]='%s' ORDER BY [time] DESC LIMIT 1",
			NEW_MSG_SQL_SELECT_MESSAGE_FULL,
			NEW_MSG_TABLE_MESSAGE,
			user_id
		);
	}
	
	if (SQLITE_OK != sqlite3_prepare(handle->db, sql, strlen(sql), &stmt, NULL)) {
		if (stmt)
			sqlite3_finalize(stmt);
		
		return -1;
	}
	
	if (SQLITE_ROW == sqlite3_step(stmt)) {
		id=(long) sqlite3_column_int64(stmt, 0);
		if (NULL!=message && new_msg_fill_message(message, stmt)<0) {
			sqlite3_finalize(stmt);
			return -2;
		}
	} else {
		id=0;
	}
	sqlite3_finalize(stmt);
	
	return id;
}

int new_msg_create(struct new_msg_handle *handle, struct new_msg_message *message, char *attachment_file) {
	char *sql;
	int size;
	sqlite3_stmt  *stmt=NULL;
	struct stat st;
	FILE *fp;
	char *attachment_content;

	if ((&(message->msg))->flag&NEW_MSG_MESSAGE_ATTACHMENT) {
		if (stat(attachment_file, &st)<0)
			return -1;
		fp=fopen(attachment_file, "rb");
		if (fp==NULL) 
			return -2;	
		fseek(fp, 0, SEEK_END);
		(&(message->attachment))->size=ftell(fp);
		fseek(fp, 0, SEEK_SET);
		attachment_content=(char *)malloc((&(message->attachment))->size+1);
		if (!attachment_content) {
			fclose(fp);
			return -3;
		}
		bzero(attachment_content, (&(message->attachment))->size+1);
		fread(attachment_content, 1, (&(message->attachment))->size, fp);
		fclose(fp);
	}

	message->id=0;
	size=1024+(&(message->msg))->size*2+(&(message->attachment))->size*2;
	sql=(char *)malloc(size);
	if (!sql) {
		if ((&(message->msg))->flag&NEW_MSG_MESSAGE_ATTACHMENT)
			free(attachment_content);
		return -4;
	}

	bzero(sql, size);
	
	sprintf(sql, "INSERT INTO [%s] ([user], [time], [host], [from], [msg], [msg_size], [attachment_type], [attachment_size], [attachment_name], [attachment_body], [flag], [key]) VALUES ('%s', datetime(%lu, 'unixepoch', 'localtime'), '%lu', ?, ?, %u, ?, %u, ?, ?, %lu, ?);", 
		NEW_MSG_TABLE_MESSAGE,
		(&(message->msg))->user, 
		(&(message->msg))->time,
		(&(message->msg))->host,
		(&(message->msg))->size,
		(&(message->attachment))->size,
		(&(message->msg))->flag
	);
	if (SQLITE_OK != sqlite3_prepare(handle->db, sql, size, &stmt, NULL)) {
		if (stmt)
			sqlite3_finalize(stmt);
		free(sql);
		if ((&(message->msg))->flag&NEW_MSG_MESSAGE_ATTACHMENT)
			free(attachment_content);
		return -5;
	}
	
	sqlite3_bind_text(stmt, 1, (&(message->msg))->from, strlen((&(message->msg))->from), NULL);
	sqlite3_bind_text(stmt, 2, (&(message->msg))->msg, strlen((&(message->msg))->msg), NULL);
	if ((&(message->msg))->flag&NEW_MSG_MESSAGE_ATTACHMENT) {
		sqlite3_bind_text(stmt, 3, (&(message->attachment))->type, strlen((&(message->attachment))->type), NULL);
		sqlite3_bind_text(stmt, 4, (&(message->attachment))->name, strlen((&(message->attachment))->name), NULL);
		sqlite3_bind_blob(stmt, 5, attachment_content, (&(message->attachment))->size, NULL);

		
	} else {
		sqlite3_bind_null(stmt, 3);
		sqlite3_bind_null(stmt, 4);
		sqlite3_bind_null(stmt, 5);
	}
	sqlite3_bind_text(stmt, 6, (&(message->msg))->key, strlen((&(message->msg))->key), NULL);
	if (SQLITE_DONE != sqlite3_step(stmt)) {
		sqlite3_finalize(stmt);
		free(sql);
		if ((&(message->msg))->flag&NEW_MSG_MESSAGE_ATTACHMENT)
			free(attachment_content);	
		return -6;
	}
	message->id=(long)sqlite3_last_insert_rowid(handle->db);
	sqlite3_finalize(stmt);
	free(sql);
	
	if ((&(message->msg))->flag&NEW_MSG_MESSAGE_ATTACHMENT)
		free(attachment_content);

	if (new_msg_update_user(handle, (&(message->msg))->user, message)<0)
		return -7;
	
	return 0;
}

/**
  * 发送一条短消息，此函数仅进行发送操作，未检查权限和变量初始化
  * @return  0: 成功
  *         -1: sender未初始化
  *         -2: incept未初始化
  *         -3: msg->from 未设定，该参数反应发送的方式，如Telnet/nForum/Android等
  *         -4: msg->msg 为空，即空消息
  *         -5: msg->size 过长，即短消息过长， 最大长度为 NEW_MSG_MAX_SIZE
  *         -6: attachment->type 为空，即缺少附件的MIME类型
  *         -7: attachment->type 过长，即附件的MIME过长，最大长度 NEW_MSG_ATTACHMENT_TYPE_LEN
  *         -8: attachment->name 为空，即缺少附件的名称
  *         -9: attachment->name 过长，即附件文件名过长, 最大长度 NEW_MSG_ATTACHMENT_NAME_LEN
  *         -10: attachment->body 为空，即附件内容为空
  *         -11: attachment->size 过大，即附件过大，最大文件大小为 NEW_MSG_ATTACHMENT_MAX_SIZE
  *         -12: 发送失败，接收方存储信息时失败
  *         -13: 发送失败，发送方存储信息时失败，此时接收方能接受信息
  *         -14: 系统错误，内存不足?
  */
int new_msg_send(struct new_msg_handle *sender, struct new_msg_handle *incept, struct new_msg_info *msg, struct new_msg_attachment *attachment, char *attachment_file) {
	struct new_msg_message message;
	int flag;
	struct stat st;	
	//struct new_msg_member *members;
	
	if (!(sender->flag&NEW_MSG_HANDLE_OK))
		return -1;
	if (!(incept->flag&NEW_MSG_HANDLE_OK))
		return -2;
	
	if (!msg->time)
		msg->time=time(NULL);
	if (!msg->host)
		msg->host=new_msg_ip2long(getSession()->fromhost);
	if (!msg->from)
		return -3;
	if (!msg->msg)
		return -4;
	msg->size=strlen(msg->msg);
	if (msg->size > NEW_MSG_MAX_SIZE || msg->size <= 0)
		return -5;
		
	flag=0;
	if (NULL!=attachment) {
		flag |= NEW_MSG_MESSAGE_ATTACHMENT;
		
		if (!attachment->type)
			return -6;
		if (strlen(attachment->type)>NEW_MSG_ATTACHMENT_TYPE_LEN)
			return -7;
		if (!attachment->name)
			return -8;
		if (strlen(attachment->name)>NEW_MSG_ATTACHMENT_NAME_LEN)
			return -9;
		if (stat(attachment_file, &st)<0)
			return -10;
		attachment->size=st.st_size;
		if (attachment->size > NEW_MSG_ATTACHMENT_MAX_SIZE)
			return -11;
			
		if (strncmp(attachment->type, "image/", strlen("image/"))==0)
			flag |= NEW_MSG_MESSAGE_IMAGE;
	}
	
	/* 初始化收件人信息 */
	strncpy(message.msg.user, sender->user, IDLEN+1);
	message.msg.time=msg->time;
	message.msg.host=msg->host;
	strncpy(message.msg.from, msg->from, NEW_MSG_FROM_LEN+1);
	strncpy(message.msg.msg, msg->msg, NEW_MSG_TEXT_LEN+1);
	message.msg.size=strlen(message.msg.msg);
	message.msg.flag=flag;

	/*
	members=(struct new_msg_member *)malloc(sizeof(struct new_msg_member)*2);
	bzero(members, sizeof(struct new_msg_member)*2);
	strncpy(members[0].user, sender->user, IDLEN+1);
	strncpy(members[1].user, incept->user, IDLEN+1);
	new_msg_set_key(&(message.msg), members, 2);
	free(members);
	*/

	if (message.msg.flag&NEW_MSG_MESSAGE_ATTACHMENT) {
		strncpy(message.attachment.type, attachment->type, NEW_MSG_ATTACHMENT_TYPE_LEN+1);
		message.attachment.size=attachment->size;
		strncpy(message.attachment.name, attachment->name, NEW_MSG_ATTACHMENT_NAME_LEN+1);
	} else {
		message.attachment.size=0;
		message.attachment.name[0]=0;
		message.attachment.type[0]=0;
	}

	sprintf(message.name, "与 %s 的对话", sender->user);	
	strncpy(message.msg.key, sender->user, IDLEN+1);
	message.msg.key[IDLEN+1]=0;
	
	// windinsn, 2013.07.26 filter	
	if(check_badword_str(msg->msg, msg->size, getSession())){
		newbbslog(BBSLOG_USER, "want to send filtered message to '%s' ", incept->user);	
		// 有关键字的直接拦截
	} else {
		if (new_msg_create(incept, &message, attachment_file)<0) {
			return -12;
		}
	}
	
	message.msg.flag|=NEW_MSG_MESSAGE_SEND;
	message.msg.flag|=NEW_MSG_MESSAGE_READ;
	strncpy(message.msg.user, incept->user, IDLEN+1);
	sprintf(message.name, "与 %s 的对话", incept->user);
	strncpy(message.msg.key, incept->user, IDLEN+1);
	message.msg.key[IDLEN+1]=0;
	if (new_msg_create(sender, &message, attachment_file)<0) {
		return -13;
	}
		
	setmailcheck(incept->user);
	newbbslog(BBSLOG_USER, "sent new message to '%s' (size: %d, attachment: [%d]%s)", incept->user, message.msg.size, message.attachment.size, message.attachment.name);	
	
	return 0;
}

int new_msg_get_users(struct new_msg_handle *handle) {
	return new_msg_count(handle, NEW_MSG_TABLE_USER, NULL);
}

int new_msg_load_users(struct new_msg_handle *handle, int start, int count, struct new_msg_user *users) {
	char sql[300];
	sqlite3_stmt  *stmt=NULL;
	int i;
	
	if (NULL==users)
		return -1;
	
	sprintf(sql, "SELECT %s FROM [%s] ORDER BY [time] DESC LIMIT %d, %d;",
		NEW_MSG_SQL_SELECT_USER_FULL,
		NEW_MSG_TABLE_USER,
		start,
		count
	);
	if (SQLITE_OK != sqlite3_prepare(handle->db, sql, strlen(sql), &stmt, NULL)) {
		if (stmt)
			sqlite3_finalize(stmt);
		
		return -2;
	}
	
	i=0;
	while (SQLITE_ROW == sqlite3_step(stmt)) {
		if (new_msg_fill_user(&(users[i]), stmt)<0) {
			sqlite3_finalize(stmt);
			return -3;
		}
		i++;
	}
	sqlite3_finalize(stmt);
	
	return i;
}

int new_msg_load_user_messages(struct new_msg_handle *handle, char *user_id, int start, int count, struct new_msg_message *messages) {
	char sql[300];
	sqlite3_stmt  *stmt=NULL;
	int i;
	
	if (NULL==messages)
		return -1;
	
	sprintf(sql, "SELECT %s FROM [%s] WHERE [user]='%s' ORDER BY [time] DESC LIMIT %d, %d;",
		NEW_MSG_SQL_SELECT_MESSAGE_FULL,
		NEW_MSG_TABLE_MESSAGE,
		user_id,
		start,
		count
	);
	
	if (SQLITE_OK != sqlite3_prepare(handle->db, sql, strlen(sql), &stmt, NULL)) {
		if (stmt)
			sqlite3_finalize(stmt);
		
		return -2;
	}
	i=0;
	while (SQLITE_ROW == sqlite3_step(stmt)) {
		if (new_msg_fill_message(&(messages[i]), stmt)<0) {
			sqlite3_finalize(stmt);
			return -3;
		}
		i++;
	}
	sqlite3_finalize(stmt);
	
	return i;
}

long new_msg_get_message(struct new_msg_handle *handle, long id, struct new_msg_message *message) {
	char sql[300];
	sqlite3_stmt  *stmt=NULL;
	long ret;
	
	if (NULL==message)
		return -1;
	if (id<=0)
		return -2;
	
	sprintf(sql, "SELECT %s FROM [%s] WHERE [id]=%ld LIMIT 1;",
		NEW_MSG_SQL_SELECT_MESSAGE_FULL,
		NEW_MSG_TABLE_MESSAGE,
		id
	);
	
	if (SQLITE_OK != sqlite3_prepare(handle->db, sql, strlen(sql), &stmt, NULL)) {
		if (stmt)
			sqlite3_finalize(stmt);
		
		return -3;
	}
	
	if (SQLITE_ROW == sqlite3_step(stmt)) {
		if (new_msg_fill_message(message, stmt)<0) {
			sqlite3_finalize(stmt);
			return -4;
		}
		ret=message->id;
	} else {
		ret=0;
	}
	sqlite3_finalize(stmt);
	
	return ret;
}

int new_msg_delete_message(struct new_msg_handle *handle, struct new_msg_message *message) {
	char buf[100];
	int ret;
	
	if (NULL==message || message->id<=0 || !(&(message->msg))->user[0])
		return -1;
	
	sprintf(buf, " [id]=%ld ", message->id);
	ret=new_msg_delete(handle, NEW_MSG_TABLE_MESSAGE, buf);
	if (ret<0)
		return -2;
	if (ret>0 && new_msg_update_user(handle, (&(message->msg))->user, NULL)<0)
		return -3;
	return ret;
}

int new_msg_remove_user_messages(struct new_msg_handle *handle, char *user_id) {
	char buf[100];
	
	if (NULL==user_id || !user_id[0])
		return -1;
	sprintf(buf, " [user]='%s' ", user_id);
	if (new_msg_delete(handle, NEW_MSG_TABLE_MESSAGE, buf)<0)
		return -2;
	if (new_msg_delete(handle, NEW_MSG_TABLE_USER, buf)<0)
		return -3;
	return 0;
}

int new_msg_get_size(struct userec *user) {
	struct stat st;
	char path[PATHLEN];

	sethomefile(path, user->userid, NEW_MSG_DB);
	if (stat(path, &st)<0)
		return 0;

	return st.st_size;	
}

int new_msg_read(struct new_msg_handle *handle, struct new_msg_user *info) {
	char sql[300];
	char *errmsg=NULL; 
	
	if ((&(info->msg))->flag&NEW_MSG_MESSAGE_READ && !((&info->msg)->flag&NEW_MSG_MESSAGE_SEND))
		return 0;
	
	sprintf(sql, "UPDATE [%s] SET [flag]=[flag]|%d WHERE [user]='%s';",
		NEW_MSG_TABLE_USER,
		NEW_MSG_MESSAGE_READ,
		(&(info->msg))->user
	);	
	sqlite3_exec(handle->db, sql, NULL, NULL, &errmsg);
	
	sprintf(sql, "UPDATE [%s] SET [flag]=[flag]|%d WHERE [user]='%s' AND [flag]&%d=0;",
		NEW_MSG_TABLE_MESSAGE,
		NEW_MSG_MESSAGE_READ,
		(&(info->msg))->user,
		NEW_MSG_MESSAGE_READ
	);
	sqlite3_exec(handle->db, sql, NULL, NULL, &errmsg);
	
	return 0;
}

int new_msg_show_border_left(char *content, char *s, int offset, int left, char *color) {
	int i;
	for (i=0;i<left;i++)
		content[offset++]=' ';
	for (i=0;i<strlen(color);i++)
		content[offset++]=color[i];
	for (i=0;i<strlen(s);i++)
		content[offset++]=s[i];
	content[offset++]='\033';
	content[offset++]='[';
	content[offset++]='m';
	return offset;
}
int new_msg_show_border_right(char *content, char *s, int offset, int right, char *color) {
	int i;
	for (i=0;i<right;i++)
		content[offset++]=' ';
	for (i=0;i<strlen(color);i++)
		content[offset++]=color[i];
	for (i=0;i<strlen(s);i++)
		content[offset++]=s[i];
	content[offset++]='\033';
	content[offset++]='[';
	content[offset++]='m';
	return offset;
}
int new_msg_show_border_horizontal(char *content, char *s_left, char *s_main, char *s_right, int offset, int left, int size, char *color) {
	int i, j, k;
	for (i=0;i<left;i++)
		content[offset++]=' ';
	for (i=0;i<strlen(color);i++)
		content[offset++]=color[i];
	for (i=0;i<strlen(s_left);i++)
		content[offset++]=s_left[i];
	k=strlen(s_main);
	for (i=0;i<size;i+=k) for (j=0;j<k;j++)
		content[offset++]=s_main[j];
	for (i=0;i<strlen(s_right);i++)
		content[offset++]=s_right[i];
	content[offset++]='\033';
	content[offset++]='[';
	content[offset++]='m';
	return offset;
}
/*
 * mode:
 * 0x01 显示摘要
 * 0x02 没有上边框
 * 0x04 没有下边框
 * 0x08 有附件
 * 0x010 不显示附件URL
 */
int new_msg_show_info_content(char *content, char *src, int left, int right, char *color, int mode, char *attach_name, char *attach_url) {
	int lines, total, pos, i, j, k, size, border_left, border_right;
	
	content[0]=0;
	total=strlen(src);
	pos=0;
	j=0;
	k=0;
	border_left=1;
	border_right=0;
	size=NEW_MSG_DISPLAY_COLUMNS-left-right-4;
	lines=0;
	if (!(mode&0x02)) {
		j=new_msg_show_border_horizontal(content, NEW_MSG_BORDER_TOP_LEFT, NEW_MSG_BORDER_HOR, NEW_MSG_BORDER_TOP_RIGHT, j, left, size, color);
		lines++;
		content[j++]='\n';
	}
	if (mode&0x08) {
		j=new_msg_show_border_left(content, NEW_MSG_BORDER_VER, j, left, color);
		for (i=0;i<strlen(color);i++)
			content[j++]=color[i];
		for (i=0;i<strlen(attach_name);i++)
			content[j++]=attach_name[i];
		j=new_msg_show_border_right(content, NEW_MSG_BORDER_VER, j, size-i, color);
		content[j++]='\n';
		j=new_msg_show_border_left(content, NEW_MSG_BORDER_VER, j, left, color);
		for (i=0;i<strlen(color);i++)
			content[j++]=color[i];
		for (i=0;i<strlen(attach_url);i++)
			content[j++]=attach_url[i];
		j=new_msg_show_border_right(content, NEW_MSG_BORDER_VER, j, size-i, color);
		content[j++]='\n';
		lines+=2;
	}
	for (i=0;i<total;i++) {
		if (j>NEW_MSG_DISPLAY_MAX_CONTENT-200)
			break;
		if (mode&0x01 && lines>=NEW_MSG_DISPLAY_LINES-1+((mode&0x02)?0:1)) {
			j=new_msg_show_border_left(content, NEW_MSG_BORDER_VER, j, left, color);
			content[j++]=' ';
			content[j++]=' ';
			content[j++]='\033';
			content[j++]='[';
			content[j++]='1';
			content[j++]=';';
			content[j++]='3';
			content[j++]='1';
			content[j++]='m';
			content[j++]='.';
			content[j++]='.';
			content[j++]='.';
			border_right=1;
			pos=5;
			break;
		}
		if (border_left) {
			j=new_msg_show_border_left(content, NEW_MSG_BORDER_VER, j, left, color);
			border_left=0;
			border_right=1;
			pos=0;
			k=0;
		}
		if (k==1)
			k=0;
		else if (src[i]<0)
			k=1;
		if (
			(k==0 && pos>=size)
			|| (k==1 && pos>=size-1)
			|| (src[i]=='\n' || src[i]=='\r')
		) {
			j=new_msg_show_border_right(content, NEW_MSG_BORDER_VER, j, size-pos, color);
			pos=0;
			content[j++]='\n';
			lines++;
			if (i<=total-1)
				border_left=1;
			border_right=0;
			if (src[i]=='\n' || src[i]=='\r') 
				continue;
		}
		if (border_left) {
			i--;
			continue;
		}
		content[j++]=src[i];
		pos++;	
	}
	if (border_right)
		j=new_msg_show_border_right(content, NEW_MSG_BORDER_VER, j, size-pos, color);
	if (!(mode&0x04)) {
		content[j++]='\n';
		j=new_msg_show_border_horizontal(content, NEW_MSG_BORDER_BOTTOM_LEFT, NEW_MSG_BORDER_HOR, NEW_MSG_BORDER_BOTTOM_RIGHT, j, left, size, color);
		lines++;
	}
	content[j]=0;
	return lines;
}
int new_msg_show_info(char *content, struct new_msg_info *msg, int mode, struct new_msg_attachment *attachment) {
	int lines, left, right;
	char color[16];
	char attach_name[NEW_MSG_ATTACHMENT_NAME_LEN+20], attach_url[STRLEN];
	char *ext, buf[NEW_MSG_ATTACHMENT_NAME_LEN+1], sid[STRLEN];
	
	if (msg->flag&NEW_MSG_MESSAGE_SEND) {
		left=10;
		right=4;
		strcpy(color, "\033[0;33m");
	} else {
		left=4;
		right=10;
		strcpy(color, "\033[0;32m");
	}
	if (msg->flag&NEW_MSG_MESSAGE_ATTACHMENT) {
		mode |= 0x08;
		get_telnet_sessionid(sid, getSession()->utmpent);
		new_msg_show_size(buf, attachment->size);
		sprintf(attach_name, "附件: %s (%s)", attachment->name, buf);
		buf[0]=0;
		strcpy(buf, attachment->name);
		ext=strrchr(buf, '.');
		if (mode & 0x010)
			sprintf(attach_url, "(请至本人短信箱内查看附件)");
		else
			sprintf(attach_url, "http://%s/b.php?sid=%s&id=%ld&f%s", NEW_MSG_ATTACHMENT_URL, sid, attachment->id, ext);
		lines=new_msg_show_info_content(content, msg->msg, left, right, color, mode, attach_name, attach_url);
	} else {
		lines=new_msg_show_info_content(content, msg->msg, left, right, color, mode, NULL, NULL);
	}
	return lines;
}

int new_msg_dump_file(char *path, struct new_msg_handle *handle, struct new_msg_user *info) {
	char buf[STRLEN];
	
	sprintf(buf, ".msg.%s", (&(info->msg))->user);
	sethomefile(path, handle->user, buf);
	
	return 0;
}

/*
 *file_mode: 0x01: 不显示附件URL
 */
int new_msg_dump(struct new_msg_handle *handle, struct new_msg_user *info, int file_mode, int start, int count) {
	char path[PATHLEN], buf[STRLEN];
	struct userec *user;
	struct stat st;
	FILE *fp;
	int total, size, i, j, k, m, n;	
	struct new_msg_message *messages;
	char content[NEW_MSG_DISPLAY_MAX_CONTENT];
	int show_time, is_send, last_send, mode;
	time_t last_time;

	if (!getuser((&(info->msg))->user, &user))
		return -1;
	
	new_msg_dump_file(path, handle, info);

	if (1 || stat(path, &st) < 0 || st.st_mtime < (&(info->msg))->time) {
		fp=fopen(path, "w");
		if (NULL==fp)
			return -2;
	
		fprintf(fp, "%s\n", info->name);
		total=new_msg_get_user_messages(handle, user->userid);
		if (total > 0) {
			size=10;
			i=start;
			m=0;
			messages=(struct new_msg_message *)malloc(sizeof(struct new_msg_message)*size);
			if (!messages) {
				fclose(fp);
				return 0;
			}
			last_time=0;
			last_send=-1;
			while(i<total) {
				bzero(messages, sizeof(struct new_msg_message)*size);
				j=new_msg_load_user_messages(handle, user->userid, i, size, messages);
				for (k=0;k<j;k++) {
					if (count>0 && i>=start+count) {
						i=total;
						break;
					}
					show_time=(abs(last_time-messages[k].msg.time)>600)?1:0;
					is_send=(messages[k].msg.flag&NEW_MSG_MESSAGE_SEND)?1:0;
					mode=0x04;

					if (last_send!=is_send || show_time) {
						if (m!=0) { 
							content[0]=0;
							n=new_msg_show_border_horizontal(content, NEW_MSG_BORDER_BOTTOM_LEFT, NEW_MSG_BORDER_HOR, NEW_MSG_BORDER_BOTTOM_RIGHT, 0, last_send?10:4, NEW_MSG_DISPLAY_COLUMNS-18, last_send?"\033[0;33m":"\033[0;32m");
							content[n]=0;
							fprintf(fp, "%s\n", content);
						}
						new_msg_show_time(buf, &(messages[k].msg), 0);
						fprintf(fp, 
							"  %s%-12s\033[m      \033[1;36m%s 来自%s\033[m\n", 
							is_send?"\033[1;33m":"\033[1;32m", 
							is_send?handle->user:user->userid,
							buf, 
							messages[k].msg.from
						);
					} else {
						mode |= 0x02;
						content[0]=0;
						n=new_msg_show_border_left(content, NEW_MSG_BORDER_VER, 0, is_send?10:4, is_send?"\033[0;33m":"\033[0;32m");
						content[n++]='\033';
						content[n++]='[';
						content[n++]='1';
						content[n++]=';';
						content[n++]='3';
						content[n++]='0';
						content[n++]='m';
						content[n++]=' ';
						content[n++]='-';
						content[n++]='-';
						content[n++]='-';
						content[n++]='-';
						n=new_msg_show_border_right(content, NEW_MSG_BORDER_VER, n, NEW_MSG_DISPLAY_COLUMNS-23, is_send?"\033[0;33m":"\033[0;32m");
						content[n]=0;
						fprintf(fp, "%s\n", content);
					}
					if (m==0 || last_send!=is_send || show_time) {
						content[0]=0;
						n=new_msg_show_border_horizontal(content, NEW_MSG_BORDER_TOP_LEFT, NEW_MSG_BORDER_HOR, NEW_MSG_BORDER_TOP_RIGHT, 0, is_send?10:4, NEW_MSG_DISPLAY_COLUMNS-18, is_send?"\033[0;33m":"\033[0;32m");
						content[n]=0;
						fprintf(fp, "%s\n", content);
					}
					mode=6;
					if (file_mode&0x01)
						mode|=0x010;
					content[0]=0;
					new_msg_show_info(content, (&(messages[k].msg)), mode, &(messages[k].attachment));
					fprintf(fp, "%s\n", content);
					last_send=is_send;
					last_time=messages[k].msg.time;
					m++;
				}
				i+=size;
			}
			if (m!=0) { 
				content[0]=0;
				n=new_msg_show_border_horizontal(content, NEW_MSG_BORDER_BOTTOM_LEFT, NEW_MSG_BORDER_HOR, NEW_MSG_BORDER_BOTTOM_RIGHT, 0, last_send?10:4, NEW_MSG_DISPLAY_COLUMNS-18, last_send?"\033[0;33m":"\033[0;32m");
				content[n]=0;
				fprintf(fp, "%s\n", content);
			}
			free(messages);
		}

		
		fclose(fp);
	}

	return 0;
}

int new_msg_get_new_count(struct new_msg_handle *handle) {
	char buf[100];

	sprintf(buf, " [flag]&%d=0 ", NEW_MSG_MESSAGE_READ);
	return new_msg_count(handle, NEW_MSG_TABLE_MESSAGE, buf);
}

int new_msg_check_new(struct new_msg_handle *handle) {
/*
	// 仅检查最新的一条记录
	struct new_msg_user user;
	
	if (new_msg_get_users(handle)<=0)
		return -1;
		
	if (new_msg_load_users(handle, 0, 1, &user)<=0)
		return -2;
	return (user.msg.flag&NEW_MSG_MESSAGE_READ)?0:1;
*/
	// 检查所有未读记录
	return (new_msg_get_new_count(handle)>0)?1:0;
}

char *new_msg_show_time(char *buf, struct new_msg_info *msg, int mode) {
	time_t now=time(NULL);
	long offset=now-msg->time;
	if (mode&&offset < 60)
		sprintf(buf, "%ld秒前", offset);
	else if (mode&&offset < 3600)
		sprintf(buf, "%ld分钟前", offset/60);
	else if (mode&&offset < 86400)
		sprintf(buf, "%ld小时前", offset/3600);
	else {
		struct tm t;
		if (msg->time <= 0 || !localtime_r(&msg->time, &t))
			sprintf(buf, "未知的时间");
		else
			sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d", t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
	}

	return buf;
}

int new_msg_get_attachment(struct new_msg_handle *handle, long id, char *output, int size) {
	char sql[300];
	sqlite3_stmt  *stmt=NULL;
	int ret;

	if (id<=0)
		return -1;

	ret=0;
	sprintf(sql, "SELECT [attachment_body] FROM %s WHERE [id]=%ld AND [flag]&%d!=0 LIMIT 1;", 
		NEW_MSG_TABLE_MESSAGE,
		id,
		NEW_MSG_MESSAGE_ATTACHMENT
	);
	if (SQLITE_OK != sqlite3_prepare(handle->db, sql, strlen(sql), &stmt, NULL)) {
		if (stmt)
			sqlite3_finalize(stmt);
		return -2;
	}	
	if (SQLITE_ROW == sqlite3_step(stmt)) {
		memcpy(output, sqlite3_column_blob(stmt, 0), size);
	} else {
		ret=-3;
	}

	sqlite3_finalize(stmt);
	return ret;
}

#undef NEW_MSG_SQL_SELECT_MESSAGE_FULL
#undef NEW_MSG_SQL_SELECT_USER_FULL
#undef NEW_MSG_SQL_SELECT_MEMBER_FULL

#undef NEW_MSG_BORDER_TOP_LEFT
#undef NEW_MSG_BORDER_TOP_RIGHT
#undef NEW_MSG_BORDER_BOTTOM_LEFT
#undef NEW_MSG_BORDER_BOTTOM_RIGHT
#undef NEW_MSG_BORDER_HOR
#undef NEW_MSG_BORDER_VER

#endif /* NEW_MSG_LIB */
#endif /* ENABLE_NEW_MSG */
