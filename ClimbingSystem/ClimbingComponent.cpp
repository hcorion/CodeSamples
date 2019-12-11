/* Copyright 2019 Alexandrea Shackelford, Zion Nimchuk, All Rights Reserved.
 * Unauthorized copying or modification of this file, via any medium is strictly prohibited.
 */

#include "ClimbingComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "World/Climbable.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/Engine.h"
#include "Mokosh.h"
#include "Curves/CurveFloat.h"
#include "World/SplineLedge.h"
#include "Kismet/GameplayStatics.h"
#include "MasterIncludes.h"
#include "Utility/ClimbingFunctionLibrary.h"

// Sets default values for this component's properties
UClimbingComponent::UClimbingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}


// Called when the game starts
void UClimbingComponent::BeginPlay()
{
	Super::BeginPlay();
	PlayerRef = Cast<AMokosh>(GetOwner());
}

void UClimbingComponent::Init(UCharacterMovementComponent* ParentCharacterMovement, UCapsuleComponent* ParentCapsule)
{
	CharacterMovement = ParentCharacterMovement;
	CharacterCapsule = ParentCapsule;
}

void UClimbingComponent::ForceInitClimb()
{
	auto World = GetWorld();

	if (World == nullptr)
	{
		return;
	}

	// If we're mantling we don't want to receive any input
	if (bIsCurrentlyMantling)
	{
		return;
	}
	
	if (IsMovingToNewClimbable())
	{
		return;
	}

	if (bHasPossibleTargets)
	{
		FVector2D Direction = {
			GetOwner()->GetInputAxisValue("Vertical"),
			GetOwner()->GetInputAxisValue("Horizontal")
		};

		if (Direction.Size() < 0.5f)
		{
			// If we don't have a clear direction, we probably just want to go upwards
			Direction = FVector2D(0, 1);
		}

		auto ClimbingDetectionType = CDT_InAir;

		if (CharacterMovement->IsMovingOnGround())
		{
			ClimbingDetectionType = CDT_Walking;
		}
		else if (IsClimbing())
		{
			ClimbingDetectionType = CDT_IsClimbing;
		}

		auto Climbable = FindBestClimbable(Direction, ClimbingDetectionType);
		AttemptClimb(Climbable);

		if (bIsDebugging)
		{
			auto HangingTransform = GetHangingPosition(Climbable);
			UKismetSystemLibrary::DrawDebugCapsule(World,
			                                       HangingTransform.GetLocation(),
			                                       CharacterCapsule->GetScaledCapsuleHalfHeight(),
			                                       CharacterCapsule->GetScaledCapsuleRadius(),
			                                       HangingTransform.GetRotation().Rotator(),
			                                       FLinearColor::Green,
			                                       100.0f);
		}
	}
}

EClimbingDirectionEnum UClimbingComponent::GetDirection(AActor* Origin, AClimbable* Target) const
{
	float Result = FVector::DotProduct((Target->GetActorLocation() - Origin->GetActorLocation()).GetSafeNormal(), Origin->GetActorRightVector() * -1.0f);
	if (Result > ClimbingAnimDotProductDifference)
	{
		return EClimbingDirectionEnum::CD_Left;
	}
	else if (Result < -ClimbingAnimDotProductDifference)
	{
		return EClimbingDirectionEnum::CD_Right;
	}

	return EClimbingDirectionEnum::CD_Up;
}

bool UClimbingComponent::AttemptClimb(AClimbable* NewClimbable)
{
	auto World = GetWorld();
	if (NewClimbable == nullptr || World == nullptr)
	{
		return false;
	}

	PlayerRef->CancelSapAim();
	PlayerRef->IsClimbJumping = true;
	PlayerRef->ClimbingDirection = GetDirection(
		/*Origin: */CurrentClimbable == nullptr ? GetOwner() : CurrentClimbable, 
		NewClimbable);

	BeforeMovingPosition = GetOwner()->GetActorLocation();
	BeforeMovingRotation = GetOwner()->GetActorRotation();
	NextClimbable = NewClimbable;
	
	OnGrabbedNewClimbable.Broadcast(NextClimbable);

	CharacterMovement->SetMovementMode(EMovementMode::MOVE_Custom,
	                                   static_cast<uint8>(ECustomMovementModesEnum::MME_Climbing));
	MovingTime = 0.0f;
	
	return true;
}

