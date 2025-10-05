编译指令

下载项目
git clone https://github.com/TheLemonTeaa/muduo-core.git

进入到muduo-core文件
cd muduo-core

创建build文件夹，并且进入build文件:
mkdir build && cd build

然后生成可执行程序文件：
cmake .. && make -j${nproc}

运行程序，执行可执行文件
 ./testserver
