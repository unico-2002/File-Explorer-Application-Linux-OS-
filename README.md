🧭 Console File Explorer (C++17)

A console-based File Explorer built in modern C++17, designed for Linux (works perfectly inside Ubuntu or WSL).
It provides a simple, command-driven interface to explore, manage, and manipulate your file system — similar to a shell like Bash, but built from scratch using C++ std::filesystem.


---

✨ Features

✅ Day 1: List files and directories
✅ Day 2: Navigate between folders (cd, pwd)
✅ Day 3: File management (cp, mv, rm, mkdir, touch, cat)
✅ Day 4: Search (find)
✅ Day 5: Manage file permissions (chmod, info)
✅ Bonus:

Recursive tree view (tree)

Safe delete confirmation

Human-readable file sizes

Owner and permission details

Added echo command for writing text into files



---

🧩 Project Structure

📂 Assignment 1 – Console File Explorer (C++)
 ├── file_explorer.cpp      # Main C++ source code
 ├── fexplorer              # Compiled executable (after build)
 ├── README.md              # Project documentation


---

⚙️ Build Instructions

🔹 Requirements

GCC or G++ 9+

Linux or WSL (Ubuntu recommended)

C++17 support


🔹 Compile

g++ -std=gnu++17 -O2 -Wall -Wextra -pedantic file_explorer.cpp -o fexplorer

🔹 Run

./fexplorer


---

💻 Usage

When you run the program, it starts in your current directory and shows:

Console File Explorer — starting in "/home/anupam"
Type 'help' for commands. Ctrl+C to interrupt long ops.
["/home/anupam"]$

Type help to see all supported commands:

Command	Description

pwd	Show current directory
ls [-a] [-l] [--tree] [--depth=N]	List files/folders
cd [path]	Change directory
cp <src> <dst>	Copy file/folder
mv <src> <dst>	Move or rename file/folder
rm <path>	Delete file or folder (asks confirmation)
mkdir <dir>	Create a directory
touch <file>	Create an empty file
cat <file>	Display file contents
find [pattern] [-r] [--in=dir]	Search for files
chmod <mode> <file>	Change file permissions (e.g., 755 or u+x)
info [file]	Show file info (size, perms, owner)
`echo <text> [> file	>> file]`
tree	Recursive directory view
`force on	off`
exit	Exit the program



---

🧠 Example Session

["/home/anupam"]$ mkdir test_folder
["/home/anupam"]$ cd test_folder
["/home/anupam/test_folder"]$ echo "Hello World!" > hello.txt
["/home/anupam/test_folder"]$ ls -l
["/home/anupam/test_folder"]$ cat hello.txt
Hello World!


---

🔐 Safety Features

Asks before deleting directories (rm) unless force on is used.

Gracefully handles Ctrl+C interruptions.

Ignores hidden/system files unless -a is specified.



---

🧰 Technical Details

Built using std::filesystem for file operations.

Uses std::chrono for timestamps and std::regex for searches.

Portable across Linux systems.

Written in standard C++17 (no external libraries).



---

🧑‍💻 Author

Anupam Das
Engineering Student | Passionate about Systems, C++, and Software Design

> “With great power comes great responsibility.” — Spider-Man 🕷️




---

🏁 License

This project is open-source under the MIT License.
Feel free to use, modify, and learn from it.
