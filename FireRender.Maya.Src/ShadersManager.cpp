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
#include "ShadersManager.h"
#include <fstream>

Shader::Shader() :
	m_uShaderId(0),
	m_uShaderVp(0),
	m_uShaderFp(0),
	m_uPrevProgram(0)
{

}

Shader::Shader(const std::string& vsText, const std::string& fsText, const std::string& gText) :
	m_uShaderId(0),
	m_uShaderVp(0),
	m_uShaderFp(0),
	m_uShaderGeo(0),
	m_uPrevProgram(0)
{
	initFromFiles(vsText, fsText, gText);
}

Shader::~Shader()
{
	glDeleteProgram(m_uShaderId);
}


bool Shader::init(const std::string& vsText, const std::string& fsText, const std::string& gText)
{
	bool createGeoShader = false;

	if (gText != "")
		createGeoShader = true;

	if (m_uShaderVp != 0)
		glDeleteShader(m_uShaderVp);

	m_uShaderVp = glCreateShader(GL_VERTEX_SHADER);

	if (m_uShaderFp != 0)
		glDeleteShader(m_uShaderFp);

	m_uShaderFp = glCreateShader(GL_FRAGMENT_SHADER);

	if (m_uShaderGeo != 0)
		glDeleteShader(m_uShaderGeo);

	if (vsText == "" || fsText == "") {
		MGlobal::displayError("Either vertex shader or fragment shader file not found.");
		return false;
	}

	if (createGeoShader)
		m_uShaderGeo = glCreateShader(GL_GEOMETRY_SHADER);

	const char* vS = vsText.c_str();
	const char* fS = fsText.c_str();
	glShaderSource(m_uShaderVp, 1, &vS, 0);
	glShaderSource(m_uShaderFp, 1, &fS, 0);

	if (createGeoShader)
	{
		const char* gS = gText.c_str();
		glShaderSource(m_uShaderGeo, 1, &gS, 0);
	}

	glCompileShader(m_uShaderVp);
	if (!validateShader(m_uShaderVp, vsText))
		return false;

	glCompileShader(m_uShaderFp);
	if (!validateShader(m_uShaderFp, fsText))
		return false;

	if (createGeoShader)
	{
		glCompileShader(m_uShaderGeo);
		if (!validateShader(m_uShaderGeo, gText))
			return false;
	}

	m_uShaderId = glCreateProgram();
	glAttachShader(m_uShaderId, m_uShaderFp);
	glAttachShader(m_uShaderId, m_uShaderVp);

	if (createGeoShader)
		glAttachShader(m_uShaderId, m_uShaderGeo);

#if !(GLSL21) && !(OSMac_)
	glBindFragDataLocation(m_uShaderId, 0, "fragColor");
#endif
	glLinkProgram(m_uShaderId);

	if (validateProgram(m_uShaderId))
	{
		glDeleteShader(m_uShaderFp);
		glDeleteShader(m_uShaderVp);

		if (createGeoShader)
			glDeleteShader(m_uShaderGeo);

		m_uProjectionMatrixLoc = glGetUniformLocation(m_uShaderId, "projMatrix");
		m_uViewMatrixLoc = glGetUniformLocation(m_uShaderId, "viewMatrix");
		m_uNormalMatrixLoc = glGetUniformLocation(m_uShaderId, "normalMatrix");
		return true;
	}

	return false;
}

bool Shader::initFromFiles(const std::string& vsText, const std::string& fsText, const std::string& gText)
{
	std::string vertexShaderSource, fragmentShaderSource, geometryShaderSource;
	if (!loadShader(vsText, vertexShaderSource))
		return false;

	if (!loadShader(fsText, fragmentShaderSource))
		return false;

	if ((gText != "") && (!loadShader(gText, geometryShaderSource)))
		return false;

	return init(vertexShaderSource, fragmentShaderSource, geometryShaderSource);
}

bool Shader::loadShader(const std::string& filePath, std::string& outShaderCode)
{
	outShaderCode.clear();

	std::fstream fs;
	fs.open(filePath.c_str(), std::fstream::in);
	if (!fs.is_open())
	{
		MGlobal::displayError("Could not open "+ MString(filePath.c_str()));
		return false;
	}

	std::string tmpString;
	while (getline(fs, tmpString))
	{
		outShaderCode += tmpString;
		outShaderCode += '\n';
	}
	fs.close();
	return true;
}

