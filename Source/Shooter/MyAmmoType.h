#pragma once

UENUM(BlueprintType)
enum class EMyAmmoType : uint8
{
	EAT_9mm UMETA(DisplayName = "9mm"),
	EAT_AR UMETA(DisplayName = "AssaultRifle"),

	EAT_NAX UMETA(DisplayName = "DefaultMAX")
};
