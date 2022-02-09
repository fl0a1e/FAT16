#include "fat16.h"
#include "options.h"

#include <stdio.h>
#include <assert.h>

static void show_help(const char *progname)
{
    printf("usage: %s [options] <mountpoint>\n\n", progname);
    printf("FileSystem Options: \n");
    printf("--name filename to store data\n");
}

static const struct fuse_opt options[] = {
        OPTION("--name=%s", filename),
        OPTION("-h", show_help),
        OPTION("--help", show_help),
        FUSE_OPT_END
};

static const struct fuse_operations operations = {
    .init = fat16_init,
    .getattr = fat16_getattr,
    .open = fat16_open,
	.opendir = fat16_opendir,
	.readdir = fat16_readdir,
    .read = fat16_read,
    .write = fat16_write,
    .flush = fat16_flush,
    .rename = fat16_rename,
    .create = fat16_create,
    .mkdir = fat16_mkdir,
    .rmdir = fat16_rmdir,
    .truncate = fat16_truncate,
    .chmod = fat16_chmod,
    .chown = fat16_chown,
    .access = fat16_access,
    .unlink = fat16_unlink,
    .release = fat16_release,
    .destroy = fat16_destroy,
};

int main(int argc, char *argv[]){

    int ret;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    if (fuse_opt_parse(&args, &g_options, options, NULL) == -1)
        return 1;

    if (g_options.show_help) {
        show_help(argv[0]);
        assert(fuse_opt_add_arg(&args, "--help") == 0);
        args.argv[0][0] = '\0';
    }

    ret = fuse_main(args.argc, args.argv, &operations, NULL);
    fuse_opt_free_args(&args);

    return ret;
}