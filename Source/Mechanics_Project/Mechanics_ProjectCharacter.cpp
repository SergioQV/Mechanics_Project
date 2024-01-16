// Copyright Epic Games, Inc. All Rights Reserved.

#include "Mechanics_ProjectCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h"

//////////////////////////////////////////////////////////////////////////
// AMechanics_ProjectCharacter

AMechanics_ProjectCharacter::AMechanics_ProjectCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
	bIsFallingFromWallCollision = false;
}

void AMechanics_ProjectCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	GetCapsuleComponent()->OnComponentHit.AddDynamic(this, &AMechanics_ProjectCharacter::SetWallJumpIfApplies);
	

	//Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void AMechanics_ProjectCharacter::SetWallJumpIfApplies(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{

	CheckIfCollisionWithGround();

	if (!IsValidCollisionForWallJump(OtherActor, OtherComp, -Hit.ImpactNormal)) return;
	
	DrawDebugLine(GetWorld(), Hit.ImpactPoint, Hit.ImpactPoint + Hit.ImpactNormal * 300.f, FColor::Red, false, 1.f, 0.f, 1.f);	
	
	FRotator DirtyRotation = UKismetMathLibrary::MakeRotFromX(Hit.ImpactPoint - GetActorLocation());
	FRotator RotationToWall(0.f, DirtyRotation.Yaw, 0.f);

	SetActorRotation(RotationToWall);

	//if (Hit.ImpactPoint.Z >= GetActorLocation().Z) {
		GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Flying);
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->BrakingDecelerationFlying = 2000.f;


		bWaitingForJumpInWall = true;

		GetWorld()->GetTimerManager().SetTimer(JumpWallTimerHandle, this, &AMechanics_ProjectCharacter::StopWallWaiting, 0.4f, false);
	//}
}

#pragma region WallJumpUtils
float AMechanics_ProjectCharacter::AngleWithForwardVector(FVector Other)
{
	float DotProduct = FVector::DotProduct(GetActorForwardVector(), Other);

	float Angle = FMath::RadiansToDegrees(FMath::Acos(DotProduct));

	return Angle;
}

bool AMechanics_ProjectCharacter::IsValidCollisionForWallJump(AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector ImpactNormal)
{

	if (!GetMovementComponent()->IsFalling()) return false;
	if (bIsFallingFromWallCollision) return false;
	if (OtherComp->GetCollisionObjectType() != ECC_WorldStatic) return false;
	if (OtherActor->GetActorRotation().Pitch != 0) return false;
	if (OtherActor->GetActorRotation().Roll != 0) return false;

	//calculate the angle between forward vector and ImpactNormalVector
	float Angle = AngleWithForwardVector(ImpactNormal);

	if (Angle >= 45) return false;

	return true;
}
void AMechanics_ProjectCharacter::StopWallWaiting()
{
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Falling);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->BrakingDecelerationFlying = 0.f;
	bWaitingForJumpInWall = false;
	bIsFallingFromWallCollision = true;
}
void AMechanics_ProjectCharacter::CheckIfCollisionWithGround()
{
	if (GetCharacterMovement()->Velocity.Z == 0) {
		bIsFallingFromWallCollision = false;
	} 
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Input

void AMechanics_ProjectCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		//Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AMechanics_ProjectCharacter::CustomJump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		//Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMechanics_ProjectCharacter::Move);

		//Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMechanics_ProjectCharacter::Look);

	}

}

void AMechanics_ProjectCharacter::Move(const FInputActionValue& Value)
{

	if (bWaitingForJumpInWall) return;

	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AMechanics_ProjectCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}


void AMechanics_ProjectCharacter::CustomJump()
{
	if (!bWaitingForJumpInWall) {
		Super::Jump();
	}
	else {

		bWaitingForJumpInWall = false;

		GetWorld()->GetTimerManager().ClearTimer(JumpWallTimerHandle);

		FRotator JumpBackRotation = GetActorRotation() + FRotator(0.f, 180.f, 0.f);
		SetActorRotation(JumpBackRotation);



		FVector LaunchVelocity = 600*GetActorForwardVector() + FVector(0, 0.f, 500.f);

		UE_LOG(LogTemp, Warning, TEXT("Actor location: %s"), *LaunchVelocity.ToString());

		LaunchCharacter(LaunchVelocity, true, true);
	}
}