unsigned int Shader::id() const
{
	return m_uShaderId;
}

bool Shader::bind()
{
	if (m_uShaderId == 0)
		return false;

	int prog = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
	m_uPrevProgram = prog;
	glUseProgram(m_uShaderId);
	return true;
}

bool Shader::unbind()
{
	glUseProgram(m_uPrevProgram);
	return true;
}

bool Shader::validateShader(GLuint shader, const std::string& source)
{
	const unsigned int BUFFER_SIZE = 1024;
	char buffer[BUFFER_SIZE];
	memset(buffer, 0, BUFFER_SIZE);
	GLsizei length = 0;

	glGetShaderInfoLog(shader, BUFFER_SIZE, &length, buffer);
	if (length > 0)
	{
		MString message = "Shader ";
		message += shader;
		message += " compile error: \n";
		message += buffer;
		MGlobal::displayError(message);
		glDeleteShader(shader);
		return false;
	}

	return true;
}

bool Shader::validateProgram(GLuint program)
{
	const unsigned int BUFFER_SIZE = 1024;
	char buffer[BUFFER_SIZE];
	memset(buffer, 0, BUFFER_SIZE);
	GLsizei length = 0;

	glGetProgramInfoLog(program, BUFFER_SIZE, &length, buffer);
	if (length > 0)
	{
		MString message = "Program ";
		message += program;
		message += " link error: \n";
		message += buffer;
		MGlobal::displayError(message);
		glDeleteProgram(program);
		return false;
	}

	glValidateProgram(program);
	GLint status;
	glGetProgramiv(program, GL_VALIDATE_STATUS, &status);
	if (status == GL_FALSE)
	{
		MString message = "Error validating program ";
		message += program;
		MGlobal::displayError(message);
		glDeleteProgram(program);
		return false;
	}

	return true;
}

ShadersManager::ShaderRefCount::ShaderRefCount() :refCount(0), ptr(NULL)
{
}

ShadersManager::ShaderRefCount::ShaderRefCount(Shader* shader) : refCount(1), ptr(shader)
{
}

ShadersManager::ShaderRefCount::~ShaderRefCount()
{
	if (ptr)
	{
		delete ptr;
	}
}

ShadersManager::ShaderRefCount& ShadersManager::ShaderRefCount::operator++()
{
	refCount++;
	return *this;
}

ShadersManager::ShaderRefCount& ShadersManager::ShaderRefCount::operator--()
{
	refCount--;
	if ((refCount < 1) && ptr)
	{
		delete ptr;
		ptr = NULL;
	}
	return *this;
}

ShadersManager::ShaderRefCount::ShaderRefCount(const ShadersManager::ShaderRefCount& rhs)
{
	this->refCount = rhs.refCount;
	this->ptr = rhs.ptr;
}

ShadersManager::ShaderRefCount& ShadersManager::ShaderRefCount::operator=(const ShadersManager::ShaderRefCount& rhs)
{
	this->refCount = rhs.refCount;
	this->ptr = rhs.ptr;
	return *this;
}
Shader* ShadersManager::loadShader(const std::string& name, const std::string& vsText, const std::string& fsText, const std::string& gText, bool loadFromFile)
{
	std::map<std::string, ShaderRefCount>::iterator it = m_shaders.find(name);
	if (it == m_shaders.end())
	{
		Shader* shaderPtr = new Shader();
		if (loadFromFile)
		{
			if (shaderPtr->initFromFiles(vsText, fsText, gText))
			{
				m_shaders[name] = ShaderRefCount();
				m_shaders[name].ptr = shaderPtr;
				m_shaders[name].refCount = 1;
				return m_shaders[name].ptr;
			}
			else
			{
				return NULL;
			}
		}
		else
		{
			if (shaderPtr->init(vsText, fsText, gText))
			{
				m_shaders[name] = ShaderRefCount();
				m_shaders[name].ptr = shaderPtr;
				m_shaders[name].refCount = 1;
				return m_shaders[name].ptr;
			}
			else
			{
				return NULL;
			}
		}
	}
	else
	{
		++m_shaders[name];
		return m_shaders[name].ptr;
	}
}

void ShadersManager::unloadShader(const std::string& name)
{
	std::map<std::string, ShaderRefCount>::iterator it = m_shaders.find(name);
	if (it != m_shaders.end())
	{
		--m_shaders[name];
		if (m_shaders[name].refCount < 1)
			m_shaders.erase(it);
	}
}
