#!/system/bin/sh

# Fix directory ownership and permissions on an Android device.
# This is (only) used by 'home2l-adb' for installing a new 'etc'
# directory.

DIR="$1"
UID="$2"

rm -fr `find $DIR -type l`    # remove symlinks (not supported)
chown $UID:$UID `find $DIR`   # assert that the group has the same name as the user
chmod 700       `find $DIR`   # restrict access
