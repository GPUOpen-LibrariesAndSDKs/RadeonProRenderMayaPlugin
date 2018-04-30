#include "Translators.h"
#include "base_mesh.h"
#include <maya/MFnMesh.h>
#include <maya/MVectorArray.h>
#include <maya/MIntArray.h>
#include <maya/MPlug.h>
#include <maya/MDagPath.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFloatArray.h>
#include <maya/MPlugArray.h>
#include <maya/MDistance.h>
#include <maya/MFnLight.h>
#include <maya/MFnSpotLight.h>
#include <maya/MRenderUtil.h>
#include <maya/MFloatMatrix.h>
#include <maya/M3dView.h>
#include <maya/MGlobal.h>
#include <maya/MImage.h>
#include <maya/MDGContext.h>
#include <maya/MTime.h>
#include <maya/MItMeshEdge.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFnMeshData.h>
#include <maya/MFileObject.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MQuaternion.h>
#include <maya/MAnimControl.h>
#include <maya/MFnSubd.h>
#include <maya/MEulerRotation.h>
#include <maya/MPointArray.h>
#include <unordered_map>

#include <functional>
#include <map>
#include <string>
#include <cfloat>

#include "FireMaya.h"
#include "FireRenderObjects.h"
#include "SkyBuilder.h"
#include "SunPositionCalculator.h"
#include "DependencyNode.h"
#include "maya/MTransformationMatrix.h"
#include "VRay.h"
#include "FireRenderUtils.h"
#include "FireRenderGlobals.h"
#include "FireRenderMath.h"
#include "FireRenderThread.h"

#include "PhysicalLightAttributes.h"
#include "PhysicalLightGeometryUtility.h"

namespace
{
	//	#define PROFILE 1			// profile mesh loading code

	// This code gives ~20% performance improvement in release, and ~10x boost for debug build.
	// Restrictions compared to original implementation:
	// - reserve() is required, must fit entire data
	// - works only with POD types.
	template <class T>
	class FastVector
	{
		size_t m_reserved = 0;
		size_t m_count = 0;
		T*     m_data = nullptr;

	public:
		~FastVector()
		{
			if (m_data) free(m_data);
		}

		void reserve(size_t n)
		{
			assert(!m_data);
			m_data = (T*)malloc(sizeof(T) * n);
			m_reserved = n;
			m_count = 0;
		}

		T& operator[](size_t index)
		{
			return *(m_data + index);
		}

		const T& operator[](size_t index) const
		{
			return *(m_data + index);
		}

		T* data() { return m_data; }
		const T* data() const { return m_data; }
		const size_t size() const { return m_count; }

		// returns index
		int Add(const T& v)
		{
			assert(m_data != nullptr && m_count < m_reserved);
			size_t index = m_count++;
			m_data[index] = v;
			return int(index);
		}
	};
}

namespace FireMaya
{
	bool translateCamera(frw::Camera& frw_camera, const MObject& camera, const MMatrix& matrix, bool isRenderView, float aspectRatio, bool useAspectRatio, int cameraType)
	{
		rpr_int frstatus;
		MStatus mstatus;
		MFnCamera fnCamera(camera);

		// Maya always returns all lengths in centimeters despite the settings in Preferences (detected experimentally)
		float cmToMCoefficient = 0.01f;

		auto frcamera = frw_camera.Handle();

		bool showFilmGate = fnCamera.isDisplayFilmGate();
		float overscan = isRenderView ? 1.0f : float(fnCamera.overscan());

		DebugPrint("\tDisplay Film Gate: %d", showFilmGate);
		DebugPrint("\tOverscan: %f", overscan);

		auto cameraMode = FireRenderGlobals::getCameraModeForType(FireRenderGlobals::CameraType(cameraType), fnCamera.isOrtho());
		frw_camera.SetMode(cameraMode);

		switch (cameraMode)
		{
		case frw::CameraModeOrthographic:
		{
			float cs = static_cast<float>(fnCamera.cameraScale(&mstatus));
			assert(mstatus == MStatus::kSuccess);
			float ow = static_cast<float>(fnCamera.orthoWidth(&mstatus) * overscan);
			assert(mstatus == MStatus::kSuccess);
			float ps = static_cast<float>(fnCamera.preScale(&mstatus));
			assert(mstatus == MStatus::kSuccess);

			frstatus = rprCameraSetOrthoWidth(frcamera, float(CM_2_M * ps * ow) / cs);
			checkStatus(frstatus);

			frstatus = rprCameraSetOrthoHeight(frcamera, float(CM_2_M * ps * ow) / (cs * aspectRatio));
			checkStatus(frstatus);

		} break;

		case frw::CameraModePanorama: //break;
		case frw::CameraModePanoramaStereo: //break;
		case frw::CameraModeCubemap: //break;
		case frw::CameraModeCubemapStereo: //break;
		case frw::CameraModePerspective: // break
		default:
		{
			frstatus = rprCameraSetFocalLength(frcamera, static_cast<float>(fnCamera.focalLength(&mstatus)));
			checkStatus(frstatus);

			// convert fnCamera.focusDistance() in cm to m
			float focusDistance = static_cast<float>(fnCamera.focusDistance(&mstatus) * cmToMCoefficient);
			checkStatus(frstatus);

			frstatus = rprCameraSetFocusDistance(frcamera, focusDistance);
			checkStatus(frstatus);

		}	break;
		}

		// fStop == FLT_MAX is equivalent to disable DoF
		float fStop = FLT_MAX;
		if (fnCamera.isDepthOfField(&mstatus))
		{
			fStop = static_cast<float>(fnCamera.fStop(&mstatus));
			assert(mstatus == MStatus::kSuccess);
		}

		frstatus = rprCameraSetFStop(frcamera, fStop);
		checkStatus(frstatus);

		// convert from inch to mm
		float apertureWidth = static_cast<float>(fnCamera.horizontalFilmAperture(&mstatus)) * 25.4f;
		float apertureHeight = static_cast<float>(fnCamera.verticalFilmAperture(&mstatus)) * 25.4f;

		apertureWidth *= overscan;
		apertureHeight *= overscan;

		if (useAspectRatio)
		{
			MFnCamera::FilmFit filmFit = fnCamera.filmFit();
			switch (filmFit)
			{
			case MFnCamera::kFillFilmFit:
			{
				if (aspectRatio < 1.0)
				{
					apertureWidth = apertureHeight * aspectRatio;
					frstatus = rprCameraSetSensorSize(frcamera, apertureWidth, apertureHeight);
				}
				else
				{
					apertureHeight = apertureWidth / aspectRatio;
					frstatus = rprCameraSetSensorSize(frcamera, apertureWidth, apertureHeight);
				}
				break;
			}
			case MFnCamera::kHorizontalFilmFit:
			{
				apertureHeight = apertureWidth / aspectRatio;
				frstatus = rprCameraSetSensorSize(frcamera, apertureWidth, apertureHeight);
				break;
			}
			case MFnCamera::kVerticalFilmFit:
			{
				apertureWidth = apertureHeight * aspectRatio;
				frstatus = rprCameraSetSensorSize(frcamera, apertureWidth, apertureHeight);
				break;
			}
			default:
				float frApertureHeight = apertureWidth / aspectRatio;
				frstatus = rprCameraSetSensorSize(frcamera, apertureWidth, frApertureHeight);
			}
		}
		else
		{
			frstatus = rprCameraSetSensorSize(frcamera, apertureWidth, apertureHeight);
		}

		checkStatus(frstatus);

		// set transformation matrix
		assert(mstatus == MStatus::kSuccess);
		MPoint eye = MPoint(0, 0, 0, 1) * matrix;
		// convert eye and lookat from cm to m
		eye = eye * cmToMCoefficient;
		MVector viewDir = MVector::zNegAxis * matrix;
		MVector upDir = MVector::yAxis * matrix;
		MPoint  lookat = eye + viewDir;
		frstatus = rprCameraLookAt(frcamera,
			static_cast<float>(eye.x), static_cast<float>(eye.y), static_cast<float>(eye.z),
			static_cast<float>(lookat.x), static_cast<float>(lookat.y), static_cast<float>(lookat.z),
			static_cast<float>(upDir.x), static_cast<float>(upDir.y), static_cast<float>(upDir.z));

		checkStatus(frstatus);

		//Check aperture and blades
		MPlug bladesPlug = fnCamera.findPlug("fireRenderApertureBlades");
		if (!bladesPlug.isNull())
		{
			int num_blades = bladesPlug.asInt();
			frstatus = rprCameraSetApertureBlades(frcamera, num_blades);
			checkStatus(frstatus);
		}

		MPlug exposurePlug = fnCamera.findPlug("fireRenderExposure");
		if (!exposurePlug.isNull())
		{
			float exposure = exposurePlug.asFloat();
			frstatus = rprCameraSetExposure(frcamera, exposure);
			checkStatus(frstatus);
		}

		// Setting Near and Far clipping plane
		frstatus = rprCameraSetNearPlane(frcamera, (float)(fnCamera.nearClippingPlane() * cmToMCoefficient));
		checkStatus(frstatus);

		frstatus = rprCameraSetFarPlane(frcamera, (float)(fnCamera.farClippingPlane() * cmToMCoefficient));
		checkStatus(frstatus);

		return true;
	}

