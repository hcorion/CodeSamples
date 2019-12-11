/* Copyright 2019 Alexandrea Shackelford, Zion Nimchuk, All Rights Reserved.
 * Unauthorized copying or modification of this file, via any medium is strictly prohibited.
 */
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "World/Climbable.h"
#include "Components/CapsuleComponent.h"
#include "AI/Navigation/AvoidanceManager.h"
#include "ClimbingComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGrabbedNewClimbableDelegate, AClimbable*, AttachedClimbable);


UENUM(BlueprintType)
enum class EClimbingDirectionEnum: uint8
{
	CD_Up UMETA(DisplayName = "Up"),
	CD_Left UMETA(DisplayName = "Left"),
	CD_Right UMETA(DisplayName = "Right")
};

enum EClimableDetectionTypeEnum
{
	CDT_Walking, CDT_InAir, CDT_ForwardInAir, CDT_IsClimbing
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class THEELDER_API UClimbingComponent : public UActorComponent
{

	GENERATED_BODY()


public:

	// Sets default values for this component's properties
	UClimbingComponent();

	/* This should be called in BeginPlay on the parent*/
	void Init(UCharacterMovementComponent* ParentCharacterMovement, UCapsuleComponent* ParentCapsule);

	/* This should be sent from the player controller, on a button press*/
	void ForceInitClimb();
	
	bool AttemptClimb(AClimbable* NewClimbable);

	void OnHangingOnLedge();

	/* Blueprint event to allow for particle spawning and such*/
	UPROPERTY(BlueprintAssignable, Category = "Climbing")
	FGrabbedNewClimbableDelegate OnGrabbedNewClimbable;
	

	void DetachFromClimbing();
	/**
	 * \brief 
	 * \param InputDirection (Up by default) The direction in which we want to climb towards
	 * \return The best Climbable to move to!
	 */

	AClimbable* FindBestClimbable(FVector2D InputDirection, EClimableDetectionTypeEnum DetectionType);

	FTransform GetHangingPosition(const AActor* NewClimbable);

	void DetectClimbables();
	
	/**
	 * \brief 
	 * \param Climbable The climbable that is used to 
	 * \return Returns true if the player's possible collision is inside geometry (ie an area the player can't climb into)
	 */
	bool IsPlayerCapsuleInsideCollision(AActor* Climbable);

	/**
	 * \brief
	 * \return Is the player currently attached to a climbable.
	 */
	bool IsAttached() const;

	bool IsClimbing() const;

	/**
	 * \brief
	 * \return Is the player jumping towards a new climbable.
	 */
	bool IsMovingToNewClimbable() const;

	/**
	* \brief Returns whether the player is on a moving climbable, IE like climbing on a boss.
	* \return Is the player currently attached to a moving climbable.
	*/
	bool IsOnMovingClimbable() const;
	

protected:

	// Called when the game starts
	virtual void BeginPlay() override;
	
	UFUNCTION()
	void ClimbingMontageFinished(class UAnimMontage* Montage, bool bInterrupted);


public:

	void HasMovedToNewClimbable();
	void UpdateMovement(float DeltaTime);

	
	void RunAgainstWallChecker();
	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	UPROPERTY(EditDefaultsOnly, Category = "Tunables\|Climbing")
	UCurveFloat* MovementCurve;
	
	/* The value at which in the dot product calculation the animation switches from left/right to up*/
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Tunables\|Animation")
	float ClimbingAnimDotProductDifference = 0.5f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Tunables\|Hanging")
	FVector DistanceFromClimbTarget = FVector(-150, 0, -200);
	
	UPROPERTY(EditDefaultsOnly, Category = "Tunables\|Detection")
	float ClimbingDetectionRadius = 800.0f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Tunables\|Detection")
	float GroundClimbingDetectionRadius = 800.0f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Tunables\|Detection")
	float InAirClimbingDetectionRadius = 50.0f;
	
	/* This variable represents how far in front of the player they can grab a climbable while walking on the ground.*/
	UPROPERTY(EditDefaultsOnly, Category = "Tunables\|Detection")
	float MaxForwardGroundJumpDistance = 100.0f;
	
	/* This variable represents how far in front of the player they can grab a climbable while in the air being thrown.*/
	UPROPERTY(EditDefaultsOnly, Category = "Tunables\|Detection")
	float MaxForwardThrownDistance = 20.0f;
	
	/* This is the maximum rotation that we can grab a climbable at on the Z axis (Yaw)*/
	UPROPERTY(EditDefaultsOnly, Category = "Tunables\|Detection")
	float MaxZRotation = 45.0f;
	
	/* This is the maximum rotation that we can grab a climbable at on the Y axis (Pitch)*/
	UPROPERTY(EditDefaultsOnly, Category = "Tunables\|Detection")
	float MaxYRotation = 45.0f;
	
	/* This is the minimum speed at which the character will attempt to automatically grab while in the air*/
	UPROPERTY(EditDefaultsOnly, Category = "Tunables\|Detection")
	float MinimumAutoGrabForwardVelocity = 45.0f;
	
	/* The time that the player needs to run against a wall before we try to auto grab a wall*/
	UPROPERTY(EditDefaultsOnly, Category = "Tunables\|Detection")
	float TimeRunningAgainstWallToTryGrab = 0.1f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Tunables\|Detection\|FlyingForward", meta = (DisplayName = "Capsule Height Detection"))
	float FlyingForwardCapsuleHeight = 45.0f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Tunables\|Detection\|FlyingForward", meta = (DisplayName = "Capsule Radius Detection"))
	float FlyingForwardCapsuleRadius = 45;
	
	UPROPERTY(EditDefaultsOnly, Category = "Tunables\|Detection\|FlyingForward")
	FVector CapsuleOffset = FVector();
	
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Tunables\|Debugging")
	bool bIsDebugging = false;
	
	UPROPERTY()
	UCapsuleComponent* CharacterCapsule;
	
	UPROPERTY()
	UCharacterMovementComponent* CharacterMovement;
	
	FVector BeforeMovingPosition;
	
	FRotator BeforeMovingRotation;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Animations")
	class UAnimMontage* ClimbLedgeMontage;


private:

	enum ECharacterStateEnum
	{
		CCS_OnGround, CCS_Climbing, CCS_Falling
	};

	bool bHasPossibleTargets = false;
	
	/*Value used for lerping between current position and new climbables*/
	float MovingTime;
	
	UPROPERTY()
	TArray<AClimbable*> PossibleClimbables;
	
	/*The climbable we're currently on*/
	UPROPERTY()
	AClimbable* CurrentClimbable;
	
	/*The climbable we're going to move to*/
	UPROPERTY()
	AClimbable* NextClimbable;

	ECharacterStateEnum CharacterState = CCS_OnGround;

	// reference to mokosh to update player state
	class AMokosh* PlayerRef;

	bool bIsFlyingForwardInAir = false;

	/* This timer is used to check if the player has been running against the wall for a time*/
	UPROPERTY()
	FTimerHandle TimerHandle_AutoGrabWall;
	
	bool bIsHoldingDownForward = false;
	
	bool bHoldingForwardDoCheck = false;

	// Mantling stuff
	bool bIsCurrentlyMantling = false;

	void CalculateCharacterState();
	
	void AutoGrabChecker();
	
	TArray<AClimbable*> ConvertArrayToClimbable(TArray<AActor*> ActorArray);
	
	/* Gets the direction that the the player is jumping towards*/
	EClimbingDirectionEnum GetDirection(AActor* Origin, AClimbable* TheClimbable) const;

};