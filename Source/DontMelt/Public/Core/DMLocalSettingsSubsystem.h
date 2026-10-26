
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "DMLocalSettingsSubsystem.generated.h"

class UDMLocalSettingsSaveGame;

/**
 * 로컬 설정의 메모리 원본.
 * SaveGame 로드 및 저장을 담당한다.
 */
UCLASS()
class DONTMELT_API UDMLocalSettingsSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
#pragma region Lifecycle

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

#pragma endregion

#pragma region Audio

    float GetMasterVolume() const;
    float GetSFXVolume() const;
    float GetMusicVolume() const;

    void SetAudioVolumes(float InMaster, float InSFX, float InMusic);

#pragma endregion

#pragma region Voice

    // 저장된 음성 모드를 반환한다.
    bool GetVoiceActivationEnabled() const;

    // 음성 모드의 로컬 설정값을 변경한다.
    void SetVoiceActivationEnabled(bool bEnabled);

#pragma endregion

#pragma region Controls

    float GetMouseSensitivity() const;

    void SetMouseSensitivity(float InSensitivity);

#pragma endregion

#pragma region Persistence

    bool SaveSettings();

#pragma endregion

private:
#pragma region Data

    UPROPERTY(Transient)
    TObjectPtr<UDMLocalSettingsSaveGame> SettingsData;

#pragma endregion
};
