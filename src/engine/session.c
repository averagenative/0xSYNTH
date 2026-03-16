/*
 * 0xSYNTH Extended Session State Implementation
 */

#include "session.h"
#include "preset.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool oxs_session_ui_save(const oxs_session_ui_t *ui, const char *path)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return false;

    cJSON_AddStringToObject(root, "version", "0.1.0");
    cJSON_AddNumberToObject(root, "theme_id", ui->theme_id);
    cJSON_AddNumberToObject(root, "window_x", ui->window_x);
    cJSON_AddNumberToObject(root, "window_y", ui->window_y);
    cJSON_AddNumberToObject(root, "window_w", ui->window_w);
    cJSON_AddNumberToObject(root, "window_h", ui->window_h);
    cJSON_AddStringToObject(root, "preset_name", ui->preset_name);
    cJSON_AddNumberToObject(root, "octave_offset", ui->octave_offset);
    cJSON_AddBoolToObject(root, "keyboard_visible", ui->keyboard_visible);

    char *json_str = cJSON_Print(root);
    cJSON_Delete(root);
    if (!json_str) return false;

    FILE *f = fopen(path, "w");
    if (!f) { free(json_str); return false; }
    fputs(json_str, f);
    fclose(f);
    free(json_str);
    return true;
}

bool oxs_session_ui_load(oxs_session_ui_t *ui, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0 || len > 64 * 1024) { fclose(f); return false; }

    char *json_str = malloc((size_t)len + 1);
    if (!json_str) { fclose(f); return false; }
    size_t read = fread(json_str, 1, (size_t)len, f);
    fclose(f);
    json_str[read] = '\0';

    cJSON *root = cJSON_Parse(json_str);
    free(json_str);
    if (!root) return false;

    cJSON *item;

    item = cJSON_GetObjectItemCaseSensitive(root, "version");
    if (item && cJSON_IsString(item))
        strncpy(ui->version, item->valuestring, sizeof(ui->version) - 1);

    item = cJSON_GetObjectItemCaseSensitive(root, "theme_id");
    if (item && cJSON_IsNumber(item)) ui->theme_id = item->valueint;

    item = cJSON_GetObjectItemCaseSensitive(root, "window_x");
    if (item && cJSON_IsNumber(item)) ui->window_x = item->valueint;

    item = cJSON_GetObjectItemCaseSensitive(root, "window_y");
    if (item && cJSON_IsNumber(item)) ui->window_y = item->valueint;

    item = cJSON_GetObjectItemCaseSensitive(root, "window_w");
    if (item && cJSON_IsNumber(item)) ui->window_w = item->valueint;

    item = cJSON_GetObjectItemCaseSensitive(root, "window_h");
    if (item && cJSON_IsNumber(item)) ui->window_h = item->valueint;

    item = cJSON_GetObjectItemCaseSensitive(root, "preset_name");
    if (item && cJSON_IsString(item))
        strncpy(ui->preset_name, item->valuestring, sizeof(ui->preset_name) - 1);

    item = cJSON_GetObjectItemCaseSensitive(root, "octave_offset");
    if (item && cJSON_IsNumber(item)) ui->octave_offset = item->valueint;

    item = cJSON_GetObjectItemCaseSensitive(root, "keyboard_visible");
    if (item) ui->keyboard_visible = cJSON_IsTrue(item);

    cJSON_Delete(root);
    return true;
}

const char *oxs_session_ui_path(void)
{
    static char path[576] = "";
    if (path[0]) return path;
    const char *user_dir = oxs_preset_user_dir();
    snprintf(path, sizeof(path), "%s/../session_ui.json", user_dir);
    return path;
}