	struct Float3
	{
		float x, y, z;

		Float3(const MFloatPoint& v)
		{
			x = v.x;
			y = v.y;
			z = v.z;
		}
		Float3(const MFloatVector& v)
		{
			x = v.x;
			y = v.y;
			z = v.z;
		}
		bool operator<(const Float3& b) const
		{
			if (x < b.x)
				return true;
			if (x > b.x)
				return false;
			if (y < b.y)
				return true;
			if (y > b.y)
				return false;
			return z < b.z;
		}
	};

	struct Float2
	{
		float x, y;
		bool operator<(const Float2& b) const
		{
			if (x < b.x)
				return true;
			if (x > b.x)
				return false;
			return y < b.y;
		}
	};

	MObject generateSmoothMesh(const MObject& object, const MObject& parent, MStatus& status) {
		MFnMesh mesh(object);

		DependencyNode attributes(object);

		bool smoothPreview = attributes.getBool("displaySmoothMesh");

		//for non smooth preview case:
		status = MStatus::kSuccess;

		return (smoothPreview) ? mesh.generateSmoothMesh(parent, 0, &status) : MObject::kNullObj;
	}

	MObject tessellateNurbsSurface(const MObject& object, const MObject& parent, MStatus& status)
	{
		MObject ret = FireMaya::FireRenderThread::RunOnMainThread<MObject>([&]
		{
			// Read tessellation attributes.
			DependencyNode attributes(object);

			int modeU = attributes.getInt("modeU");
			int numberU = attributes.getInt("numberU");
			int modeV = attributes.getInt("modeV");
			int numberV = attributes.getInt("numberV");

			bool smoothEdge = attributes.getBool("smoothEdge");

			bool useChordHeightRatio = attributes.getBool("useChordHeightRatio");
			bool edgeSwap = attributes.getBool("edgeSwap");
			bool useMinScreen = attributes.getBool("useMinScreen");

			double chordHeightRatio = attributes.getDouble("chordHeightRatio");
			double minScreen = attributes.getDouble("minScreen");

			// Construct tessellation parameters.
			MTesselationParams params(
				MTesselationParams::kGeneralFormat,
				MTesselationParams::kTriangles);

			switch (modeU)
			{
			case 1:	params.setUIsoparmType(MTesselationParams::kSurface3DEquiSpaced); break;
			case 2:	params.setUIsoparmType(MTesselationParams::kSurfaceEquiSpaced);	break;
			case 3:	params.setUIsoparmType(MTesselationParams::kSpanEquiSpaced); break;
			case 4: params.setUIsoparmType(MTesselationParams::kSurfaceEquiSpaced);	break;
			}

			switch (modeV)
			{
			case 1:	params.setVIsoparmType(MTesselationParams::kSurface3DEquiSpaced); break;
			case 2:	params.setVIsoparmType(MTesselationParams::kSurfaceEquiSpaced); break;
			case 3:	params.setVIsoparmType(MTesselationParams::kSpanEquiSpaced); break;
			case 4:	params.setVIsoparmType(MTesselationParams::kSurfaceEquiSpaced); break;
			}

			params.setUNumber(numberU);
			params.setVNumber(numberV);
			params.setSubdivisionFlag(MTesselationParams::kUseChordHeightRatio, useChordHeightRatio);
			params.setChordHeightRatio(chordHeightRatio);
			params.setSubdivisionFlag(MTesselationParams::kUseMinScreenSize, useMinScreen);
			params.setMinScreenSize(minScreen, minScreen);
			params.setSubdivisionFlag(MTesselationParams::kUseEdgeSmooth, smoothEdge);
			params.setSubdivisionFlag(MTesselationParams::kUseTriangleEdgeSwapping, edgeSwap);

			// Tessellate the surface and return the resulting mesh object.
			MFnNurbsSurface surface(object);

			return surface.tesselate(params, parent, &status);
		});

		return ret;
	}

