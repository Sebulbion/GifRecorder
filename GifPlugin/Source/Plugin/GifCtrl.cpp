// Fill out your copyright notice in the Description page of Project Settings.

#include "GifCtrl.h"

// Sets default values
AGifCtrl::AGifCtrl()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RootComponent"));
	RootComponent = mesh;
}

// Called when the game starts or when spawned
void AGifCtrl::BeginPlay()
{
	Super::BeginPlay();

	mesh->SetMaterial(0, material);
}

// Called every frame
void AGifCtrl::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

