#pragma once

#include "CoreMinimal.h"
#include "Core/DMMatchConstants.h"

class ADMMatchPlayerState;
class AGameStateBase;
class APlayerController;
class UObject;
class UEnhancedInputLocalPlayerSubsystem;

/** 4인 이하 매치에서 힙 할당 없이 사용하는 PlayerState 목록. */
using FDMMatchPlayerStateList = TArray<ADMMatchPlayerState*, TInlineAllocator<DMMatch::MaxPlayers>>;

/**
 * 여러 클래스에서 반복되던 조회 로직을 모은 공통 함수.
 */
namespace DMGameplay
{
    /**
     * 동기화된 서버 World 시간을 반환한다.
     *
     * GameState가 아직 없는 초기화 구간에서는 로컬 World 시간을, World가 없으면 0을 반환한다.
     */
    double GetServerTimeSeconds(const UObject* WorldContextObject);

    /** GameState의 PlayerArray 중 유효한 ADMMatchPlayerState만 순서대로 반환한다. */
    FDMMatchPlayerStateList GetMatchPlayerStates(const AGameStateBase* GameState);

    /** 로컬 PlayerController의 Enhanced Input Subsystem을 반환한다. 원격 Controller면 nullptr이다. */
    UEnhancedInputLocalPlayerSubsystem* GetLocalInputSubsystem(const APlayerController* PlayerController);
}