	std::vector<frw::Shape> TranslateMesh(frw::Context context, const MObject& originalObject)
	{
		MAIN_THREAD_ONLY;

		std::vector<frw::Shape> elements;
		MStatus mstatus;
		MString errMsg;

		MFnDagNode node(originalObject);

		DebugPrint("TranslateMesh: %s", node.fullPathName().asUTF8());

		// Don't render intermediate object
		if (node.isIntermediateObject(&mstatus))
			return elements;

		auto parent = node.parent(0);

		MObject tessellated;

		// tessellate to mesh if we aren't already one
		if (originalObject.hasFn(MFn::kMesh))
		{
			// all good :)
			tessellated = generateSmoothMesh(originalObject, parent, mstatus);
			if (mstatus != MStatus::kSuccess)
			{
				mstatus.perror("MFnMesh::generateSmoothMesh");
				return elements;
			}
		}
		else if (originalObject.hasFn(MFn::kNurbsSurface))
		{
			tessellated = tessellateNurbsSurface(originalObject, parent, mstatus);
			if (mstatus != MStatus::kSuccess)
			{
				mstatus.perror("MFnNurbsSurface::tessellate");
				return elements;
			}
		}
		else if (originalObject.hasFn(MFn::kSubdiv))
		{
			MFnSubd surface(originalObject);
			tessellated = surface.tesselate(false, 1, 1, parent, &mstatus);
			if (mstatus != MStatus::kSuccess)
			{
				mstatus.perror("MFnSubd::tessellate");
				return elements;
			}
		}
		else
		{
			return elements;
		}

		auto object = !tessellated.isNull() ? tessellated : originalObject;

		MFnMesh fnMesh(object, &mstatus);
		if (MStatus::kSuccess != mstatus)
		{
			mstatus.perror("MFnMesh constructor");
			return elements;
		}
		MStringArray uvSetNames;
		fnMesh.getUVSetNames(uvSetNames);
		unsigned int uvSetNamesNum = uvSetNames.length();

		// RPR supports only 2 UV sets. There is no way to get this value from RPR so it's hadrcoded
		static const int rprMaxUVSetCount = 2;
		if (uvSetNamesNum > rprMaxUVSetCount)
		{
			uvSetNamesNum = rprMaxUVSetCount;
			FireRenderError err("UV set error", "One or more objects have multiple UV sets. Only two UV sets per object supported. Scene will be rendered with first two UV sets.", true);
		}

		MFloatPointArray points;
		MFloatVectorArray normals;

		mstatus = fnMesh.getPoints(points);
		assert(MStatus::kSuccess == mstatus);
		mstatus = fnMesh.getNormals(normals);
		assert(MStatus::kSuccess == mstatus);

		MIntArray faceMaterialIndices;
		auto elementCount = GetFaceMaterials(fnMesh, faceMaterialIndices);
		elements.resize(elementCount);

		assert(faceMaterialIndices.length() == fnMesh.numPolygons());

		DebugPrint("Elements: %d; Polygons: %d; Vertices: %d; Normals: %d", elementCount, fnMesh.numPolygons(), points.length(), normals.length());

		int degenerates = 0;

#if PROFILE
		clock_t tick = clock();
#endif

#define USE_NEW_MESH_EXPORT
#ifdef USE_NEW_MESH_EXPORT
		// pointer to array of vertices coordinates in Maya
		const float* pVertices = fnMesh.getRawPoints(&mstatus);
		assert(MStatus::kSuccess == mstatus);
		int countVertices = fnMesh.numVertices(&mstatus);
		assert(MStatus::kSuccess == mstatus);

		// pointer to array of normal coordinates in Maya
		const float* pNormals = fnMesh.getRawNormals(&mstatus);
		assert(MStatus::kSuccess == mstatus);
		int countNormals = fnMesh.numNormals(&mstatus);
		assert(MStatus::kSuccess == mstatus);

		int numIndices = 0;
		{
			// get triangle count (max possible count; this number is used for reserve only)
			MIntArray triangleCounts; // basically number of triangles in polygons; size of array equal to number of polygons in mesh
			MIntArray triangleVertices; // indices of points in triangles (3 indices per triangle)
			mstatus = fnMesh.getTriangles(triangleCounts, triangleVertices);
			numIndices = triangleVertices.length();
		}

		assert(MStatus::kSuccess == mstatus);

		// UV coordinates
		// - auxillary arrays for passing data to RPR
		std::vector<std::vector<Float2> > uvCoords;
		std::vector<const float*> puvCoords; 
		puvCoords.reserve(uvSetNamesNum);

		std::vector<size_t> sizeCoords; 
		sizeCoords.reserve(uvSetNamesNum);

		// Core accepts only equal sized UV coordinate arrays
		// We should fill less array with zeros
		size_t maxUVSize = 0;

		// - fill these arrays with data
		// - up to 2 UV channels is supported
		for (unsigned int currUVCHannel = 0; currUVCHannel < uvSetNamesNum; ++currUVCHannel)
		{
			// - get data from Maya
			MFloatArray uArray;
			MFloatArray vArray;
			mstatus = fnMesh.getUVs(uArray, vArray, &uvSetNames[currUVCHannel]);
			assert(MStatus::kSuccess == mstatus);
			assert(uArray.length() == vArray.length());

			// - RPR needs UV pairs instead of 2 parallel arrays (U and V) that Maya returns
			uvCoords.emplace_back();
			uvCoords[currUVCHannel].reserve(uArray.length());
			for (unsigned int idx = 0; idx < uArray.length(); ++idx)
			{
				Float2 uv;
				uv.x = uArray[idx];
				uv.y = vArray[idx];
				uvCoords[currUVCHannel].push_back(uv);
			}

			if (maxUVSize < uvCoords[currUVCHannel].size())
			{
				maxUVSize = uvCoords[currUVCHannel].size();
			}
		}

		// making equal size
		for (unsigned int currUVCHannel = 0; currUVCHannel < uvSetNamesNum; ++currUVCHannel)
		{
			sizeCoords.push_back(maxUVSize);
			uvCoords[currUVCHannel].resize(maxUVSize);
			puvCoords.push_back(uvCoords[currUVCHannel].size() > 0 ? (float*)uvCoords[currUVCHannel].data() : nullptr);
		}

		// different RPR mesh is created for each shader
		for (int shaderId = 0; shaderId < elementCount; shaderId++)
		{
			// output indices of vertexes (3 indices for each triangle)
			std::vector<int> vertexIndices;
			vertexIndices.reserve(numIndices);

			// output indices of normals (3 indices for each triangle)
			std::vector<int> normalIndices;
			normalIndices.reserve(numIndices);

			// output indices of UV coordinates (3 indices for each triangle)
			// up to 2 UV chanels is supported, thus vector of vectors
			std::vector<std::vector<int> > uvIndices;
			uvIndices.reserve(uvSetNamesNum);
			for (unsigned int currUVCHannel = 0; currUVCHannel < uvSetNamesNum; ++currUVCHannel)
			{
				uvIndices.emplace_back();
				uvIndices[currUVCHannel].reserve(numIndices);
			}
			
			// iterate through mesh
			MItMeshPolygon it = MItMeshPolygon(fnMesh.object());
			for (; !it.isDone(); it.next())
			{
				// skip polygons that have different shader from current one
				if (faceMaterialIndices[it.index()] != shaderId)
					continue;

				// get indices of vertexes of polygon
				// - these are indices of verts of polygion, not triangles!!!
				MIntArray vertices;
				mstatus = it.getVertices(vertices);
				assert(MStatus::kSuccess == mstatus);

				// get indices of vertices of triangles of current polygon
				// - these are indices of verts in triangles!
				MIntArray vertexList;
				MPointArray points;
				mstatus = it.getTriangles(points, vertexList);
				assert(MStatus::kSuccess == mstatus);

				// write indices of triangles in mesh into output triangle indices array
				for (unsigned int idx = 0; idx < vertexList.length(); ++idx)
				{
					vertexIndices.push_back(vertexList[idx]);
				}

				// create table to convert global index in vertex indices array to local one [0...number of vertex in polygon]
				std::unordered_map<int, int> vertexIdxGlobalToLocal;
				for (unsigned int idx = 0; idx < vertices.length(); ++idx)
				{
					vertexIdxGlobalToLocal[vertices[idx]] = idx;
				}

				// write indices of normals of vertices (parallel to triangle vertices) into output array
				for (unsigned int idx = 0; idx < vertexList.length(); ++idx)
				{
					auto localNormalIdxIt = vertexIdxGlobalToLocal.find(vertexList[idx]);
					assert(localNormalIdxIt != vertexIdxGlobalToLocal.end());
					normalIndices.push_back(it.normalIndex(localNormalIdxIt->second));
				}

				// up to 2 UV channels is supported
				for (unsigned int currUVCHannel = 0; currUVCHannel < uvSetNamesNum; ++currUVCHannel)
				{
					// write indices 
					for (unsigned int idx = 0; idx < vertexList.length(); ++idx)
					{
						int uvIdx = 0;
						auto localUVIdxIt = vertexIdxGlobalToLocal.find(vertexList[idx]);
						assert(localUVIdxIt != vertexIdxGlobalToLocal.end());

						mstatus = it.getUVIndex(localUVIdxIt->second, uvIdx, &uvSetNames[currUVCHannel]);
						
						if (mstatus == MStatus::kSuccess)
						{
							uvIndices[currUVCHannel].push_back(uvIdx);
						}
						else
						{
							// in case if uv coordinate not assigned to polygon set it index to 0
							uvIndices[currUVCHannel].push_back(0);
						}
					}
				}
			}

			// auxillary array for passing data to RPR
			std::vector<const rpr_int*>	puvIndices;
			for (unsigned int idx = 0; idx < uvSetNamesNum; ++idx)
			{
				puvIndices.push_back(uvIndices[idx].size() > 0 ? uvIndices[idx].data() : nullptr);
			}

			std::vector<int> multiUV_texcoord_strides(uvSetNamesNum, sizeof(Float2));
			std::vector<int> texIndexStride(uvSetNamesNum, sizeof(int));

			if (sizeCoords.size() == 0 || puvIndices.size() == 0 || sizeCoords[0] == 0 || puvIndices[0] == nullptr)
			{
				// no uv set
				uvSetNamesNum = 0;
			}

			// create mesh in RPR
			elements[shaderId] = context.CreateMeshEx(
				pVertices, countVertices, sizeof(Float3),
				pNormals, countNormals, sizeof(Float3),
				nullptr, 0, 0,
				uvSetNamesNum, puvCoords.data(), sizeCoords.data(), multiUV_texcoord_strides.data(),
				vertexIndices.data(), sizeof(rpr_int),
				normalIndices.data(), sizeof(rpr_int),
				puvIndices.data(), texIndexStride.data(),
				std::vector<int>(vertexIndices.size() / 3, 3).data(), vertexIndices.size() / 3 );
		}
#else
		for (int shaderId = 0; shaderId < elementCount; shaderId++)
		{
			FastVector<Float3> uniqueVertices;
			FastVector<Float3> uniqueNormals;
			FastVector<int> pointIndices;
			FastVector<int> normalIndices;

			std::vector<FastVector<Float2>>	multiUV_texcoords_Temp;
			std::vector<FastVector<int>>	multiUV_indices_Temp;

			// count geometry stats
			int totalTriangleCount = 0;
			for (auto polyId = 0; polyId < fnMesh.numPolygons(); polyId++)
			{
				if (faceMaterialIndices[polyId] != shaderId)
					continue;

				int vertexCount = fnMesh.polygonVertexCount(polyId);
				int triangleCount = vertexCount - 2;

				totalTriangleCount += triangleCount;
			}

			// preallocate arrays
			uniqueVertices.reserve(totalTriangleCount * 3);
			uniqueNormals.reserve(totalTriangleCount * 3);
			pointIndices.reserve(totalTriangleCount * 3);
			normalIndices.reserve(totalTriangleCount * 3);
			multiUV_indices_Temp.resize(uvSetNamesNum);
			multiUV_texcoords_Temp.resize(uvSetNamesNum);
			for (int j = 0; j < (int)uvSetNamesNum; j++)
			{
				multiUV_indices_Temp[j].reserve(totalTriangleCount * 3);
				multiUV_texcoords_Temp[j].reserve(totalTriangleCount * 3);
			}

			for (auto polyId = 0; polyId < fnMesh.numPolygons(); polyId++)
			{
				if (faceMaterialIndices[polyId] != shaderId)
					continue;

				int vertexCount = fnMesh.polygonVertexCount(polyId);
				int triangleCount = vertexCount - 2;

				MIntArray vertexList;
				MIntArray normalList;

				fnMesh.getPolygonVertices(polyId, vertexList);
				fnMesh.getFaceNormalIds(polyId, normalList);
				assert(vertexList.length() == vertexCount);

				for (int triangle = 0; triangle < triangleCount; triangle++)
				{
					int vi[3];
					fnMesh.getPolygonTriangleVertices(polyId, triangle, vi);
					for (int i = 0; i < 3; i++)
					{
						for (unsigned int j = 0; j < vertexList.length(); j++)
						{
							if (vertexList[j] == vi[i])
							{
								vi[i] = j;
								break;
							}
						}
					}

					MFloatPoint pt[3] = {};
					for (int i = 0; i < 3; i++)
						pt[i] = points[vertexList[vi[i]]];

					if (pt[0] == pt[1] || pt[1] == pt[2] || pt[2] == pt[0])
					{
						degenerates++;
						continue;
					}

					for (int i = 0; i < 3; i++)
					{
						int j = vi[i];
						pointIndices.Add(uniqueVertices.Add(pt[i]));
						normalIndices.Add(uniqueNormals.Add(normals[normalList[j]]));

						for (unsigned int k = 0; k < uvSetNamesNum; k++)
						{
							MString *uvSetName = &uvSetNames[k];
							Float2 uv = {};
							fnMesh.getPolygonUV(polyId, j, uv.x, uv.y, uvSetName);
							multiUV_indices_Temp[k].Add(multiUV_texcoords_Temp[k].Add(uv));
						}
					}
				}
			}

			std::vector<const float*>	multiUV_texcoords;
			std::vector<const int*>		multiUV_indices;
			std::vector<size_t>			multiUV_numberOfTexcoords;
			std::vector<int>			multiUV_texcoord_strides;
			std::vector<int>			multiUV_indice_strides;

			for (size_t j = 0; j < uvSetNamesNum; j++)
			{
				multiUV_texcoords.push_back((float*)multiUV_texcoords_Temp[j].data());
				multiUV_indices.push_back(multiUV_indices_Temp[j].data());

				multiUV_numberOfTexcoords.push_back(multiUV_texcoords_Temp[j].size());

				multiUV_texcoord_strides.push_back(sizeof(Float2));
				multiUV_indice_strides.push_back(sizeof(int));
			}

			if (auto triangles = pointIndices.size() / 3)
			{
				elements[shaderId] = context.CreateMeshEx(
					(float*)uniqueVertices.data(), uniqueVertices.size(), sizeof(Float3),
					(float*)uniqueNormals.data(), uniqueNormals.size(), sizeof(Float3),
					nullptr, 0, 0,
					uvSetNamesNum, multiUV_texcoords.data(), multiUV_numberOfTexcoords.data(), multiUV_texcoord_strides.data(),
					pointIndices.data(), sizeof(rpr_int),
					normalIndices.data(), sizeof(rpr_int),
					multiUV_indices.data(), multiUV_indice_strides.data(),
					std::vector<int>(triangles, 3).data(), triangles);

			}
		}
#endif

		if (degenerates > 0)
			DebugPrint("Skipped %d degenerate triangle(s).", degenerates);
#if PROFILE
		float time = (clock() - tick) / (float)CLOCKS_PER_SEC;
		static float totalTime = 0;
		totalTime += time;
		LogPrint("Mesh data built in %g s (total: %g s)", time, totalTime);
#endif

		// Now remove any temporary mesh we created.
		if (!tessellated.isNull())
		{
			FireRenderThread::RunProcOnMainThread([&]
			{
				MFnDagNode parentNode(parent, &mstatus);
				if (MStatus::kSuccess == mstatus)
				{
					mstatus = parentNode.removeChild(tessellated);
					if (MStatus::kSuccess != mstatus)
						mstatus.perror("MFnDagNode::removeChild");
				}

				if (!tessellated.isNull()) // double-check if node hasn't already been removed
					MGlobal::deleteNode(tessellated);
			});
		}

		return elements;
	}

