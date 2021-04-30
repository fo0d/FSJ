# FSJ

FSJ - File Splitter & Joiner v.009_6226 <br>
by Yuriy Y. Yermilov.<br>
Open-Source, Please read the disclaimer in fsj.c<br>
Contact info included.<br>

-h [print this message]<br>
-n<parts> [define # of parts to split file into]<br>
-s<part_size> [splits file into file_size/<your value (in bytes)>]<br>
-j <name_of_join_file.fsj> [joins files back together]<br><br>
To split file into 10 parts (at most):<br>
Example [#1]: fsj -n10 FILE.EXT<br><br>
To splits file into N parts 512 bytes each (at most):<br>
Example [#2]: fsj -s512 FILE.EXT<br><br>
To join files specified in FILE_name.fsj into file named <name>.<br>
Example [#3]: fsj -j FILE_name.fsj<br>
