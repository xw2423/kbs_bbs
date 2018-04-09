#include "php_kbs_bbs.h"
#include "SAPI.h"
#include "md5.h"

#ifdef ENABLE_LIKE
void bbs_make_like_array(zval *array, struct like *like) {
    add_assoc_string(array, "user", like->user, 1);
    add_assoc_string(array, "ip", like->ip, 1);
    add_assoc_long(array, "time", like->time);
    add_assoc_string(array, "msg", like->msg, 1);
    add_assoc_long(array, "score", like->score);
}
#endif

PHP_FUNCTION(bbs_load_like)
{
#ifdef ENABLE_LIKE
    char *path;
    int path_len;
    zval *list, *element;
    int count, i;
    struct like *likes;
    
    if (ZEND_NUM_ARGS()!=2 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz", &path, &path_len, &list) == FAILURE) {
        WRONG_PARAM_COUNT;
    }
    if (!PZVAL_IS_REF(list)) {
        zend_error(E_WARNING, "Parameter wasn't passed by reference");
        RETURN_LONG(-5);
    }
    
    count=get_like_count(path);
    if(count<0)
        RETURN_LONG(-1);
    
    if (array_init(list) != SUCCESS)
        RETURN_LONG(-2);
        
    if(count==0)
        RETURN_LONG(0);
        
    likes=(struct like *)emalloc(sizeof(struct like)*count);
    if(NULL==likes)
        RETURN_LONG(-3);
        
    count=load_like(path, 0, count, likes);
    if(count>0) for(i=0;i<count;i++) {
        MAKE_STD_ZVAL(element);
        array_init(element);
        bbs_make_like_array(element, likes+i);
        zend_hash_index_update(Z_ARRVAL_P(list), i, (void *) &element, sizeof(zval*), NULL);
    }
    efree(likes);
    if(count<0)
        RETURN_LONG(-4);
    RETURN_LONG(count);
#else
    RETURN_LONG(-1);
#endif
}

PHP_FUNCTION(bbs_add_like) {
#ifdef ENABLE_LIKE
    long article_id, score;
    char *board_id, *msg, *tag;
    int board_id_len, msg_len, tag_len;
    
    const struct boardheader *board;
    struct fileheader article;
    char path[MAXPATH];
    int fd, ret;
    
    if (ZEND_NUM_ARGS()!=5 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "slssl", &board_id, &board_id_len, &article_id, &msg, &msg_len, &tag, &tag_len, &score) == FAILURE) {
        WRONG_PARAM_COUNT;
    }
    
    if ((board=getbcache(board_id))==NULL)
        RETURN_LONG(-101);
        
    setbdir(DIR_MODE_NORMAL, path, board->filename);
    if ((fd=open(path, O_RDWR, 0644))==-1)
        RETURN_LONG(-102);
    ret=get_records_from_id(fd, article_id, &article, 1, NULL);
    close(fd);
    if(ret==0)
        RETURN_LONG(-103);
    
    ret=add_user_like(board, &article, score, msg, tag);
    RETURN_LONG(ret);
#else
    RETURN_LONG(-1);
#endif
}

PHP_FUNCTION(bbs_del_like) {
#ifdef ENABLE_LIKE
    long article_id;
    char *board_id, *user_id;
    int board_id_len, user_id_len;
    
    struct userec *user;
    const struct boardheader *board;
    struct fileheader article;
    char path[MAXPATH];
    int fd, ret;
    
    if (ZEND_NUM_ARGS()!=3 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sls", &board_id, &board_id_len, &article_id, &user_id, &user_id_len) == FAILURE) {
        WRONG_PARAM_COUNT;
    }
    
    if ((board=getbcache(board_id))==NULL)
        RETURN_LONG(-101);
        
    if(!getuser(user_id, &user))
        RETURN_LONG(-102);
    
    setbdir(DIR_MODE_NORMAL, path, board->filename);
    if ((fd=open(path, O_RDWR, 0644))==-1)
        RETURN_LONG(-103);
    ret=get_records_from_id(fd, article_id, &article, 1, NULL);
    close(fd);
    if(ret==0)
        RETURN_LONG(-104);
    
    ret=delete_user_like(board, &article, user);
    RETURN_LONG(ret);
#else
    RETURN_LONG(-1);
#endif
}

PHP_FUNCTION(bbs_like_score) {
#ifdef ENABLE_LIKE
    zval *list, *element;
    int i, max, j;
    
    if (ZEND_NUM_ARGS()!=1 || zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &list) == FAILURE) {
        WRONG_PARAM_COUNT;
    }
    if (!PZVAL_IS_REF(list)) {
        zend_error(E_WARNING, "Parameter wasn't passed by reference");
        RETURN_FALSE;
    }
    
    if (array_init(list) != SUCCESS)
        RETURN_FALSE;
        
    max=get_like_max_score();
	j=0;
    for(i=-max;i<=max;i++){
        MAKE_STD_ZVAL(element);
        array_init(element);
        add_assoc_long(element, "score", i);
        add_assoc_long(element, "cost", get_like_user_score(i));
        add_assoc_long(element, "award", get_like_award_score(i));
        zend_hash_index_update(Z_ARRVAL_P(list), j++, (void *) &element, sizeof(zval*), NULL);
    }
    
    RETURN_TRUE;
#else
    RETURN_FALSE;
#endif
}

/*
 * refer Ecma-262
 * '\033'  -> \r (not exactly the same thing, but borrow...)
 * '\n'    -> \n
 * '\\'    -> \\
 * '\''    -> \'
 * '\"'    -> \"
 * '\0'    -> possible start of attachment
 * 0 <= char < 32 -> ignore
 * others  -> passthrough
 */

