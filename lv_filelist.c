/**
 * @file lv_filelist.c
 *
 */


/*********************
 *      INCLUDES
 *********************/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "lv_filelist.h"
#if USE_LV_FILELIST != 0

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lv_filelist_rel_action(lv_obj_t * obj, lv_event_t event);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_signal_cb_t ancestor_signal;
static lv_design_cb_t ancestor_design;

/**********************
 *      MACROS
 **********************/
#define get_browser_prefix()  "/usr/bin/dat"
const char *get_next_full_path(const char *name, int load);
/**********************
 *   GLOBAL FUNCTIONS
 **********************/
#define lv_mem_assert(n) (void *)(n)
/**
 * Create a filelist object
 * @param par pointer to an object, it will be the parent of the new filelist
 * @param copy pointer to a filelist object, if not NULL then the new object will be copied from it
 * @return pointer to the created filelist
 */
lv_obj_t * lv_filelist_create(lv_obj_t * par, const lv_obj_t * copy, lv_filelist_pf pf)
{
    static lv_style_t style_bg;

    LV_LOG_TRACE("filelist create started");

    /*Create the ancestor of filelist*/
    lv_obj_t * new_filelist = lv_list_create(par, copy);
    lv_mem_assert(new_filelist);
    if(new_filelist == NULL) return NULL;

    lv_obj_set_size(new_filelist, LV_HOR_RES_MAX, LV_VER_RES_MAX - 60);

    lv_style_set_bg_opa(&style_bg, LV_STATE_DEFAULT, LV_OPA_100);
    lv_style_set_border_width(&style_bg, LV_STATE_DEFAULT, 0);
    lv_style_set_border_opa(&style_bg, LV_STATE_DEFAULT, LV_OPA_0);
    lv_obj_add_style(new_filelist, LV_BTN_PART_MAIN, &style_bg);

    lv_obj_align(new_filelist, par, LV_ALIGN_CENTER, 0, 60);

    /*Allocate the filelist type specific extended data*/
    lv_filelist_ext_t * ext = lv_obj_allocate_ext_attr(new_filelist, sizeof(lv_filelist_ext_t));
    lv_mem_assert(ext);
    if(ext == NULL) return NULL;
    if(ancestor_signal == NULL) ancestor_signal = lv_obj_get_signal_cb(new_filelist);
    if(ancestor_design == NULL) ancestor_design = lv_obj_get_design_cb(new_filelist);

    /*Initialize the allocated 'ext' */
    memset(ext->current_path, 0, PATH_MAX); /*to be on the safe side*/
    ext->file_view_pf = pf;
    /*Init the new filelist object*/
    if(copy == NULL) {
        //strncpy(ext->current_path, LV_FILELIST_DEFAULT_PATH, PATH_MAX-1);
        strncpy(ext->current_path, get_next_full_path(NULL, 1), PATH_MAX-1);
    }
    /*Copy an existing filelist*/
    else {
        lv_filelist_ext_t * copy_ext = lv_obj_get_ext_attr(copy);

        memcpy(ext->current_path, copy_ext->current_path, PATH_MAX);
        /*Refresh the style with new signal function*/
        //DEBUG lv_obj_refresh_style(new_filelist);
    }

    lv_filelist_update_list(new_filelist);
    LV_LOG_INFO("filelist created");

    return new_filelist;
}

/*=====================
 * Setter functions
 *====================*/


/**
 * Set a style of a filelist.
 * @param filelist pointer to filelist object
 * @param type which style should be set
 * @param style pointer to a style
 */
void lv_filelist_set_style(lv_obj_t * filelist, lv_list_style_t type, lv_style_t * style)
{
    //lv_list_set_style(filelist, type, style);
}

/*=====================
 * Getter functions
 *====================*/

/**
 * Get style of a filelist.
 * @param filelist pointer to filelist object
 * @param type which style should be get
 * @return style pointer to the style
 */
lv_style_t * lv_filelist_get_style(const lv_obj_t * filelist, lv_list_style_t type)
{
    //return lv_list_get_style(filelist, type);
    return NULL;
}

