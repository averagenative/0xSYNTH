/*
 * 0xSYNTH Preset Management Implementation
 */

#include "preset.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

#ifdef OXS_PLATFORM_WINDOWS
#include <direct.h>
#define mkdir_p(path) _mkdir(path)
#else
#define mkdir_p(path) mkdir(path, 0755)
#endif

/* ─── Save ───────────────────────────────────────────────────────────────── */

bool oxs_preset_save(const oxs_param_store_t *store,
                     const oxs_param_registry_t *reg,
                     const oxs_midi_cc_map_t *cc_map,
                     const char *path,
                     const char *name,
                     const char *author,
                     const char *category)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return false;

    cJSON_AddStringToObject(root, "name", name ? name : "Untitled");
    cJSON_AddStringToObject(root, "author", author ? author : "User");
    cJSON_AddStringToObject(root, "category", category ? category : "Custom");

    /* Params object — keyed by param name */
    cJSON *params = cJSON_CreateObject();
    if (!params) { cJSON_Delete(root); return false; }

    for (uint32_t i = 0; i < OXS_PARAM_COUNT; i++) {
        if (reg->info[i].name[0] == '\0') continue;
        float val = oxs_param_get(store, i);
        cJSON_AddNumberToObject(params, reg->info[i].name, (double)val);
    }
    cJSON_AddItemToObject(root, "params", params);

    /* MIDI CC map — only non-empty mappings */
    if (cc_map) {
        cJSON *cc = cJSON_CreateObject();
        bool has_mappings = false;
        for (int i = 0; i < OXS_MIDI_CC_COUNT; i++) {
            if (cc_map->param_id[i] != OXS_MIDI_CC_UNASSIGNED) {
                int32_t pid = cc_map->param_id[i];
                if (pid >= 0 && pid < OXS_PARAM_COUNT &&
                    reg->info[pid].name[0] != '\0') {
                    char cc_key[8];
                    snprintf(cc_key, sizeof(cc_key), "%d", i);
                    cJSON_AddStringToObject(cc, cc_key, reg->info[pid].name);
                    has_mappings = true;
                }
            }
        }
        if (has_mappings) {
            cJSON_AddItemToObject(root, "midi_cc_map", cc);
        } else {
            cJSON_Delete(cc);
        }
    }

    /* Write to file */
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

/* ─── Load ───────────────────────────────────────────────────────────────── */

bool oxs_preset_load(oxs_param_store_t *store,
                     const oxs_param_registry_t *reg,
                     oxs_midi_cc_map_t *cc_map,
                     const char *path)
{
    /* Read file */
    FILE *f = fopen(path, "r");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len <= 0 || len > 1024 * 1024) { /* sanity: max 1MB */
        fclose(f);
        return false;
    }

    char *json_str = malloc((size_t)len + 1);
    if (!json_str) { fclose(f); return false; }

    size_t read = fread(json_str, 1, (size_t)len, f);
    fclose(f);
    json_str[read] = '\0';

    /* Reset all params to defaults before loading —
     * ensures params not in the JSON don't carry over from previous state */
    oxs_param_store_init(store, reg);

    /* Parse */
    cJSON *root = cJSON_Parse(json_str);
    free(json_str);
    if (!root) return false;

    /* Load params */
    cJSON *params = cJSON_GetObjectItemCaseSensitive(root, "params");
    if (params && cJSON_IsObject(params)) {
        cJSON *item;
        cJSON_ArrayForEach(item, params) {
            if (!cJSON_IsNumber(item)) continue;
            int32_t id = oxs_param_id_by_name(reg, item->string);
            if (id < 0) continue;

            float val = (float)item->valuedouble;

            /* Clamp to range */
            if (val < reg->info[id].min) val = reg->info[id].min;
            if (val > reg->info[id].max) val = reg->info[id].max;

            oxs_param_set(store, (uint32_t)id, val);
        }
    }

    /* Load MIDI CC map */
    if (cc_map) {
        oxs_midi_cc_map_init(cc_map); /* clear existing */
        cJSON *cc = cJSON_GetObjectItemCaseSensitive(root, "midi_cc_map");
        if (cc && cJSON_IsObject(cc)) {
            cJSON *item;
            cJSON_ArrayForEach(item, cc) {
                if (!cJSON_IsString(item)) continue;
                int cc_num = atoi(item->string);
                if (cc_num < 0 || cc_num >= OXS_MIDI_CC_COUNT) continue;
                int32_t pid = oxs_param_id_by_name(reg, item->valuestring);
                if (pid >= 0) {
                    oxs_midi_cc_assign(cc_map, (uint8_t)cc_num, pid);
                }
            }
        }
    }

    cJSON_Delete(root);
    return true;
}

/* ─── List ───────────────────────────────────────────────────────────────── */

int oxs_preset_list(const char *directory, char **names_out, int max_names)
{
    DIR *dir = opendir(directory);
    if (!dir) return 0;

    int count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL && count < max_names) {
        const char *name = entry->d_name;
        size_t len = strlen(name);

        /* Only .json files */
        if (len < 6) continue;
        if (strcmp(name + len - 5, ".json") != 0) continue;

        /* Strip .json extension for display name */
        size_t name_len = len - 5;
        names_out[count] = malloc(name_len + 1);
        if (names_out[count]) {
            memcpy(names_out[count], name, name_len);
            names_out[count][name_len] = '\0';
            count++;
        }
    }

    closedir(dir);

    /* Simple bubble sort by name */
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (strcmp(names_out[j], names_out[j + 1]) > 0) {
                char *tmp = names_out[j];
                names_out[j] = names_out[j + 1];
                names_out[j + 1] = tmp;
            }
        }
    }

    return count;
}

/* ─── Platform Directories ───────────────────────────────────────────────── */

const char *oxs_preset_user_dir(void)
{
    static char path[512] = {0};
    if (path[0] != '\0') return path;

#ifdef OXS_PLATFORM_WINDOWS
    const char *appdata = getenv("APPDATA");
    if (appdata) {
        snprintf(path, sizeof(path), "%s\\0xSYNTH\\presets", appdata);
    } else {
        snprintf(path, sizeof(path), "presets");
    }
#elif defined(OXS_PLATFORM_MACOS)
    const char *home = getenv("HOME");
    if (home) {
        snprintf(path, sizeof(path), "%s/Library/Application Support/0xSYNTH/presets", home);
    } else {
        snprintf(path, sizeof(path), "presets");
    }
#else /* Linux */
    const char *xdg = getenv("XDG_DATA_HOME");
    if (xdg) {
        snprintf(path, sizeof(path), "%s/0xSYNTH/presets", xdg);
    } else {
        const char *home = getenv("HOME");
        if (home) {
            snprintf(path, sizeof(path), "%s/.local/share/0xSYNTH/presets", home);
        } else {
            snprintf(path, sizeof(path), "presets");
        }
    }
#endif

    /* Create directory chain */
    char tmp[512];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
            mkdir_p(tmp);
            *p = '/';
        }
    }
    mkdir_p(tmp);

    return path;
}

const char *oxs_preset_factory_dir(void)
{
    /* Relative to executable — will be overridden by install path */
    return "presets/factory";
}
