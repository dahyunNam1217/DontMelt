
#include "Core/DMLocalSettingsSubsystem.h"

#include "Core/DMLocalSettingsSaveGame.h"

#include "Kismet/GameplayStatics.h"

namespace
{
    const FString LocalSettingsSlot = TEXT("DM_LocalSettings");

    constexpr int32 LocalSettingsUserIndex = 0;
}

#pragma region Lifecycle

void UDMLocalSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (UGameplayStatics::DoesSaveGameExist(LocalSettingsSlot, LocalSettingsUserIndex))
    {
        SettingsData =
            Cast<UDMLocalSettingsSaveGame>(UGameplayStatics::LoadGameFromSlot(LocalSettingsSlot, LocalSettingsUserIndex)
            );
    }

    if (!IsValid(SettingsData.Get()))
    {
        SettingsData = Cast<UDMLocalSettingsSaveGame>(
            UGameplayStatics::CreateSaveGameObject(UDMLocalSettingsSaveGame::StaticClass())
        );
    }
}

#pragma endregion

#pragma region Audio

float UDMLocalSettingsSubsystem::GetMasterVolume() const
{
    return IsValid(SettingsData.Get()) ? SettingsData->MasterVolume : 1.0f;
}

float UDMLocalSettingsSubsystem::GetSFXVolume() const
{
    return IsValid(SettingsData.Get()) ? SettingsData->SFXVolume : 1.0f;
}

float UDMLocalSettingsSubsystem::GetMusicVolume() const
{
    return IsValid(SettingsData.Get()) ? SettingsData->MusicVolume : 1.0f;
}

void UDMLocalSettingsSubsystem::SetAudioVolumes(float InMaster, float InSFX, float InMusic)
{
    if (!IsValid(SettingsData.Get()))
    {
        return;
    }

    SettingsData->MasterVolume = FMath::Clamp(InMaster, 0.0f, 1.0f);

    SettingsData->SFXVolume = FMath::Clamp(InSFX, 0.0f, 1.0f);

    SettingsData->MusicVolume = FMath::Clamp(InMusic, 0.0f, 1.0f);
}

#pragma endregion

#pragma region Voice

bool UDMLocalSettingsSubsystem::GetVoiceActivationEnabled() const
{
    return IsValid(SettingsData.Get()) ? SettingsData->bVoiceActivationEnabled : false;
}

void UDMLocalSettingsSubsystem::SetVoiceActivationEnabled(const bool bEnabled)
{
    if (!IsValid(SettingsData.Get()))
    {
        return;
    }

    SettingsData->bVoiceActivationEnabled = bEnabled;
}

#pragma endregion

#pragma region Controls

float UDMLocalSettingsSubsystem::GetMouseSensitivity() const
{
    return IsValid(SettingsData.Get()) ? SettingsData->MouseSensitivity : 1.0f;
}

void UDMLocalSettingsSubsystem::SetMouseSensitivity(float InSensitivity)
{
    if (!IsValid(SettingsData.Get()))
    {
        return;
    }

    SettingsData->MouseSensitivity = FMath::Clamp(InSensitivity, 0.1f, 3.0f);
}

#pragma endregion

#pragma region Persistence

bool UDMLocalSettingsSubsystem::SaveSettings()
{
    if (!IsValid(SettingsData.Get()))
    {
        return false;
    }

    return UGameplayStatics::SaveGameToSlot(SettingsData.Get(), LocalSettingsSlot, LocalSettingsUserIndex);
}

#pragma endregion