PHP_FUNCTION(bbs2_readfile)
{
    char *filename;
    int filename_len;
    char *output_buffer;
    int output_buffer_len, output_buffer_size, j;
    char c;
    char *ptr, *cur_ptr;
    off_t ptrlen, mmap_ptrlen;
    int in_chinese = false;
    int chunk_size = 51200;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &filename, &filename_len) == FAILURE) {
        WRONG_PARAM_COUNT;
    }

    if (safe_mmapfile(filename, O_RDONLY, PROT_READ, MAP_SHARED, &ptr, &mmap_ptrlen, NULL) == 0)
        RETURN_LONG(-1);

    j = ptrlen = mmap_ptrlen;
    if (j > chunk_size) j = chunk_size;
    output_buffer_size = 2 * j + 16;
    output_buffer = (char*)emalloc(output_buffer_size);
    output_buffer_len = 0;
    cur_ptr = ptr;
    strcpy(output_buffer + output_buffer_len, "prints('");
    output_buffer_len += 8;
    while (1) {
        for (; j > 0 ; j--) {
            c = *cur_ptr;
#ifdef ENABLE_LIKE
            /*
             * 略过整个Like结构, added by windinsn
             */
            if(c==LIKE_PAD[0]) {
                if(ptrlen>LIKE_PAD_SIZE && !memcmp(cur_ptr, LIKE_PAD, LIKE_PAD_SIZE)) {
                    cur_ptr += LIKE_PAD_SIZE;
                    ptrlen -= LIKE_PAD_SIZE;
                    while(ptrlen>0 && (*cur_ptr)!='\n') {
                        cur_ptr += sizeof(struct like);
                        ptrlen -= sizeof(struct like);
                    }
                }
                ptrlen--; 
                cur_ptr++;
                
                if(ptrlen<0) { 
                    ptrlen=0;
                    break;
                }
                continue;
            }
#endif
            if (c == '\0') { //assume ATTACHMENT_PAD[0] is '\0'
                if (ptrlen >= ATTACHMENT_SIZE + sizeof(int) + 2) {
                    if (!memcmp(cur_ptr, ATTACHMENT_PAD, ATTACHMENT_SIZE)) {
                        ptrlen = -ptrlen;
                        break;
                    }
                }
                ptrlen--; cur_ptr++;
                continue;
            }
            if (c < 0) {
                in_chinese = !in_chinese;
                output_buffer[output_buffer_len++] = c;
            } else {
                do {
                    if (c == '\n') c = 'n';
                    else if (c == '\033') c = 'r';
                    else if (c != '\\' && c != '\'' && c != '\"'
                             && c != '/'    /* to prevent things like </script> */
                            ) {
                        if (c >= 32) {
                            output_buffer[output_buffer_len++] = c;
                        }
                        break;
                    }
                    if (in_chinese && c == 'n') {
                        output_buffer[output_buffer_len++] = ' ';
                    }
                    output_buffer[output_buffer_len++] = '\\';
                    output_buffer[output_buffer_len++] = c;
                } while (0);
                in_chinese = false;
            }
            ptrlen--; cur_ptr++;
        }
        if (ptrlen <= 0) break;
        j = ptrlen;
        if (j > chunk_size) j = chunk_size;
        output_buffer_size += 2 * j;
        output_buffer = (char*)erealloc(output_buffer, output_buffer_size);
        if (output_buffer == NULL) RETURN_LONG(3);
    }
    if (in_chinese) {
        output_buffer[output_buffer_len++] = ' ';
    }
    strcpy(output_buffer + output_buffer_len, "');");
    output_buffer_len += 3;

    if (ptrlen < 0) { //attachment
        char *attachfilename, *attachptr;
        char buf[1024];
        char *startbufptr, *bufptr;
        long attach_len, attach_pos, newlen;
        int l;

        ptrlen = -ptrlen;
        strcpy(buf, "attach('");
        startbufptr = buf + strlen(buf);
        while (ptrlen > 0) {
            if (((attachfilename = checkattach(cur_ptr, ptrlen,
                                               &attach_len, &attachptr)) == NULL)) {
                break;
            }
            attach_pos = attachfilename - ptr;
            newlen = attachptr - cur_ptr + attach_len;
            cur_ptr += newlen;
            ptrlen -= newlen;
            if (ptrlen < 0) break;
            bufptr = startbufptr;
            while (*attachfilename != '\0') {
                switch (*attachfilename) {
                    case '\'':
                    case '\"':
                    case '\\':
                        *bufptr++ = '\\'; /* TODO: boundary check */
                        /* break is missing *intentionally* */
                    default:
                        *bufptr++ = *attachfilename++;  /* TODO: boundary check */
                }
            }
            sprintf(bufptr, "', %ld, %ld);", attach_len, attach_pos);  /* TODO: boundary check */

            l = strlen(buf);
            if (output_buffer_len + l > output_buffer_size) {
                output_buffer_size = output_buffer_size + sizeof(buf) * 10;
                output_buffer = (char*)erealloc(output_buffer, output_buffer_size);
                if (output_buffer == NULL) RETURN_LONG(3);
            }
            strcpy(output_buffer + output_buffer_len, buf);
            output_buffer_len += l;
        }
    }
    end_mmapfile(ptr, mmap_ptrlen, -1);

    RETVAL_STRINGL(output_buffer, output_buffer_len, 0);
}

