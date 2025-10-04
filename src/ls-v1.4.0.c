/*
 * Programming Assignment 02: ls-v1.4.0
 * Version 1.4.0 — Alphabetical Sort
 * Features: alphabetically sorted output, integrates with existing display modes
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
#include <sys/ioctl.h>
#include <fcntl.h>

extern int errno;

/* Function declarations */
void do_ls_sorted(const char *dir, int display_mode);
void do_ls_long(const char *dir, char **names, size_t count);
void do_ls_columns(char **names, size_t count);
void do_ls_horizontal(char **names, size_t count);
void print_long_format(const char *path, const char *filename);
void print_permissions(mode_t mode);
static int get_terminal_width(void);

/* Comparison function for qsort */
int cmpstring(const void *a, const void *b)
{
    char *str1 = *(char **)a;
    char *str2 = *(char **)b;
    return strcmp(str1, str2);
}

int main(int argc, char *argv[])
{
    int opt;
    int long_format = 0;
    int horizontal_format = 0;

    /* parse options -l and -x */
    while ((opt = getopt(argc, argv, "lx")) != -1)
    {
        switch (opt)
        {
        case 'l':
            long_format = 1;
            break;
        case 'x':
            horizontal_format = 1;
            break;
        default:
            fprintf(stderr, "Usage: %s [-l] [-x] [directory...]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    int display_mode = 0; // 0: default down-across, 1: long, 2: horizontal
    if (long_format)
        display_mode = 1;
    else if (horizontal_format)
        display_mode = 2;

    /* If no directories specified, use current directory */
    if (optind == argc)
    {
        do_ls_sorted(".", display_mode);
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            printf("Directory listing of %s:\n", argv[i]);
            do_ls_sorted(argv[i], display_mode);
            puts("");
        }
    }

    return 0;
}

/* -------------------------
   Sorted directory listing
   display_mode: 0=down-across, 1=long, 2=horizontal
   ------------------------- */
void do_ls_sorted(const char *dir, int display_mode)
{
    DIR *dp = opendir(dir);
    if (!dp)
    {
        fprintf(stderr, "Cannot open directory: %s\n", dir);
        return;
    }

    size_t capacity = 64;
    size_t count = 0;
    char **names = malloc(capacity * sizeof(char *));
    if (!names)
    {
        perror("malloc");
        closedir(dp);
        return;
    }

    errno = 0;
    struct dirent *entry;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;

        if (count >= capacity)
        {
            capacity *= 2;
            char **tmp = realloc(names, capacity * sizeof(char *));
            if (!tmp)
            {
                perror("realloc");
                for (size_t i = 0; i < count; ++i) free(names[i]);
                free(names);
                closedir(dp);
                return;
            }
            names = tmp;
        }

        names[count] = strdup(entry->d_name);
        if (!names[count])
        {
            perror("strdup");
            for (size_t i = 0; i < count; ++i) free(names[i]);
            free(names);
            closedir(dp);
            return;
        }
        count++;
    }

    if (errno != 0)
        perror("readdir failed");

    closedir(dp);

    if (count == 0)
    {
        free(names);
        return;
    }

    /* sort the filenames alphabetically */
    qsort(names, count, sizeof(char *), cmpstring);

    /* display according to mode */
    if (display_mode == 1)
        do_ls_long(dir, names, count);
    else if (display_mode == 2)
        do_ls_horizontal(names, count);
    else
        do_ls_columns(names, count);

    /* free memory */
    for (size_t i = 0; i < count; ++i) free(names[i]);
    free(names);
}

/* -------------------------
   Down-across column display
   ------------------------- */
void do_ls_columns(char **names, size_t count)
{
    size_t maxlen = 0;
    for (size_t i = 0; i < count; ++i)
    {
        size_t len = strlen(names[i]);
        if (len > maxlen) maxlen = len;
    }

    int term_width = get_terminal_width();
    const int spacing = 2;
    int col_width = (int)maxlen + spacing;
    if (col_width <= 0) col_width = 1;

    int num_cols = term_width / col_width;
    if (num_cols < 1) num_cols = 1;
    if ((size_t)num_cols > count) num_cols = (int)count;

    int num_rows = (count + num_cols - 1) / num_cols;

    for (int r = 0; r < num_rows; ++r)
    {
        for (int c = 0; c < num_cols; ++c)
        {
            int idx = c * num_rows + r;
            if (idx >= (int)count) continue;
            if (c == num_cols - 1)
                printf("%s", names[idx]);
            else
                printf("%-*s", col_width, names[idx]);
        }
        putchar('\n');
    }
}

/* -------------------------
   Horizontal (-x) display
   ------------------------- */
void do_ls_horizontal(char **names, size_t count)
{
    size_t maxlen = 0;
    for (size_t i = 0; i < count; ++i)
    {
        size_t len = strlen(names[i]);
        if (len > maxlen) maxlen = len;
    }

    int term_width = get_terminal_width();
    const int spacing = 2;
    int col_width = (int)maxlen + spacing;
    if (col_width <= 0) col_width = 1;

    int curr_width = 0;
    for (size_t i = 0; i < count; ++i)
    {
        if (curr_width + col_width > term_width)
        {
            putchar('\n');
            curr_width = 0;
        }
        printf("%-*s", col_width, names[i]);
        curr_width += col_width;
    }
    putchar('\n');
}

/* -------------------------
   Long listing display (-l)
   ------------------------- */
void do_ls_long(const char *dir, char **names, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
        print_long_format(path, names[i]);
    }
}

/* -------------------------
   Long format helper functions
   ------------------------- */
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

/* -------------------------
   Terminal width helper
   ------------------------- */
static int get_terminal_width(void)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
        return 80;
    return (int)ws.ws_col;
}