	bool translateLight(FrLight& frlight, Scope& scope, frw::Context frcontext, const MObject& object, const MMatrix& matrix, bool update)
	{
		rpr_int frstatus;
		MStatus mstatus;

		MFnDagNode dagNode(object);
		MDagPath dagPath;
		mstatus = dagNode.getPath(dagPath);
		assert(mstatus == MStatus::kSuccess);

		if (!update)
		{
			frlight.isAreaLight = false;
			if (frlight.areaLight)
				frlight.areaLight.SetAreaLightFlag(false);
		}

		PhysicalLightData lightData;
		FillLightData(lightData, object, scope);

		if (!lightData.enabled)
		{
			return true;
		}
	
		if (lightData.lightType == PLTArea)
		{
			return translateAreaLightInternal(frlight, scope, frcontext, object, matrix, dagPath, lightData, update);
		}
		else
		{
			MColor color = lightData.GetChosenColor() * lightData.GetCalculatedIntensity();

			float mfloats[4][4];
			ScaleMatrixFromCmToMFloats(matrix, mfloats);

			if (lightData.lightType == PLTSpot)
			{
				if (!update)
					frlight.light = frcontext.CreateSpotLight();

				frstatus = rprSpotLightSetConeShape(frlight.light.Handle(), lightData.spotInnerAngle, lightData.spotOuterFallOff);
				checkStatus(frstatus);

				float factor = 1.0f;// MDistance::uiToInternal(0.01);
				frstatus = rprSpotLightSetRadiantPower3f(frlight.light.Handle(),
					static_cast<float>(color.r * factor),
					static_cast<float>(color.g * factor),
					static_cast<float>(color.b * factor));
				checkStatus(frstatus);
			}
			else if (lightData.lightType == PLTDirectional)
			{
				if (!update)
					frlight.light = frcontext.CreateDirectionalLight();

				frstatus = rprDirectionalLightSetRadiantPower3f(frlight.light.Handle(), color.r, color.g, color.b);
				checkStatus(frstatus);

				float softness = lightData.shadowsEnabled ? lightData.shadowsSoftness : 0.0f;
				
				frstatus = rprDirectionalLightSetShadowSoftness(frlight.light.Handle(), softness);
				checkStatus(frstatus);
			}
			else if (lightData.lightType == PLTPoint)
			{
				if (!update)
					frlight.light = frcontext.CreatePointLight();

				float factor = 1.0f;// MDistance::uiToInternal(0.01);
				frstatus = rprPointLightSetRadiantPower3f(frlight.light.Handle(),
					static_cast<float>(color.r * factor * factor),
					static_cast<float>(color.g * factor * factor),
					static_cast<float>(color.b * factor * factor));
				checkStatus(frstatus);
			}
			else if (lightData.lightType == PLTUnknown)
			{
				return false;
			}

			frlight.light.SetTransform((rpr_float*)mfloats);
		}

		return true;
	}

