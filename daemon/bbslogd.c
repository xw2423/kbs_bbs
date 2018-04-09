#include "bbs.h"
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>

#ifdef NEWSMTH
#define BBSLOG_APNS
#define BBSLOG_UPDATE_AR
#define BBSLOG_UPDATE_STIGERTEST
#endif

/* stiger: 20140217: format to update AR:
   TCP socket dport: 5111
   256 bytes for one update: struct fileheader, except:
   byte 222~251: 30bytes boardname, (it's unused[] in fileheader_t).
*/

#ifdef BBSLOG_APNS
static int udpserver_sockfd = -1;
static struct sockaddr_in dest_addr;

static void udpsock_exit()
{
    if(udpserver_sockfd >= 0){
        close(udpserver_sockfd);
        udpserver_sockfd = -1;
    }
}

static int udpsock_init()
{
    if(udpserver_sockfd >= 0){
        return 0;
    }

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    dest_addr.sin_port = htons(2195);

    udpserver_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(udpserver_sockfd < 0){
        return -1;
    }

    return 0;
}

static int udpsock_send(const void *data, int len)
{
    if(udpserver_sockfd < 0){
        return -1;
    }

    int val = sendto(udpserver_sockfd, data, len, 0,(struct sockaddr *)&dest_addr, sizeof(struct sockaddr));

    return val;
}

static int apns_udp_send(char * userid, unsigned char type)
{
    unsigned char buf[32];
    memset(buf, 0, 32);

        //UDP packet format:
        //0~3: magic: 0x45891267
        //4~19: userid
        //20: type: 1:refer, 2:at, 3:mail
        //30~31: crc16, not used because it's loopback now.
    buf[0] = 0x45;
    buf[1] = 0x89;
    buf[2] = 0x12;
    buf[3] = 0x67;
    strncpy(&(buf[4]), userid, 15);
    buf[20] = type;

    int ret = udpsock_send(buf, 32);

    return ret;
}
#endif

struct bbs_msgbuf *rcvlog(int msqid)
{
    static char buf[1024];
    struct bbs_msgbuf *msgp = (struct bbs_msgbuf *) buf;
    int retv;

    retv = msgrcv(msqid, msgp, sizeof(buf) - sizeof(msgp->mtype) - 2, 0, MSG_NOERROR);
    if (retv < 0) {
	if (errno==EINTR)
            return NULL;
	else {
	    bbslog("3error","bbslogd(rcvlog):%s",strerror(errno));
	    exit(0);
	}
    }
    retv-=((char*)msgp->mtext-(char*)&msgp->msgtime);
#ifdef NEWPOSTLOG
	if (msgp->mtype == BBSLOG_POST){
		if(retv <= sizeof(struct _new_postlog)){
			return NULL;
		}
		return msgp;
	}
#endif
#ifdef NEWBMLOG
	if (msgp->mtype == BBSLOG_BM){
		if(retv <= sizeof(struct _new_bmlog)){
			return NULL;
		}
		return msgp;
	}
#endif
    while (retv > 0 && msgp->mtext[retv - 1] == 0)
        retv--;
    if (retv==0) return NULL;
    if (msgp->mtext[retv - 1] != '\n') {
        msgp->mtext[retv] = '\n';
        retv++;
    }
    msgp->mtext[retv]=0;
    return msgp;
}

struct taglogconfig {
    char *filename;
    int bufsize;                /* 缓存大小，如果是 0，不缓存 */

    /*
     * 运行时参数 
     */
    int bufptr;                 /* 使用缓存位置 */
    char *buf;                  /* 缓存 */
    int fd;                     /* 文件句柄 */
};
static struct taglogconfig logconfig[] = {
    {"usies", 100 * 1024, 0, NULL, -1},
    {"user.log", 100 * 1024, 0, NULL, 0},
    {"boardusage.log", 100 * 1024, 0, NULL, 0},
    {"sms.log", 10 * 1024, 0, NULL, 0},
    {"debug.log", 10 * 1024, 0, NULL, 0}
};

