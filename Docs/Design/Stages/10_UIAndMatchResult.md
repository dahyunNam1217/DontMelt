# 10 — UI·결과·설정 통합

## 1. UI Root

`UDMGameRootWidget`이 경기 UI의 진입점이다. `ADMMatchPlayerController`가 한 번 만들고 Gameplay·Activity·Modal·Announcement·Spectator·Menu 레이어를 제어한다. GameState, PlayerState, 스킬·전투·유물·퀘스트 상태는 표시용으로만 전달되고, Widget은 원본 데이터를 소유하지 않는다.

상시 HUD는 Phase·남은 시간·전투·스킬·유물·상태 효과를 보여 준다. 상자와 대상 선택 모달은 같은 Dim 배경을 쓴다. 사망 중에는 일반 HUD를 닫고 관전 HUD로 바꾼다. 경기 결과는 다른 모든 경기 UI보다 우선한다.

## 2. 이 단계에서 정리한 UI

| 영역 | 최종 형태 |
|---|---|
| SkillSlot | 키, 쿨다운 텍스트, ProgressBar 중심. 이름·설명·상태 텍스트 제거 |
| StatusHUD | 스턴·보호·투명·시야 교란·발소리 관찰 5개 상태 |
| RandomEventHUD | 이벤트 이름·설명·ProgressBar |
| ChestPanel | F로 열고 닫기, Space로 진행·해제. 진행 상태는 한 곳에만 표시 |
| RelicHUD | 운반 상태와 Q 드롭 안내. 상자 패널과 같은 값을 따로 갱신하지 않음 |
| Quest | 성공 보상 제거, 마감 지난 미완료 캐릭터 주변 3D 사이렌 |
| Overtime·Reveal | 종료 전 유물 위치 공개, 연장전 소개 |

C++ 위젯은 `BindWidget`으로 WBP와 연결한다. 필수 바인딩은 WBP 컴파일·Cook 단계에서 검사되므로, C++에 필드를 추가할 때 WBP도 함께 맞춘다.

## 3. 경기 결과

결과는 서버가 확정해 GameState의 `FDMMatchResult`로 복제한다. 정상 결과는 이름·등수·유물 수·킬 수 4열이고, 승자 없음·중단 같은 특수 사유는 `TXT_ResultDetail`로 표시한다.

| 항목 | 규칙 |
|---|---|
| 등수 | 종료 시 등록 유물 수 기준, 같으면 공동 순위. 전멸 승리일 때만 승자 1위 고정 |
| 유물 열 | 경기 중 누적 획득 수 |
| 결과 전달 | ResultId별 캐시 후 ACK, 서버는 전원 확인(최대 5초) 뒤 이동 |
| 자동 복귀 | 결과 화면 30초 후 로비 |
| 버튼 | Host는 로비 복귀·재시작, Client는 재경기 준비. 일반 Client가 서버 전체를 이동시키지 않음 |

```text
Server: 결과 확정 / ResultId 발급
  → GameState: MatchResult RepNotify
  → 소유 Client: GameInstance 결과 캐시, HUD 갱신
  → Controller: ResultId 수신 ACK
  → Host 요청 / 재경기 준비 / 30초 자동 복귀
  → 서버 검증 후 Travel
```

이 단계의 정적 검수에서 결과표의 유물 열·등수 기준 불일치, 동률 시 등수 공란, ACK 호출 누락을 찾았다. 세 가지 모두 이후 수정했다([결과표 데이터 분리](../../Refactoring/03_MatchResult_Data.md), [결과 ACK](../../Debugging/Network/07_MatchResult_Travel_Race.md)).

유물 위치 공개(정규 종료 60초 전, 3초)와 연장전 소개(5초)는 서버가 허용하는 시점에만 GameState에 공개 데이터를 만들고, 끝나면 바로 비운다.

## 4. 설정

같은 Settings 위젯을 메인 메뉴·로비·인게임 메뉴에서 쓴다. 탭은 Graphics / Audio / Controls / Language / Voice다. 저장값과 현재 입력값을 비교해 변경 여부를 계산하고, 변경이 있으면 탭 이동·닫기 전에 적용·변경 취소·계속 수정을 고르게 한다. 화면 모드를 바꾸고 15초 안에 확인하지 않으면 이전 설정으로 되돌린다.

`UGameUserSettings`가 그래픽 설정을, `UDMLocalSettingsSubsystem`과 SaveGame이 음량·마우스 감도를 소유한다. 서버는 개인 설정을 복제하지 않는다.

## 5. 연결 실패

`UDMGameInstance`가 의도한 이동과 Network·Travel 실패를 구분한다. 호스트 강제 종료·참가 실패·맵 이동 실패는 `UDMConnectionLostWidget`으로 안내한 뒤 메인 메뉴로 돌아간다. 중복 복구를 막고, 메인 메뉴에서 입력 포커스를 다시 맞춘다. 호스트 이전은 지원하지 않는다.

## 6. 이 단계에서 나온 문제

| 증상 | 처리 |
|---|---|
| 로비가 `0 / ?`, `로비 정보 동기화 중...`에서 멈춤 | GameState 바인딩 재시도 소진 → [로비 목록 문제](../../Debugging/Network/09_Lobby_PlayerList_Binding.md) |
| 경기 시작 후 마우스를 누른 채로만 시점 회전 | 로비 입력 모드 잔존 → [입력 모드 복구](../../Debugging/UI/02_InputMode_Restore.md) |
| 설정창 변경값 유실 | 요청·승인 분리 → [설정창 문제](../../Debugging/UI/01_Settings_Unsaved_Changes.md) |

관련 문서: [UI 기획서](../UIDesign.md) · [UI 구현](../../Technical/11_UMG_UI.md)

[Stages 목차](README.md)
