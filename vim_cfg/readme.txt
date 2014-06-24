sudo apt-get install cscope ctags vim

1. ctags
在~/vimrc中增加一句
:set tags=/home/ubuntu/android2.2-r941/tags
ctags -R *

自动补全功能(需要ctags支持)
    在~/vimrc中增加一句
    :set completeopt=longest,menu

2. cscope
    find . -name "*.h" -o -name "*.mk" -o -name "*.c" -o -name "*.s" -o -name
    "*.S" -o -name "*.cpp" -o -name "*.java" -o -name "*.cc" > cscope.files
    cscope -Rbkq -i cscope.files
    在~/.vimrc中增加一句

    :set cst
    :set tags=/work/uboot-r1031/tags
    :cs add /work/uboot-r1031/cscope.out /work/uboot-r1031/

    nmap <C-\>s :cs find s <C-R>=expand("<cword>")<CR><CR>
    nmap <C-\>g :cs find g <C-R>=expand("<cword>")<CR><CR>
    nmap <C-\>c :cs find c <C-R>=expand("<cword>")<CR><CR>
    nmap <C-\>t :cs find t <C-R>=expand("<cword>")<CR><CR>
    nmap <C-\>e :cs find e <C-R>=expand("<cword>")<CR><CR>
    nmap <C-\>f :cs find f <C-R>=expand("<cfile>")<CR><CR>
    nmap <C-\>i :cs find i <C-R>=expand("<cfile>")<CR><CR>
    nmap <C-\>d :cs find d <C-R>=expand("<cword>")<CR><CR>
    cscope快捷方式的用法：先按下crtl+\松开，再快速按下s,g,c等。

0 或 s 查找本C语言符号，宏定义，变量等(可以跳过注释) 
    1 或 g 查找本定义 
    2 或 d 查找本函数调用的函数 
    3 或 c 查找调用本函数的函数 
    4 或 t 查找本变量在哪赋值
    6 或 e 查找本 egrep 模式 
    7 或 f 查找本文件 
    8 或 i 查找包含本文件的文件 

3. lookupfile
    lookupfile的安装方法如下：
    a.下载lookupfile和genutils， 后者是lookupfile依赖的一个通用函数库。
    b. 解压这两个压缩包，解压到Linux“~/.vim下”，
    c.进入Vim运行时环境下的“doc”目录，用Vim打开任一文件，输入命令“:helptags
    .”并回 车。
    至此，lookupfile的安装已经完成。但要使用它提升工作效率，还需要做些额外的工作，毕竟你总不能每次都要到工程根目录下手动生成Tags
    文件，再“:let
    g:LookupFile_TagExpr='"./filenametags"'”，然后每次
    找文件的候还要“:LookupFile”！

#!/bin/sh
#generate tag file for lookupfile plugin
    echo -e "!_TAG_FILE_SORTED\t2\t/2=foldcase/" > filenametags
#find . -not -regex '.*\.\(png\|gif\)' -type f -printf "%f\t%p\t1\n" | sort -f
    >> filenametags
    find . -regex
    '.*\.\(png\|gif\|c\|h\|mk\|s\|S\|cc\|cpp\|java\|jpg\|xml\|conf\)\|.*\(Makefile\|Kconfig\)'
    -type f -printf "%f\t%p\t1\n" | sort -f >> filenametag

    .vimrc 
    """"""""""""""""""""""""""""""
    " lookupfile setting
    """"""""""""""""""""""""""""""
    "let filename=~/work/a20_homlet/lichee/u-boot/filenametags
    let g:LookupFile_MinPatLength = 2 "最少输入2个字符才开始查找
    let g:LookupFile_PreserveLastPattern = 0
    "不保存上次查找的字符串
    let g:LookupFile_PreservePatternHistory = 1 "保存查找历史
    let g:LookupFile_AlwaysAcceptFirst = 1 "回车打开第一个匹配项目
    let g:LookupFile_AllowNewFiles = 0 "不允许创建不存在的文件
    "if filereadable(filename) "设置tag文件的名字
    let g:LookupFile_TagExpr =
    '"/home/brad/work/a20_homlet/lichee/u-boot/filenametags"'
    "endif
    "映射LookupFile为,lk
    nmap <silent> <leader>lk :LUTags<cr>
    "映射LUBufs为,ll
    nmap <silent> <leader>ll :LUBufs<cr>
    "映射LUWalk为,lw
    nmap <silent> <leader>lw :LUWalk<cr>

4. winManager
    Winmanager插件在这里下载：http://vim.sourceforge.net/scripts/script.php?script_id=95
    下载后，把该文件在~/.vim/目录中解压缩，这会把winmanager插件解压到~/.vim/plugin和~/.vim/doc目录中
    .vimrc
    """"""""""""""""""""""""""""""
    " winManager setting
    """"""""""""""""""""""""""""""
    let g:winManagerWindowLayout =
    "BufExplorer,FileExplorer|TagList"
    let g:winManagerWidth = 30
    let g:defaultExplorer = 0
    nmap <C-W><C-F> :FirstExplorerWindow<cr>
    nmap <C-W><C-B> :BottomExplorerWindow<cr>
    nmap <silent> <leader>wm :WMToggle<cr>

5. taglist
    下载taglist.zip文件，解压到$HOME/.vim目录下
    .vimrc
    " taglist
    let Tlist_Show_One_File=1 "只显示当前文件的tags
    let Tlist_WinWidth=40 "设置taglist宽度
    let Tlist_Exit_OnlyWindow=1
    "tagList窗口是最后一个窗口，则退出Vim
    let Tlist_Use_Right_Window=1 "在Vim窗口右侧显示taglist窗口