PHP_FUNCTION(bbs2_readfile_text)
{
    char *filename;
    int filename_len;
    long maxchar;
    long escape_flag; /* 0(default) - escape <>&\n, 1 - double escape <>&\n, 2 - escape <>&\n and space */
    char *output_buffer;
    int output_buffer_len, output_buffer_size, last_return = 0;
    char c;
    char *ptr, *cur_ptr;
    off_t ptrlen, mmap_ptrlen;
    int in_escape = false, is_last_space = false;
    int i;
    char escape_seq[4][16], escape_seq_len[4];

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sll", &filename, &filename_len, &maxchar, &escape_flag) == FAILURE) {
        WRONG_PARAM_COUNT;
    }

    if (safe_mmapfile(filename, O_RDONLY, PROT_READ, MAP_SHARED, &ptr, &mmap_ptrlen, NULL) == 0)
        RETURN_LONG(-1);

    ptrlen = mmap_ptrlen;

    if (!maxchar) {
        maxchar = ptrlen;
    } else if (ptrlen > maxchar) {
        ptrlen = maxchar;
    }
    output_buffer_size = 2 * maxchar;
    output_buffer = (char*)emalloc(output_buffer_size);
    output_buffer_len = 0;
    cur_ptr = ptr;
    if (escape_flag == 1) {
        strcpy(escape_seq[0], "&amp;amp;");
        strcpy(escape_seq[1], "&amp;lt;");
        strcpy(escape_seq[2], "&amp;gt;");
        strcpy(escape_seq[3], "&lt;br/&gt;");
    } else {
        strcpy(escape_seq[0], "&amp;");
        strcpy(escape_seq[1], "&lt;");
        strcpy(escape_seq[2], "&gt;");
        strcpy(escape_seq[3], "<br/>");
    }
    for (i=0;i<4;i++) escape_seq_len[i] = strlen(escape_seq[i]);
    while (ptrlen > 0) {
        c = *cur_ptr;
#ifdef ENABLE_LIKE
        if (c==LIKE_PAD[0]) {
            break;
        } else 
#endif
        if (c == '\0') { //assume ATTACHMENT_PAD[0] is '\0'
            break;
        } else if (c == '\033') {
            in_escape = true;
        } else if (!in_escape) {
            if (output_buffer_len + 16 > output_buffer_size) {
                output_buffer = (char*)erealloc(output_buffer, output_buffer_size += 128);
            }
            if (escape_flag == 2 && c == ' ') {
                if (!is_last_space) {
                    output_buffer[output_buffer_len++] = ' ';
                } else {
                    strcpy(output_buffer + output_buffer_len, "&nbsp;");
                    output_buffer_len += 6;
                }
                is_last_space = !is_last_space;
            } else {
                is_last_space = false;
                switch (c) {
                    case '&':
                        strcpy(output_buffer + output_buffer_len, escape_seq[0]);
                        output_buffer_len += escape_seq_len[0];
                        break;
                    case '<':
                        strcpy(output_buffer + output_buffer_len, escape_seq[1]);
                        output_buffer_len += escape_seq_len[1];
                        break;
                    case '>':
                        strcpy(output_buffer + output_buffer_len, escape_seq[2]);
                        output_buffer_len += escape_seq_len[2];
                        break;
                    case '\n':
                        strcpy(output_buffer + output_buffer_len, escape_seq[3]);
                        output_buffer_len += escape_seq_len[3];
                        last_return = output_buffer_len;
                        is_last_space = true;
                        break;
                    default:
                        if (c < 0 || c >= 32)
                            output_buffer[output_buffer_len++] = c;
                        break;
                }
            }
        } else if (isalpha(c)) {
            in_escape = false;
        }
        ptrlen--; cur_ptr++;
    }

    end_mmapfile(ptr, mmap_ptrlen, -1);

    RETVAL_STRINGL(output_buffer, last_return, 0);
}
#ifdef NFORUM
/* fancy Aug 14 2011，nForum 不带颜色的正文 + 附件输出
   return array(
       'content' => 'blablabla',
       'attachment' => array(
           array('name' => 'filename.jpg', size => 12345, pos => 678),
           ...
       )
   );
*/
PHP_FUNCTION(bbs2_readfile_nforum)
{
    char *filename;
    int filename_len;
    long maxchar;
    long escape_flag; /* 0(default) - escape <>&\n, 1 - double escape <>&\n, 2 - escape <>&\n and space */
    char *output_buffer;
    int output_buffer_len, output_buffer_size, last_return = 0;
    char c;
    char *ptr, *cur_ptr;
    off_t ptrlen, mmap_ptrlen;
    int in_escape = false, is_last_space = false;
    int i;
    char escape_seq[4][16], escape_seq_len[4];
    zval *attachment;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sll", &filename, &filename_len, &maxchar, &escape_flag) == FAILURE) {
        WRONG_PARAM_COUNT;
    }

    if (safe_mmapfile(filename, O_RDONLY, PROT_READ, MAP_SHARED, &ptr, &mmap_ptrlen, NULL) == 0)
        RETURN_LONG(-1);

    ptrlen = mmap_ptrlen;

    if (!maxchar) {
        maxchar = ptrlen;
    } else if (ptrlen > maxchar) {
        ptrlen = maxchar;
    }
    output_buffer_size = 2 * maxchar;
    output_buffer = (char*)emalloc(output_buffer_size);
    output_buffer_len = 0;
    cur_ptr = ptr;
    if (escape_flag == 1) {
        strcpy(escape_seq[0], "&amp;amp;");
        strcpy(escape_seq[1], "&amp;lt;");
        strcpy(escape_seq[2], "&amp;gt;");
        strcpy(escape_seq[3], "&lt;br/&gt;");
    } else {
        strcpy(escape_seq[0], "&amp;");
        strcpy(escape_seq[1], "&lt;");
        strcpy(escape_seq[2], "&gt;");
        strcpy(escape_seq[3], "<br/>");
    }
    for (i=0;i<4;i++) escape_seq_len[i] = strlen(escape_seq[i]);
    while (ptrlen > 0) {
        c = *cur_ptr;
#ifdef ENABLE_LIKE
        /*
         * 略过整个Like结构, added by windinsn
         */
        if(c==LIKE_PAD[0]) {
            if(ptrlen>LIKE_PAD_SIZE && !memcmp(cur_ptr, LIKE_PAD, LIKE_PAD_SIZE)) {
                cur_ptr += LIKE_PAD_SIZE;
                ptrlen -= LIKE_PAD_SIZE;
                while(ptrlen>0 && (*cur_ptr)!='\n') {
                    cur_ptr += sizeof(struct like);
                    ptrlen -= sizeof(struct like);
                }
            }
            ptrlen--; 
            cur_ptr++;
            
            if(ptrlen<0) { 
                ptrlen=0;
                break;
            }
            continue;
        } else 
#endif
        if (c == '\0') { //assume ATTACHMENT_PAD[0] is '\0'
            if (ptrlen >= ATTACHMENT_SIZE + sizeof(int) + 2) {
                if (!memcmp(cur_ptr, ATTACHMENT_PAD, ATTACHMENT_SIZE)) {
                    ptrlen = -ptrlen;
                    break;
                }
            }
        } else if (c == '\033') {
            in_escape = true;
        } else if (!in_escape) {
            if (output_buffer_len + 16 > output_buffer_size) {
                output_buffer = (char*)erealloc(output_buffer, output_buffer_size += 128);
            }
            if (escape_flag == 2 && c == ' ') {
                if (!is_last_space) {
                    output_buffer[output_buffer_len++] = ' ';
                } else {
                    strcpy(output_buffer + output_buffer_len, "&nbsp;");
                    output_buffer_len += 6;
                }
                is_last_space = !is_last_space;
            } else {
                is_last_space = false;
                switch (c) {
                    case '&':
                        strcpy(output_buffer + output_buffer_len, escape_seq[0]);
                        output_buffer_len += escape_seq_len[0];
                        break;
                    case '<':
                        strcpy(output_buffer + output_buffer_len, escape_seq[1]);
                        output_buffer_len += escape_seq_len[1];
                        break;
                    case '>':
                        strcpy(output_buffer + output_buffer_len, escape_seq[2]);
                        output_buffer_len += escape_seq_len[2];
                        break;
                    case '\n':
                        strcpy(output_buffer + output_buffer_len, escape_seq[3]);
                        output_buffer_len += escape_seq_len[3];
                        last_return = output_buffer_len;
                        is_last_space = true;
                        break;
                    default:
                        if (c < 0 || c >= 32)
                            output_buffer[output_buffer_len++] = c;
                        break;
                }
            }
        } else if (isalpha(c)) {
            in_escape = false;
        }
        ptrlen--; cur_ptr++;
    }

    MAKE_STD_ZVAL(attachment);
    array_init(return_value);
    array_init(attachment);

    add_assoc_stringl(return_value, "content", output_buffer, last_return, 1);

    if (ptrlen < 0) { //attachment
        char *attachfilename, *attachptr;
        long attach_len, attach_pos, newlen;
        zval *item;

        ptrlen = -ptrlen;
        while (ptrlen > 0) {
            if (((attachfilename = checkattach(cur_ptr, ptrlen,
                                               &attach_len, &attachptr)) == NULL)) {
                break;
            }
            attach_pos = attachfilename - ptr;
            newlen = attachptr - cur_ptr + attach_len;
            cur_ptr += newlen;
            ptrlen -= newlen;
            if (ptrlen < 0) break;
            MAKE_STD_ZVAL(item);
            array_init(item);
            add_assoc_string(item, "name", attachfilename, 1);
            add_assoc_long(item, "size", attach_len);
            add_assoc_long(item, "pos", attach_pos);
            add_next_index_zval(attachment, item);
        }
    }
    add_assoc_zval(return_value, "attachment", attachment);

    end_mmapfile(ptr, mmap_ptrlen, -1);

}


