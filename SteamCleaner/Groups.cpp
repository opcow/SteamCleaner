/*
	SteamCleaner Steam popup auto-closer.
    Copyright (C) 2013  Mitch Crane

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Groups.h"

CGroups::CGroups(void) : m_count(0)
{

}

CGroups::~CGroups(void)
{
	for (int i = 0; i < m_count; i++)
		delete(m_names[m_count--]);
}

void CGroups::Push(const WCHAR * s, int size)
{
	if (m_count < MAX_NAMES)
	{
		m_names[m_count] = new WCHAR[size+1];
		if (m_names[m_count] != 0)
		{
			wcsncpy_s(m_names[m_count++], size+1, s, size);
		}
	}
}

void CGroups::Clear()
{
	for (int i = 0; i < m_count; i++)
		delete(m_names[m_count--]);
}

int CGroups::Size()
{
	return m_count;
}