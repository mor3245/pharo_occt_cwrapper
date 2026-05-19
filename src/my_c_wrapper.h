// my_c_wrapper.h
#pragma once

#include <stddef.h>

/* `__declspec(dllexport)` and `__declspec(dllimport)` are Windows-specific annotations that control
   symbol import and export for DLLs.
   A symbol is basically a name that the compiler and linker use to refer to a variable, function, or
   object in the compiled code.
   `__declspec(dllexport)` tells Windows to put this symbol in the DLL's export table.
   `__declspec(dllimport)` tells Windows that this symbol lives in a DLL, and generate code that calls
   it correctly.
     This affects call instruction generation by the compiler.
     This affects performance optimization. */
#if defined(_WIN32) || defined(_WIN64)
#ifdef my_occt_c_wrapper_EXPORTS
#define OCPSt __declspec(dllexport)
#else
#define OCPSt __declspec(dllimport)
#endif
#else
#define OCPSt __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
     /* `extern "C"` compiles the enclosed declarations with C language linkage.
        In the Application Binary Interface (ABI), linkage determines a symbol's name and how the function
        is called. It disables name mangling, which C++ uses for function overloading, namespaces, and
        similar features. */
extern "C" {
#endif

    /* Opaque handle to a BRepBuilderAPI_MakeShape (or any subclass such as BRepPrimAPI_MakeBox /
       BRepPrimAPI_MakeCylinder). The caller never dereferences this pointer directly. */
    typedef void* hBRepBuilderAPI_MakeShape;

    /* Forward-declares the WodenShape struct so callers can write `OCCShape*` without knowing its
       internals. */
    typedef struct WodenShape WodenShape;

    /* Opaque handle that owns both the TopoDS_Shape and its cached tessellation buffers
       (vertices, normals, triangles). Always freed with cxxShapeDelete / cxxFreeShape. */
    typedef WodenShape* hWodenShape;

    /* -------------------------------------------------------------------------
       BRepBuilderAPI_MakeShape — generic builder state
       ------------------------------------------------------------------------- */

       /* Returns 1 if the builder completed successfully, 0 otherwise.
          Safe to call on a null handle (returns 0). */
    OCPSt int cxxIsDone(hBRepBuilderAPI_MakeShape handle);

    /* -------------------------------------------------------------------------
       Box primitives
       ------------------------------------------------------------------------- */

       /* Allocates and builds a BRepPrimAPI_MakeBox for a box of dimensions dx × dy × dz.
          Returns a handle on success, nullptr on failure.
          Caller must eventually call cxxMakeBoxDelete to free the builder. */
    OCPSt hBRepBuilderAPI_MakeShape cxxBoxNew(double dx, double dy, double dz);

    /* Frees the BRepPrimAPI_MakeBox builder created by cxxBoxNew.
       Does NOT free the shape extracted from it. */
    OCPSt void cxxMakeBoxDelete(hBRepBuilderAPI_MakeShape handle);

    /* Convenience wrapper: builds a box, extracts its shape, deletes the builder, and returns the
       resulting hWodenShape. Returns nullptr on failure.
       Caller must free the shape with cxxShapeDelete / cxxFreeShape. */
    OCPSt hWodenShape cxxCreateBox(double dx, double dy, double dz);

    /* -------------------------------------------------------------------------
       Cylinder primitives
       ------------------------------------------------------------------------- */

       /* Allocates and builds a BRepPrimAPI_MakeCylinder for a cylinder of the given radius and height.
          Returns a handle on success, nullptr on failure.
          Caller must eventually call cxxMakeCylinderDelete to free the builder. */
    OCPSt hBRepBuilderAPI_MakeShape cxxCylinderNew(double radius, double height);

    /* Frees the BRepPrimAPI_MakeCylinder builder created by cxxCylinderNew.
       Does NOT free the shape extracted from it. */
    OCPSt void cxxMakeCylinderDelete(hBRepBuilderAPI_MakeShape handle);

    /* Convenience wrapper: builds a cylinder, extracts its shape, deletes the builder, and returns
       the resulting hWodenShape. Returns nullptr on failure.
       Caller must free the shape with cxxShapeDelete / cxxFreeShape. */
    OCPSt hWodenShape cxxCreateCylinder(double radius, double height);

    /* -------------------------------------------------------------------------
       TopoDS_Shape lifetime management
       ------------------------------------------------------------------------- */

       /* Destroys the WodenShape object (shape + all cached mesh buffers). */
    OCPSt void cxxShapeDelete(hWodenShape handle);

    /* Alias for cxxShapeDelete; provided for UFFI callers that prefer a "free" naming convention. */
    OCPSt void cxxFreeShape(hWodenShape handle);

    /* Extracts the TopoDS_Shape from a finished builder and wraps it in a new WodenShape.
       Returns nullptr if the handle is null or the extraction fails.
       Caller must free the returned handle with cxxShapeDelete / cxxFreeShape. */
    OCPSt hWodenShape cxxToShape(hBRepBuilderAPI_MakeShape handle);

    /* -------------------------------------------------------------------------
       Tessellation — must be called before reading any mesh buffers
       ------------------------------------------------------------------------- */

       /* Tessellates the shape using BRepMesh_IncrementalMesh with the given linear deflection.
          Populates the internal vertex, normal, and triangle buffers.
          Any previously cached mesh data is discarded before re-meshing. */
    OCPSt void cxxTessellate(hWodenShape shape, double deflection);

    /* Returns the number of vertices produced by the last cxxTessellate call.
       Returns 0 if shape is null or not yet tessellated. */
    OCPSt int cxxVertexCount(hWodenShape shape);

    /* Returns the number of triangles produced by the last cxxTessellate call.
       Returns 0 if shape is null or not yet tessellated. */
    OCPSt int cxxTriangleCount(hWodenShape shape);

    /* Copies all vertex positions (x,y,z triples) into buffer.
       buffer must be large enough to hold cxxVertexCount(shape) * 3 doubles. */
    OCPSt void cxxGetAllVertices(hWodenShape shape, double* buffer);

    /* Copies all per-vertex normals (nx,ny,nz triples) into buffer.
       buffer must be large enough to hold cxxVertexCount(shape) * 3 doubles. */
    OCPSt void cxxGetAllNormals(hWodenShape shape, double* buffer);

    /* Copies all triangle indices (i0,i1,i2 triples, 0-based) into buffer.
       buffer must be large enough to hold cxxTriangleCount(shape) * 3 ints. */
    OCPSt void cxxGetAllTriangles(hWodenShape shape, int* buffer);

    /* -------------------------------------------------------------------------
       Zero-copy mesh buffer access (UFFI / FFI pointer API)
       Returned pointers remain valid until the next cxxTessellate call or the shape is freed.
       ------------------------------------------------------------------------- */

       /* Returns a direct pointer to the internal vertex buffer (x,y,z triples).
          Returns nullptr if the shape is null or has no vertices. */
    OCPSt const double* cxxGetVertices(hWodenShape shape);

    /* Returns the number of vertices (i.e. buffer length / 3). */
    OCPSt int cxxGetVertexCount(hWodenShape shape);

    /* Returns the total number of doubles in the vertex buffer (vertexCount * 3). */
    OCPSt int cxxGetVertexBufferLength(hWodenShape shape);

    /* Returns a direct pointer to the internal normal buffer (nx,ny,nz triples).
       Returns nullptr if the shape is null or has no normals. */
    OCPSt const double* cxxGetNormals(hWodenShape shape);

    /* Returns the number of normals (i.e. buffer length / 3). */
    OCPSt int cxxGetNormalCount(hWodenShape shape);

    /* Returns the total number of doubles in the normal buffer (normalCount * 3). */
    OCPSt int cxxGetNormalBufferLength(hWodenShape shape);

    /* Returns a direct pointer to the internal triangle index buffer (i0,i1,i2 triples, 0-based).
       Returns nullptr if the shape is null or has no triangles. */
    OCPSt const int* cxxGetTriangles(hWodenShape shape);

    /* Returns the number of triangles (i.e. index count / 3). */
    OCPSt int cxxGetTriangleCount(hWodenShape shape);

    /* Returns the total number of ints in the triangle index buffer (triangleCount * 3). */
    OCPSt int cxxGetTriangleIndexCount(hWodenShape shape);

    /* -------------------------------------------------------------------------
       Boolean operations
       ------------------------------------------------------------------------- */

       /* Computes hShape1 minus hShape2 using BRepAlgoAPI_Cut and returns the result as a new shape.
          Returns nullptr if either input is null or the operation fails.
          Input shapes are not modified or freed; caller owns the returned handle. */
    OCPSt hWodenShape cxxCut(hWodenShape hShape1, hWodenShape hShape2);

    /* -------------------------------------------------------------------------
       Debug memory tracking (Debug builds only — entirely absent in Release)
       ------------------------------------------------------------------------- */

#ifdef _DEBUG
       /* Records a CRT heap snapshot into the global checkpoint state.
          Call this before the code section you want to monitor for leaks. */
    OCPSt void cxxMemoryCheckpoint();

    /* Computes the heap difference since the last cxxMemoryCheckpoint call and, if allocations
       differ, dumps statistics and all outstanding objects to the debug output. */
    OCPSt void cxxDumpMemoryDifference();

    /* Returns 1 if the heap state differs from the last checkpoint, 0 otherwise.
       Useful for asserting no-leak conditions in tests. */
    OCPSt int cxxHasMemoryDifference();
#endif /* _DEBUG */

#ifdef __cplusplus
}
#endif