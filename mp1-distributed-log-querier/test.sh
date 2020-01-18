#!/bin/sh
./client test_request > test_out

diff test_answer test_out > diff_file
FN=$(awk 'END{print NR}' diff_file)

if [ $FN == '0' ]
then
    echo "right!"
else
    echo "wrong!"
fi
