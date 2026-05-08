// my_c_wrapper.cpp
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <stdlib.h>

// Own includes
#include "my_c_wrapper.h"

// OpenCASCADE includes
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <BRepLib_ToolTriangulatedShape.hxx>
#include <Poly_Triangulation.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

#include <vector>

static _CrtMemState gMemStateStart;

struct OCCShape
{
	TopoDS_Shape shape;
	std::vector<double> vertices;  // x,y,z per vertex
	std::vector<double> normals;   // nx,ny,nz per vertex
	std::vector<int> triangles;    // i0,i1,i2 per triangle
};

extern "C" {
	hBRepBuilderAPI_MakeShape cxxBoxNew(double dx, double dy, double dz)
	{
		BRepPrimAPI_MakeBox* makeBoxHandle = new BRepPrimAPI_MakeBox(dx, dy, dz);
		// Ensure the builder runs now so IsDone reflects reality before Shape() is requested.
		makeBoxHandle->Build();
		return static_cast<hBRepBuilderAPI_MakeShape>(makeBoxHandle);
	}

	void cxxMakeBoxDelete(hBRepBuilderAPI_MakeShape handle)
	{
		BRepPrimAPI_MakeBox* makeBoxHandle = static_cast<BRepPrimAPI_MakeBox*>(handle);
		delete makeBoxHandle;
	}

	hTopoDS_Shape cxxToShape(hBRepBuilderAPI_MakeShape handle)
	{
		BRepBuilderAPI_MakeShape* makeBoxHandle = static_cast<BRepBuilderAPI_MakeShape*>(handle);
		OCCShape* out = new OCCShape();
		out->shape = makeBoxHandle->Shape();
		return static_cast<hTopoDS_Shape>(out);
	}

	void cxxShapeDelete(hTopoDS_Shape handle)
	{
		OCCShape* shapeHandle = static_cast<OCCShape*>(handle);
		delete shapeHandle;
	}

	int cxxIsDone(hBRepBuilderAPI_MakeShape handle)
	{
		BRepBuilderAPI_MakeShape* makeShapeHandle = static_cast<BRepBuilderAPI_MakeShape*>(handle);
		return makeShapeHandle && makeShapeHandle->IsDone() ? 1 : 0;
	}

	void cxxTessellate(hTopoDS_Shape shape, double deflection)
	{
		OCCShape* s = static_cast<OCCShape*>(shape);
		if (!s)
			return;

		s->vertices.clear();
		s->normals.clear();
		s->triangles.clear();

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

			BRepLib_ToolTriangulatedShape::ComputeNormals(face, tri);

			const gp_Trsf trsf = loc.Transformation();

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

			const bool reversed = (face.Orientation() == TopAbs_REVERSED);
			for (int i = 1; i <= tri->NbTriangles(); ++i)
			{
				int n1, n2, n3;
				tri->Triangle(i).Get(n1, n2, n3);

				// OCC indices are 1-based; convert to 0-based + offset.
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

	int cxxVertexCount(hTopoDS_Shape shape)
	{
		OCCShape* s = static_cast<OCCShape*>(shape);
		if (!s)
			return 0;
		return static_cast<int>(s->vertices.size() / 3);
	}

	int cxxTriangleCount(hTopoDS_Shape shape)
	{
		OCCShape* s = static_cast<OCCShape*>(shape);
		if (!s)
			return 0;
		return static_cast<int>(s->triangles.size() / 3);
	}

	void cxxGetAllVertices(hTopoDS_Shape shape, double* buffer)
	{
		OCCShape* s = static_cast<OCCShape*>(shape);
		if (!s || !buffer)
			return;
		for (size_t i = 0; i < s->vertices.size(); ++i)
			buffer[i] = s->vertices[i];
	}

	void cxxGetAllNormals(hTopoDS_Shape shape, double* buffer)
	{
		OCCShape* s = static_cast<OCCShape*>(shape);
		if (!s || !buffer)
			return;
		for (size_t i = 0; i < s->normals.size(); ++i)
			buffer[i] = s->normals[i];
	}

	void cxxGetAllTriangles(hTopoDS_Shape shape, int* buffer)
	{
		OCCShape* s = static_cast<OCCShape*>(shape);
		if (!s || !buffer)
			return;
		for (size_t i = 0; i < s->triangles.size(); ++i)
			buffer[i] = s->triangles[i];
	}

	void cxxMemoryCheckpoint()
	{
	#ifdef _DEBUG
			_CrtMemCheckpoint(&gMemStateStart);
	#endif
	}

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

	hBRepBuilderAPI_MakeShape cxxCylinderNew(double radius, double height) {
		BRepPrimAPI_MakeCylinder* makeCylinderHandle = new BRepPrimAPI_MakeCylinder(radius, height);
		makeCylinderHandle->Build();
		return static_cast<hBRepBuilderAPI_MakeShape>(makeCylinderHandle);
	}

	void cxxMakeCylinderDelete(hBRepBuilderAPI_MakeShape handle) {
		BRepPrimAPI_MakeCylinder* makeCylinderHandle = static_cast<BRepPrimAPI_MakeCylinder*>(handle);
		delete makeCylinderHandle;
	}
}