	void FillLightData(PhysicalLightData& physicalLightData, const MObject& node, Scope& scope)
	{
		// Maya's light
		if (node.hasFn(MFn::kLight))
		{
			MStatus mstatus;
			MFnLight fnLight(node);

			physicalLightData.enabled = true;
			physicalLightData.colorMode = PLCColor;
			physicalLightData.intensityUnits = PLTIUWatts;
			physicalLightData.colorBase = fnLight.color();
			physicalLightData.intensity = fnLight.intensity(&mstatus) * LIGHT_SCALE;

			physicalLightData.resultFrwColor = frw::Value(physicalLightData.colorBase.r, physicalLightData.colorBase.g,
															physicalLightData.colorBase.b) * physicalLightData.intensity;

			physicalLightData.areaWidth = 1.0f;
			physicalLightData.areaLength = 1.0f;

			physicalLightData.shadowsEnabled = true;
			physicalLightData.shadowsSoftness = 1.0f;

			assert(mstatus == MStatus::kSuccess);

			if (node.apiType() == MFn::kAreaLight)
			{
				physicalLightData.lightType = PLTArea;
				physicalLightData.areaLightShape = PLARectangle;
			}
			else if (node.apiType() == MFn::kSpotLight)
			{
				physicalLightData.lightType = PLTSpot;

				MFnSpotLight fnSpotLight(node);
				double coneAngle = fnSpotLight.coneAngle(&mstatus) * 0.5;
				assert(mstatus == MStatus::kSuccess);

				double penumbraAngle = coneAngle + fnSpotLight.penumbraAngle(&mstatus);
				assert(mstatus == MStatus::kSuccess);

				physicalLightData.spotInnerAngle = (float)coneAngle;
				physicalLightData.spotOuterFallOff = (float) penumbraAngle;
			}
			else if (node.apiType() == MFn::kDirectionalLight)
			{
				physicalLightData.lightType = PLTDirectional;
			}
			else if (node.apiType() == MFn::kPointLight)
			{
				physicalLightData.lightType = PLTPoint;
			}
			else if (node.apiType() == MFn::kAmbientLight)
			{
				physicalLightData.lightType = PLTUnknown;
			}
			else if (node.apiType() == MFn::kVolumeLight)
			{
				physicalLightData.lightType = PLTUnknown;
			}
		}
		// RPR physical light
		else 
		{
			PhysicalLightAttributes::FillPhysicalLightData(physicalLightData, node, &scope);
		}
	}

