/*
 * ls-v1.3.0
 * Adds -x horizontal (row-major) display; integrates -l and default (down-then-across)
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

extern int errno;

/* display modes */
typedef enum { MODE_DEFAULT, MODE_LONG, MODE_HORIZONTAL } display_mode_t;

static int get_terminal_width(void)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
        return 80;
    return (int)ws.ws_col;
}

/* ----- helpers to read directory names into dynamic array ----- */
static int read_dir_names(const char *dir, char ***out_names, size_t *out_count, size_t *out_maxlen)
{
    DIR *dp = opendir(dir);
    if (!dp) return -1;

    size_t capacity = 64, count = 0;
    char **names = malloc(capacity * sizeof(char *));
    if (!names) { closedir(dp); return -1; }

    size_t maxlen = 0;
    struct dirent *entry;
    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.') continue; /* skip hidden */
        if (count >= capacity)
        {
            capacity *= 2;
            char **tmp = realloc(names, capacity * sizeof(char *));
            if (!tmp) { /* cleanup */ for (size_t i=0;i<count;i++) free(names[i]); free(names); closedir(dp); return -1; }
            names = tmp;
        }
        names[count] = strdup(entry->d_name);
        if (!names[count]) { for (size_t i=0;i<count;i++) free(names[i]); free(names); closedir(dp); return -1; }
        size_t len = strlen(names[count]);
        if (len > maxlen) maxlen = len;
        count++;
    }

    if (errno != 0) { for (size_t i=0;i<count;i++) free(names[i]); free(names); closedir(dp); return -1; }

    closedir(dp);
    *out_names = names;
    *out_count = count;
    *out_maxlen = maxlen;
    return 0;
}

/* free names array */
static void free_names(char **names, size_t count)
{
    if (!names) return;
    for (size_t i = 0; i < count; ++i) free(names[i]);
    free(names);
}

/* ----- long listing (same logic as before) ----- */
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

void print_long_format(const char *dir, const char *filename)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", dir, filename);

    struct stat st;
    if (lstat(path, &st) == -1) { perror("lstat"); return; }

    print_permissions(st.st_mode);
    printf(" %3ld", (long)st.st_nlink);

    struct passwd *pw = getpwuid(st.st_uid);
    struct group *gr = getgrgid(st.st_gid);
    printf(" %-8s %-8s", pw ? pw->pw_name : "unknown", gr ? gr->gr_name : "unknown");
    printf(" %8ld", (long)st.st_size);

    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", localtime(&st.st_mtime));
    printf(" %s %s\n", timebuf, filename);
}

/* do long listing using the names array */
static void display_long(const char *dir, char **names, size_t count)
{
    for (size_t i = 0; i < count; ++i) print_long_format(dir, names[i]);
}

/* ----- default down-then-across display (v1.2 behavior) ----- */
static void display_down_across(char **names, size_t count, size_t maxlen, int term_width)
{
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

/* ----- horizontal (row-major) display for -x ----- */
static void display_horizontal(char **names, size_t count, size_t maxlen, int term_width)
{
    const int spacing = 2;
    int col_width = (int)maxlen + spacing;
    if (col_width <= 0) col_width = 1;

    int current_pos = 0; /* characters used on current line */

    for (size_t i = 0; i < count; ++i)
    {
        int name_len = (int)strlen(names[i]);
        int field_width = col_width;

        /* If the name itself is longer than terminal, print it on its own line */
        if (name_len >= term_width)
        {
            if (current_pos != 0) { putchar('\n'); current_pos = 0; }
            printf("%s\n", names[i]);
            continue;
        }

        /* If adding this field would exceed terminal width, wrap */
        if (current_pos + field_width > term_width)
        {
            putchar('\n');
            current_pos = 0;
        }

        /* If this is last item in line or the only remaining space, print without forced padding at end-of-line:
           still use %-*s to keep alignment for non-last in line */
        if (current_pos + field_width <= term_width)
        {
            /* If printing this will exactly fill the line or next item would exceed, it's fine; always left-pad */
            printf("%-*s", field_width, names[i]);
            current_pos += field_width;
        }
        else
        {
            /* fallback: print name and a space */
            printf("%s ", names[i]);
            current_pos += name_len + 1;
        }
    }

    if (current_pos != 0) putchar('\n');
}

/* ----- single directory processor: read names, choose display ----- */
static void process_directory(const char *dir, display_mode_t mode)
{
    char **names = NULL;
    size_t count = 0, maxlen = 0;

    if (read_dir_names(dir, &names, &count, &maxlen) == -1)
    {
        fprintf(stderr, "Cannot open or read directory: %s\n", dir);
        return;
    }

    if (count == 0) { free_names(names, count); return; }

    int term_width = get_terminal_width();

    switch (mode)
    {
        case MODE_LONG:
            display_long(dir, names, count);
            break;
        case MODE_HORIZONTAL:
            display_horizontal(names, count, maxlen, term_width);
            break;
        case MODE_DEFAULT:
        default:
            display_down_across(names, count, maxlen, term_width);
            break;
    }

    free_names(names, count);
}

/* ----- main: parse -l and -x ----- */
int main(int argc, char *argv[])
{
    int opt;
    display_mode_t mode = MODE_DEFAULT;

    while ((opt = getopt(argc, argv, "lx")) != -1)
    {
        switch (opt)
        {
            case 'l': mode = MODE_LONG; break;
            case 'x': mode = MODE_HORIZONTAL; break;
            default:
                fprintf(stderr, "Usage: %s [-l] [-x] [directory...]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (optind == argc)
    {
        process_directory(".", mode);
    }
    else
    {
        for (int i = optind; i < argc; ++i)
        {
            printf("Directory listing of %s:\n", argv[i]);
            process_directory(argv[i], mode);
            if (i + 1 < argc) putchar('\n');
        }
    }

    return 0;
}

