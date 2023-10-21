#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <random>
#include <vector>
#include <random>
#include <direct.h>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

static const int c_defaultMaxDepthOffset = 20;

bool CreateAutostereogram(
	const unsigned char* srcColor, int srcColorWidth, int srcColorHeight, int srcColorChannels,
	const unsigned char* srcDepth, int srcDepthWidth, int srcDepthHeight, int srcDepthChannels,
	int maxDepthOffset,
	std::vector<unsigned char>& outPixels)
{
	if (!srcColor || !srcDepth || srcColorChannels > 4 || srcDepthChannels != 1)
		return false;

	outPixels.resize(srcDepthWidth * srcDepthHeight * srcColorChannels);

	for (int iy = 0; iy < srcDepthHeight; ++iy)
	{
		const unsigned char* srcDepthPixel = &srcDepth[iy * srcDepthWidth * srcDepthChannels];
		unsigned char* outPixel = &outPixels[iy * srcDepthWidth * srcColorChannels];

		int srcColorY = iy % srcColorHeight;

		for (int ix = 0; ix < srcDepthWidth; ++ix)
		{
			int depthOffset = int((float(*srcDepthPixel) / 255.0f) * float(maxDepthOffset));

			int srcColorX = (ix + srcColorWidth - depthOffset) % srcColorWidth;

			const unsigned char* srcColorPixel = &srcColor[srcColorY * srcColorWidth * srcColorChannels + srcColorX * srcColorChannels];
			memcpy(outPixel, srcColorPixel, srcColorChannels);

			// for debugging the depth map
			//for (int i = 0; i < srcColorChannels; ++i)
				//outPixel[i] = *srcDepthPixel;

			srcDepthPixel += srcDepthChannels;
			outPixel += srcColorChannels;
		}
	}

	return true;
}

bool CreateAutostereogram(
	const char* srcColorFileName,
	const char* srcDepthFileName,
	bool invertDepth,
	int padXMultiplier,
	int padYMultiplier,
	int maxDepthOffset,
	const char* outFileName)
{
	int srcColorW, srcColorH, srcColorC;
	unsigned char* srcColor = stbi_load(srcColorFileName, &srcColorW, &srcColorH, &srcColorC, 3);
	srcColorC = 3;

	int srcDepthW, srcDepthH, srcDepthC;
	unsigned char* srcDepth = stbi_load(srcDepthFileName, &srcDepthW, &srcDepthH, &srcDepthC, 1);
	srcDepthC = 1;
	unsigned char* srcDepthOrig = srcDepth;

	if (invertDepth)
	{
		for (size_t i = 0; i < srcDepthW * srcDepthH * srcDepthC; ++i)
			srcDepth[i] = 255 - srcDepth[i];
	}

	// Make depth be 2x in size and put the original in the middle, if we should
	std::vector<unsigned char> paddedDepth;
	if (padXMultiplier > 1 || padYMultiplier > 1)
	{
		paddedDepth.resize(srcDepthW * srcDepthH * srcDepthC * padXMultiplier * padYMultiplier, 0);
		size_t ixOffset = (srcDepthW * padXMultiplier - srcDepthW)/ 2;
		size_t iyOffset = (srcDepthH * padYMultiplier - srcDepthH)/ 2;
		for (size_t iy = 0; iy < srcDepthH; ++iy)
		{
			const unsigned char* src = &srcDepth[iy * srcDepthW * srcDepthC];
			unsigned char* dest = &paddedDepth[(iy + iyOffset) * srcDepthW * padXMultiplier * srcDepthC + ixOffset * srcDepthC];
			memcpy(dest, src, srcDepthW * srcDepthC);
		}
		srcDepthW *= padXMultiplier;
		srcDepthH *= padYMultiplier;
		srcDepth = paddedDepth.data();
	}

	// TODO: testing
	//maxDepthOffset = srcColorW;

	std::vector<unsigned char> outPixels;
	bool ret = CreateAutostereogram(srcColor, srcColorW, srcColorH, srcColorC, srcDepth, srcDepthW, srcDepthH, srcDepthC, maxDepthOffset, outPixels);

	if (ret)
		ret = (stbi_write_png(outFileName, srcDepthW, srcDepthH, srcColorC, outPixels.data(), 0) != 0);

	stbi_image_free(srcColor);
	stbi_image_free(srcDepthOrig);

	if (!ret)
		printf("Could not create %s!\n", outFileName);
	else
		printf("%s saved\n", outFileName);

	return ret;
}

void GenerateWhiteNoiseTextures()
{
	std::mt19937 rng;
	std::uniform_int_distribution<int> dist(0, 255);

	static const size_t c_whiteNoiseSize = 128;

	std::vector<unsigned char> wn(c_whiteNoiseSize * c_whiteNoiseSize * 3);
	for (unsigned char& v : wn)
		v = dist(rng);

	stbi_write_png("Assets/color_whiteNoise.png", c_whiteNoiseSize, c_whiteNoiseSize, 3, wn.data(), 0);

	stbi_write_png("Assets/grey_whiteNoise.png", c_whiteNoiseSize, c_whiteNoiseSize, 1, wn.data(), 0);
}

int main(int argc, char** argv)
{
	_mkdir("out");

	//GenerateWhiteNoiseTextures();

	struct Texture
	{
		const char* filename;
		const char* shortName;
	};

	const Texture colorPatterns[] =
	{
		{ "Assets/color_whiteNoise.png", "color_whiteNoise" },
		{ "Assets/grey_whiteNoise.png", "grey_whiteNoise" },
		{ "Assets/grey_blueNoise.png", "grey_blueNoise" },
		{ "Assets/color_candy.jpg", "color_candy" },
		{ "Assets/color_pumpkins.jpg", "color_pumpkins" },
	};

	struct DepthTexture
	{
		const char* filename;
		const char* shortName;
		bool reverseDepth;
		int padX;
		int padY;
	};

	const DepthTexture depthImages[] =
	{
		{ "Assets/bw_witch.jpg", "witch", true, 2, 2 },
		{ "Assets/bw_house.jpg", "house", true, 4, 2 },
		{ "Assets/grey_grave.jpg", "grave", false, 2, 2 },
		{ "Assets/grey_portrait.jpg", "portrait", false, 4, 2 },
		{ "Assets/grey_squares.png", "squares", false, 2, 2 },
	};

	for (const Texture& colorTexture : colorPatterns)
	{
		for (const DepthTexture& depthTexture : depthImages)
		{
			std::string outFilename = std::string("out/") + depthTexture.shortName + "_" + colorTexture.shortName + ".png";

			CreateAutostereogram(colorTexture.filename, depthTexture.filename, depthTexture.reverseDepth, depthTexture.padX, depthTexture.padY, c_defaultMaxDepthOffset, outFilename.c_str());
		}
	}

	return 0;
}

/*
TODO:
- try with a blue noise point set src image
- try with a blue noise texture src image
- also white noise?

- halloween themed
- black and white depth maps
- greyscale depth maps
- halloween themed src image? like candy?

*/