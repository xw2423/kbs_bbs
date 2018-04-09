#ifndef PHP_BBS_MAIL_H
#define PHP_BBS_MAIL_H

/* 信件相关 */

PHP_FUNCTION(bbs_checknewmail);
PHP_FUNCTION(bbs_mail_get_limit);
PHP_FUNCTION(bbs_mail_get_num);
PHP_FUNCTION(bbs_getmailnum);
PHP_FUNCTION(bbs_getmailnum2);
PHP_FUNCTION(bbs_getmails);
PHP_FUNCTION(bbs_getmailusedspace);
PHP_FUNCTION(bbs_is_save2sent);
PHP_FUNCTION(bbs_can_send_mail);
PHP_FUNCTION(bbs_sufficient_score_to_sendmail);
PHP_FUNCTION(bbs_loadmaillist);
PHP_FUNCTION(bbs_changemaillist);

PHP_FUNCTION(bbs_postmail);
PHP_FUNCTION(bbs_mail_file);
PHP_FUNCTION(bbs_delmail);
PHP_FUNCTION(bbs_setmailreaded);
PHP_FUNCTION(bbs_domailforward);
PHP_FUNCTION(bbs_sendmail);

PHP_FUNCTION(bbs_get_refer);
PHP_FUNCTION(bbs_load_refer);
PHP_FUNCTION(bbs_read_refer);
PHP_FUNCTION(bbs_delete_refer);
PHP_FUNCTION(bbs_read_all_refer);
PHP_FUNCTION(bbs_truncate_refer);

PHP_FUNCTION(bbs_new_msg_get_message);
PHP_FUNCTION(bbs_new_msg_get_attachment);
PHP_FUNCTION(bbs_new_msg_get_new);
PHP_FUNCTION(bbs_new_msg_get_users);
PHP_FUNCTION(bbs_new_msg_load_users);
PHP_FUNCTION(bbs_new_msg_get_user_messages);
PHP_FUNCTION(bbs_new_msg_load_user_messages);
PHP_FUNCTION(bbs_new_msg_delete);
PHP_FUNCTION(bbs_new_msg_forward);
PHP_FUNCTION(bbs_new_msg_read);
PHP_FUNCTION(bbs_new_msg_send);
PHP_FUNCTION(bbs_new_msg_get_capacity);
PHP_FUNCTION(bbs_new_msg_get_size);

#define PHP_BBS_MAIL_EXPORT_FUNCTIONS \
    PHP_FE(bbs_checknewmail, NULL) \
    PHP_FE(bbs_mail_get_limit, NULL) \
    PHP_FE(bbs_mail_get_num, NULL) \
    PHP_FE(bbs_getmailnum, third_arg_force_ref_011) \
    PHP_FE(bbs_getmailnum2, NULL) \
    PHP_FE(bbs_getmails, NULL) \
    PHP_FE(bbs_getmailusedspace, NULL) \
    PHP_FE(bbs_is_save2sent, NULL) \
    PHP_FE(bbs_can_send_mail, NULL) \
    PHP_FE(bbs_sufficient_score_to_sendmail, NULL) \
    PHP_FE(bbs_loadmaillist, NULL) \
    PHP_FE(bbs_changemaillist, NULL) \
    PHP_FE(bbs_postmail, NULL) \
    PHP_FE(bbs_mail_file, NULL) \
    PHP_FE(bbs_delmail,NULL) \
    PHP_FE(bbs_setmailreaded,NULL) \
    PHP_FE(bbs_domailforward, NULL) \
    PHP_FE(bbs_sendmail, NULL) \
    PHP_FE(bbs_get_refer, NULL) \
    PHP_FE(bbs_load_refer, NULL) \
    PHP_FE(bbs_read_refer, NULL) \
    PHP_FE(bbs_delete_refer, NULL) \
    PHP_FE(bbs_read_all_refer, NULL) \
    PHP_FE(bbs_truncate_refer, NULL) \
    PHP_FE(bbs_new_msg_get_message, NULL) \
    PHP_FE(bbs_new_msg_get_attachment, NULL) \
    PHP_FE(bbs_new_msg_get_new, NULL) \
    PHP_FE(bbs_new_msg_get_users, NULL) \
    PHP_FE(bbs_new_msg_load_users, NULL) \
    PHP_FE(bbs_new_msg_get_user_messages, NULL) \
    PHP_FE(bbs_new_msg_load_user_messages, NULL) \
    PHP_FE(bbs_new_msg_delete, NULL) \
    PHP_FE(bbs_new_msg_forward, NULL) \
    PHP_FE(bbs_new_msg_read, NULL) \
    PHP_FE(bbs_new_msg_send, NULL) \
    PHP_FE(bbs_new_msg_get_capacity, NULL) \
    PHP_FE(bbs_new_msg_get_size, NULL)
#endif //PHP_BBS_MAIL_H

