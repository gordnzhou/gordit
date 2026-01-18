# gordit
Git implementation in C

# Notes
1. git init creates .git folder with proper base folders and files
1. take repo directory and convert to objects in git/objects, and reconstruct repo from those objects
    - blob = "blob <size>\0<file contents>" <-- SHA is hash of this
    - tree = parse folders, files (get blobbed) into k-v text file recursively:
    ```
    <mode> <type> <hash> <name>
    100644 blob a906cb2a4a904a152e80877d4088654daad0c859      README
    100644 blob 8f94139338f9404f26296befa88755fc2598c289      Rakefile
    040000 tree 99f1a6d12cb4b6f19c8655fca46c3ecf317074e0      lib
    ```
    - commit = points to a tree, to previous commit (0 or more), with author and commiter (both are same for now, is global saved state), timestamp, message:
    ```
    tree d8329fc1cc938780ffdd9f94e0d364e0ea74f579
    author Scott Chacon <schacon@gmail.com> 1243040974 -0700
    committer Scott Chacon <schacon@gmail.com> 1243040974 -0700

    First commit
    ```
    - tags = object being tagged and type, tag, tagger + timestamp, message (can point to any object)
    - all objects compressed AFTER being SHA hashed
1. print diff between diff commits (including current uncommited state) of repo
    - diffs trees objects **uncompresses and diffs files only blobs have differing hashes, or file is missing from other tree**
1. .git/refs and HEAD
    - text files containing hash in ASCII (direct)
    - "ref: refs/remotes/origin/master" (indirect / symbolic)
    - recursively navigate refs until direct is reached
    - .git\refs\head stores local branches (file name is branch name and contents is HASH of commit)
    - .git\refs\tags stores **lightweight tags** (point directly to commit) and **annotated tags** (point to tag object)
    - HEAD contains symbolic ref to current branch or even SHA of objects (detached HEAD state). `git commit` always sets parent to the object which HEAD points to
1. parse commits objects (git log)
1. index file = contains staging area info in binary format
    1. list of blobs sorted by path name. git diff and status uses blobs in index when diff with staged state
    ```
    <mode> <blob-hash> <stage-number> <filename>
    100644 63c918c667fa005ff12ad89437f2fdc80926e21c 0   .gitignore
    ```
    - also stored: change and modification time, 
    1. do not add files in .gorditignore to index and .git\info\exclude 
1. git stash creates hidden commit and adds it to .git\refs\stash based on index. for unstaged state, staged state, or even untracked state
1. merging branches
    1. other branch's commit's tree converted to index
    1. **file has conflict iff file differs from base ancestor in both others and my branch**
    1. conflicting files are stored three times in index (with different stage numbers for base, ours and theirs)
        - git add replaces three entries in index with next file
    1. merge commit has 1+ parents, can merge more than two branches together
    1. MERGE_HEAD: created during **MERGE STATE** contain SHA hashes of all merging branch's commits. deleted after merge commit
1. rebasing branches
    1. find common ancestor (CA) branch of others and my branch
    1. .git\ORIG_HEAD created pointing to HEAD
    1. replays my commits from CA to my branch, starting from others branch. HEAD is detached and points to commit being replayed. Each replayed commit is a new commit. When replay is done my branch now points to newest commit and HEAD is reattached, pointing to my branch. rebase folders are deleted
    1. .git/rebase-apply/ or .git/rebase-merge/ contain git-rebase-apply file:
    ```
    pick 4a5b6c7 Fix typo in README
    pick 8d9e0f1 Add unit tests
    pick 9a0b1c2 Refactor authentication module
    ```
1. FUTURE: remote repo refs
1. FUTURE: packfiles to store loose objects into one file and deltas of similar blobs

## Functions
- create, read, delete binary and text files
- detect if file is binary or text
- create folder, list all files/folders in a folder (cross-platform)
- compress/decompress files using zlib
- decrypt/encrypt SHA-1 using openssl
- regex match 
- diff function for two text files

- blob object: to/from file 
- tree object: to/from folder
- commit object: constructor 
- index file:
    - array of entries, sorted by path
    - functions: to/from file, to/from tree


