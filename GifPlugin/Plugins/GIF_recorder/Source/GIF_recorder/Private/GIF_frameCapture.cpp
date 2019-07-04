#include "GIF_frameCapture.h"

#include <string>

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "Components/SceneCaptureComponent2D.h"

#include "Engine/World.h"
#include "Styling/SlateStyleRegistry.h"

#include "Editor/UnrealEd/Classes/Editor/EditorEngine.h"

/* Mad comment: Get the play-in-editor world, returns null when not playing the game */
static inline UWorld* GetPrimaryWorld()
{
	UWorld* ReturnVal = NULL;

	if (GEngine != NULL)
	{
		for (auto It = GEngine->GetWorldContexts().CreateConstIterator(); It; ++It)
		{
			const FWorldContext& Context = *It;

			if ((Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE) && Context.World())
			{
				ReturnVal = Context.World();
				break;
			}
		}
	}

	return ReturnVal;
}

/* Mad comment: Initialize rendertarget and tick function */
GIF_frameCapture::GIF_frameCapture()
{
	RenderTarget = Cast<UTextureRenderTarget2D>(StaticLoadObject(UTextureRenderTarget2D::StaticClass(), nullptr, TEXT("TextureRenderTarget2D'/GIF_recorder/RT_Target.RT_Target'")));
	RenderTarget->AddToRoot();
	TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &GIF_frameCapture::Tick), FPS);
}

/* Mad comment: Clean up memory */
GIF_frameCapture::~GIF_frameCapture()
{
	FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);

	if(RenderTarget != nullptr)
		if(RenderTarget->IsRooted())
			RenderTarget->RemoveFromRoot();

	for (UTexture2D* tex : AllFrames)
	{
		if(tex!=nullptr)
			if(tex->IsRooted())
				tex->RemoveFromRoot();
	}
}

bool GIF_frameCapture::Tick(float DeltaTime)
{
	GIF_frameCapture::Update(DeltaTime);
	return true;
}

void GIF_frameCapture::Update(float DeltaTime)
{
	if (IsRecording)
	{
		SaveFrame();
	}
}

/* Mad comment: Save currently viewed image into a vector */
void GIF_frameCapture::SaveFrame()
{
	/* Mad comment: If the scene capture is valid and we have a PIE world */
	if (SceneCapture && GetPrimaryWorld())
	{
		if (FollowingPlayer)
		{
			/* Mad comment: Get the player's viewpoint and record from there */
			APlayerController* playerOne = UGameplayStatics::GetPlayerController(GetPrimaryWorld(), 0);
			if (playerOne != nullptr)
			{
				FVector Location;
				FRotator Rotation;
				playerOne->GetPlayerViewPoint(Location, Rotation);
				SceneCapture->GetCaptureComponent2D()->SetWorldLocation(Location);
				SceneCapture->GetCaptureComponent2D()->SetWorldRotation(Rotation);
			}
		}

		/* Mad comment: Produce render target that the scene capture 2d uses */
		RenderTarget = SceneCapture->GetCaptureComponent2D()->TextureTarget;
		FTextureRenderTargetResource* RTResource = RenderTarget->GameThread_GetRenderTargetResource();

		FReadSurfaceDataFlags ReadPixelFlags(RCM_UNorm);
		ReadPixelFlags.SetLinearToGamma(true);

		/* Mad comment: Read pixels from the render texture into a color array */
		TArray<FColor> OutBMP;
		RTResource->ReadPixels(OutBMP, ReadPixelFlags);

		RedChannel.push_back(std::vector<GifByteType>());
		GreenChannel.push_back(std::vector<GifByteType>());
		BlueChannel.push_back(std::vector<GifByteType>());

		/* Mad comment: Store individual colors of the frame for later processing */
		for (FColor& color : OutBMP)
		{
			RedChannel[RedChannel.size()-1].push_back(color.R);
			GreenChannel[GreenChannel.size() - 1].push_back(color.G);
			BlueChannel[BlueChannel.size() - 1].push_back(color.B);

			color.A = 255;
		}

		/* Mad comment: Creates Texture2D to store TextureRenderTarget content */
		Texture = UTexture2D::CreateTransient(RenderTarget->GetSurfaceWidth(), RenderTarget->GetSurfaceHeight(), PF_B8G8R8A8);
		Texture->AddToRoot();
#if WITH_EDITORONLY_DATA
		Texture->MipGenSettings = TMGS_NoMipmaps;
#endif
		Texture->SRGB = RenderTarget->SRGB;


		/* Mad comment: Lock and copies the data between the textures */
		void* TextureData = Texture->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
		const int32 TextureDataSize = OutBMP.Num() * 4;
		FMemory::Memcpy(TextureData, OutBMP.GetData(), TextureDataSize);
		Texture->PlatformData->Mips[0].BulkData.Unlock();

		/* Mad comment: Apply Texture changes to GPU memory */
		Texture->UpdateResource();

		/* Mad comment: Add texture to frame vector to present on the slate UI */
		AllFrames.Add(Texture); 

		/* Mad comment: Adjust vector if exceeds max frame count */
		if (AllFrames.Num() > MaxFrames)
		{
			AllFrames.RemoveAt(0);

			RedChannel.erase(RedChannel.begin());
			GreenChannel.erase(GreenChannel.begin());
			BlueChannel.erase(BlueChannel.begin());
		}

	}
	else if (IsRecording)
	{
		const ISlateStyle* StyleSet = FSlateStyleRegistry::FindSlateStyle(TEXT("GIF_recorderStyle"));
		FSlateBrush* brush = const_cast<FSlateBrush*>(StyleSet->GetBrush(TEXT("GIF_recorder.ToggleRecording")));
		brush->TintColor = FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
		Reset();
	}
}

