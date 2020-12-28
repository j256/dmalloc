#!/bin/sh
#
# Shell script if you are going to package the library up for release
#

cwd=`pwd`
version=`grep dmalloc_version version.h | cut -f2 -d\"`
dir=dmalloc-$version

head -1 ChangeLog.txt | grep -q $version
if [ $? -ne 0 ]; then
    echo "First line of ChangeLog.txt does not include version $version"
    head -1 ChangeLog.txt
    exit 1
fi
grep -q "dmalloc_version Version $version" dmalloc.texi
if [ $? -ne 0 ]; then
    echo "dmalloc.texi does not include version $version"
    grep -q "dmalloc_version " dmalloc.texi
    exit 1
fi
grep -q "Version: $version" dmalloc.spec
if [ $? -ne 0 ]; then
    echo "dmalloc.spec does not include version $version"
    grep "Version: " dmalloc.spec
    exit 1
fi

set -e

# run tests
echo "Running tests"
rm -rf $dir
# clone our repo
git clone git@github.com:j256/dmalloc.git $dir
mkdir $dir/tmp
cd $dir/tmp
../configure
make all heavy
dmalloc

echo "----------------------------------------------------------------"
echo "Building release"
cd $cwd

echo "Building release directory: $dir"
rm -rf $dir
# clone our repo
git clone git@github.com:j256/dmalloc.git $dir
# clear our dot files not suitable for release
rm -rf $dir/.git $dir/.gitignore $dir/.circleci

dest=$dir.tgz
rm -f $dest
tar -cf - $dir | gzip -9 > $dest
echo "Created $dest"