#endif

PHP_FUNCTION(bbs_file_output_attachment)
{
    char *filename;
    int filename_len;
    char *attachname = NULL;
    int attachname_len = 0;
    long attachpos;
    char *ptr;
    char *sendptr = NULL;
    int sendlen;
    off_t ptrlen, mmap_ptrlen;
    const char *mime;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl|s", &filename, &filename_len, &attachpos, &attachname, &attachname_len) == FAILURE) {
        WRONG_PARAM_COUNT;
    }

    if (attachpos == 0 && attachname == NULL) {
        RETURN_LONG(-2);
    }

    if (safe_mmapfile(filename, O_RDONLY, PROT_READ, MAP_SHARED, &ptr, &mmap_ptrlen, NULL) == 0)
        RETURN_LONG(-1);

    ptrlen = mmap_ptrlen;

    if (attachpos == 0) {
        sendptr = ptr;
        sendlen = ptrlen;
    } else if (attachpos >= 0 && attachpos < ptrlen) {
        char *p, *pend = ptr + ptrlen;
        p = attachname = ptr + attachpos;
        while (*p && p < pend) {
            p++;
        }
        p++;
        if (p <= pend - 4) {
            memcpy(&sendlen, p, 4);
            sendlen = ntohl(sendlen);
            if (sendlen >= 0 && pend - 4 - p >= sendlen) {
                sendptr = p + 4;
            }
        }
    }

    if (sendptr) {
        sapi_header_line ctr = {0};
        char buf[256];
        ctr.line = buf;

        snprintf(buf, 256, "Content-Type: %s", mime = get_mime_type(attachname));
        ctr.line_len = strlen(buf);
        sapi_header_op(SAPI_HEADER_ADD, &ctr TSRMLS_CC);

        snprintf(buf, 256, "Accept-Ranges: bytes");
        ctr.line_len = strlen(buf);
        sapi_header_op(SAPI_HEADER_ADD, &ctr TSRMLS_CC);

        snprintf(buf, 256, "Content-Length: %d", sendlen);
        ctr.line_len = strlen(buf);
        sapi_header_op(SAPI_HEADER_ADD, &ctr TSRMLS_CC);

        if(strncmp(mime, "image", 5) == 0 || strcmp(mime, "application/x-shockwave-flash") == 0 || strcmp(mime, "application/pdf") == 0)
            snprintf(buf, 256, "Content-Disposition: inline;filename=%s", attachname);
        else
            snprintf(buf, 256, "Content-Disposition: attachment;filename=%s", attachname);
        ctr.line_len = strlen(buf);
        sapi_header_op(SAPI_HEADER_ADD, &ctr TSRMLS_CC);

        ZEND_WRITE(sendptr, sendlen);
    }
    end_mmapfile(ptr, mmap_ptrlen, -1);
    RETURN_LONG(0);
}






static char* output_buffer=NULL;
static int output_buffer_len=0;
static int output_buffer_size=0;

void reset_output_buffer()
{
    output_buffer=NULL;
    output_buffer_size=0;
    output_buffer_len=0;
}

static void output_printf(const char* buf, uint len)
{
    int bufLen;
    int n,newsize;
    char * newbuf;
    if (output_buffer==NULL) {
        output_buffer=(char*)emalloc(51200);  //first 50k
        if (output_buffer==NULL) {
            return;
        }
        output_buffer_size=51200;
    }
    bufLen=strlen(buf);
    if (bufLen>len) {
        bufLen=len;
    }
    n=1+output_buffer_len+bufLen-output_buffer_size;
    if (n>=0) {
        newsize=output_buffer_size+((n/102400)+1)*102400; //n*100k every time
        newbuf=(char*)erealloc(output_buffer,newsize);
        if (newbuf==NULL) {
            return;
        }
        output_buffer=newbuf;
        output_buffer_size=newsize;
    }
    memcpy(output_buffer+output_buffer_len,buf,bufLen);
    output_buffer_len+=bufLen;
}

static char* get_output_buffer()
{
    return output_buffer;
}

static int get_output_buffer_len()
{
    int len=output_buffer_len;
    output_buffer_len=0;
    return len;
}

#if 0
static int new_buffered_output(char *buf, size_t buflen, void *arg)
{
    output_printf(buf,buflen);
    return 0;
}
#endif

static int new_write(const char *buf, uint buflen)
{
    output_printf(buf, buflen);
    return 0;
}

