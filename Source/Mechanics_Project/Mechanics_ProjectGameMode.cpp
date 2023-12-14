// Copyright Epic Games, Inc. All Rights Reserved.

#include "Mechanics_ProjectGameMode.h"
#include "Mechanics_ProjectCharacter.h"
#include "UObject/ConstructorHelpers.h"

AMechanics_ProjectGameMode::AMechanics_ProjectGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
