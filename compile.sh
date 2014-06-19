#!/system/bin/bash

cd files/
gcc -o shell Shell.c Parser.c Execute.c Tools.c -lreadline       
./shell