/* Mad comment: Reset recording data when recording again */
void GIF_frameCapture::Reset()
{
	FollowingPlayer = false;
	for (UTexture2D* tex : AllFrames)
	{
		tex->ClearFlags(RF_NoFlags);
	}

	AllFrames.Empty();

	RedChannel.clear();
	GreenChannel.clear();
	BlueChannel.clear();
	SceneCapture = nullptr;
	IsRecording = false;
}


bool GIF_frameCapture::StartRecording()
{
	Reset();
	UWorld* currentWorld = GetPrimaryWorld();
	if (currentWorld != nullptr)
	{
		/* Mad comment: If a scene capture already exists */
		TArray<AActor*> allSceneCaptures;
		UGameplayStatics::GetAllActorsOfClass(currentWorld, ASceneCapture2D::StaticClass(), allSceneCaptures);

		if (allSceneCaptures.Num() > 0)
		{
			SceneCapture = Cast<ASceneCapture2D>(allSceneCaptures[0]);
			IsRecording = true;

			if (SceneCapture->GetCaptureComponent2D()->TextureTarget == nullptr) {
				SceneCapture->GetCaptureComponent2D()->TextureTarget = RenderTarget;
			} else {
				RenderTarget = SceneCapture->GetCaptureComponent2D()->TextureTarget;
			}

			return true;
		}
		else
		{
			SceneCapture = currentWorld->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass());
			/* Mad comment: Set capture component initial data */
			USceneCaptureComponent2D *sceneCaptureComponent = SceneCapture->GetCaptureComponent2D();
			sceneCaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
			sceneCaptureComponent->RegisterComponent();

			/* Mad comment: Try to get the first player's camera. If it fails just get the first available camera */
			APlayerController* playerOne = UGameplayStatics::GetPlayerController(currentWorld, 0);
			if (playerOne != nullptr)
			{
				FVector Location;
				FRotator Rotation;

				playerOne->GetPlayerViewPoint(Location, Rotation);
				SceneCapture->GetCaptureComponent2D()->SetWorldLocation(Location);
				SceneCapture->GetCaptureComponent2D()->SetWorldRotation(Rotation);

				/* Mad comment: Use default render texture we provide in the plugin */
				sceneCaptureComponent->TextureTarget = RenderTarget;
				
				IsRecording = true;
				FollowingPlayer = true;
				return true;
			}
		}
	}
	return false;
}

void GIF_frameCapture::SaveGIF(std::string pathName, int32 startFrame, int32 endFrame)
{
	/* Open output file */
	int ErrorCode;
	std::string filePath = pathName + "/GIF.gif";
	if ((GifFile = EGifOpenFileName(filePath.c_str(), false, &ErrorCode)) == NULL) {
		// TODO: Add debug logging
		return;
	}

	SetupGif(RenderTarget->GetSurfaceWidth(), RenderTarget->GetSurfaceHeight());

	for (int i = startFrame; i <= endFrame; i++)
	{
		AppendFrameToGif(RedChannel[i], GreenChannel[i], BlueChannel[i]);
	}

	//	 TODO: Add debugging output
	if (EGifSpew(GifFile) == GIF_ERROR) {
		// TODO: Add debug logging
		return;
	}
}