#if defined(NEWPOSTLOG) || defined(NEWBMLOG)
static MYSQL s;
static int postlog_start=0;
static time_t mysqlclosetime=0;
static int mysql_fail=0;

static void opennewpostlog()
{
	mysql_init (&s);

	if (! my_connect_mysql(&s) ){
		bbslog("3system","mysql connect error:%s",mysql_error(&s));
		return;
	}
	postlog_start = 1;
	mysqlclosetime=0;
	return;
}

static void closenewpostlog()
{
	bbslog("3system","mysql log closed");
	mysql_close(&s);
	postlog_start=0;
	mysqlclosetime = time(0);
}
#endif

#ifdef BBSLOG_UPDATE_AR
static int updatear_fd = -1;
static time_t updatear_retrytime=0;

static void open_updatear()
{
	updatear_retrytime=time(0);

	struct sockaddr_in araddr;
	updatear_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(updatear_fd < 0){
		return;
	}
	memset(&araddr, 0, sizeof(struct sockaddr));
	araddr.sin_family = AF_INET;
	araddr.sin_port = htons(5111);
	araddr.sin_addr.s_addr = inet_addr("10.0.4.236");
	if(connect(updatear_fd, (struct sockaddr *)&araddr, sizeof(struct sockaddr)) < 0){
		bbslog("3system","update_ar connect fail");
		close(updatear_fd);
		updatear_fd = -1;
		return;
	}
}

static void close_updatear()
{
    close(updatear_fd);
    updatear_fd = -1;
	updatear_retrytime=time(0);
}
#endif

#ifdef BBSLOG_UPDATE_STIGERTEST
static int stserver_sockfd = -1;
static struct sockaddr_in stdest_addr;

static void stsock_exit()
{
    if(stserver_sockfd >= 0){
        close(stserver_sockfd);
        stserver_sockfd = -1;
    }
}

static int stsock_init()
{
    if(stserver_sockfd >= 0){
        return 0;
    }

    stdest_addr.sin_family = AF_INET;
    stdest_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    stdest_addr.sin_port = htons(5222);

    stserver_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(stserver_sockfd < 0){
        return -1;
    }

    return 0;
}

static int stsock_send(const void *data, int len)
{
    if(stserver_sockfd < 0){
        return -1;
    }

    int val = sendto(stserver_sockfd, data, len, 0,(struct sockaddr *)&stdest_addr, sizeof(struct sockaddr));

    return val;
}

#endif

