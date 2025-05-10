/*
 * VTFCmd
 * Copyright (C) 2005-2010 Neil Jedrzejewski & Ryan Gregg
 * Copyright (C) 2025 Foxysen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

// Foxysen - cut a lot around to fit project needs, currently in "make it fast, break things" state

#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <string>
#include <vector>

#include "IL/il.h"

#include "VTFLib13/VTFLib.h"
#include "VTFLib13/VTFFormat.h"
#include "VTFLib13/VTFWrapper.h"
#include "VTFLib13/VMTWrapper.h"

#include "enumerations.h"

#ifdef __linux__
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif

#define MAX_ITEMS	1024

vlUInt uiFileCount = 0;
vlChar *lpFiles[MAX_ITEMS];							// Files to convert.

vlChar const *lpPrefix = "";								// String to add to start of output file name.
vlChar const *lpPostfix = "";								// String to add to end of output file name.
vlChar *lpOutput = 0;								// Output folder.

vlBool bSilent = vlFalse;							// Don't display output.
vlBool bPause = vlFalse;							// Don't pause the console.
vlBool bHelp = vlFalse;								// Display help.

vlUInt uiVTFImage;									// VTF image handle.
vlUInt uiVMTMaterial;								// VMT material handle.
ILuint uiDevILImage;								// DevIL image handle.

VTFImageFormat AlphaFormat = IMAGE_FORMAT_DXT5;		// VTF image format for alpha textures.
VTFImageFormat NormalFormat = IMAGE_FORMAT_DXT1;	// VTF image format for non-alpha textures.
SVTFCreateOptions CreateOptions;					// VTF creation options.
vlChar *lpShader = 0;								// VMT shader to use.
vlUInt uiParameterCount = 0;
vlChar *lpParameters[MAX_ITEMS][2];					// VMT parameters.
vlChar const *lpExportFormat = "tga";				// Format extension for exporting VTF images.

void Print(const vlChar *lpFormat, ...);

void ProcessFile(const vlChar *lpInputFile);
void ProcessFolder(vlChar *lpInputFolder, vlChar *lpWildcard);

//
// stristr()
// Case insensitive version of strstr().
//
char *stristr(const char *string, const char *strSearch)
{
	const char *ptr = string;
	const char *ptr2;

    while(1)
	{
		ptr = strchr(string, toupper(*strSearch));
		ptr2 = strchr(string, tolower(*strSearch));

		if(ptr == 0)
		{
			ptr = ptr2;
		}
		if(ptr == 0)
		{
			break;
		}
		if(ptr2 && (ptr2 < ptr))
		{
			ptr = ptr2;
		}
		if(!strnicmp(ptr, strSearch, strlen(strSearch)))
		{
			return (char *)ptr;
		}

		string = ptr + 1;
    }

    return 0;
}

//
// strrpl()
// Replace a char in a string with another.
//
void strrpl(char *string, char chr, char rplChr)
{
	while(*string != 0)
	{
		if(*string == chr)
			*string = rplChr;
		string++;
	}
}

int main(int argc, char* argv[])
{
	std::vector<std::string> arg_file_names;
	std::vector<std::string> arg_flags;

	for (int i = 1; i < argc; ++i)
	{
		std::string temp = std::string(argv[i]);
		if (temp.length() < 1) // how
			continue;
		if (temp[0] == '-')
		{
			arg_flags.push_back(temp);
		}
		else
		{
			arg_file_names.push_back(temp);
		}
	}

	if (arg_file_names.size() > 1)
	{
		Print("Too many file names but ok, picking the first\n");
	}
	else if (arg_file_names.size() <= 0)
	{
		Print("No file name, quitting\n");
		return 6;
	}

	std::string file_name = arg_file_names[0];



	VTFImageFormat ImageFormat;			// Temp variable for string to VTFImageFormat test.
	VTFImageFlag ImageFlag;				// Temp variable for string to VTFImageFlag test.


	// Fill in our CreateOptions struct with VTFLib defaults.
	vlImageCreateDefaultCreateStructure(&CreateOptions);

	CreateOptions.bMipmaps = vlFalse;
	CreateOptions.bThumbnail = vlFalse;
	CreateOptions.bReflectivity = vlFalse;
	CreateOptions.uiFlags |= TEXTUREFLAGS_CLAMPS;
	CreateOptions.uiFlags |= TEXTUREFLAGS_CLAMPT;
	CreateOptions.uiFlags |= TEXTUREFLAGS_NOMIP;
	CreateOptions.uiFlags |= TEXTUREFLAGS_NOLOD;

	// This will be uncompressed option
	//AlphaFormat = IMAGE_FORMAT_RGBA8888;
	//NormalFormat = IMAGE_FORMAT_RGB888;


	//ImageFormat = StringToImageFormat(argv[++i]);
	//CreateOptions.uiFlags |= ImageFlag;

	// Check we have the right DLL version.
	if(vlGetVersion() != VL_VERSION)
	{
		Print("Wrong VTFLib version.\n");
		return 1;
	}

	// Initialize VTFLib.
	vlInitialize();
	vlCreateImage(&uiVTFImage);
	vlBindImage(uiVTFImage);
	vlCreateMaterial(&uiVMTMaterial);
	vlBindMaterial(uiVMTMaterial);

	// Initialize DevIL.
	ilInit();
	ilEnable(IL_ORIGIN_SET);  // Filps images that are upside down (by format).
	ilOriginFunc(IL_ORIGIN_UPPER_LEFT);
	ilGenImages(1, &uiDevILImage);
	ilBindImage(uiDevILImage);



	ProcessFile(file_name.c_str());



	// Shutdown DevIL.
	ilDeleteImages(1, &uiDevILImage);
	ilShutDown();

	// Shutdown VTFLib.
	vlDeleteMaterial(uiVMTMaterial);
	vlDeleteImage(uiVTFImage);
	vlShutdown();

	return 0;
}

//
// Print()
// Wrap printf() so we don't have to keep checking for silent mode.
//
void Print(const vlChar *lpFormat, ...)
{
	va_list ArgumentList;

	if(!bSilent)
	{
		va_start(ArgumentList, lpFormat);
		vprintf(lpFormat, ArgumentList);
		va_end(ArgumentList);
	}
}

//
// FlipImage()
// Flip lpImageData over the horizontal axis.
//
void FlipImage(vlByte *lpImageData, vlUInt uiWidth, vlUInt uiHeight, vlUInt uiChannels)
{
	vlUInt i, j, k;
	vlByte bTemp;

	for(i = 0; i < uiWidth; i++)
	{
		for(j = 0; j < uiHeight / 2; j++)
		{
			vlByte *pOne = lpImageData + (i + j * uiWidth) * uiChannels;
			vlByte *pTwo = lpImageData + (i + (uiHeight - j - 1) * uiWidth) * uiChannels;

			for(k = 0; k < uiChannels; k++)
			{
				bTemp = pOne[k];
				pOne[k] = pTwo[k];
				pTwo[k] = bTemp;
			}
		}
	}
}

//
// ProcessFile()
// Convert input file to a vtf file and place it in the output folder.
//
void ProcessFile(const vlChar *lpInputFile)
{
	vlUInt i;

	vlChar lpVTFFile[512];			// Holds output .vtf file name.
	vlChar lpExportFile[512];		// Holds output export file name.

	vlInt iTest;					// Holds .vmt integer test result.
	vlSingle sTest;					// Holds .vmt float test result.
	vlChar cTest[4096];				// Holds .vmt string test result.

	vlSingle sR, sG, sB;			// Reflectivity.
	vlByte *lpImageData;			// Export data.
	VTFImageFormat DestFormat;		// Export format.


	Print("Processing %s...\n", lpInputFile);

	std::string s_input = std::string(lpInputFile);
	auto last_dot = s_input.find_last_of('.');
	Print("%s\n", (s_input.substr(last_dot)).c_str());
	if (last_dot == std::string::npos || s_input.substr(last_dot) != ".vtf")
	{
		// Load input file.
		if(!ilLoadImage(lpInputFile))
		{
			Print(" Error loading input file.\n\n");
			return;
		}

		Print(" Information:\n");

		// Display input file info.
		Print("  Width: %d\n", ilGetInteger(IL_IMAGE_WIDTH));
		Print("  Height: %d\n", ilGetInteger(IL_IMAGE_HEIGHT));
		Print("  BPP: %d\n\n", ilGetInteger(IL_IMAGE_BYTES_PER_PIXEL));

		CreateOptions.ImageFormat = ilGetInteger(IL_IMAGE_BYTES_PER_PIXEL) == 4 ? AlphaFormat : NormalFormat;

		Print(" Creating texture:\n");

		// Convert input file to RGBA.
		if(!ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE))
		{
			Print("  Error converting input file.\n\n");
			return;
		}

		// Create vtf file.
		if(!vlImageCreateSingle((vlUInt)ilGetInteger(IL_IMAGE_WIDTH), (vlUInt)ilGetInteger(IL_IMAGE_HEIGHT), ilGetData(), &CreateOptions))
		{
			Print("  Error creating vtf file:\n%s\n\n", vlGetLastError());
			return;
		}


		// oh output
		std::string s_output;
		if (last_dot != std::string::npos)
			s_output = s_input.substr(0, last_dot) + ".vtf";
		else
			s_output = s_input + ".vtf";


		// Write vtf file.
		Print("  Writing %s...\n", s_output.c_str());
		if(!vlImageSave(s_output.c_str()))
		{
			Print(" Error creating vtf file:\n%s\n\n", vlGetLastError());
			return;
		}
		Print("  %s written.\n\n", s_output.c_str());

	}
	/*else
	{
		if(!vlImageLoad(lpInputFile, vlFalse))
		{
			Print(" Error loading input file:\n%s\n\n", vlGetLastError());
			return;
		}

		Print(" Information:\n");

		// Display input file info.
		Print("  Version: v%u.%u\n", vlImageGetMajorVersion(), vlImageGetMinorVersion());
		Print("  Size On Disk: %.2f KB\n", (vlSingle)vlImageGetSize() / 1024.0f);
		Print("  Width: %u\n", vlImageGetWidth());
		Print("  Height: %u\n", vlImageGetHeight());
		Print("  Depth: %u\n", vlImageGetDepth());
		Print("  Frames: %u\n", vlImageGetFrameCount());
		Print("  Start Frame: %u\n", vlImageGetStartFrame());
		Print("  Faces: %u\n", vlImageGetFaceCount());
		Print("  Mipmaps: %u\n", vlImageGetMipmapCount());
		Print("  Flags: %#.8x\n", vlImageGetFlags());
		Print("  Bumpmap Scale: %.2f\n", vlImageGetBumpmapScale());
		vlImageGetReflectivity(&sR, &sG, &sB);
		Print("  Reflectivity: %.2f, %.2f, %.2f\n", sR, sG, sB);
		Print("  Format: %s\n\n", vlImageGetImageFormatInfo(vlImageGetFormat())->lpName);
		Print("  Resources: %u\n", vlImageGetResourceCount());

		Print(" Creating texture:\n");

		// Figure out which destination format to use.
		DestFormat = (vlImageGetFlags() & (TEXTUREFLAGS_ONEBITALPHA | TEXTUREFLAGS_EIGHTBITALPHA)) ? IMAGE_FORMAT_RGBA8888 : IMAGE_FORMAT_RGB888;

		// Alocate the required memory to convert the vtf to.
		lpImageData = malloc(vlImageComputeImageSize(vlImageGetWidth(), vlImageGetHeight(), 1, 1, DestFormat));

		if(lpImageData == 0)
		{
			Print(" malloc() failed.\n\n");
			return;
		}

		// Convert the .vtf.
		if(!vlImageConvert(vlImageGetData(0, 0, 0, 0), lpImageData, vlImageGetWidth(), vlImageGetHeight(), vlImageGetFormat(), DestFormat))
		{
			free(lpImageData);

			Print(" Error converting input file:\n%s\n\n", vlGetLastError());
			return;
		}

		// DevIL likes the image data upside down.
		FlipImage(lpImageData, vlImageGetWidth(), vlImageGetHeight(), DestFormat == IMAGE_FORMAT_RGBA8888 ? 4 : 3);

		// Create a new image with the converted image data in DevIL.
		if(!ilTexImage(vlImageGetWidth(), vlImageGetHeight(), 1, DestFormat == IMAGE_FORMAT_RGBA8888 ? 4 : 3, DestFormat == IMAGE_FORMAT_RGBA8888 ? IL_RGBA : IL_RGB, IL_UNSIGNED_BYTE, lpImageData))
		{
			free(lpImageData);

			Print("  Error creating %s file.\n\n", lpExportFormat);
			return;
		}

		free(lpImageData);

		CreateOutputPath(lpExportFile, lpInputFile, lpExportFormat);

		// Write tga file.
		Print("  Writing %s...\n", lpExportFile);
		if(!ilSaveImage(lpExportFile))
		{
			Print(" Error creating %s file.\n\n", lpExportFormat);
			return;
		}
		Print("  %s written.\n\n", lpExportFile);
	}*/

	Print("%s processed.\n\n", lpInputFile);
}
