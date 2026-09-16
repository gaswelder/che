tmp=/tmp/chefmt
cargo run fmt "$1" > $tmp && meld "$1" $tmp
