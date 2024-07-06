find . -type f -name "*.cpp" -or -name "*.h" | xargs wc -l | awk '{sum += $1} END {print sum}'
