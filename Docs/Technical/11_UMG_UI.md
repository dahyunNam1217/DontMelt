# 11. UI — Root Widget과 상태 기반 갱신

## UI가 게임 판정을 하지 않는 구조

경기 UI는 `UDMGameRootWidget` 아래에 기능 위젯을 레이어별로 둔다. UI는 승패나 유물 소유권을 계산하지 않고, GameState·소유 PlayerState·캐릭터 Component가 내보낸 상태를 읽기만 한다. 상태가 바뀌면 Delegate·RepNotify로 갱신하고, 시간이 흐르는 표시만 짧은 타이머로 갱신한다.

```text
WBP_DMGameRoot
├─ GameplayLayer        HUD (전투·스킬·유물·상태), 상호작용 안내
├─ ActivityLayer        퀘스트·이벤트·자세
├─ ModalLayer           상자·스킬 대상 선택 / 공통 Dim
├─ AnnouncementLayer    알림·유물 공개·연장전 소개
├─ SpectatorLayer       관전 HUD
└─ MenuLayer            인게임 메뉴·경기 결과
```

| 위젯 | 표시 | 데이터 원본 |
|---|---|---|
| `UDMGameplayHUDWidget` | Phase·남은 시간 | `ADMMatchGameState` |
| `UDMCombatHUDWidget` | 공격·방어 쿨다운 | `UDMCombatComponent` |
| `UDMRelicHUDWidget`, `UDMChestPanelWidget` | 유물 운반·상자 조작 | `UDMInteractionComponent`, 서버 상자 스냅샷 |
| `UDMSkillBarWidget`, `UDMSkillTargetPanelWidget` | 스킬·대상 선택 | `UDMSkillComponent`, Controller |
| `UDMQuestHUDWidget`, `UDMRandomEventHUDWidget` | 퀘스트·이벤트 | `ADMMatchGameState`, `UDMQuestProgressComponent` |
| `UDMSpectatorHUDWidget` | 관전 진행 | 소유 Controller |
| `UDMMatchResultWidget` | 결과·재경기 | `FDMMatchResult`, 리매치 Ready |
| `UDMVoiceStatusWidget`, `UDMSettingsWidget` | 음성 상태·설정 | `UDMVoiceChatSubsystem`, 로컬 설정 |

## 수명 관리

- `ADMMatchPlayerController`가 로컬 `WBP_DMGameRoot`를 한 번 만들고 정리한다.
- Pawn이나 PlayerState가 바뀌면 이전 Delegate를 해제하고 새 데이터 원본으로 다시 연결한다. 리스폰 뒤 이전 Pawn을 잡고 있지 않게 한다.
- 모달(상자·대상 선택), 메뉴, 관전, 결과는 레이어 가시성으로 제어한다. 결과 화면은 다른 모든 경기 UI보다 우선한다.
- 보이지 않는 위젯이 마우스 입력을 가로채지 않도록 `Visibility`와 Hit Test를 구분한다.
- 플레이 중에는 상대 닉네임·진영·유물 소지 정보를 붙이지 않는다. 결과 화면에서만 이름을 공개한다.

## Tick 대신 필요한 동안만 도는 타이머

`UDMGameRootWidget`은 상호작용 안내 후보를 0.2초 간격으로 갱신한다. 주변 유물·상자 후보는 플레이어가 움직일 때마다 바뀌는 값이라 이벤트로만 처리하기 어렵다. 나머지 값은 시스템 Delegate로 갱신하고, 쿨다운처럼 시간이 흐르는 표시는 쿨다운 중일 때만 타이머를 켠다. 위젯별 주기는 [비용 관리 설계](../Performance/02_Cost_Design.md#ui-갱신)에 정리했다.

설정창의 변경 감지와 닫기 흐름은 [설정창 변경값 유실 문제](../Debugging/UI/01_Settings_Unsaved_Changes.md)에서 다뤘다.

## 관련 코드

- [DMGameRootWidget.cpp](../../Source/DontMelt/Private/UI/DMGameRootWidget.cpp) — `NativeConstruct`, `RefreshRootVisibility`, `RefreshInteractionPrompt`
- [DMMatchResultWidget.cpp](../../Source/DontMelt/Private/UI/DMMatchResultWidget.cpp)
- [DMSettingsWidget.cpp](../../Source/DontMelt/Private/UI/DMSettingsWidget.cpp)

## 핵심 코드

```cpp
if (!TimerManager.IsTimerActive(InteractionPromptTimerHandle))
{
    TimerManager.SetTimer(
        InteractionPromptTimerHandle,
        this,
        &ThisClass::RefreshInteractionPrompt,
        InteractionPromptRefreshInterval,   // 0.2초
        true
    );
}
```

UI 화면 구성과 위젯별 데이터 계약은 [UI 기획서](../Design/UIDesign.md)에 있다.

---

[Technical 목차](README.md)