static void openbbslog(int first)
{
    int i;
    for (i = 0; i < sizeof(logconfig) / sizeof(struct taglogconfig); i++) {
		if (!first && !strcmp(logconfig[i].filename,"boardusage.log") && logconfig[i].fd )
			continue;
        if (logconfig[i].filename) {
            /*logconfig[i].fd = open(logconfig[i].filename, O_WRONLY);
            if (logconfig[i].fd < 0)
                logconfig[i].fd = creat(logconfig[i].filename, 0644);*/
            logconfig[i].fd = open(logconfig[i].filename, O_RDWR | O_CREAT, 0644);
            if (logconfig[i].fd < 0)
                bbslog("3error","can't open log file:%s.%s",logconfig[i].filename,strerror(errno));
        }
        if (logconfig[i].buf==NULL)
            logconfig[i].buf = malloc(logconfig[i].bufsize);
    }

#if defined(NEWPOSTLOG) || defined(NEWBMLOG)
	if(first || !postlog_start)
		opennewpostlog();
#endif

#ifdef BBSLOG_UPDATE_AR
    if(first || updatear_fd < 0){
        open_updatear();
    }
#endif
}
static void writelog(struct bbs_msgbuf *msg)
{
    char header[256];
    struct tm *n;
    struct taglogconfig *pconf;
    char ch;

#if defined(NEWPOSTLOG) || defined(NEWBMLOG)
	if(!postlog_start && mysqlclosetime && time(0)-mysqlclosetime>600)
		opennewpostlog();
#endif

#ifdef BBSLOG_UPDATE_AR
    if(updatear_fd < 0 && updatear_retrytime && (time(0) - updatear_retrytime > 60)){
        open_updatear();
    }
#endif

#ifdef NEWBMLOG
	if (msg->mtype == BBSLOG_BM){
		char sqlbuf[512];
		struct _new_bmlog * ppl = (struct _new_bmlog *)( &msg->mtext[1]) ;
		int affect;

		if(!postlog_start)
			return;

		if(ppl->value == 0)
			return;

		msg->mtext[0]=0;

		sprintf(sqlbuf, "UPDATE bmlog SET `log%d`=`log%d`+%d WHERE userid='%s' AND bname='%s' AND month=MONTH(CURDATE()) AND year=YEAR(CURDATE()) ;", ppl->type, ppl->type, ppl->value, msg->userid, ppl->boardname );

		if( mysql_real_query(&s,sqlbuf,strlen(sqlbuf)) || (affect=(int)mysql_affected_rows(&s))<0 ){
			mysql_fail ++;
			bbslog("3system","mysql bmlog error:%s",mysql_error(&s));
			if(mysql_fail > 10)
				closenewpostlog();
			return;
		}

		if(affect <= 0){
			sprintf(sqlbuf, "INSERT INTO bmlog (`id`, `userid`, `bname`, `month`, `year`, `log%d` ) VALUES (NULL, '%s', '%s', MONTH(CURDATE()), YEAR(CURDATE()), '%d' );", ppl->type, msg->userid, ppl->boardname, ppl->value);

			if( mysql_real_query( &s, sqlbuf, strlen(sqlbuf) )){
				mysql_fail ++;
				bbslog("3system","mysql bmlog error:%s",mysql_error(&s));
				if(mysql_fail > 10)
					closenewpostlog();
			}else
				mysql_fail = 0;
		}else{
			mysql_fail = 0;
		}

		return;
	}
#endif

#ifdef BBSLOG_UPDATE_AR
	if ((msg->mtype == BBSLOG_POST || msg->mtype == BBSLOG_UNLINK) && updatear_fd >= 0){
		struct _new_postlog * ppl = (struct _new_postlog *) ( &msg->mtext[1]) ;

        struct fileheader fh;
        char path[STRLEN];
        int fd;

        bzero(&fh, sizeof(struct fileheader));

        if(msg->mtype == BBSLOG_UNLINK){
            fh.filename[0] = '\0';
            fh.id = ppl->articleid;
            fh.groupid = ppl->threadid;
        }else{
            setbdir(DIR_MODE_NORMAL, path, ppl->boardname);
            if ((fd = open(path, O_RDWR, 0644)) >= 0) {
                get_records_from_id(fd, ppl->articleid, &fh, 1, NULL);
                close(fd);
            }
        }

        if(fh.id){
            strncpy(((unsigned char *)(&fh)) + 222, ppl->boardname, 30);

            ssize_t n = write(updatear_fd, &fh, 256);
            if(n != 256){
                close_updatear();
            }
        }
    }
#endif

#ifdef BBSLOG_UPDATE_STIGERTEST
	if(msg->mtype == BBSLOG_POST || msg->mtype == BBSLOG_UNLINK){
		struct _new_postlog * ppl = (struct _new_postlog *) ( &msg->mtext[1]) ;
        stsock_send(ppl, sizeof(struct _new_postlog));
	}
#endif

#ifdef NEWPOSTLOG
	if (msg->mtype == BBSLOG_POST && postlog_start){

		char newtitle[161];
		char sqlbuf[512];
		struct _new_postlog * ppl = (struct _new_postlog *) ( &msg->mtext[1]) ;
		char newts[20];

		msg->mtext[0]=0;

		mysql_escape_string(newtitle, ppl->title, strlen(ppl->title));

#ifdef NEWSMTH
		sprintf(sqlbuf, "INSERT INTO postlog (`id`, `userid`, `bname`, `title`, `time`, `threadid`, `articleid`, `ip`) VALUES (NULL, '%s', '%s', '%s', '%s', '%d', '%d', '%s');", msg->userid, ppl->boardname, newtitle, tt2timestamp(msg->msgtime, newts), ppl->threadid, ppl->articleid, ppl->ip );
#else
		sprintf(sqlbuf, "INSERT INTO postlog (`id`, `userid`, `bname`, `title`, `time`, `threadid`, `articleid`) VALUES (NULL, '%s', '%s', '%s', '%s', '%d', '%d');", msg->userid, ppl->boardname, newtitle, tt2timestamp(msg->msgtime, newts), ppl->threadid, ppl->articleid );
#endif

		if( mysql_real_query( &s, sqlbuf, strlen(sqlbuf) )){
			mysql_fail ++;
			bbslog("3system","mysql postlog error:%s",mysql_error(&s));
			if(mysql_fail > 10)
				closenewpostlog();
		}else{
			mysql_fail = 0;

			return;
		}
	}

	if (msg->mtype == BBSLOG_POST){
		struct _new_postlog * ppl = (struct _new_postlog *) ( &msg->mtext[1]) ;

		msg->mtype = BBSLOG_USER;

    	if ((msg->mtype < 0) || (msg->mtype > sizeof(logconfig) / sizeof(struct taglogconfig)))
        	return;
    	pconf = &logconfig[msg->mtype-1];

    	if (pconf->fd<0) return;
    	n = localtime(&msg->msgtime);

    	snprintf(header, 256, "[%02u/%02u %02u:%02u:%02u %5lu %lu] %s post '%s' on '%s'\n", n->tm_mon + 1, n->tm_mday, n->tm_hour, n->tm_min, n->tm_sec, (long int) msg->pid, msg->mtype, msg->userid, ppl->title, ppl->boardname);
    	if (pconf->buf) {
        	if ((int) (pconf->bufptr + strlen(header)) <= pconf->bufsize) {
            	strcpy(&pconf->buf[pconf->bufptr], header);
            	pconf->bufptr += strlen(header);
            	return;
        	}
    	}

/*目前log还是分散的，就先lock,seek吧*/
        writew_lock(pconf->fd, 0, SEEK_SET, 0);
    	lseek(pconf->fd, 0, SEEK_END);

    	if (pconf->buf && pconf->bufptr) {
        	write(pconf->fd, pconf->buf, pconf->bufptr);
        	pconf->bufptr = 0;
    	}
        un_lock(pconf->fd, 0, SEEK_SET, 0);

		return;
	}

#endif

#ifdef BBSLOG_APNS
    //stiger: 20140128, send to daemon_apns
    //reply: newbbslog(BBSLOG_USER, "sent reply refer '%s' to '%s'", fh->title, user->userid);
    //refer: newbbslog(BBSLOG_USER, "sent refer '%s' to '%s'", fh->title, user->userid);
    //mail www: newbbslog(BBSLOG_USER, "mailed(www) %s %s", targetID, mail_title);
    //mail backup: newbbslog(BBSLOG_USER, "mailed(www) %s ", targetID);
    //mail : newbbslog(BBSLOG_USER, "mailed %s %s", targetID, mail_title);
    //mail backup: newbbslog(BBSLOG_USER, "mailed %s ", targetID);
    if(msg->mtype == BBSLOG_USER){
        unsigned char action = 0;
        char dest_userid[16];
        dest_userid[0] = '\0';
		if(!strncmp(msg->mtext, "sent reply ", strlen("sent reply "))){
            action = 1;
        }
		if(!strncmp(msg->mtext, "sent refer ", strlen("sent refer "))){
            action = 2;
        }
		if(!strncmp(msg->mtext, "mailed(www) ", strlen("mailed(www) "))){
            action = 3;
        }
		if(!strncmp(msg->mtext, "mailed ", strlen("mailed "))){
            action = 3;
        }
        if(action == 1 || action == 2){
            char * c_end = msg->mtext + strlen(msg->mtext) - 1;
            int q_cnt = 0;
            while(q_cnt < 2 && c_end > msg->mtext){
                if(c_end[0] == '\''){
                    q_cnt ++;
                }
                c_end --;
            }
            if(q_cnt == 2){
                c_end ++;
                c_end ++;
                strncpy(dest_userid, c_end, 16);
                c_end = strchr(dest_userid, '\'');
                if(c_end){
                    c_end[0] = '\0';
                }
            }
        }
        if(action == 3){
            char * c_end = strchr(msg->mtext, ' ');
            if(c_end){
                char * sec_end = strchr(c_end + 1, ' ');
                if(sec_end && sec_end[1] != '\0' && sec_end[1] != '\n' && sec_end[1] != '\r'){
                    strncpy(dest_userid, c_end + 1, 16);
                    c_end = strchr(dest_userid, ' ');
                    if(c_end){
                        c_end[0] = '\0';
                    }
                }
            }
        }
        if(action && dest_userid[0]){
            int ret = apns_udp_send(dest_userid, action);
            //TODO: no error handler.
        }
    }
#endif

    if ((msg->mtype < 0) || (msg->mtype > sizeof(logconfig) / sizeof(struct taglogconfig)))
        return;
    pconf = &logconfig[msg->mtype-1];

    if (pconf->fd<0) return;
    n = localtime(&msg->msgtime);

    ch=msg->mtext[0];
    msg->mtext[0]=0;
    snprintf(header, 256, "[%d-%02u-%02u %02u:%02u:%02u %5lu %lu] %s %c%s", n->tm_year + 1900, n->tm_mon + 1, n->tm_mday, n->tm_hour, n->tm_min, n->tm_sec, (long int) msg->pid, msg->mtype, msg->userid,ch,&msg->mtext[1]);
    if (pconf->buf) {
        if ((int) (pconf->bufptr + strlen(header)) <= pconf->bufsize) {
            strcpy(&pconf->buf[pconf->bufptr], header);
            pconf->bufptr += strlen(header);
            return;
        }
    }

/*目前log还是分散的，就先lock,seek吧*/
    writew_lock(pconf->fd, 0, SEEK_SET, 0);
    lseek(pconf->fd, 0, SEEK_END);

    if (pconf->buf && pconf->bufptr) {
        write(pconf->fd, pconf->buf, pconf->bufptr);
        pconf->bufptr = 0;
    }
    un_lock(pconf->fd, 0, SEEK_SET, 0);
}