	bool translateAreaLightInternal(FrLight& frlight, 
									Scope& scope,
									frw::Context frcontext, 
									const MObject& lightObject, 
									const MMatrix& matrix,
									const MDagPath& dagPath,
									const PhysicalLightData& areaLightData,
									bool update)
	{
		MFnDependencyNode depNode(lightObject);

		MTransformationMatrix trm;

		double scale[3]{ areaLightData.areaWidth, areaLightData.areaWidth, areaLightData.areaLength };
		trm.setScale(scale, MSpace::Space::kObject);

		MMatrix transformMatrix = trm.asMatrix() * matrix;

		float calculatedArea = 1.0f;
		if (update && !frlight.areaLight)
		{
			return false;
		}
		else
		{
            frw::MaterialSystem matSys = scope.MaterialSystem();
			if (!frlight.emissive)
				frlight.emissive = frw::EmissiveShader(matSys);

			if (!frlight.transparent)
				frlight.transparent = frw::EmissiveShader(matSys);

			if (areaLightData.areaLightShape != PLAMesh)
			{
				frlight.areaLight = PhysicalLightGeometryUtility::CreateShapeForAreaLight(areaLightData.areaLightShape, frcontext);
				calculatedArea = PhysicalLightGeometryUtility::GetAreaOfMeshPrimitive(areaLightData.areaLightShape, transformMatrix);
			}
			else
			{
				MDagPath shapeDagPath;
				if (!findMeshShapeForMeshPhysicalLight(dagPath, shapeDagPath))
				{
					return false; // no mesh found
				}

				std::vector<frw::Shape> shapes = TranslateMesh(frcontext, shapeDagPath.node());
				if (shapes.size() > 0)
				{
					frlight.areaLight = shapes[0];
				}
				else
				{
					// bad node probably
					return false;
				}

				transformMatrix = trm.asMatrix() * shapeDagPath.inclusiveMatrix();
				calculatedArea = PhysicalLightGeometryUtility::GetAreaOfMesh(shapeDagPath.node(), transformMatrix);
			}

			frlight.areaLight.SetShader(frlight.emissive);
			frlight.areaLight.SetAreaLightFlag(true);
			frlight.isAreaLight = true;
		}

		if (frlight.areaLight)
		{
			float matrixfloats[4][4];
			ScaleMatrixFromCmToMFloats(transformMatrix, matrixfloats);

			frlight.areaLight.SetTransform((rpr_float*)matrixfloats);
			if (frlight.emissive)
			{
				frlight.emissive.SetValue("color", 
					areaLightData.resultFrwColor * areaLightData.GetCalculatedIntensity(calculatedArea));
			}

			bool shapeVisibility = false;
			MPlug shapeVisibilityPlug = depNode.findPlug("primaryVisibility");
			if (!shapeVisibilityPlug.isNull())
			{
				shapeVisibilityPlug.getValue(shapeVisibility);
			}

			shapeVisibility = shapeVisibility && areaLightData.areaLightVisible;

			frlight.areaLight.SetPrimaryVisibility(shapeVisibility);
			frlight.areaLight.SetShadowFlag(shapeVisibility && areaLightData.shadowsEnabled);
			frlight.areaLight.SetReflectionVisibility(shapeVisibility);
		}

		return true;
	}

	void ScaleMatrixFromCmToMFloats(const MMatrix& matrix, float floats[4][4])
	{
		MStatus mstatus;

		MMatrix m = matrix;
		// convert Maya mesh in cm to m

		ScaleMatrixFromCmToM(m);
		mstatus = m.get(floats);
		assert(mstatus == MStatus::kSuccess);
	}

