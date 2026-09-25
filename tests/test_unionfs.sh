#!/bin/bash

FUSE_BINARY="./mini_unionfs"
TEST_DIR="./unionfs_test_env"
LOWER_DIR="$TEST_DIR/lower"
UPPER_DIR="$TEST_DIR/upper"
MOUNT_DIR="$TEST_DIR/mnt"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

echo "Starting Advanced Mini-UnionFS Test Suite..."

# Setup: Create complex scenarios
rm -rf "$TEST_DIR"
mkdir -p "$LOWER_DIR/shared_dir" "$UPPER_DIR/shared_dir" "$MOUNT_DIR"
mkdir -p "$LOWER_DIR/dir_to_delete"

echo "base_only_content" > "$LOWER_DIR/base.txt"
echo "to_be_deleted" > "$LOWER_DIR/delete_me.txt"
echo "lower_file" > "$LOWER_DIR/shared_dir/file_A.txt"
echo "upper_file" > "$UPPER_DIR/shared_dir/file_B.txt"

# Mount
$FUSE_BINARY "$LOWER_DIR" "$UPPER_DIR" "$MOUNT_DIR"
sleep 1

# Test 1: Layer Visibility
echo -n "Test 1: Layer Visibility... "
if grep -q "base_only_content" "$MOUNT_DIR/base.txt"; then 
    echo -e "${GREEN}PASSED${NC}"
else 
    echo -e "${RED}FAILED${NC}"
fi

# Test 2: Copy-on-Write
echo -n "Test 2: Copy-on-Write... "
echo "modified_content" >> "$MOUNT_DIR/base.txt" 2>/dev/null

if [ $(grep -c "modified_content" "$MOUNT_DIR/base.txt" 2>/dev/null) -eq 1 ] && \
   [ $(grep -c "modified_content" "$UPPER_DIR/base.txt" 2>/dev/null) -eq 1 ] && \
   [ $(grep -c "modified_content" "$LOWER_DIR/base.txt" 2>/dev/null) -eq 0 ]; then
    echo -e "${GREEN}PASSED${NC}"
else
    echo -e "${RED}FAILED${NC}"
fi

# Test 3: File Whiteout (unlink)
echo -n "Test 3: File Whiteout mechanism... "
rm "$MOUNT_DIR/delete_me.txt" 2>/dev/null

if [ ! -f "$MOUNT_DIR/delete_me.txt" ] && \
   [ -f "$LOWER_DIR/delete_me.txt" ] && \
   [ -f "$UPPER_DIR/.wh.delete_me.txt" ]; then
    echo -e "${GREEN}PASSED${NC}"
else
    echo -e "${RED}FAILED${NC}"
fi

# Test 4: Directory Merging (readdir)
echo -n "Test 4: Directory Merging (readdir)... "
if [ -f "$MOUNT_DIR/shared_dir/file_A.txt" ] && [ -f "$MOUNT_DIR/shared_dir/file_B.txt" ]; then
    echo -e "${GREEN}PASSED${NC}"
else
    echo -e "${RED}FAILED${NC}"
fi

# Test 5: New File Creation (create)
echo -n "Test 5: New File Creation... "
echo "new_data" > "$MOUNT_DIR/new_file.txt" 2>/dev/null

if [ -f "$MOUNT_DIR/new_file.txt" ] && [ -f "$UPPER_DIR/new_file.txt" ] && [ ! -f "$LOWER_DIR/new_file.txt" ]; then
    echo -e "${GREEN}PASSED${NC}"
else
    echo -e "${RED}FAILED${NC}"
fi

# Test 6: Directory Deletion (rmdir)
echo -n "Test 6: Directory Deletion & Opaque Dirs... "
rmdir "$MOUNT_DIR/dir_to_delete" 2>/dev/null

if [ ! -d "$MOUNT_DIR/dir_to_delete" ] && \
   [ -d "$LOWER_DIR/dir_to_delete" ] && \
   [ -f "$UPPER_DIR/.wh.dir_to_delete" ]; then
    echo -e "${GREEN}PASSED${NC}"
else
    echo -e "${RED}FAILED${NC}"
fi

# Teardown
fusermount3 -u "$MOUNT_DIR" 2>/dev/null || umount "$MOUNT_DIR" 2>/dev/null
rm -rf "$TEST_DIR"

echo "Test Suite Completed."