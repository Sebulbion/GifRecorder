// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "GIF_recorderCommands.h"

#define LOCTEXT_NAMESPACE "FGIF_recorderModule"

void FGIF_recorderCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "GIF Window", "Bring up GIF recorder window", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(ToggleRecording, "Record", "Start recording", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
