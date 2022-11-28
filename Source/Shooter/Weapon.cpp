// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon.h"

AWeapon::AWeapon() :
	ThrowWeaponTime(3.f),
	bFalling(false),
	Ammo(0)
{
	// Actor can tick
	PrimaryActorTick.bCanEverTick = true;
}

void AWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Keep the Weapon upright
	/*if (GetItemState() == EItemState::EIS_Falling && bFalling)
	{
		const FRotator MeshRotation{ 0.f, GetItemMesh()->GetComponentRotation().Yaw, 0.f };
		GetItemMesh()->SetWorldRotation(MeshRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}*/
}

void AWeapon::ThrowWeapon()
{
	FRotator MeshRotation{ 0.f, GetItemMesh()->GetComponentRotation().Yaw, 0.f };
	GetItemMesh()->SetWorldRotation(MeshRotation, false, nullptr, ETeleportType::TeleportPhysics);

	const FVector MeshForward{ GetItemMesh()->GetForwardVector() };
	// Rotate by Right / rotate down
	const FVector MeshRight{ GetItemMesh()->GetRightVector() };
	// Direction in which we throw the Weapon
	FVector throwDirection = MeshRight.RotateAngleAxis(-10.f, MeshForward);

	// Rotate by Z
	float RandomRotation{ FMath::FRandRange(-10, 10)};
	throwDirection = throwDirection.RotateAngleAxis(RandomRotation, 
		FVector(0.f, 0.f, 1.f));
	throwDirection *= 10'000.f;
	GetItemMesh()->AddImpulse(throwDirection);

	bFalling = true;
	GetWorldTimerManager().SetTimer(
		ThrowWeaponTimer, 
		this, 
		&AWeapon::StopFalling, 
		ThrowWeaponTime);
}

void AWeapon::StopFalling()
{
	bFalling = false;
	SetItemState(EItemState::EIS_Idle);
}

#pragma region Ammo
void AWeapon::DecrementAmmo()
{
	if (Ammo >= 1)
	{
		Ammo--;
	}
	else
	{
		Ammo = 0;
	}
}

void AWeapon::ReloadAmmo(int32 Amount)
{
	checkf(Ammo + Amount <= MagazineCapacity,
		TEXT("Attempted to reload with more than magazine capacity!"));
	Ammo += Amount;
}
#pragma endregion
