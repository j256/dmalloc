#!/bin/sh
#
# Shell script if you are going to package the library up for release
#

set -e

cwd=`pwd`

# run tests
echo "Running tests"
rm -rf dmalloc
# clone our repo
git clone git@github.com:j256/dmalloc.git
cd dmalloc
mkdir tmp
cd tmp
../configure
make all heavy
dmalloc

echo "----------------------------------------------------------------"
echo "Building release"

cd $cwd
rm -rf dmalloc
# clone our repo
git clone git@github.com:j256/dmalloc.git
# clear our dot files not suitable for release
rm -rf dmalloc/.git dmalloc/.gitignore dmalloc/.circleci

version=`grep dmalloc_version version.h | cut -f2 -d\"`
dir=dmalloc-$version
echo "Building release directory: $dir"
rm -rf $dir
mv dmalloc $dir

dest=$dir.tar.gz
rm -f $dest
tar -cf - $dir | gzip -9 > $dest
echo "Created $dest"