void UClimbingComponent::DetachFromClimbing()
{
	CharacterMovement->SetMovementMode(EMovementMode::MOVE_Falling);
	PlayerRef->IsDetachClimbing = true;

	CharacterMovement->Velocity = FVector::ZeroVector;
	CurrentClimbable = nullptr;
	NextClimbable = nullptr;
	auto NewRotation = GetOwner()->GetActorRotation();
	NewRotation.Roll = 0.0f;
	NewRotation.Pitch = 0.0f;
	GetOwner()->SetActorRotation(NewRotation);
}

AClimbable* UClimbingComponent::FindBestClimbable(FVector2D InputDirection, EClimableDetectionTypeEnum DetectionType)
{
	auto World = GetWorld();
	if (World == nullptr || bHasPossibleTargets == false || PossibleClimbables.Num() == 0)
	{
		return nullptr;
	}

	FVector Direction = {0, InputDirection.X, InputDirection.Y};
	
	auto WorldDirection = UKismetMathLibrary::TransformDirection(GetOwner()->GetActorTransform(), Direction);
	Direction.Normalize();
	FTransform DirectionTransform = {WorldDirection.Rotation(), GetOwner()->GetActorLocation()};

	if (bIsDebugging)
	{
		UKismetSystemLibrary::DrawDebugArrow(World, GetOwner()->GetActorLocation(),
		                                     GetOwner()->GetActorLocation() + WorldDirection * 100.0f, 60.0f,
		                                     FLinearColor::Blue, 20.0f, 5.0f);
	}

	AClimbable* PossibleClimbable = nullptr;
	bool bPossibleClimbableIsLedge = false;
	TArray<float> ClimbableRatings;

	for (auto Climbable : PossibleClimbables)
	{
		int RatingIndex = ClimbableRatings.Add(0.0f);

		FVector LocalPosition = UKismetMathLibrary::InverseTransformLocation(
			GetOwner()->GetActorTransform(), Climbable->GetActorLocation());

		FVector CurrentLocalPosition;
		if (CurrentClimbable != nullptr)
		{
			CurrentLocalPosition = UKismetMathLibrary::InverseTransformLocation(
				GetOwner()->GetActorTransform(), CurrentClimbable->GetActorLocation());
		}

		FVector PreviousLocalPosition;
		if (PossibleClimbable != nullptr)
		{
			PreviousLocalPosition = UKismetMathLibrary::InverseTransformLocation(
				GetOwner()->GetActorTransform(), PossibleClimbable->GetActorLocation());
		}


		if (Climbable->bIsClimbable == false)
		{
			continue;
		}

		// We don't want to check if the player is obstructed when climbing a ledge, we only want to check when on crystals
		if (IsPlayerCapsuleInsideCollision(Climbable) && Cast<ASplineLedge>(Climbable) == nullptr)
		{
			continue;
		}

		if (DetectionType == CDT_ForwardInAir)
		{
			// For now we just shouldn't pick this up
			auto Ledge = Cast<ASplineLedge>(Climbable);
			if (Ledge != nullptr)
			{
				continue;
			}
		}
		
		// We can only grab onto ledges if we're standing on the ground and we don't want to do a ledge check when we're flying forward in air
		if ((IsAttached() || CharacterMovement->IsMovingOnGround()))
		{
			auto Ledge = Cast<ASplineLedge>(Climbable);
			if (Ledge != nullptr)
			{
				auto LedgeTransform = Ledge->GetClimbUpTransform(GetOwner());
				FVector LocalLedgePosition = UKismetMathLibrary::InverseTransformLocation(
					GetOwner()->GetActorTransform(), LedgeTransform.GetLocation());
				if (LedgeTransform.GetLocation().Z < GetOwner()->GetActorLocation().Z)
				{
					continue;
				}
				
				PossibleClimbable = Climbable;
				bPossibleClimbableIsLedge = true;
				continue;
			}
		}

		// We don't want to check for saps directly below us if we're not climbing
		if (DetectionType != CDT_IsClimbing)
		{
			if (Climbable->GetActorLocation().Z < (GetOwner()->GetActorLocation().Z) - CharacterCapsule->GetScaledCapsuleHalfHeight())
			{
				continue;
			}
		}

		// If we already have a ledge, then we don't need to go any further
		if (bPossibleClimbableIsLedge)
		{
			continue;
		}

		// If it's behind us, we can't climb onto it.
		// We don't really need to preform this check while we're climbing
		if (DetectionType != CDT_ForwardInAir)
		{
			if (!IsAttached())
			{
				if (LocalPosition.X < 0)
				{
					continue;
				}
			}
		}

		// If we're attempting to auto climb on wall, then we want a different distance than usual
		if (DetectionType == CDT_ForwardInAir)
		{
			if (LocalPosition.X > MaxForwardThrownDistance)
			{
				continue;
			}	
		}
		else
		{
			// Even though we do a circle cast, we don't want the player to be able to reach the full extent of the 
			if (CharacterState == ECharacterStateEnum::CCS_OnGround && LocalPosition.X > MaxForwardGroundJumpDistance)
			{
				continue;
			}
		}

		// Fancy directional check

		// We want to create a Transform based on the direction and then for each of the saps compare the forward direction
		auto RelativeSapPos = UKismetMathLibrary::InverseTransformLocation(
			DirectionTransform, Climbable->GetActorLocation());

		if (DetectionType != CDT_ForwardInAir)
		{
			if (RelativeSapPos.X <= 0)
			{
				continue;
			}
		}

		auto ZRot = FMath::FindDeltaAngleDegrees(GetOwner()->GetActorRotation().Yaw, Climbable->GetActorRotation().Yaw);
		if (ZRot > MaxYRotation)
		{
			continue;
		}

		auto YRot = FMath::FindDeltaAngleDegrees(GetOwner()->GetActorRotation().Pitch, Climbable->GetActorRotation().Pitch);
		if (YRot > MaxYRotation)
		{
			continue;
		}

		ClimbableRatings[RatingIndex] = 1.0f;

		
		// Magical formula for deciding best climbable
		if (DetectionType != CDT_ForwardInAir)
		{
			// We prefer going towards the positive X direction, and any saps that are farther to the left or right are not preferred
			ClimbableRatings[RatingIndex] += RelativeSapPos.X * 0.5f;
			ClimbableRatings[RatingIndex] -= FMath::Abs(RelativeSapPos.Y) * 0.1f;
		}
		else
		{
			// If we're going forward in air, then the center is actually the preferred position
			ClimbableRatings[RatingIndex] = 60;
			ClimbableRatings[RatingIndex] -= FMath::Abs(RelativeSapPos.X) * 0.1f;
			ClimbableRatings[RatingIndex] -= FMath::Abs(RelativeSapPos.Y) * 0.1f;
		}
	}
	// If we've already got a ledge, don't bother looking at any of the other options
	if (bPossibleClimbableIsLedge)
	{
		return PossibleClimbable;
	}
	
	int PossibleGoodClimableIndex = -1;
	for (int i = 0; i < PossibleClimbables.Num(); i++)
	{
		if (bIsDebugging)
		{
			if (FMath::RoundToInt(ClimbableRatings[i]) != -9999)
			{
				UKismetSystemLibrary::DrawDebugString(World, PossibleClimbables[i]->GetActorLocation(),
					FString::Printf(TEXT("%d"), FMath::RoundToInt(ClimbableRatings[i])), nullptr,
					FLinearColor::Blue, 5.0f);
			}
		}
		
		if (PossibleGoodClimableIndex == -1)
		{
			if (ClimbableRatings[i] > 0)
			{
				PossibleGoodClimableIndex = i;
			}
		}
		else if (ClimbableRatings[i] > ClimbableRatings[PossibleGoodClimableIndex])
		{
			PossibleGoodClimableIndex = i;
		}
	}

	if (PossibleGoodClimableIndex != -1)
	{
		return PossibleClimbables[PossibleGoodClimableIndex];
	}
	else
	{
		return nullptr;
	}
}

