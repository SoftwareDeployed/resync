#!/usr/bin/env bash
set -euo pipefail

dune_project="${1:-dune-project}"

if [[ ! -f "$dune_project" ]]; then
  echo "dune-project not found: $dune_project" >&2
  exit 1
fi

awk '
  function flush() {
    if (in_pin && name != "" && url != "") {
      print name "\t" url;
      in_pin = 0;
      waiting_url = 0;
      name = "";
      url = "";
    }
  }

  BEGIN {
    in_pin = 0;
    waiting_url = 0;
    name = "";
    url = "";
  }

  /^\(pin[[:space:]]*$/ {
    in_pin = 1;
    waiting_url = 0;
    name = "";
    url = "";
    next;
  }

  in_pin && waiting_url {
    line = $0;
    sub(/^[[:space:]]*"/, "", line);
    sub(/"[[:space:]]*\)+[[:space:]]*$/, "", line);
    url = line;
    waiting_url = 0;
    flush();
    next;
  }

  in_pin && /^[[:space:]]*\(url[[:space:]]*$/ {
    waiting_url = 1;
    next;
  }

  in_pin && /^[[:space:]]*\(url[[:space:]]*"/ {
    line = $0;
    sub(/^[[:space:]]*\(url[[:space:]]*"/, "", line);
    sub(/"\)[[:space:]]*$/, "", line);
    url = line;
    flush();
    next;
  }

  in_pin && /^[[:space:]]*\(name[[:space:]]+/ {
    line = $0;
    sub(/^[[:space:]]*\(name[[:space:]]+/, "", line);
    sub(/\)[[:space:]]*\)*[[:space:]]*$/, "", line);
    name = line;
    flush();
    next;
  }
' "$dune_project" | while IFS=$'\t' read -r package_name package_url; do
  opam pin add "$package_name" "$package_url" --yes --no-action
done
