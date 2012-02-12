#include "Common.hpp"
#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <string>

#ifndef MAKEFOURCC
	#define MAKEFOURCC(ch0, ch1, ch2, ch3) \
		((unsigned int)(unsigned char)(ch0) | ((unsigned int)(unsigned char)(ch1) << 8) | \
		((unsigned int)(unsigned char)(ch2) << 16) | ((unsigned int)(unsigned char)(ch3) << 24))
#endif

#define FOURCC_DXT1 MAKEFOURCC('D', 'X', 'T', '1')
#define FOURCC_DXT2 MAKEFOURCC('D', 'X', 'T', '2')
#define FOURCC_DXT3 MAKEFOURCC('D', 'X', 'T', '3')
#define FOURCC_DXT4 MAKEFOURCC('D', 'X', 'T', '4')
#define FOURCC_DXT5 MAKEFOURCC('D', 'X', 'T', '5')

#define DDS_CUBEMAP_POSITIVEX 0x00000600 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEX
#define DDS_CUBEMAP_NEGATIVEX 0x00000a00 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEX
#define DDS_CUBEMAP_POSITIVEY 0x00001200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEY
#define DDS_CUBEMAP_NEGATIVEY 0x00002200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEY
#define DDS_CUBEMAP_POSITIVEZ 0x00004200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEZ
#define DDS_CUBEMAP_NEGATIVEZ 0x00008200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEZ

#define DDS_CUBEMAP_ALLFACES ( DDS_CUBEMAP_POSITIVEX | DDS_CUBEMAP_NEGATIVEX |\
                               DDS_CUBEMAP_POSITIVEY | DDS_CUBEMAP_NEGATIVEY |\
                               DDS_CUBEMAP_POSITIVEZ | DDS_CUBEMAP_NEGATIVEZ )

#define CR_DDS_PIXEL_DATA_OFFSET 128
#define CR_DDS_HEIGHT_OFFSET 12
#define CR_DDS_WIDTH_OFFSET 16
#define CR_LINEAR_SIZE_OFFSET 20
#define CR_DEPTH_OFFSET 24
#define CR_MIPMAP_COUNT_OFFSET 28
#define CR_FOURCC_OFFSET 84
#define CR_CUBEMAP_FLAGS_OFFSET 92+20

struct DDSData
{
    int	Width;
    int	Height;
    int	Components;
    GLenum Format;
    int	NumMipMaps;
    unsigned char* Pixels;
};

static DDSData *ReadCompressedTexture(const char *pFileName);

Texture LoadTexture(std::string ddsFilename)
{
    Texture texture;
    DDSData *pDDSImageData = ReadCompressedTexture(ddsFilename.c_str());

    PezCheckCondition(pDDSImageData != 0, "Unable to load DDS file %s\n", ddsFilename.c_str());

    int iSize;
    int iBlockSize;
    int iOffset		= 0;
    int iHeight		= pDDSImageData->Height;
    int iWidth		= pDDSImageData->Width;
    int iNumMipMaps = pDDSImageData->NumMipMaps;

    iBlockSize = (pDDSImageData->Format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) ? 8 : 16;

    glGenTextures(1, &texture.Handle);
    glBindTexture(GL_TEXTURE_2D, texture.Handle);

    if(iNumMipMaps < 2)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        iNumMipMaps = 1;
    }
    else
    {
        //Set texture filtering.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    for(int i = 0; i < iNumMipMaps; i++)
    {
        iSize = ((iWidth + 3) / 4) * ((iHeight + 3) / 4) * iBlockSize;
        glCompressedTexImage2D(GL_TEXTURE_2D, i, pDDSImageData->Format, iWidth, iHeight, 0, iSize, pDDSImageData->Pixels + iOffset);
        iOffset += iSize;

        //Scale next level.
        iWidth  /= 2;
        iHeight /= 2;
    }

    texture.Format = pDDSImageData->Format;
    texture.Width = pDDSImageData->Width;
    texture.Height = pDDSImageData->Height;

    if(pDDSImageData->Pixels)
        delete[] pDDSImageData->Pixels;

    delete pDDSImageData;

    return texture;
}

DDSData *ReadCompressedTexture(const char *pFileName)
{
	FILE *pFile;
	if(!(pFile = fopen(pFileName, "rb")))
		return NULL;

	char pFileCode[4];
	fread(pFileCode, 1, 4, pFile);
	if(strncmp(pFileCode, "DDS ", 4) != 0)
		return NULL;

	//Get the descriptor.
	unsigned int uiFourCC;
	fseek(pFile, CR_FOURCC_OFFSET, SEEK_SET);
	fread(&uiFourCC, sizeof(unsigned int), 1, pFile);

	unsigned int uiLinearSize;
	fseek(pFile, CR_LINEAR_SIZE_OFFSET, SEEK_SET);
	fread(&uiLinearSize, sizeof(unsigned int), 1, pFile);

	unsigned int uiMipMapCount;
	fseek(pFile, CR_MIPMAP_COUNT_OFFSET, SEEK_SET);
	fread(&uiMipMapCount, sizeof(unsigned int), 1, pFile);

	unsigned int uiWidth;
	fseek(pFile, CR_DDS_WIDTH_OFFSET, SEEK_SET);
	fread(&uiWidth, sizeof(unsigned int), 1, pFile);

	unsigned int uiHeight;
	fseek(pFile, CR_DDS_HEIGHT_OFFSET, SEEK_SET);
	fread(&uiHeight, sizeof(unsigned int), 1, pFile);

    unsigned int uiCubemapFlags;
	fseek(pFile, CR_CUBEMAP_FLAGS_OFFSET, SEEK_SET);
	fread(&uiCubemapFlags, sizeof(uiCubemapFlags), 1, pFile);

    int iFactor;
	int iBufferSize;
	
	DDSData *pDDSImageData = new DDSData;
	memset(pDDSImageData, 0, sizeof(DDSData));
	
	switch(uiFourCC)
	{
		case FOURCC_DXT1:
			pDDSImageData->Format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
			iFactor = 2;
			break;

		case FOURCC_DXT3:
			pDDSImageData->Format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
			iFactor = 4;
			break;

		case FOURCC_DXT5:
			pDDSImageData->Format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
			iFactor = 4;
			break;

        // http://msdn.microsoft.com/en-us/library/bb943991.aspx
        case 113:
			pDDSImageData->Format = GL_RGBA16F;
            uiLinearSize = 0;
            {
                unsigned int w = uiWidth;
                unsigned int h = uiHeight;
                unsigned int lod = uiMipMapCount;
                while (lod-- && w && h) {
                    uiLinearSize += w * h * 8;
                    w = w >> 1; h = h >> 1;
                }
            }
            iFactor = 1;
            if (uiCubemapFlags == DDS_CUBEMAP_ALLFACES)
                uiLinearSize *= 6;
            break;

		default:
			return NULL;
	}
	
	iBufferSize = (uiMipMapCount > 1) ? (uiLinearSize * iFactor) : uiLinearSize;
	pDDSImageData->Pixels = new unsigned char[iBufferSize];
	
	pDDSImageData->Width      = uiWidth;
	pDDSImageData->Height     = uiHeight;
	pDDSImageData->NumMipMaps = uiMipMapCount;
	pDDSImageData->Components = (uiFourCC == FOURCC_DXT1) ? 3 : 4;

	fseek(pFile, CR_DDS_PIXEL_DATA_OFFSET, SEEK_SET);
	fread(pDDSImageData->Pixels, 1, iBufferSize, pFile);
	fclose(pFile);

	return pDDSImageData;
}
