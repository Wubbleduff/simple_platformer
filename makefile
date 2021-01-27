
INTERMEDIATE=.\intermediate

DEBUG_RUN_TREE=.\run_trees\debug

OPTIMIZED_DEBUG_RUN_TREE=.\run_trees\optimized_debug

RELEASE_RUN_TREE=.\run_trees\release

EXE=engine.exe

PDB=engine.pdb

SOURCE=\
src\platform_windows\main.cpp \
src\platform_windows\platform.cpp \
src\platform_windows\network.cpp \
src\platform_windows\graphics.cpp \
src\platform_windows\shader.cpp \
src\game.cpp \
src\logging.cpp \
src\game_math.cpp \
src\data_structures.cpp \
src\algorithms.cpp \
src\game_console.cpp \
src\levels.cpp \
src\serialization.cpp \
lib\imgui\imgui.cpp \
lib\imgui\imgui_demo.cpp \
lib\imgui\imgui_draw.cpp \
lib\imgui\imgui_widgets.cpp \
lib\imgui\examples\imgui_impl_opengl3.cpp \
lib\imgui\examples\imgui_impl_win32.cpp

INCLUDE_DIRS=/I"src" /I"lib\glew-2.1.0\include" /I"lib\imgui" /I"lib\stb"

LIBS=user32.lib gdi32.lib shell32.lib opengl32.lib Ws2_32.lib lib\glew-2.1.0\lib\Release\x64\glew32.lib

# DLLs
DLL_GLEW=lib\glew-2.1.0\bin\Release\x64\glew32.dll

DEBUG_MACROS=/DDEBUG
OPTIMIZED_DEBUG_MACROS=/DDEBUG
RELEASE_MACROS=

DEBUG_COMPILE_FLAGS=/c /Zi /EHsc /Fo$(INTERMEDIATE)\ $(DEBUG_MACROS)
DEBUG_LINK_FLAGS=/DEBUG:FULL /OUT:"$(INTERMEDIATE)\$(EXE)"

OPTIMIZED_DEBUG_COMPILE_FLAGS=/c /Z7 /EHsc /Fo$(INTERMEDIATE)\ $(DEBUG_MACROS)
OPTIMIZED_DEBUG_LINK_FLAGS=/DEBUG:FULL /OUT:"$(INTERMEDIATE)\$(EXE)"

RELEASE_COMPILE_FLAGS=/c /O2 /EHsc /Fo$(INTERMEDIATE)\ $(RELEASE_MACROS)
RELEASE_LINK_FLAGS=/DEBUG:NONE /OUT:"$(INTERMEDIATE)\$(EXE)"

debug: | intermediate $(DEBUG_RUN_TREE)
	cl $(DEBUG_COMPILE_FLAGS) $(INCLUDE_DIRS) $(SOURCE)
	link $(DEBUG_LINK_FLAGS) $(LIBS) $(INTERMEDIATE)\*.obj
	xcopy /Y $(INTERMEDIATE)\$(EXE) $(DEBUG_RUN_TREE)
	xcopy /Y $(INTERMEDIATE)\$(PDB) $(DEBUG_RUN_TREE)
	xcopy /Y $(DLL_GLEW) $(DEBUG_RUN_TREE)
	xcopy /Y /E assets $(DEBUG_RUN_TREE)\assets\

optimized_debug: | intermediate $(OPTIMIZED_DEBUG_RUN_TREE)
	cl $(OPTIMIZED_DEBUG_COMPILE_FLAGS) $(INCLUDE_DIRS) $(SOURCE)
	link $(OPTIMIZED_DEBUG_LINK_FLAGS) $(LIBS) $(INTERMEDIATE)\*.obj
	xcopy /Y $(INTERMEDIATE)\$(EXE) $(OPTIMIZED_DEBUG_RUN_TREE)
	xcopy /Y $(INTERMEDIATE)\$(PDB) $(OPTIMIZED_DEBUG_RUN_TREE)
	xcopy /Y $(DLL_GLEW) $(OPTIMIZED_DEBUG_RUN_TREE)
	xcopy /Y /E assets $(OPTIMIZED_DEBUG_RUN_TREE)\assets\

release: | intermediate $(RELEASE_RUN_TREE)
	cl $(RELEASE_COMPILE_FLAGS) $(INCLUDE_DIRS) $(SOURCE)
	link $(RELEASE_LINK_FLAGS) $(LIBS) $(INTERMEDIATE)\*.obj
	xcopy /Y $(INTERMEDIATE)\$(EXE) $(RELEASE_RUN_TREE)
	xcopy /Y $(DLL_GLEW) $(RELEASE_RUN_TREE)
	xcopy /Y /E assets $(RELEASE_RUN_TREE)\assets\

clean: | intermediate
	del $(INTERMEDIATE)\*.obj
	del $(INTERMEDIATE)\*.exe
	del $(INTERMEDIATE)\*.pdb
	del $(INTERMEDIATE)\*.ilk


# Building directory rules
intermediate:
	mkdir $(INTERMEDIATE)

$(DEBUG_RUN_TREE): | run_trees
	mkdir $(DEBUG_RUN_TREE)

$(OPTIMIZED_DEBUG_RUN_TREE): | run_trees
	mkdir $(OPTIMIZED_DEBUG_RUN_TREE)

$(RELEASE_RUN_TREE): | run_trees
	mkdir $(RELEASE_RUN_TREE)

run_trees :
	mkdir run_trees

