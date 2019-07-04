// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <vector>
#include "Runtime/SlateCore/Public/Types/SlateEnums.h"
#include "Modules/ModuleManager.h"


class FToolBarBuilder;
class FMenuBuilder;
class FReply;
class FUICommandList;
class SButton;
class UMaterial;
struct FSlateImageBrush;
struct FSlateDynamicImageBrush;
struct FSlateBrush;
class UMaterialInstanceDynamic;
class GIF_frameCapture;
class FDelegateHandle;
class UTexture2D;
class SImage;
class SDockTab;

class FGIF_recorderModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** This function will be bound to Command (by default it will bring up plugin window) */
	void PluginButtonClicked();
	void PluginRecordButtonClicked();

	/** Seb comment: Function for what to do when pressing the save button**/
	FReply SaveButtonClicked();
	FReply SelectPathButtonClicked();
	void OnViewportTabClosed(TSharedRef<SDockTab> ClosedTab);

	
private:

	/** Seb comment: Function for what to do when the value was changed**/
	void StartTimeChanged(int32 NewNumber);
	TOptional<int32> HandleStartTimeValue() const;
	void EndTimeChanged(int32 NewNumber);
	TOptional<int32> HandleEndTimeValue() const;
	TOptional<int32> HandleMaxValues() const;
	const FSlateBrush* GetMainScreenBrush() const;
	void OnPathTextCommitted(const FText& InText, ETextCommit::Type InCommitType);
	FText GetPathText() const;


	void AddToolbarExtension(FToolBarBuilder& Builder);
	void AddMenuExtension(FMenuBuilder& Builder);
	void AddRecorderToolbarExtension(FToolBarBuilder& Builder);
	/** Seb comment: Functions for setting the styles**/
	void CreateSaveButtonStyle();
	void CreateMainScreenStyle();
	bool Tick(float DeltaTime);

	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);


private:
	TSharedPtr<class FUICommandList> PluginCommands;
	TSharedPtr<class FUICommandList> RecordCommands;
	TSharedPtr<SButton> SaveButton;
	TSharedPtr<SImage> MainScreenImage;
	TSharedPtr<FSlateDynamicImageBrush> SaveButtonBrush;
	TSharedPtr<FSlateImageBrush> MainScreenBrush;
	FDelegateHandle TickDelegateHandle;

	TArray<FSlateImageBrush*> brushArray;

	int32 StartTime;
	int32 EndTime;
	int32 MaxValue;

	float FramesPerSecond;
	float FrameTimer;

	TSharedPtr<UTexture2D> test1;
	TSharedPtr<UTexture2D> test2;

	TSharedPtr<GIF_frameCapture> recorder;
	bool isRecording = false;
	bool WindowIsOpen = false;

	int currentFrame = 0;
	int FrameCounter = 0;
	FText SavePath;
};
