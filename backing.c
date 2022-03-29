/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPLv2.
  See the file COPYING.
*/

/** @file
 *
 * This "filesystem" provides only a single file. The mountpoint
 * needs to be a file rather than a directory. All writes to the
 * file will be discarded, and reading the file always returns
 * \0.
 *
 * Compile with:
 *
 *     gcc -Wall null.c `pkg-config fuse3 --cflags --libs` -o null
 *
 * ## Source code ##
 * \include passthrough_fh.c
 */


#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <fuse_lowlevel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "ebs.h"

static int backing_getattr(const char *path, struct stat *stbuf,
			struct fuse_file_info *fi)
{
	(void) fi;

	if(strcmp(path, "/") != 0)
		return -ENOENT;

	stbuf->st_mode = S_IFREG | 0644;
	stbuf->st_nlink = 1;
	stbuf->st_uid = getuid();
	stbuf->st_gid = getgid();
	stbuf->st_size = (1ULL << 32); /* 4G */
  stbuf->st_blksize = (1ULL << 12); /* 4K */
	stbuf->st_blocks = 0;
	stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);

	return 0;
}

static int backing_open(const char *path, struct fuse_file_info *fi)
{
	(void) fi;

	if(strcmp(path, "/") != 0)
		return -ENOENT;

	return 0;
}

static int backing_read(const char *path, char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;

	if(strcmp(path, "/") != 0)
		return -ENOENT;

	if (offset >= (1ULL << 32))
		return 0;

  size_t res = ebs_read(buf, offset);

  if (res == -1) {
    perror("ebs_read");
    return -errno;
  }

	return res;
}

static int backing_write(const char *path, const char *buf, size_t size,
		      off_t offset, struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;

	if(strcmp(path, "/") != 0)
		return -ENOENT;

  size_t res = ebs_write(buf, offset);

  if (res == -1) {
    perror("ebs_write");
    return -errno;
  }

	return res;
}

static const struct fuse_operations null_oper = {
	.getattr	= backing_getattr,
	.open		= backing_open,
	.read		= backing_read,
	.write		= backing_write,
};

int main(int argc, char *argv[])
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct fuse_cmdline_opts opts;
	struct stat stbuf;

	if (fuse_parse_cmdline(&args, &opts) != 0)
		return 1;
	fuse_opt_free_args(&args);

	if (!opts.mountpoint) {
		fprintf(stderr, "missing mountpoint parameter\n");
		return 1;
	}

	if (stat(opts.mountpoint, &stbuf) == -1) {
		fprintf(stderr ,"failed to access mountpoint %s: %s\n",
			opts.mountpoint, strerror(errno));
		free(opts.mountpoint);
		return 1;
	}
	free(opts.mountpoint);
	if (!S_ISREG(stbuf.st_mode)) {
		fprintf(stderr, "mountpoint is not a regular file\n");
		return 1;
	}

  if (ebs_init("localhost", "18001", "localhost", "18002") != 0) {
    perror("ebs_init");
    return 1;
  }
  
	return fuse_main(argc, argv, &null_oper, NULL);
}