/* 注意，当 is_preview 为 1 的时候，第一个参数 filename 就是供预览的帖子内容，而不是文件名 - atppp */
PHP_FUNCTION(bbs_printansifile)
{
    char *filename;
    int filename_len;
    long linkmode,is_tex,is_preview;
    char *ptr;
    off_t ptrlen, mmap_ptrlen;
    const int outbuf_len = 4096;
    buffered_output_t *out;
    char* attachlink;
    int attachlink_len;
    char attachdir[MAXPATH];

    if (ZEND_NUM_ARGS() == 1) {
        if (zend_parse_parameters(1 TSRMLS_CC, "s", &filename, &filename_len) != SUCCESS) {
            WRONG_PARAM_COUNT;
        }
        linkmode = 1;
        attachlink=NULL;
        is_tex=is_preview=0;
    } else if (ZEND_NUM_ARGS() == 2) {
        if (zend_parse_parameters(2 TSRMLS_CC, "sl", &filename, &filename_len, &linkmode) != SUCCESS) {
            WRONG_PARAM_COUNT;
        }
        attachlink=NULL;
        is_tex=is_preview=0;
    } else if (ZEND_NUM_ARGS() == 3) {
        if (zend_parse_parameters(3 TSRMLS_CC, "sls", &filename, &filename_len, &linkmode,&attachlink,&attachlink_len) != SUCCESS) {
            WRONG_PARAM_COUNT;
        }
        is_tex=is_preview=0;
    } else {
        if (zend_parse_parameters(5 TSRMLS_CC, "slsll", &filename, &filename_len, &linkmode,&attachlink,&attachlink_len,&is_tex,&is_preview) != SUCCESS) {
            WRONG_PARAM_COUNT;
        }
    }
    if (!is_preview) {
        if (safe_mmapfile(filename, O_RDONLY, PROT_READ, MAP_SHARED, &ptr, &mmap_ptrlen, NULL) == 0)
            RETURN_LONG(-1);

        ptrlen = mmap_ptrlen;
    } else {
        ptr = filename;
        ptrlen = filename_len;
        getattachtmppath(attachdir, MAXPATH, getSession());
    }
    if ((out = alloc_output(outbuf_len)) == NULL) {
        if (!is_preview) end_mmapfile(ptr, mmap_ptrlen, -1);
        RETURN_LONG(2);
    }
    /*
     override_default_output(out, buffered_output);
     override_default_flush(out, flush_buffer);
    */
    /*override_default_output(out, new_buffered_output);
    override_default_flush(out, new_flush_buffer);*/
    override_default_write(out, new_write);

    output_ansi_html(ptr, ptrlen, out, attachlink, is_tex, is_preview ? attachdir : NULL);
    free_output(out);
    if (!is_preview) end_mmapfile(ptr, mmap_ptrlen, -1);
    RETURN_STRINGL(get_output_buffer(), get_output_buffer_len(),1);
}

#ifdef NFORUM
/**
 * output without attachment modify by xw 20090919
 */
PHP_FUNCTION(bbs_printansifile_noatt)
{
    char *filename;
    int filename_len;
    long is_preview;
    char *ptr;
    off_t ptrlen = 0, mmap_ptrlen;
    const int outbuf_len = 4096;
    buffered_output_t *out;

    if (ZEND_NUM_ARGS() == 1) {
        if (zend_parse_parameters(1 TSRMLS_CC, "s", &filename, &filename_len) != SUCCESS) {
            WRONG_PARAM_COUNT;
        }
        is_preview=0;
    } else if (ZEND_NUM_ARGS() == 2) {
        if (zend_parse_parameters(2 TSRMLS_CC, "sl", &filename, &filename_len, &is_preview) != SUCCESS) {
            WRONG_PARAM_COUNT;
        }
    }
    if (!is_preview) {
        if (safe_mmapfile(filename, O_RDONLY, PROT_READ, MAP_SHARED, &ptr, &mmap_ptrlen, NULL) == 0)
            RETURN_LONG(-1);
        ptrlen = mmap_ptrlen;
    }
    if ((out = alloc_output(outbuf_len)) == NULL) {
        if (!is_preview) end_mmapfile(ptr, mmap_ptrlen, -1);
        RETURN_LONG(2);
    }
    override_default_write(out, new_write);
    output_ansi_html_noatt(ptr, ptrlen, out);
    free_output(out);
    if (!is_preview) end_mmapfile(ptr, mmap_ptrlen, -1);
    RETURN_STRINGL(get_output_buffer(), get_output_buffer_len(),1);
}


