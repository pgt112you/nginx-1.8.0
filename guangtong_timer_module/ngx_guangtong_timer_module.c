#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <netdb.h>
#include "hiredis.h"


typedef struct {
    ngx_rbtree_t rbtree;
    ngx_rbtree_node_t sentinel;
} task_rbt;

typedef struct {
    ngx_rbtree_node_t node;
    ngx_str_t task;
} task_node_t;

typedef struct
{
    time_t my_sec; 
    ngx_rbtree_t *t_rbt;
    //ngx_str_node_t *sentinel;
    ngx_rbtree_node_t *sentinel;
} ngx_guangtong_timer_conf_t;


static ngx_event_t ev;
static ngx_cycle_t *cycle1;

static void *ngx_guangtong_test_create_conf(ngx_conf_t *cf);
static char *ngx_guangtong_test_merge_conf(ngx_conf_t *cf, void *parent, void *child);

static ngx_int_t ngx_guangtong_deny_init(ngx_conf_t *cf);
static ngx_int_t ngx_guangtong_test_header_filter(ngx_http_request_t *r);

static ngx_int_t init_process(ngx_cycle_t *cycle);

static void refresh_rbt(ngx_rbtree_t *t_rbt, ngx_rbtree_node_t *sentinel);
static void del_str_rbtree_node(ngx_str_node_t *delnode, ngx_rbtree_node_t *sentinel, ngx_str_node_t *root);



static ngx_command_t  ngx_guangtong_timer_commands[] =
{
    {
        ngx_string("guangtong_timer"),
        NGX_HTTP_SRV_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_sec_slot,
        NGX_HTTP_SRV_CONF_OFFSET,
        offsetof(ngx_guangtong_timer_conf_t, my_sec),
        NULL
    },

    ngx_null_command
};


static ngx_http_module_t  ngx_guangtong_timer_module_ctx =
{
    NULL,                                  /* preconfiguration方法  */
    ngx_guangtong_deny_init,            /* postconfiguration方法 */

    NULL,                                  /*create_main_conf 方法 */
    NULL,                                  /* init_main_conf方法 */

    ngx_guangtong_test_create_conf,        /* create_srv_conf方法 */
    // 因为这里是server的方法，如果不设置merge_srv_conf的话，默认会调用父模块的，而父模块在create_srv_conf里面设置了时间值为-1，所以必须要设置这个merge_srv_conf来得到正确的值
    ngx_guangtong_test_merge_conf,         /* merge_srv_conf方法 */  

    NULL,                               /* create_loc_conf方法 */
    NULL                                /*merge_loc_conf方法*/
};


