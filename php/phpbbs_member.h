#ifndef PHP_BBS_MEMBER_H
#define PHP_BBS_MEMBER_H

#ifdef ENABLE_BOARD_MEMBER
/* 驻版用户相关, windinsn, 2012.8.12 */
	
PHP_FUNCTION(bbs_load_board_member_config);
PHP_FUNCTION(bbs_save_board_member_config);
PHP_FUNCTION(bbs_join_board_member);
PHP_FUNCTION(bbs_leave_board_member);
PHP_FUNCTION(bbs_set_board_member_status);
PHP_FUNCTION(bbs_remove_board_member);
PHP_FUNCTION(bbs_get_board_member);
PHP_FUNCTION(bbs_load_board_members);
PHP_FUNCTION(bbs_load_member_boards);
PHP_FUNCTION(bbs_get_board_members);
PHP_FUNCTION(bbs_get_member_boards);
PHP_FUNCTION(bbs_get_user_max_boards);
PHP_FUNCTION(bbs_load_board_member_request);
PHP_FUNCTION(bbs_load_board_member_articles);
PHP_FUNCTION(bbs_get_board_member_articles);
PHP_FUNCTION(bbs_get_board_member_titles);
PHP_FUNCTION(bbs_load_board_member_titles);
PHP_FUNCTION(bbs_get_board_member_title);
PHP_FUNCTION(bbs_get_bmp_value);
PHP_FUNCTION(bbs_get_bmp_name);
PHP_FUNCTION(bbs_set_board_member_title);
PHP_FUNCTION(bbs_create_board_member_title);
PHP_FUNCTION(bbs_remove_board_member_title);
PHP_FUNCTION(bbs_modify_board_member_title);
PHP_FUNCTION(bbs_view_member_managers);
PHP_FUNCTION(bbs_load_ar_timeline);
PHP_FUNCTION(bbs_load_ar_attachment);
PHP_FUNCTION(bbs_member_post_perm);

#define PHP_BBS_MEMBER_EXPORT_FUNCTIONS \
    PHP_FE(bbs_load_board_member_config, NULL) \
    PHP_FE(bbs_save_board_member_config, NULL) \
    PHP_FE(bbs_join_board_member, NULL) \
    PHP_FE(bbs_leave_board_member, NULL) \
    PHP_FE(bbs_set_board_member_status, NULL) \
    PHP_FE(bbs_remove_board_member, NULL) \
    PHP_FE(bbs_get_board_member, NULL) \
    PHP_FE(bbs_load_board_members, NULL) \
    PHP_FE(bbs_load_member_boards, NULL) \
    PHP_FE(bbs_get_board_members, NULL) \
    PHP_FE(bbs_get_member_boards, NULL) \
	PHP_FE(bbs_get_user_max_boards, NULL) \
	PHP_FE(bbs_load_board_member_request, NULL) \
	PHP_FE(bbs_load_board_member_articles, NULL) \
	PHP_FE(bbs_get_board_member_articles, NULL) \
	PHP_FE(bbs_get_board_member_titles, NULL) \
	PHP_FE(bbs_load_board_member_titles, NULL) \
	PHP_FE(bbs_get_board_member_title, NULL) \
	PHP_FE(bbs_get_bmp_value, NULL) \
	PHP_FE(bbs_get_bmp_name, NULL) \
	PHP_FE(bbs_set_board_member_title, NULL) \
	PHP_FE(bbs_create_board_member_title, NULL) \
	PHP_FE(bbs_remove_board_member_title, NULL) \
	PHP_FE(bbs_modify_board_member_title, NULL) \
	PHP_FE(bbs_view_member_managers, NULL) \
	PHP_FE(bbs_load_ar_timeline, NULL) \
	PHP_FE(bbs_load_ar_attachment, NULL) \
	PHP_FE(bbs_member_post_perm, NULL)

#endif // ENABLE_BOARD_MEMBER	
#endif //PHP_BBS_MEMBER_H

