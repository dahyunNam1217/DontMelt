# 05. 입력과 이동 — Mapping Context 전환과 Sprint 예측

## 입력 Context를 나눈 이유

상자를 연 상태에서 같은 `F`가 다시 상자를 여닫거나, 스킬 대상 선택 중 이동·공격 입력이 섞이지 않도록 Mapping Context를 상황별로 나눴다. `ADMPlayerCharacter`가 로컬 Context의 진입과 해제를 직접 관리한다.

| 입력 상태 | 담당 함수 | 받는 입력 |
|---|---|---|
| 일반 | `PawnClientRestart`, `BindMovementInput`, `BindCombatInput` | 이동·전투·유물·스킬·메뉴 |
| 상자 패널 | `EnterChestInteractionInputMode`, `ExitChestInteractionInputMode` | 등록·탈취, 닫기 |
| 스킬 대상 선택 | `EnterSkillTargetingInputMode`, `ExitSkillTargetingInputMode` | 랜덤·선두·취소 |
| 자세 | `UDMPoseComponent`, `SetPoseInputMode` | 자세 순환·이동·점프 |
| 사망 후 관전 | `UDMSpectatorPresentationComponent::EnterSpectatorInputMode` | Pawn 입력 해제, 관전 조작 |

Context를 바꿀 때 `bIgnoreAllPressedKeysUntilRelease = true`를 쓴다. 상자를 여는 `F`가 새 Context의 닫기 `F`로 바로 인식되는 문제를 막는다. 일반 조작으로 돌아오는 지점마다 `GameOnly` 입력 모드와 마우스 캡처도 다시 맞춘다([마우스 캡처 문제](../Debugging/UI/02_InputMode_Restore.md)).

## Sprint를 이동 예측에 넣기

달리기 전환을 별도 Reliable RPC로 보내고 bool을 복제하는 대신, CharacterMovement의 예측 경로에 넣었다.

- `UDMCharacterMovementComponent`가 `FSavedMove_Character`를 상속한 `FSavedMove_DMCharacter`에 `bSavedWantsToSprint`를 저장한다.
- `GetCompressedFlags()`에서 `FLAG_Custom_0`으로 압축해 이동 패킷에 실어 보내고, 서버와 재생 시 `UpdateFromCompressedFlags()`로 복원한다.
- `CanCombineWith()`는 Sprint 의도가 다른 Move의 병합을 막는다. 입력이 바뀐 프레임을 잘못 합치지 않기 위해서다.
- `GetMaxSpeed()`가 Stunned, 달리기 가능 여부, 자세 조건을 검사해 실제 속도를 정한다.

서버 보정(Correction)이 일어나도 Sprint 의도가 SavedMove와 함께 재생되므로, 클라이언트 예측과 서버 판정이 같은 입력으로 계산된다. 다른 클라이언트의 달리기 애니메이션은 관측 속도로 판단한다.

## 관련 코드

- [DMPlayerCharacter.cpp](../../Source/DontMelt/Private/Characters/DMPlayerCharacter.cpp) — `PawnClientRestart`, `EnterChestInteractionInputMode`, `EnterSkillTargetingInputMode`
- [DMCharacterMovementComponent.cpp](../../Source/DontMelt/Private/Characters/Components/DMCharacterMovementComponent.cpp) — `GetCompressedFlags`, `CanCombineWith`, `UpdateFromCompressedFlags`, `GetMaxSpeed`
- [DMPoseComponent.cpp](../../Source/DontMelt/Private/Characters/Components/DMPoseComponent.cpp) — `RequestSetPoseModeActive_Implementation`

## 핵심 코드

```cpp
virtual uint8 GetCompressedFlags() const override
{
    uint8 Result = Super::GetCompressedFlags();

    if (bSavedWantsToSprint)
    {
        Result |= FSavedMove_Character::FLAG_Custom_0;
    }

    return Result;
}
```

입력 설계 전체(키 배치, 자세·상자·스킬 Context)는 [Enhanced Input 설계](../Design/EnhancedInput.md)에 있다.

---

[Technical 목차](README.md)
