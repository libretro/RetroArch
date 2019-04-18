/*
Copyright (c) 2016 Raspberry Pi (Trading) Ltd.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#include <libfdt.h>

#include "dtoverlay.h"
#include "utils.h"

#define CFG_DIR_1 "/sys/kernel/config"
#define CFG_DIR_2 "/config"
#define DT_SUBDIR "/device-tree"
#define WORK_DIR "/tmp/.dtoverlays"
#define OVERLAY_SRC_SUBDIR "overlays"
#define README_FILE "README"
#define DT_OVERLAYS_SUBDIR "overlays"
#define DTOVERLAY_PATH_MAX 128
#define DIR_MODE 0755

enum {
    OPT_ADD,
    OPT_REMOVE,
    OPT_REMOVE_FROM,
    OPT_LIST,
    OPT_LIST_ALL,
    OPT_HELP
};

static const char *boot_dirs[] =
{
#ifdef FORCE_BOOT_DIR
    FORCE_BOOT_DIR,
#else
    "/boot",
    "/flash",
#ifdef OTHER_BOOT_DIR
    OTHER_BOOT_DIR,
#endif
#endif
    NULL /* Terminator */
};

typedef struct state_struct
{
    int count;
    struct dirent **namelist;
} STATE_T;

static int dtoverlay_add(STATE_T *state, const char *overlay,
                         int argc, const char **argv);
static int dtoverlay_remove(STATE_T *state, const char *overlay, int and_later);
static int dtoverlay_list(STATE_T *state);
static int dtoverlay_list_all(STATE_T *state);
static void usage(void);
static void root_check(void);

static void overlay_help(const char *overlay, const char **params);

static int apply_overlay(const char *overlay_file, const char *overlay);
static int overlay_applied(const char *overlay_dir);

static STATE_T *read_state(const char *dir);
static void free_state(STATE_T *state);

const char *cmd_name;

const char *work_dir = WORK_DIR;
const char *overlay_src_dir;
const char *dt_overlays_dir;
const char *error_file = NULL;
int dry_run = 0;

