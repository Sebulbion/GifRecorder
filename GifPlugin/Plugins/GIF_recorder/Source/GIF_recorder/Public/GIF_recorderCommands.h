// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "GIF_recorderStyle.h"

class FGIF_recorderCommands : public TCommands<FGIF_recorderCommands>
{
public:

	FGIF_recorderCommands()
		: TCommands<FGIF_recorderCommands>(TEXT("GIF_recorder"), FText::FromString("GIF Commands"), NAME_None, FGIF_recorderStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
	TSharedPtr< FUICommandInfo > ToggleRecording;
};