static void output_ansi_nforum(char *buf, size_t buflen,
                      buffered_output_t * output, zval *attachment)
{
    unsigned int font_style = 0;
    unsigned int ansi_state;
    unsigned int ansi_val[STRLEN];
    int ival = 0;
    size_t i;
    bool att = false;
    char *ansi_begin = NULL;
    char *ansi_end;
    size_t article_len = buflen;

    if (buf == NULL)
        return;

    STATE_ZERO(ansi_state);
    bzero(ansi_val, sizeof(ansi_val));

    for (i = 0; i < article_len; i++) {
#ifdef ENABLE_LIKE
        /*
         * 略过整个Like结构, added by windinsn
         */
        if(buf[i]==LIKE_PAD[0] && article_len-i>=LIKE_PAD_SIZE && !memcmp(buf+i, LIKE_PAD, LIKE_PAD_SIZE)) {
            i += LIKE_PAD_SIZE;
            while(i<article_len && buf[i]!='\n') {
                i += sizeof(struct like);
            }
            
            if(i>article_len) { 
                i=article_len;
                break;
            }
            continue;
        } else 
#endif
        if (buf[i] == '\0') { //assume ATTACHMENT_PAD[0] is '\0'
            if (article_len - i >= ATTACHMENT_SIZE + sizeof(int) + 2) {
                if (!memcmp(buf + i, ATTACHMENT_PAD, ATTACHMENT_SIZE)) {
                    att = true;
                    break;
                }
            }
        }
        if (STATE_ISSET(ansi_state, STATE_NEW_LINE)) {
            STATE_CLR(ansi_state, STATE_NEW_LINE);
            if (i < (buflen - 1) && !STATE_ISSET(ansi_state,STATE_TEX_SET) && (buf[i] == ':' && buf[i + 1] == ' ')) {
                STATE_SET(ansi_state, STATE_QUOTE_LINE);
                if (STATE_ISSET(ansi_state, STATE_FONT_SET))
                    BUFFERED_OUTPUT(output, "</font>", 7);
                /*
                 * set quoted line styles
                 */
                STYLE_SET(font_style, FONT_STYLE_QUOTE);
                STYLE_SET_FG(font_style, FONT_COLOR_QUOTE);
                STYLE_CLR_BG(font_style);
                print_font_style(font_style, output);
                BUFFERED_OUTPUT(output, &buf[i], 1);
                STATE_SET(ansi_state, STATE_FONT_SET);
                STATE_CLR(ansi_state, STATE_ESC_SET);
                /*
                 * clear ansi_val[] array
                 */
                bzero(ansi_val, sizeof(ansi_val));
                ival = 0;
                continue;
            } else
                STATE_CLR(ansi_state, STATE_QUOTE_LINE);
        }
        /*
        * is_tex 情况下，\[upload 优先匹配 \[ 而不是 [upload
        * is_tex 情况下应该还有一个问题是 *[\[ 等，不过暂时不管了 - atppp
        */
        if (i < (buflen - 1) && !STATE_ISSET(ansi_state,STATE_TEX_SET) && (buf[i] == 0x1b && buf[i + 1] == '[')) {
            if (STATE_ISSET(ansi_state, STATE_ESC_SET)) {
                /*
                 *[*[ or *[13;24*[ */
                size_t len;

                ansi_end = &buf[i - 1];
                len = ansi_end - ansi_begin + 1;
                print_raw_ansi(ansi_begin, len, output);
            }
            STATE_SET(ansi_state, STATE_ESC_SET);
            ansi_begin = &buf[i];
            i++;                /* skip the next '[' character */
        } else if (buf[i] == '\n') {
            if (STATE_ISSET(ansi_state, STATE_ESC_SET)) {
                /*
                 *[\n or *[13;24\n */
                size_t len;

                ansi_end = &buf[i - 1];
                len = ansi_end - ansi_begin + 1;
                print_raw_ansi(ansi_begin, len, output);
                STATE_CLR(ansi_state, STATE_ESC_SET);
            }
            if (STATE_ISSET(ansi_state, STATE_QUOTE_LINE)) {
                /*
                 * end of a quoted line
                 */
                BUFFERED_OUTPUT(output, " </font>", 8);
                STYLE_CLR(font_style, FONT_STYLE_QUOTE);
                STATE_CLR(ansi_state, STATE_FONT_SET);
            }
            if (!STATE_ISSET(ansi_state,STATE_TEX_SET)) {
                BUFFERED_OUTPUT(output, " <br /> ", 8);
           }
            STATE_CLR(ansi_state, STATE_QUOTE_LINE);
            STATE_SET(ansi_state, STATE_NEW_LINE);
        } else {
            if (STATE_ISSET(ansi_state, STATE_ESC_SET)) {
                if (buf[i] == 'm') {
                    /*
                     *[0;1;4;31m */
                    if (STATE_ISSET(ansi_state, STATE_FONT_SET)) {
                        BUFFERED_OUTPUT(output, "</font>", 7);
                        STATE_CLR(ansi_state, STATE_FONT_SET);
                    }
                    if (i < buflen - 1) {
                        generate_font_style(&font_style, ansi_val, ival + 1);
                        if (STATE_ISSET(ansi_state, STATE_QUOTE_LINE))
                            STYLE_SET(font_style, FONT_STYLE_QUOTE);
                        print_font_style(font_style, output);
                        STATE_SET(ansi_state, STATE_FONT_SET);
                        STATE_CLR(ansi_state, STATE_ESC_SET);
                        /*
                         * STYLE_ZERO(font_style);
                         */
                        /*
                         * clear ansi_val[] array
                         */
                        bzero(ansi_val, sizeof(ansi_val));
                        ival = 0;
                    }
                } else if (isalpha(buf[i])) {
                    /*
                     *[23;32H */
                    /*
                     * ignore it
                     */
                    STATE_CLR(ansi_state, STATE_ESC_SET);
                    STYLE_ZERO(font_style);
                    /*
                     * clear ansi_val[] array
                     */
                    bzero(ansi_val, sizeof(ansi_val));
                    ival = 0;
                    continue;
                } else if (buf[i] == ';') {
                    if (ival < sizeof(ansi_val) - 1) {
                        ival++; /* go to next ansi_val[] element */
                        ansi_val[ival] = 0;
                    }
                } else if (buf[i] >= '0' && buf[i] <= '9') {
                    ansi_val[ival] *= 10;
                    ansi_val[ival] += (buf[i] - '0');
                } else {
                    /*
                     *[1;32/XXXX or *[* or *[[ */
                    /*
                     * not a valid ANSI string, just output it
                     */
                    size_t len;

                    ansi_end = &buf[i];
                    len = ansi_end - ansi_begin + 1;
                    print_raw_ansi(ansi_begin, len, output);
                    STATE_CLR(ansi_state, STATE_ESC_SET);
                    /*
                     * clear ansi_val[] array
                     */
                    bzero(ansi_val, sizeof(ansi_val));
                    ival = 0;
                }

            } else
                print_raw_ansi(&buf[i], 1, output);
        }
    }
    if (STATE_ISSET(ansi_state, STATE_FONT_SET)) {
        BUFFERED_OUTPUT(output, "</font>", 7);
        STATE_CLR(ansi_state, STATE_FONT_SET);
    }

    if (att) { //attachment
        char *cur_ptr, *attachfilename, *attachptr;
        long attach_len, attach_pos, newlen;
        off_t ptrlen;
        zval *item;

        cur_ptr = buf + i;
        ptrlen = article_len - i;
        while (ptrlen > 0) {
            if (((attachfilename = checkattach(cur_ptr, ptrlen,
                                               &attach_len, &attachptr)) == NULL)) {
                break;
            }
            attach_pos = attachfilename - buf;
            newlen = attachptr - cur_ptr + attach_len;
            cur_ptr += newlen;
            ptrlen -= newlen;
            if (ptrlen < 0) break;
            MAKE_STD_ZVAL(item);
            array_init(item);
            add_assoc_string(item, "name", attachfilename, 1);
            add_assoc_long(item, "size", attach_len);
            add_assoc_long(item, "pos", attach_pos);
            add_next_index_zval(attachment, item);
        }
    }
    BUFFERED_FLUSH(output);

}

/* fancy Aug 14 2011，nForum 带颜色的正文 + 附件输出
   return array(
       'content' => 'blablabla',
       'attachment' => array(
           array('name' => 'filename.jpg', size => 12345, pos => 678),
           ...
       )
   );
*/
PHP_FUNCTION(bbs_printansifile_nforum)
{
    char *filename;
    int filename_len;
    char *ptr;
    off_t ptrlen = 0, mmap_ptrlen;
    const int outbuf_len = 4096;
    buffered_output_t *out;
    zval *attachment;

    if (ZEND_NUM_ARGS() == 1) {
        if (zend_parse_parameters(1 TSRMLS_CC, "s", &filename, &filename_len) != SUCCESS) {
            WRONG_PARAM_COUNT;
        }
    } else
        WRONG_PARAM_COUNT;
    if (safe_mmapfile(filename, O_RDONLY, PROT_READ, MAP_SHARED, &ptr, &mmap_ptrlen, NULL) == 0)
        RETURN_LONG(-1);
    ptrlen = mmap_ptrlen;
    if ((out = alloc_output(outbuf_len)) == NULL) {
        end_mmapfile(ptr, mmap_ptrlen, -1);
        RETURN_LONG(2);
    }
    override_default_write(out, new_write);
    MAKE_STD_ZVAL(attachment);
    array_init(attachment);
    output_ansi_nforum(ptr, ptrlen, out, attachment);
    free_output(out);
    end_mmapfile(ptr, mmap_ptrlen, -1);
    array_init(return_value);
    add_assoc_stringl(return_value, "content", get_output_buffer(), get_output_buffer_len(), 1);
    add_assoc_zval(return_value, "attachment", attachment);
    //RETURN_STRINGL(get_output_buffer(), get_output_buffer_len(),1);
}
#endif

