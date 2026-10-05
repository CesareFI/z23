#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton - Apache License 2.0
# purpose: admit an existing-package patch only after a compiled red/green test.
# Usage: commons_fix_admit.sh PATCH PACKAGE | --selftest
# Baseline is the caller repository's HEAD, never its dirty/index contents.
# This is a local build/test check, not a sandbox or package safety certificate.
set -u
export LC_ALL=C
tool_root=$(cd "$(dirname "$0")/../.." && pwd) || exit 1
refuse() { printf 'REFUSE %s\n' "$1"; exit 1; }

selftest() (
    local fixture script result rc patch case_name
    script=$(cd "$(dirname "$0")" && pwd)/$(basename "$0")
    fixture=$(mktemp -d) || return 1
    trap 'rm -rf "$fixture"' EXIT
    mkdir -p "$fixture/contexts/commons/packages/tiny/"{src,include,tests}
    mkdir -p "$fixture/contexts/commons/packages/leaf/"{src,include}
    cd "$fixture" || return 1
    git init -q || return 1
    printf 'int helper(void); int value(void) { return helper(); }\n' > contexts/commons/packages/tiny/src/tiny.c
    printf 'int value(void); int main(void) { return value() != 0; }\n' > contexts/commons/packages/tiny/tests/test_tiny.c
    printf '{"name":"tiny/tiny","dependencies":[{"name":"leaf/leaf"}]}\n' > contexts/commons/packages/tiny/zcode-package.json
    printf 'int helper(void) { return 0; }\n' > contexts/commons/packages/leaf/src/leaf.c
    printf '{"name":"leaf/leaf","dependencies":[]}\n' > contexts/commons/packages/leaf/zcode-package.json
    printf 'outside\n' > outside
    git add . || return 1
    git -c user.name=Fixture -c user.email=fixture@example.invalid -c core.hooksPath=/dev/null commit -qm baseline || return 1
    for case_name in admit assertion outside adds deletes no-test passes compile-red compile-green fails invalid-dependency; do
        git restore . || return 1
        rm -f contexts/commons/packages/tiny/src/new.c
        printf 'int value(void) { return 1; }\n' > contexts/commons/packages/tiny/src/tiny.c
        printf 'int value(void); int main(void) { return value() != 1; }\n' > contexts/commons/packages/tiny/tests/test_tiny.c
        case "$case_name" in
            outside) printf 'changed\n' > outside; result='REFUSE touches-files-outside-package';;
            adds) printf 'int other;\n' > contexts/commons/packages/tiny/src/new.c; git add -N contexts/commons/packages/tiny/src/new.c || return 1; result='REFUSE adds-file';;
            deletes) rm contexts/commons/packages/tiny/src/tiny.c; result='REFUSE changes-file-type-or-name';;
            no-test) git restore contexts/commons/packages/tiny/tests; result='REFUSE no-test-change';;
            passes) printf 'int main(void) { return 0; }\n' > contexts/commons/packages/tiny/tests/test_tiny.c; result='REFUSE test-passes-without-fix';;
            compile-red) printf 'invalid C\n' > contexts/commons/packages/tiny/tests/test_tiny.c; result='REFUSE does-not-compile';;
            compile-green) printf 'invalid C\n' > contexts/commons/packages/tiny/src/tiny.c; result='REFUSE does-not-compile';;
            fails) printf 'int value(void) { return 2; }\n' > contexts/commons/packages/tiny/src/tiny.c; result='REFUSE test-fails-with-fix';;
            admit) result=ADMIT;;
            assertion) printf '#include <assert.h>\nint value(void); int main(void) { assert(value() == 1); return 0; }\n' > contexts/commons/packages/tiny/tests/test_tiny.c; result=ADMIT;;
            invalid-dependency) printf '{"name":"tiny/tiny","dependencies":[{"name":"../outside"}]}\n' > contexts/commons/packages/tiny/zcode-package.json; result='REFUSE does-not-compile';;
        esac
        patch="$fixture/item.patch"
        git diff > "$patch" || return 1
        git reset -q || return 1
        # A dirty caller must remain byte-for-byte dirty, with its index intact.
        git diff > "$fixture/before"
        git ls-files --stage > "$fixture/index-before"
        rc=0
        output=$("$script" "$patch" tiny) || rc=$?
        if [[ "$output" != "$result" ]] || [[ "$rc" != "$([[ "$result" == ADMIT ]] && printf 0 || printf 1)" ]]; then
            printf 'selftest %s: expected %s, got %s (exit %s)\n' "$case_name" "$result" "$output" "$rc" >&2
            return 1
        fi
        git diff > "$fixture/after"
        git ls-files --stage > "$fixture/index-after"
        cmp -s "$fixture/before" "$fixture/after" && cmp -s "$fixture/index-before" "$fixture/index-after" || return 1
    done
    printf 'commons-fix-admit selftest: PASS\n'
)
if [[ "${1:-}" == --selftest && $# == 1 ]]; then selftest; exit $?; fi
[[ $# == 2 ]] || refuse usage
patch=$1
package=$2
[[ "$package" =~ ^[a-z][a-z0-9_]*$ ]] || refuse invalid-package
[[ -s "$patch" ]] || refuse missing-patch
root=$(git rev-parse --show-toplevel 2>/dev/null) || refuse no-repository
scratch=$(mktemp -d) || refuse temporary-copy-failed
trap 'rm -rf "$scratch"' EXIT
cp "$patch" "$scratch/patch" || refuse temporary-copy-failed
compiler=${CC:-gcc-14}
if ! command -v "$compiler" >/dev/null 2>&1; then refuse compiler-unavailable; fi
if command -v timeout >/dev/null 2>&1; then
    deadline=timeout
elif command -v gtimeout >/dev/null 2>&1; then
    deadline=gtimeout
else
    refuse timeout-unavailable
fi
# Reuse the repository parser; the candidate cannot replace this helper.
"$compiler" -std=c23 -O1 -I"$tool_root/contexts/commons/packages/zjsonp/include" \
    -I"$tool_root/contexts/commons/packages/zutf8/include" \
    "$tool_root/tools/jsonq.c" "$tool_root/contexts/commons/packages/zjsonp/src/zjsonp.c" \
    "$tool_root/contexts/commons/packages/zutf8/src/zutf8.c" \
    -o "$scratch/jsonq" > "$scratch/jsonq.build" 2>&1 || refuse helper-does-not-compile
patch=$scratch/patch
prefix=contexts/commons/packages/$package
# Ask Git to parse the patch; never infer paths from added hunk text.
git apply --numstat "$patch" > "$scratch/paths" 2> "$scratch/error" || refuse invalid-patch
[[ -s "$scratch/paths" ]] || refuse invalid-patch
while IFS=$'\t' read -r added removed path; do
    [[ "$path" == "$prefix/"* && "$path" != *'/../'* && "$path" != *'/./'* && "$path" != *'//'* ]] 2>/dev/null || refuse touches-files-outside-package
done < "$scratch/paths"
git apply --summary "$patch" > "$scratch/summary" 2> "$scratch/error" || refuse invalid-patch
if grep -q 'create mode' "$scratch/summary"; then refuse adds-file; fi
[[ ! -s "$scratch/summary" ]] || refuse changes-file-type-or-name
mkdir "$scratch/tree" || refuse temporary-copy-failed
git -C "$root" archive HEAD contexts/commons/packages > "$scratch/source.tar" 2> "$scratch/error" || refuse temporary-copy-failed
tar -xf "$scratch/source.tar" -C "$scratch/tree" 2> "$scratch/error" || refuse temporary-copy-failed
cd "$scratch/tree" || refuse temporary-copy-failed
[[ -f "$prefix/tests/test_$package.c" && -f "$prefix/zcode-package.json" ]] || refuse invalid-package
# Refuse symlinks before applying or compiling anything in this private copy.
[[ -z "$(find contexts/commons/packages -type l -print)" ]] || refuse symlink
 git apply --check "$patch" 2> "$scratch/error" || refuse patch-does-not-apply
if ! awk -F '\t' -v p="$prefix/tests/" 'index($3,p)==1 {found=1} END {exit !found}' "$scratch/paths"; then refuse no-test-change; fi
build_test() {
    local name=$1 dep manifest token count i
    local -a includes=() sources=() pending=("$package") seen=()
    while ((${#pending[@]})); do
        dep=${pending[0]}; pending=("${pending[@]:1}")
        [[ " ${seen[*]} " != *" $dep "* ]] || continue
        seen+=("$dep")
        manifest=contexts/commons/packages/$dep/zcode-package.json
        [[ -f "$manifest" ]] || return 1
        includes+=("-Icontexts/commons/packages/$dep/include")
        for token in contexts/commons/packages/"$dep"/src/*.c; do
            [[ -f "$token" ]] && sources+=("$token")
        done
        count=$("$scratch/jsonq" count dependencies < "$manifest") || return 1
        [[ "$count" =~ ^[0-9]+$ && ${#count} -le 3 ]] || return 1
        for ((i=0; i<count; i++)); do
            token=$("$scratch/jsonq" get "dependencies[$i].name" < "$manifest") || return 1
            [[ "$token" =~ ^[a-z][a-z0-9_]*/[a-z][a-z0-9_]*$ ]] || return 1
            [[ "${token%%/*}" == "${token#*/}" ]] || return 1
            pending+=("${token%%/*}")
        done
    done
    "$compiler" -std=c23 -O1 -Wall -Wextra -Werror -pedantic "${includes[@]}" "${sources[@]}" "$prefix/tests/test_$package.c" -lm -o "$scratch/$name" > "$scratch/$name.build" 2>&1
}
git apply --include="$prefix/tests/*" "$patch" 2> "$scratch/error" || refuse patch-does-not-apply
build_test red || refuse does-not-compile
rc=0
"$deadline" -k 5 60 "$scratch/red" > "$scratch/red.run" 2>&1 || rc=$?
[[ $rc != 0 ]] || refuse test-passes-without-fix
case "$rc" in 124|125|126|127|137) refuse test-did-not-complete;; esac
# Re-extract the baseline so overlapping test hunks are applied exactly once.
rm -rf contexts
 tar -xf "$scratch/source.tar" 2> "$scratch/error" || refuse temporary-copy-failed
git apply "$patch" 2> "$scratch/error" || refuse patch-does-not-apply
build_test green || refuse does-not-compile
"$deadline" -k 5 60 "$scratch/green" > "$scratch/green.run" 2>&1 || refuse test-fails-with-fix
printf 'ADMIT\n'