int main(int argc, const char **argv)
{
    int argn = 1;
    int opt = OPT_ADD;
    int is_dtparam;
    const char *overlay = NULL;
    const char **params = NULL;
    int ret = 0;
    STATE_T *state = NULL;
    const char *cfg_dir;

    cmd_name = argv[0];
    if (strrchr(cmd_name, '/'))
        cmd_name = strrchr(cmd_name, '/') + 1;
    is_dtparam = (strcmp(cmd_name, "dtparam") == 0);

    while ((argn < argc) && (argv[argn][0] == '-'))
    {
        const char *arg = argv[argn++];
        if (strcmp(arg, "-r") == 0)
        {
            if (opt != OPT_ADD)
                usage();
            opt = OPT_REMOVE;
        }
        else if (strcmp(arg, "-R") == 0)
        {
            if (opt != OPT_ADD)
                usage();
            opt = OPT_REMOVE_FROM;
        }
        else if (strcmp(arg, "-D") == 0)
        {
            if (opt != OPT_ADD)
                usage();
            dry_run = 1;
            work_dir = ".";
        }
        else if ((strcmp(arg, "-l") == 0) ||
                 (strcmp(arg, "--list") == 0))
        {
            if (opt != OPT_ADD)
                usage();
            opt = OPT_LIST;
        }
        else if ((strcmp(arg, "-a") == 0) ||
                 (strcmp(arg, "--listall") == 0) ||
                 (strcmp(arg, "--all") == 0))
        {
	    if (opt != OPT_ADD)
		usage();
	    opt = OPT_LIST_ALL;
	}
	else if (strcmp(arg, "-d") == 0)
	{
	    if (argn == argc)
		usage();
	    overlay_src_dir = argv[argn++];
	}
	else if (strcmp(arg, "-v") == 0)
	{
	    opt_verbose = 1;
	}
	else if (strcmp(arg, "-h") == 0)
	{
	    opt = OPT_HELP;
	}
	else
	{
	    fprintf(stderr, "* unknown option '%s'\n", arg);
	    usage();
	}
    }

    if ((opt == OPT_ADD) || (opt == OPT_REMOVE) ||
	(opt == OPT_REMOVE_FROM) || (opt == OPT_HELP))
    {
	if ((argn == argc) &&
	    ((!is_dtparam &&
	      ((opt == OPT_ADD) || (opt == OPT_HELP))) ||
	     (is_dtparam && (opt == OPT_HELP))))
	    usage();
	if (is_dtparam && (opt == OPT_ADD) && (argn == argc))
	    opt = OPT_HELP;
	if (is_dtparam &&
	    ((opt == OPT_ADD) || (opt == OPT_HELP)))
	    overlay = "dtparam";
	else if (argn < argc)
	    overlay = argv[argn++];
    }

    if ((opt == OPT_HELP) && (argn < argc))
    {
	params = &argv[argn];
	argn = argc;
    }

    if ((opt != OPT_ADD) && (argn != argc))
	usage();

    dtoverlay_enable_debug(opt_verbose);

    if (!overlay_src_dir)
    {
	/* Find the overlays and README */
	int i;

	for (i = 0; boot_dirs[i]; i++)
	{
	    overlay_src_dir = sprintf_dup("%s/" OVERLAY_SRC_SUBDIR,
					  boot_dirs[i]);
	    if (dir_exists(overlay_src_dir))
		break;
	    free_string(overlay_src_dir);
	    overlay_src_dir = NULL;
	}

	if (!overlay_src_dir)
	    fatal_error("Failed to find overlays directory");
    }

    if (opt == OPT_HELP)
    {
	overlay_help(overlay, params);
	goto orderly_exit;
    }

    if (!dir_exists(work_dir))
    {
	if (mkdir(work_dir, DIR_MODE) != 0)
	    fatal_error("Failed to create '%s' - %d", work_dir, errno);
    }

    error_file = sprintf_dup("%s/%s", work_dir, "error.dtb");

    cfg_dir = CFG_DIR_1 DT_SUBDIR;
    if (!dry_run && !dir_exists(cfg_dir))
    {
	root_check();

	cfg_dir = CFG_DIR_2;
	if (!dir_exists(cfg_dir))
	{
	    if (mkdir(cfg_dir, DIR_MODE) != 0)
		fatal_error("Failed to create '%s' - %d", cfg_dir, errno);
	}

	cfg_dir = CFG_DIR_2 DT_SUBDIR;
	if (!dir_exists(cfg_dir) &&
	    (run_cmd("mount -t configfs none '%s'", cfg_dir) != 0))
	    fatal_error("Failed to mount configfs - %d", errno);
    }

    dt_overlays_dir = sprintf_dup("%s/%s", cfg_dir, DT_OVERLAYS_SUBDIR);
    if (!dir_exists(dt_overlays_dir))
	fatal_error("configfs overlays folder not found - incompatible kernel");

    if (!dry_run)
    {
        state = read_state(work_dir);
        if (!state)
            fatal_error("Failed to read state");
    }

    switch (opt)
    {
    case OPT_ADD:
    case OPT_REMOVE:
    case OPT_REMOVE_FROM:
        if (!dry_run)
        {
            root_check();
            run_cmd("which dtoverlay-pre >/dev/null 2>&1 && dtoverlay-pre");
        }
        break;
    default:
	break;
    }

    switch (opt)
    {
    case OPT_ADD:
	ret = dtoverlay_add(state, overlay, argc - argn, argv + argn);
	break;
    case OPT_REMOVE:
	ret = dtoverlay_remove(state, overlay, 0);
	break;
    case OPT_REMOVE_FROM:
	ret = dtoverlay_remove(state, overlay, 1);
	break;
    case OPT_LIST:
	ret = dtoverlay_list(state);
	break;
    case OPT_LIST_ALL:
	ret = dtoverlay_list_all(state);
	break;
    default:
	ret = 1;
	break;
    }

    switch (opt)
    {
    case OPT_ADD:
    case OPT_REMOVE:
    case OPT_REMOVE_FROM:
	if (!dry_run)
	    run_cmd("which dtoverlay-post >/dev/null 2>&1 && dtoverlay-post");
	break;
    default:
	break;
    }

orderly_exit:
    if (state)
	free_state(state);
    free_strings();

    if ((ret == 0) && error_file)
	unlink(error_file);

    return ret;
}

struct dtparam_state
{
    STRING_VEC_T *used_props;
    const char *override_value;
};

