// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/DMJellyCharacterBase.h"

#include "Engine/DataTable.h"
#include "DMNPCCharacter.generated.h"

/**
 * NPC 젤리 인간 Character.
 *
 * 플레이어 전용 Combat, Interaction, Pose Component를 소유하지 않는다.
 */

UCLASS()
class DONTMELT_API ADMNPCCharacter : public ADMJellyCharacterBase
{
    GENERATED_BODY()

public:
    ADMNPCCharacter(const FObjectInitializer& ObjectInitializer);

#pragma region AI Configuration Query

    const FDataTableRowHandle& GetBehaviorProfileHandle() const
    {
        return BehaviorProfile;
    }

#pragma endregion

    
#pragma region NPC Alert Presentation

    /**
     * 서버가 승인한 NPC 놀람 연출을 관련 클라이언트에 전달한다.
     *
     * Gameplay 상태를 변경하지 않는 일회성 시각 연출이다.
     * 서버의 AIController에서만 호출한다.
     */
    UFUNCTION(NetMulticast, Unreliable)
    void MulticastPlayAlertEffect();

#pragma endregion


private:
#pragma region AI Configuration Data

    /**
     * 이 NPC Archetype이 사용할
     * Behavior DataTable Row.
     *
     * 런타임 행동 상태가 아니라
     * AI 정의 데이터 연결 정보다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DM|AI|Behavior", meta = (AllowPrivateAccess = "true"))
    FDataTableRowHandle BehaviorProfile;

#pragma endregion
};