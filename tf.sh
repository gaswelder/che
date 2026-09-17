tmp=/tmp/chefmt
cargo run fmt "$1" > $tmp
diff "$1" >/dev/null "$tmp" && echo "$1" identical || meld "$1" $tmp
