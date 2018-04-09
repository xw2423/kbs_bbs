/*
 * 关于共享内存存储驻版信息的文档简要注释于此，暂存。
 * windinsn, 2013-03-05
 */
#ifndef _KBSBBS_MEMBER_CACHE_H_
#define _KBSBBS_MEMBER_CACHE_H_

#ifdef ENABLE_MEMBER_CACHE

#ifndef MAX_MEMBERS
#define MAX_MEMBERS 400000
#endif

#ifndef MAX_MEMBER_TITLES
#define MAX_MEMBER_TITLES 10000
#endif

#ifndef MEBERS_TMP_FILE
#define MEBERS_TMP_FILE ".MEMBERS.TMP"
#endif

#ifndef MAX_BOARD_MEMBERS
#define MAX_BOARD_MEMBERS 10000
#endif

#ifndef MAX_MEMBER_BOARDS
#define MAX_MEMBER_BOARDS 100
#endif

/*
 * struct.h 中 board_member 的瘦身版，便于存储和比较
 * 40byte
 */
struct MEMBER_CACHE_NODE {
	int bid;                   // board的bid, 对应 board_member.board
	int uid;                   // user的uid, 对应 board_member.user
	time_t time;               // 加入驻版的时间
	int status;                // 驻版状态
	unsigned int score;        // 驻版积分
	unsigned int flag;         // 核心驻版用户的权限
	int title;                 // 驻版称号的ID, 这个值与数据库中的记录不一致，在做转换时自动生成
	
	int user_next;             // 同一驻版用户(同uid记录)的下一条记录的index, 1-based
	int board_next;            // 同一版面(同bid记录)下一位驻版用户记录的index, 1-based
};
/*
 * strcut.h 中 board_member_title 的瘦身版，便于存储和比较
 */
struct MEMBER_CACHE_TITLE {
	int id;                    // 这个id和board_member_title中的id是不一致的，为了加速索引
	                           // 此id被设定为与本MEMBER_CACHE_TITLE在MEMBER_CACHE.titles中的index一致
							   // 在用转换程序初始化.MEMBER.TITLES时自动生成
	int bid;                   // 所属版面的bid
	char name[STRLEN];         // 驻版称号
	int serial;                // 排序号
	unsigned int flag;         // 一些可扩展的flag, 目前未使用
	
	int next;                  // 同一版面(同bid记录)的下一驻版称号记录的index, 1-based
};
/*
 * MEMBER_CACHE 是共享内存 (shmid=MEMBER_CACHE_SHMKEY) 的主体结构
 * struct MEMBER_CACHE *membershm
 */
struct MEMBER_CACHE {
	int users[MAXUSERS];                                  // 此数组保存的是用户第一条记录的index(1-based), 这个index表明 MEMBER_CACHE.nodes[index-1] 为该用户驻版信息的第一条记录, 后续记录通过 MEMBER_CACHE.nodes[index-1].user_next 获得索引.
	int boards[MAXBOARD];                                 // 此数组保存的是版面第一条记录的index(1-based), 这个index表明 MEMBER_CAHCE.nodes[index-1] 为该版面驻版信息的第一条记录, 后续记录通过 MEMBER_CACHE.nodes[index-1].board_next 获得索引.
	int board_titles[MAXBOARD];                           // 此数组保存的是版面第一条驻版称号记录的index(1-based), 这个index表明 MEMBER_CACHE.titles[index-1] 为该版面的第一条驻版称号, 后续记录通过 MEMBER_CACHE.titles[index-1].next 获得索引.
	struct MEMBER_CACHE_NODE nodes[MAX_MEMBERS];          // 驻版信息, 注意 MEMBER_CACHE.nodes 并不是紧凑的, 当某一个记录被删除时，仅将该记录的 MEMBER_CACHE_NODE.bid 和 MEMBER_CACHE_NODE.uid 设置为0, 判断方法参见 is_null_member 和 find_null_member. 新增记录时将优先使用靠前的空记录.
	struct MEMBER_CACHE_TITLE titles[MAX_MEMBER_TITLES];  // 驻版称号, MEMBER_CACHE.titles 也不是紧凑的
	
	int member_count;                                     // 全站驻版用户总数, 即 MEMBER_CACHE.nodes 中的非空记录数(!is_null_member)
	int title_count;                                      // 全站驻版称号总数
};
/*
 * 由于 MEMBER_CAHCE 中没有复杂索引，而 libmember.c 在load驻版信息时支持复杂排序,
 * 因此在查询这些记录集时需要先将所有记录取出, 再通过qsort进行排序后取中段数据。
 * 考虑到无论是版面、还是用户，驻版信息总量都较小，因此我认为这个操作是“可接受的”，
 * 但仍是ulgy和dirty的。 MEMBER_CACHE_CONTAINER 和 MEMBER_TITLE_CONTAINER 是为了
 * 进行qsort排序使用的。
 */
struct MEMBER_CACHE_CONTAINER {
	struct MEMBER_CACHE_NODE *node;
};
struct MEMBER_TITLE_CONTAINER {
	struct MEMBER_CACHE_TITLE *title;
};
/*
 * 一些常用方法说明: 
 * 1、查询用户 user_id 在 board_name 的驻版信息。
 *    获得 uid=getuser(user_id, NULL) 和 bid=getbid(board_name, NULL)，通过 MEMBER_CACHE.users[uid-1] 得到第一条记录（若为0表示该用户无任何驻版）。比对 bid 若相同获得记录；若不相同通过 user_next 获取下一条用户记录。直至 user_next == 0 表明该用户无该版面的驻版信息。
 * 2、查询用户 user_id 的所有驻版信息。
 *    获得 uid=getuser(user_id, NULL)， MEMBER_CACHE.users[uid-1] 为第一条记录在 MEMBER_CACHE.nodes 中的索引值(1-based)，通过 node 中的 user_next 获取下一条记录的索引值直至 user_next == 0 完毕。
 * 3、查询版面 board_name 的所有驻版信息。
 *    获得 bid=getbid(board_name, NULL), MEMBER_CACHE.boards[bid-1] 为第一条记录在 MEMBER_CACHE.nodes 中的索引值(1-based)，通过 node 中的 board_next 获取下一条记录的索引值直至 board_next == 0 完毕。
 * 4、查询版面 board_name 的所有驻版称号
 *    获得 bid=getbid(board_name, NULL), MEMBER_CACHE.board_titles[bid-1] 为第一条记录在 MEMBER_CACHE.titles 中的索引值(1-based)，通过 title 中的 next 获取下一条记录的索引值直至 next == 0 完毕。
 * 5、获得ID为title_id的驻版称号
 *    要求 title_id > 0 && title_id <= MAX_MEMBER_TITLES，记录即为 MEMBER_CACHE.titles[title_id-1]
 */
#endif
#endif
