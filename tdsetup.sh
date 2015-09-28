#!/bin/bash
# tdsetup.sh - initialise a set of test data for testing
# oldfiles

td=testdata
tl=~/testlinks
# remove any existing crap
if [[ -d $td ]]; then
    rm -rf $td
fi
if [[ -d $tl ]]; then
    rm -rf $tl
fi
if [[ -h "$td"/ne2 ]]; then
    rm "$td"/ne2
fi
mkdir "$tl"
sync
#create the test data anew
cp -a financedir $td
cp -a progs $td
#now some symlinks
touch "$tl"/noent
ln -s "$tl"/noent "$tl"/ne1 # a link out in linktest dir
ln -s "$tl"/ne1 "$td"/ne2 # a chain of symlinks 2 items long
rm "$tl"/noent  # now this symlink chain is a dangler
# circular symlink chain
touch "$tl"/circular
ln -s "$tl"/circular "$td"/circular
rm "$tl"/circular
ln -s "$td"/circular "$tl"/circular
# now a symlink to an oldfile
touch "$tl"/oldy
./utimefu 20030101 "$tl"/oldy
# make a 3 link chain
ln -s "$tl"/oldy "$tl"/ol1
ln -s "$tl"/ol1 "$tl"/ol2
ln -s "$tl"/ol2 "$td"/ol3