int dtparam_callback(int override_type,
		     DTBLOB_T *dtb, int node_off,
		     const char *prop_name, int target_phandle,
		     int target_off, int target_size,
		     void *callback_value)
{
    struct dtparam_state *state = callback_value;
    char prop_id[80];
    int err;

    err = dtoverlay_override_one_target(override_type,
					dtb, node_off,
					prop_name, target_phandle,
					target_off, target_size,
					(void *)state->override_value);

    if ((err == 0) && (target_phandle != 0))
    {
	if (snprintf(prop_id, sizeof(prop_id), "%08x%s", target_phandle,
		     prop_name) < 0)
	    err = FDT_ERR_INTERNAL;
	else if (string_vec_find(state->used_props, prop_id, 0) < 0)
	    string_vec_add(state->used_props, prop_id, 0);
    }

    return err;
}

// Returns 0 on success, -ve for fatal errors and +ve for non-fatal errors
int dtparam_apply(DTBLOB_T *dtb, const char *override_name,
		  const char *override_data, int data_len,
		  const char *override_value, STRING_VEC_T *used_props)
{
    struct dtparam_state state;
    void *data;
    int err;

    state.used_props = used_props;
    state.override_value = override_value;

    /* Copy the override data in case it moves */
    data = malloc(data_len);
    if (data)
    {
	memcpy(data, override_data, data_len);
	err = dtoverlay_foreach_override_target(dtb, override_name,
						data, data_len,
						dtparam_callback,
						(void *)&state);
	free(data);
    }
    else
    {
	dtoverlay_error("out of memory");
	err = NON_FATAL(FDT_ERR_NOSPACE);
    }

    return err;
}

static int dtoverlay_add(STATE_T *state, const char *overlay,
			 int argc, const char **argv)
{
    const char *overlay_name;
    const char *overlay_file;
    char *param_string = NULL;
    int is_dtparam;
    DTBLOB_T *base_dtb = NULL;
    DTBLOB_T *overlay_dtb;
    STRING_VEC_T used_props;
    int err;
    int len;
    int i;

    len = strlen(overlay) - 5;
    is_dtparam = (strcmp(overlay, "dtparam") == 0);
    if (is_dtparam)
    {
        /* Convert /proc/device-tree to a .dtb and load it */
	overlay_file = sprintf_dup("%s/%s", work_dir, "base.dtb");
	if (run_cmd("dtc -I fs -O dtb -o '%s' /proc/device-tree 1>/dev/null 2>&1",
		    overlay_file) != 0)
           return error("Failed to read active DTB");
    }
    else if ((len > 0) && (strcmp(overlay + len, ".dtbo") == 0))
    {
	const char *p;
	overlay_file = overlay;
	p = strrchr(overlay, '/');
	if (p)
	{
	    overlay = p + 1;
	    len = strlen(overlay) - 5;
	}

	overlay = sprintf_dup("%.*s", len, overlay);
    }
    else
    {
	overlay_file = sprintf_dup("%s/%s.dtbo", overlay_src_dir, overlay);
    }

    if (dry_run)
        overlay_name = "dry_run";
    else
	overlay_name = sprintf_dup("%d_%s", state->count, overlay);
    overlay_dtb = dtoverlay_load_dtb(overlay_file, DTOVERLAY_PADDING(4096));
    if (!overlay_dtb)
	return error("Failed to read '%s'", overlay_file);

    if (is_dtparam)
    {
        base_dtb = overlay_dtb;
	string_vec_init(&used_props);
    }

    /* Apply any parameters next */
    for (i = 0; i < argc; i++)
    {
	const char *arg = argv[i];
	const char *param_val = strchr(arg, '=');
	const char *param, *override;
	char *p = NULL;
	int override_len;
	if (param_val)
	{
	    int len = (param_val - arg);
	    p = sprintf_dup("%.*s", len, arg);
	    param = p;
	    param_val++;
	}
	else
	{
	    /* Use the default parameter value - true */
	    param = arg;
	    param_val = "true";
	}

	override = dtoverlay_find_override(overlay_dtb, param, &override_len);

	if (!override)
	    return error("Unknown parameter '%s'", param);

	if (is_dtparam)
	    err = dtparam_apply(overlay_dtb, param,
				override, override_len,
				param_val, &used_props);
	else
	    err = dtoverlay_apply_override(overlay_dtb, param,
					   override, override_len,
					   param_val);
	if (err != 0)
	    return error("Failed to set %s=%s", param, param_val);

	param_string = sprintf_dup("%s %s=%s",
				   param_string ? param_string : "",
				   param, param_val);

	free_string(p);
    }

    if (is_dtparam)
    {
        /* Build an overlay DTB */
        overlay_dtb = dtoverlay_create_dtb(2048 + 256 * used_props.num_strings);

        for (i = 0; i < used_props.num_strings; i++)
        {
            int phandle, node_off, prop_len;
            const char *str, *prop_name;
            const void *prop_data;

            str = used_props.strings[i];
            sscanf(str, "%8x", &phandle);
            prop_name = str + 8;
            node_off = dtoverlay_find_phandle(base_dtb, phandle);

            prop_data = dtoverlay_get_property(base_dtb, node_off,
                                               prop_name, &prop_len);
            err = dtoverlay_create_prop_fragment(overlay_dtb, i, phandle,
                                   prop_name, prop_data, prop_len);
        }

        dtoverlay_free_dtb(base_dtb);
    }

    /* Prevent symbol clash by keeping them all private.
     * In future we could choose to expose some - perhaps using
     * a naming convention, or an "__exports__" node, at which
     * point it will no longer be necessary to explictly target
     * the /__symbols__ node with a fragment.
     */
    dtoverlay_delete_node(overlay_dtb, "/__symbols__", 0);

    if (param_string)
	dtoverlay_dtb_set_trailer(overlay_dtb, param_string,
				  strlen(param_string) + 1);

    /* Create a filename with the sequence number */
    overlay_file = sprintf_dup("%s/%s.dtbo", work_dir, overlay_name);

    /* then write the overlay to the file */
    dtoverlay_pack_dtb(overlay_dtb);
    dtoverlay_save_dtb(overlay_dtb, overlay_file);
    dtoverlay_free_dtb(overlay_dtb);

    if (!dry_run && !apply_overlay(overlay_file, overlay_name))
    {
	if (error_file)
	{
	    rename(overlay_file, error_file);
	    free_string(error_file);
	}
	return 1;
    }

    return 0;
}

