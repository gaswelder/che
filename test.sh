for i in test/*; do
	echo $i
	php mc.php $i || exit 1
done
