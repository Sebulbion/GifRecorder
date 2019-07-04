// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "GIF_recorder.h"
#include "GIF_recorderStyle.h"
#include "GIF_recorderCommands.h"
#include "GIF_frameCapture.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"
#include "Runtime/Slate/Public/Widgets/Images/SImage.h"
#include "Runtime/CoreUObject/Public/UObject/ConstructorHelpers.h"
#include "Widgets/Input/SButton.h"
#include "Runtime/SlateCore/Public/Brushes/SlateDynamicImageBrush.h"
#include "Runtime/SlateCore/Public/Styling/SlateBrush.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Runtime/Core/Public/Delegates/IDelegateInstance.h"
#include "Containers/Ticker.h"
#include "Runtime/Engine/Classes/Engine/Texture2D.h"
#include "Runtime/ImageWrapper/Public/IImageWrapper.h"
#include "Runtime/ImageWrapper/Public/IImageWrapperModule.h"
#include "Runtime/Core/Public/Misc/FileHelper.h"
#include "Runtime/Slate/Public/Widgets/Input/SEditableTextBox.h"
#include "Developer/DesktopPlatform/Public/IDesktopPlatform.h"
#include "Runtime/Engine/Classes/Engine/Engine.h"
#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#include "Editor/UnrealEd/Classes/Editor/EditorEngine.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"


static const FName GIF_recorderTabName("GIF_recorder");

#define LOCTEXT_NAMESPACE "FGIF_recorderModule"

void FGIF_recorderModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	recorder = MakeShareable(new GIF_frameCapture);
	FramesPerSecond = 3.0f;
	FrameTimer = 0.0f;
	SavePath = FText::FromName(FName(*(IPluginManager::Get().FindPlugin("GIF_recorder")->GetBaseDir() / TEXT("Content") / "")));

	// Seb comment: sets up the image for the save button
	CreateSaveButtonStyle();
	// Seb comment: sets up the image for the main screen
	CreateMainScreenStyle();

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

	// Seb comment: sets up the image size and makes it a child to the plugin
	FGIF_recorderStyle::Initialize();
	FGIF_recorderStyle::ReloadTextures();
	FGIF_recorderCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);
	// Seb comment: this registers what to do when you click the button
	PluginCommands->MapAction(
		FGIF_recorderCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FGIF_recorderModule::PluginButtonClicked),
		FCanExecuteAction());
		
	
	// Seb comment: adds the plugin to the bottom of the "Window" tab
	{
		TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
		MenuExtender->AddMenuExtension("WindowLayout", EExtensionHook::After, PluginCommands, FMenuExtensionDelegate::CreateRaw(this, &FGIF_recorderModule::AddMenuExtension));

		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
	}
	
	// Seb comment: adds the plugin button to the editor
	{
		TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
		ToolbarExtender->AddToolBarExtension("Settings", EExtensionHook::After, PluginCommands, FToolBarExtensionDelegate::CreateRaw(this, &FGIF_recorderModule::AddToolbarExtension));
		
		LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
	}

	// Seb comment: links a bunch of things together. Most notably the spawn function. Somehow this makes it so that OnSpawnPluginTab is called when the above button is pressed
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(GIF_recorderTabName, FOnSpawnTab::CreateRaw(this, &FGIF_recorderModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FGIF_recorderTabTitle", "Edit window"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	RecordCommands = MakeShareable(new FUICommandList);

	RecordCommands->MapAction(
		FGIF_recorderCommands::Get().ToggleRecording,
		FExecuteAction::CreateRaw(this, &FGIF_recorderModule::PluginRecordButtonClicked),
		FCanExecuteAction()
	);

	// Seb comment: adds the record button to the editor
	{
		TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
		ToolbarExtender->AddToolBarExtension("Settings", EExtensionHook::After, RecordCommands, FToolBarExtensionDelegate::CreateRaw(this, &FGIF_recorderModule::AddRecorderToolbarExtension));

		LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
	}

	TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FGIF_recorderModule::Tick), recorder->FPS);
}