static int dtoverlay_remove(STATE_T *state, const char *overlay, int and_later)
{
    const char *overlay_dir;
    const char *dir_name = NULL;
    char *end;
    int overlay_len;
    int count = state->count;
    int rmpos;
    int i;

    if (chdir(work_dir) != 0)
	fatal_error("Failed to chdir to '%s'", work_dir);

    if (overlay)
    {
	overlay_len = strlen(overlay);

	rmpos = strtoul(overlay, &end, 10);
	if (end && (*end == '\0'))
	{
	    if (rmpos >= count)
		return error("Overlay index (%d) too large", rmpos);
	    dir_name = state->namelist[rmpos]->d_name;
	}
	/* Locate the most recent reference to the overlay */
	else for (rmpos = count - 1; rmpos >= 0; rmpos--)
	{
	    const char *left, *right;
	    dir_name = state->namelist[rmpos]->d_name;
	    left = strchr(dir_name, '_');
	    if (!left)
		return error("Internal error");
	    left++;
	    right = strchr(left, '.');
	    if (!right)
		return error("Internal error");
	    if (((right - left) == overlay_len) &&
		(memcmp(overlay, left, overlay_len) == 0))
		break;
	    dir_name = NULL;
	}

	if (rmpos < 0)
	    return error("Overlay '%s' is not loaded", overlay);
    }
    else
    {
	if (!count)
	    return error("No overlays loaded");
	rmpos = and_later ? 0 : (count - 1);
	dir_name = state->namelist[rmpos]->d_name;
    }

    if (rmpos < count)
    {
	/* Unload it and all subsequent overlays in reverse order */
	for (i = count - 1; i >= rmpos; i--)
	{
	    const char *left, *right;
	    left = state->namelist[i]->d_name;
	    right = strrchr(left, '.');
	    if (!right)
		return error("Internal error");

	    overlay_dir = sprintf_dup("%s/%.*s", dt_overlays_dir,
				      right - left, left);
	    if (rmdir(overlay_dir) != 0)
		return error("Failed to remove directory '%s'", overlay_dir);

	    free_string(overlay_dir);
	}

	/* Replay the sequence, deleting files for the specified overlay,
	   and renumbering and reloading all other overlays. */
	for (i = rmpos, state->count = rmpos; i < count; i++)
	{
	    const char *left, *right;
	    const char *filename = state->namelist[i]->d_name;

	    left = strchr(filename, '_');
	    if (!left)
		return error("Internal error");
	    left++;
	    right = strchr(left, '.');
	    if (!right)
		return error("Internal error");

            if (and_later || (i == rmpos))
            {
                /* This one is being deleted */
                unlink(filename);
            }
            else
            {
                /* Keep this one - renumber and reload */
                int len = right - left;
                char *new_name = sprintf_dup("%d_%.*s", state->count,
					     len, left);
		char *new_file = sprintf_dup("%s.dtbo", new_name);
		int ret = 0;

                if ((len == 7) && (memcmp(left, "dtparam", 7) == 0))
		{
		    /* Regenerate the overlay in case multiple overlays target
                       different parts of the same property. */

		    DTBLOB_T *dtb;
		    char *params;
		    const char **paramv;
		    int paramc;
		    int j;
		    char *p;

                    /* Extract the parameters */
		    dtb = dtoverlay_load_dtb(filename, 0);
		    unlink(filename);

		    if (!dtb)
		    {
			error("Failed to re-apply dtparam");
			continue;
		    }

		    params = (char *)dtoverlay_dtb_trailer(dtb);
		    if (!params)
		    {
			error("Failed to re-apply dtparam");
			dtoverlay_free_dtb(dtb);
			continue;
		    }

		    /* Count and NUL-separate the params */
		    p = params;
		    paramc = 0;
		    while (*p)
		    {
			int paramlen;
			*(p++) = '\0';
			paramlen = strcspn(p, " ");
			paramc++;
			p += paramlen;
		    }

		    paramv = malloc((paramc + 1) * sizeof(const char *));
		    if (!paramv)
		    {
			error("out of memory re-applying dtparam");
			dtoverlay_free_dtb(dtb);
			continue;
		    }

		    for (j = 0, p = params + 1; j < paramc; j++)
		    {
			paramv[j] = p;
			p += strlen(p) + 1;
		    }
		    paramv[j] = NULL;

		    /* Create the new overlay */
		    ret = dtoverlay_add(state, "dtparam", paramc, paramv);
		    free(paramv);
		    dtoverlay_free_dtb(dtb);
		}
		else
		{
		    rename(filename, new_file);
		    ret = !apply_overlay(new_file, new_name);
		}
		if (ret != 0)
		{
		    error("Failed to re-apply dtparam");
		    continue;
		}
		state->count++;
	    }
	}
    }

    return 0;
}

