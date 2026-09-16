#!/usr/bin/env bash
# 压测预置：100 个用户 + 各 1 个文件（供下载/预览）+ 1000 个本地上传文件
set -euo pipefail

BASE="${BASE:-http://localhost:8080}"
DIR="${DIR:-/tmp/loadtest}"

mkdir -p "$DIR/upfiles"

# 1000 个本地小文件（上传用，文件名唯一，避免唯一约束误伤）
for i in $(seq 1 1000); do
  printf 'loadtest upload content %s\n' "$i" > "$DIR/upfiles/upload_$i.txt"
done

: > "$DIR/users.csv"
for i in $(seq 1 100); do
  U="load$i"
  # 注册（已存在则忽略失败）
  curl -s -m 10 -X POST "$BASE/api/auth/register" \
    -H 'Content-Type: application/json' \
    -d "{\"username\":\"$U\",\"password\":\"pass1234\"}" >/dev/null || true
  TOKEN=$(curl -s -m 10 -X POST "$BASE/api/auth/login" \
    -H 'Content-Type: application/json' \
    -d "{\"username\":\"$U\",\"password\":\"pass1234\"}" \
    | python3 -c 'import sys,json; print(json.load(sys.stdin)["data"]["token"])')
  # 每个用户传一个内容不同的文件（供下载/预览）
  printf 'seed content for %s\n' "$U" > "$DIR/seed_$i.txt"
  FID=$(curl -s -m 15 -X POST "$BASE/api/files/upload" \
    -H "Authorization: Bearer $TOKEN" \
    -F "file=@$DIR/seed_$i.txt" -F "parentId=0" \
    | python3 -c 'import sys,json; print(json.load(sys.stdin)["data"]["id"])')
  echo "$U,pass1234,$FID" >> "$DIR/users.csv"
done

echo "预置完成：$DIR/users.csv（100 行），$DIR/upfiles（1000 个）"
