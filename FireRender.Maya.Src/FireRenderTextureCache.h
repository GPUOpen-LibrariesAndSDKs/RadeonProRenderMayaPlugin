/**********************************************************************
Copyright 2020 Advanced Micro Devices, Inc
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
********************************************************************/
#pragma once

#include <maya/MTextureManager.h>
#include <maya/MShaderManager.h>
#include <map>
#ifndef MAYA2015
#include <maya/MGL.h>
#else
#include <maya/MTypes.h>

#if defined(OSMac_)
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
//class NSOpenGLContext;
//! \brief GL context type (OS X 64-bit)
typedef	void*	MGLContext;

#elif defined (LINUX)
#include <GL/glx.h>
#include <GL/glu.h>
//! \brief GL context type (Linux)
typedef	GLXContext	MGLContext;

#elif defined (_WIN32)
#ifndef APIENTRY
#define MAYA_APIENTRY_DEFINED
#define APIENTRY __stdcall
#endif
#ifndef CALLBACK
#define MAYA_CALLBACK_DEFINED
#define CALLBACK __stdcall
#endif
#ifndef WINGDIAPI
#define MAYA_WINGDIAPI_DEFINED
#define WINGDIAPI __declspec(dllimport)
#endif

#include <gl/Gl.h>
#include <gl/Glu.h>

#ifdef MAYA_APIENTRY_DEFINED
#undef MAYA_APIENTRY_DEFINED
#undef APIENTRY
#endif
#ifdef MAYA_CALLBACK_DEFINED
#undef MAYA_CALLBACK_DEFINED
#undef CALLBACK
#endif
#ifdef MAYA_WINGDIAPI_DEFINED
#undef MAYA_WINGDIAPI_DEFINED
#undef WINGDIAPI
#endif

//! \brief GL context type (Microsoft Windows)
typedef	HGLRC MGLContext;
#else
#error Unknown OS
#endif //OSMac_
#endif
#include <vector>

// Fire render texture cache
// This class contain a map of all the fr_image used in the Maya session
// It uses the textures path as keys

namespace FireMaya
{
	class StoredFrame
	{
		std::vector<float> m_data;
	public:
		StoredFrame() {}
		StoredFrame(int width, int height);

		float* data() { return m_data.data(); }
		operator bool() const { return !m_data.empty(); }
		bool Resize(int width, int height);	// returns true if reallocated

		size_t byteSize() const { return m_data.size() * sizeof(float); }
	};

	class TextureCache
	{
	public:
		enum
		{
			InvalidTexture = 0,
			MaxSize = 1000,
		};

		// clear
		void Clear();

		bool Contains(const char *sz) const;
		StoredFrame& operator[](const char * sz);

	private:
		struct Entry
		{
			StoredFrame frame;
			int age = 0;
		};

		int m_age = 0;
		std::map<std::string, Entry> m_map;
	};
}
