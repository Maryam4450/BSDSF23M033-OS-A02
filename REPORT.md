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
