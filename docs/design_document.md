\# Mini-UnionFS Design Document



\## 1. Global State Management

The filesystem utilizes a global `mini\_unionfs\_state` struct passed into the FUSE context via `fuse\_get\_context()->private\_data`. This struct stores the absolute paths to `lower\_dir` and `upper\_dir`, ensuring all callback functions have context without relying on global static variables.



\## 2. Path Resolution \& Whiteouts

The `resolve\_path` function is the core routing mechanism. For any given `/path`:

1\. It generates the expected whiteout path (`/upper\_dir/.wh.path`).

2\. If the whiteout exists, it returns `-ENOENT` (File Not Found), successfully masking the lower layer file.

3\. If no whiteout exists, it checks `upper\_dir`. If present, it returns the upper path.

4\. Finally, it checks `lower\_dir`.



\## 3. Copy-on-Write (CoW) Mechanism

When `unionfs\_open` detects the `O\_WRONLY`, `O\_RDWR`, or `O\_APPEND` flags on a file that currently only exists in the lower directory, it triggers `execute\_cow`. This function performs a block-by-block POSIX `read()`/`write()` copy of the file into the `upper\_dir` and replicates the `st\_mode` permissions before allowing the FUSE open operation to proceed. 



\## 4. Edge Cases Handled

\* \*\*Directory Merging (`readdir`):\*\* The `readdir` operation reads from both directories. It explicitly hides files starting with `.wh.` and omits files from the lower directory if a corresponding whiteout file exists.

\* \*\*Directory Deletion:\*\* `rmdir` implements opaque directory logic. If a directory is deleted from the lower layer, a standard whiteout marker is created in the upper layer, instructing `readdir` and `getattr` to treat it as nonexistent.

