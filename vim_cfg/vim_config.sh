#!/bin/sh
#generate tag file for lookupfile plugin
echo -e "!_TAG_FILE_SORTED\t2\t/2=foldcase/" > filenametags
#find . -not -regex '.*\.\(png\|gif\)' -type f -printf "%f\t%p\t1\n" | sort -f >> filenametags
find .  -regex '.*\.\(png\|gif\|c\|h\|mk\|s\|S\|cc\|cpp\|java\|jpg\|xml\|conf\)\|.*\(Makefile\|Kconfig\)' -type f -printf "%f\t%p\t1\n" | sort -f >> filenametags

find . -name "*.h" -o -name "*.mk" -o -name "*.c" -o -name "*.s" -o -name "*.S" -o -name "*.cpp" -o -name "*.java" -o -name "*.cc" > cscope.files
cscope -Rbkq -i cscope.files
ctags -R *