FTransform UClimbingComponent::GetHangingPosition(const AActor* NewClimbable)
{
	auto World = GetWorld();

	if (NewClimbable == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Tried to GetHangingPosition while NewClimbable was nullptr"))
		return FTransform{};
	}

	auto BaseLoc = NewClimbable->GetActorLocation();
	auto Ledge = Cast<ASplineLedge>(NewClimbable);

	if (Ledge != nullptr)
	{
		BaseLoc = Ledge->GetClimbUpTransform(GetOwner()).GetLocation();
	}

	auto HangingVerticalLocalPosition = CharacterCapsule->GetUpVector() * (DistanceFromClimbTarget.Z + CharacterCapsule
		->GetScaledCapsuleHalfHeight());
	auto HorizontalDirection = NewClimbable->GetActorForwardVector() * DistanceFromClimbTarget.X;

	if (Ledge != nullptr)
	{
		HorizontalDirection = Ledge->GetClimbUpTransform(GetOwner()).Rotator().Vector() * -DistanceFromClimbTarget.X;
	}

	FTransform Result;
	Result.SetLocation((BaseLoc - HangingVerticalLocalPosition) + HorizontalDirection);
	
	if (Ledge == nullptr)
	{
		Result.SetRotation(NewClimbable->GetActorForwardVector().Rotation().Quaternion());
	}
	else
	{
		if (bIsDebugging)
		{
			UKismetSystemLibrary::DrawDebugArrow(World, BaseLoc,
			                                     BaseLoc + ((Ledge->GetClimbUpTransform(GetOwner()).Rotator().Vector() *
				                                     -1.0f).Rotation().Quaternion().Vector() * 200),
			                                     10, FLinearColor::Green, 10.0f);
		}
		Result.SetRotation((Ledge->GetClimbUpTransform(GetOwner()).Rotator().Vector() * -1.0f).Rotation().Quaternion());
	}
	return Result;
}