ngx_module_t  ngx_guangtong_timer_module =
{
    NGX_MODULE_V1,
    &ngx_guangtong_timer_module_ctx,     /* module context */
    ngx_guangtong_timer_commands,        /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    init_process,                          /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static void del_str_rbtree_node(ngx_str_node_t *delnode, ngx_rbtree_node_t *sentinel, ngx_str_node_t *root) {
    if ((delnode->node.left != sentinel) && (delnode->node.left != NULL)) {
        del_str_rbtree_node((ngx_str_node_t *)delnode->node.left, sentinel, root);
        delnode->node.left = sentinel;
    }
    if ((delnode->node.right != sentinel) && (delnode->node.right != NULL)) {
        del_str_rbtree_node((ngx_str_node_t *)delnode->node.right, sentinel, root);
        delnode->node.right = sentinel;
    }
    if ((&(delnode->node) != sentinel) && (delnode != root)) {
        ngx_pfree(cycle1->pool, delnode->str.data);
        ngx_str_null(&delnode->str);
        ngx_pfree(cycle1->pool, delnode);
    }
    
}

static void refresh_rbt(ngx_rbtree_t *t_rbt, ngx_rbtree_node_t *sentinel) {
    redisContext *robj = redisConnect("127.0.0.1", 6379);
    if (robj == NULL || robj->err) {
        if (robj) {
            printf("Error: %s\n", robj->errstr);
        } else {
            printf("Can't allocate redis context\n");
        }
    }    
    redisReply *reply = redisCommand(robj, "SMEMBERS ngx_set");
    if (reply == NULL) {
        printf("get NGX_set error!!!\n");
    }

    // 判断如果redis返回数据有值，就把红黑树中所有数据都删除
    if (reply->type == REDIS_REPLY_ARRAY) {
        if (reply->elements > 0) {
            // 删除红黑树中所有的节点
            del_str_rbtree_node((ngx_str_node_t *)t_rbt->root, t_rbt->sentinel, (ngx_str_node_t *)t_rbt->root);
        }
        // 向红黑树中添加节点
        unsigned int j;
        for (j = 0; j < reply->elements; j++) {
            ngx_str_node_t *newnode = (ngx_str_node_t *)ngx_pcalloc(cycle1->pool, sizeof(ngx_str_node_t));
            newnode->node.key = 0;
            newnode->node.left = t_rbt->sentinel;
            newnode->node.right = t_rbt->sentinel;
            newnode->node.parent = t_rbt->sentinel;
            newnode->str.data = (u_char *)ngx_pcalloc(cycle1->pool, 32);
            size_t len = strlen(reply->element[j]->str);
            ngx_memcpy(newnode->str.data, reply->element[j]->str, len); 
            newnode->str.len = len;
            printf("-----> redis data is %s\n", newnode->str.data);
            ngx_str_rbtree_insert_value(t_rbt->root, (ngx_rbtree_node_t *)newnode, t_rbt->sentinel);
        }
        printf("%d t_rbt addr is %p\n", getpid(), t_rbt);
        printf("%d root is %p\n", getpid(), t_rbt->root);
        printf("left is %p\n", t_rbt->root->left);
        printf("right is %p\n", t_rbt->root->right);
    }

    freeReplyObject(reply);
    redisFree(robj);
}


static void ngx_http_hello_print(ngx_event_t *p_ev) {
    ngx_guangtong_timer_conf_t *ccf = (ngx_guangtong_timer_conf_t *)p_ev->data;
    ngx_add_timer(p_ev, ccf->my_sec);
    //refresh_rbt(ccf->t_rbt, (ngx_rbtree_node_t *)ccf->sentinel);
    refresh_rbt(ccf->t_rbt, ccf->sentinel);
}

static ngx_int_t init_process(ngx_cycle_t *cycle) {
   ngx_guangtong_timer_conf_t *p_conf = (ngx_guangtong_timer_conf_t *)(((ngx_http_conf_ctx_t*)cycle->conf_ctx[ngx_http_module.index])->srv_conf[ngx_guangtong_timer_module.ctx_index]);
   //ngx_guangtong_timer_conf_t *p_conf = ngx_http_cycle_get_module_main_conf(cycle, ngx_guangtong_timer_module);
   printf("XXXXXXXXXXXXXXXXXXXX ADDR IS %p\n", p_conf);
   cycle1 = cycle;
   ev.handler = ngx_http_hello_print;  
   ev.data = p_conf;
   ev.log = cycle->log;  
   ngx_add_timer(&ev, p_conf->my_sec);
   //refresh_rbt(p_conf->t_rbt, (ngx_rbtree_node_t *)p_conf->sentinel);
   printf("in here t_rbt addr is %p\n", p_conf->t_rbt);
   printf("p_conf addr is %p\n", p_conf);
   refresh_rbt(p_conf->t_rbt, p_conf->sentinel);
   return NGX_OK;
}

static ngx_int_t ngx_guangtong_deny_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt *h;
    ngx_http_core_main_conf_t *cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
    h = ngx_array_push(&cmcf->phases[NGX_HTTP_POST_READ_PHASE].handlers);
    //h = ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }
    *h = ngx_guangtong_test_header_filter;
    return NGX_OK;
}

