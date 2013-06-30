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

#include <windows.h>
#include <string>

#pragma once

#define MAX_NAMES 20

using namespace std;

class CGroups
{
public:
	CGroups(void);
	~CGroups(void);

	void Push(const WCHAR * s, int size);
	void Clear();
	int CGroups::Size();

	WCHAR * operator[] (int i) { return m_names[i]; };

private:
	int m_count;
	WCHAR * m_names[MAX_NAMES];
};

