#pragma once

#include <vector>

#include "gif_lib.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Containers/UnrealString.h"
#include "Containers/Ticker.h"
#include "Engine/SceneCapture2D.h"

typedef unsigned char GifByteType;

class GIF_frameCapture
{
public:
	GIF_frameCapture();
	~GIF_frameCapture();


	TArray<UTexture2D*> AllFrames;
	/* Lance Comment: Frame rate for gif capture */
	float FPS = 1.0f / 15.0f;
	
	/* Lance Comment: Save recorded frames to gif */
	void SaveGIF(std::string pathName, int32 startFrame, int32 endFrame);

	bool GetRecording() { return IsRecording; }
	bool StartRecording();
	void StopRecording();

private:
	bool Tick(float DeltaTime);
	void Update(float DeltaTime);
	void SaveFrame();
	void Reset();

	void SetRecording(bool recording) { IsRecording = recording;  }

	/* Mad comment: Split up all the colors of every frame into RGB vectors */
	std::vector<std::vector<GifByteType>> RedChannel;
	std::vector<std::vector<GifByteType>> GreenChannel;
	std::vector<std::vector<GifByteType>> BlueChannel;

	/* Mad comment: Tick handlers */
	FTickerDelegate TickDelegate;
	FDelegateHandle TickDelegateHandle;

	/* Mad comment: Variables associated with recording frame */
	ASceneCapture2D* SceneCapture = nullptr;
	UTextureRenderTarget2D* RenderTarget = nullptr;
	UTexture2D* Texture = nullptr;

	const int MaxFrames = 150;

	int CurrentRecordedFrame = 0;
	bool IsRecording = false;
	bool FollowingPlayer = false;

	/* Mad comment: GIF saving */
	GifFileType* GifFile = nullptr;
	/* Lance comment: Setup first data blocks for gif image, mainly just sets the width and height of the image. */
	void SetupGif(int ImageWidth, int ImageHeight);
	/* Lance comment: Appends a frame to our in memory gif structure (GifFile) */
	void AppendFrameToGif(std::vector<GifByteType>& RedChannel, 
	                      std::vector<GifByteType>& GreenChannel,
	                      std::vector<GifByteType>& BlueChannel);
};

