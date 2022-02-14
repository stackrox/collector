#!/usr/bin/env bash

while read -r f; do
    [[ -f "$f" ]] || continue
    # Only strip executable files that are ELFs, as well as anything that looks like a
    # .so or .a file.
    if [[ -x "$f" ]]; then
        if ! cmp -s -n 4 "$f" <(echo -en '\x7fELF'); then
            continue
        fi
    elif ! [[ "$f" =~ .*\.so(\.|$) || "$f" =~ .*\.a(\.|$) ]]; then
        continue
    fi

    strip --strip-unneeded "$f"
done < <(find /usr/local -type f)
