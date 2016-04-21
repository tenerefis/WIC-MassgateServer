#include "../stdafx.h"

namespace MC_Misc
{
	uint MultibyteToUnicode(const char *in, wchar_t *out, uint outSize)
	{
		size_t convertedChars = 0;

		mbstowcs_s(&convertedChars, out, outSize, in, _TRUNCATE);

		return convertedChars;
	}

	uint UnicodeToMultibyte(const wchar_t *in, char *out, uint outSize)
	{
		size_t convertedChars = 0;

		wcstombs_s(&convertedChars, out, outSize, in, _TRUNCATE);

		return convertedChars;
	}
}