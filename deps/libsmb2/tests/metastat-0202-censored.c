// metastat.c — queue multiple SMB2 STATs in parallel on one connection
#define _GNU_SOURCE
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <smb2/smb2.h>
#include <smb2/libsmb2.h>

struct op {
    const char *path;          // relative path under share (or URL path + filename)
    struct smb2_stat_64 st;    // result buffer filled by libsmb2
    int status;                // 0 = OK from callback
};

struct cbwrap {
    struct op *op;
    int *pending;              // shared counter
};

static void stat_cb(struct smb2_context *ctx, int status, void *cmd_data, void *cb_data) {
    (void)ctx; (void)cmd_data;
    struct cbwrap *w = (struct cbwrap *)cb_data;
    w->op->status = status;
    if (w->pending) (*w->pending)--;
    free(w);
}

static int service_loop(struct smb2_context *ctx, int *pending) {
    while (*pending > 0) {
        struct pollfd pfd = { .fd = smb2_get_fd(ctx), .events = smb2_which_events(ctx) };
        int ret = poll(&pfd, 1, 1000);
        if (ret < 0) { perror("poll"); return -1; }
        if (ret == 0) continue;
        if (smb2_service(ctx, pfd.revents) < 0) {
            fprintf(stderr, "smb2_service: %s\n", smb2_get_error(ctx));
            return -1;
        }
    }
    return 0;
}

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [-p PASSWORD] smb://[domain;][user@]server/share[/base][?args] file1 [file2 ...]\n",
        prog);
}

int main(int argc, char **argv) {
    const char *password = NULL;
    int opt;
    while ((opt = getopt(argc, argv, "p:")) != -1) {
        if (opt == 'p') password = optarg;
        else { usage(argv[0]); return 2; }
    }
    if (optind >= argc || optind == argc-1) { usage(argv[0]); return 2; }

    const char *urlstr = argv[optind++];
    int nfiles = argc - optind;

    struct smb2_context *ctx = smb2_init_context();
    if (!ctx) { fprintf(stderr, "smb2_init_context failed\n"); return 1; }

    struct smb2_url *url = smb2_parse_url(ctx, urlstr);
    if (!url) { fprintf(stderr, "parse_url: %s\n", smb2_get_error(ctx)); return 1; }

    if (url->user)   smb2_set_user(ctx, url->user);
    if (url->domain) smb2_set_domain(ctx, url->domain);
    if (password)    smb2_set_password(ctx, password);
    smb2_set_version(ctx, SMB2_VERSION_0202);

    // One connection/session to the share
    if (smb2_connect_share(ctx, url->server, url->share, url->user) != 0) {
        fprintf(stderr, "connect_share: %s\n", smb2_get_error(ctx));
        return 1;
    }

    // Queue parallel STATs
    int pending = 0;
    for (int i = 0; i < nfiles; i++) {
        const char *name = argv[optind + i];
        char rel[4096];

        if (url->path && *url->path)
            snprintf(rel, sizeof rel, "%s/%s", url->path, name);
        else
            snprintf(rel, sizeof rel, "%s", name);

        struct op *op = calloc(1, sizeof *op);
        if (!op) { perror("calloc"); return 1; }
        op->path = name;

        struct cbwrap *w = malloc(sizeof *w);
        if (!w) { perror("malloc"); free(op); return 1; }
        w->op = op;
        w->pending = &pending;

        // NOTE: third arg is the OUT buffer where libsmb2 writes results
        if (smb2_stat_async(ctx, rel, &op->st, stat_cb, w) != 0) {
            fprintf(stderr, "smb2_stat_async(%s): %s\n", rel, smb2_get_error(ctx));
            free(w); free(op);
            continue;
        }
        pending++;

        // stash pointer to 'op' for later printing (re-use argv slot)
        *((struct op **)&argv[optind + i]) = op;
    }

    if (service_loop(ctx, &pending) < 0) return 1;

    // Results
    for (int i = 0; i < nfiles; i++) {
        struct op *op = *((struct op **)&argv[optind + i]);
        if (!op) continue;
        if (op->status != 0) {
            fprintf(stderr, "%s : error %d\n", op->path, op->status);
        } else {
            printf("%-40s size=%llu  mtime=%lld  isdir=%d\n",
                   op->path,
                   (unsigned long long)op->st.smb2_size,
                   (long long)op->st.smb2_mtime,
                   (op->st.smb2_type == SMB2_TYPE_DIRECTORY));
        }
        free(op);
    }

    smb2_destroy_url(url);
    smb2_destroy_context(ctx);
    return 0;
}