	void ScaleMatrixFromCmToM(MMatrix& matrix)
	{
		// convert Maya mesh in cm to m
		MMatrix scaleM;
		scaleM.setToIdentity();
		scaleM[0][0] = scaleM[1][1] = scaleM[2][2] = CM_2_M;

		matrix *= scaleM;
	}

	bool getInputColorConnection(const MPlug& colorPlug, MPlug& connectedPlug)
	{
		MPlugArray connections;
		colorPlug.connectedTo(connections, true, false);
		if (connections.length() > 0)
		{
			if (connections[0].node().hasFn(MFn::kAnimCurve))
				return false;
			connectedPlug = connections[0];
			return true;
		}

		if (colorPlug.numChildren() == 0)
			return false;

		MPlug redPlug = colorPlug.child(0);
		redPlug.connectedTo(connections, true, false);
		if (connections.length() > 0)
		{
			if (connections[0].node().hasFn(MFn::kAnimCurve))
				return false;
			connectedPlug = connections[0];
			return true;
		}

		if (colorPlug.numChildren() <= 1)
			return false;

		MPlug greenPlug = colorPlug.child(1);
		greenPlug.connectedTo(connections, true, false);
		if (connections.length() > 0)
		{
			if (connections[0].node().hasFn(MFn::kAnimCurve))
				return false;
			connectedPlug = connections[0];
			return true;
		}

		if (colorPlug.numChildren() <= 2)
			return false;

		MPlug bluePlug = colorPlug.child(2);
		bluePlug.connectedTo(connections, true, false);
		if (connections.length() > 0)
		{
			if (connections[0].node().hasFn(MFn::kAnimCurve))
				return false;
			connectedPlug = connections[0];
			return true;
		}

		return false;
	}

	bool setEnvironmentIBL(
		frw::EnvironmentLight& frlight,
		frw::Context frcontext,
		frw::Image frImage,
		float intensity,
		const MMatrix& matrix,
		bool update)
	{
		if (!update)
			frlight = frcontext.CreateEnvironmentLight();

		if (frImage)
			frlight.SetImage(frImage);

		frlight.SetLightIntensityScale(intensity);

		MMatrix m = matrix;
		// convert Maya mesh in cm to m
		MMatrix scaleM;
		scaleM.setToIdentity();
		scaleM[0][0] = scaleM[1][1] = scaleM[2][2] = 0.01;
		m *= scaleM;
		float mfloats[4][4];
		m.get(mfloats);
		frlight.SetTransform((rpr_float*)mfloats);

		return true;
	}

	bool translateFireRenderIBL(frw::EnvironmentLight& frlight, frw::Image frImage, frw::Context frcontext, const MObject& object, const MMatrix& matrix, bool update)
	{
		float intensity = 1.0;
		{
			MFnDependencyNode nodeFn(object);
			MPlug intensityPlug = nodeFn.findPlug("intensity");
			intensityPlug.getValue(intensity);

			MPlug portalPlug = nodeFn.findPlug("portal");
			MPlugArray portalConnections;
			portalPlug.connectedTo(portalConnections, true, false);
			for (int i = 0; i != portalConnections.length(); ++i) {
				MObject nodeObj = portalConnections[i].node();
			}
		}

		return setEnvironmentIBL(frlight, frcontext, frImage, intensity, matrix, update);
	}

	bool setEnvironmentAmbientLight(
		frw::EnvironmentLight& frlight,
		frw::Context frcontext,
		float intensity,
		MColor colorTemp,
		const MMatrix& matrix,
		bool update)
	{
		if (!update)
			frlight = frcontext.CreateEnvironmentLight();

		frlight.SetAmbientLight(true);

		rpr_image_desc imgDesc;
		imgDesc.image_width = 1;
		imgDesc.image_height = 1;
		imgDesc.image_depth = 0;
		imgDesc.image_row_pitch = imgDesc.image_width * sizeof(rpr_float) * 4;
		imgDesc.image_slice_pitch = 0;

		float imgData[4] = { colorTemp.r, colorTemp.g, colorTemp.b, 1.f };

		frw::Image frImage(frcontext, { 4, RPR_COMPONENT_TYPE_FLOAT32 }, imgDesc, imgData);

		frlight.SetImage(frImage);
		frlight.SetLightIntensityScale(intensity);

		MMatrix m = matrix;
		// convert Maya mesh in cm to m
		MMatrix scaleM;
		scaleM.setToIdentity();
		scaleM[0][0] = scaleM[1][1] = scaleM[2][2] = 0.01;
		m *= scaleM;
		float mfloats[4][4];
		m.get(mfloats);
		frlight.SetTransform((rpr_float*)mfloats);

		return true;
	}

	bool readFireRenderEnvironmentLight(frw::EnvironmentLight& frlight, frw::Context frcontext, const MObject& object, const MMatrix& matrix, bool update)
	{
		MColor color;
		float intensity = 1.0;
		bool display = true;

		{
			MFnDependencyNode nodeFn(object);
			intensity = findPlugTryGetValue(nodeFn, "intensity", 1.0f);
			display = findPlugTryGetValue(nodeFn, "display", true);

			MPlug colorPlug = nodeFn.findPlug("color");

			if (!colorPlug.isNull())
			{
				colorPlug.child(0).getValue(color.r);
				colorPlug.child(1).getValue(color.g);
				colorPlug.child(2).getValue(color.b);
			}
		}

		return setEnvironmentAmbientLight(frlight, frcontext, display ? intensity : 0.0f, color, matrix, update);
	}

	bool translateAmbientLight(frw::EnvironmentLight& frlight, frw::Context frcontext, const MObject& object, const MMatrix& matrix, bool update)
	{
		float intensity = 1.0;
		{
			MFnDependencyNode nodeFn(object);
			MPlug intensityPlug = nodeFn.findPlug("intensity");
			intensityPlug.getValue(intensity);
		}

		MFnLight fnLight(object);
		MColor colorTemp = fnLight.color() /** LIGHT_SCALE*/;

		return setEnvironmentAmbientLight(frlight, frcontext, intensity, colorTemp, matrix, update);
	}