static void flushlog(int signo)
{
    int i;
    for (i = 0; i < sizeof(logconfig) / sizeof(struct taglogconfig); i++) {
        struct taglogconfig *pconf;

        pconf = &logconfig[i];
        if (pconf->fd>=0 && pconf->buf && pconf->bufptr) {
            writew_lock(pconf->fd, 0, SEEK_SET, 0);
            lseek(pconf->fd, 0, SEEK_END);
            write(pconf->fd, pconf->buf, pconf->bufptr);
            pconf->bufptr = 0;
            un_lock(pconf->fd, 0, SEEK_SET, 0);
        }
        if (signo!=-1)
            close(pconf->fd);
    }
    if (signo==-1) return;
#if defined(NEWPOSTLOG) || defined(NEWBMLOG)
	closenewpostlog();
#endif
#ifdef BBSLOG_UPDATE_AR
    close_updatear();
#endif
#ifdef BBSLOG_APNS
    udpsock_exit();
#endif
#ifdef BBSLOG_UPDATE_STIGERTEST
	stsock_exit();
#endif
    exit(0);
}

static void flushBBSlog_exit()
{
    flushlog(-1);
}
bool gb_trunclog;
bool truncboard;
static void trunclog(int signo)
{
    int i;
    flushlog(-1);

    /* see libBBS/log.c:logconf[] */
    static const char *dirty_rotate[] = {"error.log", "connect.log", "msg.log",
       "trace.chatd", "trace"};
    for (i = 0; i < sizeof(dirty_rotate) / sizeof(dirty_rotate[0]); i++) {
        char buf[MAXPATH];
        int j;

        if (!dashf(dirty_rotate[i]))
            continue;

        j=0;
        while (1) {
            sprintf(buf,"%s.%d", dirty_rotate[i],j);
            if (!dashf(buf))
               break;
            j++;
        }
        f_mv(dirty_rotate[i],buf);
    }
    
    for (i = 0; i < sizeof(logconfig) / sizeof(struct taglogconfig); i++) {
        struct taglogconfig *pconf;

		if (! strcmp(logconfig[i].filename,"boardusage.log"))
			continue;
        pconf = &logconfig[i];
        if (pconf->fd>=0) {
        	char buf[MAXPATH];
        	int j;
        	close(pconf->fd);
		j=0;
        	while (1) {
        	    sprintf(buf,"%s.%d",pconf->filename,j);
        	    if (!dashf(buf))
        	    	break;
		    j++;
        	}
        	f_mv(pconf->filename,buf);
        }
    }
    openbbslog(0);
    gb_trunclog=true;
}

