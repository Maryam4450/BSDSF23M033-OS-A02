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
