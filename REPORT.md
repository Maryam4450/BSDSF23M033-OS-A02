---
                          Feature-2: ls-v1.1.0 - Complete Long Listing Format 
---

**Q1: What is the difference between stat() and lstat()? When should we use lstat()?**  

stat() follows symbolic links and gives info about the target file.  
lstat() gives info about the link itself, not the file it points to.  
In the ls command, we use lstat() when we want to show details of a symbolic link instead of its target.  

**Q2: How does st_mode store file type and permissions? How do we extract them?**  

The st_mode field is an integer that uses bits to store file type and permission info.  
We use bitwise AND (&) with macros to check specific bits:  
Example:  
*(st_mode & S_IFDIR)* → checks if it’s a directory  
*(st_mode & S_IRUSR)* → checks if the user has read permission  
This way we can test each bit to know the file type and access rights.  

---
                         Feature-3: ls-v1.2.0 – Column Display (Down Then Across)
---

**Q1: How does "down then across" printing work? Why not a simple loop?**  

In "down then across" format, files are printed in columns — going down each column first, then moving across.  
We first read all filenames, find the longest one, and calculate how many columns can fit in the terminal.  
Then we print row by row using two loops: one for rows and one for columns.  
A single loop can only print one filename per line, so it can’t make a grid layout.  

**Q2: What is the purpose of ioctl? What happens if we use a fixed width?**  

ioctl gets the terminal width so the program can adjust columns automatically.  
If we use a fixed width (like 80), the output won’t adjust to different terminal sizes —  
on wide screens space will be wasted, and on small screens text will wrap or look messy  

---
                         Feature-4: ls-v1.3.0 – Horizontal Column Display (-x Option)
---

**Q1: Compare the implementation complexity of the "down then across" (vertical) printing logic versus the "across" (horizontal) printing logic. Which one requires more pre-calculation and why?**

**Answer:**
The "down then across" (vertical) printing logic is more complex than the "across" (horizontal) printing logic because it requires additional pre-calculations to correctly map items from a one-dimensional list of filenames into a two-dimensional table layout (rows × columns).

**In vertical layout:**
The program must first calculate how many columns fit within the terminal width.
Then, it computes how many rows are needed and determines each item’s position using a formula like index = column * num_rows + row.
This mapping ensures files are printed top-to-bottom, then left-to-right — which needs extra computation and careful indexing.

**In horizontal layout (-x option):**
Files are printed left-to-right, then top-to-bottom (natural order).
The only calculation needed is checking whether the current line width exceeds the terminal width and moving to the next line when it does.
Hence, the horizontal layout is simpler because it does not require multi-dimensional indexing or pre-calculation of rows and columns.

**Q2: Describe the strategy you used in your code to manage the different display modes (-l, -x, and default). How did your program decide which function to call for printing?**

My code uses an enumeration (display_mode_t) to represent the display modes:
typedef enum { MODE_DEFAULT, MODE_LONG, MODE_HORIZONTAL } display_mode_t;

*In the main() function:  
I parse command-line options using getopt().  
Each flag sets the display mode variable:  
while ((opt = getopt(argc, argv, "lx")) != -1) {  
    switch (opt) {  
        case 'l': mode = MODE_LONG; break;  
        case 'x': mode = MODE_HORIZONTAL; break;  
    }  
}*  

Then, in the process_directory() function, I use a switch statement to call the appropriate display function:  
*switch (mode) {  
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
}*  

This approach keeps the design modular:   
Each display mode has its own dedicated function.  

---
                         Feature-5: ls-v1.4.0 – Alphabetical Sort 
---

**Q1: Why is it necessary to read all directory entries into memory before you can sort them? What are the potential drawbacks of this approach for directories containing millions of files?**
To sort filenames, all entries must first be stored in memory (e.g., in an array) so the sorting algorithm can compare and reorder them.
The drawback is high memory usage and slow performance — storing millions of entries can exhaust memory and increase sorting time significantly.

**Q2: Explain the purpose and signature of the comparison function required by qsort(). How does it work, and why must it take const void * arguments?
qsort() needs a comparison function to decide the order of two elements.**
Its signature is:
int cmpstring(const void *a, const void *b);
It returns negative, zero, or positive depending on whether a < b, a == b, or a > b.
The parameters are const void * so that the function can work with any data type (generic) and not modify the elements being compared.

---
                         Feature-6: ls-v1.5.0 – Colorized Output Based on File Type 
---

**Q1: How do ANSI escape codes work to produce color in a standard Linux terminal? Show the specific code sequence for printing text in green.**

**Answer:**
ANSI escape codes are special character sequences that control text formatting, color, and style in the terminal. They start with \033[ (ESC), followed by parameters and end with m.
For example:
*printf("\033[32mThis text is green\033[0m\n");*
Here:
\033[32m → sets green foreground color.
\033[0m → resets color back to default.

**Q2: To color an executable file, you need to check its permission bits. Explain which bits in the st_mode field you need to check to determine if a file is executable by the owner, group, or others.**

**Answer:**
The executable permission bits in the st_mode field are:
S_IXUSR → executable by owner
S_IXGRP → executable by group
S_IXOTH → executable by others

---
                         Feature-7: ls-v1.6.0 – Recursive Listing (-R Option)
---

**Q1: In a recursive function, what is a "base case"? In the context of your recursive ls, what is the base case that stops the recursion from continuing forever?**

Answer:
A base case is the condition in a recursive function that stops further recursive calls.
In the recursive ls, the base case occurs when the program encounters a directory with no subdirectories or when it reaches "." or ".." (to avoid looping back to the parent or current directory). This prevents infinite recursion through directory cycles.

**Q2: Explain why it is essential to construct a full path (e.g., "parent_dir/subdir") before making a recursive call. What would happen if you simply called do_ls("subdir") from within the do_ls("parent_dir") function call?**

**Answer:**
It’s essential to construct the full path (parent_dir/subdir) so the program knows the exact location of the subdirectory relative to the filesystem.
If you only called do_ls("subdir"), the program would look for "subdir" in the current working directory instead of inside "parent_dir", causing incorrect traversal or “directory not found” errors during recursion.