static void truncboardlog(int signo)
{
    int i;
    flushlog(-1);
    
    for (i = 0; i < sizeof(logconfig) / sizeof(struct taglogconfig); i++) {
		if (strcmp(logconfig[i].filename,"boardusage.log"))
			continue;
        if (logconfig[i].fd>=0) {
        	close(logconfig[i].fd);
        	f_mv(logconfig[i].filename,"boardusage.log.0");
        }
        if (logconfig[i].filename) {
            /*logconfig[i].fd = open(logconfig[i].filename, O_WRONLY);
            if (logconfig[i].fd < 0)
                logconfig[i].fd = creat(logconfig[i].filename, 0644);*/
            logconfig[i].fd = open(logconfig[i].filename, O_RDWR | O_CREAT, 0644);
            if (logconfig[i].fd < 0)
                bbslog("3error","can't open log file:%s.%s",logconfig[i].filename,strerror(errno));
        }
        if (logconfig[i].buf==NULL)
            logconfig[i].buf = malloc(logconfig[i].bufsize);
    }
    truncboard=true;
}

static void flushBBSlog_time(int signo)
{
    flushlog(-1);
    alarm(60*10); /*十分钟flush一次*/
}

static void do_truncboardlog()
{
    truncboard=false;
}

static void do_trunclog()
{
    gb_trunclog=false;
}

