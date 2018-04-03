{
D=$(mktemp -d)
pushd $D
git init
for i in `seq 1 10`; do
	echo "txt $i" > file-$i;
	git add file-$i;
	git commit -m "commit $i";
	git tag tag-$i;
done
popd
} &>/dev/null

echo $D