void FGIF_recorderModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FGIF_recorderStyle::Shutdown();

	FGIF_recorderCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GIF_recorderTabName);
}

TSharedRef<SDockTab> FGIF_recorderModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	WindowIsOpen = true;
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.OnTabClosed_Raw(this, &FGIF_recorderModule::OnViewportTabClosed)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.Padding(0.0f, 0.0f, 0.0f, 0.0f)
			.FillWidth(1.0f)
			[
				SNew(SVerticalBox)

				// Seb comment: The top bar containing the save button
				+ SVerticalBox::Slot()
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Left)
				.FillHeight(0.1f)
				.Padding(4.0f, 4.0f, 0.0f, 0.0f)
				[
					// Seb comment: This is a hack because I have no idea how to size the button to the image
					SNew(SBox)
					[
						SAssignNew(SaveButton, SButton)
						.ButtonStyle(FCoreStyle::Get(), "NoBorder")
						.OnClicked_Raw(this, &FGIF_recorderModule::SaveButtonClicked)
						.VAlign(VAlign_Fill)
						.HAlign(HAlign_Fill)
						.OnHovered_Lambda([this] { SaveButtonBrush->TintColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f); })
						.OnUnhovered_Lambda([this] { SaveButtonBrush->TintColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f); })
						[
							SNew(SImage)
							.Image(SaveButtonBrush.Get())
							
						]
					]
				]
				
				// Seb comment: Big UI where the gif is displayed
				+ SVerticalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.FillHeight(1.0f)
				[
					SNew(SBorder)
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					//.BorderImage(SaveButtonBrush.Get())
					[
						SAssignNew(MainScreenImage, SImage)
						.Image_Raw(this, &FGIF_recorderModule::GetMainScreenBrush)
					]
				]
			]

			// Seb comment: The UI containing the plugin options
			+ SHorizontalBox::Slot()
			.Padding(0.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Center)
				.FillWidth(0.25f)
			[
			//===========================Start Time============================
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.Padding(0.0f, 80.0f, 0.0f, 8.0f)
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Center)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.Padding(0.0f, 0.0f, 2.0f, 0.0f)
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SBox)
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("WindowWidgetText", "testKey3", "Start Frame: "))
						]
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.FillWidth(1.0f)
					[
						SNew(SBox)
						.WidthOverride(100)
						[
							SNew(SNumericEntryBox<int32>)
							.AllowSpin(true)
							.MinSliderValue(0)
							.MinValue(0)
							.MaxSliderValue_Raw(this, &FGIF_recorderModule::HandleMaxValues)
							.MaxValue_Raw(this, &FGIF_recorderModule::HandleMaxValues)
							.Value_Raw(this, &FGIF_recorderModule::HandleStartTimeValue)
							.OnValueChanged_Raw(this, &FGIF_recorderModule::StartTimeChanged)
							.IsEnabled(true)
						]
					]
				]
			//==============================End Time==========================
			+ SVerticalBox::Slot()
				.Padding(0.0f, 8.0f, 0.0f, 8.0f)
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Center)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
				.Padding(0.0f, 0.0f, 2.0f, 0.0f)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("WindowWidgetText", "testKey4", "End Frame: "))
					]
				]

				+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.FillWidth(1.0f)
					[
						SNew(SBox)
						.WidthOverride(100)
						[
							SNew(SNumericEntryBox<int32>)
							.AllowSpin(true)
							.MinSliderValue(0)
							.MinValue(0)
							.MaxSliderValue_Raw(this, &FGIF_recorderModule::HandleMaxValues)
							.MaxValue_Raw(this, &FGIF_recorderModule::HandleMaxValues)
							.Value_Raw(this, &FGIF_recorderModule::HandleEndTimeValue)
							.OnValueChanged_Raw(this, &FGIF_recorderModule::EndTimeChanged)
							.IsEnabled(true)
						]
					]
				]

			//==============================Save Path==========================
			+ SVerticalBox::Slot()
				.Padding(0.0f, 8.0f, 0.0f, 8.0f)
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Center)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
						.Padding(0.0f, 0.0f, 2.0f, 0.0f)
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SBox)
							[
								SNew(STextBlock)
								.Text(NSLOCTEXT("WindowWidgetText", "testKey5", "Save Path: "))
							]
						]

					+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.FillWidth(1.0f)
						[
							SNew(SBox)
							.WidthOverride(100)
							[
								SNew(SEditableTextBox)
								.Text_Raw(this, &FGIF_recorderModule::GetPathText)
								.OnTextCommitted_Raw(this, &FGIF_recorderModule::OnPathTextCommitted)
							]
						]
				]

			//==============================Path Button==========================
			+ SVerticalBox::Slot()
				.Padding(0.0f, 8.0f, 0.0f, 8.0f)
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Center)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
						.Padding(0.0f, 0.0f, 2.0f, 0.0f)
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SBox)
							[
								SNew(SButton)
								.OnClicked_Raw(this, &FGIF_recorderModule::SelectPathButtonClicked)
								.VAlign(VAlign_Fill)
								.HAlign(HAlign_Fill)
								[
									SNew(STextBlock)
									.Text(NSLOCTEXT("WindowWidgetText", "testKey6", "Select Path"))
								]
							]
						]
				]
			]
		];
}

