/*
 * ls-v1.5.0
 * Version 1.5.0 — Colorized Output
 * Color rules:
 *  - Directory: blue
 *  - Executable: green
 *  - Archives (.tar, .gz, .zip, .tgz, .tar.gz, .bz2, .xz): red
 *  - Symbolic Link: magenta (pink)
 *  - Special files (char/block/socket/fifo): reverse video
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

/* display modes */
typedef enum { MODE_DEFAULT, MODE_LONG, MODE_HORIZONTAL } display_mode_t;

/* ANSI color codes */
#define CLR_RESET    "\033[0m"
#define CLR_BLUE     "\033[0;34m"
#define CLR_GREEN    "\033[0;32m"
#define CLR_RED      "\033[0;31m"
#define CLR_MAGENTA  "\033[0;35m"
#define CLR_REVERSE  "\033[7m"

/* prototypes */
static int get_terminal_width(void);
static int read_dir_names(const char *dir, char ***out_names, size_t *out_count, size_t *out_maxlen);
static void free_names(char **names, size_t count);
static int is_archive_name(const char *name);
static void print_colored_name_with_pad(const char *dir, const char *name, int pad);
static void display_down_across(char **names, size_t count, size_t maxlen, int term_width, const char *dir);
static void display_horizontal(char **names, size_t count, size_t maxlen, int term_width, const char *dir);
static void display_long(const char *dir, char **names, size_t count);
static void print_long_format(const char *path, const char *filename);
static void print_permissions(mode_t mode);

/* compare for qsort (case-insensitive) */
static int cmpstring_ci(const void *a, const void *b)
{
    char *s1 = *(char **)a;
    char *s2 = *(char **)b;
    return strcasecmp(s1, s2);
}

/* ---------- main ---------- */
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
        char **names = NULL;
        size_t count = 0, maxlen = 0;
        if (read_dir_names(".", &names, &count, &maxlen) == -1) return 1;
        if (count > 0)
        {
            qsort(names, count, sizeof(char *), cmpstring_ci);
            int tw = get_terminal_width();
            if (mode == MODE_LONG) display_long(".", names, count);
            else if (mode == MODE_HORIZONTAL) display_horizontal(names, count, maxlen, tw, ".");
            else display_down_across(names, count, maxlen, tw, ".");
        }
        free_names(names, count);
    }
    else
    {
        for (int i = optind; i < argc; ++i)
        {
            printf("Directory listing of %s:\n", argv[i]);
            char **names = NULL;
            size_t count = 0, maxlen = 0;
            if (read_dir_names(argv[i], &names, &count, &maxlen) == -1) continue;
            if (count > 0)
            {
                qsort(names, count, sizeof(char *), cmpstring_ci);
                int tw = get_terminal_width();
                if (mode == MODE_LONG) display_long(argv[i], names, count);
                else if (mode == MODE_HORIZONTAL) display_horizontal(names, count, maxlen, tw, argv[i]);
                else display_down_across(names, count, maxlen, tw, argv[i]);
            }
            free_names(names, count);
            if (i + 1 < argc) putchar('\n');
        }
    }

    return 0;
}

/* ---------- helpers ---------- */

static int get_terminal_width(void)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) return 80;
    return (int)ws.ws_col;
}

/* read names into dynamic array, skip hidden files */
static int read_dir_names(const char *dir, char ***out_names, size_t *out_count, size_t *out_maxlen)
{
    DIR *dp = opendir(dir);
    if (!dp) return -1;
    size_t capacity = 64, count = 0;
    char **names = malloc(capacity * sizeof(char *));
    if (!names) { closedir(dp); return -1; }
    size_t maxlen = 0;
    errno = 0;
    struct dirent *entry;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.') continue;
        if (count >= capacity)
        {
            capacity *= 2;
            char **tmp = realloc(names, capacity * sizeof(char *));
            if (!tmp)
            {
                for (size_t i = 0; i < count; ++i) free(names[i]);
                free(names);
                closedir(dp);
                return -1;
            }
            names = tmp;
        }
        names[count] = strdup(entry->d_name);
        if (!names[count])
        {
            for (size_t i = 0; i < count; ++i) free(names[i]);
            free(names);
            closedir(dp);
            return -1;
        }
        size_t len = strlen(names[count]);
        if (len > maxlen) maxlen = len;
        count++;
    }
    if (errno != 0)
    {
        perror("readdir failed");
        for (size_t i = 0; i < count; ++i) free(names[i]);
        free(names);
        closedir(dp);
        return -1;
    }
    closedir(dp);
    *out_names = names;
    *out_count = count;
    *out_maxlen = maxlen;
    return 0;
}

