#!/bin/bash
	if [ -e "./test/test1.txt" ]; then
		echo "test1.txt already made..."
	else
		echo "making test1.txt..."
		./test/filegen.py > ./test/test1.txt
	fi

	if [ -e "./test/test2.txt" ]; then
		echo "test2.txt already made..."
	else
		echo "making test2.txt..."
		./test/filegen.py > ./test/test2.txt
	fi

	if [ -e "./test/test3.txt" ]; then
		echo "test3.txt already made..."
	else
		echo "making test3.txt..."
		./test/filegen.py > ./test/test3.txt
	fi