PHP_FUNCTION(bbs_print_article)
{
    char *filename;
    int filename_len;
    long linkmode;
    char *ptr;
    off_t mmap_ptrlen;
    const int outbuf_len = 4096;
    buffered_output_t *out;
    char* attachlink;
    int attachlink_len;

    if (ZEND_NUM_ARGS() == 1) {
        if (zend_parse_parameters(1 TSRMLS_CC, "s", &filename, &filename_len) != SUCCESS) {
            WRONG_PARAM_COUNT;
        }
        linkmode = 1;
        attachlink=NULL;
    } else if (ZEND_NUM_ARGS() == 2) {
        if (zend_parse_parameters(2 TSRMLS_CC, "sl", &filename, &filename_len, &linkmode) != SUCCESS) {
            WRONG_PARAM_COUNT;
        }
        attachlink=NULL;
    } else {
        if (zend_parse_parameters(3 TSRMLS_CC, "sls", &filename, &filename_len, &linkmode,&attachlink,&attachlink_len) != SUCCESS) {
            WRONG_PARAM_COUNT;
        }
    }
    if (safe_mmapfile(filename, O_RDONLY, PROT_READ, MAP_SHARED, &ptr, &mmap_ptrlen, NULL) == 0)
        RETURN_LONG(-1);
    if ((out = alloc_output(outbuf_len)) == NULL) {
        end_mmapfile(ptr, mmap_ptrlen, -1);
        RETURN_LONG(2);
    }

    override_default_write(out, zend_write);

    output_ansi_text(ptr, mmap_ptrlen, out, attachlink);
    free_output(out);
    end_mmapfile(ptr, mmap_ptrlen, -1);
}

PHP_FUNCTION(bbs_print_article_js)
{
    char *filename;
    int filename_len;
    long linkmode;
    char *ptr;
    off_t mmap_ptrlen;
    const int outbuf_len = 4096;
    buffered_output_t *out;
    char* attachlink;
    int attachlink_len;

    if (ZEND_NUM_ARGS() == 1) {
        if (zend_parse_parameters(1 TSRMLS_CC, "s", &filename, &filename_len) != SUCCESS) {
            WRONG_PARAM_COUNT;
        }
        linkmode = 1;
        attachlink=NULL;
    } else if (ZEND_NUM_ARGS() == 2) {
        if (zend_parse_parameters(2 TSRMLS_CC, "sl", &filename, &filename_len, &linkmode) != SUCCESS) {
            WRONG_PARAM_COUNT;
        }
        attachlink=NULL;
    } else {
        if (zend_parse_parameters(3 TSRMLS_CC, "sls", &filename, &filename_len, &linkmode,&attachlink,&attachlink_len) != SUCCESS) {
            WRONG_PARAM_COUNT;
        }
    }
    if (safe_mmapfile(filename, O_RDONLY, PROT_READ, MAP_SHARED, &ptr, &mmap_ptrlen, NULL) == 0)
        RETURN_LONG(-1);
    if ((out = alloc_output(outbuf_len)) == NULL) {
        end_mmapfile(ptr, mmap_ptrlen, -1);
        RETURN_LONG(2);
    }

    override_default_write(out, zend_write);

    output_ansi_javascript(ptr, mmap_ptrlen, out, attachlink);
    free_output(out);
    end_mmapfile(ptr, mmap_ptrlen, -1);
}


/* function bbs_printoriginfile(string board, string filename);
 * 输出原文内容供编辑
 */
PHP_FUNCTION(bbs_printoriginfile)
{
    char *board,*filename;
    int boardLen,filenameLen;
    FILE* fp;
    const int outbuf_len = 4096;
    char buf[512],path[512];
    buffered_output_t *out;
    int i;
    int skip;
    const boardheader_t* bp;

    if ((ZEND_NUM_ARGS() != 2) || (zend_parse_parameters(2 TSRMLS_CC, "ss", &board,&boardLen, &filename,&filenameLen) != SUCCESS)) {
        WRONG_PARAM_COUNT;
    }
    if ((bp=getbcache(board))==0) {
        RETURN_LONG(-1);
    }
    setbfile(path, bp->filename, filename);
    fp = fopen(path, "r");
    if (fp == 0)
        RETURN_LONG(-1); //文件无法读取
    if ((out = alloc_output(outbuf_len)) == NULL) {
        RETURN_LONG(-2);
    }
    override_default_write(out, zend_write);
    /*override_default_output(out, buffered_output);
    override_default_flush(out, flush_buffer);*/

    i=0;
    skip=0;
    while (skip_attach_fgets(buf, sizeof(buf), fp) != 0) {
        i++;
        if (Origin2(buf))
            break;
        if ((i==1) && (strncmp(buf,"发信人",6)==0)) {
            skip=1;
        }
        if ((skip) && (i<=4)) {
            continue;
        }
        if (strstr(buf,"\033[36m※ 修改:·")==buf) {
            continue;
        }
        if (!strcasestr(buf, "</textarea>")) {
            int len = strlen(buf);
            BUFFERED_OUTPUT(out, buf, len);
        }
    }
    fclose(fp);
    BUFFERED_FLUSH(out);
    free_output(out);
    RETURN_LONG(0);
}


/* function bbs_originfile(string board, string filename);
 * 返回原文内容供编辑 modified from the function above by pig2532
 解决很猪的问题之二
 */
