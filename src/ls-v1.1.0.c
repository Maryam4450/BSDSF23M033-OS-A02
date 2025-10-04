/*
 * Programming Assignment 02: ls-v1.1.0
 * Version 1.1.0 — Long Listing Format
 * Features: Uses stat(), getpwuid(), getgrgid(), ctime(), and permission formatting
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>

extern int errno;

void do_ls(const char *dir);
void do_ls_long(const char *dir);
void print_long_format(const char *path, const char *filename);
void print_permissions(mode_t mode);

int main(int argc, char *argv[])
{
    int opt;
    int long_format = 0;

    // Parse command line options
    while ((opt = getopt(argc, argv, "l")) != -1)
    {
        switch (opt)
        {
        case 'l':
            long_format = 1;
            break;
        default:
            fprintf(stderr, "Usage: %s [-l] [directory...]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    // If no directory specified, default to current
    if (optind == argc)
    {
        if (long_format)
            do_ls_long(".");
        else
            do_ls(".");
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            printf("Directory listing of %s:\n", argv[i]);
            if (long_format)
                do_ls_long(argv[i]);
            else
                do_ls(argv[i]);
            puts("");
        }
    }

    return 0;
}

void do_ls(const char *dir)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory: %s\n", dir);
        return;
    }

    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        printf("%s\n", entry->d_name);
    }

    if (errno != 0)
        perror("readdir failed");

    closedir(dp);
}

void do_ls_long(const char *dir)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory: %s\n", dir);
        return;
    }

    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
        print_long_format(path, entry->d_name);
    }

    if (errno != 0)
        perror("readdir failed");

    closedir(dp);
}

void print_long_format(const char *path, const char *filename)
{
    struct stat st;
    if (lstat(path, &st) == -1)
    {
        perror("lstat");
        return;
    }

    print_permissions(st.st_mode);
    printf(" %3ld", (long)st.st_nlink);

    struct passwd *pw = getpwuid(st.st_uid);
    struct group *gr = getgrgid(st.st_gid);
    printf(" %-8s %-8s", pw ? pw->pw_name : "unknown", gr ? gr->gr_name : "unknown");

    printf(" %8ld", (long)st.st_size);

    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", localtime(&st.st_mtime));
    printf(" %s", timebuf);

    printf(" %s\n", filename);
}

void print_permissions(mode_t mode)
{
    char perms[11];
    perms[0] = S_ISDIR(mode) ? 'd' :
               S_ISLNK(mode) ? 'l' :
               S_ISCHR(mode) ? 'c' :
               S_ISBLK(mode) ? 'b' :
               S_ISSOCK(mode) ? 's' :
               S_ISFIFO(mode) ? 'p' : '-';

    perms[1] = (mode & S_IRUSR) ? 'r' : '-';
    perms[2] = (mode & S_IWUSR) ? 'w' : '-';
    perms[3] = (mode & S_IXUSR) ? 'x' : '-';
    perms[4] = (mode & S_IRGRP) ? 'r' : '-';
    perms[5] = (mode & S_IWGRP) ? 'w' : '-';
    perms[6] = (mode & S_IXGRP) ? 'x' : '-';
    perms[7] = (mode & S_IROTH) ? 'r' : '-';
    perms[8] = (mode & S_IWOTH) ? 'w' : '-';
    perms[9] = (mode & S_IXOTH) ? 'x' : '-';
    perms[10] = '\0';

    printf("%s", perms);
}

