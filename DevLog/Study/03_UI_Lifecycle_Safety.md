# UI·라이프사이클·안전성

## Root Widget 레이어와 입력 모드

HUD, 상자 패널, 결과창, 메뉴를 각자 `AddToViewport`하면 ZOrder와 포커스, 닫는 순서가 서로 얽힌다. `UDMGameRootWidget` 하나를 띄우고 그 안에 Gameplay·Activity·Modal·Announcement·Spectator·Menu 레이어를 두어 화면 우선순위를 한곳에서 정했다.

입력 모드(`GameOnly`·`UIOnly`·`GameAndUI`)는 포커스와 마우스만 바꾼다. 행동 허용 여부는 여전히 서버가 판단한다.

입력 모드는 열 때보다 **돌아올 때**가 문제였다. 로비의 `GameAndUI` 설정이 경기까지 남아, 마우스 버튼을 누르고 있어야 시점이 돌았다. 일반 조작으로 들어가는 모든 지점에서 `RestoreGameplayMouseCapture`로 입력 모드와 마우스 캡처를 명시적으로 복구해 해결했다([입력 모드 복구](../../Docs/Debugging/UI/02_InputMode_Restore.md)).

소스: [DMGameRootWidget.cpp](../../Source/DontMelt/Private/UI/DMGameRootWidget.cpp) · [DMMatchPlayerController.cpp](../../Source/DontMelt/Private/Framework/Match/DMMatchPlayerController.cpp)

## Widget과 데이터 소스의 수명

위젯이 만들어진 뒤에도 PlayerState·Pawn은 바뀐다. 사망과 리스폰으로 Pawn이 교체되고, GameState·PlayerState는 클라이언트에 늦게 도착한다.

그래서 Root Widget은 Controller·GameState·PlayerState·Pawn을 `TWeakObjectPtr`로 들고, `RebindDataSources`에서 비교한다. 넷 중 하나라도 바뀌었으면 `ShutdownDataSources`로 이전 구독을 먼저 해제한 뒤 다시 연결하고, 같으면 하위 위젯에 데이터 준비 여부만 다시 확인시킨다. 같은 Delegate를 두 번 등록하면 갱신도 두 번 일어나기 때문이다. `NativeDestruct`에서도 같은 해제 함수를 부른다.

늦게 오는 데이터는 재시도로 기다린다. 로비 플레이어 목록이 `0 / ?`에서 멈췄던 문제는, GameState 탐색 재시도가 포기한 뒤 PlayerState가 도착해도 다시 시도하지 않았기 때문이었다([로비 목록 바인딩](../../Docs/Debugging/Network/09_Lobby_PlayerList_Binding.md)).

소스: [DMGameRootWidget.cpp](../../Source/DontMelt/Private/UI/DMGameRootWidget.cpp) · [DMLobbyWidget.cpp](../../Source/DontMelt/Private/UI/DMLobbyWidget.cpp)

## 동적 Row 목록

로비 플레이어 목록과 결과표는 항목 수가 바뀌므로 `UVerticalBox`에 Row 위젯을 만들어 넣는다. 갱신은 항상 `기존 Row 제거 → 정렬된 데이터 순회 → Row 생성·값 설정` 순서다.

로비 목록은 위젯에서 정렬한 뒤 그리고, 결과표는 서버가 확정한 순위 순서를 그대로 쓴다. Row 생성에 실패하면 Error 로그를 남겨, 빈 목록이 "데이터 없음"인지 "Row 클래스 누락"인지 구분할 수 있게 했다.

소스: [DMLobbyWidget.cpp](../../Source/DontMelt/Private/UI/DMLobbyWidget.cpp) · [DMMatchResultWidget.cpp](../../Source/DontMelt/Private/UI/DMMatchResultWidget.cpp)

## ViewTarget과 관전 Pawn

`SetViewTargetWithBlend()`는 카메라만 옮긴다. Possess, 입력 Context, 마우스 캡처는 따로 정리해야 한다.

사망 후 30초 관전에서 10~25초 구간은 `ADMRestrictedSpectatorPawn`을 쓴다. 이 Pawn은 로컬 전용이고 Possess하지 않으며, 복제하지 않고 서버 권한 데이터도 갖지 않는다. PlayerController가 입력을 월드 방향으로 바꿔 넘기면, 벽·바닥과 Sweep 충돌하며 움직이는 카메라 역할만 한다.