PHP_FUNCTION(bbs_originfile)
{
    char *board,*filename;
    int boardLen,filenameLen;
    FILE* fp;
    char buf[512],path[512];
    char *content, *ptr;
    int chunk_size=51200, calen, clen, buflen;
    int i;
    int skip;
    const boardheader_t* bp;

    if ((ZEND_NUM_ARGS() == 1) && (zend_parse_parameters(1 TSRMLS_CC, "s", &filename,&filenameLen) == SUCCESS)) {
        fp = fopen(filename, "r");
    }else if ((ZEND_NUM_ARGS() == 2) && (zend_parse_parameters(2 TSRMLS_CC, "ss", &board,&boardLen, &filename,&filenameLen) == SUCCESS)) {
        if ((bp=getbcache(board))==0) {
            RETURN_LONG(-1);
        }
        setbfile(path, bp->filename, filename);
        fp = fopen(path, "r");
    }else
        WRONG_PARAM_COUNT;
    if (fp == 0)
        RETURN_LONG(-1); //文件无法读取

    i=0;
    skip=0;
    calen = chunk_size;
    content = (char *)emalloc(calen);
    clen = 0;
    ptr = content;
    while (skip_attach_fgets(buf, sizeof(buf), fp) != 0) {
        i++;
        if (Origin2(buf))
            break;
        if ((i==1) && (strncmp(buf,"发信人",6)==0 || strncmp(buf,"寄信人",6)==0)) {
            skip=1;
        }
        if ((skip) && (i<=4)) {
            continue;
        }
        if (strstr(buf,"\033[36m※ 修改:·")==buf) {
            continue;
        }
        buflen = strlen(buf);
        if ((clen + buflen) >= calen) {
            calen += chunk_size;
            content = (char *)erealloc(content, calen);
            ptr = content + clen;
        }
        memcpy(ptr, buf, buflen);
        clen += buflen;
        ptr += buflen;
    }
    fclose(fp);
    content[clen] = '\0';
    RETURN_STRINGL(content, clen, 0);
}


PHP_FUNCTION(bbs_decode_att_hash)
{
#ifndef DISABLE_INTERNAL_BOARD_PPMM_VIEWING
    char *info;
    int infolen;
    char *spec;
    int speclen;
    int specbytes[10];
    int totalspecbytes;
    char decoded[256];
    char md5ret[256];
    uint16_t u16;
    int i, u32, len;
    char *ptr;
    MD5_CTX md5;

    if ((ZEND_NUM_ARGS() != 2) || (zend_parse_parameters(2 TSRMLS_CC, "ss", &info, &infolen, &spec, &speclen) != SUCCESS)) {
        WRONG_PARAM_COUNT;
    }

    if (infolen > 128) {
        RETURN_FALSE;
    }
    if (speclen > sizeof(specbytes)/sizeof(int)) {
        RETURN_FALSE;
    }

    totalspecbytes = 0;
    for (i = 0; i < speclen; i++) {
        u32 = spec[i] - '0';
        if (u32 != 2 && u32 != 4) {
            RETURN_FALSE;
        }
        specbytes[i] = u32;
        totalspecbytes += u32;
    }
    memcpy(decoded, info, 9);
    len = from64tobits(decoded+9, info+9);
    if (4+totalspecbytes+4 != len) {
        RETURN_FALSE;
    }

    MD5Init(&md5);
    MD5Update(&md5, (unsigned char *) decoded, 9+4+totalspecbytes);
    MD5Final((unsigned char*)md5ret, &md5);

    if (memcmp(md5ret, decoded+9+4+totalspecbytes, 4) != 0) {
        RETURN_FALSE;
    }
    ptr = decoded+9;
    memcpy(&u32, ptr, 4); ptr+=4;
    if (time(NULL) < u32 || (time(NULL) - u32 >= 300)) {
        RETURN_FALSE;
    }

    if (array_init(return_value) == FAILURE) {
        RETURN_FALSE;
    }

    add_next_index_stringl(return_value, info, 9, 1);
    for (i = 0; i < speclen; i++) {
        if (specbytes[i] == 2) {
            memcpy(&u16, ptr, 2); ptr += 2;
            u32 = u16;
        } else {
            memcpy(&u32, ptr, 4); ptr += 4;
        }
        add_next_index_long(return_value, u32);
    }
#else
    RETURN_FALSE;
#endif
}

// long bbs_parse_articles(string fname, array arr);
// mode:  0 - parse, 1 - get brief
PHP_FUNCTION(bbs_parse_article)
{
    int ac, fname_len;
    long mode;
    char *fname, line[1024], *ptr, *ptr1;
    FILE *fp;
    zval *arr;

    ac = ZEND_NUM_ARGS();
    if (ac != 3 || zend_parse_parameters(3 TSRMLS_CC, "sal", &fname, &fname_len, &arr, &mode) == FAILURE) {
        WRONG_PARAM_COUNT;
    }

    fp = fopen(fname, "r");
    if (!fp) {
        RETURN_LONG(-1);
    }
    if (array_init(arr) != SUCCESS) {
        fclose(fp);
        RETURN_LONG(-2);
    }

    if (mode == 0) {
        while (skip_attach_fgets(line, 1024, fp)) {
            if (strncmp(line, "发信人: ", 8) == 0) {
                ptr = strchr(line, '(');
                if (!ptr)
                    continue;
                *(ptr - 1) = '\0';
                ptr1 = line + 8;
                add_assoc_string(arr, "userid", ptr1, 1);
                *(ptr - 1) = ' ';
                ptr1 = strrchr(line, ')');
                if (!ptr1)
                    continue;
                *ptr1 = '\0';
                add_assoc_string(arr, "username", ptr + 1, 1);
                continue;
            } else if (strncmp(line, "标  题: ", 8) == 0) {
                ptr = strrchr(line, '\n');
                if (!ptr)
                    continue;
                *ptr = '\0';
                add_assoc_string(arr, "title", line + 8, 1);
                continue;
            } else if (strncmp(line, "发信站: ", 8) == 0) {
                ptr = strchr(line, '(');
                if (!ptr)
                    continue;
                *(ptr - 1) = '\0';
                add_assoc_string(arr, "postsite", line + 8, 1);
                continue;
            } else if (strncmp(line, "转信站: ", 8) == 0) {
                ptr = strrchr(line, '\n');
                if (!ptr)
                    continue;
                *ptr = '\0';
                add_assoc_string(arr, "innlist", line + 8, 1);
                continue;
            } else if (strncmp(line, "\n", 1) == 0) {
                break;
            }
        }
    } else if (mode == 1) {
        while (skip_attach_fgets(line, 1024, fp)) {
            if (strncmp(line, "\n", 1) == 0) {
                bzero(line, 100);
                fread(line, 50, 1, fp);
                process_control_chars(line, NULL);
                add_assoc_string(arr, "brief", line, 1);
                break;
            }
        }
    }

    fclose(fp);
    RETURN_LONG(0);
}