static int dtoverlay_list(STATE_T *state)
{
    if (state->count == 0)
    {
	printf("No overlays loaded\n");
    }
    else
    {
	int i;
	printf("Overlays (in load order):\n");
	for (i = 0; i < state->count; i++)
	{
	    const char *name, *left, *right;
	    const char *saved_overlay;
	    DTBLOB_T *dtb;
	    name = state->namelist[i]->d_name;
	    left = strchr(name, '_');
	    if (!left)
		return error("Internal error");
	    left++;
	    right = strchr(left, '.');
	    if (!right)
		return error("Internal error");

	    saved_overlay = sprintf_dup("%s/%s", work_dir, name);
	    dtb = dtoverlay_load_dtb(saved_overlay, 0);

	    if (dtoverlay_dtb_trailer(dtb))
		printf("%d:  %.*s %.*s\n", i, (int)(right - left), left,
		       dtoverlay_dtb_trailer_len(dtb),
		       (char *)dtoverlay_dtb_trailer(dtb));
	    else
		printf("%d:  %.*s\n", i, (int)(right - left), left);

	    dtoverlay_free_dtb(dtb);
	}
    }

    return 0;
}

static int dtoverlay_list_all(STATE_T *state)
{
    int i;
    DIR *dh;
    struct dirent *de;
    STRING_VEC_T strings;

    string_vec_init(&strings);

    /* Enumerate .dtbo files in the /boot/overlays directory */
    dh = opendir(overlay_src_dir);
    while ((de = readdir(dh)) != NULL)
    {
	int len = strlen(de->d_name) - 5;
	if ((len >= 0) && strcmp(de->d_name + len, ".dtbo") == 0)
        {
	    char *str = string_vec_add(&strings, de->d_name, len + 2);
            str[len] = '\0';
            str[len + 1] = ' ';
        }
    }
    closedir(dh);

    /* Merge in active overlays, marking them */
    for (i = 0; i < state->count; i++)
    {
	const char *left, *right;
	char *str;
	int len, idx;

	left = strchr(state->namelist[i]->d_name, '_');
	if (!left)
	    return error("Internal error");
	left++;
	right = strchr(left, '.');
	if (!right)
	    return error("Internal error");

        len = right - left;
        if ((len == 7) && (memcmp(left, "dtparam", 7) == 0))
            continue;
	idx = string_vec_find(&strings, left, len);
	if (idx >= 0)
	{
	    str = strings.strings[idx];
            len = strlen(str);
	}
	else
        {
	    str = string_vec_add(&strings, left, len + 2);
            str[len] = '\0';
        }
	str[len + 1] = '*';
    }

    if (strings.num_strings == 0)
    {
	printf("No overlays found\n");
    }
    else
    {
	/* Sort */
	string_vec_sort(&strings);

	/* Display */
	printf("All overlays (* = loaded):\n");

	for (i = 0; i < strings.num_strings; i++)
	{
            const char *str = strings.strings[i];
	    printf("%c %s\n", str[strlen(str)+1], str);
	}
    }

    string_vec_uninit(&strings);

    return 0;
}

