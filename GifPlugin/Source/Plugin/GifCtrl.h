// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "GifCtrl.generated.h"


UCLASS()
class PLUGIN_API AGifCtrl : public AActor
{
	GENERATED_BODY()
	
		/* Functions */
public:	

	AGifCtrl();

	virtual void Tick(float DeltaTime) override;

protected:

	virtual void BeginPlay() override;

private:

	/* Variables */

public:	
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "StaticMesh")
		UStaticMeshComponent* mesh;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Material")
		UMaterialInstanceDynamic* material;

private:
	int currentFrame;


};
