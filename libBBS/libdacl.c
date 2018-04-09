#include "bbs.h"

#ifndef LIB_DACL
#ifdef ENABLE_DYNAMIC_ACL
#include <mysql.h>

#define DYNAMIC_ACL_TABLE_WHITE_LIST "dynamic_acl_white_list"
#define DYNAMIC_ACL_TABLE_BLACK_LIST "dynamic_acl_black_list"
#define DYNAMIC_ACL_TABLE_LIST       "dynamic_acl_list"
#define DYNAMIC_ACL_TABLE_RECORD     "dynamic_acl_record"

#define DYNAMIC_ACL_MAX_TIME         86400

static int dynamic_acl_insert_record(MYSQL *s, char *id, unsigned long ip)
{
	char sql[300];
	char my_id[STRLEN];
	long now;
	
	if (strlen(id)>IDLEN)
		id[IDLEN]=0;
		
	now=time(NULL);
	my_id[0]=0;
	mysql_escape_string(my_id, id, strlen(id));
	sprintf(sql, "INSERT INTO `%s` (`id`, `ip`, `user`, `time`, `flag`) VALUES (NULL, %lu, '%s', FROM_UNIXTIME(%ld), 0);", DYNAMIC_ACL_TABLE_RECORD, ip, my_id, now);
	
	if (mysql_real_query(s, sql, strlen(sql))) {
		bbslog("3system", "mysql error: %s", mysql_error(s));
		return -1;
	}
	
	return 0;
}

static int dynamic_acl_error_times(MYSQL *s, unsigned long ip, int time_long)
{	
	char sql[300];
	MYSQL_RES *res;
    MYSQL_ROW row;
	int i;
	
	sprintf(sql, "SELECT COUNT(*) FROM `%s` WHERE `ip`=%lu AND %ld-UNIX_TIMESTAMP(`time`)<=%d", DYNAMIC_ACL_TABLE_RECORD, ip, time(NULL), time_long);
	if (mysql_real_query(s, sql, strlen(sql))) {
		bbslog("3system", "mysql error: %s", mysql_error(s));
		return 0;
	}
	
	res = mysql_store_result(s);
	row = mysql_fetch_row(res);
	if (row != NULL)
		i=atoi(row[0]);
	else
		i=0;
		
	mysql_free_result(res);	
	
	return i;
}

static int dynamic_acl_add_block(MYSQL *s, unsigned long ip, long block_time)
{
	/* 这里本来要考虑查询 DYNAMIC_ACL_TABLE_LIST 内有同C段IP被封禁时，对整个C段封禁，现在未实现，仅针对单个IP封禁 */
	char sql[300];
	long now, expire_time;
	
	now=time(NULL);
	expire_time=now+block_time;
	
	sprintf(sql, "INSERT INTO `%s` (`start`, `end`, `update_time`, `create_time`, `expire_time`, `flag`) VALUES (%lu, %lu, FROM_UNIXTIME(%ld), FROM_UNIXTIME(%ld), FROM_UNIXTIME(%ld), %d);", 
		DYNAMIC_ACL_TABLE_LIST, ip, ip, now, now, expire_time, 0);
		
	if (mysql_real_query(s, sql, strlen(sql))) {
		bbslog("3system", "mysql error: %s", mysql_error(s));
		return -1;
	}
	
	return 0;
}

static int dynamic_acl_update(MYSQL *s, unsigned long ip)
{
	int cnt;
	
	/* 获取过去3600s内的错误记录数 */
	cnt=dynamic_acl_error_times(s, ip, 3600);
	/* 如果错误数不超过10，则放行 */
	if (cnt<10)
		return 0;
	
	/* 如果一天内累计错误50次，封禁一天；若错误次数超过20 或 过去7200s内错误次数超过15，封禁1个小时；其他封禁10分钟 */
	if (dynamic_acl_error_times(s, ip, 86400)>=50)
		dynamic_acl_add_block(s, ip, 86400);
	else if (cnt>=20 || dynamic_acl_error_times(s, ip, 7200)>=15)
		dynamic_acl_add_block(s, ip, 3600);
	else
		dynamic_acl_add_block(s, ip, 600);
	
	return 0;
}

