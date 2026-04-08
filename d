set -e
clang-format  < $1  > /tmp/zzz
ksdiff $1 /tmp/zzz