void FGIF_recorderModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->InvokeTab(GIF_recorderTabName);
}

void FGIF_recorderModule::PluginRecordButtonClicked()
{
	/* Mad comment: If not recording, start recording.*/
	if (!recorder->GetRecording())
	{
		if (recorder->StartRecording())
		{
			/* Mad Comment: Reset slate UI styles and such. Reset variables */
			const ISlateStyle* StyleSet = FSlateStyleRegistry::FindSlateStyle(TEXT("GIF_recorderStyle"));
			FSlateBrush* brush = const_cast<FSlateBrush*>(StyleSet->GetBrush(TEXT("GIF_recorder.ToggleRecording")));
			brush->TintColor = FSlateColor(FLinearColor(0.0f, 1.0f, 0.0f, 1.0f));

			StartTime = 0;
			EndTime = 0;
			MaxValue = 0;
		}
	}
	else
	{
		/* Mad Comment: If recording, STOP recording */
		const ISlateStyle* StyleSet = FSlateStyleRegistry::FindSlateStyle(TEXT("GIF_recorderStyle"));
		FSlateBrush* brush = const_cast<FSlateBrush*>(StyleSet->GetBrush(TEXT("GIF_recorder.ToggleRecording")));
		brush->TintColor = FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
		recorder->StopRecording();

		currentFrame = 0;
		FrameCounter = 0;

		/* Mad Comment: Populate brush vector with all the frames */
		brushArray.Empty();
		EndTime = recorder->AllFrames.Num() - 1;
		MaxValue = recorder->AllFrames.Num() - 1;
		for (int i = 0; i < recorder->AllFrames.Num(); i++)
		{

			brushArray.Add(new FSlateImageBrush(
				recorder->AllFrames[i], 
				FVector2D(1000.0f, 1000.0f),
				FLinearColor(1.0f, 1.0f, 1.0f)));
		}
	}
}

FReply FGIF_recorderModule::SaveButtonClicked()
{
	/* Mad Comment: Call save gif using variables stored in this class */
	recorder->SaveGIF(std::string(TCHAR_TO_UTF8(*SavePath.ToString())), StartTime, EndTime);

	return FReply::Handled();
}

FReply FGIF_recorderModule::SelectPathButtonClicked()
{
	/* Seb Comment: Retrieve save path from default windows file navigator */
	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(FGlobalTabmanager::Get()->GetActiveTab()->AsShared());
	const void* ParentWindowHandle = (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid())
		? ParentWindow->GetNativeWindow()->GetOSWindowHandle()
		: nullptr;
	FString OutFilename;

	if (FDesktopPlatformModule::Get()->OpenDirectoryDialog(
		ParentWindowHandle,
		FString("Save Path"),
		SavePath.ToString(),
		OutFilename
	))
	{
		SavePath = FText::FromString(OutFilename);
	}

	return FReply::Handled();
}

