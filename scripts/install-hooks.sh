#!/usr/bin/env bash
# Install cofi git hooks into .git/hooks/ as symlinks.
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel)"
src_dir="$repo_root/scripts/hooks"
dst_dir="$repo_root/.git/hooks"

mkdir -p "$dst_dir"

for hook in "$src_dir"/*; do
    name="$(basename "$hook")"
    ln -sf "../../scripts/hooks/$name" "$dst_dir/$name"
    chmod +x "$hook"
    echo "installed: $name -> scripts/hooks/$name"
done
