<h1 align="center">
  <img src="https://github.com/Hridoy-31/Codible/blob/main/Artwork/cover.png" alt="Codible" width="500">
</h1>

![GitHub last commit](https://img.shields.io/github/last-commit/Hridoy-31/Codible) ![GitHub](https://img.shields.io/github/license/Hridoy-31/codible) ![GitHub code size in bytes](https://img.shields.io/github/languages/code-size/Hridoy-31/codible) ![GitHub repo size](https://img.shields.io/github/repo-size/Hridoy-31/codible)


Codible is a lightweight terminal based text editor with basic operations. It is heavily inspired from Kilo
written by Salvatore Sanfilippo and is released under the BSD 2 clause license.

Features
--------

- Headless
- Can run irrespective of any desktop environment
- Embedded, scriptable
- Fully written in pure C language
- Doesn't depend on any library
- Independent of `curses`
- Standard escape sequenences like VT100
- REPL style CLI

Usage:
-------

- By typing `codible` in Linux and `codible.exe` in Windows, it will open 
new buffer (clean slate mode).
- To open any existing file, just type `codible` and after a `space` provide 
the `filename`.  
For example, for opening `codible.c`, simply type:

    ```
    codible codible.c
    ```
    In Windows, simply type:
    ```
    codible.exe codible.c
    ```

Key Bindings
--------------

- `Ctrl-S` : Save
- `Ctrl-Q` : Quit
- `Ctrl-F` : Find string in file (`Esc` to exit, arrows to navigate)
- `Home` : Cursor at Left most character
- `End` : Cursor at Right most character
- `Page Up` : Previous Page
- `Page Down` : Next Page
- `Arrow Keys` : For navigation


Installation in Linux
-----------------------

1. The following packages should be installed:
    - `git`
    - `gcc`
    - `make`

2. Clone the repository by typing:
    ```
    git clone https://github.com/Hridoy-31/Codible.git
    ```
    in the terminal and press `Enter`.
    
    If you are using **GitHub CLI**, then type:
    ```
    gh repo clone Hridoy-31/Codible
    ```
    in the terminal and press `Enter`.
    
    *It is recommended to use Github CLI*
    
3. Change the directory to the cloned folder by typing:
    ```
    cd Codible
    ```
    and press `Enter`. Now the terminal will operate in that directory

4. To build Codible from source, type:
    ```
    make codible
    ```
    This will create the Linux executable file named `codible`
    
5. To run Codible, simply just type:
    ```
    ./codible
    ```
    That's it.  
 
See the `usage` section and `Key Bindings` section for help.
   
Installation in Windows:
--------------------------

Because of using the `<termios.h>` header file, this text editor can not be installed
or used natively in Windows. There is no cross compiled library for `<termios.h>`
in Windows operating system.

1. To use Codible in windows, **Cygwin** must be installed. To install **Cygwin**,
go to the official website [Cygwin](https://www.cygwin.com/). Then, download and install **Cygwin** 
by running `setup-x86_64.exe`

2. In `Select packages` menu, select the following packages:
    * `automake`
    * `cygwin-devel`
    * `gcc-core`
    * `gcc-g++`
    * `gcc-tools-epoch1-autoconf`
    * `gcc-tools-epoch1-automake`
    * `gcc-tools-epoch2-autoconf`
    * `gcc-tools-epoch2-automake`
    * `gccmakedep`
    * `gdb`
    * `git`
    * `make`
    * `makedepend`

3. After installing **Cygwin**, add it to the PATH by simply putting
`C:\cygwin64\bin;` in the PATH variable.
*It might be different in your system. So check it carefully.*

4. Open `Cygwin64 Terminal`. It will be in `C:\cygwin64\home\USER` directory. 
*USER is your username here*

5. In the terminal, type:
    ```
    git clone https://github.com/Hridoy-31/Codible.git
    ```
    and press `Enter`. It will fetch the repository to the above mentioned directory.
    
6. Change the directory by typing:
    ```
    cd Codible
    ```
    and press `Enter`. Now the terminal will active inside the directory.
    
7. Now for building Codible from the source, type:
    ```
    make codible
    ```
    and press `Enter`. 
    
8. Now type `codible.exe` and press `Enter`. Thats it !!  

See the `usage` section and `Key Bindings` section for help.