void UClimbingComponent::CalculateCharacterState()
{
	if (CharacterMovement->IsMovingOnGround() && !IsClimbing())
	{
		if (PlayerRef != nullptr && CharacterState != ECharacterStateEnum::CCS_OnGround)
		{
			// update player state to neutral
			if (PlayerRef->GetState() == EPlayerStates::Climbing) 
			{
				PlayerRef->UpdateState(EPlayerStates::Neutral);
				PlayerRef->CameraBoom->ChangeCamera(ECameraTypes::Normal);
			}
		}
		CharacterState = ECharacterStateEnum::CCS_OnGround;
	}
	else if (IsClimbing())
	{
		if (PlayerRef != nullptr && CharacterState != ECharacterStateEnum::CCS_Climbing)
		{
			// update player state to climbing
			PlayerRef->UpdateState(EPlayerStates::Climbing);
		}
		CharacterState = ECharacterStateEnum::CCS_Climbing;
	}
	else
	{
		CharacterState = ECharacterStateEnum::CCS_Falling;
	}
}

void UClimbingComponent::AutoGrabChecker()
{
	if (bIsFlyingForwardInAir && bHasPossibleTargets)
	{
		auto Direction = FVector2D(0, 1);
		auto Climbable = FindBestClimbable(Direction, CDT_ForwardInAir);
		AttemptClimb(Climbable);
	}
}

