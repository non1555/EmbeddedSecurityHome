#!/usr/bin/env bash

ui_show_info() {
  local title="$1"
  local message="$2"
  if command -v zenity >/dev/null 2>&1; then
    zenity --info --title="$title" --text="$message" >/dev/null 2>&1 || true
    return
  fi
  if command -v notify-send >/dev/null 2>&1; then
    notify-send "$title" "$message" >/dev/null 2>&1 || true
    return
  fi
  printf "%s\n%s\n" "$title" "$message"
}

ui_show_error() {
  local title="$1"
  local message="$2"
  if command -v zenity >/dev/null 2>&1; then
    zenity --error --title="$title" --text="$message" >/dev/null 2>&1 || true
    return
  fi
  if command -v notify-send >/dev/null 2>&1; then
    notify-send -u critical "$title" "$message" >/dev/null 2>&1 || true
    return
  fi
  printf "ERROR: %s\n%s\n" "$title" "$message" >&2
}

ui_open_file() {
  local file_path="$1"
  if [[ -z "$file_path" ]]; then
    return
  fi
  if command -v xdg-open >/dev/null 2>&1; then
    xdg-open "$file_path" >/dev/null 2>&1 || true
  fi
}
