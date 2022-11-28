#include "ShooterAnimInstance.h"
#include "ShooterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

// Init
UShooterAnimInstance::UShooterAnimInstance():
	Speed(0.f),
	bIsInAir(false),
	bIsAccelerating(false),
	MovementOffsetYaw(0.f),
	LastMovementOffsetYaw(0.f),
	bAiming(false),
	// turn
	TIPCharacterYaw(0.f),
	TIPCharacterYawLastFrame(0.f),
	RootYawOffset(0.f),
	// lean
	CharacterRotation(FRotator(0.f)),
	CharacterRotationLastFrame(FRotator(0.f)),
	LeanYawDelta(0.f),
	LeanInterpSpeed(10.f),
	//
	Pitch(0.f),
	bReloading(false),
	OffsetState(EOffsetState::EOS_Hip)
{

}

void UShooterAnimInstance::NativeInitializeAnimation()
{
	ShooterCharacter = Cast<AShooterCharacter>(TryGetPawnOwner());
}

// Tick
void UShooterAnimInstance::UpdateAnimationProperties(float deltaTime)
{
	if (ShooterCharacter == nullptr)
	{
		ShooterCharacter = Cast<AShooterCharacter>(TryGetPawnOwner());
	}

	if (ShooterCharacter)
	{
		// reloading
		bReloading = ShooterCharacter->GetCombatState() == ECombatState::ECS_Reloading;
		bCrouching = ShooterCharacter->GetCrouching();

		// speed
		FVector velocity = ShooterCharacter->GetVelocity();
		velocity.Z = 0;
		Speed = velocity.Size();

		// in air
		bIsInAir = ShooterCharacter->GetCharacterMovement()->IsFalling();

		// acce
		if (ShooterCharacter->GetCharacterMovement()->GetCurrentAcceleration().Size() > 0.f)
		{
			bIsAccelerating = true;
		}
		else
		{
			bIsAccelerating = false;
		}

		// rotation
		FRotator AimRotation = ShooterCharacter->GetBaseAimRotation();
		FRotator MovementRotation =
			UKismetMathLibrary::MakeRotFromX(
				ShooterCharacter->GetVelocity());

		MovementOffsetYaw = UKismetMathLibrary::NormalizedDeltaRotator(
			MovementRotation,
			AimRotation).Yaw;

		if (ShooterCharacter->GetVelocity().Size() > 0.f)
		{
			LastMovementOffsetYaw = MovementOffsetYaw;
		}
		
		bAiming = ShooterCharacter->GetAiming();

		if (bReloading)
		{
			OffsetState = EOffsetState::EOS_Reloading;
		}
		else if (bIsInAir)
		{
			OffsetState = EOffsetState::EOS_InAir;
		}
		else if (ShooterCharacter->GetAiming())
		{
			OffsetState = EOffsetState::EOS_Aiming;
		}
		else
		{
			OffsetState = EOffsetState::EOS_Hip;
		}
	}

	TurnInPlace();
	Lean(deltaTime);
}


void UShooterAnimInstance::TurnInPlace()
{
	if (ShooterCharacter == nullptr) return;

	Pitch = ShooterCharacter->GetBaseAimRotation().Pitch;

	// move or jump, reset
	if (Speed > 0 || bIsInAir)
	{
		// Don't want to turn in place; Character is moving
		RootYawOffset = 0.f;
		
		TIPCharacterYaw = ShooterCharacter->GetActorRotation().Yaw;
		TIPCharacterYawLastFrame = TIPCharacterYaw;
		
		RotationCurve = 0.f;
		RotationCurveLastFrame = 0.f;
	}
	else
	{
		TIPCharacterYawLastFrame = TIPCharacterYaw;
		TIPCharacterYaw = ShooterCharacter->GetActorRotation().Yaw;
		const float TIPYawDelta{ TIPCharacterYaw - TIPCharacterYawLastFrame };

		// Root Yaw Offset, updated and clamped to [-180, 180]
		RootYawOffset = UKismetMathLibrary::NormalizeAxis(RootYawOffset - TIPYawDelta);

		// 1.0 if turning, 0.0 if not
		const float Turning{ GetCurveValue(TEXT("Turning")) };
		if (Turning > 0)
		{
			RotationCurve = GetCurveValue(TEXT("Rotation"));
			const float DeltaRotation{ RotationCurve - RotationCurveLastFrame };

			// RootYawOffset > 0, -> Turning Left. RootYawOffset < 0, -> Turning Right.
			RootYawOffset > 0 ? RootYawOffset -= DeltaRotation : RootYawOffset += DeltaRotation;

			const float ABSRootYawOffset{ FMath::Abs(RootYawOffset) };
			if (ABSRootYawOffset > 90.f)
			{
				const float YawExcess{ ABSRootYawOffset - 90.f };
				RootYawOffset > 0 ? RootYawOffset -= YawExcess : RootYawOffset += YawExcess;
			}

			RotationCurveLastFrame = RotationCurve;
		}

		if (GEngine) GEngine->AddOnScreenDebugMessage(
			1, 
			-1, 
			FColor::Blue, 
			FString::Printf(TEXT("TIPCharacterYaw: %f"), TIPCharacterYaw));
		if (GEngine) GEngine->AddOnScreenDebugMessage(
			2,
			-1,
			FColor::Red,
			FString::Printf(TEXT("RootYawOffset: %f"), RootYawOffset));
	}
}

#pragma region Lean
void UShooterAnimInstance::Lean(float DeltaTime)
{
	if (ShooterCharacter == nullptr) return;

	CharacterRotationLastFrame = CharacterRotation;
	CharacterRotation = ShooterCharacter->GetActorRotation();

	const FRotator DeltaRot{ UKismetMathLibrary::NormalizedDeltaRotator(CharacterRotation, CharacterRotationLastFrame) };

	const float Target{ (float)DeltaRot.Yaw / DeltaTime };
	const float Interp{ 
		FMath::FInterpTo(LeanYawDelta, Target, DeltaTime, LeanInterpSpeed) };
	LeanYawDelta = FMath::Clamp(Interp, -90.f, 90.f);

	if (GEngine) GEngine->AddOnScreenDebugMessage(
			1, 
			-1, 
			FColor::Blue, 
			FString::Printf(TEXT("LeanYawDelta.Yaw: %f"), DeltaRot.Yaw));
}
#pragma endregion