### Terminology
- Untracked files = in working tree but not in index nor HEAD
- Unstaged change = change in working tree but not in index
- Uncommited change = change in index but not in HEAD ('s commit's tree)
- Detached HEAD = HEAD points to commit HASH instead of ref
- Clean Index = index matches HEAD
- Clean Working Tree = Working directory matches index
- Tracking branch = a branch that has a default remote branch to push/pull/fetch from (called its "Upstream")
- Ref = stored in `.git\refs\` can be a branch, tag, remote ref, stash. HEAD, ORIG_HEAD, MERGE_HEAD are also refs. They point to a commit object (which points to a tree object)
- Tag = **immutable** refs, can be lightweight (points directy to commit), or annoated (points to a tag object)
- reflog = history of a ref. entry is added whenever ref moves (clone, commit, merge, checkout, reset, revert)
- merge commit: commit with 2+ parents
- three-way-merge: merge two commits using LCA
- merge-base = LCA commit(s) of two commits being merged, A and B. During three-way merge diff(A, LCA(A, B)) and diff(B, LCA(A, B)) are compared to find merged conflicts

### Update Commands
- `git init` create .git folder along with subfolder/files in current working directory
- `git add <file>` checks if file is ignored (in .gorditignore or .git\info\exclude), if not creates blob of file and index entry. If file name already exists index update it (remove conflict stage entries if merge conflict), otherwise adds it to index (sorted by file 
name).
- `git rm <file>` removes file from index, if index contains it 
- `git commit [-m <message>]` Create commit tree object from index, and create commit object which points to tree and HEAD. author and commiter name and email are global vars. update branch in HEAD to point to new commit. if HEAD points to commit (detached HEAD state), print warning. FAIL if any entries in index have stage number != 0 (merge conflict exists)
    - merge commit: MERGE_HEAD exists. add it as another parent and delete it. delete MERGE_MSG and MERGE_MODE
- `git checkout <branch/commit/tag>` converts commit object's tree to index file (old index is discarded, better implement warning for lost stages). gets files in tree replaces their current working tree version with blob ONLY IF its old index entry differs in new index. update HEAD. FAIL if there is an file NOT in commit tree such that its old index differs from new index. Add `--detach` to detach head to commit HASH. 
- `git checkout -b <branch-name> [start-point] [remote/branch]` / `git switch <branch-name>` create branch in .git\refs\heads pointing to current commit. If start point is set to another past commit, do checkout into past commit. `[remote/branch]` sets branch's upstream in .git\config.ini. FAIL if branch already exists
- `git checkout <branch/commit> -- <file-name>` / `git restore <file-name>` restores file in working tree and index to its version in commit arg's tree. if no branch/commit argument, restore file to index's version. HEAD remains unchanged. 
- `git tag` create tag in .git\refs\tags. If SHA-1 hash is given creates lightweight tag. Otherwise, can add message and make annotated tag. 
- `git reset` resets repo to match HEAD commit. backup of HEAD is stored in .git\ORIG_HEAD 
    - `soft` changes head
    - `mixed` changes head and index (changed files remain unchanged but are not marked for commit)
    - `hard` changes head, index and working tree (removes any uncomitted changes to tracked files)
- `git merge <other-branch>` merge other-branch into HEAD branch (only HEAD branche gets updated). FAIL if HEAD is detached, uncommited changes in index that will be overwritten by merge.
    - if `<other-branch>` is ancestor of HEAD, do nothing. 
    - fast forward merge: if HEAD is ancestor of `<other-branch>`, move HEAD up to other
    - merge: create index from other branch's tree object. create .git\MERGE_HEAD containing commit hash of other branch and .git\MERGE_MSG (prompt user for message) and .git\MERGE_MODE. Merge with current index using three way merge, checking for conflicts.
        - three-way-merge (base, ours, theirs) = before reading files, match deletes in "theirs" to adds in "ours", and vice versa (possible rename based on similarity %). 3 way text diff = compare diff(base, ours) to diff(base, theirs). diff(a, b) = list of line numbers and change type (modify, add, delete). 3 way diffs of binary files are trivial
        - No conflicts: merge commit is created right away (HEAD is updated). MERGE_HEAD, MERGE_MSG, MERGE_MODE deleted
        - Merge conflict: store backup of HEAD in ORIG_HEAD. For conflicting text files, add markers to conflicting lines in working tree. include conflicting files three times with stage numbers: 1 = common ancestor's ver, 2 = current branch's ver, 3 = other branch's ver.
- `git gc` removes objects that are not pointed to by any ref, HEAD ref, or refs in reflog

### Patching (diff) Commands
- `git apply <patch>` apply a .patch file to a file and/or to index (with `--index`):
```patch
diff --git a/foo.c b/foo.c
index 30cfd169..8de130c2 100644
--- a/foo.c
+++ b/foo.c
@@ -1,5 +1,5 @@
 #include <string.h>
 
 int check (char *string) {
-    return ! strcmp(string, "ok");
+    return (string != NULL) && ! strcmp(string, "ok");
 }
