# 11. UMG·상태 HUD·결과 화면·입력 포커스

## UI 책임

`ADMMatchPlayerController`가 소유 클라이언트에 `WBP_DMGameRoot` 한 번 생성/정리한다. `UDMGameRootWidget` 및 자식 `UDM*Widget` C++ 클래스는 GameState/PlayerState/Component/Local Subsystem 이벤트의 **표현만** 담당. UI에서 유물 등록 수·승패·기절 판정을 다시 하지 않는다.

## 레이어

```text
WBP_DMGameRoot
├─ GameplayLayer: GameplayHUD(Combat/Skill/Relic/Status), InteractionPrompt, VoiceStatus
├─ ActivityLayer: Quest/RandomEvent/Pose
├─ ModalLayer: Chest/SkillTargeting/Emote + Dim
├─ AnnouncementLayer: 공개 타이밍/연장전 등 알림
├─ SpectatorLayer: 사망 관전 HUD
└─ MenuLayer: 일시 메뉴 / MatchResult
```

우선순위: 결과 > 메뉴 > 모달 > 기본 HUD. Widget Visibility와 Hit Test를 구분하여 **보이지 않는 UI가 입력을 가로채지 않게** 한다. Esc는 로컬 메뉴만 열며 Listen Server 월드 전체 일시 정지 금지.

## 데이터 계약

| 위젯 | 내용 | 원본 |
|---|---|---|
| `WBP_DMGameplayHUD` | Phase·남은 서버 시간 | MatchGameState |
| `WBP_DMCombatHUD` | 공격/방어 쿨다운 수치·진행 | CombatComponent |
| `WBP_DMSkillBar`/Slot | 조작키, 아이콘(선택), 남은초, ProgressBar | SkillComponent |
| `WBP_DMRelicHUD` | 운반 상태·Q 드롭 | 소유자 운반 표시 |
| `WBP_DMInteractionPrompt` | F 상호작용 후보 안내 | 로컬 후보 검색, 서버와 별도 검증 |
| `WBP_DMChestPanel` | 열어본 상자의 등록 수·Space Hold 진행 | 해당 소유자 서버 스냅샷/Session |
| `WBP_DMQuestHUD` | 개인 목표·진행도/마감 | QuestProgress (OwnerOnly), 공개 Quest |
| `WBP_DMRandomEventHUD` | 현재 이벤트 안내 | 공개 이벤트 상태 |
| `WBP_DMStatusHUD` | Stunned, SpawnProtected, Invisible, VisionDistorted, FootstepObserved | 적합한 소유 컴포넌트 |
| `WBP_DMSpectatorHUD` | 관전 단계/카운트다운 | 로컬 SpectatorPresentation |
| `WBP_DMMatchResult` | 이름·등수·누적 획득 유물 수·Kill, 30초 자동 복귀 | 최종 서버 Result |

**문서 충돌:** 원본 `UIDesign.md`/기술 문서에는 RandomEventHUD를 이름·설명·진행바로 적었지만 이후 UI 대화에는 **설명 텍스트 하나만** 유지하려는 지시가 있었다. 현재 Widget 에셋/C++를 확인하지 못했으므로 `ImplementationStatus.md`에 검증 필요로 표시. 다른 UI들도 실제 BindWidget 구성 확인 전 임의 필드 추가하지 않는다.

## 갱신·수명

- GameState RepNotify, Component Delegate 갱신. 쿨다운 표시·상호작용 후보만 **필요할 때 타이머**(문서상 InteractionPrompt 기본 0.2초)로 갱신. Actor 전수 탐색/서버 폴링 금지.
- Pawn 교체 시 이전 Pawn/PlayerState Delegate 해제 후 새 원본 바인딩.
- ChestPanel은 F 한 번으로 열고 닫고 Space Hold/Release로 진행/취소. ChestPanel과 RelicHUD에 같은 상자 진행 상태를 중복 소유시키지 않음.
- 사망 시 관전 HUD로 전환, 결과가 오면 그 위에 결과 UI만 우선 표시.
- `BindWidget` 이름과 Widget Blueprint 실제 변수 이름이 정확히 맞아야 함. 한글/영문 혼용 폰트에서 **C++ SetText 문자열도** 글리프 확인.

## 결과 버튼·로비 복귀

Host만 전체 로비 복귀/재시작을 승인. Client는 Rematch Ready 등 소유자 동작. Result는 `ResultId` 캐시 후 ACK이며, 자동 복귀 30초. 경기 결과표 `유물` 열은 등록 수가 아닌 **누적 획득 유물 수**.

## 완료 기준

- Host와 원격 Client 같은 HUD Phase·시각, 상호작용 안내/상자/쿨다운/결과 정상.
- 모달에서 입력 Context/마우스 캡처 복귀, 위젯 배경 Hit Test로 입력 안 막힘.
- 사망/리스폰 반복·ServerTravel 후 Delegate/Timer/Widget 중복 없음.
- 결과 화면 경쟁상태, Client가 Host 권한 버튼 실행 불가, 숨겨진 상대 정보 비노출.
- `.uasset`을 열어 `BindWidget`·Designer 글꼴·부모 위젯 참조 모두 확인.

[UI 설계](../../Docs/Design/UIDesign.md) · [구현](../../Docs/Technical/11_UMG_UI.md) · [설정 입력 버그](../../Docs/Debugging/UI/02_InputMode_Restore.md)
