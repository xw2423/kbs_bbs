#include "bbs.h"
static int utmp_hash(const char *userid)
{
    int hash;

    hash = ucache_hash(userid);
    if (hash == 0)
        return 0;
    hash = (hash / 3) % UTMP_HASHSIZE;
    if (hash == 0)
        return 1;
    return hash;
}
int in_hash(int n){
	int next, nn;
		next = utmphead->hashhead[0];
		while(next) {
			if (n==next) {
				return 1;
			}
			nn = utmphead->next[next - 1];
			next = nn;
		}
		return 0;
}

int in_list(int i){
	int j,k;

	k=utmphead->listhead;
	for(j=utmphead->list_next[k-1]; j!=k && j; j=utmphead->list_next[j-1]){
		if(j==i+1) return 1;
	}
	return 0;
}

static int add_empty(int uent){
	//struct user_info zeroinfo;

    utmphead->next[uent - 1] = utmphead->hashhead[0];
    utmphead->hashhead[0] = uent;
    /* Delete the user's msglist entry from webmsgd,
     * if the user is login from web. */
	/*
    if (utmpshm->uinfo[uent - 1].pid == 1)
        delfrom_msglist(uent, utmpshm->uinfo[uent - 1].userid);
		*/
	/*
    zeroinfo.active = false;
    zeroinfo.pid = 0;
    zeroinfo.invisible = true;
    zeroinfo.sockactive = false;
    zeroinfo.sockaddr = 0;
    zeroinfo.destuid = 0;

    utmpshm->uinfo[uent - 1] = zeroinfo;
	*/
    return 0;
}
int utmp_hash(const char *userid);
int utmp_lock(void);
void utmp_unlock(int fd);

int main(void)
{
    int n,prev,pos,next,nn,ppd;
	int pig=0;
	int i,j;//,k;
	//int inlist=0;
    int utmpfd;//, hashkey;

    init_all();
	pos=-1;

#if 1
    utmpfd = utmp_lock();

	utmphead->hashhead[0]=0;

    for (i = 0; i <= USHM_SIZE - 1; i++) {
		if (!utmpshm->uinfo[i].active) {
                        add_empty(i+1);
		}
	}

                        utmp_unlock(utmpfd);

						printf("end\n");
//						return;
#endif
						j=0;
		for(i = utmphead->hashhead[0]; i; i = utmphead->next[i - 1])
			j++;

		printf("items on hashhead[0] chain:%d\n",j);
		j=0; pig=0;
		for(i=0;i<USHM_SIZE;i++){
			if (!utmpshm->uinfo[i].active) j++;
			else pig++;
		}
		printf("direct iteration: inactive:%d, active:%d\n",j,pig);
pig=0;
    for(n=1;n<=UTMP_HASHSIZE;n++) {
        ppd=false;
        next = utmphead->hashhead[n];
        prev = -1;
        while(next) {
            nn = utmphead->next[next - 1];
            if (!utmpshm->uinfo[next - 1].active) {
                if (pos==-1) pos=prev;
                //printf("%d, %d, %s\n", next, nn, utmpshm->uinfo[next - 1].userid);
                ppd=true;
            } else {
				int hash = utmp_hash(getuserid2(utmpshm->uinfo[next - 1].uid));
				if (hash!=n) {
					printf("utmp_hash err: %d %d %d %d %s\n", n, next,
							utmphead->hashhead[n], nn,
							getuserid2(utmpshm->uinfo[next - 1].uid));
					if (0) {
						if (utmphead->hashhead[n] == next) {
							utmphead->hashhead[n] = nn;
						} else {
							utmphead->next[prev - 1] = nn;
						}
					}
				}
			}
            prev = next;
            next = nn;
            pig++;
        }

        if(ppd) printf("ERROR:%d,%d,%d\n", n, utmphead->hashhead[n], pos);
    }
    printf("active: %d, total: %d\n", pig, pig+j);
	n=utmphead->listhead;
	pig=0; prev=-1;
	while(n) {
		pig++;
		if (prev>0) {
			if (strcasecmp(utmpshm->uinfo[prev-1].userid, utmpshm->uinfo[n-1].userid) > 0) {
				printf("list chain error: %d %d\n", prev, n);
			}
		}
		prev=n;
		n=utmphead->list_next[n-1];
		if (utmphead->list_prev[n-1]!=prev) {
			printf("list chain list_prev error: %d %d\n", prev, n);
		}
		if (n==utmphead->listhead) {
			break;
		}
	}
	printf("active on list chain: %d\n", pig);
//		return 0;
#if 0
		for (i = 0; i <= USHM_SIZE - 1; i++) {
		if (!utmpshm->uinfo[i].active) {
			if(!in_hash(i+1)){
				//printf("%d empty but not in hash\n", i);
				pig ++;
				if(in_list(i))
					inlist++;
				//if(pig >=40) break;
			}
		}
	}


	printf("%d total,%d inlist\n", pig, inlist);
	if(i==USHM_SIZE) return;

	return;

	k=utmphead->listhead;
	for(j=utmphead->list_next[k-1]; j!=k && j; j=utmphead->list_next[j-1]){
		if(j==i+1) printf("in listhead\n");
	}

	return;
#endif
	pig=0;
		next = utmphead->hashhead[0];
		while(next) {
			if (pos!=-1 &&pos==next) {
				printf("%d: on empty list\n", pos);
				return 0;
			}
			nn = utmphead->next[next - 1];
			next = nn;
			pig++;
		}
		printf("%d %ld %ld\n", pig, utmphead->uptime, time(NULL));
        time_t now = time(NULL); int web=0;int kick=0;
pig=0;
            for (n = 0; n < USHM_SIZE; n++) {
                struct user_info *uentp = &(utmpshm->uinfo[n]);
				//printf("%d, %d\n", n, uentp->pid);
                if ((uentp->pid == 1)
                    && ((now - uentp->freshtime) < IDLE_TIMEOUT)) {
                    continue;
                }
				if (uentp->pid==0 && uentp->active) kick++;
                if (/*uentp->active &&*/ uentp->pid && kill(uentp->pid, 0) == -1) {     /*uentp¼ì²é */
					pig++;
					if(uentp->pid==1) web++;
                }
            }
			printf("killable: %d, web: %d, pid0: %d\n", pig, web, kick);
    return 0;
}