static void usage(void)
{
    printf("Usage:\n");
    if (strcmp(cmd_name, "dtparam") == 0)
    {
    printf("  %s                Display help on all parameters\n", cmd_name);
    printf("  %s <param>=<val>...\n", cmd_name);
    printf("  %*s                Add an overlay (with parameters)\n", (int)strlen(cmd_name), "");
    printf("  %s -D [<idx>]     Dry-run (prepare overlay, but don't apply -\n",
	   cmd_name);
    printf("  %*s                save it as dry-run.dtbo)\n", (int)strlen(cmd_name), "");
    printf("  %s -r [<idx>]     Remove an overlay (by index, or the last)\n", cmd_name);
    printf("  %s -R [<idx>]     Remove from an overlay (by index, or all)\n",
	   cmd_name);
    printf("  %s -l             List active overlays/dtparams\n", cmd_name);
    printf("  %s -a             List all overlays/dtparams (marking the active)\n", cmd_name);
    printf("  %s -h             Show this usage message\n", cmd_name);
    printf("  %s -h <param>...  Display help on the listed parameters\n", cmd_name);
    }
    else
    {
    printf("  %s <overlay> [<param>=<val>...]\n", cmd_name);
    printf("  %*s                Add an overlay (with parameters)\n", (int)strlen(cmd_name), "");
    printf("  %s -D [<idx>]     Dry-run (prepare overlay, but don't apply -\n",
	   cmd_name);
    printf("  %*s                save it as dry-run.dtbo)\n", (int)strlen(cmd_name), "");
    printf("  %s -r [<overlay>] Remove an overlay (by name, index or the last)\n", cmd_name);
    printf("  %s -R [<overlay>] Remove from an overlay (by name, index or all)\n",
	   cmd_name);
    printf("  %s -l             List active overlays/params\n", cmd_name);
    printf("  %s -a             List all overlays (marking the active)\n", cmd_name);
    printf("  %s -h             Show this usage message\n", cmd_name);
    printf("  %s -h <overlay>   Display help on an overlay\n", cmd_name);
    printf("  %s -h <overlay> <param>..  Or its parameters\n", cmd_name);
    printf("    where <overlay> is the name of an overlay or 'dtparam' for dtparams\n");
    }
    printf("Options applicable to most variants:\n");
    printf("    -d <dir>    Specify an alternate location for the overlays\n");
    printf("                (defaults to /boot/overlays or /flash/overlays)\n");
    printf("    -v          Verbose operation\n");
    printf("\n");
    printf("Adding or removing overlays and parameters requires root privileges.\n");

    exit(1);
}

static void root_check(void)
{
    if (getuid() != 0)
	fatal_error("Must be run as root - try 'sudo %s ...'", cmd_name);
}

