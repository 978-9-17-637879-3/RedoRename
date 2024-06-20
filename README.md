# RedoRename (WIP)

Sync file renames between copies of a directory.

Imagine you create a clone of a directory of records, and then move around some files in the source. You can sync those changes by deleting paths that don't match on the destination and adding the new ones, but that's wasteful. Instead, RedoRename allows you to scan file contents themselves and perform the same rename actions on the destination.

TODO:
* Database comparison
* Comparison -> rename action
* Implementation of help, verbose, and dry-run