# PES VCS Project

## About the Project

This project is a simple version control system built in C. It works similar to Git but only basic features are implemented. Using this, we can track changes in files, store versions and see commit history.

The main idea of this project is to understand how Git works internally like object storage, hashing and commits.

---

## Features

* Initialize repository using pes init
* Add files using pes add
* Commit changes using pes commit
* Check status using pes status
* View history using pes log

---

## Files in Project

* object.c - handles storing and reading objects
* tree.c - creates directory structure
* index.c - manages staging area
* commit.c - handles commits and history

---

## Screenshots

All required screenshots for each phase are added in the screenshots folder in this repository.

They include:

* test outputs
* object storage structure
* tree output
* index file
* commit logs
* integration test

---

## Analysis Questions

### Q5.1

For checkout, we change HEAD to the selected branch and read its commit. Then we update working directory files based on that commit. We also update the index.

This is complex because we need to handle file changes properly and avoid overwriting user changes.

---

### Q5.2

To detect conflict, we compare file hash in index with working directory. If both are different and target branch also has different version, then we stop checkout.

---

### Q5.3

Detached HEAD means HEAD points to a commit directly. If we commit in this state, commits are created but no branch points to them. They can be lost later. We can recover by creating a branch.

---

### Q6.1

To remove unused objects, we start from all branches and mark all reachable commits, trees and blobs. Then delete the remaining objects.

We can use a hash set to track visited objects.

---

### Q6.2

Running garbage collection during commit is risky because new objects may not yet be referenced. GC may delete them and cause errors.

Git avoids this using safe updates and locking.
