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


static ngx_http_module_t  ngx_transcode_module_ctx =
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


ngx_module_t  ngx_transcode_module =
{
    NGX_MODULE_V1,
    &ngx_transcode_module_ctx,     /* module context */
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

    //如果不是返回成功，这时是不需要理会是否加前缀的，直接交由下一个过滤模块
//处理响应码200的情形
    if (r->headers_out.status == NGX_HTTP_OK)
    {
        return ngx_http_next_header_filter(r);
    }

    ngx_guangtong_test_conf_t  *mycf;
    mycf = ngx_http_get_module_loc_conf(r, ngx_transcode_module);
    code_map_stru *node;
    code_map_stru *array = mycf->code_map->elts;
    ngx_uint_t seq = 0;
    for (; seq < mycf->code_map->nelts; ++seq) { 
        node = array + seq; 
        if (node->source == r->headers_out.status) {
            r->headers_out.status = node->dest;
            ngx_str_set(&r->headers_out.status_line, "200 OK");
            break;
        }
        printf("in 1111 source code is %d, dest code is %d\n", (int)node->source, (int)node->dest); 
    }

    ngx_table_elt_t *h;
    ngx_str_t name = ngx_string("helloHeaders");
    ngx_str_t value = ngx_string("123344");
    h = ngx_list_push(&r->headers_out.headers);
    if (h != NULL) {
        h->hash = 1;
        h-> key.len = name.len;
        h-> key.data = name.data;
        h-> value.len = value.len;
        h-> value.data = value.data;
    }

    //交由下一个过滤模块继续处理
    return ngx_http_next_header_filter(r);
}



static void* ngx_guangtong_test_create_conf(ngx_conf_t *cf)
{
    printf("kaka\n");
    ngx_guangtong_test_conf_t  *mycf;

    //创建存储配置项的结构体
    mycf = (ngx_guangtong_test_conf_t  *)ngx_pcalloc(cf->pool, sizeof(ngx_guangtong_test_conf_t));
    if (mycf == NULL)
    {
        return NULL;
    }
    mycf->code_map = NULL;

    
    return mycf;
}



static char* ngx_conf_set_myconfig(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_guangtong_test_conf_t  *mycf = conf;

    // cf->args是1个ngx_array_t队列，它的成员都是ngx_str_t结构。
    //我们用value指向ngx_array_t的elts内容，其中value[1]就是第1
    //个参数，同理value[2]是第2个参数
    ngx_str_t* value = cf->args->elts;

    //ngx_array_t的nelts表示参数的个数
    if (cf->args->nelts <= 0)
    {
        //直接赋值即可， ngx_str_t结构只是指针的传递
        return NGX_CONF_ERROR;
    }


    if (mycf->code_map == NULL) {
        mycf->code_map = ngx_array_create(cf->pool, 4, sizeof(code_map_stru));
        if (mycf->code_map == NULL) {
            return NGX_CONF_ERROR;
        }
    }
    //获取存储配置项的ngx_guangtong_test_conf_t结构体
    printf("======log_path is %s\n", value[1].data);
    FILE *fp = fopen((const char *)value[1].data, "r");
    if (fp == NULL) {
        printf("can not open path %s error %s\n", value[1].data, strerror(errno));
        return "can not open path";
    }
    //int sour_code, dest_code;
    int result;
    code_map_stru *kv;
    do {
        //int *sour_code = (char *)ngx_palloc(cf->pool, sizeof(ngx_unit_t));
        //int *dest_code = (char *)ngx_palloc(cf->pool, sizeof(ngx_uint_t));
        int sour_code, dest_code;
        result = fscanf(fp, "%d %d\n", &sour_code, &dest_code);
        if (result != EOF) {
            printf("complete code %d, %d\n", sour_code, dest_code);
            kv = ngx_array_push(mycf->code_map);
            if (kv == NULL) {
                return NGX_CONF_ERROR;
            }
            kv->source = sour_code;
            kv->dest = dest_code;
            printf("k is %d, value is %d\n", (int)kv->source, (int)kv->dest);
            //kv->key = ngx_string(sour_code_s);
            //kv->value = ngx_string(dest_code_s);
            //kv->key = {sizeof(sour_code_s)-1, (u_char *)sour_code_s };
            //kv->value = {sizeof(dest_code_s)-1, (u_char *)dest_code_s }
        }
    } while (result != EOF);
    fclose(fp);

    //返回成功
    return NGX_CONF_OK;
}

