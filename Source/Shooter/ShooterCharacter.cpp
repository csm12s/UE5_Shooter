#include "ShooterCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "Engine/SkeletalMeshSocket.h"
#include "DrawDebugHelpers.h"
#include "Particles/ParticleSystemComponent.h"
#include "Item.h"
#include "Components/WidgetComponent.h"
#include "Weapon.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"

// Sets default values
AShooterCharacter::AShooterCharacter() :
	BaseTurnRate(45.f),
	BaseLookUpRate(45.f),
	// controller 
	HipTurnRate(90.f),
	HipLookUpRate(90.f),
	AimingTurnRate(20.f),
	AimingLookUpRate(20.f),
	// mouse
	MouseHipTurnRate(1.0f),
	MouseHipLookUpRate(1.0f),
	MouseAimingTurnRate(0.2f),
	MouseAimingLookUpRate(0.2f),
	// aim
	bAiming(false),
	// FOV
	CameraDefaultFOV(90.f), // set in BeginPlay
	CameraZoomedFOV(35.f),
	CameraCurrentFOV(90.f),
	ZoomInterpSpeed(35.f),
	// Crosshair spread factors
	CrosshairSpreadMultiplier(0.f),
	CrosshairVelocityFactor(0.f),
	CrosshairInAirFactor(0.f),
	CrosshairAimFactor(0.f),
	CrosshairShootingFactor(0.f),
	// Bullet fire timer variables
	CrosshairFirePeriod(0.05f),
	bFiringBullet(false),
	// Auto fire
	AutoFirePeriod(0.1f),
	bShouldFire(true),
	bFireButtonPressed(false),
	// pick up
	bShouldTraceForItems(false),
	OverlappedItemCount(0),
	// ammo
	Starting9mmAmmo(85),
	StartingARAmmo(120),
	// crouch
	bCrouching(false),
	BaseMovementSpeed(650.f),
	CrouchMovementSpeed(300.f),
	StandingCapsuleHalfHeight(88.f),
	CrouchingCapsuleHalfHeight(44.f),
	BaseGroundFriction(2.f),
	CrouchingGroundFriction(100.f)
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Camera boom / Spring arm
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 180.f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->SocketOffset = FVector(0.f, 50.f, 70.f);

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	//CameraBoom->bUsePawnControlRotation = false;

	// Free camera: RPG camera
	if (bIsFreeCamera)
	{
		bUseControllerRotationYaw = false;
	}
	else // TPS camera
	{
		bUseControllerRotationYaw = true;
	}
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	// Rotate to move direction
	if (bIsFreeCamera)
	{
		GetCharacterMovement()->bOrientRotationToMovement = true;
	}
	else
	{
		GetCharacterMovement()->bOrientRotationToMovement = false;
	}
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	HandSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("HandSceneComp"));
}

#pragma region Init

// Called when the game starts or when spawned
void AShooterCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (FollowCamera)
	{
		CameraDefaultFOV = GetFollowCamera()->FieldOfView;
		CameraCurrentFOV = CameraDefaultFOV;
	}

	InitializeAmmoMap();
	SpawnDefaultWeapon();
}
#pragma endregion


void AShooterCharacter::MoveForward(float value)
{
	if (Controller != nullptr && value != 0.0f)
	{
		const FRotator rotation = Controller->GetControlRotation();
		const FRotator yawRotation{ 0, rotation.Yaw, 0 };
		const FVector direction = FRotationMatrix{ yawRotation }.GetUnitAxis(EAxis::X);
		AddMovementInput(direction, value);

		StartCrosshairMove();
	}
	else
	{
		StopCrosshairMove();
	}
}

void AShooterCharacter::MoveRight(float value)
{
	if (Controller != nullptr && value != 0.0f)
	{
		const FRotator rotation = Controller->GetControlRotation();
		const FRotator yawRotation{ 0, rotation.Yaw, 0 };
		const FVector direction = FRotationMatrix{ yawRotation }.GetUnitAxis(EAxis::Y);
		AddMovementInput(direction, value);

		StartCrosshairMove();
	}
	else
	{
		StopCrosshairMove();
	}
}

void AShooterCharacter::OnStartJump()
{
	Jump();
}

