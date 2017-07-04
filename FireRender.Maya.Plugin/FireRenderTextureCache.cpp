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