관전 중 행동 제한은 카메라가 아니라 서버가 정한다. 사망한 플레이어의 공격·상호작용 요청은 LifeState에서 거절된다. 리스폰 후 화면 복귀도 클라이언트 타이머가 아니라 서버의 리스폰 성공을 기준으로 했다([Respawn Handoff](../../Docs/Debugging/Gameplay/08_Respawn_Handoff.md)).

소스: [DMRestrictedSpectatorPawn.cpp](../../Source/DontMelt/Private/Spectator/DMRestrictedSpectatorPawn.cpp) · [DMSpectatorPresentationComponent.cpp](../../Source/DontMelt/Private/Spectator/DMSpectatorPresentationComponent.cpp)

## 로컬 설정과 SaveGame

설정값은 세 가지로 나눈다. 화면에 보이는 입력값, 지금 시스템에 적용된 값, 다음 실행에도 남는 저장값이다. 이걸 섞으면 "적용 안 하고 닫았는데 바뀌어 있다"는 문제가 생긴다.

`UDMLocalSettingsSubsystem`이 저장값(`UDMLocalSettingsSaveGame`)을 읽어 범위를 제한하고 음성·사운드에 적용한다. 설정창은 저장값 스냅샷과 현재 입력값을 비교해 변경 여부를 계산하고, 닫기 경로를 요청 → 확인 → 완료 하나로 모았다([설정창 변경값](../../Docs/Debugging/UI/01_Settings_Unsaved_Changes.md)).

소스: [DMLocalSettingsSubsystem.cpp](../../Source/DontMelt/Private/Core/DMLocalSettingsSubsystem.cpp) · [DMSettingsWidget.cpp](../../Source/DontMelt/Private/UI/DMSettingsWidget.cpp)

## GameInstanceSubsystem

맵이 바뀌어도 유지돼야 하는 온라인 세션, 로컬 설정, 음성은 `UGameInstanceSubsystem`으로 만들었다. `Initialize()`에서 Delegate를 연결하고 `Deinitialize()`에서 해제한다.

Subsystem이 오래 산다고 월드 객체를 붙잡아도 되는 것은 아니다. 음성 Subsystem은 Controller와 원격 PlayerState를 `TWeakObjectPtr`로만 참조해, 맵 이동 뒤 이전 월드의 객체가 남지 않게 했다.

소스: [DMOnlineSessionSubsystem.cpp](../../Source/DontMelt/Private/Online/DMOnlineSessionSubsystem.cpp) · [DMVoiceChatSubsystem.cpp](../../Source/DontMelt/Private/Voice/DMVoiceChatSubsystem.cpp)

## TGuardValue로 재진입 막기

`TGuardValue`는 스코프 동안 값을 바꿔 두고, 스코프가 끝나면 이전 값으로 되돌린다. 중간에 `return`해도 복구된다.

| 위치 | 막는 것 |
|---|---|
| `UDMSettingsWidget` | 코드로 컨트롤 값을 채우는 동안 발생한 변경 이벤트가 "플레이어가 바꿨다"로 처리되는 것 |
| `UDMQuestDirector` | 완료 순위를 확정하는 동안 들어온 진행 갱신이 순위 계산을 다시 건드리는 것 |

```cpp
// 코드로 UI를 채우는 동안 발생하는 변경 이벤트는 무시한다.
TGuardValue<bool> PopulateGuard(bIsPopulatingControls, true);
```

소스: [DMSettingsWidget.cpp](../../Source/DontMelt/Private/UI/DMSettingsWidget.cpp) · [DMQuestDirector.cpp](../../Source/DontMelt/Private/Quests/DMQuestDirector.cpp)

## 비동기 콜백과 TWeakObjectPtr

세션 완료 콜백, 타이머, UI 이벤트는 요청한 시점과 실행되는 시점이 다르다. 그 사이 Controller나 위젯이 사라질 수 있으므로 `TWeakObjectPtr`로 참조하고, 쓰기 직전에 유효성을 검사한다.

포인터가 유효하다고 해서 요청이 아직 유효한 것은 아니다. 세션 Subsystem은 진행 중인 작업 종류(`CurrentOperation`)를 기록해 두고, 작업 종류나 세션 이름이 맞지 않는 완료 콜백은 무시한다.

소스: [DMOnlineSessionSubsystem.cpp](../../Source/DontMelt/Private/Online/DMOnlineSessionSubsystem.cpp) · [DMGameRootWidget.cpp](../../Source/DontMelt/Private/UI/DMGameRootWidget.cpp)

[DevLog 목차](../README.md)
