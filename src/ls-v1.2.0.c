/*
 * Programming Assignment 02: ls-v1.2.0
 * Version 1.2.0 — Column Display (Down Then Across)
 * Features: terminal width detection (ioctl), dynamic arrays, down-then-across columns
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

void do_ls_columns(const char *dir);    /* new column display */
void do_ls_long(const char *dir);       /* keep long listing if needed (reuse v1.1.0) */
void print_long_format(const char *path, const char *filename);
void print_permissions(mode_t mode);

int main(int argc, char *argv[])
{
    int opt;
    int long_format = 0;

    /* parse -l like before, but default non -l will use column display */
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

    if (optind == argc)
    {
        if (long_format)
            do_ls_long(".");
        else
            do_ls_columns(".");
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            printf("Directory listing of %s:\n", argv[i]);
            if (long_format)
                do_ls_long(argv[i]);
            else
                do_ls_columns(argv[i]);
            puts("");
        }
    }

    return 0;
}

/* -------------------------
   Column display (v1.2.0)
   ------------------------- */
static int get_terminal_width(void)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1)
    {
        /* fallback to 80 if ioctl fails */
        return 80;
    }
    if (ws.ws_col == 0)
        return 80;
    return (int)ws.ws_col;
}

void do_ls_columns(const char *dir)
{
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory: %s\n", dir);
        return;
    }

    /* dynamic array for filenames */
    size_t capacity = 64;
    size_t count = 0;
    char **names = malloc(capacity * sizeof(char *));
    if (!names)
    {
        perror("malloc");
        closedir(dp);
        return;
    }

    size_t maxlen = 0;
    errno = 0;
    struct dirent *entry;
    while ((entry = readdir(dp)) != NULL)
    {
        /* skip hidden files as before */
        if (entry->d_name[0] == '.')
            continue;

        /* ensure capacity */
        if (count >= capacity)
        {
            capacity *= 2;
            char **tmp = realloc(names, capacity * sizeof(char *));
            if (!tmp)
            {
                perror("realloc");
                /* free allocated names */
                for (size_t i = 0; i < count; ++i) free(names[i]);
                free(names);
                closedir(dp);
                return;
            }
            names = tmp;
        }

        /* copy filename */
        names[count] = strdup(entry->d_name);
        if (!names[count])
        {
            perror("strdup");
            for (size_t i = 0; i < count; ++i) free(names[i]);
            free(names);
            closedir(dp);
            return;
        }

        size_t len = strlen(names[count]);
        if (len > maxlen) maxlen = len;
        count++;
    }

    if (errno != 0)
    {
        perror("readdir failed");
        /* cleanup */
        for (size_t i = 0; i < count; ++i) free(names[i]);
        free(names);
        closedir(dp);
        return;
    }

    closedir(dp);

    if (count == 0)
    {
        /* nothing to print */
        free(names);
        return;
    }

    /* determine terminal width */
    int term_width = get_terminal_width();

    /* spacing between columns */
    const int spacing = 2;
    int col_width = (int)maxlen + spacing;
    if (col_width <= 0) col_width = 1;

    /* compute number of columns that fit */
    int num_cols = term_width / col_width;
    if (num_cols < 1) num_cols = 1;
    if ((size_t)num_cols > count) num_cols = (int)count; /* don't exceed count */

    /* compute rows needed (down then across) */
    int num_rows = (count + num_cols - 1) / num_cols; /* ceil(count/num_cols) */

    /* print rows, iterating row by row */
    for (int r = 0; r < num_rows; ++r)
    {
        for (int c = 0; c < num_cols; ++c)
        {
            int idx = c * num_rows + r;
            if (idx >= (int)count) continue;

            /* last column: print filename without trailing spaces */
            if (c == num_cols - 1)
            {
                printf("%s", names[idx]);
            }
            else
            {
                /* pad to column width (left justified) */
                /* Use printf with width: %-*s ensures left alignment */
                printf("%-*s", col_width, names[idx]);
            }
        }
        putchar('\n');
    }

    /* free memory */
    for (size_t i = 0; i < count; ++i) free(names[i]);
    free(names);
}

/* ---------------
   Long listing
   --------------- */
/* if you already have v1.1.0 functions, you can reuse them exactly.
   For completeness, minimal implementations are provided below. */

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