void UClimbingComponent::DetectClimbables()
{
	auto World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	CalculateCharacterState();

	const TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes =
	{
		UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic),
		UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldDynamic),
	};

	const TArray<AActor*> IgnoreList = {CurrentClimbable, NextClimbable};
	TArray<AActor*> OutActors = {};

	FVector CastTarget = GetOwner()->GetActorLocation();
	if (IsAttached())
	{
		CastTarget = CurrentClimbable->GetActorLocation();
	}

	auto DetectionRadius = ClimbingDetectionRadius;
	if (CharacterState == ECharacterStateEnum::CCS_OnGround)
	{
		DetectionRadius = GroundClimbingDetectionRadius;
	}
	else if (CharacterState == ECharacterStateEnum::CCS_Falling)
	{
		DetectionRadius = InAirClimbingDetectionRadius;
	}

	// Only used if bIsFlyingForwardInAir is set
	auto FlyingForwardCastPosition = GetOwner()->GetActorLocation() + CapsuleOffset;
	bool bIsOverlapped = false;

	if (bIsFlyingForwardInAir)
	{
		auto FlyingForwardCastPosition = GetOwner()->GetActorLocation() + CapsuleOffset;
		bIsOverlapped = UKismetSystemLibrary::CapsuleOverlapActors(World, FlyingForwardCastPosition, FlyingForwardCapsuleRadius, 
			FlyingForwardCapsuleHeight * 0.5f, ObjectTypes, AClimbable::StaticClass(), IgnoreList, OutActors);
	}
	else
	{
		bIsOverlapped = UKismetSystemLibrary::SphereOverlapActors(World, CastTarget, DetectionRadius, ObjectTypes,
		                                                               AClimbable::StaticClass(), IgnoreList, OutActors);
	}

	if (bIsOverlapped)
	{
		bHasPossibleTargets = true;
		PossibleClimbables = ConvertArrayToClimbable(OutActors);
		if (bIsDebugging)
		{
			if (bIsFlyingForwardInAir)
			{
				UKismetSystemLibrary::DrawDebugCapsule(World, FlyingForwardCastPosition, FlyingForwardCapsuleHeight * 0.5f, 
					FlyingForwardCapsuleRadius, FRotator::ZeroRotator, FLinearColor::Green);
			}
			else
			{
				UKismetSystemLibrary::DrawDebugSphere(World, CastTarget, DetectionRadius, 12, FLinearColor::Green);
			}
		}
	}
	else
	{
		bHasPossibleTargets = false;
		PossibleClimbables.Empty();

		if (bIsDebugging)
		{
			if (bIsFlyingForwardInAir)
			{
				UKismetSystemLibrary::DrawDebugCapsule(World, FlyingForwardCastPosition, FlyingForwardCapsuleHeight * 0.5f,
					FlyingForwardCapsuleRadius, FRotator::ZeroRotator, FLinearColor::Red);
			}
			else
			{
				UKismetSystemLibrary::DrawDebugSphere(World, CastTarget, DetectionRadius, 12, FLinearColor::Red);
			}
		}
	}
}

bool UClimbingComponent::IsPlayerCapsuleInsideCollision(AActor* Climbable)
{
	auto World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	TArray<FOverlapResult> Overlaps;
	TArray<TEnumAsByte<EObjectTypeQuery>> Params;
	Params.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic));
	Params.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldDynamic));
	
	FCollisionObjectQueryParams ObjectParams = {Params};
	auto Transform = GetHangingPosition(Climbable);
	World->OverlapMultiByObjectType(Overlaps, Transform.GetLocation(), Transform.GetRotation(), ObjectParams,
		FCollisionShape::MakeCapsule(CharacterCapsule->GetScaledCapsuleRadius(), CharacterCapsule->GetScaledCapsuleHalfHeight()));

	for (auto Result : Overlaps)
	{
		// Let's see if the actor if set to block Mokosh
		if (Result.Component->GetCollisionResponseToChannel(ECC_MokoshChannel) == ECollisionResponse::ECR_Block)
		{
			if (bIsDebugging)
			{
				LOG(FString::Printf(TEXT("%s is in the way."), *Result.Actor->GetName()), 20);
			}
			
			return true;
		}
	}

	return false;
}

void UClimbingComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                       FActorComponentTickFunction* ThisTickFunction)
{
	// If we're currently mantling we don't need to worry about any of this stuff
	if (bIsCurrentlyMantling)
	{
		return;
	}

	// We don't want auto grabber on while in a cinematic or while being thrown by the boss
	if (PlayerRef->GetState() == EPlayerStates::Cinematic || PlayerRef->GetState() == EPlayerStates::BossThrown)
	{
		return;
	}
	
	auto LocalVelocity = UKismetMathLibrary::InverseTransformDirection(GetOwner()->GetTransform(), CharacterMovement->Velocity);
	if (LocalVelocity.X > MinimumAutoGrabForwardVelocity &&
		CharacterMovement->IsMovingOnGround() == false &&
		IsClimbing() == false)
	{
		bIsFlyingForwardInAir = true;
	}
	else
	{
		bIsFlyingForwardInAir = false;
	}

	RunAgainstWallChecker();
	
	DetectClimbables();

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bHoldingForwardDoCheck)
	{
		auto Direction = FVector2D(0, 1);
		auto Climbable = FindBestClimbable(Direction, CDT_ForwardInAir);
		AttemptClimb(Climbable);
	}

	AutoGrabChecker();
	UpdateMovement(DeltaTime);

}

void UClimbingComponent::UpdateMovement(float DeltaTime)
{
	if (!IsClimbing())
	{
		return;
	}

	if (IsMovingToNewClimbable())
	{
		if (MovementCurve != nullptr)
		{
			MovingTime += DeltaTime;
			float MinTime, MaxTime;
			MovementCurve->GetTimeRange(MinTime, MaxTime);
			auto Val = MovementCurve->GetFloatValue(MovingTime);

			// If we're done moving
			if (MovingTime >= MaxTime)
			{
				HasMovedToNewClimbable();
			}
			else
			{
				auto HangingTransform = GetHangingPosition(NextClimbable);
				GetOwner()->SetActorLocation(
					UKismetMathLibrary::VLerp(BeforeMovingPosition, HangingTransform.GetLocation(), Val));
				GetOwner()->SetActorRotation(
					UKismetMathLibrary::RLerp(BeforeMovingRotation, HangingTransform.Rotator(), Val, /*bShortestPath: */true));
			}
		}
		else
		{
			// If the curve is null for some reason, just teleport.
			auto HangingTransform = GetHangingPosition(NextClimbable);
			GetOwner()->SetActorLocationAndRotation(HangingTransform.GetLocation(), HangingTransform.Rotator());
			GEngine->AddOnScreenDebugMessage(-1, 5, FColor::Red,
			                                 TEXT(
				                                 "WARNING: MovementCurve in ClimbingSystem is null, so just teleporting"));
			HasMovedToNewClimbable();
		}
	}
	else if (IsOnMovingClimbable())
	{
		auto HangingTransform = GetHangingPosition(CurrentClimbable);
		GetOwner()->SetActorRotation(HangingTransform.GetRotation());
		GetOwner()->SetActorLocation(HangingTransform.GetLocation());
		// To improve this further collision checks should be done here to make sure we're not going inside something while moving.
		return;
	}
}

void UClimbingComponent::RunAgainstWallChecker()
{
	auto World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	bool bTimerActive = World->GetTimerManager().IsTimerActive(TimerHandle_AutoGrabWall);
	auto Vertical = GetOwner()->GetInputAxisValue("Horizontal");
	auto LocalVelocity = UKismetMathLibrary::InverseTransformDirection(GetOwner()->GetTransform(), CharacterMovement->Velocity);

	if (IsClimbing())
	{
		bIsHoldingDownForward = false;
		bHoldingForwardDoCheck = false;
		if (bTimerActive)
		{
			World->GetTimerManager().ClearTimer(TimerHandle_AutoGrabWall);
		}
	}
	
	// If we're not going forward
	if (Vertical < 0.7f)
	{
		if (bIsHoldingDownForward)
		{
			if (bTimerActive)
			{
				World->GetTimerManager().ClearTimer(TimerHandle_AutoGrabWall);
				bHoldingForwardDoCheck = false;
			}
		}
		bIsHoldingDownForward = false;
	}
	// If we're moving forward
	else
	{
		// If we're moving forward but not actually moving
		if (LocalVelocity.X < 100)
		{
			FTimerDelegate TimerCallback;
			TimerCallback.BindLambda([&]
			{
				bHoldingForwardDoCheck = true;
			});
			
			if (!bTimerActive)
			{
				World->GetTimerManager().SetTimer(TimerHandle_AutoGrabWall, TimerCallback, TimeRunningAgainstWallToTryGrab, false);
			}
		}
		else
		{
			World->GetTimerManager().ClearTimer(TimerHandle_AutoGrabWall);
			bHoldingForwardDoCheck = false;
		}
	}
}