void AShooterCharacter::OnStopJump()
{
	StopJumping();
}

void AShooterCharacter::Turn(float Value)
{
	float TurnScaleFactor{};
	if (bAiming)
	{
		TurnScaleFactor = MouseAimingTurnRate;
	}
	else
	{
		TurnScaleFactor = MouseHipTurnRate;
	}
	AddControllerYawInput(Value * TurnScaleFactor);
}

void AShooterCharacter::LookUp(float Value)
{
	float LookUpScaleFactor{};
	if (bAiming)
	{
		LookUpScaleFactor = MouseAimingLookUpRate;
	}
	else
	{
		LookUpScaleFactor = MouseHipLookUpRate;
	}
	AddControllerPitchInput(Value * LookUpScaleFactor);
}


void AShooterCharacter::TurnAtRate(float value)
{   // deg/sec * sec/frame = deg/frame
	AddControllerYawInput(value * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AShooterCharacter::LookUpAtRate(float value)
{
	AddControllerPitchInput(value * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

bool AShooterCharacter::GetBeamEndLocation(
	const FVector& MuzzleSocketLocation,
	FVector& OutBeamEnd)
{
	bool found = false;

	FHitResult CrosshairHitResult;
	bool bCrosshairHit = TraceFromCrosshair(CrosshairHitResult, OutBeamEnd);

	if (bCrosshairHit)
	{
		found = true;
		OutBeamEnd = CrosshairHitResult.Location;

		// Perform a second trace, from the gun barrel
		if (bUseWeaponTrace)
		{
			FHitResult WeaponTraceHit;
			const FVector WeaponTraceStart{ MuzzleSocketLocation };
			const FVector WeaponTraceEnd{ OutBeamEnd };
			GetWorld()->LineTraceSingleByChannel(
				WeaponTraceHit,
				WeaponTraceStart,
				WeaponTraceEnd,
				ECollisionChannel::ECC_Visibility);
			if (WeaponTraceHit.bBlockingHit)
			{
				OutBeamEnd = WeaponTraceHit.Location;
			}
		}
	}

	return found;
}

void AShooterCharacter::SwitchAim()
{
	if (bAiming)
	{
		StopAim();
	}
	else
	{
		StartAim();
	}
}

void AShooterCharacter::StartAim()
{
	if (EquippedWeapon)
	{
		bAiming = true;
	}
}

void AShooterCharacter::StopAim()
{
	bAiming = false;
}

void AShooterCharacter::CameraInterpZoom(float DeltaTime)
{
	// Set current camera field of view
	if (bAiming)
	{
		// Interpolate to zoomed FOV
		CameraCurrentFOV = FMath::FInterpTo(
			CameraCurrentFOV,
			CameraZoomedFOV,
			DeltaTime,
			ZoomInterpSpeed);
	}
	else
	{
		// Interpolate to default FOV
		CameraCurrentFOV = FMath::FInterpTo(
			CameraCurrentFOV,
			CameraDefaultFOV,
			DeltaTime,
			ZoomInterpSpeed);
	}
	GetFollowCamera()->SetFieldOfView(CameraCurrentFOV);
}

void AShooterCharacter::SetLookRates()
{
	// controller
	if (bAiming)
	{
		BaseTurnRate = AimingTurnRate;
		BaseLookUpRate = AimingLookUpRate;
	}
	else
	{
		BaseTurnRate = HipTurnRate;
		BaseLookUpRate = HipLookUpRate;
	}
}

void AShooterCharacter::CalculateCrosshairSpread(float DeltaTime)
{
	// Move based on speed, set in moveforward
	/*FVector2D WalkSpeedRange{ 0.f, 600.f };
	FVector2D VelocityMultiplierRange{ 0.f, 1.f };
	FVector Velocity{ GetVelocity() };
	Velocity.Z = 0.f;
	float CrosshairVelocityFactorTemp = FMath::GetMappedRangeValueClamped(
		WalkSpeedRange,
		VelocityMultiplierRange,
		Velocity.Size());*/

		// Move interp
	if (bIsMoving)
	{
		CrosshairVelocityFactor = FMath::FInterpTo(
			CrosshairVelocityFactor,
			1.5f,
			DeltaTime,
			20.f);
	}
	else
	{
		CrosshairVelocityFactor = FMath::FInterpTo(
			CrosshairVelocityFactor,
			0.f,
			DeltaTime,
			20.f);
	}

	// Jump
	if (GetCharacterMovement()->IsFalling())
	{
		CrosshairInAirFactor = FMath::FInterpTo(
			CrosshairInAirFactor,
			2.25f,
			DeltaTime,
			20.f);
	}
	else // On ground
	{
		CrosshairInAirFactor = FMath::FInterpTo(
			CrosshairInAirFactor,
			0.f,
			DeltaTime,
			30.f);
	}

	// Aim
	if (bAiming)
	{
		CrosshairAimFactor = FMath::FInterpTo(
			CrosshairAimFactor,
			0.6f,
			DeltaTime,
			30.f);
	}
	else // Not aiming
	{
		CrosshairAimFactor = FMath::FInterpTo(
			CrosshairAimFactor,
			0.f,
			DeltaTime,
			30.f);
	}

	// Fire, True 0.05 second after firing
	if (bFiringBullet)
	{
		CrosshairShootingFactor = FMath::FInterpTo(
			CrosshairShootingFactor,
			0.3f,
			DeltaTime,
			60.f); // crosshair get big speed
	}
	else
	{
		CrosshairShootingFactor = FMath::FInterpTo(
			CrosshairShootingFactor,
			0.f,
			DeltaTime,
			5.f); // crosshair get small speed, slow enough so next fire will expand crosshair before it go back to origin
	}

	CrosshairSpreadMultiplier =
		1.f + //0.5f
		CrosshairVelocityFactor +
		CrosshairInAirFactor -
		CrosshairAimFactor +
		CrosshairShootingFactor;
}

void AShooterCharacter::StartCrosshairBulletFire()
{
	bFiringBullet = true;

	GetWorldTimerManager().SetTimer(
		CrosshairShootTimer,
		this,
		&AShooterCharacter::FinishCrosshairBulletFire,
		CrosshairFirePeriod);
}

void AShooterCharacter::FinishCrosshairBulletFire()
{
	bFiringBullet = false;
}

void AShooterCharacter::StartCrosshairMove()
{
	bIsMoving = true;
}

void AShooterCharacter::StopCrosshairMove()
{
	bIsMoving = false;
}

// auto fire
void AShooterCharacter::FireButtonPressed()
{
	bFireButtonPressed = true;
	FireWeapon();
}

void AShooterCharacter::FireButtonReleased()
{
	bFireButtonPressed = false;
}

bool AShooterCharacter::TraceFromCrosshair(
	FHitResult& OutHitResult,
	FVector& OutHitLocation)
{
	bool found = false;

	// Get Viewport Size
	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}

	// Get screen space location of crosshairs
	FVector2D CrosshairLocation(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);
	FVector CrosshairWorldPosition;
	FVector CrosshairWorldDirection;

	// Screen deproject to world
	bool bScreenToWorld = UGameplayStatics::DeprojectScreenToWorld(
		UGameplayStatics::GetPlayerController(this, 0),
		CrosshairLocation,
		CrosshairWorldPosition,
		CrosshairWorldDirection);

	if (bScreenToWorld) // deproject success
	{
		// Trace from Crosshair world location
		const FVector Start{ CrosshairWorldPosition };
		const FVector End{ Start + CrosshairWorldDirection * 50'000.f };
		OutHitLocation = End;

		GetWorld()->LineTraceSingleByChannel(
			OutHitResult,
			Start,
			End,
			ECollisionChannel::ECC_Visibility);
		if (OutHitResult.bBlockingHit)
		{
			found = true;
			OutHitLocation = OutHitResult.Location;

			//test
			AActor* HitItem = OutHitResult.GetActor();
			if (HitItem) // AWeapon
			{
				found = true;
			}
		}
	}

	return found;
}

void AShooterCharacter::TraceForItems()
{
	if (bShouldTraceForItems)
	{
		// todo collide with character

		// Crosshair trace
		FHitResult ItemTraceResult;
		FVector HitLocation;
		TraceFromCrosshair(ItemTraceResult, HitLocation);
		if (ItemTraceResult.bBlockingHit)
		{
			TraceHitItem = Cast<AItem>(ItemTraceResult.GetActor());

			if (TraceHitItem)
			{
				TraceHitItem->ShowUI();
			}

			if (LastTraceItem)
			{
				// We are hitting a different AItem this frame from last frame
				// Or AItem is null.
				if (TraceHitItem != LastTraceItem)
				{
					LastTraceItem->HideUI();
				}
			}

			LastTraceItem = TraceHitItem;
		}
	}
	else // !bShouldTraceForItems
	{
		// Crosshair trace
		if (LastTraceItem)
		{
			LastTraceItem->HideUI();
		}
	}
}

void AShooterCharacter::SpawnDefaultWeapon()
{
	// Check the TSubclassOf variable
	if (DefaultWeaponClass)
	{
		AWeapon* DefaultWeapon = GetWorld()->SpawnActor<AWeapon>(DefaultWeaponClass);
		EquipWeapon(DefaultWeapon);
	}
}


// Called every frame
void AShooterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	CameraInterpZoom(DeltaTime);

	SetLookRates();

	CalculateCrosshairSpread(DeltaTime);

	TraceForItems();
}

float AShooterCharacter::GetCrosshairSpreadMultiplier() const
{
	return CrosshairSpreadMultiplier;
}

void AShooterCharacter::AddOverlappedItemCount(int8 Amount)
{
	OverlappedItemCount += Amount;

	if (OverlappedItemCount > 0)
	{
		bShouldTraceForItems = true;
	}
	else
	{
		bShouldTraceForItems = false;
		OverlappedItemCount = 0;
	}
}

void AShooterCharacter::AutoPickUpItem(AItem* item)
{
	// todo TraceForItems
	CollisionItem = item;

	if (!EquippedWeapon)
	{
		auto weapon = Cast<AWeapon>(item);
		if (weapon)
		{
			CollisionItem = nullptr;
			EquipWeapon(weapon);
		}
	}
}

void AShooterCharacter::EquipWeapon(AWeapon* WeaponToEquip)
{
	if (WeaponToEquip)
	{
		// Get the Hand Socket
		const USkeletalMeshSocket* HandSocket = GetMesh()->GetSocketByName(
			FName("RightHandSocket"));
		if (HandSocket)
		{
			// Attach the Weapon to the hand socket RightHandSocket
			HandSocket->AttachActor(WeaponToEquip, GetMesh());
		}
		// Set EquippedWeapon to the newly spawned Weapon
		EquippedWeapon = WeaponToEquip;
		EquippedWeapon->SetItemState(EItemState::EIS_Equipped);
	}
}

void AShooterCharacter::DropWeapon()
{
	if (EquippedWeapon)
	{
		FDetachmentTransformRules DetachmentTransformRules(EDetachmentRule::KeepWorld, true);
		EquippedWeapon->GetItemMesh()->DetachFromComponent(DetachmentTransformRules);

		EquippedWeapon->SetItemState(EItemState::EIS_Falling);
		EquippedWeapon->ThrowWeapon();
		EquippedWeapon = nullptr;

		if (CollisionItem)
		{
			auto weapon = Cast<AWeapon>(CollisionItem);
			if (weapon)
			{
				EquipWeapon(weapon);
				CollisionItem = nullptr;
			}
		}

	}

}

void AShooterCharacter::DropButtonPressed()
{
	DropWeapon();
}

void AShooterCharacter::SelectButtonPressed()
{
	if (TraceHitItem)
	{
		auto TraceHitWeapon = Cast<AWeapon>(TraceHitItem);
		SwapWeapon(TraceHitWeapon);
	}
}

void AShooterCharacter::SelectButtonReleased()
{
}

void AShooterCharacter::SwapWeapon(AWeapon* WeaponToSwap)
{
	DropWeapon();
	EquipWeapon(WeaponToSwap);
}

#pragma region Fire weapon
void AShooterCharacter::FireWeapon()
{
	if (EquippedWeapon == nullptr) return;
	if (CombatState != ECombatState::ECS_Unoccupied) return;

	if (WeaponHasAmmo())
	{
		PlayFireSound();
		SendBullet();
		PlayGunfireMontage();
		EquippedWeapon->DecrementAmmo();

		StartFireTimer();
	}
}

void AShooterCharacter::InitializeAmmoMap()
{
	AmmoMap.Add(EMyAmmoType::EAT_9mm, Starting9mmAmmo);
	AmmoMap.Add(EMyAmmoType::EAT_AR, StartingARAmmo);
}

bool AShooterCharacter::WeaponHasAmmo()
{
	if (EquippedWeapon == nullptr) return false;

	return EquippedWeapon->GetAmmo() > 0;
}

void AShooterCharacter::PlayFireSound()
{
	// Play fire sound
	if (FireSound)
	{
		UGameplayStatics::PlaySound2D(this, FireSound);
	}
}

void AShooterCharacter::SendBullet()
{
	// Barrel
	const USkeletalMeshSocket* BarrelSocket =
		EquippedWeapon->GetItemMesh()->GetSocketByName("BarrelSocket");
	if (BarrelSocket)
	{
		// FX: MuzzleFlash + bullet shell
		const FTransform SocketTransform =
			BarrelSocket->GetSocketTransform(EquippedWeapon->GetItemMesh());
		if (MuzzleFlash)
		{
			UGameplayStatics::SpawnEmitterAtLocation
			(GetWorld(), MuzzleFlash, SocketTransform);
		}

		// Hit
		FVector BeamEnd;
		bool bHit = GetBeamEndLocation(SocketTransform.GetLocation(), BeamEnd);

		// beam
		UParticleSystemComponent* Beam = UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(),
			BeamParticles,
			SocketTransform);
		if (Beam)
		{
			Beam->SetVectorParameter(FName("Target"), BeamEnd);
		}

		// impact
		if (bHit)
		{
			if (ImpactParticles)
			{
				UGameplayStatics::SpawnEmitterAtLocation(
					GetWorld(),
					ImpactParticles,
					BeamEnd);
			}
		}

	}
}

void AShooterCharacter::PlayGunfireMontage()
{
	// Play Hip Fire Montage
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && HipFireMontage)
	{
		AnimInstance->Montage_Play(HipFireMontage);
		AnimInstance->Montage_JumpToSection(FName("StartFire"));
	}

	// Start bullet fire timer for crosshairs
	StartCrosshairBulletFire();
}

void AShooterCharacter::StartFireTimer()
{
	CombatState = ECombatState::ECS_FireTimerInProgress;

	GetWorldTimerManager().SetTimer(
		AutoFireTimer,
		this,
		&AShooterCharacter::AutoFireReset,
		AutoFirePeriod);
}

void AShooterCharacter::AutoFireReset()
{
	CombatState = ECombatState::ECS_Unoccupied;

	if (WeaponHasAmmo())
	{
		if (bFireButtonPressed)
		{
			FireWeapon();
		}
	}
	else
	{
		// Reload Weapon
		ReloadWeapon();
	}
}
#pragma endregion


#pragma region Reload
void AShooterCharacter::ReloadButtonPressed()
{
	ReloadWeapon();
}

void AShooterCharacter::ReloadWeapon()
{
	if (CombatState != ECombatState::ECS_Unoccupied) return;

	if (CarryingAmmo())
	{
		CombatState = ECombatState::ECS_Reloading;

		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		if (AnimInstance && ReloadMontage)
		{
			AnimInstance->Montage_Play(ReloadMontage);
			AnimInstance->Montage_JumpToSection(
				EquippedWeapon->GetReloadMontageSection());
		}
	}

}

bool AShooterCharacter::CarryingAmmo()
{
	if (EquippedWeapon == nullptr) return false;

	auto AmmoType = EquippedWeapon->GetAmmoType();

	if (AmmoMap.Contains(AmmoType))
	{
		return AmmoMap[AmmoType] > 0;
	}

	return false;
}

void AShooterCharacter::FinishReloading()
{
	CombatState = ECombatState::ECS_Unoccupied;

	// Update AmmoMap
	if (EquippedWeapon == nullptr) return;
	const auto AmmoType{ EquippedWeapon->GetAmmoType() };

	// Update the AmmoMap
	if (AmmoMap.Contains(AmmoType))
	{
		// Amount of ammo the Character is carrying of the EquippedWeapon type
		int32 CarriedAmmo = AmmoMap[AmmoType];

		// Space left in the magazine of EquippedWeapon
		const int32 MagEmptySpace =
			EquippedWeapon->GetMagazineCapacity() -
			EquippedWeapon->GetAmmo();

		if (MagEmptySpace > CarriedAmmo)
		{
			// Reload the magazine with all the ammo we are carrying
			EquippedWeapon->ReloadAmmo(CarriedAmmo);
			CarriedAmmo = 0;
			AmmoMap.Add(AmmoType, CarriedAmmo);
		}
		else
		{
			// fill the magazine
			EquippedWeapon->ReloadAmmo(MagEmptySpace);
			CarriedAmmo -= MagEmptySpace;
			AmmoMap.Add(AmmoType, CarriedAmmo);
		}
	}

}

#pragma endregion


#pragma region Player input
// Called to bind functionality to input
void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	check(PlayerInputComponent);
	PlayerInputComponent->BindAxis("MoveForward", this, &AShooterCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AShooterCharacter::MoveRight);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AShooterCharacter::OnStartJump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &AShooterCharacter::OnStopJump);

	// mouse
	PlayerInputComponent->BindAxis("Turn", this, &AShooterCharacter::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &AShooterCharacter::LookUp);
	// controller / arrow key
	PlayerInputComponent->BindAxis("TurnRate", this, &AShooterCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AShooterCharacter::LookUpAtRate);

	PlayerInputComponent->BindAction("Fire", IE_Pressed, this,
		&AShooterCharacter::FireButtonPressed);
	PlayerInputComponent->BindAction("Fire", IE_Released, this,
		&AShooterCharacter::FireButtonReleased);

	PlayerInputComponent->BindAction("Aim", IE_Pressed, this,
		&AShooterCharacter::SwitchAim);

	PlayerInputComponent->BindAction("Drop", IE_Pressed, this,
		&AShooterCharacter::DropButtonPressed);

	PlayerInputComponent->BindAction("Select", IE_Pressed, this,
		&AShooterCharacter::SelectButtonPressed);
	PlayerInputComponent->BindAction("Select", IE_Released, this,
		&AShooterCharacter::SelectButtonReleased);

	PlayerInputComponent->BindAction("Reload", IE_Pressed, this,
		&AShooterCharacter::ReloadButtonPressed);

	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this,
		&AShooterCharacter::CrouchButtonPressed);
}

