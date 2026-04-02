#!/usr/bin/env bash
set -euo pipefail

source_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
repo_root="${WORK_REPO_ROOT:-$source_root}"

if [[ "$repo_root" != "$source_root" ]]; then
  mkdir -p "$repo_root"
  rsync -a --delete \
    --exclude '.git' \
    --exclude '_build' \
    --exclude '_opam' \
    --exclude 'node_modules' \
    "$source_root/" "$repo_root/"
fi

cd "$repo_root"

export GIT_CONFIG_COUNT=1
export GIT_CONFIG_KEY_0=safe.directory
export GIT_CONFIG_VALUE_0="*"

ocaml_version="${OCAML_VERSION:-5.4.0}"
switch_name="${OPAM_SWITCH_NAME:-linux-demo}"
command_name="${1:-check}"

export ECOMMERCE_DOC_ROOT="${ECOMMERCE_DOC_ROOT:-$repo_root/_build/default/demos/ecommerce/ui/src}"
export TODO_DOC_ROOT="${TODO_DOC_ROOT:-$repo_root/_build/default/demos/todo/ui/src}"

setup_toolchain() {
  echo "==> Ensuring opam switch: $switch_name"
  if ! opam switch list --short | grep -Fqx "$switch_name"; then
    opam switch create "$switch_name" "ocaml-base-compiler.$ocaml_version" --yes
  fi

  eval "$(opam env --switch="$switch_name" --set-switch)"

  local switch_prefix
  switch_prefix="$(opam var prefix --switch="$switch_name")"
  ln -sfn "$switch_prefix" "$repo_root/_opam"

  echo "==> Updating opam metadata"
  opam update

  echo "==> Pinning git dependencies"
  opam pin add server-reason-react "git+https://github.com/ml-in-barcelona/server-reason-react.git#main" --yes --no-action
  opam pin add reason-react "git+https://github.com/reasonml/reason-react.git" --yes --no-action
  opam pin add reason-react-ppx "git+https://github.com/reasonml/reason-react.git" --yes --no-action
  opam pin add reason-react-day-picker "git+https://github.com/Software-Deployed/reason-react-day-picker.git#reason-native" --yes --no-action

  echo "==> Installing opam dependencies"
  opam install . --deps-only --yes

  echo "==> Installing pnpm workspace dependencies"
  pnpm install --frozen-lockfile --force
}

run_check() {
  echo "==> Building all demos"
  dune build @all-apps @all-servers
}

run_ecommerce() {
  echo "==> Starting ecommerce demo with hot reload"
  # Initial build
  dune build @ecommerce-app @ecommerce-server
  # Start build watcher in background and server watcher in foreground using pnpm
  pnpm run ecommerce:dune:watch &
  BUILD_PID=$!
  pnpm run ecommerce:watch &
  WATCH_PID=$!
  # Wait for either process to exit
  wait -n $BUILD_PID $WATCH_PID
  EXIT_CODE=$?
  # Kill the other process
  kill $BUILD_PID $WATCH_PID 2>/dev/null || true
  exit $EXIT_CODE
}

run_todo() {
  echo "==> Starting todo demo with hot reload"
  # Initial build
  dune build @todo-app @todo-server
  # Start build watcher in background and server watcher in foreground using pnpm
  pnpm run todo:dune:watch &
  BUILD_PID=$!
  pnpm run todo:watch &
  WATCH_PID=$!
  # Wait for either process to exit
  wait -n $BUILD_PID $WATCH_PID
  EXIT_CODE=$?
  # Kill the other process
  kill $BUILD_PID $WATCH_PID 2>/dev/null || true
  exit $EXIT_CODE
}

setup_toolchain

case "$command_name" in
  check)
    run_check
    ;;
  ecommerce)
    run_ecommerce
    ;;
  todo)
    run_todo
    ;;
  shell)
    exec bash
    ;;
  *)
    shift || true
    exec "$command_name" "$@"
    ;;
esac