int main()
{
    int msqid, i;
    struct bbs_msgbuf *msg;

    struct sigaction act;

    umask(027);

    chdir(BBSHOME);
    setuid(BBSUID);
    setreuid(BBSUID, BBSUID);
    setgid(BBSGID);
    setregid(BBSGID, BBSGID);
    if (dodaemon("bbslogd", true, true)) {
        bbslog("3error", "bbslogd had already been started!");
        return 0;
    }
    atexit(flushBBSlog_exit);
    bzero(&act, sizeof(act));
    act.sa_handler = flushlog;
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGABRT, &act, NULL);
    sigaction(SIGHUP, &act, NULL);
    act.sa_handler = flushBBSlog_time;
    sigaction(SIGALRM, &act, NULL);
    act.sa_handler = trunclog;
    sigaction(SIGUSR1, &act, NULL);
    act.sa_handler = truncboardlog;
    sigaction(SIGUSR2, &act, NULL);
    alarm(60*10); /*十分钟flush一次*/

    msqid = init_bbslog();
    if (msqid < 0)
        return -1;

#ifdef BBSLOG_APNS
	udpsock_init();
#endif
#ifdef BBSLOG_UPDATE_STIGERTEST
	stsock_init();
#endif

    gb_trunclog=false;
    truncboard=false;
    openbbslog(1);
    while (1) {
        if ((msg = rcvlog(msqid)) != NULL)
            writelog(msg);
        if (gb_trunclog)
        	do_trunclog();
		else if(truncboard)
			do_truncboardlog();
    }
    flushlog(-1);
    for (i = 0; i < sizeof(logconfig) / sizeof(struct taglogconfig); i++) {
        free(logconfig[i].buf);
    }
}