void FGIF_recorderModule::OnViewportTabClosed(TSharedRef<SDockTab> ClosedTab)
{
	WindowIsOpen = false;
}

void FGIF_recorderModule::StartTimeChanged(int32 NewNumber)
{
	/* Seb Comment: Adjust start time */
	StartTime = NewNumber;
	if (MaxValue > 0)
		if (StartTime > EndTime)
			EndTime = StartTime;
	currentFrame = StartTime;
	FrameCounter = StartTime;
}

TOptional<int32> FGIF_recorderModule::HandleStartTimeValue() const
{
	return StartTime;
}

void FGIF_recorderModule::EndTimeChanged(int32 NewNumber)
{
	/* Seb Comment: Adjust end time */
	EndTime = NewNumber;
	if (MaxValue > 0)
		if (EndTime < StartTime)
			StartTime = EndTime;
	currentFrame = StartTime;
	FrameCounter = StartTime;
}

TOptional<int32> FGIF_recorderModule::HandleEndTimeValue() const
{
	return EndTime;
}

TOptional<int32> FGIF_recorderModule::HandleMaxValues() const
{
	return MaxValue;
}

void FGIF_recorderModule::AddMenuExtension(FMenuBuilder& Builder)
{
	Builder.AddMenuEntry(FGIF_recorderCommands::Get().OpenPluginWindow);
}

void FGIF_recorderModule::CreateSaveButtonStyle()
{
	SaveButtonBrush = MakeShareable(new FSlateDynamicImageBrush
	(
		FName(*(IPluginManager::Get().FindPlugin("GIF_recorder")->GetBaseDir() / TEXT("Resources") / TEXT("SaveIcon_40x.png"))),
		FVector2D(40.0f,40.0f),
		FLinearColor(1.0f,1.0f,1.0f),
		ESlateBrushTileType::NoTile,
		ESlateBrushImageType::FullColor
	));
}

void FGIF_recorderModule::CreateMainScreenStyle()
{
	MainScreenBrush = MakeShareable(new FSlateImageBrush(
		"",
		FVector2D(1000.0f, 1000.0f),
		FLinearColor(1.0f, 1.0f, 1.0f),
		ESlateBrushTileType::NoTile,
		ESlateBrushImageType::FullColor
	));
}

bool FGIF_recorderModule::Tick(float DeltaTime)
{
	/* Seb Comment: Tick through brush array between start and end, and loop continuously */
	if (!recorder->GetRecording() && brushArray.Num() > 0 && MainScreenImage.Get() != nullptr && WindowIsOpen == true)
	{
		MainScreenBrush->SetResourceObject(brushArray[currentFrame]->GetResourceObject());
		currentFrame += 1;
		FrameCounter += 1;
		currentFrame = StartTime + (FrameCounter % (EndTime + 1 - StartTime));
	}
	return true;
}

const FSlateBrush * FGIF_recorderModule::GetMainScreenBrush() const
{
	return MainScreenBrush.Get();
}

void FGIF_recorderModule::OnPathTextCommitted(const FText & InText, ETextCommit::Type InCommitType)
{
	if (InCommitType == ETextCommit::OnEnter || InCommitType == ETextCommit::OnUserMovedFocus)
	{
		SavePath = InText;
	}
}

FText FGIF_recorderModule::GetPathText() const
{
	return SavePath;
}

void FGIF_recorderModule::AddToolbarExtension(FToolBarBuilder& Builder)
{
	Builder.AddToolBarButton(FGIF_recorderCommands::Get().OpenPluginWindow);
}

void FGIF_recorderModule::AddRecorderToolbarExtension(FToolBarBuilder & Builder)
{
	Builder.AddToolBarButton(FGIF_recorderCommands::Get().ToggleRecording);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FGIF_recorderModule, GIF_recorder)