# Mini-UnionFS

A lightweight, user-space layered **Union File System** implemented in **C** using **FUSE 3 (Filesystem in Userspace)**. 

Mini-UnionFS merges two distinct underlying directory branches (`lower_dir` and `upper_dir`) into a single unified mount point (`mount_dir`). It enforces **Copy-on-Write (CoW)** semantics to guarantee that the lower branch remains strictly read-only, while supporting file creation, modification, directory merging, and file/directory deletion via **whiteout markers**.

---

## Key Features

- **Layered Branch Merging**: Transparently blends a read-only base layer (`lower`) and a read-write layer (`upper`) into a unified virtual namespace.
- **Copy-on-Write (CoW)**:
  - Modifying (`O_WRONLY`, `O_RDWR`, `O_APPEND`) a file existing only in the lower layer automatically triggers `execute_cow`.
  - Performs block-by-block POSIX `read()`/`write()` copying into the `upper` directory while replicating file permissions (`st_mode`) before write operations occur.
  - Guarantees complete immutability of the base filesystem.
- **Whiteout File & Directory Masking**:
  - Unlinking a file from the lower layer creates a `.wh.<filename>` marker in the upper layer, hiding it from path resolution and `readdir`.
  - Directory deletion (`rmdir`) implements opaque directory markers (`.wh.<dirname>`), preventing lower layer directories from surfacing.
- **POSIX-Compliant Operations**: Implements core VFS callbacks including `getattr`, `readdir`, `open`, `read`, `write`, `create`, `unlink`, `mkdir`, `rmdir`, `truncate`, and `utimens`.
- **Clean Context & State Management**: Encapsulates filesystem state in `mini_unionfs_state` stored within `fuse_get_context()->private_data`, avoiding fragile global static variables.

---

## Architecture Overview

```
+--------------------------------------------------------------+
|                      Unified Mount Point                     |
|                         (/mnt/union)                         |
+------------------------------+-------------------------------+
                               |
               Path Resolution & Whiteout Routing
                               |
        +----------------------+----------------------+
        |                                             |
        v                                             v
+-----------------------+                   +-----------------------+
|      Upper Layer      |                   |      Lower Layer      |
|     (Read / Write)    |                   |      (Read-Only)      |
|                       |                   |                       |
| - Newly created files |   Copy-on-Write   | - Base system files   |
| - CoW modified copies | <================ | - Immutable data      |
| - Whiteouts (.wh.*)   |                   |                       |
+-----------------------+                   +-----------------------+
```

### Path Resolution Priority:
1. Check if a whiteout marker exists in `upper` (`.wh.<filename>`). If found, return `-ENOENT` (file masked/deleted).
2. Check if the entry exists in `upper`. If found, return the upper path.
3. Fallback to `lower`. If found, return the lower path.
4. Return `-ENOENT` if absent from both branches.

---

## Directory Structure

```
mini-unionfs/
├── src/
│   ├── main.c           # Entry point, argument parsing, FUSE initialization
│   ├── fs_ops.c         # FUSE callback handlers (getattr, readdir, open, write, etc.)
│   ├── fs_ops.h         # FUSE callback declarations
│   ├── cow.c            # Copy-on-Write implementation (execute_cow)
│   ├── cow.h            # CoW declarations
│   ├── path_util.c      # Path resolution and whiteout translation
│   ├── path_util.h      # Path resolution headers
│   ├── logger.c         # Structured logging utilities
│   ├── logger.h         # Logger declarations
│   └── state.h          # mini_unionfs_state context struct
├── docs/
│   └── design_document.md # Detailed architecture and edge-case documentation
├── tests/
│   ├── test_unionfs.sh  # Automated verification suite (6 core test scenarios)
│   └── new_tests.sh     # Extended test harness for edge cases
├── Makefile             # Compilation rules with pkg-config fuse3
└── README.md            # Project documentation
```

---

## Prerequisites

- **Linux** (or macOS with macFUSE)
- **GCC / Clang**
- **libfuse3-dev** (FUSE 3 development headers)
- **pkg-config**

### Installing Dependencies (Ubuntu / Debian):
```bash
sudo apt-get update
sudo apt-get install -y build-essential libfuse3-dev pkg-config
```

---

## Building and Running

### 1. Compile Mini-UnionFS
```bash
make
```
This builds the `mini_unionfs` executable in the project root.

### 2. Mount the Union Filesystem
```bash
# Create underlying directories
mkdir -p lower upper mnt

# Mount the filesystem
./mini_unionfs lower upper mnt
```

### 3. Unmount
```bash
fusermount3 -u mnt
# Or on macOS / BSD:
# umount mnt
```

---

## Automated Test Suite

A comprehensive test harness is included in `tests/`:

```bash
chmod +x tests/test_unionfs.sh
./tests/test_unionfs.sh
```

### Test Coverage:
1. **Layer Visibility**: Verifies lower files are accessible at the mount point.
2. **Copy-on-Write**: Modifies a lower file and verifies changes persist only in `upper`, leaving `lower` untouched.
3. **Whiteout Mechanism**: Deletes a lower file and verifies `.wh.` marker generation in `upper`.
4. **Directory Merging**: Verifies concurrent directory contents from both layers are correctly presented in `readdir`.
5. **New File Creation**: Confirms new files are isolated to `upper`.
6. **Directory Deletion & Opaque Dirs**: Verifies recursive/opaque directory deletion masking.

---

## License

MIT License. Designed for systems programming and operating systems coursework at PES University.
