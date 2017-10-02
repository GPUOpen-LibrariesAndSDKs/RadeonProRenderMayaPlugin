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
			float focusDistance = static_cast<float>(fnCamera.focusDistance(&mstatus) * 0.01);
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
		eye = eye * 0.01;
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

			bool useChordHeight = attributes.getBool("useChordHeight");
			bool useChordHeightRatio = attributes.getBool("useChordHeightRatio");
			bool edgeSwap = attributes.getBool("edgeSwap");
			bool useMinScreen = attributes.getBool("useMinScreen");

			double chordHeight = attributes.getDouble("chordHeight");
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


	bool translateLight(FrLight& frlight, frw::MaterialSystem frMatsys, frw::Context frcontext, const MObject& object, const MMatrix& matrix, bool update)
	{
		rpr_int frstatus;
		MStatus mstatus;

		MFnDagNode dagNode(object);
		MDagPath dagPath;
		mstatus = dagNode.getPath(dagPath);
		assert(mstatus == MStatus::kSuccess);

		MMatrix m = matrix;
		assert(mstatus == MStatus::kSuccess);
		// convert Maya mesh in cm to m
		MMatrix scaleM;
		scaleM.setToIdentity();
		scaleM[0][0] = scaleM[1][1] = scaleM[2][2] = 0.01;
		m *= scaleM;
		float mfloats[4][4];
		mstatus = m.get(mfloats);
		assert(mstatus == MStatus::kSuccess);

		MFnLight fnLight(object);
		MColor color = fnLight.color() * fnLight.intensity(&mstatus) * LIGHT_SCALE;

		assert(mstatus == MStatus::kSuccess);
		MFn::Type lightType = object.apiType();
		if (!update)
		{
			frlight.isAreaLight = false;
			if (frlight.areaLight)
				frlight.areaLight.SetAreaLightFlag(false);
		}

		if (object.apiType() == MFn::kAreaLight)
		{
			// Fire Render's area light is implemented as a mesh with emissive shader
			if (update)
			{
				if (!frlight.areaLight)
				{
					return false;
				}

				frlight.areaLight.SetTransform((rpr_float*)mfloats);
				if (frlight.emissive)
					frlight.emissive.SetValue("color", frw::Value(color.r, color.g, color.b));
			}
			else
			{
				if (!frlight.emissive)
					frlight.emissive = frw::EmissiveShader(frMatsys);

				if (!frlight.transparent)
					frlight.transparent = frw::EmissiveShader(frMatsys);

				//create transparent shader for portals

				struct vertex
				{
					rpr_float pos[3];
					rpr_float norm[3];
					rpr_float tex[2];
				};

				static const vertex quad[] =
				{
					{ -1.f, -1.f, 0.f,		0.f, 0.f, -1.f,			0.f, 0.f },
					{ 1.f, -1.f, 0.f,		0.f, 0.f, -1.f,			1.f, 0.f },
					{ 1.f,  1.f, 0.f,		0.f, 0.f, -1.f,			1.f, 1.f },
					{ -1.f,  1.f, 0.f,		0.f, 0.f, -1.f,			0.f, 1.f }
				};

				static const rpr_int indices[] =
				{
					0, 2, 1,
					0, 3, 2
				};

				static const rpr_int num_face_vertices[] =
				{
					3, 3
				};

				frlight.areaLight = frcontext.CreateMesh(
					(rpr_float const*)&quad[0], 24, sizeof(vertex),
					(rpr_float const*)((char*)&quad[0] + sizeof(rpr_float) * 3), 24, sizeof(vertex),
					(rpr_float const*)((char*)&quad[0] + sizeof(rpr_float) * 6), 24, sizeof(vertex),
					(rpr_int const*)indices, sizeof(rpr_int),
					(rpr_int const*)indices, sizeof(rpr_int),
					(rpr_int const*)indices, sizeof(rpr_int),
					num_face_vertices, 2);

				frlight.areaLight.SetTransform((rpr_float*)mfloats);
				frlight.areaLight.SetShader(frlight.emissive);
				frlight.areaLight.SetAreaLightFlag(true);
				frlight.emissive.SetValue("color", frw::Value(color.r, color.g, color.b));
				frlight.isAreaLight = true;
			}

			if (frlight.areaLight)
			{
				bool shapeVisibility = false;
				MPlug shapeVisibilityPlug = fnLight.findPlug("primaryVisibility");
				if (!shapeVisibilityPlug.isNull())
					shapeVisibilityPlug.getValue(shapeVisibility);

				frlight.areaLight.SetPrimaryVisibility(shapeVisibility);
				frlight.areaLight.SetShadowFlag(shapeVisibility);
			}
		}
		else
		{
			if (object.apiType() == MFn::kSpotLight)
			{
				if (!update)
					frlight.light = frcontext.CreateSpotLight();

				MFnSpotLight fnSpotLight(object);
				double coneAngle = fnSpotLight.coneAngle(&mstatus) * 0.5;
				assert(mstatus == MStatus::kSuccess);

				double penumbraAngle = coneAngle + fnSpotLight.penumbraAngle(&mstatus);

				assert(mstatus == MStatus::kSuccess);
				frstatus = rprSpotLightSetConeShape(frlight.light.Handle(), static_cast<float>(coneAngle), static_cast<float>(penumbraAngle));
				checkStatus(frstatus);

				auto factor = MDistance::uiToInternal(0.01);
				frstatus = rprSpotLightSetRadiantPower3f(frlight.light.Handle(),
					static_cast<float>(color.r * factor),
					static_cast<float>(color.g * factor),
					static_cast<float>(color.b * factor));
				checkStatus(frstatus);
			}
			else if (object.apiType() == MFn::kDirectionalLight)
			{
				if (!update)
					frlight.light = frcontext.CreateDirectionalLight();

				frstatus = rprDirectionalLightSetRadiantPower3f(frlight.light.Handle(), color.r, color.g, color.b);
				checkStatus(frstatus);
			}
			else if (object.apiType() == MFn::kPointLight)
			{
				if (!update)
					frlight.light = frcontext.CreatePointLight();
				auto factor = MDistance::uiToInternal(0.01);
				frstatus = rprPointLightSetRadiantPower3f(frlight.light.Handle(),
					static_cast<float>(color.r * factor * factor),
					static_cast<float>(color.g * factor * factor),
					static_cast<float>(color.b * factor * factor));
				checkStatus(frstatus);
			}
			else if (object.apiType() == MFn::kAmbientLight) {
				return false;
			}
			else if (object.apiType() == MFn::kVolumeLight) {
				return false;
			}

			frlight.light.SetTransform((rpr_float*)mfloats);
		}

		return true;
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
		rpr_int frstatus = RPR_SUCCESS;

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
		rpr_int frstatus = RPR_SUCCESS;

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
		rpr_int frstatus = RPR_SUCCESS;

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

		bool hasShape = object.hasFn(MFn::kShape);

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