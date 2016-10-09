for i in test/* prog/*; do
	echo $i
	php mc.php $i || exit 1
done