```
Conflict if lines changed in patch are modified in HEAD or index. 
- `git cherry-pick <commit>` compute diff of commit and its parent = diff(C, P). creates new commit to HEAD that applies the diff(). FAIL if directory is not clean (cannot have uncommited changes). Merge commits need a parent to be specified. Three-way-merge done using diff(C, C^) and diff(HEAD, C^), possible conflicts. 
- `git revert <commit>` (done on an ancestor of HEAD 99% of time) compute 'inverse' diff of commit and its parent (additions are removals and vice-versa). creates new commit to HEAD that applies the diff. FAIL if directory is not clean (cannot have uncommited changes). Merge commits need a parent to be specified. Three-way-merge done using inverse(diff(C, C^)) and diff(HEAD, C^) 

### Stash Commands
- `git stash` creates at most 3 commit objects:  
    - I: tree of index, parent is HEAD
    - W: tree of working directory, parent is HEAD and I
    - U  (optional): tree of untracked files, parent is W
    add ref to .git\refs\stash and log entry in .git\logs\refs\stash that points to W (or U). **.git\refs\stash only contains the most recent stash**. Reset index and working dir to match HEAD commit snapshot, so `git reset --hard HEAD`(optionally removes untracked files). git\logs\refs\stash (list of stashes used by pop)
- `git stash apply` reads commit W and I from .git\refs\stash git stash pop. Updates index by doing three way merge on tree of current index and tree of commit I (adds stage numbers for conflicts). Updates working tree by doing tree way merge on current working tree and tree of commit W (adds markers for conflicts). Optionally restore untracked files.
- `git stash drop` removes most recent entry in .git\logs\refs\stash
- `git stash pop` = `git stash apply` + `git stash pop` (only if merge in apply succeeds)

### Read Commands
- `git status` shows:
    1. HEAD (current branch or Detached HEAD)
    1. what will be committed (diff between index and current HEAD commit)
    1. changes not staged for commit (diff between index and working tree)
    1. files in working tree and not in index that are not ignored by .gitignore
- `git show <object>` prints info of object (blob, tree, commit, tag)
- `git log` prints ancestry of a specific branch/commit, recursively following parent commits. Defaults to HEAD ref
- `git reflog` shows list of history for a specific ref. **commit history** for a ref is stored in .git\logs in format `<prev-ref-hash> <ref-hash> <commiter-name> <commiter-email> <timestamp> <operation> <message>`
- `git diff` print diff between:
    - working tree state and staged state: diff between index and working tree:
    - staged state and commit: diff between commit tree and index
    - commit and commit: diff between both commit trees
    - two blobs or two files 
- `git blame` show revision, author, date of each modified line in file

### Distributed Commands
- `git fetch [<repostory>] [<branch>]` Only works if a remote is defined in .git\config.ini or one is specified. By default, fetches all refs (branches and tags) and their objects from remote (can also specify specific refs to fetch). adds refs to .git\refs\remotes\<remote>\<branch> (remote name defaults to 'origin'). refs that are fetched and objects they point at are added to .git\FETCH_HEAD. A remote can be a URL, SSH, or another dir (local or network shared). Sample of FETCH_HEAD:
```
abc123 branch 'main' of origin
def456 branch 'develop' of origin
```
- `git clone <repository>` = init + fetch + checkout. init a repo, create remote (default to 'origin'). fetch objects from remote. checkout local branch and set its upstream branch in .git\config.ini and create remote entry:
```
[remote "origin"]
    url = https://github.com/gordnzhou/gordit.git
    fetch = +refs/heads/*:refs/remotes/origin/*
[branch "main"]
    remote = origin
    merge = refs/heads/main
```
- `git pull` = fetch + merge. if on tracking branch, fetches only from upstream branch (usually origin/main), which updates FETCH_HEAD and merges commit in FETCH_HEAD into current branch. Otherwise, need to specify remote and branch to pull from.
- `git push` if on tracking branch, finds local commits that exist in local but not in remote upstream branch, sends them and updates remote repo. Otherwise need to specify remote and branch to push to (`-u` sets local branch's upstream to that remote in .git\config.ini). 
- `git remote add/rename/remove` 