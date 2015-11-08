#!/bin/bash

OUROBOROS=$srcdir/../build/src/ouroboros
PROJECT=$srcdir/project-tree.tgz

# Test basic functionality for given engine type - recursive watch with
# nodes update within specified path. This function takes engine type as
# its first argument.
function test_ouroboros_watch {

	TEMPDIR=`mktemp -d`
	PRJ=$TEMPDIR/project
	LOG=$TEMPDIR/project.log

	tar -xzpf $PROJECT -C $TEMPDIR

	$OUROBOROS -vv -E $1 -p $PRJ -r true -u true -l 0.2 -- pwd |tee $LOG &
	PID=$(jobs -p)

	sleep 0.5
	echo "* touch $PRJ"
	touch $PRJ
	sleep 0.5
	echo "* touch $PRJ/share/images/image.png"
	touch $PRJ/share/images/image.png
	sleep 0.5
	echo "* touch $PRJ/share/docs/html/index.html"
	mkdir -p $PRJ/share/docs/html
	touch $PRJ/share/docs/html/index.html
	sleep 0.5
	echo "* touch $PRJ/share/docs/html/contact.html"
	touch $PRJ/share/docs/html/contact.html
	sleep 0.5

	RETVAL=$(grep -c $PWD $LOG)

	kill -9 $PID
	rm -r $TEMPDIR

	if [ $RETVAL != 5 ]; then
		return 1
	fi
}

test_ouroboros_watch pool || exit 1
test_ouroboros_watch inotify || exit 1
