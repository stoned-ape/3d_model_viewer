all: main.exe

cc=clang-cl
# cc=cl
# cc=nvcc

args=-march=native -mavx2 /std:c++20 /O2 /I"glm" /Z7

quat.obj: quat.cpp quat.h common.h makefile
	$(cc) $(args) /c quat.cpp 

las_file.obj: las_file.cpp las_file.h common.h makefile
	$(cc) $(args) /c las_file.cpp 

obj_file.obj: obj_file.cpp obj_file.h common.h makefile
	$(cc) $(args) /c obj_file.cpp 

main.obj: main.cpp quat.h obj_file.h common.h makefile
	$(cc) $(args) /c main.cpp

main.exe: main.obj obj_file.obj las_file.obj quat.obj makefile
	link /DEBUG main.obj obj_file.obj las_file.obj quat.obj /out:main.exe


run: main.exe
	main.exe