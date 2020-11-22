#!/bin/sh
#
# Shell script if you are going to package the library up for release
#

set -e

cwd=`pwd`
version=`grep dmalloc_version version.h | cut -f2 -d\"`
dir=dmalloc-$version

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

dest=$dir.tar.gz
rm -f $dest
tar -cf - $dir | gzip -9 > $dest
echo "Created $dest"