static int lv_filelist_filter(const struct dirent * entry)
{
    size_t len;
    if(!strcmp(entry->d_name, "."))
        return 0;
    if(!strcmp(entry->d_name, ".git"))
        return 0;
    if(entry->d_type == DT_DIR)
        return 1;
    len = strlen(entry->d_name);
    if (len >= 4) {
        if((entry->d_name[len - 4] == '.'
                && entry->d_name[len - 3] == 'C'
                && entry->d_name[len - 2] == 'S'
                && entry->d_name[len - 1] == 'V')) {
            return 1;
        }
        /*if((entry->d_name[len - 4] == '.'
                && entry->d_name[len - 3] == 'p'
                && entry->d_name[len - 2] == 'n'
                && entry->d_name[len - 1] == 'g')) {
            return 1;
        }*/
    }

    return 0;
}

lv_res_t lv_filelist_update_list(lv_obj_t *filelist)
{
    lv_filelist_ext_t * ext = lv_obj_get_ext_attr(filelist);
    struct stat st;
    struct dirent **entries;
    char *tmp_buf;
    const char *symbol;
    lv_obj_t * list_btn;
    int n = 0;

    /*Remove existing items from the list*/
    lv_list_clean(filelist);

    tmp_buf = lv_mem_alloc(PATH_MAX + 1);
    lv_mem_assert(tmp_buf);
    getcwd(tmp_buf, PATH_MAX);

    n = scandir(ext->current_path, &entries, lv_filelist_filter, alphasort);
    /* Keep reading entries from the directory */
    for(int i = 0; i < n; i++) {
        struct dirent * entry = entries[i];
        chdir(ext->current_path);
        stat(entry->d_name, &st);
        if(S_ISDIR(st.st_mode))
            symbol = LV_SYMBOL_DIRECTORY;
        else
            symbol = LV_SYMBOL_FILE;
        if(!strcmp(entry->d_name, "..")) {
            if(!strcmp(ext->current_path, "/")) {
                free(entry);
                continue;
            }
            if(!strcmp(ext->current_path, get_browser_prefix())) {
                free(entry);
                continue;
            }
            strcpy(entry->d_name, "UpFolder");
            symbol = LV_SYMBOL_UP;
        }
        list_btn = lv_list_add_btn(filelist, symbol, entry->d_name);
        lv_obj_set_event_cb(list_btn, lv_filelist_rel_action);
        free(entry);
    }
    free(entries);
    chdir(tmp_buf);
    lv_mem_free(tmp_buf);
    return LV_RES_OK;
}

#include <libgen.h>
const char *get_next_full_path(const char *name, int load)
{
    static char path[PATH_MAX];
    const char *base_name = NULL;
    const char *dir_name = NULL;

    if(load) {
        snprintf(path,PATH_MAX,"%s",get_browser_prefix());
    }

    if(NULL == name) {
        return path;
    }

    if(0 == strcmp(name, "..")) {
        dir_name  = dirname(path);
        strncpy(path, dir_name, PATH_MAX);
        return path;
    }
    strcat(path, "/");
    strcat(path, name);

    return path;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void lv_filelist_rel_action(lv_obj_t * listItem, lv_event_t event)
{
    if(event == LV_EVENT_CLICKED) {
        printf("Clicked: %s\n", lv_list_get_btn_text(listItem));
        lv_obj_t * fileList = lv_obj_get_parent(lv_obj_get_parent(listItem));
        const char * name = lv_list_get_btn_text(listItem);
        const char * symbol = (const char *) lv_img_get_src(lv_list_get_btn_img(listItem));
        lv_filelist_ext_t * ext = lv_obj_get_ext_attr(fileList);

        if(!strcmp(symbol, LV_SYMBOL_UP) && !strcmp(name, "UpFolder"))
            name = "..";

        if(0 == strcmp(symbol, LV_SYMBOL_DIRECTORY)
                ||(0 == strcmp(symbol, LV_SYMBOL_UP))) {
            strncpy(ext->current_path, get_next_full_path(name, 0), PATH_MAX);
            lv_filelist_update_list(fileList);
        } else if(!strcmp(symbol, LV_SYMBOL_FILE)) {
            printf("wufeng: name=%s path=%s\n",\
                   name, get_next_full_path(NULL, 0));
            ext->file_view_pf(lv_obj_get_parent(listItem), get_next_full_path(NULL, 0), name);
        } else {
            printf("wufeng: not a dir or file\n");
        }
    }
}

#endif