void AShooterCharacter::GrabClip()
{
	if (EquippedWeapon == nullptr) return;
	if (HandSceneComponent == nullptr) return;

	// Index for the clip bone on the Equipped Weapon
	int32 ClipBoneIndex{ EquippedWeapon->GetItemMesh()->GetBoneIndex(EquippedWeapon->GetClipBoneName()) };
	// Store the transform of the clip
	ClipTransform = EquippedWeapon->GetItemMesh()->GetBoneTransform(ClipBoneIndex);

	FAttachmentTransformRules AttachmentRules(EAttachmentRule::KeepRelative, true);
	HandSceneComponent->AttachToComponent(GetMesh(), AttachmentRules, FName(TEXT("Hand_L")));
	HandSceneComponent->SetWorldTransform(ClipTransform);

	EquippedWeapon->SetMovingClip(true);
}

void AShooterCharacter::ReleaseClip()
{
	EquippedWeapon->SetMovingClip(false);
}
#pragma endregion

#pragma region Crouch
void AShooterCharacter::CrouchButtonPressed()
{
	if (!GetCharacterMovement()->IsFalling())
	{
		bCrouching = !bCrouching;
	}

	if (bCrouching)
	{
		GetCharacterMovement()->MaxWalkSpeed = CrouchMovementSpeed;
		GetCharacterMovement()->GroundFriction = CrouchingGroundFriction;
	}
	else
	{
		GetCharacterMovement()->MaxWalkSpeed = BaseMovementSpeed;
		GetCharacterMovement()->GroundFriction = BaseGroundFriction;
	}
}

#pragma endregion
