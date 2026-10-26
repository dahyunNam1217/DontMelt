# 경기 시작 후 마우스 버튼을 누르고 있어야 시점이 돌던 문제

| 항목 | 내용 |
|---|---|
| 문제 | 로비를 지나 경기에 들어오면, 마우스를 움직여도 시점이 돌지 않고 버튼을 누른 동안에만 회전했다 |
| 원인 | 로비의 `GameAndUI` 입력 모드와 커서 설정이 경기에 그대로 남아, 뷰포트가 마우스 버튼을 누를 때만 캡처했다 |
| 해결 | 일반 캐릭터 조작으로 들어가는 모든 지점에서 `GameOnly` 입력 모드와 마우스 캡처를 명시적으로 복구했다 |
| 관련 코드 | `ADMMatchPlayerController::RestoreGameplayMouseCapture` |

## 원인 추적

카메라 Look 코드를 먼저 의심하기 쉬웠지만, 증상은 "입력은 들어오는데 **누르고 있을 때만**"이었다. 이것은 Look 처리보다 **뷰포트의 마우스 캡처 정책** 쪽을 가리킨다. 그래서 메뉴 UI에서 게임플레이로 넘어가는 경계를 먼저 봤다.

| 화면 | 입력 모드 | 커서 |
|---|---|---|
| 메인 메뉴 | `FInputModeUIOnly` | 표시 |
| 로비 | `FInputModeGameAndUI` (음성 키 입력 허용) | 표시 |
| 경기 | 명시적인 설정 없음 | **로비 상태 유지** |

`GameAndUI` 모드에서는 뷰포트가 마우스를 계속 캡처하지 않고, 버튼을 누르는 동안만 캡처한다. 경기 쪽에 입력 모드를 되돌리는 코드가 없었기 때문에 로비 상태가 이어졌다.

그 밖에 Gameplay IMC 재등록 여부, 위젯에 남은 InputFocus도 함께 확인했다. 원인은 입력 모드와 커서 설정이었다.

## 해결

일반 캐릭터 조작 상태로 돌아가는 지점마다 같은 함수를 부르도록 했다.

```cpp
void ADMMatchPlayerController::RestoreGameplayMouseCapture()
{
    // 소유 클라이언트의 일반 캐릭터 조작 상태에서만 복구한다.
    if (!IsLocalController() || !IsValid(GetPawn<ADMPlayerCharacter>()) || bIngameMenuOpen || bResultInputModeActive
        || bMatchResultPresentationStarted || bLeavingMatchToTitle || IsSpectatorInputModeActive()
        || IsSpectatorInformationRestricted() || IsChestInteractionOpen() || IsSkillTargetingActive())
    {
        return;
    }

    // 경기 종료 후 결과 UI의 마우스 입력을 가로채지 않는다. (Finished / Aborted면 반환)

    // 게임플레이에서는 마우스를 뷰포트가 캡처하도록 한다.
    FInputModeGameOnly InputMode;
    InputMode.SetConsumeCaptureMouseDown(false);
    SetInputMode(InputMode);

    bShowMouseCursor = false;
    bEnableClickEvents = false;
    bEnableMouseOverEvents = false;
}
```

| 호출 위치 | 이유 |
|---|---|
| `BeginPlay` | 경기 맵 진입. Pawn이 아직 없으면 아래 `PawnClientRestart`에서 다시 호출 |
| `ADMPlayerCharacter::PawnClientRestart` | Gameplay Mapping Context 재등록 직후 |
| 인게임 메뉴 닫기 | 메뉴의 UI 모드에서 복귀 |
| 리스폰 Handoff 완료 | 관전 제한 해제 직후 |

**가드 조건이 긴 이유:** 상자 패널, 스킬 대상 선택, 관전, 결과 화면은 마우스를 UI가 써야 한다. 복구 함수가 이 상태들을 덮어쓰면 반대 방향의 버그가 생긴다. 복구는 "일반 조작 상태일 때만" 일어나야 한다.

## 검증 방법

| 시나리오 | 확인할 결과 |
|---|---|
| Standalone에서 로비 → 경기 진입 | 클릭 없이 시점 회전, 로그 `Gameplay mouse input mode restored` |
| 인게임 메뉴 열고 닫기 | 닫은 직후 바로 회전 |
| 사망 → 관전 → 리스폰 | 리스폰 후 바로 회전 |
| 상자 패널을 연 상태 | 커서 유지 (복구가 개입하지 않음) |

PIE는 에디터 창의 포커스가 섞여 결과가 달라질 수 있어 Standalone으로 확인한다.

## 정리

- "누르고 있을 때만 된다" 같은 **증상의 모양**이 원인 계층을 알려 준다. 이 경우는 입력 처리가 아니라 캡처 정책이었다.
- 맵이 바뀌어도 뷰포트 설정은 남는다. 화면마다 자신이 필요한 입력 모드를 명시적으로 설정한다.
- 복구 함수에는 **복구하면 안 되는 상태**를 함께 적어야 한다.

---

[Debugging 목차](../README.md)
