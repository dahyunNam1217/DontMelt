#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "DMRandomEventTypes.generated.h"

class USoundBase;

/**
 * Random Event 하나의 불변 정의 데이터.
 *
 * 현재 실행 상태는 저장하지 않는다.
 */
USTRUCT(BlueprintType)
struct DONTMELT_API FDMRandomEventDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Random Event")
    FGameplayTag EventTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Random Event")
    FText DisplayName;

    /**
     * 랜덤 이벤트 HUD에 표시할 안내 문구.
     * 이벤트 효과의 실제 판정에는 사용하지 않는다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Random Event|Presentation")
    FText Description;

    /**
     * Event 실제 시작 전 경고시간.
     */
    UPROPERTY(
        EditAnywhere, BlueprintReadOnly, Category = "DM|Random Event|Time", meta = (ClampMin = "0.0", UIMin = "0.0")
    )
    float WarningDuration = 3.0f;

    UPROPERTY(
        EditAnywhere, BlueprintReadOnly, Category = "DM|Random Event|Time", meta = (ClampMin = "0.0", UIMin = "0.0")
    )
    float Duration = 10.0f;

    /**
     * Event 시작/경고용 Sound.
     *
     * Runtime 강한 참조가 필요하지 않으므로 Soft Reference.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Random Event|Presentation")
    TSoftObjectPtr<USoundBase> EventSound;
};

/**
 * 현재 글로벌 Random Event의 공개 가능한 Runtime 상태.
 *
 * 실제 권위 원본은 UDMRandomEventDirector.
 */
USTRUCT(BlueprintType)
struct DONTMELT_API FDMRandomEventRuntimeState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DM|Random Event")
    FGameplayTag EventTag;

    /**
     * Event 사전 경고가 시작된 서버 시간.
     */
    UPROPERTY(BlueprintReadOnly, Category = "DM|Random Event|Time")
    double WarningStartServerTime = 0.0;

    /**
     * 실제 효과가 시작되는 서버 시간.
     */
    UPROPERTY(BlueprintReadOnly, Category = "DM|Random Event|Time")
    double StartServerTime = 0.0;

    /**
     * 실제 효과 종료 서버 시간.
     */
    UPROPERTY(BlueprintReadOnly, Category = "DM|Random Event|Time")
    double EndServerTime = 0.0;

    bool IsScheduledOrActive() const
    {
        return EventTag.IsValid() && EndServerTime > WarningStartServerTime;
    }
};