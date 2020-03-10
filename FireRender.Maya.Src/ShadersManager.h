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

#include <GL/glew.h>
#include <maya/M3dView.h>

#include <maya/MMatrix.h>
#include <maya/MVector.h>
#include <maya/MGlobal.h>

#include <vector>
#include <string>
#include <map>

/** OpenGL shader program
*
* Initialize and handle a glProgram given a vertex and fragment shader
*/
class Shader
{
public:

	/** Constructor
	*/
	Shader();

	/** Constructor
	*
	* Construct the shader using a vertex and fragment shader
	* @param[in] vsText Vertex shader source code
	* @param[in] fsText Fragment shader source code
	* @param[in] gText Geometry shader source code
	*/
	Shader(const std::string& vsText, const std::string& fsText, const std::string& gText);

	/** Destructor
	*/
	~Shader();

	/** Init the glProgram using a vertex and fragment shader
	*
	* @param[in] vsText Vertex shader source code
	* @param[in] fsText Fragment shader source code
	* @param[in] gText Geometry shader source code
	*/
	bool init(const std::string& vsText, const std::string& fsText, const std::string& gText);

	/** Init the glProgram using a vertex and fragment and vertex shader on external files
	*
	* @param[in] vsText Vertex shader source code
	* @param[in] fsText Fragment shader source code
	* @param[in] gText Geometry shader source code
	*/
	bool initFromFiles(const std::string& vsText, const std::string& fsText, const std::string& gText);

	/** Bind the glProgram
	*/
	bool bind();

	/** Unbind the gl program
	*
	* Revert to the glProgram that was active before calling the bind
	*/
	bool unbind();

	/** Get glProgram id
	* @return unsigned int Return the shader glProgram id
	*/
	unsigned int id() const;

	inline unsigned int projectMatrixLocation() const
	{
		return m_uProjectionMatrixLoc;
	}

	inline unsigned int viewMatrixLocation() const
	{
		return m_uViewMatrixLoc;
	}

	inline unsigned int normalMatrixLocation() const
	{
		return m_uNormalMatrixLoc;
	}

private:

	/** Load shader source code from file
	*/
	bool loadShader(const std::string& filePath, std::string& outShaderCode);

	/** Validate the glProgram
	*
	* @param[in] program gl program id
	*/
	bool validateProgram(GLuint program);

	/** Validate the shader
	*
	* @param[in] shader gl shader id
	* @param[in] source code of the shader
	*/
	bool validateShader(GLuint shader, const std::string &source);

private:

	/** Shader id
	*/
	unsigned int m_uShaderId;

	/** vertex shader id
	*/
	unsigned int m_uShaderVp;

	/** fragment shader id
	*/
	unsigned int m_uShaderFp;

	/** fragment shader id
	*/
	unsigned int m_uShaderGeo;

	/** previous glProgram active before call the bind function
	*/
	unsigned int m_uPrevProgram;

	unsigned int m_uViewMatrixLoc;

	unsigned int m_uProjectionMatrixLoc;

	unsigned int m_uNormalMatrixLoc;
};

/** Shader Manager
*
* Singleton object that store gl shader instances
*/
class ShadersManager
{
public:

	/** Singleton interface
	*
	*/
	static ShadersManager& instance()
	{
		static ShadersManager  inst;
		return inst;
	}

	/** Load a shader
	*
	* Load a shader using a vertex/fragment shader, and store in the map
	* If a shader is already loaded it will increment the reference counter
	* and return the shader pointer from the map
	*
	* @param name[in] Shader name
	* @param vsText[in] vertex shader file path
	* @param fsText[in] fragment shader file path
	* @return pointer to the shader loaded
	*/
	Shader* loadShader(const std::string& name, const std::string& vsText, const std::string& fsText, const std::string& gText, bool loadFromFile = true);

	/** Unload a shader
	*
	* Unload a shader from the map, if the reference count is 1, than it will delete the shader
	* and remove the entry from the map, otherwise it will decrement just the reference counter
	*
	* @param name[in] Shader name
	*/
	void unloadShader(const std::string& name);

private:

	/** Constructor
	*/
	ShadersManager() {}

	/** Copy constructor - NOT IMPLEMENTED
	*/
	ShadersManager(const ShadersManager&) = delete;

	/** Destructor
	*/
	~ShadersManager() {}

	/** Assignment operator - NOT IMPLEMENTD
	*/
	ShadersManager& operator=(const ShadersManager&) = delete;

private:

	/** Shader reference counter class
	*
	* A class that store a pointer to a shader object
	* and a reference counter to it
	*/
	class ShaderRefCount
	{
	public:

		/** Constructor
		*/
		ShaderRefCount();

		/** Constructor
		*
		* @param shader[in] Shader pointer
		*/
		ShaderRefCount(Shader* shader);

		/** Copy constructor
		*/
		ShaderRefCount(const ShaderRefCount& rhs);

		/** Assign operator
		*/
		ShaderRefCount& operator=(const ShaderRefCount& rhs);

		/** Destructor
		*/
		~ShaderRefCount();

		/** Increment reference counter by 1
		*/
		ShaderRefCount& operator++();

		/** Decrement reference counter by 1
		*
		* Decrement the reference counter to 1 and if the counter
		* goes to zero delete the shader
		*/
		ShaderRefCount& operator--();

	public:

		/** Reference counter
		*/
		unsigned int refCount;

		/** Shader pointer
		*/
		Shader* ptr;
	};


	/** Shader map
	*/
	std::map<std::string, ShaderRefCount> m_shaders;
};