static void free_names(char **names, size_t count)
{
    if (!names) return;
    for (size_t i = 0; i < count; ++i) free(names[i]);
    free(names);
}

/* detect archive-like filenames by extension */
static int is_archive_name(const char *name)
{
    const char *exts[] = { ".tar", ".tar.gz", ".tgz", ".gz", ".zip", ".bz2", ".xz", NULL };
    for (int i = 0; exts[i]; ++i)
    {
        size_t l = strlen(name);
        size_t e = strlen(exts[i]);
        if (l >= e)
        {
            if (strcasecmp(name + l - e, exts[i]) == 0) return 1;
        }
    }
    return 0;
}

/* print colored name and pad spaces (pad is number of characters to add after printed name) */
static void print_colored_name_with_pad(const char *dir, const char *name, int pad)
{
    char path[4096];
    struct stat st;
    int is_link __attribute__((unused)) = 0;
    snprintf(path, sizeof(path), "%s/%s", dir, name);

    if (lstat(path, &st) == -1)
    {
        /* on error just print plain */
        printf("%s", name);
        for (int i = 0; i < pad; ++i) putchar(' ');
        return;
    }

    const char *start = "";
    const char *end = CLR_RESET;

    /* decide color/style */
    if (S_ISLNK(st.st_mode))
    {
        start = CLR_MAGENTA;
    }
    else if (S_ISDIR(st.st_mode))
    {
        start = CLR_BLUE;
    }
    else if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode) || S_ISSOCK(st.st_mode) || S_ISFIFO(st.st_mode))
    {
        start = CLR_REVERSE;
    }
    else if (is_archive_name(name))
    {
        start = CLR_RED;
    }
    else if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH))
    {
        start = CLR_GREEN;
    }
    else
    {
        start = ""; /* default */
        end = "";   /* avoid printing RESET if not used */
    }

    if (start[0] != '\0') printf("%s", start);
    printf("%s", name);
    if (end[0] != '\0') printf("%s", end);

    /* pad with spaces to keep columns aligned (color sequences don't affect pad) */
    for (int i = 0; i < pad; ++i) putchar(' ');
}

/* ---------- displays ---------- */

/* default: down then across */
static void display_down_across(char **names, size_t count, size_t maxlen, int term_width, const char *dir)
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
            int is_last_col = (c == num_cols - 1);
            int pad = is_last_col ? 0 : (col_width - (int)strlen(names[idx]));
            print_colored_name_with_pad(dir, names[idx], pad);
        }
        putchar('\n');
    }
}

/* horizontal (-x): row-major */
static void display_horizontal(char **names, size_t count, size_t maxlen, int term_width, const char *dir)
{
    const int spacing = 2;
    int col_width = (int)maxlen + spacing;
    if (col_width <= 0) col_width = 1;

    int curr = 0;
    for (size_t i = 0; i < count; ++i)
    {
        int name_len = (int)strlen(names[i]);
        int field = col_width;
        if (name_len >= term_width)
        {
            if (curr != 0) { putchar('\n'); curr = 0; }
            print_colored_name_with_pad(dir, names[i], 0);
            putchar('\n');
            continue;
        }
        if (curr + field > term_width)
        {
            putchar('\n');
            curr = 0;
        }
        int pad = field - name_len;
        print_colored_name_with_pad(dir, names[i], pad);
        curr += field;
    }
    if (curr != 0) putchar('\n');
}

/* long listing (-l) */
static void display_long(const char *dir, char **names, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
        print_long_format(path, names[i]);
    }
}

/* print long format but color the file name */
static void print_long_format(const char *path, const char *filename)
{
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
    printf(" %s ", timebuf);

    /* print colored filename (no extra padding) */
    /* path contains dir/name; we already have path to get lstat info again if needed */
    print_colored_name_with_pad(".", filename, 0);
    putchar('\n');
}

/* permissions string */
static void print_permissions(mode_t mode)
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