/* This is called when you finishing lerping to the ledge hang position*/
void UClimbingComponent::OnHangingOnLedge()
{
	auto Ledge = Cast<ASplineLedge>(NextClimbable);
	if (Ledge == nullptr)
	{
		return;
	}

	bool bDoMantle = true;
	if (bDoMantle)
	{
		// Mantle animation and such
		PlayerRef->AttachToActor(CurrentClimbable, FAttachmentTransformRules::KeepWorldTransform);
		PlayerRef->IsMantleLedge = true;
		CharacterMovement->SetMovementMode(EMovementMode::MOVE_Flying);
		PlayerRef->PlayAnimMontage(ClimbLedgeMontage);
		PlayerRef->UpdateState(EPlayerStates::Mantling);
		PlayerRef->CameraBoom->ChangeCamera(ECameraTypes::Normal);
		PlayerRef->SetActorEnableCollision(false);
		auto Montage = PlayerRef->GetRootMotionAnimMontageInstance();
		if (Montage != nullptr)
		{
			Montage->OnMontageEnded.BindUObject(this, &UClimbingComponent::ClimbingMontageFinished);
		}
		bIsCurrentlyMantling = true;
	}
	else
	{
		CurrentClimbable = nullptr;
		NextClimbable = nullptr;
		MovingTime = 0.0f;
		
		auto ClimbUpTransform = Ledge->GetClimbUpTransform(GetOwner());	
		GetOwner()->SetActorLocationAndRotation(ClimbUpTransform.GetLocation() + FVector::UpVector * 100.0f,
		                                        (ClimbUpTransform.Rotator().Vector() * -1.0f).Rotation().Quaternion());
		DetachFromClimbing();
	}
}

void UClimbingComponent::ClimbingMontageFinished(class UAnimMontage* Montage, bool bInterrupted)
{
	PlayerRef->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	bIsCurrentlyMantling = false;
	CurrentClimbable = nullptr;
	NextClimbable = nullptr;
	MovingTime = 0.0f;
	PlayerRef->SetActorEnableCollision(true);
	PlayerRef->UpdateState(EPlayerStates::Neutral);
	CharacterMovement->Velocity = FVector::ZeroVector;
	DetachFromClimbing();
}

void UClimbingComponent::HasMovedToNewClimbable()
{
	NextClimbable->PlayerHasGrabbed();
	auto Ledge = Cast<ASplineLedge>(NextClimbable);
	if (Ledge != nullptr)
	{
		OnHangingOnLedge();
		return;
	}

	// Just reset the position, just in case it was slightly off.
	auto HangingTransform = GetHangingPosition(NextClimbable);
	GetOwner()->SetActorLocationAndRotation(HangingTransform.GetLocation(), HangingTransform.Rotator());

	CurrentClimbable = NextClimbable;
	NextClimbable = nullptr;
	MovingTime = 0.0f;
}

TArray<AClimbable*> UClimbingComponent::ConvertArrayToClimbable(TArray<AActor*> ActorArray)
{
	TArray<AClimbable*> Result;
	for (int i = 0; i < ActorArray.Num(); i++)
	{
		Result.Add(Cast<AClimbable>(ActorArray[i]));
	}

	return Result;
}

bool UClimbingComponent::IsAttached() const { return CurrentClimbable != nullptr; }
bool UClimbingComponent::IsClimbing() const { return CurrentClimbable != nullptr || NextClimbable != nullptr; }
bool UClimbingComponent::IsMovingToNewClimbable() const { return NextClimbable != nullptr; }

bool UClimbingComponent::IsOnMovingClimbable() const
{
	if (CurrentClimbable == nullptr)
	{
		return false;
	}
	return CurrentClimbable->bIsMoving;
}
