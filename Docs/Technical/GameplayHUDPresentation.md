# 경기 HUD — 데이터 연결과 표시 수명

경기 HUD는 서버 상태와 로컬 입력 상태를 화면에 투영한다. `UDMGameRootWidget`이 자식 위젯에 데이터 소스를 전달하고, 각 위젯은 자기 표시와 구독 수명을 관리한다.

## 생성과 데이터 연결

로컬 `ADMMatchPlayerController`가 설정된 `GameRootWidgetClass`로 Root를 생성해 뷰포트에 붙인다. Root의 `RebindDataSources`는 Controller, GameState, PlayerState, 현재 Pawn을 받는다. 원본이 바뀌면 이전 연결을 먼저 끊는다. 객체가 같아도 스킬·퀘스트 데이터나 Pawn 컴포넌트가 나중에 준비될 수 있어 필요한 연결을 다시 확인한다.

GameplayHUD는 자식 CombatHUD에 현재 Pawn의 전투 컴포넌트를, SkillBar에 PlayerState를 전달한다. 미니맵에는 PlayerState와 Pawn을 넘긴다. Root는 QuestHUD에 GameState와 PlayerState를 연결하고, StatusHUD에 Pawn의 상태 컴포넌트와 로컬 Controller를 연결한다. [Controller 생성과 연결](../../Source/DontMelt/Private/Framework/Match/DMMatchPlayerController.cpp) · [Root 연결](../../Source/DontMelt/Private/UI/DMGameRootWidget.cpp) · [GameplayHUD 하위 연결](../../Source/DontMelt/Private/UI/DMGameplayHUDWidget.cpp)

## 표시 조건과 갱신

`RefreshRootVisibility`는 Controller의 현재 Pawn이 바인딩된 Pawn과 일치하고 플레이어가 살아 있으며, 경기 종료 또는 관전 정보 제한 상태가 아닐 때 Gameplay 레이어를 활성화한다. 각 보조 패널에는 Phase와 로컬 UI 상태에 맞는 조건을 추가한다.

| 위젯 | 원본과 갱신 경로 |
|---|---|
| 미니맵 | `Playing`에서 자기 Pawn과 자신의 활성 BaseZone을 표시한다. 월드 위치를 설정된 범위와 Canvas 좌표로 변환한다. 매핑 또는 Pawn이 유효하지 않으면 숨기고 갱신을 중지한다. 표시 중에는 로컬 위치 갱신 타이머를 사용한다. |
| 조작 도움말 | `Playing` 또는 `Overtime`의 Gameplay 표시 상태에서 메뉴·상자·스킬 타기팅이 닫혀 있어야 한다. Action 시작은 상세 표시 요청을 세우고 Completed·Canceled에서 해제한다. 키 안내 문구와 실제 IMC 매핑은 에디터에서 대조한다. |
| QuestHUD | 공개 퀘스트와 개인 진행도를 읽는다. 제시간 완료와 시간 초과 후 완료를 구분하고, RunId로 완료 표시와 퇴장 애니메이션의 수명을 다룬다. |
| StatusHUD | 캐릭터 상태 타이밍과 Controller에 도착한 개인 효과 알림을 표시한다. 서버 시간에서 남은 시간을 계산하며 효과 표시가 필요할 때 갱신한다. |
| SkillBar | PlayerState의 스킬 데이터와 쿨다운을 읽는다. 슬롯 4와 5의 태그 연결은 각각 크기 증폭과 이동 둔화다. 활성 표시 및 진행 중인 쿨다운에 맞춰 갱신한다. |
| CombatHUD | 현재 Pawn의 CombatComponent에서 공격·방어의 쿨다운 변경 통지를 받고 남은 시간과 전체 시간을 조회한다. 숫자와 진행률을 표시하며 숨김 또는 표시할 쿨다운이 없는 상태에서 표시 타이머를 정리한다. |

[미니맵](../../Source/DontMelt/Private/UI/DMMinimapWidget.cpp) · [도움말](../../Source/DontMelt/Private/UI/DMControlsHelpWidget.cpp) · [퀘스트](../../Source/DontMelt/Private/UI/DMQuestHUDWidget.cpp) · [상태](../../Source/DontMelt/Private/UI/DMStatusHUDWidget.cpp) · [스킬 슬롯](../../Source/DontMelt/Private/UI/DMSkillBarWidget.cpp) · [전투](../../Source/DontMelt/Private/UI/DMCombatHUDWidget.cpp)

## 종료와 다시 연결

Pawn 교체·관전·종료에서 Root는 표시 조건을 다시 계산한다. 이전 소스 구독은 `ShutdownDataSources`에서 제거하며 Root 파괴 시에도 이 경로를 사용한다. GameplayHUD 파괴 시 CombatHUD와 SkillBar의 표시를 끄고 데이터 소스를 비운다. 미니맵은 비활성화와 파괴에서 갱신 타이머를 해제한다. QuestHUD는 종료 대상 RunId를 확인해 이전 실행의 퇴장 처리와 새 실행의 표시를 분리한다.

## 확인 범위

C++에 선언된 데이터 연결, 가시 조건, Delegate와 타이머 정리를 확인했다. WBP 레이아웃과 실제 바인딩·애니메이션, 활성 IMC는 에디터 확인 항목이다. Host와 Client에서 Pawn 교체, 관전·리스폰, 연장전 진입, 상자나 스킬 선택 중 도움말 해제, 새 퀘스트 진입과 기존 퇴장의 경합을 재현해야 한다. [UMG 구조 개요](11_UMG_UI.md) · [입력 설계](../Design/EnhancedInput.md) · [회귀 시나리오](../Verification/03_Regression_Scenarios.md)