void GIF_frameCapture::StopRecording()
{
	SceneCapture->Destroy();
	IsRecording = false;
}

void GIF_frameCapture::SetupGif(int ImageWidth, int ImageHeight)
{
	GifFile->SWidth = ImageWidth;
	GifFile->SHeight = ImageHeight;
	GifFile->SColorMap = NULL;
	GifFile->SColorResolution = GifBitSize(256);
}

void GIF_frameCapture::AppendFrameToGif(std::vector<GifByteType>& RedChannel,
	std::vector<GifByteType>& GreenChannel,
	std::vector<GifByteType>& BlueChannel)
{
	int ImageWidth = RenderTarget->GetSurfaceWidth();
	int ImageHeight = RenderTarget->GetSurfaceHeight();

	// Create local color map and raster bits
	int ColorCount = 256;
	GifByteType* RasterBits;
	if ((RasterBits = (GifByteType*)malloc(sizeof(GifByteType) * ImageWidth * ImageHeight)) == NULL) {
		// TODO: Add debug logging
		return;
	}
	GifColorType* Colors = (GifColorType*)malloc(sizeof(GifColorType) * ColorCount);
	if ((Colors = (GifColorType*)malloc(sizeof(GifColorType) * ColorCount)) == NULL) {
		// TODO: Add debug logging
		return;
	}
	if (GifQuantizeBuffer(
		ImageWidth,
		ImageHeight,
		&ColorCount,
		RedChannel.data(),
		GreenChannel.data(),
		BlueChannel.data(),
		RasterBits,
		Colors) != GIF_OK)
	{
		// TODO: Add debug logging here
		return;
	}
	ColorMapObject* ColorMap;
	if ((ColorMap = GifMakeMapObject(ColorCount, Colors)) == NULL) {
		// TODO: Add debug logging here
		return;
	}
	free(Colors);

	// Create image descriptor
	GifFile->Image.Left = 0;
	GifFile->Image.Top = 0;
	GifFile->Image.Width = ImageWidth;
	GifFile->Image.Height = ImageHeight;
	GifFile->Image.Interlace = false;
	GifFile->Image.ColorMap = ColorMap;

	// Save Gif frame
	GifMakeSavedImage(GifFile, NULL);
	SavedImage* sp = &GifFile->SavedImages[GifFile->ImageCount - 1];
	memcpy(&sp->ImageDesc, &GifFile->Image, sizeof(GifImageDesc));
	sp->ImageDesc.ColorMap = ColorMap;
	sp->RasterBits = RasterBits;
	sp->ExtensionBlockCount = 0;
	sp->ExtensionBlocks = (ExtensionBlock *)NULL;

	if (GifFile->ImageCount == 1) {
		// Add Netscape 2.0 loop block
		if (GifAddExtensionBlock(
			&sp->ExtensionBlockCount,
			&sp->ExtensionBlocks,
			APPLICATION_EXT_FUNC_CODE,
			11,
			(unsigned char *)"NETSCAPE2.0") == GIF_ERROR)
		{
			// TODO: Add debug logging here
			return;
		}
		unsigned char params[3] = { 1, 0, 0 }; // first byte always 1, remaining bytes are 16 bit unsigned int loop count
		if (GifAddExtensionBlock(
			&sp->ExtensionBlockCount,
			&sp->ExtensionBlocks,
			CONTINUE_EXT_FUNC_CODE,
			sizeof(params),
			params) == GIF_ERROR)
		{
			// TODO: Add debug logging here
			return;
		}
	}

	// Add GCB Extension block
	GraphicsControlBlock gcb;
	gcb.DelayTime = static_cast<int>(100.0f * FPS);
	gcb.DisposalMode = DISPOSE_DO_NOT;
	gcb.TransparentColor = NO_TRANSPARENT_COLOR;
	gcb.UserInputFlag = false;
	GifByteType GifExtension[4];
	size_t Len = EGifGCBToExtension(&gcb, GifExtension);
	if (GifAddExtensionBlock(
		&sp->ExtensionBlockCount,
		&sp->ExtensionBlocks,
		GRAPHICS_EXT_FUNC_CODE,
		Len,
		(unsigned char *)GifExtension) == GIF_ERROR)
	{
		// TODO: Add debug logging here
		return;
	}
}
