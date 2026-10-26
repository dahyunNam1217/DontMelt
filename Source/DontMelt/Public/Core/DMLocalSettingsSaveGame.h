
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"

#include "DMLocalSettingsSaveGame.generated.h"

/**
 * 로컬 사용자 설정의 디스크 저장 데이터.
 * 서버 복제 대상이 아니다.
 */
UCLASS()
class DONTMELT_API UDMLocalSettingsSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
#pragma region Audio

    UPROPERTY()
    float MasterVolume = 1.0f;

    UPROPERTY()
    float SFXVolume = 1.0f;

    UPROPERTY()
    float MusicVolume = 1.0f;

#pragma endregion

#pragma region Voice

    // false: PTT, true: 음성 감지
    UPROPERTY()
    bool bVoiceActivationEnabled = false;

#pragma endregion

#pragma region Controls

    UPROPERTY()
    float MouseSensitivity = 1.0f;

#pragma endregion
};
