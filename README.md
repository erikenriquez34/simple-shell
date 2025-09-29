
# Simple-Shell

A lightweight Unix-like shell built in C for my Operating Systems course.  Supports job control, 
pipelines, and built-in commands, while keeping the implementation simple and focused on the shell features.

## Features:
- Foreground and background execution (`sleep 5` vs `sleep 5 &`)
- Job control with `Ctrl-C`, `Ctrl-Z`, `fg`, and `bg`
- Type `jobs` to view all running/stopped jobs
- Built-in `cd` and `quit`
- Pipelines (`echo "hello world" | sort | wc -w`)
- Optional verbose mode for debugging

## Installation (Local Setup):

Clone the repository:
```
git clone https://github.com/erikenriquez34/simple-shell
```

Build with make:
```
cd ./simple-shell
make
```

Run the shell:
```
./shell
```

## Screenshots:

<img width="836.25" height="471" alt="shot" src="https://github.com/user-attachments/assets/96a75079-5fa3-4c49-991d-515352fd29ab" />