static int dynamic_acl_check_in_list(MYSQL *s, char *table, unsigned long ip)
{
	MYSQL_RES *res;
    MYSQL_ROW row;
	char sql[300];
	int i;
	
	sprintf(sql, "SELECT COUNT(*) FROM `%s` WHERE `start`<=%lu AND `end`>=%lu", table, ip, ip);
	if (mysql_real_query(s, sql, strlen(sql))) {
		bbslog("3system", "mysql error: %s", mysql_error(s));
		return 0;
	}
	res = mysql_store_result(s);
    row = mysql_fetch_row(res);
	i=0;
    if (row != NULL) {
        i=atoi(row[0]);
    }
    mysql_free_result(res);
	
	return i;
}

static long dynamic_acl_block_expire_time(MYSQL *s, unsigned long ip)
{
	MYSQL_RES *res;
    MYSQL_ROW row;
	char sql[300];
	long i, now;
	
	sprintf(sql, "SELECT MAX(UNIX_TIMESTAMP(`expire_time`)) FROM `%s` WHERE `start`<=%lu AND `end`>=%lu", DYNAMIC_ACL_TABLE_LIST, ip, ip);

	if (mysql_real_query(s, sql, strlen(sql))) {
		bbslog("3system", "mysql error: %s", mysql_error(s));
		return 0;
	}

	res = mysql_store_result(s);
	row = mysql_fetch_row(res);
	i=0;
	if (row != NULL) {
        i=atol(row[0]);
    }
    mysql_free_result(res);
	
	now=time(NULL);
	if (i<=now)
		return 0;
		
	return i-now;
}
/**
  * 检查IP是否被封禁
  * @param   unsigned long ip: IP地址
  * @return   0: 正常IP未被封禁
  * @return  >0: 已被封禁, 返回的值为距离解除封禁的时长(s)
  * @return  <0: dynamic ACL的黑名单
  */
long dynamic_acl_check_ip(unsigned long ip)
{
	MYSQL s;
	long ret=0;
	
	mysql_init(&s);
	if (!my_connect_mysql(&s)) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        return ret;
    }
	
	if (dynamic_acl_check_in_list(&s, DYNAMIC_ACL_TABLE_BLACK_LIST, ip)>0)
		ret=-1;
	else if (dynamic_acl_check_in_list(&s, DYNAMIC_ACL_TABLE_WHITE_LIST, ip)>0)
		ret=0;
	else if (dynamic_acl_check_in_list(&s, DYNAMIC_ACL_TABLE_LIST, ip)>0) 
		ret=dynamic_acl_block_expire_time(&s, ip);
	else
		ret=0;
	
	mysql_close(&s);
	return ret;
}

/**
  * 密码验证错误后调用此函数新增错误记录
  * @param char *id: ID
  * @param unsigned long ip: IP
  * @return  0: success
  * @return <0: failure
  */
int dynamic_acl_add_record(char *id, unsigned long ip)
{
	MYSQL s;
	
	mysql_init(&s);
	if (!my_connect_mysql(&s)) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        return -1;
    }
	
	if (dynamic_acl_insert_record(&s, id, ip)>=0) 
		dynamic_acl_update(&s, ip);
	
	mysql_close(&s);
	return 0;
}

int dynamic_acl_clear()
{
	MYSQL s;
	char sql[300];
	long now;
	
	mysql_init(&s);
	if (!my_connect_mysql(&s)) {
        bbslog("3system", "mysql error: %s", mysql_error(&s));
        return -1;
    }
	
	now=time(NULL);
	sprintf(sql, "DELETE FROM `%s` WHERE %ld-UNIX_TIMESTAMP(`time`)>=%d", DYNAMIC_ACL_TABLE_RECORD, now, DYNAMIC_ACL_MAX_TIME);
	if (mysql_real_query(&s, sql, strlen(sql))) 
		bbslog("3system", "mysql error: %s", mysql_error(&s));
	
	sprintf(sql, "DELETE FROM `%s` WHERE UNIX_TIMESTAMP(`expire_time`)<=%ld", DYNAMIC_ACL_TABLE_LIST, now);
	if (mysql_real_query(&s, sql, strlen(sql))) 
		bbslog("3system", "mysql error: %s", mysql_error(&s));
	
	mysql_close(&s);
	return 0;
}

unsigned long dynamic_acl_ip2long(const char *ip)
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
#endif
#endif
