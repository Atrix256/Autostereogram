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

static const int c_maxDepthOffset = 20;
static const bool c_drawHelperDots = true;

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
		unsigned char* outPixelRow = outPixel;

		int srcColorY = iy % srcColorHeight;

		for (int ix = 0; ix < srcDepthWidth; ++ix)
		{
			int depthOffset = int((float(*srcDepthPixel) / 255.0f) * float(maxDepthOffset));

			const unsigned char* srcColorPixel = nullptr;
			if ((ix + depthOffset - srcColorWidth) < srcColorWidth)
			{
				int srcColorX = (ix + depthOffset) % srcColorWidth;
				srcColorPixel = &srcColor[srcColorY * srcColorWidth * srcColorChannels + srcColorX * srcColorChannels];
			}
			else
			{
				int srcColorX = (ix + depthOffset - srcColorWidth);
				srcColorPixel = &outPixelRow[srcColorX * srcColorChannels];
			}

			memcpy(outPixel, srcColorPixel, srcColorChannels);

			// for debugging the depth map
			//for (int i = 0; i < srcColorChannels; ++i)
				//outPixel[i] = *srcDepthPixel;

			srcDepthPixel += srcDepthChannels;
			outPixel += srcColorChannels;
		}
	}

	// draw the two dots to overlap
	if (c_drawHelperDots)
	{
		int dotsDistance = srcColorWidth;

		int dot1x = srcDepthWidth / 2 - dotsDistance / 2;
		int dotsy = 15;
		int dot2x = srcDepthWidth / 2 + dotsDistance / 2;

		for (int iy = -10; iy <= 10; ++iy)
		{
			unsigned char* dest = &outPixels[(iy + dotsy) * srcDepthWidth * srcColorChannels];
			for (int ix = -10; ix <= 10; ++ix)
			{
				if ((iy * iy + ix * ix) < 100)
				{
					for (int i = 0; i < srcColorChannels; ++i)
					{
						dest[(ix + dot1x) * srcColorChannels + i] = 192;
						dest[(ix + dot2x) * srcColorChannels + i] = 255;
					}
				}
			}
		}
	}

	return true;
}

bool CreateAutostereogram(
	const char* srcColorFileName,
	const char* srcDepthFileName,
	bool invertDepth,
	bool normalizeDepth,
	bool binarizeDepth,
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

	if (normalizeDepth)
	{
		unsigned char min = srcDepth[0];
		unsigned char max = srcDepth[0];
		for (size_t i = 0; i < srcDepthW * srcDepthH * srcDepthC; ++i)
		{
			min = std::min(min, srcDepth[i]);
			max = std::max(max, srcDepth[i]);
		}
		for (size_t i = 0; i < srcDepthW * srcDepthH * srcDepthC; ++i)
		{
			float percent = float(srcDepth[i] - min) / float(max - min);
			srcDepth[i] = (unsigned char)(percent * 255.0f);
		}
	}

	if (binarizeDepth)
	{
		for (size_t i = 0; i < srcDepthW * srcDepthH * srcDepthC; ++i)
			srcDepth[i] = srcDepth[i] < 128 ? 0 : 255;
	}

	// Re-center the image by padding it on the left side
	std::vector<unsigned char> paddedDepth;
	{
		int newSrcDepthW = srcDepthW + srcColorW;
		paddedDepth.resize(newSrcDepthW * srcDepthH * srcDepthC, 0);
		size_t ixOffset = srcColorW;
		for (size_t iy = 0; iy < srcDepthH; ++iy)
		{
			const unsigned char* src = &srcDepth[iy * srcDepthW * srcDepthC];
			unsigned char* dest = &paddedDepth[iy * newSrcDepthW * srcDepthC + ixOffset * srcDepthC];
			memcpy(dest, src, srcDepthW * srcDepthC);
		}
		srcDepthW = newSrcDepthW;
		srcDepth = paddedDepth.data();
	}

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
		bool normalizeDepth;
		bool binarize;
	};

	const DepthTexture depthImages[] =
	{
		{ "Assets/bw_witch.jpg", "witch", true, false, true },
		{ "Assets/bw_house.jpg", "house", true, false, true },
		{ "Assets/grey_grave.jpg", "grave", false, true, false },
		{ "Assets/grey_yoda.jpg", "yoda", false, true, false },
		{ "Assets/grey_art.jpg", "art", false, true, false },
		{ "Assets/grey_squares.png", "squares", false, true, false },
	};

	for (const Texture& colorTexture : colorPatterns)
	{
		for (const DepthTexture& depthTexture : depthImages)
		{
			std::string outFilename = std::string("out/") + depthTexture.shortName + "_" + colorTexture.shortName + ".png";

			CreateAutostereogram(colorTexture.filename, depthTexture.filename, depthTexture.reverseDepth, depthTexture.normalizeDepth, depthTexture.binarize, c_maxDepthOffset, outFilename.c_str());
		}
	}

	return 0;
}

/*
Notes:
* the helper dots are the size of the color tile texture. Making that color texture big makes it real hard to see things!
* it shifts the image to the left, so i had to add an extra tile to the left to recenter it.
* blue noise doesn't work well, but it shows how the pattern "ripples" over space, which is pretty neat.
* hard to make out fine details
*/