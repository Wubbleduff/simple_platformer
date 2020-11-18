
INTERMEDIATE=./intermediate/

DEBUG_RUN_TREE=./run_trees/debug/
OPTIMIZED_DEBUG_RUN_TREE=./run_trees/optimized_debug/
RELEASE_RUN_TREE=./run_trees/release/

EXE=engine.exe

PDB=engine.pdb

#SOURCE=src/main.cpp src/engine.cpp src/memory.cpp src/my_algorithms.cpp src/input.cpp src/renderer.cpp src/shader.cpp src/level.cpp lib/imgui/imgui.cpp lib/imgui/imgui_demo.cpp lib/imgui/imgui_draw.cpp lib/imgui/imgui_widgets.cpp lib/imgui/examples/imgui_impl_opengl3.cpp lib/imgui/examples/imgui_impl_win32.cpp
#SOURCE=compile_unit.cpp lib/imgui/imgui.cpp lib/imgui/imgui_demo.cpp lib/imgui/imgui_draw.cpp lib/imgui/imgui_widgets.cpp lib/imgui/examples/imgui_impl_opengl3.cpp lib/imgui/examples/imgui_impl_win32.cpp
SOURCE=compile_unit.cpp


INCLUDE_DIRS=/I"src" /I"lib/glew-2.1.0/include" /I"lib/imgui" /I"lib/stb"

LIBS=user32.lib gdi32.lib shell32.lib opengl32.lib lib/glew-2.1.0/lib/Release/x64/glew32.lib

# DLLs
DLL_GLEW=lib/glew-2.1.0/bin/Release/x64/glew32.dll

DEBUG_MACROS=
OPTIMIZED_DEBUG_MACROS=
RELEASE_MACROS=

DEBUG_COMPILE_FLAGS=/c /Z7 /EHsc /Fo$(INTERMEDIATE) $(DEBUG_MACROS)
DEBUG_LINK_FLAGS=/DEBUG:FULL /OUT:$(INTERMEDIATE)$(EXE)

OPTIMIZED_DEBUG_COMPILE_FLAGS=/c /Z7 /EHsc /Fo$(INTERMEDIATE) $(DEBUG_MACROS)
OPTIMIZED_DEBUG_LINK_FLAGS=/DEBUG:FULL /OUT:$(INTERMEDIATE)$(EXE)

RELEASE_COMPILE_FLAGS=/c /O2 /EHsc /Fo$(INTERMEDIATE) $(RELEASE_MACROS)
RELEASE_LINK_FLAGS=/DEBUG:NONE /OUT:$(INTERMEDIATE)$(EXE)

debug: | intermediate $(DEBUG_RUN_TREE)
	cl $(DEBUG_COMPILE_FLAGS) $(INCLUDE_DIRS) $(SOURCE)
	link $(DEBUG_LINK_FLAGS) $(LIBS) $(INTERMEDIATE)*.obj
	cp -f $(INTERMEDIATE)$(EXE) $(DEBUG_RUN_TREE)
	cp -f $(INTERMEDIATE)$(PDB) $(DEBUG_RUN_TREE)
	cp -f $(DLL_GLEW) $(DEBUG_RUN_TREE)
	cp -rf textures/ $(DEBUG_RUN_TREE)
	cp -rf shaders/ $(DEBUG_RUN_TREE)

optimized_debug: | intermediate $(OPTIMIZED_DEBUG_RUN_TREE)
	cl $(OPTIMIZED_DEBUG_COMPILE_FLAGS) $(INCLUDE_DIRS) $(SOURCE)
	link $(OPTIMIZED_DEBUG_LINK_FLAGS) $(LIBS) $(INTERMEDIATE)*.obj
	cp -f $(INTERMEDIATE)$(EXE) $(OPTIMIZED_DEBUG_RUN_TREE)
	cp -f $(INTERMEDIATE)$(PDB) $(OPTIMIZED_DEBUG_RUN_TREE)
	cp -f $(DLL_GLEW) $(OPTIMIZED_DEBUG_RUN_TREE)
	cp -rf textures/ $(OPTIMIZED_DEBUG_RUN_TREE)
	cp -rf shaders/ $(OPTIMIZED_DEBUG_RUN_TREE)

release: | intermediate $(RELEASE_RUN_TREE)
	cl $(RELEASE_COMPILE_FLAGS) $(INCLUDE_DIRS) $(SOURCE)
	link $(RELEASE_LINK_FLAGS) $(LIBS) $(INTERMEDIATE)*.obj
	cp -f $(INTERMEDIATE)$(EXE) $(RELEASE_RUN_TREE)
	cp -f $(DLL_GLEW) $(RELEASE_RUN_TREE)
	cp -rf textures/ $(RELEASE_RUN_TREE)
	cp -rf shaders/ $(RELEASE_RUN_TREE)

imgui: | intermediate
	cl $(OPTIMIZED_DEBUG_COMPILE_FLAGS) /I"lib/imgui" /I"lib/glew-2.1.0/include" /D"IMGUI_IMPL_OPENGL_LOADER_GLEW" lib/imgui/imgui.cpp lib/imgui/imgui_demo.cpp lib/imgui/imgui_draw.cpp lib/imgui/imgui_widgets.cpp lib/imgui/examples/imgui_impl_opengl3.cpp lib/imgui/examples/imgui_impl_win32.cpp
	link $(DEBUG_LINK_FLAGS) $(LIBS) $(INTERMEDIATE)*.obj


clean: | intermediate
	rm -f $(INTERMEDIATE)*.obj
	rm -f $(INTERMEDIATE)*.exe
	rm -f $(INTERMEDIATE)*.pdb
	rm -f $(INTERMEDIATE)*.ilk


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

