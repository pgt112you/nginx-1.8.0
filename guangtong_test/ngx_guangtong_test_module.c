#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <netdb.h>



typedef struct {
    ngx_uint_t source;
    ngx_uint_t dest;
} code_map_stru;

typedef struct
{
    ngx_array_t     *code_map;
} ngx_guangtong_test_conf_t;


typedef struct {
    ngx_rbtree_t rbtree;
    ngx_rbtree_node_t sentinel;
} code_trans_rbt;


typedef struct {
    ngx_rbtree_node_t node;
    ngx_uint_t code;
} code_trans_node;



static ngx_http_output_header_filter_pt ngx_http_next_header_filter;


static char* ngx_conf_set_myconfig(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static void* ngx_guangtong_test_create_conf(ngx_conf_t *cf);

static ngx_int_t ngx_guangtong_test_init(ngx_conf_t *cf);
static ngx_int_t
ngx_guangtong_test_header_filter(ngx_http_request_t *r);



static ngx_command_t  ngx_guangtong_test_commands[] =
{
    {
        //ngx_string("transform_code"),
        //NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        //ngx_conf_set_str_slot,
        //NGX_HTTP_LOC_CONF_OFFSET,
        //offsetof(ngx_guangtong_test_conf_t, log_path),
        //NULL
        ngx_string("transform_code"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_myconfig,
        NGX_HTTP_LOC_CONF_OFFSET,
        0,
        NULL
    },

    ngx_null_command
};


static ngx_http_module_t  ngx_guangtong_test_module_ctx =
{
    NULL,                                  /* preconfiguration方法  */
    ngx_guangtong_test_init,            /* postconfiguration方法 */

    NULL,                                  /*create_main_conf 方法 */
    NULL,                                  /* init_main_conf方法 */

    NULL,                                  /* create_srv_conf方法 */
    NULL,                                  /* merge_srv_conf方法 */

    ngx_guangtong_test_create_conf,    /* create_loc_conf方法 */
    NULL                                /*merge_loc_conf方法*/
};


ngx_module_t  ngx_guangtong_test_module =
{
    NGX_MODULE_V1,
    &ngx_guangtong_test_module_ctx,     /* module context */
    ngx_guangtong_test_commands,        /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};



static ngx_int_t ngx_guangtong_test_init(ngx_conf_t *cf)
{
    //插入到头部处理方法链表的首部
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_guangtong_test_header_filter;

    return NGX_OK;
}

static ngx_int_t
ngx_guangtong_test_header_filter(ngx_http_request_t *r)
{

    printf("in here hrer here herere hrere\n");
    //如果不是返回成功，这时是不需要理会是否加前缀的，直接交由下一个过滤模块
//处理响应码200的情形
    if (r->headers_out.status == NGX_HTTP_OK)
    {
        return ngx_http_next_header_filter(r);
    }

    code_trans_rbt  *p_rbt;
    p_rbt = ngx_http_get_module_loc_conf(r, ngx_guangtong_test_module);
    ngx_uint_t sc;    // status code
    int tc;    // transform code
    sc = r->headers_out.status;
    ngx_rbtree_t *p_nrbt = &p_rbt->rbtree;
    ngx_rbtree_node_t *point = p_nrbt->root;
    while (point != p_nrbt->sentinel) {
        if (sc == point->key) {
            tc = ((code_trans_node *)point)->code;
            break;
        }
        if (sc > point->key) 
            point = point->right;
        else
            point = point->left;
    }
    if (point == p_nrbt->sentinel) {
        printf("%d not find trans code\n", (int)sc);
        return ngx_http_next_header_filter(r);
    }
    r->headers_out.status = tc;
    ngx_str_set(&r->headers_out.status_line, "200 OK");

    ngx_table_elt_t *h;
    ngx_str_t name = ngx_string("helloHeaders");
    ngx_str_t value = ngx_string("123344");
    h = ngx_list_push(&r->headers_out.headers);
    if (h != NULL) {
        h->hash = 1;
        h->key.len = name.len;
        h->key.data = name.data;
        h->value.len = value.len;
        h->value.data = value.data;
    }

    //交由下一个过滤模块继续处理
    printf("world world rowlr world world\n");
    return ngx_http_next_header_filter(r);
}



static void* ngx_guangtong_test_create_conf(ngx_conf_t *cf)
{
    printf("in guangtong_test create_conf cmd is %s\n", ((ngx_str_t *)cf->args->elts)[0].data);
    code_trans_rbt  *p_rbt;
    
    //创建存储配置项的结构体
    p_rbt = (code_trans_rbt  *)ngx_pcalloc(cf->pool, sizeof(code_trans_rbt));
    if (p_rbt == NULL)
    {
        return NULL;
    }
    ngx_rbtree_node_t *p_sentinel = ngx_pcalloc(cf->pool, sizeof(ngx_rbtree_node_t));
    ngx_rbtree_init(&p_rbt->rbtree, p_sentinel, ngx_rbtree_insert_value);
    
    return p_rbt;
}



static char* ngx_conf_set_myconfig(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    printf("in cmd set %s\n", ((ngx_str_t *)cf->args->elts)[0].data);
    code_trans_rbt  *p_rbt = conf;

    // cf->args是1个ngx_array_t队列，它的成员都是ngx_str_t结构。
    //我们用value指向ngx_array_t的elts内容，其中value[1]就是第1
    //个参数，同理value[2]是第2个参数
    ngx_str_t* value = cf->args->elts;

    //ngx_array_t的nelts表示参数的个数
    if (ngx_strcmp(value[0].data, "transform_code") != 0) {
        //直接赋值即可， ngx_str_t结构只是指针的传递
        return NGX_CONF_ERROR;
    }
    const char *filename = (const char *)value[1].data;
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("%s %s can not open path %s, error %s\n",value[0].data, value[1].data, filename, strerror(errno));
        return NGX_CONF_ERROR;
    }
    int result;
    do {
        int sour_code, dest_code;
        result = fscanf(fp, "%d %d\n", &sour_code, &dest_code);
        if (result != EOF) {
            code_trans_node *p_node = ngx_pcalloc(cf->pool, sizeof(code_trans_rbt));
            p_node->node.key = sour_code;
            p_node->code = dest_code;
            ngx_rbtree_insert(&p_rbt->rbtree, &(p_node->node));
        }
    } while (result != EOF);
    fclose(fp);

    ////////// for test ////////////////////
    pid_t pid = getpid();
    char filename1[128];
    bzero(filename1, 128);
    sprintf(filename1, "/tmp/%d_file.txt", pid);
    printf("file name is %s\n", filename1);
    fp = fopen((const char *)filename1, "a");
    char *hw = "hello world\n";
    fwrite(hw, 1, strlen(hw), fp);
    fclose(fp);
    ////////// end for test ///////////////////

    //返回成功
    return NGX_CONF_OK;
}

