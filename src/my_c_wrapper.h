// my_c_wrapper.h
#pragma once

#include <stddef.h>

/* `__declspec(dllexport)` and `__declspec(dllimport)` are Windows-specific annotation that control symbol import and export for DLLs.
	   A symbol is basically a name that the compiler and linker use to refer to a variable, function, or object in the compiled code.
   `__declspec(dllexport)` tells Windows to put this symbol in the DLL's export table.
   `__declspec(dllimport)` tells Windows that this symbol lives in a DLL, and generate code that calls it correctly.
		This affects call instruction generation by the compiler.
		This affects performance optimization. */
#if defined(_WIN32) || defined(_WIN64)
	#ifdef box_c_wrapper_EXPORTS
		#define OCPSt __declspec(dllexport)
	#else
		#define OCPSt __declspec(dllimport)
	#endif
#else
	#define OCPSt __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
		/* `extern` complies the enclosed declarations with C language linkage.
			In the Application Binary Interface (ABI), linkage determines a symbol’s name and how the function is called.
			   It disables name mangling, which C++ uses for function overloading, namespaces, and similar features. */
extern "C" {
#endif

	typedef void* hBRepBuilderAPI_MakeShape; // opaque handle that can be the childof BRepBuilderAPI_MakeShape
	typedef void* hTopoDS_Shape; // opaque handle that owns shape + cached mesh buffers

	// BRepBuilderAPI_MakeShape functions
	OCPSt int cxxIsDone(hBRepBuilderAPI_MakeShape handle); // backward-compatible alias

	// Box functions
	OCPSt hBRepBuilderAPI_MakeShape cxxBoxNew(double dx, double dy, double dz);
	OCPSt void cxxMakeBoxDelete(hBRepBuilderAPI_MakeShape handle);

	// Cylinder functions
	OCPSt hBRepBuilderAPI_MakeShape cxxCylinderNew(double radius, double height);
	OCPSt void cxxMakeCylinderDelete(hBRepBuilderAPI_MakeShape handle);

	// TopoDS_Shape functions
	OCPSt void cxxShapeDelete(hTopoDS_Shape handle);
	OCPSt hTopoDS_Shape cxxToShape(hBRepBuilderAPI_MakeShape handle);

	// Tessellation + mesh access (call cxxTessellate before reading buffers)
	OCPSt void cxxTessellate(hTopoDS_Shape shape, double deflection);
	OCPSt int cxxVertexCount(hTopoDS_Shape shape);
	OCPSt int cxxTriangleCount(hTopoDS_Shape shape);
	OCPSt void cxxGetAllVertices(hTopoDS_Shape shape, double* buffer);
	OCPSt void cxxGetAllNormals(hTopoDS_Shape shape, double* buffer);
	OCPSt void cxxGetAllTriangles(hTopoDS_Shape shape, int* buffer);

	// Boolean Operations
	OCPSt hTopoDS_Shape cxxCut(hTopoDS_Shape& hShape1, hTopoDS_Shape& hShape2);

	OCPSt void cxxMemoryCheckpoint();
	OCPSt void cxxDumpMemoryDifference();
	OCPSt int cxxHasMemoryDifference();

#ifdef __cplusplus
}
#endif