	bool translateWhiteLight(frw::EnvironmentLight& frlight, frw::Context frcontext, const MObject& object, const MMatrix& matrix, bool update)
	{
		MFnDependencyNode nodeFn(object);

		float intensity = 1.0;
		MPlug intensityPlug = nodeFn.findPlug("intensity");
		intensityPlug.getValue(intensity);

		if (!update)
			frlight = frcontext.CreateEnvironmentLight();

		rpr_image_desc imgDesc;
		imgDesc.image_width = 1;
		imgDesc.image_height = 1;
		imgDesc.image_depth = 0;
		imgDesc.image_row_pitch = imgDesc.image_width * sizeof(rpr_float) * 4;
		imgDesc.image_slice_pitch = 0;

		float imgData[4] = { 1.f, 1.f, 1.f, 1.f };

		frw::Image frImage(frcontext, { 4, RPR_COMPONENT_TYPE_FLOAT32 }, imgDesc, imgData);

		frlight.SetImage(frImage);
		frlight.SetLightIntensityScale(intensity);

		MMatrix m = matrix;
		// convert Maya mesh in cm to m
		MMatrix scaleM;
		scaleM.setToIdentity();
		scaleM[0][0] = scaleM[1][1] = scaleM[2][2] = 0.01;
		m *= scaleM;
		float mfloats[4][4];
		m.get(mfloats);
		frlight.SetTransform((rpr_float*)mfloats);

		return true;
	}

	bool translateEnvLight(
		frw::EnvironmentLight& frlight,
		frw::Image& frImage,
		frw::Context frcontext,
		FireMaya::Scope & scope,
		const MObject& object,
		const MMatrix& matrix,
		bool update)
	{
		MFnDependencyNode nodeFn(object);
		if (nodeFn.typeId() == FireMaya::TypeId::FireRenderIBL)
		{
			if (frImage)
				return translateFireRenderIBL(frlight, frImage, frcontext, object, matrix, update);
			else
				return translateWhiteLight(frlight, frcontext, object, matrix, update);
		}
		else if (nodeFn.typeName() == "ambientLight")
		{
			return translateAmbientLight(frlight, frcontext, object, matrix, update);
		}
		else if (nodeFn.typeId() == FireMaya::TypeId::FireRenderEnvironmentLight)
		{
			return readFireRenderEnvironmentLight(frlight, frcontext, object, matrix, update);
		}
		else if (nodeFn.typeName() == FireMaya::VRay::TypeStr::VRayLightDomeShape)
		{
			return FireMaya::VRay::translateVRayLightDomeShape(frlight, frcontext, scope, nodeFn, matrix, update);
		}

		return false;
	}


	bool translateVrayLight(FrLight& frlight, frw::MaterialSystem frMatsys, frw::Context frcontext, const MObject& object, const MMatrix& matrix, bool update)
	{
		using namespace FireMaya::VRay;

		MStatus mstatus;

		MFnDagNode dagNode(object);
		MDagPath dagPath;
		mstatus = dagNode.getPath(dagPath);
		assert(mstatus == MStatus::kSuccess);

		MMatrix m2 = dagPath.inclusiveMatrix(&mstatus);
		MMatrix m = matrix;

		// convert Maya mesh in cm to m
		MMatrix scaleM;
		scaleM.setToIdentity();
		scaleM[0][0] = scaleM[1][1] = scaleM[2][2] = 0.01f;
		m *= scaleM;

		MFnDependencyNode fnLight(object);

		MColor color = readColorOfVRayLight(fnLight);

		if (!update)
			frlight.isAreaLight = false;

		auto typeName = fnLight.typeName();

		if (typeName == TypeStr::VRayLightRectShape)
		{
			if (!translateVRayRectLightShape(frlight, frMatsys, frcontext, fnLight, scaleM, update))
				return false;
		}
		else if (typeName == TypeStr::VRayLightSphereShape)
		{
			if (!translateVRayLightSphereShape(frlight, frMatsys, frcontext, fnLight, scaleM, update))
				return false;
		}
		else if (typeName == TypeStr::VRayLightIESShape || fnLight.typeId() == TypeId::FireRenderIESLightLocator)
		{
			MEulerRotation rotation = MEulerRotation(PI_F / 2.0f, 0.0, 0.0);

			if (!translateVRayLightIESShape(frlight, frMatsys, frcontext, fnLight, scaleM, rotation, update))
				return false;
		}

		return true;
	}

	bool translateSky(
		frw::EnvironmentLight& envLight,
		frw::DirectionalLight& sunLight,
		frw::Image& frImage,
		SkyBuilder& skyBuilder,
		frw::Context frcontext,
		const MObject& object,
		const MMatrix& matrix,
		bool update)
	{
		using namespace FireMaya::VRay;

		// Check that the node is the correct type.
		MFnDependencyNode node(object);
		if (node.typeName() == VRay::TypeStr::VRayGeoSun)
			return translateVRaySunShape(envLight, sunLight, frImage, frcontext, node, matrix, update);
		else if (node.typeId() != TypeId::FireRenderSkyLocator)
			return false;

		// Create lights.
		if (!update)
		{
#ifdef USE_DIRECTIONAL_SKY_LIGHT
			sunLight = frcontext.CreateDirectionalLight();
#endif
			envLight = frcontext.CreateEnvironmentLight();
		}

		// Update the sky image.
		if (skyBuilder.refresh())
		{
			skyBuilder.updateImage(frcontext, frImage);

			// Update the environment light image.
			envLight.SetImage(frImage);
		}

		// Scale lights to RPR space.
		float m[4][4];

		// Transform (rotate) generated IBL image
		MTransformationMatrix t;
		double r1[3] = { 0, -skyBuilder.getSunAzimuth(), 0 };
		t.setRotation(r1, MTransformationMatrix::RotationOrder::kXYZ);
		MMatrix m2 = t.asMatrix() * matrix;
		m2.get(m);
		envLight.SetTransform(reinterpret_cast<rpr_float*>(m));

		// IBL's intensity
		envLight.SetLightIntensityScale(skyBuilder.getSkyIntensity());

#ifdef USE_DIRECTIONAL_SKY_LIGHT
		// Transform the sun light to the sun direction.
		double r2[3] = { -skyBuilder.getSunAltitude(), -skyBuilder.getSunAzimuth(), 0 };
		t.setRotation(r2, MTransformationMatrix::RotationOrder::kXYZ);
		m2 = t.asMatrix() * matrix;
		m2.get(m);
		sunLight.SetTransform(reinterpret_cast<fr_float*>(m));

		// Update the sun light color.
		MColor& c = skyBuilder.getSunLightColor() * skyBuilder.getSkyIntensity();
		sunLight.SetRadiantPower(c.r, c.g, c.b);
#endif

		return true;
	}

}
