// my_c_wrapper.cpp

// Enable CRT debug heap tracking (must come before any other includes).
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <stdlib.h>

// Own includes
#include "my_c_wrapper.h"

// OpenCASCADE includes
#include <BRepBuilderAPI_Transform.hxx>
#include <gp_Ax1.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepLib_ToolTriangulatedShape.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Tool.hxx>
#include <Poly_Triangulation.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Dir.hxx>

#include <exception>
#include <memory>
#include <vector>

/* Global CRT heap snapshot used by the memory-tracking helpers. */
static _CrtMemState gMemStateStart;

/* Internal representation of a shape together with its tessellated mesh buffers.
   All three vectors are populated (and replaced) by cxxTessellate. */
struct WodenShape
{
    TopoDS_Shape      shape;      // The underlying OCCT topological shape.
    std::vector<double> vertices; // Flat x,y,z triples — one entry per vertex.
    std::vector<double> normals;  // Flat nx,ny,nz triples — one entry per vertex (matches vertices).
    std::vector<int>  triangles;  // Flat i0,i1,i2 triples — 0-based indices into vertices.
};

extern "C" {

    /* -------------------------------------------------------------------------
       BRepBuilderAPI_MakeShape — generic builder state
       ------------------------------------------------------------------------- */

       /* Returns 1 if the builder has completed successfully, 0 otherwise.
          Safe to call on a null handle (returns 0). */
    int cxxIsDone(hBRepBuilderAPI_MakeShape handle)
    {
        BRepBuilderAPI_MakeShape* makeShapeHandle = static_cast<BRepBuilderAPI_MakeShape*>(handle);
        try
        {
            return makeShapeHandle && makeShapeHandle->IsDone() ? 1 : 0;
        }
        catch (const Standard_Failure&) { return 0; }
        catch (const std::exception&) { return 0; }
        catch (...) { return 0; }
    }

    /* -------------------------------------------------------------------------
       Box primitives
       ------------------------------------------------------------------------- */

       /* Allocates a BRepPrimAPI_MakeBox for a box of dimensions dx × dy × dz and calls Build()
          so that IsDone() reflects reality before Shape() is ever requested.
          Returns the opaque builder handle on success, nullptr on any failure.
          Caller must free with cxxMakeBoxDelete. */
    hBRepBuilderAPI_MakeShape cxxBoxNew(double dx, double dy, double dz)
    {
        try
        {
            std::unique_ptr<BRepPrimAPI_MakeBox> makeBoxHandle(new BRepPrimAPI_MakeBox(dx, dy, dz));
            makeBoxHandle->Build();
            return static_cast<hBRepBuilderAPI_MakeShape>(makeBoxHandle.release());
        }
        catch (const Standard_Failure&) { return nullptr; }
        catch (const std::exception&) { return nullptr; }
        catch (...) { return nullptr; }
    }

    /* Frees the BRepPrimAPI_MakeBox builder.
       Safe to call with a null handle. Does NOT free any shape extracted from it. */
    void cxxMakeBoxDelete(hBRepBuilderAPI_MakeShape handle)
    {
        delete static_cast<BRepPrimAPI_MakeBox*>(handle);
    }

    /* Convenience: builds a box, extracts its shape, deletes the builder, and returns the shape.
       Returns nullptr on failure. Caller must free with cxxShapeDelete / cxxFreeShape. */
    hWodenShape cxxCreateBox(double dx, double dy, double dz)
    {
        hBRepBuilderAPI_MakeShape box = cxxBoxNew(dx, dy, dz);
        if (!box)
            return nullptr;

        hWodenShape shape = cxxToShape(box);
        cxxMakeBoxDelete(box);
        return shape;
    }

    /* -------------------------------------------------------------------------
       Cylinder primitives
       ------------------------------------------------------------------------- */

       /* Allocates a BRepPrimAPI_MakeCylinder for the given radius and height and calls Build().
          Returns the opaque builder handle on success, nullptr on any failure.
          Caller must free with cxxMakeCylinderDelete. */
    hBRepBuilderAPI_MakeShape cxxCylinderNew(double radius, double height)
    {
        try
        {
            std::unique_ptr<BRepPrimAPI_MakeCylinder> makeCylinderHandle(
                new BRepPrimAPI_MakeCylinder(radius, height));
            makeCylinderHandle->Build();
            return static_cast<hBRepBuilderAPI_MakeShape>(makeCylinderHandle.release());
        }
        catch (const Standard_Failure&) { return nullptr; }
        catch (const std::exception&) { return nullptr; }
        catch (...) { return nullptr; }
    }

    /* Frees the BRepPrimAPI_MakeCylinder builder.
       Safe to call with a null handle. Does NOT free any shape extracted from it. */
    void cxxMakeCylinderDelete(hBRepBuilderAPI_MakeShape handle)
    {
        delete static_cast<BRepPrimAPI_MakeCylinder*>(handle);
    }

    /* Convenience: builds a cylinder, extracts its shape, deletes the builder, and returns the shape.
       Returns nullptr on failure. Caller must free with cxxShapeDelete / cxxFreeShape. */
    hWodenShape cxxCreateCylinder(double radius, double height)
    {
        hBRepBuilderAPI_MakeShape cylinder = cxxCylinderNew(radius, height);
        if (!cylinder)
            return nullptr;

        hWodenShape shape = cxxToShape(cylinder);
        cxxMakeCylinderDelete(cylinder);
        return shape;
    }

    /* -------------------------------------------------------------------------
       Shape extraction and lifetime management
       ------------------------------------------------------------------------- */

       /* Extracts the completed TopoDS_Shape from a builder and wraps it in a new WodenShape.
          Returns nullptr if the handle is null or extraction fails.
          Caller must free with cxxShapeDelete / cxxFreeShape. */
    hWodenShape cxxToShape(hBRepBuilderAPI_MakeShape handle)
    {
        BRepBuilderAPI_MakeShape* makeShapeHandle = static_cast<BRepBuilderAPI_MakeShape*>(handle);
        if (!makeShapeHandle)
            return nullptr;

        try
        {
            std::unique_ptr<WodenShape> out(new WodenShape());
            out->shape = makeShapeHandle->Shape();
            return out.release();
        }
        catch (const Standard_Failure&) { return nullptr; }
        catch (const std::exception&) { return nullptr; }
        catch (...) { return nullptr; }
    }

    /* Destroys the WodenShape object and all its cached mesh buffers. */
    void cxxShapeDelete(hWodenShape handle)
    {
        delete static_cast<WodenShape*>(handle);
    }

    /* Alias for cxxShapeDelete; provided for UFFI callers that prefer "free" naming. */
    void cxxFreeShape(hWodenShape handle)
    {
        cxxShapeDelete(handle);
    }

    static void clearCachedMesh(WodenShape* s)
    {
        if (!s)
            return;

        s->vertices.clear();
        s->normals.clear();
        s->triangles.clear();
    }

    void cxxTranslateShape(hWodenShape shape, double x, double y, double z)
    {
        WodenShape* s = static_cast<WodenShape*>(shape);
        if (!s)
            return;

        try
        {
            gp_Trsf trsf;
            trsf.SetTranslation(gp_Vec(x, y, z));

            BRepBuilderAPI_Transform transformer(s->shape, trsf, true);
            transformer.Build();

            if (!transformer.IsDone())
                return;

            s->shape = transformer.Shape();
            clearCachedMesh(s);
        }
        catch (...)
        {
            return;
        }
    }

    void cxxRotateShapeDegrees(hWodenShape shape, double xDegrees, double yDegrees, double zDegrees)
    {
        WodenShape* s = static_cast<WodenShape*>(shape);
        if (!s)
            return;

        try
        {
            const double pi = 3.14159265358979323846;
            const double xRadians = xDegrees * pi / 180.0;
            const double yRadians = yDegrees * pi / 180.0;
            const double zRadians = zDegrees * pi / 180.0;

            gp_Trsf rx;
            gp_Trsf ry;
            gp_Trsf rz;

            rx.SetRotation(gp_Ax1(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(1.0, 0.0, 0.0)), xRadians);
            ry.SetRotation(gp_Ax1(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 1.0, 0.0)), yRadians);
            rz.SetRotation(gp_Ax1(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0)), zRadians);

            gp_Trsf trsf = rz * ry * rx;

            BRepBuilderAPI_Transform transformer(s->shape, trsf, true);
            transformer.Build();

            if (!transformer.IsDone())
                return;

            s->shape = transformer.Shape();
            clearCachedMesh(s);
        }
        catch (...)
        {
            return;
        }
    }

    void cxxRotateShapeDegreesAboutPoint(
        hWodenShape shape,
        double xDegrees,
        double yDegrees,
        double zDegrees,
        double centerX,
        double centerY,
        double centerZ)
    {
        WodenShape* s = static_cast<WodenShape*>(shape);
        if (!s)
            return;

        try
        {
            const double pi = 3.14159265358979323846;
            const double xRadians = xDegrees * pi / 180.0;
            const double yRadians = yDegrees * pi / 180.0;
            const double zRadians = zDegrees * pi / 180.0;

            const gp_Pnt center(centerX, centerY, centerZ);

            gp_Trsf rx;
            gp_Trsf ry;
            gp_Trsf rz;

            rx.SetRotation(gp_Ax1(center, gp_Dir(1.0, 0.0, 0.0)), xRadians);
            ry.SetRotation(gp_Ax1(center, gp_Dir(0.0, 1.0, 0.0)), yRadians);
            rz.SetRotation(gp_Ax1(center, gp_Dir(0.0, 0.0, 1.0)), zRadians);

            gp_Trsf trsf = rz * ry * rx;

            BRepBuilderAPI_Transform transformer(s->shape, trsf, true);
            transformer.Build();

            if (!transformer.IsDone())
                return;

            s->shape = transformer.Shape();
            clearCachedMesh(s);
        }
        catch (...)
        {
            return;
        }
    }

    void cxxScaleShape(hWodenShape shape, double sx, double sy, double sz)
    {
        WodenShape* s = static_cast<WodenShape*>(shape);
        if (!s)
            return;

        try
        {
            gp_Trsf trsf;
            trsf.SetValues(
                sx, 0.0, 0.0, 0.0,
                0.0, sy, 0.0, 0.0,
                0.0, 0.0, sz, 0.0
            );

            BRepBuilderAPI_Transform transformer(s->shape, trsf, true);
            transformer.Build();

            if (!transformer.IsDone())
                return;

            s->shape = transformer.Shape();
            clearCachedMesh(s);
        }
        catch (...)
        {
            return;
        }
    }

    /* -------------------------------------------------------------------------
       Tessellation
       ------------------------------------------------------------------------- */

       /* Tessellates the shape with BRepMesh_IncrementalMesh using the given linear deflection.
          Iterates over every face, transforms nodes and normals into world space, and accumulates
          them into flat vertex, normal, and triangle-index buffers.
          Reverses triangle winding for back-facing (REVERSED) faces so normals stay consistent.
          Clears all buffers and returns early (without throwing) on any OCCT or C++ exception. */
    void cxxTessellate(hWodenShape shape, double deflection)
    {
        WodenShape* s = static_cast<WodenShape*>(shape);
        if (!s)
            return;

        s->vertices.clear();
        s->normals.clear();
        s->triangles.clear();

        try
        {
            BRepMesh_IncrementalMesh mesher(s->shape, deflection);
            mesher.Perform();

            int vertexOffset = 0;

            for (TopExp_Explorer explorer(s->shape, TopAbs_FACE); explorer.More(); explorer.Next())
            {
                const TopoDS_Face face = TopoDS::Face(explorer.Current());

                TopLoc_Location loc;
                Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(face, loc);
                if (tri.IsNull())
                    continue;

                /* Compute per-node normals from the face geometry. */
                BRepLib_ToolTriangulatedShape::ComputeNormals(face, tri);

                const gp_Trsf trsf = loc.Transformation();

                /* Copy nodes, transforming each from local face space to world space. */
                for (int i = 1; i <= tri->NbNodes(); ++i)
                {
                    gp_Pnt p = tri->Node(i);
                    p.Transform(trsf);
                    s->vertices.push_back(p.X());
                    s->vertices.push_back(p.Y());
                    s->vertices.push_back(p.Z());

                    gp_Dir n = tri->Normal(i);
                    n.Transform(trsf);
                    s->normals.push_back(n.X());
                    s->normals.push_back(n.Y());
                    s->normals.push_back(n.Z());
                }

                /* Copy triangle indices, reversing winding for REVERSED faces so all normals
                   point outward consistently. OCC indices are 1-based; convert to 0-based. */
                const bool reversed = (face.Orientation() == TopAbs_REVERSED);
                for (int i = 1; i <= tri->NbTriangles(); ++i)
                {
                    int n1, n2, n3;
                    tri->Triangle(i).Get(n1, n2, n3);

                    const int i1 = vertexOffset + n1 - 1;
                    const int i2 = vertexOffset + n2 - 1;
                    const int i3 = vertexOffset + n3 - 1;

                    s->triangles.push_back(i1);
                    if (reversed)
                    {
                        s->triangles.push_back(i3);
                        s->triangles.push_back(i2);
                    }
                    else
                    {
                        s->triangles.push_back(i2);
                        s->triangles.push_back(i3);
                    }
                }

                vertexOffset += tri->NbNodes();
            }
        }
        catch (...)
        {
            /* On any failure, leave the buffers empty rather than in a partial state. */
            s->vertices.clear();
            s->normals.clear();
            s->triangles.clear();
        }
    }

    /* -------------------------------------------------------------------------
       Copy-out mesh accessors (safe for languages without pointer arithmetic)
       ------------------------------------------------------------------------- */

       /* Returns the number of vertices from the last tessellation (buffer length / 3). */
    int cxxVertexCount(hWodenShape shape)
    {
        WodenShape* s = static_cast<WodenShape*>(shape);
        if (!s) return 0;
        return static_cast<int>(s->vertices.size() / 3);
    }

    /* Returns the number of triangles from the last tessellation (index count / 3). */
    int cxxTriangleCount(hWodenShape shape)
    {
        WodenShape* s = static_cast<WodenShape*>(shape);
        if (!s) return 0;
        return static_cast<int>(s->triangles.size() / 3);
    }

    /* Copies all vertex positions (x,y,z triples) into buffer.
       buffer must hold at least cxxVertexCount(shape) * 3 doubles. */
    void cxxGetAllVertices(hWodenShape shape, double* buffer)
    {
        WodenShape* s = static_cast<WodenShape*>(shape);
        if (!s || !buffer) return;
        for (size_t i = 0; i < s->vertices.size(); ++i)
            buffer[i] = s->vertices[i];
    }

    /* Copies all per-vertex normals (nx,ny,nz triples) into buffer.
       buffer must hold at least cxxVertexCount(shape) * 3 doubles. */
    void cxxGetAllNormals(hWodenShape shape, double* buffer)
    {
        WodenShape* s = static_cast<WodenShape*>(shape);
        if (!s || !buffer) return;
        for (size_t i = 0; i < s->normals.size(); ++i)
            buffer[i] = s->normals[i];
    }

    /* Copies all triangle indices (i0,i1,i2 triples, 0-based) into buffer.
       buffer must hold at least cxxTriangleCount(shape) * 3 ints. */
    void cxxGetAllTriangles(hWodenShape shape, int* buffer)
    {
        WodenShape* s = static_cast<WodenShape*>(shape);
        if (!s || !buffer) return;
        for (size_t i = 0; i < s->triangles.size(); ++i)
            buffer[i] = s->triangles[i];
    }

    /* -------------------------------------------------------------------------
       Zero-copy mesh buffer accessors (UFFI / FFI pointer API)
       Pointers remain valid until the next cxxTessellate call or the shape is freed.
       ------------------------------------------------------------------------- */

       /* Returns a direct pointer to the internal vertex buffer (x,y,z triples).
          Returns nullptr if the shape is null or has no vertices. */
    const double* cxxGetVertices(hWodenShape shape)
    {
        WodenShape* s = static_cast<WodenShape*>(shape);
        if (!s || s->vertices.empty()) return nullptr;
        return s->vertices.data();
    }

    /* Returns the number of vertices (buffer length / 3). Alias for cxxVertexCount. */
    int cxxGetVertexCount(hWodenShape shape)
    {
        return cxxVertexCount(shape);
    }

    /* Returns the total number of doubles in the vertex buffer (vertexCount * 3). */
    int cxxGetVertexBufferLength(hWodenShape shape)
    {
        WodenShape* s = static_cast<WodenShape*>(shape);
        if (!s) return 0;
        return static_cast<int>(s->vertices.size());
    }

    /* Returns a direct pointer to the internal normal buffer (nx,ny,nz triples).
       Returns nullptr if the shape is null or has no normals. */
    const double* cxxGetNormals(hWodenShape shape)
    {
        WodenShape* s = static_cast<WodenShape*>(shape);
        if (!s || s->normals.empty()) return nullptr;
        return s->normals.data();
    }

    /* Returns the number of normals (buffer length / 3). */
    int cxxGetNormalCount(hWodenShape shape)
    {
        WodenShape* s = static_cast<WodenShape*>(shape);
        if (!s) return 0;
        return static_cast<int>(s->normals.size() / 3);
    }

    /* Returns the total number of doubles in the normal buffer (normalCount * 3). */
    int cxxGetNormalBufferLength(hWodenShape shape)
    {
        WodenShape* s = static_cast<WodenShape*>(shape);
        if (!s) return 0;
        return static_cast<int>(s->normals.size());
    }

    /* Returns a direct pointer to the internal triangle index buffer (i0,i1,i2 triples, 0-based).
       Returns nullptr if the shape is null or has no triangles. */
    const int* cxxGetTriangles(hWodenShape shape)
    {
        WodenShape* s = static_cast<WodenShape*>(shape);
        if (!s || s->triangles.empty()) return nullptr;
        return s->triangles.data();
    }

    /* Returns the number of triangles (index count / 3). Alias for cxxTriangleCount. */
    int cxxGetTriangleCount(hWodenShape shape)
    {
        return cxxTriangleCount(shape);
    }

    /* Returns the total number of ints in the triangle index buffer (triangleCount * 3). */
    int cxxGetTriangleIndexCount(hWodenShape shape)
    {
        WodenShape* s = static_cast<WodenShape*>(shape);
        if (!s) return 0;
        return static_cast<int>(s->triangles.size());
    }

    /* -------------------------------------------------------------------------
       Boolean operations
       ------------------------------------------------------------------------- */

       /* Computes hShape1 minus hShape2 using BRepAlgoAPI_Cut and returns the result as a new shape.
          Returns nullptr if either input is null or the boolean operation fails.
          Input shapes are not modified or freed; caller owns the returned handle. */
    hWodenShape cxxCut(hWodenShape hShape1, hWodenShape hShape2)
    {
        WodenShape* shape1 = static_cast<WodenShape*>(hShape1);
        WodenShape* shape2 = static_cast<WodenShape*>(hShape2);

        if (!shape1 || !shape2)
            return nullptr;

        try
        {
            BRepAlgoAPI_Cut cut(shape1->shape, shape2->shape);
            cut.Build();

            if (!cut.IsDone())
                return nullptr;

            std::unique_ptr<WodenShape> out(new WodenShape());
            out->shape = cut.Shape();
            return out.release();
        }
        catch (const Standard_Failure&) { return nullptr; }
        catch (const std::exception&) { return nullptr; }
        catch (...) { return nullptr; }
    }

    /* -------------------------------------------------------------------------
       Debug memory tracking (no-ops in Release builds)
       ------------------------------------------------------------------------- */

       /* Records a CRT heap snapshot. Call this before the code section you want to monitor. */
    void cxxMemoryCheckpoint()
    {
#ifdef _DEBUG
        _CrtMemCheckpoint(&gMemStateStart);
#endif
    }

    /* Computes the heap difference since the last cxxMemoryCheckpoint call.
       If allocations differ, dumps statistics and all outstanding objects to debug output. */
    void cxxDumpMemoryDifference()
    {
#ifdef _DEBUG
        _CrtMemState endState, diffState;
        _CrtMemCheckpoint(&endState);
        if (_CrtMemDifference(&diffState, &gMemStateStart, &endState))
        {
            _CrtMemDumpStatistics(&diffState);
            _CrtMemDumpAllObjectsSince(&gMemStateStart);
        }
#endif
    }

    /* Returns 1 if the heap state differs from the last checkpoint, 0 otherwise.
       Useful for asserting no-leak conditions in unit tests. */
    int cxxHasMemoryDifference()
    {
#ifdef _DEBUG
        _CrtMemState endState, diffState;
        _CrtMemCheckpoint(&endState);
        return _CrtMemDifference(&diffState, &gMemStateStart, &endState) ? 1 : 0;
#else
        return 0;
#endif
    }

} // extern "C"