static void overlay_help(const char *overlay, const char **params)
{
    OVERLAY_HELP_STATE_T *state;
    const char *readme_path = sprintf_dup("%s/%s", overlay_src_dir,
					  README_FILE);

    state = overlay_help_open(readme_path);
    free_string(readme_path);

    if (state)
    {
	if (strcmp(overlay, "dtparam") == 0)
	    overlay = "<The base DTB>";

	if (overlay_help_find(state, overlay))
	{
	    if (params && overlay_help_find_field(state, "Params"))
	    {
		int in_param = 0;

		while (1)
		{
		    const char *line = overlay_help_field_data(state);
		    if (!line)
			break;
		    if (line[0] == '\0')
			continue;
		    if (line[0] != ' ')
		    {
			/* This is a parameter name */
			int param_len = strcspn(line, " ");
			const char **p = params;
			const char **q = p;
			in_param = 0;
			while (*p)
			{
			    if ((param_len == strlen(*p)) &&
				(memcmp(line, *p, param_len) == 0))
				in_param = 1;
			    else
				*(q++) = *p;
			    p++;
			}
			*(q++) = 0;
		    }
		    if (in_param)
			printf("%s\n", line);
		}
		/* This only shows the first unknown parameter, but
		 * that is enough. */
		if (*params)
		    fatal_error("Unknown parameter '%s'", *params);
	    }
	    else
	    {
		printf("Name:   %s\n\n", overlay);
		overlay_help_print_field(state, "Info", "Info:", 8, 0);
		overlay_help_print_field(state, "Load", "Usage:", 8, 0);
		overlay_help_print_field(state, "Params", "Params:", 8, 0);
	    }
	}
	else
	{
	    fatal_error("No help found for overlay '%s'", overlay);
	}
	overlay_help_close(state);
    }
    else
    {
	fatal_error("Help file not found");
    }
}

static int apply_overlay(const char *overlay_file, const char *overlay)
{
    const char *overlay_dir = sprintf_dup("%s/%s", dt_overlays_dir, overlay);
    int ret = 0;
    if (dir_exists(overlay_dir))
    {
	error("Overlay '%s' is already loaded", overlay);
    }
    else if (mkdir(overlay_dir, DIR_MODE) == 0)
    {
	DTBLOB_T *dtb = dtoverlay_load_dtb(overlay_file, 0);
	if (!dtb)
	{
	    error("Failed to apply overlay '%s' (load)", overlay);
	}
	else
	{
	    const char *dest_file = sprintf_dup("%s/dtbo", overlay_dir);

	    /* then write the overlay to the file */
	    if (dtoverlay_save_dtb(dtb, dest_file) != 0)
		error("Failed to apply overlay '%s' (save)", overlay);
	    else if (!overlay_applied(overlay_dir))
		error("Failed to apply overlay '%s' (kernel)", overlay);
	    else
		ret = 1;

	    free_string(dest_file);
	    dtoverlay_free_dtb(dtb);
	}

	if (!ret)
		rmdir(overlay_dir);
    }
    else
    {
	error("Failed to create overlay directory");
    }

    return ret;
}

static int overlay_applied(const char *overlay_dir)
{
    char status[7] = { '\0' };
    const char *status_path = sprintf_dup("%s/status", overlay_dir);
    FILE *fp = fopen(status_path, "r");
    int bytes = 0;
    if (fp)
    {
	bytes = fread(status, 1, sizeof(status), fp);
	fclose(fp);
    }
    free_string(status_path);
    return (bytes == sizeof(status)) &&
	(memcmp(status, "applied", sizeof(status)) == 0);
}

int seq_filter(const struct dirent *de)
{
    int num;
    return (sscanf(de->d_name, "%d_", &num) == 1);
}

int seq_compare(const struct dirent **de1, const struct dirent **de2)
{
    int num1 = atoi((*de1)->d_name);
    int num2 = atoi((*de2)->d_name);
    if (num1 < num2)
    	return -1;
    else if (num1 == num2)
    	return 0;
    else
    	return 1;
}

static STATE_T *read_state(const char *dir)
{
    STATE_T *state = malloc(sizeof(STATE_T));
    int i;

    if (state)
    {
	state->count = scandir(dir, &state->namelist, seq_filter, seq_compare);

	for (i = 0; i < state->count; i++)
	{
	    int num = atoi(state->namelist[i]->d_name);
	    if (i != num)
		error("Overlay sequence error");
	}
    }
    return state;
}

static void free_state(STATE_T *state)
{
    int i;
    for (i = 0; i < state->count; i++)
    {
    	free(state->namelist[i]);
    }
    free(state->namelist);
    free(state);
}
