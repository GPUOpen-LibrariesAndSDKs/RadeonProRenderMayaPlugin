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
#include "FireRenderTextureCache.h"

using namespace FireMaya;

bool TextureCache::Contains(const char *sz) const
{
	return m_map.find(sz) != m_map.end();
}

StoredFrame& TextureCache::operator[](const char* sz)
{
	auto& e = m_map[sz];
	e.age = m_age++;

	if (m_map.size() > MaxSize)
	{
		auto oldest = m_map.begin();
		for (auto it = m_map.begin(); it != m_map.end(); ++it)
		{
			if (it->second.age < oldest->second.age)
				oldest = it;
		}
		m_map.erase(oldest);
	}

	return e.frame;
}

bool StoredFrame::Resize(int width, int height)
{
	if (m_data.size() == width * height * 4)
		return false;

	m_data.resize(width * height * 4, 0);
	return true;
}

void TextureCache::Clear()
{
	m_map.clear();
	m_age = 0;
}

StoredFrame::StoredFrame(int width, int height)
	: m_data(width * height * 4, 0)
{
}