static ngx_int_t ngx_guangtong_test_header_filter(ngx_http_request_t *r)
{
    //ngx_guangtong_timer_conf_t *cmcf = ngx_http_get_module_srv_conf(r, ngx_guangtong_timer_module);
    ngx_guangtong_timer_conf_t *cmcf = (ngx_guangtong_timer_conf_t *)(((ngx_http_conf_ctx_t*)cycle1->conf_ctx[ngx_http_module.index])->srv_conf[ngx_guangtong_timer_module.ctx_index]);
    printf("+++++++++++++addr is %p\n", cmcf);
    ngx_str_node_t dest_node;
    dest_node.node.key = 0;
    ngx_int_t res = ngx_http_arg(r, (u_char *)"taskid", 6, &dest_node.str);
    printf("-- taskid is %s, len is %d\n", dest_node.str.data, (int)dest_node.str.len);
    if (res == NGX_DECLINED) {
        printf("get taskid failed\n");
        return NGX_DECLINED;
    }
    if (ngx_str_rbtree_lookup(cmcf->t_rbt, &dest_node.str, 0) == NULL) {
        printf("taskid is not deny\n");
        return NGX_DECLINED;
    }
    //return NGX_ERROR;
    printf("lllllllllllll return from ngx_guangtong_test_header_filter\n");
    return 801;
    //return 39;
}



static void *ngx_guangtong_test_create_conf(ngx_conf_t *cf)
{
    ngx_guangtong_timer_conf_t *p_conf_t;
    
    //创建存储配置项的结构体
    p_conf_t = (ngx_guangtong_timer_conf_t *)ngx_pcalloc(cf->pool, sizeof(ngx_guangtong_timer_conf_t));
    if (p_conf_t == NULL)
    {
        return NULL;
    }
    p_conf_t->my_sec = NGX_CONF_UNSET;

    p_conf_t->t_rbt = (ngx_rbtree_t *)ngx_pcalloc(cf->pool, sizeof(ngx_rbtree_t));
    p_conf_t->sentinel = NULL;
    ngx_str_node_t *proot = (ngx_str_node_t *)ngx_pcalloc(cf->pool, sizeof(ngx_str_node_t));
    proot->node.key = 0;
    proot->node.left = p_conf_t->sentinel;
    proot->node.right = p_conf_t->sentinel;
    proot->node.parent = p_conf_t->sentinel;
    ngx_str_null(&proot->str);
    p_conf_t->t_rbt->sentinel = p_conf_t->sentinel;
    p_conf_t->t_rbt->root = (ngx_rbtree_node_t *)proot;
    p_conf_t->t_rbt->insert = ngx_str_rbtree_insert_value;
    /*
    p_conf_t->sentinel = (ngx_rbtree_node_t *)ngx_pcalloc(cf->pool, sizeof(ngx_rbtree_node_t));
    ngx_str_null(&(p_conf_t->sentinel->str));
    p_conf_t->sentinel->key = 0;
    p_conf_t->sentinel->left = p_conf_t->sentinel;
    p_conf_t->sentinel->right = p_conf_t->sentinel;
    p_conf_t->sentinel->parent = p_conf_t->sentinel;
    p_conf_t->t_rbt->sentinel = p_conf_t->sentinel;
    ngx_rbtree_init(p_conf_t->t_rbt, p_conf_t->sentinel, ngx_str_rbtree_insert_value);
    printf(">>>>>>>===========>>>>>>>>>>> root is %p\n", p_conf_t->t_rbt->root);
    */
    return p_conf_t;
}

// 因为这是一个server级别的配置，而且在create_srv_conf里面设置了默认值，所以main的配置里面有这个配置，并且值是默认值。这样如果不进行merg的话，会用main里面的那个默认值来代替我们用set方法设置的值
static char *ngx_guangtong_test_merge_conf(ngx_conf_t *cf, void *parent, void *child) {
    ngx_guangtong_timer_conf_t *prev = (ngx_guangtong_timer_conf_t *)parent;
    ngx_guangtong_timer_conf_t *conf = (ngx_guangtong_timer_conf_t *)child;
    ngx_conf_merge_sec_value(prev->my_sec, conf->my_sec, 10000);
    return NGX_CONF_OK;
}


