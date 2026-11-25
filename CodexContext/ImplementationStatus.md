# 구현 상태와 검증 근거

## P1-P9 체크포인트 구현과 검증

기준은 `a3a6e0780ecbd86535cd667d8ea93354a82e0c9e`이다. 아래 표는 C++·Config·Git에서 확인한 사실과 에디터·실행에서 확인할 항목을 구분한다. P2 승인과 P3 조사 기록은 아래 각 절의 날짜와 기준 SHA를 따른다.

| 범위 | 정적 근거 | 남은 확인 |
|---|---|---|
| 경기 시작과 결과 | `DMMatchGameMode`의 참가자 도착·준비 확인과 초기화 후 튜토리얼 게이트. P2 E3 설계 기록은 **120초**, 현재 C++ 기본값은 **90초**, 튜토리얼 기본값은 **300초**다. `DMMatchResultRules`는 전멸 승자를 단독 1위로 처리한다. | 도착 제한의 최종 설계 목표, 활성 BP 기본값, 결과 위젯 바인딩, Host/Client ACK |
| 스킬과 이동 | `7f8c93f`에서 크기 증폭·이동 둔화, 조작 도움말, 미니맵과 새 HUD 경로가 추가됐다. 둔화는 `GetMaxSpeed`에 0.5를 적용한다. 크기 증폭은 캡슐 검사와 복구 경로를 가진다. | 활성 DA의 스킬 수치·IMC 매핑, 충돌 및 이동 예측 |
| HUD와 연출 | `f587699`에 UI·Input·VFX 에셋 변경이 있다. 미니맵은 정규 진행 중 자기 Pawn과 자기 활성 BaseZone만 그린다. 정전에는 NPC 재질 페이드, 퀘스트에는 RunId별 표시·퇴장과 알림 경로가 있다. | WBP 바인딩·애니메이션, Niagara·재질의 실제 표현 및 복구 |
| 오디오 | `65630c3`에 SoundWaves와 감쇠 에셋 `ATT_DMNear`, `ATT_DMMedium`, `ATT_DMFar`가 추가됐다. C++는 월드 위치 소리, 개인 2D 알림, 공용 UI 클릭과 캐릭터 소리를 구분한다. | BP 할당, 감쇠·믹스, 청취 범위와 중복 재생 |
| 설정·P9 현지화 | **진행 중**. `DMLocalSettingsSubsystem`이 문화권을 적용하고 로컬 설정에 보관한다. `3eeb0d1`의 Gather/Compile INI, manifest, en/ko archive, CSV와 충돌 로그를 확인했다. 원본 문화권은 ko, 대상은 en·ko다. 기준 커밋에 추적된 `.locres`/`.locmeta`는 없다. | 최신 Gather 및 번역 적용, 화면 전환·글꼴 및 패키지 스테이징 |
| NPC 실험 | `13abca7`에 이동 진단, `a3a6e07`에 P3-11r A144/B176 실험 맵이 있다. 공통 C++ 이동 기본값은 200/500, 규칙 DA 클래스 기본 배율은 15다. | 맵 배치·NavMesh, 참가 수별 성능 측정 |

`Game.csv`의 영어 단어 수 0은 당시 Gather 스냅샷이다. `Game_Conflicts.txt`에는 `DMFrontend / SessionUnavailable` 키 충돌이 기록돼 있고 현재 소스는 참가 실패 분기에 `JoinSessionUnavailable`을 사용한다. 현재 Gather에서의 재현 여부는 미확인이다.

최근 체크포인트는 기능 묶음으로 보존됐다. P1~P8 번호별 기능 대응은 확인 근거가 부족하다. 이 SHA의 빌드, 에디터 표현, PIE, Steam 멀티플레이, 패키징 및 성능은 미검증 상태다.

아래 P0·P2·P3 절의 구현 확인 문장은 각 기록의 기준 시점을 따른다. 현행 결과 행과 전멸 순위는 위 체크포인트 표를 참고한다.

과거 분석: [P0 Git 기준](ImplementationStatus.md#9-현재-git-저장소-분석--c3f94a9-기준) · [P2 승인 이력](ImplementationStatus.md#10-p2-승인-설계-및-현재-구현--2026-11-13) · [P3 조사](ImplementationStatus.md#p3-9--최신-아키텍처-결정-및-구현-대조-2026-11-17).

# P3-9 — 최신 아키텍처 결정 및 구현 대조 (2026-11-17)

기준: Windows 개발 Worktree `C:\Unreal_My\Portfolio\DontMelt-P3-Work`, 브랜치 `work`, HEAD `001a08241ecd01dfa1ef565f11c7791eaf73eaeb`. **코드 정적 확인과 문서 동기화만 수행**했다. 빌드·Automation Test·PIE·Steam·에셋 에디터 검증은 미실행이다. 아래 최신 절이 P2 및 ZIP 분석 기록의 상충 내용과 구현 전 계획을 대체하며, 기존 기록은 당시 근거로 보존한다.

## 최신 결정

이하 P3-9 항목은 해당 Windows Worktree의 조사 기록이다. 현행 C++와 검증 상태는 위 체크포인트 표를 기준으로 읽는다.

- 기존 ADMJellyCharacterBase → ADMPlayerCharacter / ADMNPCCharacter 구조와 GameMode·GameState·PlayerState·PlayerController, Manager·Component 책임을 유지한다.
- P3-7 Anonymous Presentation PoC/B안은 본 프로젝트에 통합하지 않는다. 별도 실험 브랜치·Worktree를 보존한다.
- 정상 게임 화면의 위장 일관성을 유지한다. 네트워크 메타데이터 수준에서 플레이어 정체를 숨기는 것은 필수 요구사항에서 제외한다. Listen Host의 서버 원본 은폐를 보장하지 않는다.
- Server Authority / Single Source of Truth / Least Replication / Event Driven 및 비공개 게임 데이터 보호를 유지한다. 기존 공격·방어·스킬·유물·리스폰 규칙을 이 문서 작업으로 바꾸지 않는다.

## P3-1 — 결과 통계·전멸 순위

- `Private/UI/DMPlayerResultRowWidget.cpp::SetResultEntry`는 `RelicCount`와 `AcquiredRelicCount`를 각각 표시한다. `Public/UI/DMPlayerResultRowWidget.h`의 `TXT_AcquiredRelicCount`는 **BindWidget 필수**이며 기존 필수 항목과 하나라도 없으면 Row 갱신이 반환된다. WBP 변수·형식·부모 연결은 미검증이다.
- `Private/Framework/Match/DMMatchGameMode.cpp::BuildFinalPlayerResults(EndReason, WinningBaseSlot)`는 전멸 종료에만 `SortAndRankResults(..., true)`를 전달한다. 승자는 단독1위, 나머지는 등록 수 내림차순 공동 순위(예:1·2·2·4), 일반 종료는 등록 수 순위다.
- `Private/Tests/DMMatchResultRulesTests.cpp`의 `DontMelt.Match.ResultRules.EliminationRanking` 추가를 확인했다. 승자 등록 수 0/2/5, 전원0, 통계 보존, 반복 정렬, 일반 종료 비고정을 검사한다. 테스트 실행은 미실행이다.

소스 경로는 이 절의 기능 설명에서 `Source/DontMelt/`를 기준으로 한다. 누적 획득 원본은 `Private/Relic/DMRelicManager.cpp::TryCommitRelicAcquisition` → `Private/Framework/Match/DMMatchPlayerState.cpp::IncrementAcquiredRelicCount`이고, 최종 등록 수는 RelicManager 조회다. 성공 Pickup/Steal Commit마다 증가하는 현재 동작을 기록하며 재획득 정책을 새로 확정하지 않는다.

## P3-2 — RandomEvent HUD

- `Public/UI/DMRandomEventHUDWidget.h`는 `TXT_EventDescription`만 BindWidget 필수다. `TXT_EventName`·`PB_EventProgress`는 BindWidgetOptional 호환 필드이며 존재하면 Collapsed 처리한다.
- `Private/UI/DMRandomEventHUDWidget.cpp::RefreshPresentation`은 설명·예고 접두사만 표시하며 이름/진행률 갱신을 제거했다. GameState 공개 이벤트와 DataAsset 설명을 사용하고 정의 누락 시 대체 문구를 표시한다.
- 기존0.1초 로컬 Timer는 예고→진행 전환과 종료 숨김을 위해 유지한다. 비활성·만료·Destruct 시 정리 경로가 있다. 서버 이벤트 판정과 복제 계약을 변경한 작업이 아니다.
- 설명만 있는 WBP 컴파일, 경고→진행→종료, 정의 누락, Host/Client·Travel 재생성은 미검증이다.

## P3-3 — 도착·준비·필수 초기화

- `Public/Framework/Match/DMMatchGameMode.h::StartingRosterTimeoutSeconds` 기본값은 **90.0f**다. 서버 `StartPlay`에서 Waiting·종료 사유 None·미확정 명단·비활성 Timer 조건으로 설정한다. 클라이언트별 로딩 시작 시각 기준이 아니다.
- `IsStartingRosterComplete`는 기대 인원, 유효 PlayerState·고유 BaseSlot·Alive·Pawn/Controller/PlayerState 양방향 연결을 검사한다. `HandleStartingNewPlayer_Implementation`도 Spawn/Possess 및 PlayerState 연결을 확인한다.
- `HandleStartingRosterTimeout`은 단발 Timer를 정리하고 ArrivalOrRosterIncomplete / ParticipantPreparationFailed / RequiredInitializationIncomplete를 로그로 구분한 뒤 기존 **InternalError** 중단 경로를 호출한다. 미도착을 이탈로 확정하지 않는다.
- `TryFinalizeStartingRoster`는 리스폰 지점, NPC·유물·퀘스트·이벤트 초기화를 확인한 후 Timer 해제와 튜토리얼 게이트로 진행한다. 실패는 `FailStartingRosterFinalization`의 역순 정리 및 중단 경로다. 튜토리얼 기본300초는 별도다. BP 제한값·사용자 안내·실제 실패 재현은 미검증이다.

## P3-4 — Steam Lobby 복구 차단

- `Private/Framework/Lobby/DMLobbyGameMode.cpp`의 `HandleStartMatchRequest`, `TryReopenReturningLobby`, `RestoreLobbyStatus`, `HandleReturningPlayersTimeout`에서 세션 복구 차단을 확인한다.
- `HandleLobbyStatusUpdated`는 유효 구독·진행 문맥·세션 유효성·복구 차단·Busy를 검사한다. 등록 전 기존 Delegate를 해제하고 요청 직후 Handle 유효성도 확인하여 동기 완료의 중복 처리를 방지하는 코드다.
- `StopLobbyStatusRecovery`는 소유 완료 Delegate와 복구/복귀 Timer를 정리하고 `bTravelCommitted=true`로 시작 잠금을 유지한다. `RetryOrReleaseLobbyStatusRestore`라는 기존 이름과 달리 상한 소진 시 잠금을 해제하지 않는다. 상한 상수는5이며 실패 카운트가5에 도달하면 중지한다. 반환 로비 재시도5초/일반 복구1초는 현재 구현값이다.
- `ReleaseMatchStartLock` 호출은 광고 복원 성공 경로에 있다. 종료 시 `EndPlay` 정리도 유지한다. Busy/응답 대기5초 확인 분기까지 모든 반복이 동일 실패 카운트로 제한된다고 단정하지 않는다.
- 기존 OnlineSessionSubsystem 상태 머신을 유지한다. 로그의 나가기·게임/Steam 재시작 안내는 확인했으나 UI 전달과 실제 Steam 정상/실패/지연/중복 콜백은 미검증이다.

## P3-5 — NPC·이동·오디오 정책

- `Public/Characters/DMJellyCharacterBase.h` 기본값 WalkSpeed=200/SprintSpeed=500, `Private/Characters/DMJellyCharacterBase.cpp::PostInitializeComponents`의 MaxWalkSpeed 적용, `Private/Characters/Components/DMCharacterMovementComponent.cpp::GetMaxSpeed`의 Sprint 경로를 확인했다. Player/NPC는 이 공통 기반을 상속한다.
- `Public/Data/DMMatchRuleDataAsset.h::DefaultNPCMultiplier=15`, GameMode 시작 인원×배율 스냅샷, `Private/AI/DMNPCPopulationDirector.cpp::InitializePopulation`의 1~60 검증을 확인했다. 2/3/4인 목표는30/45/60명이다.
- `Private/Characters/Components/DMCharacterAudioComponent.cpp::CanPlayFootstepLocally`는 ADMPlayerCharacter만 허용한다. NPC 발소리 제거 정책은 기존 구현이며 이번 커밋이 새로 제거한 것으로 기록하지 않는다. BP 직접 재생·Sound Notify 우회는 미검증이다.
- P3 커밋에서 P3-5에 해당하는 직접 변경은 규칙 DataAsset 헤더 주석의 4×10=40 → 4×15=60 정정이다. 이동·배율·발소리 동작은 이전부터 존재하던 코드를 재확인한 것이다. 에셋 기본값·실제 속도·수량·음향·60NPC 성능은 미검증이다.

## 남은 불일치와 P3-10 전 확인 항목

- 아래 P2의120초·결과 Row 미반영·전멸 순위 미반영·HUD 세 요소 필수·로비 복구 실패 후 잠금 해제/무제한 재시도 전제는 최신 구현 설명으로 사용하지 않는다.
- C++ 기본값과 BP/DataAsset 실제 값의 일치 여부는 확인 불가다. 특히 도착90초, Walk200/Sprint500, 배율15, NPC 목표30/45/60을 에디터에서 확인해야 한다.
- 결과 WBP에 필수 `TXT_AcquiredRelicCount`가 있는지, 등록 수·획득 수 열의 이름과 레이아웃이 올바른지 P3-10에서 확인한다. 누락 시 Row 전체 표시가 갱신되지 않을 수 있다. RandomEvent 설명 바인딩도 확인한다.
- P3-10에서 UE5.5 Development Editor 빌드 및 `DontMelt.Match.ResultRules.*` 실행을 별도 승인 범위에서 수행한다. 이후2~4인 Host/Client 결과·도착/준비 실패·Travel 정리·오디오·60NPC 측정과 실제 Steam 콜백/차단 UI를 검증한다. 이번에는 실행하지 않았다.
- 로비 차단 시 로그는 존재하나 사용자에게 구체 사유·이탈/재시작 안내가 실제 전달되는지는 미확인이다. Busy/응답 대기 반복의 종료 보장도 Steam에서 별도 확인한다.
- FakeDeath/선택 휠, 튜토리얼 실패별 정책, 리스폰 재시도 소진 시 전체 경기 중단 등 P2의 범위 밖 미결정 항목은 이번에 새로 확정하지 않는다.

## Content/Food 읽기 전용 조사

원본 main의 미추적83개 파일은 신규 Worktree에 자동 포함되지 않았고 신규 `Content/Food/`는 존재하지 않는다. `git ls-files`의1962개 경로를 기준으로 추적 .uasset/.umap/.cpp/.h/.ini/.cs/.uproject/.json을 읽어 `/Game/Food`, `Content/Food`, `Food/`의 ASCII 및 UTF-16LE 문자열을 검사했으며 일치0건이었다. 이는 패키지 문자열 검사로, Unreal Asset Registry/Reference Viewer의 참조 그래프 검증이나 런타임 로딩 검증이 아니다.

현재 증거로 필수 복사 필요는 확인되지 않았다. 동적 경로 조립, 비추적·외부 자산, 실제 에디터 참조는 확인 불가다. P3-10 에디터 참조 검사에서 의존성이 확인되면 별도 조치 대상으로 보고한 뒤 처리한다. 이번에는 원본/실험 Worktree 변경과 파일 복사·이동·삭제를 하지 않았다.

---

> **기록 구분 — P0 문서 정정**
> 아래 원래 제목과 0~8절은 2026-11-12의 **과거 ZIP 분석 기록을 그대로 보존한 것**이다. 그 안의 '현재', '지금 판정', '소스 없음', '확정'은 당시 자료와 서술을 가리키며 현재 Git 저장소의 구현 상태나 승인 인증으로 사용하지 않는다.
> 현재 Git 커밋 `c3f94a9` 기준 분석은 별도 **9절**부터 읽는다. 이번 정정은 문서 근거 체계만 다루며 기존 설계 원칙과 게임 규칙을 변경하지 않는다.

# ImplementationStatus — 구현 완료·진행 중·미구현·확인 불가

**작성 기준: 2026-11-12 / 첨부 `DontMelt_Portfolio(1).zip`만 직접 조사**

## 0. 가장 중요한 한계

해당 ZIP에는 `Docs/`, `DevLog/`, `Media/.gitkeep`, `README.md`가 있고 **최신 C++ Source / `.uproject` / Content 에셋 / Config / 빌드 결과가 없다.** 따라서 이 파일은 **실제 소스 빌드 상태를 판정할 수 없다.** 문서의 스니펫이나 GitHub용 파일 링크는 **기록 근거**이지 현재 HEAD 존재나 실행 성공의 증거가 아니다.

아래에서는 다음을 엄격히 구별한다.

| 표시 | 의미 |
|---|---|
| **구현 완료(소스 확인)** | 현재 소스·설정·에셋 또는 실행 결과로 확인된 경우만. **현재 0건 확인(완료된 코드가 0건이라는 뜻 아님)** |
| **진행 중/검증 예정** | 문서에 향후 측정·개선·검수 계획이 명시됨. 실제 작업 진행률은 알 수 없음 |
| **미구현(명시적 제외)** | 기획에서 의도적으로 지원하지 않도록 확정한 항목. 버그로 보고 자동 구현하지 않음 |
| **확인 불가** | 문서에 구현했다고 기록됐더라도 현재 소스와 실행이 제공되지 않아 사실 여부 미검증 |

## 1. 주요 기능별 상태

| 기능 | 문서에 기록된 구현/설계 범위 | 지금 판정 | Codex 확인 지점 |
|---|---|---|---|
| Steam 세션·로비 | Create/Find/Join/Destroy, Ready, 시작 잠금, stale 복구 | **확인 불가** (구현 기록 있음) | `DMOnlineSessionSubsystem`, `DMLobbyGameMode`, Config |
| Match Phase / 시간 / 결과 | `TryTransitionPhase`, 규칙 함수, ResultId ACK, Travel | **확인 불가** (구현 기록 있음) | `DMMatchGameMode`, `DMMatchResultRules`, `DMMatchGameState` |
| Enhanced Input / Sprint | IMC 전환, SavedMove Sprint 예측 | **확인 불가** (구현 기록 있음) | `DMPlayerCharacter`, `DMCharacterMovementComponent`, IMC 에셋 |
| 자세 / 감정표현 | Pose 순환·가짜 죽음·입력 제한 | **확인 불가** (구현 기록 있음) | `DMPoseComponent`, AnimBP, 입력 자산 |
| 유물 / 상자 | RequestId, 예약·홀드, 등록·탈취·드롭, 보호 | **확인 불가** (구현 기록 있음) | `DMInteractionComponent`, `DMRelicManager`, Relic Actor |
| 전투 / 사망 | 예측 몽타주, Notify Hit, 방어·NPC 오인 공격 | **확인 불가** (구현 기록 있음) | `DMCombatComponent`, `DMMatchGameMode`, AnimNotify |
| NPC AI / 인구 | Timer AI, 스폰 섞기, 패닉/오인반응, 배율15 문서 | **확인 불가** (구현 기록 있음) | `DMNPCPopulationDirector`, `DMJellyAIController`, `DMMatchRuleDataAsset` |
| 스킬 3종 | 투명화·발소리 증폭·시야 교란, 쿨다운/대상 선택 | **확인 불가** (구현 기록 있음) | `DMSkillComponent`, PostProcess·사운드 자산 |
| 퀘스트 4종 / 실패 사이렌 | Timer/Delegate 기반 진행, 사이렌 OnRep | **확인 불가** (구현 기록 있음) | `DMQuestDirector`, ProgressComponent, 3D 사이렌 에셋 |
| 랜덤 이벤트 3종 | 정전·패닉·입력 반전, 중복/동시 실행 제한 | **확인 불가** (구현 기록 있음) | `DMRandomEventDirector`, Blackout Controller |
| 관전·리스폰 | 시체/공격자/자유/Fade 30초, 서버 재시도 | **확인 불가** (구현 기록 있음) | RespawnCoordinator, SpectatorPresentation, Pawn |
| UMG / 결과 / 설정 | Root 레이어·BindWidget·로컬 설정·결과표 | **확인 불가** (구현 기록 있음) | WBP 에셋 실제 BindWidget, `DMGameRootWidget` |
| Steam Voice | Steam Voice + UVOIPTalker, PTT/근접·사망 청취 | **확인 불가** (구현 기록 있음) | `DMVoiceChatSubsystem`, `DefaultEngine.ini`, Steam 패키지 |
| Jelly / Ragdoll | AnimInstance 7 spring, Bone 8 변형, 사망 Ragdoll | **확인 불가** (구현 기록 있음) | `DMJellyAnimInstance`, `ABP_DMJelly`, PhysicsAsset |
| 네트워크 실패 복구 | PendingNetGame/Travel 구분, 안내 위젯, 설정 저장 | **확인 불가** (구현 기록 있음) | `DMGameInstance`, 설정 SaveGame, Widget |

각 기능의 **현재 소스 검증이 없다는 뜻이지 해당 시스템이 없다는 뜻이 아니다**. 자동으로 재구현하지 않는다.

## 2. 구현 완료 — 현재 직접 확인된 것

**소스 수준으로 확인된 완료 항목: 없음.** 코드와 빌드가 첨부되지 않았기 때문이다. 다만 다음 **문서 산출물의 존재**는 직접 확인했다.

- `Docs/Design/GameDesign.md`, `SystemArchitecture.md`, `EnhancedInput.md`, `UIDesign.md` 및 단계별 설계.
- `Docs/Technical/01_Architecture.md`부터 `15_Settings_NetworkRecovery.md`까지 기술 구현 기록.
- `Docs/Debugging/`의 네트워크·게임플레이·UI·빌드 문제 해결 기록.
- `Docs/Performance/`의 **NPC 40명 당시 실측 기록**과 **60명 검증 계획**.
- `Docs/Verification/`의 정적 검수·패키징·회귀 시나리오.

## 3. 진행 중 / 검증 예정 (문서 근거)

| 작업 | 현 상태의 정확한 표현 | 검증 필요 |
|---|---|---|
| NPC 60명 성능 | **40명 환경 측정 기록은 있음**, 최대 60명 기준 재측정은 계획 | 분리 프로세스/패키지 Stats, 이동·애니·복제 비용 |
| NPC NetUpdateFrequency 30/10 | **개선 후보**, 적용 여부 미확인 | 적용 전/후 이동·대역폭 지표 비교 |
| NPC URO / NavWalking / Audio Concurrency | **개선 후보**, 완료로 기록하지 않음 | 발소리 Notify·이동/충돌 회귀 |
| Steam 패키징·실제 온라인 멀티 | 검증 절차 문서 있음, 최신 성공 로그 없음 | Steam 사용자 계정 분리/네트워크, Build·Cook 로그 |
| Media(영상·스크린샷·다이어그램) | 첨부 ZIP의 `Media/`는 `.gitkeep`만 확인 | 실제 촬영/자료 준비 |
| BP로 편집 가능한 코·Niagara·오인 느낌표 | 최근 대화의 **새 연출 요구**, 소스 적용 근거 없음 | [시각 연출 명세](FeatureSpecs/16_Visual_Effects_And_Presentation.md) 및 최신 소스 |

'진행 중'은 완료율을 뜻하지 않고 **계획·후속 검증이 문서상 남아 있다는 뜻**이다.

## 4. 미구현(의도적 비범위) — 자동 구현 금지

| 제외 기능 | 사유/방침 |
|---|---|
| Dedicated Server | Listen Server를 목표로 함 |
| Host migration | 호스트 이탈 시 복구/방 종료, 새 호스트 승계 지원하지 않음 |
| 경기 중 중도 참가·재접속 | 시작 참가자 스냅샷 고정, Match PreLogin 제한 |
| 퀘스트 완료 쿨다운 회복 보상 | 제거 확정. 실패 시 3D 사이렌이 핵심 |
| NPC 가짜 공격·가짜 죽음 일반 행동 | 설계 비범위, NPC 실제 전투/유물 조작 금지 |
| 상시 전신 Active Ragdoll | 이동 중 절차적 본 흔들림, 실제 사망 때만 Ragdoll |
| 녹화형 Kill Replay | KillerView는 공격자의 현재 모습 보기 |

※ '미구현'은 실제 코드 내 미존재를 검사했다는 뜻이 아니라 **기획상 비지원**을 뜻한다.

## 5. 수치·설계 상충 항목: 반드시 현재 소스 대조

| 쟁점 | 이전 기록 | 최종 기획 문서/후속 요청 | 조치 |
|---|---|---|---|
| NPC 수 | 플레이어 ×10 / 최대 40 | **×15 / 최대 60** | DataAsset, Director 범위, 맵 스폰 Point, 패키지 성능 검증 |
| 유물 수 | 과거 요약에서 총 4개 등 표현 상충 | **2/3/4인: 3/4/5개; 승리 3/3/4개** | RuleDataAsset, RelicSpawnPoint, 매치 시작 스냅샷 확인 |
| Walk/Sprint | 단계 설계 문서 WalkSpeed=300 언급 | **후속 사용자 요청 200 / 500 cm/s** | CharacterBase·Movement·NPC/BP Override 값 검증, 구현 완료 단정 금지 |
| RandomEvent HUD | 기술 설계 일부는 이름·설명·진행바 | 후속 UI 정리에서 설명만 사용 요청 기록 있음 | 실제 `BindWidget` 및 최신 UI 의도 재확인. 명세만으로 강제 수정 금지 |
| 맵·BP·UI 비주얼 | 문서에 기능 구현 기록 | 실제 에셋 없음 | `Content/` 실제 구조, Widget 바인딩·소켓·메시 확인 |

## 6. Codex가 저장소를 열었을 때 확인 순서

```bash
# 실제 경로·프로젝트 파일 탐색
find . -maxdepth 4 -type f \( -name '*.uproject' -o -name '*.Build.cs' -o -name '*.Target.cs' \)
find Source -type f \( -name '*.h' -o -name '*.cpp' \) | sort

# 게임 규칙과 클래스 심볼, 데이터 기본값 위치 찾기
rg -n 'DefaultNPCMultiplier|StartingPlayerCountSnapshot|RequiredRelicCount|WalkSpeed|SprintSpeed|GetMaxSpeed' Source Config
rg -n 'TryFinishMatch|TryAbortMatch|TryCommitPlayerDeath|TryCommitRelicDeposit|ResolveSkillTarget' Source
rg -n 'GetLifetimeReplicatedProps|DOREPLIFETIME|ServerRequest|Client.*Implementation|NetMulticast' Source

# 소스 외에 BP/DataAsset/맵 확인 필요: .uasset은 텍스트 grep만으로 검증할 수 없음.
```

실제 파일이 없는 경로에서는 검색 명령이 실패할 수 있다. 실패 결과를 '기능 미구현'으로 등치하지 않는다. Unreal Editor를 열 수 없는 환경이면 **에셋 수치/바인딩은 확인 불가**로 남긴다.

## 7. 업데이트 기록 양식

검증 후 아래 정보를 각 항목에 추가한다.

```text
기능:
상태: 구현 완료 / 진행 중 / 미구현(의도적 제외) / 확인 불가
증거: 파일 경로:행번호, 클래스·함수, 관련 DataAsset/BP, commit hash
컴파일: 실행 결과 또는 미실행 사유
PIE/패키징: 인원, Host/Client, 네트워크 조건, 결과
문서 대비 차이:
남은 조치:
```

## 8. 근거

[최종 게임 기획](../Docs/Design/GameDesign.md) · [기술 문서](../Docs/Technical/README.md) · [NPC 측정](../Docs/Performance/01_NPC_Performance_Analysis.md) · [프로파일링 계획](../Docs/Performance/03_Profiling_Plan.md) · [검증 시나리오](../Docs/Verification/03_Regression_Scenarios.md)

## 9. 현재 Git 저장소 분석 — c3f94a9 기준

### 9.1 기준·승인·검증 범위

- 기준 커밋: `c3f94a967a9d7e756af659c53747208fda1a21fd`.
- 분석 대상: 루트/모듈 AGENTS, CodexContext 23개 문서, Docs 및 DevLog의 관련 설계·변경 기록, Source의 `.h`/`.cpp`, 관련 Config. Content는 관련 파일의 존재를 확인했으며 에디터 내부는 검증하지 않았다.
- 이 절은 해당 커밋을 대상으로 수행한 정적 분석의 기록이다. 이 문서 정정 자체는 기준 커밋 이후의 미커밋 변경이며, 새 소스 적용이나 검증 통과를 뜻하지 않는다.
- **현재 대화의 직접 승인:** 사용자가 모듈 AGENTS의 개발 원칙·승인 절차·안전 규칙을 제시하고 교체를 승인했다. 이후 문서 25개 커밋·푸시를 승인했다. 이번에는 P0 세 문서의 근거 체계 정정만 조건부 승인했다.
- **승인 미확인:** 기존 ChatGPT 대화 원문에는 접근하지 못했다. 문서가 '채택/확정/수정 완료'라고 서술하는 개별 게임 규칙은 그 서술만으로 승인 확인 처리하지 않고 **F**를 유지한다. 커밋·업로드 승인도 개별 게임 규칙의 일괄 승인이 아니다.
- **공통 에셋 상태 G:** BP, WBP, DataAsset, DataTable, 맵, 입력 자산, AnimBP, 몽타주, Skeleton, PhysicsAsset, 사운드/VFX는 에디터에서 열거나 적용값·바인딩을 검증하지 않았다. 파일 존재만으로 설정·사용 여부를 확정하지 않는다.
- **공통 실행 상태:** 컴파일/빌드, Automation Test, PIE, 패키징, Steam 연결/음성, 성능 측정 모두 **미실행**. 아래 모든 기능에 적용하며 성공으로 기록하지 않는다.
- 확인된 Git 이력은 초기 커밋 `8694975`와 문서 추가 커밋 `c3f94a9`다. 초기 커밋에 포함된 설계·소스의 선후 관계나 개별 수정 적용 시점은 이 이력만으로 확인 불가다.

### 9.2 상태 해석 — 독립 평가

| 코드 | 의미 | 적용 제한 |
|---|---|---|
| A | 설계 승인 및 대응 구현 확인 | 직접 승인 출처·범위를 명시한 항목에만 사용. 이 대화의 작업 규칙 승인을 모든 기능에 확장하지 않음 |
| B | 설계 문서 존재, 대응 C++ 경로 확인 | 소스 증거이며 전체 요구사항 완료·에셋 연결·실행 성공 인증이 아님 |
| C | 설계 문서 존재, 부분 구현 | 확인한 하위 기능과 누락/불일치/확인 불가 부분을 함께 기재 |
| D | 설계 문서 존재, 미구현 | 미구현을 확인할 수 있는 범위에서만 사용. 검색 미발견을 BP 포함 전체 미구현으로 단정하지 않음 |
| E | 설계 간 충돌 또는 문서·구현 불일치 | 양쪽 근거를 보존하며 코드 오류나 새 최종 규칙으로 자동 판정하지 않음 |
| F | 승인 이력 확인 필요 | 사용자 승인 원문이 확인되지 않은 개별 설계에 적용 |
| G | 코드 또는 에셋 검증 불가 | 실제로 확인하지 못한 대상·범위를 명시 |

승인(F), 소스(B/C), 충돌(E), 에셋(G), 실행 미실행은 독립적이다. 예를 들어 `B + F + G`는 C++ 경로는 있지만 설계 승인과 에셋 동작은 확인하지 못했다는 뜻이다. 현재 전체 미구현(D)으로 확정한 기능은 없다.

### 9.3 기능별 증거 기록

아래 각 항목의 에셋·실행 표기는 9.1의 공통 범위를 함께 적용한다. 숫자는 문서 요구 또는 C++ 기본값의 관측이며, 이번 P0에서 승인·변경한 게임 규칙이 아니다.

#### 1. Steam 세션·로비 — B + E + F + G
- **설계 요구:** [01 명세](FeatureSpecs/01_Session_Lobby.md), [Steam 기술 문서](../Docs/Technical/02_Steam_Session_Lobby.md)의 Create/Find/Join/Destroy, Host/Ready, 시작 잠금·실패 복구.
- **사용자 승인:** 개별 세션·타임아웃 정책의 승인 원문 확인 불가(F).
- **C++ 근거:** [DMOnlineSessionSubsystem.cpp](../Source/DontMelt/Private/Online/DMOnlineSessionSubsystem.cpp)의 `CreateLobbySession`, `HandleJoinComplete`, `HandleOperationTimeout`(1164행); [DMLobbyGameMode.cpp](../Source/DontMelt/Private/Framework/Lobby/DMLobbyGameMode.cpp)의 `HandleStartMatchRequest`, `CanStartMatch`, `HandleLobbyStatusUpdated`, `CommitMatchTravel`. Config는 SteamNetDriver/Steam을 지정한다. 늦은 콜백·추가 복구 제한 경로가 문서의 단순 Delegate 해제 설명과 다름(E8).
- **BP·에셋:** 로비/메뉴 BP의 클래스·맵·UI 참조와 실제 적용값 미검증(G).
- **컴파일·실행:** 미실행. Steam Create/Join/Travel 성공 여부 확인 불가.
- **미결정:** 늦은 완료 대기·복구 차단 정책을 최종 계약에 어떻게 기록할지.

#### 2. Match 시작·Phase — B + E + F + G
- **설계 요구:** [02 명세](FeatureSpecs/02_Match_Lifecycle_Result.md)의 승인 명단·스냅샷·Phase·도착 대기.
- **사용자 승인:** 시간·실패 사유·튜토리얼 정책의 원문 확인 필요(F).
- **C++ 근거:** [DMMatchGameMode.cpp](../Source/DontMelt/Private/Framework/Match/DMMatchGameMode.cpp)의 `InitGame`, `PreLogin`, `TryFinalizeStartingRoster`(1129행), `TryTransitionPhase`(2741행), `HandleStartingRosterTimeout`(1088행). [헤더](../Source/DontMelt/Public/Framework/Match/DMMatchGameMode.h) 기본 도착 제한은 120초(411행), 문서는 90초. 전원 튜토리얼 표시·완료 게이트와 300초 제한도 존재(E3/E9).
- **BP·에셋:** 실제 GameMode Override·시간 오버라이드·Tutorial WBP 미검증(G).
- **컴파일·실행:** 미실행. 초기화 실패·인원 누락·호스트/클라이언트 대기 동작 미검증.
- **미결정:** 90/120초, 중단 사유, 튜토리얼 필수 여부·시간.

#### 3. Overtime·유물 공개 — B + F + G
- **설계 요구:** [02 명세](FeatureSpecs/02_Match_Lifecycle_Result.md), [GameplayRules](GameplayRules.md)의 정규 단독 선두/동률 분기·소개·연장·제한 공개.
- **사용자 승인:** 시간·공개 범위의 원문 확인 필요(F).
- **C++ 근거:** [DMMatchGameMode.cpp](../Source/DontMelt/Private/Framework/Match/DMMatchGameMode.cpp)의 `HandleRegularTimeExpired`(2647행), `BeginRelicReveal`, `TryStartOvertimeIntro`, `StartOvertime`, `ResolveOvertimeEnd`; GameState 공개 스냅샷과 종료 시 비우는 경로 확인.
- **BP·에셋:** RuleData 시간, 공개 HUD·진영 문양·위치 표시 미검증(G).
- **컴파일·실행:** 미실행. 경계 시각·동률·공개 종료 동기화 미검증.
- **미결정:** 승인 원문과 실제 에셋값 확인 전 수치를 최종 확정하지 않음.

#### 4. Result·통계·순위·ACK — C + E + F + G
- **설계 요구:** [02 명세](FeatureSpecs/02_Match_Lifecycle_Result.md), [결과 데이터 기록](../Docs/Refactoring/03_MatchResult_Data.md), [전멸 순위 기록](../Docs/Debugging/Gameplay/09_EliminationVictory_Ranking.md)의 누적 획득 수 표시·등록 수 순위·전멸 승자 단독 1위·ResultId ACK.
- **사용자 승인:** 문서의 '채택/수정 완료' 서술은 있으나 승인 원문은 확인 불가(F).
- **C++ 근거:** `ADMMatchGameMode::BuildFinalPlayerResults`는 `AcquiredRelicCount`를 저장(3167행). [DMPlayerResultRowWidget.cpp](../Source/DontMelt/Private/UI/DMPlayerResultRowWidget.cpp)의 `SetResultEntry`는 `RelicCount` 표시(17행). [DMMatchResultRules.cpp](../Source/DontMelt/Private/Framework/Match/DMMatchResultRules.cpp)의 `SortAndRankResults`(169행)는 등록 수 우선이며 문서의 승자 고정 인자가 없음(E1/E2). [DMMatchGameState.cpp](../Source/DontMelt/Private/Framework/Match/DMMatchGameState.cpp)의 `HandleMatchResultChanged`는 캐시 성공 후 ACK, GameMode의 `CommitServerTravel`은 확인 대기 경로 보유.
- **BP·에셋:** 결과 Row 라벨·별도 BP 표시 변경 미검증(G).
- **컴파일·실행:** 미실행. [DMMatchResultRulesTests.cpp](../Source/DontMelt/Private/Tests/DMMatchResultRulesTests.cpp)의 테스트 4개 존재만 확인. 통과나 문서상 추가 테스트 적용을 주장하지 않음.
- **미결정:** E1/E2 규칙의 최종 사용자 결정. 문서와 코드가 다른 이유·적용 이력도 확인 불가.

#### 5. Player Character·Input·Movement — B + E + F + G
- **설계 요구:** [03 명세](FeatureSpecs/03_Character_Input_Movement_Pose.md), [입력 설계](../Docs/Design/EnhancedInput.md)의 시점·Context 전환·Sprint 예측·공통 속도.
- **사용자 승인:** 200/500 변경 요청은 문서에 기록되어 있으나 원문 확인 불가(F).
- **C++ 근거:** [DMJellyCharacterBase.h](../Source/DontMelt/Public/Characters/DMJellyCharacterBase.h)의 Walk/Sprint 기본값 200/500(168/181행); `PostInitializeComponents`가 WalkSpeed 반영. [DMCharacterMovementComponent.cpp](../Source/DontMelt/Private/Characters/Components/DMCharacterMovementComponent.cpp)의 `FSavedMove_DMCharacter`, `GetMaxSpeed`, `UpdateFromCompressedFlags`; `ADMPlayerCharacter::PawnClientRestart` 입력 복구. 단계 문서 WalkSpeed 300과 차이(E5).
- **BP·에셋:** Player/NPC BP 속도·공용 외형·InputConfig/IMC 바인딩 미검증(G).
- **컴파일·실행:** 미실행. 예측 보정·시점·눌린 키 재처리 미검증.
- **미결정:** 공통 속도 최종 승인과 BP 오버라이드 확인.

#### 6. State·Pose — B + F + G
- **설계 요구:** [Architecture](Architecture.md), [03 명세](FeatureSpecs/03_Character_Input_Movement_Pose.md)의 Life/Action/Status/Pose 축 분리와 행동 제한.
- **사용자 승인:** 개발 원칙 자체는 이 대화에서 승인되었으나 개별 행동 허용표·자세 규칙의 원문은 확인 필요(F).
- **C++ 근거:** [DMCharacterStateComponent.cpp](../Source/DontMelt/Private/Characters/Components/DMCharacterStateComponent.cpp)의 `CanPerformAction`, 상태 태그 OwnerOnly·관찰 표현 bool; [DMPoseComponent.cpp](../Source/DontMelt/Private/Characters/Components/DMPoseComponent.cpp)의 `RequestSetPoseModeActive_Implementation`, `IsActionAllowedByPose`. PoseType과 bPoseLocked는 실제 자세와 모드 잠금이라는 서로 다른 의미이므로 단순 중복 원본으로 판정하지 않음.
- **BP·에셋:** 자세 Blend·카메라·충돌·원격 표현 미검증(G).
- **컴파일·실행:** 미실행. Host/Client RepNotify·직접 알림 경로의 화면 결과 미검증.
- **미결정:** 개별 상태 조합의 최종 허용표 승인.

#### 7. Emote·FakeDeath — C + E + F + G
- **설계 요구:** [03 명세](FeatureSpecs/03_Character_Input_Movement_Pose.md), [입력 설계](../Docs/Design/EnhancedInput.md)의 휠 선택·Emote 유지·가짜 죽음과 실제 사망 분리.
- **사용자 승인:** 휠·선택 항목·FakeDeath 범위의 원문 확인 필요(F).
- **C++ 근거:** [DMPlayerCharacter.cpp](../Source/DontMelt/Private/Characters/DMPlayerCharacter.cpp)의 `InputEmoteStarted`(1550행)는 `RequestBeginEmote(DefaultEmoteId)` 호출. [헤더](../Source/DontMelt/Public/Characters/DMPlayerCharacter.h)(358행)는 휠 도입 전 기본 ID로 Scream 지정. CharacterState의 서버 시작/종료·퀘스트 이벤트는 존재하지만 C++ 휠 진입·FakeDeath 전환 경로는 확인하지 못함(E4). Enum·차단 조건만으로 전체 구현을 인정하지 않음.
- **BP·에셋:** Emote/FakeDeath AnimBP·BP 연결은 확인 불가(G). 이름 검색 미발견을 전체 미구현으로 단정하지 않음.
- **컴파일·실행:** 미실행. 유지 퀘스트·표현·늦은 Relevancy 미검증.
- **미결정:** 기본 홀드만 현재 범위인지, 휠/FakeDeath가 필수인지 후속 작업인지.

#### 8. Relic·Chest·BaseZone·Interaction — B + F + G
- **설계 요구:** [04 명세](FeatureSpecs/04_Relic_Interaction.md)의 수량·예약·RequestId·Hold·등록 보호·Drop·복구·정보 제한.
- **사용자 승인:** 개별 시간·수량·취소/실패 정책의 원문 확인 필요(F).
- **C++ 근거:** [DMInteractionComponent.cpp](../Source/DontMelt/Private/Characters/Components/DMInteractionComponent.cpp)의 `ServerRequestInteraction_Implementation`, `ServerValidateActiveInteraction`(883행), `ServerHandleInteractionCompleted`, `EndServerInteractionAsFailure`; [DMRelicManager.cpp](../Source/DontMelt/Private/Relic/DMRelicManager.cpp)의 예약·획득/등록 Commit·등록 수 조회·Drop/복구. [DMRelicRuleDataAsset.cpp](../Source/DontMelt/Private/Data/DMRelicRuleDataAsset.cpp)는 총수 3/4/5, 목표 3/3/4 기본값. 운반 관계 원본과 OwnerOnly 표시를 분리하고 Phase 취소를 실패 Drop과 구분.
- **BP·에셋:** Relic/Chest/BaseZone·SpawnPoint 배치·LOS/Collision·DataAsset 적용값 미검증(G).
- **컴파일·실행:** 미실행. 동시 예약·경계 취소·벽 앞 Drop·복구 미검증.
- **미결정:** 과거 수량 기록과 최종 승인 원문 대조. 코드 기본값을 새 규칙으로 확정하지 않음.

#### 9. Combat·Defense·Death — B + F + G
- **설계 요구:** [05 명세](FeatureSpecs/05_Combat_Death.md)의 서버 Notify 타격·보호/방어·오인 공격·중앙 사망 Commit.
- **사용자 승인:** 수치·대상 처리·종료 경계 정책의 원문 확인 필요(F).
- **C++ 근거:** [DMCombatComponent.cpp](../Source/DontMelt/Private/Characters/Components/DMCombatComponent.cpp)의 `ServerRequestAttack_Implementation`, `HandleAttackHitNotify`(936행), `TryConsumeDefenseBlockAgainst`, `ApplyWrongTargetPenalty`(2169행); `ADMMatchGameMode::TryCommitPlayerDeath`(1782행). 공격 요청에는 Phase 검사, 사망 Commit에는 최종 검사. 늦은 Notify의 NPC 페널티·반응 경로에는 같은 Phase 검사가 확인되지 않아 잠재 위험으로 남김.
- **BP·에셋:** CombatRuleData, 공격/방어 몽타주·AttackHit Notify·실제 타격 위치 미검증(G). AttackRange·DefenseArcAngle C++ 기본값 0만으로 에셋 설정 오류를 단정하지 않음.
- **컴파일·실행:** 미실행. 종료 직전 Notify·복수 대상·방어 경계 검증 필요.
- **미결정:** 기존 공격이 Phase 경계를 넘을 때의 정확한 부작용 금지 범위.

#### 10. Respawn·Spectator — B + E + F + G
- **설계 요구:** [10 명세](FeatureSpecs/10_Spectator_Respawn.md)의 서버 리스폰·실패 재시도·로컬 관전·성공 후 인계.
- **사용자 승인:** 관전 시간·재시도 횟수/중단 정책의 원문 확인 필요(F).
- **C++ 근거:** [DMRespawnCoordinator.cpp](../Source/DontMelt/Private/Framework/Match/DMRespawnCoordinator.cpp)의 `StartRespawnLifecycle`, `HandleRespawnTimerExpired`, `ScheduleRespawnRetry`(574행): Spawn/Possess·보호 성공 후 Alive, 최대 3회 재시도 후 InternalError. [DMSpectatorPresentationComponent.cpp](../Source/DontMelt/Private/Spectator/DMSpectatorPresentationComponent.cpp)의 로컬 단계, `ADMMatchPlayerController::TryCompletePendingRespawnHandoff`(1472행)의 승인 Pawn·입력·LifeState 검증. 재시도 상한 설명이 명세에 부족(E9).
- **BP·에셋:** RespawnPoint·카메라/입력·보호값 미검증(G).
- **컴파일·실행:** 미실행. 스폰 실패·Pawn 수신 지연·재인계·호스트/원격 차이 미검증.
- **미결정:** 3회 상한·중단 정책과 관전 최종 계약.

#### 11. NPC·AI·Panic — B + E + F + G
- **설계 요구:** [06 명세](FeatureSpecs/06_NPC_AI.md)의 ×15/최대60·타이머 AI·집단 반응·유물 미조작, [09 명세](FeatureSpecs/09_Random_Events.md)의 패닉.
- **사용자 승인:** 수량·행동·음향·추가 반응의 원문 확인 필요(F).
- **C++ 근거:** `UDMNPCPopulationDirector::InitializePopulation`(27행)의 셔플·최대60·실패 회수, `HandleWrongTargetAttack`, `HandlePanicEvent`; [DMJellyAIController.cpp](../Source/DontMelt/Private/Characters/DMJellyAIController.cpp)의 타이머/MoveCompleted, AI Tick 비활성화. `HandleNearbyPlayerDeath`(396행)의 비웃음 반응이 추가되어 있으나 대응 명세 부족(E9). `UDMCharacterAudioComponent::CanPlayFootstepLocally`는 PlayerCharacter만 허용하며 NPC 발소리 전제의 성능 문서와 차이(E6).
- **BP·에셋:** NPC BP·행동 DataTable·60개 스폰 지점·NavMesh·소리 미검증(G).
- **컴파일·실행:** 미실행. 60명 성능·패닉·군집 이동 미검증.
- **미결정:** NPC 발소리·비웃음 반응의 의도, ×15 승인 근거.

#### 12. Skill·Cooldown·Targeting — C + E + F + G
- **설계 요구:** [07 명세](FeatureSpecs/07_Skills.md)의 3종 스킬·서버 대상 선택·거리/LOS 무제한·Pawn 교체 후 쿨다운 유지·정보 제한.
- **사용자 승인:** 개별 효과·시간·대상 정보 전송 예외의 원문 확인 필요(F).
- **C++ 근거:** [DMSkillComponent.cpp](../Source/DontMelt/Private/Skills/DMSkillComponent.cpp)의 `ResolveSkillTarget`(666행), `TryExecuteInvisibility`, `TryExecuteFootstepAmplify`, `TryExecuteVisionDistort`, OwnerOnly 종료 시각. [DMMatchPlayerController.cpp](../Source/DontMelt/Private/Framework/Match/DMMatchPlayerController.cpp)의 `ClientSetFootstepAmplifyPresentation_Implementation`(2277행)은 대상 Pawn 부재 시 반환하고 재연결 경로가 확인되지 않음. 서버는 효과·쿨다운 확정 가능. 대상 PlayerState를 시전자에게 보내는 경로는 '대상 비전송'/'전역 전송 금지'의 범위 구분 필요(E10).
- **BP·에셋:** SkillSet 수치·PostProcess·소리·Material 파라미터 미검증(G).
- **컴파일·실행:** 미실행. 대상 Pawn 지연·Relevancy·효과 종료/중첩 미검증.
- **미결정:** 개인 대상 참조 허용 범위·대상 부재 시 효과 계약.

#### 13. Quest·실패 사이렌 — B + F + G
- **설계 요구:** [08 명세](FeatureSpecs/08_Quest.md)의 실제 행동 관측·개인 진행·마감·실패 후 완료·보상 제외 서술.
- **사용자 승인:** 일정·조건·보상 제외의 승인 원문 확인 필요(F). 문서에 제외된 범위로 기록되어 있음을 유지하며 이번 P0에서 새로 확정하지 않음.
- **C++ 근거:** [DMQuestDirector.cpp](../Source/DontMelt/Private/Quests/DMQuestDirector.cpp)의 `HandleQuestActionEvent`(719행), `HandlePlayerLifeStateChanged`, 실패 경고; `UDMQuestProgressComponent`의 OwnerOnly 진행·지속 관측; PlayerCharacter의 서버 행동 이벤트·사이렌 RepNotify.
- **BP·에셋:** QuestDefinition DataTable의 목표·시간·이벤트 태그·사이렌 오디오 미검증(G).
- **컴파일·실행:** 미실행. 실제 점프·Sprint·연속 유지·늦은 사이렌 복원 미검증.
- **미결정:** 원문 기준 일정·참가/제외 조건 확인.

#### 14. Random Events·Blackout — B + F + G
- **설계 요구:** [09 명세](FeatureSpecs/09_Random_Events.md)의 정전·패닉·이동 반전, 퀘스트/이벤트 배제·실제 발동 전 재검증.
- **사용자 승인:** 일정·생존 조건·반전 범위·효과 시간의 원문 확인 필요(F).
- **C++ 근거:** [DMRandomEventDirector.cpp](../Source/DontMelt/Private/Events/DMRandomEventDirector.cpp)의 `TryScheduleEvent`, `HandleWarningFinished`(693행), `ApplyInputReverseEffect`, `Shutdown`; [DMBlackoutController.cpp](../Source/DontMelt/Private/Events/DMBlackoutController.cpp)의 GameState 구독·조명 캐시·복구.
- **BP·에셋:** EventSet·조명 태그·실제 입력 자산과 WASD 한정 여부 미검증(G).
- **컴파일·실행:** 미실행. 예고 중 사망·Phase 전환·효과 복구 미검증.
- **미결정:** 최종 조건·에셋값, HUD 구성은 다음 항목/E7에서 별도 결정.

#### 15. UI·HUD·Settings — C + E + F + G
- **설계 요구:** [11 명세](FeatureSpecs/11_UI_UX.md), [14 명세](FeatureSpecs/14_Settings_Network_Recovery.md)의 Root·모달 우선순위·개인 정보·설정 확인.
- **사용자 승인:** 개별 UI 구성·후속 설명 단일화 요구의 원문 확인 필요(F).
- **C++ 근거:** `UDMGameRootWidget::RebindDataSources`(111행), `RefreshRootVisibility`, `UpdateInteractionPromptPolling`(728행), Widget의 Delegate/서버시각 기반 타이머; `UDMLocalSettingsSubsystem`/`UDMSettingsWidget`의 로컬 설정 경로. `UDMRandomEventHUDWidget::RefreshPresentation`(135행)은 이름·설명·진행바를 모두 요구하며 설명만 유지하려는 기록과 차이(E7). 결과 표시 불일치는 E1/E2 참조.
- **BP·에셋:** WBP의 BindWidget·Visibility·폰트·입력 포커스·각 탭 미검증(G).
- **컴파일·실행:** 미실행. 설정 저장·15초 확인·메뉴 복귀·Host UI 미검증.
- **미결정:** RandomEvent HUD 최종 구성과 결과 데이터 의미 승인.

#### 16. Voice·Network/Travel 복구 — B + F + G
- **설계 요구:** [12 명세](FeatureSpecs/12_Voice_Chat.md), [14 명세](FeatureSpecs/14_Settings_Network_Recovery.md)의 생존 근접/사망 전역·PTT·의도한 이동과 장애 구분.
- **사용자 승인:** 음성·실패 복구 세부 정책의 원문 확인 필요(F).
- **C++ 근거:** `UDMVoiceChatSubsystem::RefreshRemoteTalkerPolicy`(463행)의 Pawn 부재 음소거·Talker 강한 참조·해제; `UDMGameInstance::HandleNetworkFailure`(530행), PendingNetDriver 소유 확인·중복 복구 차단·결과 캐시. 실제 재생 중인 음성의 모드 전환은 소스 설정 경로만 확인.
- **BP·에셋:** 안내 WBP·설정·실제 오디오 장치/감쇠 적용 미검증(G).
- **컴파일·실행:** 미실행. Steam Voice·발화 중 사망/리스폰·접속/Travel 실패 미검증.
- **미결정:** 승인 원문·모드 전환의 현재 발화 적용 계약.

#### 17. Jelly·Ragdoll·VFX·부착 메시 — C + F + G
- **설계 요구:** [13 명세](FeatureSpecs/13_Jelly_Animation_Ragdoll.md)의 로컬 젤리·사망 물리, [16 명세](FeatureSpecs/16_Visual_Effects_And_Presentation.md)의 신규 표현 제안.
- **사용자 승인:** 16번 문서는 제안이며 사용자 승인 원문 확인 필요(F). 기존 물리·표현 세부 규칙도 원문 확인 필요(F).
- **C++ 근거:** `UDMJellyAnimInstance::NativeUpdateAnimation`, `AdvanceJellySpring`, `ResetJellyBodyMotion`; `UDMJellyPhysicsComponent::StartDeathRagdollPresentation`와 Multicast·비복제 로컬 bool. `UDMCharacterVFXComponent`는 비복제 표현·에셋/상대 위치/회전 설정을 보유. 코·Static Mesh 느낌표의 대응 C++ 경로는 확인하지 못함. Niagara 느낌표 경로와 동일한 구현으로 취급하지 않음.
- **BP·에셋:** Bone 변형·Socket·PhysicsAsset·코·Static Mesh/Niagara 구성 미검증(G). 따라서 신규 표현 전체를 D로 확정하지 않음.
- **컴파일·실행:** 미실행. 늦은 Relevant 진입 시 시체 복원·중복 표현·성능 미검증.
- **미결정:** 신규 표현의 승인/범위/공개 대상, 지속 표현 복원 계약.

#### 18. Optimization·측정 — B(구조) + F + G
- **설계 요구:** [성능 분석](../Docs/Performance/01_NPC_Performance_Analysis.md), [프로파일링 계획](../Docs/Performance/03_Profiling_Plan.md)의 측정 후 최소 최적화·60 NPC 검증.
- **사용자 승인:** 개별 최적화 후보 적용 승인 원문 확인 필요(F). 개발 원칙 승인을 특정 수치 변경 승인으로 확장하지 않음.
- **C++ 근거:** AI·다수 Component/Manager Tick 비활성화, Timer/Delegate 기반 처리. 직접 Actor Tick/Component Tick/Widget NativeTick 구현은 검색되지 않았지만 AnimInstance 업데이트·엔진 기본 동작은 별개. `UDMGameInstance::Init`(445행)의 간격 0 Core Ticker와 `TickTrackPendingNetDriver`(608행)가 있으므로 '매 프레임 작업 없음'으로 쓰지 않음. 측정 없이 불필요하거나 병목이라고 판정하지 않음.
- **BP·에셋:** BP Tick·URO·NetUpdateFrequency·NavWalking·Concurrency 실제 적용값 미검증(G).
- **컴파일·실행:** 미실행. 40명 측정은 과거 문서 기록이며 현재 60명 성능은 확인 불가.
- **미결정:** 승인 후 기준선 측정·후보별 효과/회귀 검증. 이번에는 최적화나 검증 계획 실행을 하지 않음.

### 9.4 E1~E10 미결정 목록 — 게임 규칙 변경 없음

| ID | 문서·구현 차이 또는 명세 공백 | 현재 처리 |
|---|---|---|
| E1 | 누적 획득 수 표시 요구 / Row의 등록 수 표시 | 양쪽 근거 보존, F 유지 |
| E2 | 전멸 승자 단독 1위의 채택·수정 기록 / 현재 등록 수 우선 함수 | 원문·소스 이력 확인 필요, F 유지 |
| E3 | 90초·PlayerDisconnect 기록 / 120초 기본값·내부 실패 경로 | 실제 BP값 G, 최종 정책 F |
| E4 | Emote 휠·FakeDeath 설계 / 기본 ID 홀드와 일부 상태 기반 코드 | 전체 BP구현 G, 범위 F |
| E5 | 과거 NPC ×10·Walk300 / 현재 C++ ×15·200/500 | 역사와 관측값 분리, 최종 수치 F |
| E6 | NPC 발소리를 전제한 성능 문서 / Player 전용 발소리 코드 | 의도 F, 런타임 G |
| E7 | 이름·설명·진행바 / 설명 단일화 후속 요청 기록 | 원문 F, WBP G |
| E8 | 30초 후 Delegate 해제 서술 / 늦은 완료 대기·추가 복구 경로 | 현재 경로만 기록, 최종 계약 F |
| E9 | 튜토리얼·300초·리스폰 3회 상한·NPC 비웃음의 대응 명세 부족 | 현재 구현 사실만 기록, 승인 F |
| E10 | 과거 ZIP 현황의 현재성, 문서 경로, '대상 비전송'과 개인 전송의 범위, Listen Host 위협 모델 | 현황/경로만 P0 정정. 네트워크·게임 계약은 F 유지 |

어느 항목도 이번 P0에서 설계를 코드에 맞추거나 코드를 설계에 맞추어 변경하지 않는다. Dedicated Server·호스트 이전·중도 참가/재접속·퀘스트 보상·상시 Active Ragdoll·녹화 Replay의 비범위 서술은 과거 기록으로 보존하며, 신규 구현 승인으로 해석하지 않는다.

### 9.5 후속 기록·작업 경계

- 후속 확인은 기능별로 `설계 요구 / 사용자 승인 출처·범위 / 커밋·경로·클래스·함수·행 / 에셋 확인 / 컴파일·실행 조건·로그 / 미결정 사항` 여섯 축을 갱신한다.
- 기존 기록을 삭제하거나 과거 테스트를 현재 테스트 결과로 바꾸지 않는다. 새로운 검증을 수행한 경우에만 결과와 기준 버전을 추가한다.
- 당시 P0 승인 범위는 모듈 AGENTS, 이 폴더 README, 이 파일의 근거 정정뿐이다. P1~P3, C++/BP/에셋/Config 변경, Git add/commit/push/merge, 빌드·테스트는 실행하지 않았다.
- P0 완료 후 즉시 중단하고 변경 diff와 남은 F/G 및 P1 결정 항목을 보고한다. 다음 작업은 별도 승인 후 진행한다.


## 10. P2 승인 설계 및 현재 구현 — 2026-11-13

**승인 출처:** 현재 대화의 사용자 요청 「Don't Melt — P2 최종 설계 문서 동기화」(본문에서 지칭한 P1-B 채택 범위). 이 승인은 이번 시점의 목표 설계에만 적용하며 과거 ChatGPT 승인 이력으로 소급하지 않는다. 원문에 명시하지 않은 세부 정책은 승인하지 않았다.

**소스 기준:** HEAD `914173a2822f4adcfb3b16d3686a1ebb1c73fc29`. `914173a` 이후 커밋된 Source/Config 차이는 없고 작업 전 추적 파일 변경도 없었다. 이번 변경은 문서만이며 Content/Food는 기존 미추적 상태로 건드리지 않는다.

**읽는 순서:** 이10절과 각 문서의 P2 승인 동기화 절이 E1~E10의 최신 기준이다. 0~8절은 ZIP 기록,9절은 c3f94a9/P0 당시 분석으로 보존한다. 과거 F는 당시의 승인 미확인 기록이며 이번에 명시 승인된 범위에만 최신 승인이 추가된다. 원문 접근 불가인 과거 승인 이력이나 이번 미결정 사항까지 해결됐다는 뜻은 아니다.

**상태:** 설계 승인 / 구현 확인 / 부분 구현 / 에셋 미검증 / 실행 미검증 / 세부 정책 미확정을 독립적으로 사용한다. 모든 항목의 Blueprint·DataAsset·Animation 실제 적용은 미검증이고 빌드·PIE·Steam 테스트는 미실행이다. 설계 동기화를 기능 구현 완료로 표시하지 않는다.

**공통 원칙:** UE5.5, C++ 중심, 2~4인 Listen Server, Steam, Server Authority, Single Source of Truth, 최소 Replication, Event Driven 우선, 클래스 책임 분리, 판정/UI 분리, Blueprint 참조 호환성을 유지한다. C++ 구현은 별도 승인 후 진행한다.

### E1 — 결과 통계와 순위 분리

**설계 승인:** 결과표 유물 열은 경기 중 누적 획득 횟수, 최종 순위는 종료 시 등록 유물 수를 사용하며 동률 공동 순위를 허용한다. 두 통계를 별도 관리하고 표시 변경으로 서버 승리 판정을 바꾸지 않는다.

**구현 확인 / 부분 구현:** `ADMRelicManager::TryCommitRelicAcquisition` → `ADMMatchPlayerState::IncrementAcquiredRelicCount` → `ADMMatchGameMode::BuildFinalPlayerResults`로 누적 통계를 보관한다. `FDMMatchResultEntry`의 `AcquiredRelicCount`와 `RelicCount`가 분리되어 있다. 그러나 `UDMPlayerResultRowWidget::SetResultEntry`는 현재 `RelicCount`를 표시하므로 목표 표시와 불일치한다. 성공 Pickup/Steal Commit마다 증가하는 것이 현재 동작이다.

**세부 정책 미확정:** 자신의 유물을 Drop 후 재획득할 때 누적 횟수에 포함할지 미결정이다. 과거 '+1' 기록을 이번 승인으로 간주하지 않는다.

**구현 변경 예상 범위:** 결과 Row의 표시 원본과 WBP 열 제목을 검토한다. 기존 결과 필드를 사용하고 승패 판정·등록 수 원본·새 RPC/복제는 변경하지 않는 방향이다. 집계 정책 변경은 별도 승인 대상이다.

**검증 시나리오:** 누적5/등록2와 누적3/등록3의 표시·순위 분리, 탈취, Drop/재획득, 공동 순위, Host/Client 결과 캐시와 ACK. 결과 WBP 내부는 에셋 미검증, 빌드·실행 미검증.

### E2 — 전멸 승자 단독 1위

**설계 승인:** 일반 종료는 최종 등록 수 순위다. 전멸 승리에서는 등록 수와 무관하게 승자를 단독1위로 고정하고, 나머지는 2위부터 등록 수 기준 공동 순위를 부여한다. 승리 배너와 결과표의 승자 의미가 모순되지 않아야 한다.

**구현 확인 / 부분 구현:** `DMMatchResultRules::SortAndRankResults`는 등록 수 내림차순이며 같은 등록 수에서만 승자를 우선한다. `ADMMatchGameMode::BuildFinalPlayerResults`가 이 함수를 호출한다. 따라서 등록 수가 적은 전멸 승자가 결과표1위가 아닐 수 있다. 현재 함수는 종료 사유별 승자 고정 계약을 받지 않는다.

**공동 순위 계약:** 현재 코드는 동률이면 직전 Rank를 사용하고 비동률이면 Index+1을 사용한다(예: 등록4/2/2/1 → 순위1/2/2/4). 이 기존 계산 체계는 유지 대상으로 기록한다. 전멸 승자를 제외한 집합에 적용할 때의 등수 건너뛰기 등 상세 예시는 별도 확정 없이 새 규칙으로 추가하지 않는다.

**구현 변경 예상 범위:** 결과 규칙 헤더·소스, GameMode 결과 생성 호출, 결과 규칙 테스트. 기존 Rank 필드 전달을 우선하며 새 RPC는 필요성이 확인될 때만 검토한다. 결과 WBP 배너·행을 수동 확인한다.

**검증 시나리오:** 일반 종료 동률, 전멸 승자 등록 수가 더 적음/같음/많음, 나머지 동률, 2인 전멸 제외와 기존 승리 옵션 회귀. 에셋 미검증·실행 미검증.

### E3 — 참가자 도착 대기 120초

**설계 승인:** 기본 도착 제한은120초다. 경기 맵 서버 GameMode가 승인 참가자의 준비와 시스템 초기화를 검증한다. PC 존재만으로 완료하지 않는다. 도착 제한과 튜토리얼 제한을 분리하고 무한 대기를 방지한다. 미도착·명시적 이탈·Spawn/Possess 실패는 가능한 범위에서 진단·사용자 안내를 구분한다.

**구현 확인:** `ADMMatchGameMode::StartPlay`가 아직 명단이 확정되지 않은 Waiting에서 타이머를 설정한다. 클라이언트 로딩 시작 시점 기준이 아니다. `HandleStartingNewPlayer_Implementation`은 부모 Spawn/Possess 후 Pawn/Controller를 확인한다. `IsStartingRosterComplete`는 기대 인원, 유효 PlayerState, 유효·고유 BaseSlot, Alive, Pawn·Controller·PlayerState 연결을 확인한다. `TryFinalizeStartingRoster`는 스폰 지점 및 NPC/유물/퀘스트/이벤트 초기화 후 도착 타이머를 해제하고 튜토리얼 게이트로 이동한다.

**현재 중단 처리:** 시간 초과·Spawn/Possess 실패는 `AbortMatchForInternalFailure`의 InternalError 경로다. 종료 전 승인 참가자 Logout은 조건에 따라 PlayerDisconnect다. `TryAbortMatch` → 결과 처리 → 자동 복귀/결과 ACK 대기 → 서버 로비 Travel 경로가 있다. 과거90초·PlayerDisconnect 일괄 서술은 이전 설계 기록이다.

**세부 정책 미확정:** 새 중단 사유 Enum·RPC는 승인하지 않았다. 기존 사유·진단·메시지로 충분한지 먼저 검토한다. 90초보다30초 여유가 늘지만 실제 로딩 적정성은 측정 전 확인 불가다.

**구현 변경 예상 범위 / 검증:** P2 E3의 설계 기록은 120초다. 기준 커밋에서 `StartingRosterTimeoutSeconds`의 C++ 기본값은 90초다. 활성 GameMode BP 값은 미확인이며 최종 설계 목표는 미확정이다. 미도착, 지연 도착, 타임아웃 경계, 이탈, Spawn/Possess 실패, 결과·로비 복귀를 향후 검증한다. 실행 미검증.

### E4 — 선택 휠과 지속형 Emote, 별도 FakeDeath

**설계 승인:** 휠은 Emote 선택 UI, 홀드는 선택한 지속형 Emote 실행·유지 방식이다. 입력 종료·취소·상태 전환에서 종료하고, Emote 퀘스트 시작·지속·종료 계약을 유지한다. FakeDeath는 별도 동작 기능으로 관리한다. 기존 문서의 휠 항목 목록만으로 FakeDeath를 일반 Emote와 같은 구현으로 확정하지 않는다.

**구현 확인 / 부분 구현:** `ADMPlayerCharacter::BindPoseAndEmoteInput`, `InputEmoteStarted`, `InputEmoteCompleted`는 기본 `DefaultEmoteId=Scream` 홀드 요청을 연결한다. `UDMCharacterStateComponent::RequestBeginEmote_Implementation` / `RequestEndEmote_Implementation`과 서버 행동·퀘스트 이벤트 경로가 있다. C++ 입력 설정에서 선택 휠 연결은 확인하지 못했다. FakeDeath enum·제한·Anim 분기는 있으나 명시적인 C++ 진입 호출 및 완전한 해제 경로는 확인하지 못했다. BP 전체 미구현이라고 단정하지 않는다.

**세부 정책 미확정:** FakeDeath 진입/해제 조건, 공격·상호작용 제한, 서버 검증, 상태 복제, 애니메이션, 사망·스턴·관전 전환 상세 계약. 휠·홀드의 구체 키/에셋 연결도 구현 전 확인한다.

**구현 변경 예상 범위:** InputConfigDataAsset, PlayerCharacter, CharacterStateComponent, 휠 WBP/입력 에셋/AnimBP. 선택 UI는 로컬, 판정은 서버이며 새 RPC·복제 계약은 상세 명세 승인 후 결정한다.

**검증 시나리오:** 선택→홀드→해제/취소, 메뉴·스턴·사망·관전 전환, 퀘스트 지속 시간·종료 통지, Host/Client 표현. BP/Animation 에셋 미검증·실행 미검증.

### E5 — 공통 이동 속도와 NPC 목표 수

**설계 승인:** Player와 NPC 모두 공통 CharacterBase 기준 Walk200cm/s·Sprint500cm/s를 사용한다. 시작 참가자당 NPC15명, 최대60명이다. 2/3/4인 목표는30/45/60명이다.

**구현 확인:** `ADMJellyCharacterBase`의 WalkSpeed/SprintSpeed 기본값은200/500이며 `PostInitializeComponents`와 이동 컴포넌트 `GetMaxSpeed` 경로에서 사용한다. `UDMMatchRuleDataAsset::DefaultNPCMultiplier` 기본15, `ADMMatchGameMode::TryFinalizeStartingRoster`의 인원×배율 스냅샷, `UDMNPCPopulationDirector::InitializePopulation`의 최대60 검증을 확인했다.

**에셋 미검증:** Character BP·NPC 자식 BP의 속도, 규칙 DataAsset 배율, Nav/스폰 지점의 실제 적용은 확인 불가다. C++ 기본값이 일치한다는 이유만으로 모든 실행값이 일치한다고 선언하지 않는다.

**역사 구분:** Walk300, NPC×10·최대40은 이전 버전이다. 40NPC 비용 측정 기록을60NPC 성능 완료로 사용하지 않는다.

**구현 변경 예상 범위 / 검증:** 현재 기본값의 불필요한 이중 수정은 하지 않는다. BP/DataAsset 검수 후 차이가 확인된 범위만 별도 승인한다. 2/3/4인 수량, Player/NPC 실속도, SavedMove 보정, 4인60NPC AI/Nav/Movement/Animation/복제 비용을 측정한다. 성능·실행 미검증.

### E6 — NPC 이동 발소리 금지

**설계 승인:** NPC는 이동 발소리를 내지 않고 PlayerCharacter만 이동 발소리를 낸다. 사용자가 직접 확인한 의도이며 정책 자체를 재설계하지 않는다. NPC 비명·웃음·이벤트 사운드는 별개다. 이동 발소리는 플레이어 식별 단서가 될 수 있고 발소리 증폭·투명화 표현과 연결된다.

**구현 확인:** `UDMCharacterAudioComponent::CanPlayFootstepLocally`는 소유자가 `ADMPlayerCharacter`인 경우만 허용한다. `HandleFootstepNotify`는 이 검사를 거쳐 로컬 재생하며 발소리 전용 RPC를 추가하지 않는다. 조사한 C++ 발소리 경로에서 NPC 허용 우회는 확인하지 못했다.

**에셋 미검증:** BP 직접 재생, Animation의 Sound Notify, 별도 AudioComponent 우회는 확인 불가다. NPC 웃음/비명을 발소리로 간주해 제거하지 않는다.

**문서·구현 변경 예상 범위:** NPC 발소리를 전제한 과거 URO/Concurrency/군집 오디오 분석은 역사 자료다. NPC 최적화와 Player 발소리 Notify 보존을 구분한다. 현재 C++ 정책 변경 필요는 확인되지 않았다. 새 RPC·복제는 계획하지 않는다.

**검증 시나리오:** Host/Client 각각 NPC 걷기·달리기 무음, Player 걷기·달리기·공중·투명화·증폭, NPC 웃음·비명 유지. 실제 출력·에셋·실행 미검증.

### E7 — RandomEvent HUD 설명 단일화

**설계 승인:** `TXT_EventDescription`만 표시한다. 이름과 진행바는 표시하지 않으며 필수 UI 바인딩에서도 해제하는 것이 목표다. 설명 갱신, 경고·진행·종료 전환, GameState 연결, 종료 정리는 유지한다. DataAsset DisplayName과 서버 이벤트 판정 데이터는 HUD 미사용을 이유로 제거하지 않는다.

**구현 확인 / 부분 구현:** `UDMRandomEventHUDWidget::RefreshPresentation`은 현재 TXT_EventName/TXT_EventDescription/PB_EventProgress 세 요소를 모두 요구하고 이름·진행률도 갱신한다. `UDMGameRootWidget`은 GameState 및 표시 활성 상태를 연결한다. 조사한 C++에서 외부 시스템이 이름·진행바 자식 포인터를 직접 사용하는 경로는 확인하지 못했다. 기존 세 요소 명세는 이전 버전이다.

**구현 변경 예상 범위:** DMRandomEventHUDWidget.h/.cpp의 필수 조건·필드·SetText/SetPercent와 WBP_DMRandomEventHUD 레이아웃을 함께 변경한다. WBP에서만 먼저 삭제하면 현재 C++의 전체 표시 조건과 충돌한다. 서버 Director/GameState 판정·RPC/Replication 변경은 목표에 필요하지 않다. 설명 전환·만료에 필요한 타이머를 무조건 제거하지 않는다.

**검증 시나리오:** 설명만 바인딩한 WBP 컴파일, 경고→진행→종료, 정의 누락, Phase 종료·Travel·재생성, Host/Client. WBP 존재는 확인했으나 내부는 에셋 미검증·실행 미검증.

### E8 — Steam 안전 복구 상태 머신 유지

**설계 승인:** 기존 상태 머신을 유지하며 전면 재작성하지 않는다. Create/Find/Join/Destroy/Update별 비동기 결과를 구분하고 기본 요청30초를 유지한다. 타임아웃은 플랫폼 취소 완료가 아니다. 작업 중 중복 요청을 거절하고 원 요청 완료·필요한 잔여 세션 정리·안전한 재시도 조건 확인 후 다음 요청을 허용한다. 불확실하면 차단·재시작 안내한다. 영구 차단 후 무의미한 로비 재시도를 반복하지 않는다.

**구현 확인:** `UDMOnlineSessionSubsystem::HandleOperationTimeout`은 지연 완료를 기다리고 Find 취소를 시도한다. Create/Join 지연·실패는 `BeginRecoveryDestroy`로 잔여 세션을 처리한다. Destroy 완료는 성공 플래그와 NamedSession 잔존을 확인한다. Find는 취소/완료 결과에 따라 종료하며 Update 지연 완료는 실패 처리한다. `CompleteOperation`은 Delegate·Timer·상태를 정리한 뒤 원 작업 결과를 알린다. `FailClosedRecovery`의 bRecoveryBlocked와 `RejectBlockedOperation`은 불확실한 상태에서 새 요청을 막는다.

**시간·콜백 경계:** 요청30초, 기존 완료 대기20초, 필요시 정리 단계20초가 각각 설정될 수 있다. 전체50초 고정 상한으로 확정하지 않는다. RequestId/진단 순번은 플랫폼 취소나 이전 콜백 격리를 보증하지 않는다. 작업 종류·세션명 검사와 Delegate 해제의 정적 확인을 Steam 안전성 보증으로 확대하지 않는다.

**부분 구현 / 보완 목표:** 연결 중·작업 실패·복구 진행 중·재시도 가능·재시작 필요를 UI에서 구분한다. `ADMLobbyGameMode::HandleLobbyStatusUpdated`/`HandleReturningPlayersTimeout`의 재개 실패5초 반복은 영구 차단 시 중지·안내 연결이 필요하다. Frontend 실패/정리 안내는 있으나 단계별 상태 전달 전체는 확인되지 않았다.

**구현 변경 예상 범위:** Subsystem 상태 통지, FrontendPlayerController/MainMenu와 LobbyGameMode/로비 UI의 안내·반복 제어를 최소 보완하는 계획. 새로운 통지 API/RPC 상세는 별도 구현 승인으로 결정한다. 온라인 상태 머신과 GameMode 경기 판정을 섞지 않는다.

**검증 시나리오:** 작업별 정상·명시 실패·무응답·지연/중복/동기 콜백, Find 취소 실패, 잔여 세션 Destroy 실패, 복구 후 재요청, 차단 후 반복 중지·재시작 안내. 실제 Steam·UI·실행 미검증.

### E9-A — 전원 튜토리얼과 300초 제한

**설계 승인:** 경기 시작 전 전원 완료, 기존300초 제한, 완료·실패·시간 초과·이탈 구분, 서버 최종 판정을 채택한다.

**구현 확인:** `ADMMatchGameMode::StartPreMatchTutorialGate`는 확정 명단을 기준으로 완료 집합·300초 타이머·Client 튜토리얼 표시를 관리하고 완료 후 Countdown으로 진행한다. 표시 실패·시간 초과는 현재 내부 실패 중단 경로다. 도착120초 타이머와 별도다.

**세부 정책 미확정:** 모든 튜토리얼 실패 유형에서 전체 경기를 중단할지의 상세 계약은 현재 구현 사실과 구분한다. 이번 기본 정책 승인으로 모든 실패 처리 세부를 일괄 승인하지 않는다.

**구현 변경 예상 범위 / 검증:** GameMode 게이트·PlayerController Client 표시·완료 요청·튜토리얼 WBP를 검토한다. 현재 전원 완료/300초 기본은 일치한다. 중복 완료·미완료·표시 실패·시간 초과·이탈·Host UI를 확인해야 한다. WBP/시작 조건 실제 적용은 에셋 미검증·실행 미검증.

### E9-B — 서버 리스폰 재시도 상한

**설계 승인:** 최초 실패 후 최대3회 재시도, 서버 관리, 무한 재시도 방지, 성공 시 Pawn·Controller·PlayerState 연결 복구, 실패 원인·소진 기록을 유지한다.

**구현 확인:** `UDMRespawnCoordinator::HandleRespawnTimerExpired` / `ScheduleRespawnRetry`가 재시도와 연결·보호 적용을 관리한다. 기본 재시도 간격은 GameMode의1초이며 최소0.05초 보정이 있다. 최대3회 소진 시 현재 코드는 전체 경기 내부 실패 중단을 요청한다.

**세부 정책 미확정:** 한 명의 재시도 소진으로 전체 경기를 중단할지 최종 미승인이다. 현재 구현을 승인된 규칙으로 승격하지 않는다. 간격1초는 현재 구현 기준값으로 기록하며 새로운 수치 승인을 추가하지 않는다.

**구현 변경 예상 범위 / 검증:** RespawnCoordinator·GameMode 실패 처리·관전 Handoff를 정책 결정 후 검토한다. 새 Pawn 성공 전 Alive/Client 복귀 금지 계약을 유지한다. 일시 실패 후 성공, 최초+재시도3회 소진, 종료·Logout 중 타이머 정리, 보호 적용 실패를 Host/Client로 검증한다. 스폰 에셋·실행 미검증.

### E9-C — 플레이어 실제 사망에 대한 NPC 비웃음

**설계 승인:** 실제 플레이어 사망 후 주변 NPC 반응과 비웃음 표현을 유지한다. Panic/Stun·행동 상태 충돌을 막고 다른 참가자에게 제공하는 사망 위치·상황 단서를 고려한다. 단순한 장식 효과로 취급하지 않는다.

**구현 확인:** GameMode 사망 처리 → `UDMNPCPopulationDirector::HandleNearbyPlayerDeath` → `ADMJellyAIController::TryStartDeathMockingReaction`의 행동 전환·이동 중지·Laugh 요청 경로가 있다. 규칙 DataAsset C++ 기본값은 반경800cm, 최대3명, 지속2.5초다. 이는 현재 구현 기준값이며 실제 에셋 적용 수치와 재생 성공을 의미하지 않는다.

**구현 변경 예상 범위 / 검증:** Director·AIController·RuleDataAsset·Laugh Animation/Audio 연결을 검수한다. 수치 변경은 별도 승인한다. 범위 경계·대상 수·반응 종료·Panic/Stun 경합·사망/Travel 정리와 Host/Client 청각 단서를 검증한다. NPC 이동 발소리 금지와 웃음 허용은 충돌하지 않는다. 에셋·실행 미검증.

### E10 — 정보 공개 범주와 Listen Host 신뢰 경계

**설계 승인:** Server Authority·최소 전송·OwnerOnly를 유지한다. 전체 Client 공개 / Owner Client 전용 / 스킬 시전자 전용 / 관전자 예외 / 서버 전용으로 구분한다. 판정 원본과 표현 데이터를 분리한다. UI 비표시를 비전송 보장으로 취급하지 않는다. Listen Host는 권위 서버를 실행하는 신뢰 서버다. Host에게 서버 원본까지 은폐한다고 보장하지 않는다.

| 범주 | 목표 계약 | 현재 코드 관측 / 승인 경계 |
|---|---|---|
| 전체 Client 공개 | 공개 경기 상태·필요한 관찰 표현만 | GameState Phase·공개 이벤트/퀘스트·최종 결과, PlayerState rematch ready, Character Action/스턴·투명 표현. Relevant 범위와 필드별 재검수 필요 |
| Owner 전용 | 개인 상태·개인 응답 최소화 | MatchPlayerState LifeState/BaseSlot/운반 여부, CharacterState 상세 태그·투명화 시간은 OwnerOnly |
| 시전자 전용 | 기능에 필요한 최소 정보의 예외 명세 | ClientSetFootstepAmplifyPresentation은 대상 PlayerState를 전달. 현재 전달 사실이며 해당 참조 전체의 최종 예외 승인은 시스템 명세 확인 필요 |
| 관전자 예외 | 관전 정책상 필요한 범위만 | ClientBeginSpectatorSequence의 Context·KillerPawn 등은 현재 구현. 숨길 정보와 예외 범위를 별도 확인 |
| 서버 전용 | 원본 판정·불필요한 신원/운반 관계 | RelicManager 비복제, 스킬 서버 대상 선정. 클라이언트가 대상 Actor 대신 선택 정책을 제출 |

**세부 정책 미확정:** 개별 RPC의 Actor/PlayerState 참조 허용 항목·수명·대상 범위는 기능별 명세 확인 전 새로 승인하지 않는다. '전역 전송 금지'를 '정당한 개별 기능에도 모든 참조 금지'로 해석하지 않는다. 반대로 현 코드 전달을 그대로 예외 승인하지 않는다. 일시 공개 종료 후 데이터를 비워도 Client의 과거 저장까지 방지하지 못한다.

**구현 변경 예상 범위 / 검증:** MatchGameState/PlayerState, CharacterStateComponent, SkillComponent, MatchPlayerController의 GetLifetimeReplicatedProps/Client RPC와 관전·음성 참조를 필드별 재검수한다. 엔진 기본 필드·클래스 구분·BP 경로까지 전부 검증한 것은 아니다. 승인 후 필요한 범위만 변경하며 참조 무조건 제거로 스킬·관전을 깨뜨리지 않는다. 원격 Client/Host, 사망·관전·리스폰·늦은 Relevant, 실제 수신 데이터 확인은 미실행. 에셋·실행 미검증.

### 후속 수정 제안 — 이번 수정 범위 밖

아래는 역사 근거로 보존하며 이번에는 수정하지 않는다. 후속 승인 시 기존 기록을 지우지 않고 최신 기준 링크·적용 시점을 덧붙이는 방식으로 정정한다.

- Docs/Debugging/Network/10_Match_Waiting_Timeout.md:90초·중단 사유의 과거 계약(E3).
- Docs/Debugging/Network/04_Steam_Session_Timeout.md:30초 뒤 즉시 Delegate 해제로 재시도하는 과거 설명(E8).
- Docs/Refactoring/03_MatchResult_Data.md:재획득+1 기록은 현재 구현이며 최종 정책 미결정(E1); 전멸 순위 현재 함수와 차이(E2).
- Docs/Debugging/Gameplay/09_EliminationVictory_Ranking.md:수정 완료 기록과 현재 함수 불일치(E2).
- Docs/Debugging/Gameplay/07_Respawn_Failure_Retry.md:무한 재시도 설명과 현재3회 상한 차이(E9-B).
- Docs/Performance/01_NPC_Performance_Analysis.md 및03_Profiling_Plan.md:NPC 발소리·군집 오디오 전제와 Player 전용 정책 구분(E6),40명 과거 측정과60명 미검증 구분(E5).
- Docs/Technical/02_Steam_Session_Lobby.md,03_Match_Flow_Result.md,05_Input_Movement.md,08_NPC_AI.md,11_UMG_UI.md,13_Spectator_Respawn.md:관련 목표/현재 구현/옛 설명 구분 후 별도 정정 검토.
- Docs/Technical/04_Replication_RPC.md와10_Skill_Cooldown_Target.md:전역 공개 금지와 시전자·관전자 참조 예외의 경계(E10).
- DevLog/step03_Movement_Pose.md,step06_NPC_AI.md,step10_UI_Result.md,step11_Steam_Voice_Jelly.md:당시 기록 유지, 최신 목표로 오독하지 않도록 후속 안내 검토. Docs/DevLog가 아닌 루트 DevLog가 실제 경로다.

### 후속 구현 순서와 경계

1. 재획득 집계, FakeDeath 상세, 튜토리얼 실패별 중단, 리스폰 소진 시 전체 중단, 정보 공개 예외 및 순위 세부 계약의 미결정 범위를 먼저 확인한다.
2. 별도 승인 후 E1 결과 Row·E7 HUD의 작은 표현 변경을 우선한다. E2 순위 규칙과 호출·테스트는 승패 판정 변경 없이 분리한다. E8은 상태 머신 보존과 차단/안내 보완이다.
3. 공통 속도·NPC 수·도착 제한의 BP/DataAsset 적용을 검수하고 필요 범위만 변경 승인한다. Config 변경 필요는 현재 확인되지 않았고 이번에 승인하지 않는다.
4. 별도 승인 후 빌드→Editor 바인딩/에셋→Host/Client→실제 Steam 실패·지연 콜백→60NPC 성능 순으로 검증하고 근거를 기록한다.
5. 이번에는 C++/에셋/Config/범위 밖 문서, Git add/commit/push/merge, 빌드·테스트를 하지 않는다. P3와 Git 커밋은 별도 승인 대상이다.

### 현재 소스 근거 색인

경로는 저장소 기준이며 링크는 문서 상대 경로다. 행 번호는 HEAD914173a의 정적 검색 위치이고 실행 증거가 아니다.

| 항목 | 실제 소스 파일 | 함수·필드 및 검색 행 |
|---|---|---|
| E1 | [DMRelicManager.cpp](../Source/DontMelt/Private/Relic/DMRelicManager.cpp) | `TryCommitRelicAcquisition` — 1762행 |
| E1 | [DMMatchPlayerState.cpp](../Source/DontMelt/Private/Framework/Match/DMMatchPlayerState.cpp) | `IncrementAcquiredRelicCount` — 93행 |
| E1/E2 | [DMMatchGameMode.cpp](../Source/DontMelt/Private/Framework/Match/DMMatchGameMode.cpp) | `BuildFinalPlayerResults` — 3143행 |
| E1 | [DMPlayerResultRowWidget.cpp](../Source/DontMelt/Private/UI/DMPlayerResultRowWidget.cpp) | `SetResultEntry` — 5행 |
| E1 | [DMMatchTypes.h](../Source/DontMelt/Public/Core/DMMatchTypes.h) | `AcquiredRelicCount` — 185행 |
| E2 | [DMMatchResultRules.cpp](../Source/DontMelt/Private/Framework/Match/DMMatchResultRules.cpp) | `SortAndRankResults` — 169행 |
| E2 | [DMMatchResultRulesTests.cpp](../Source/DontMelt/Private/Tests/DMMatchResultRulesTests.cpp) | `SortAndRankResults` — 188행 |
| E3 | [DMMatchGameMode.cpp](../Source/DontMelt/Private/Framework/Match/DMMatchGameMode.cpp) | `IsStartingRosterComplete` — 1000행 |
| E3/E9-A | [DMMatchGameMode.h](../Source/DontMelt/Public/Framework/Match/DMMatchGameMode.h) | `StartingRosterTimeoutSeconds` — 411행 |
| E4 | [DMPlayerCharacter.cpp](../Source/DontMelt/Private/Characters/DMPlayerCharacter.cpp) | `InputEmoteStarted` — 576행 |
| E4 | [DMCharacterStateComponent.cpp](../Source/DontMelt/Private/Characters/Components/DMCharacterStateComponent.cpp) | `RequestBeginEmote_Implementation` — 216행 |
| E5 | [DMJellyCharacterBase.h](../Source/DontMelt/Public/Characters/DMJellyCharacterBase.h) | `WalkSpeed =` — 168행 |
| E5 | [DMJellyCharacterBase.cpp](../Source/DontMelt/Private/Characters/DMJellyCharacterBase.cpp) | `PostInitializeComponents` — 45행 |
| E5/E9-C | [DMMatchRuleDataAsset.h](../Source/DontMelt/Public/Data/DMMatchRuleDataAsset.h) | `NPCDeathMockReactionRadius` — 111행 |
| E5 | [DMNPCPopulationDirector.cpp](../Source/DontMelt/Private/AI/DMNPCPopulationDirector.cpp) | `InitializePopulation` — 27행 |
| E6 | [DMCharacterAudioComponent.cpp](../Source/DontMelt/Private/Characters/Components/DMCharacterAudioComponent.cpp) | `CanPlayFootstepLocally` — 80행 |
| E7 | [DMRandomEventHUDWidget.h](../Source/DontMelt/Public/UI/DMRandomEventHUDWidget.h) | `TXT_EventName` — 60행 |
| E7 | [DMRandomEventHUDWidget.cpp](../Source/DontMelt/Private/UI/DMRandomEventHUDWidget.cpp) | `RefreshPresentation()` — 46행 |
| E8 | [DMOnlineSessionSubsystem.cpp](../Source/DontMelt/Private/Online/DMOnlineSessionSubsystem.cpp) | `HandleOperationTimeout()` — 1164행 |
| E8 | [DMLobbyGameMode.cpp](../Source/DontMelt/Private/Framework/Lobby/DMLobbyGameMode.cpp) | `HandleLobbyStatusUpdated(` — 645행 |
| E9-A | [DMMatchGameMode.cpp](../Source/DontMelt/Private/Framework/Match/DMMatchGameMode.cpp) | `void ADMMatchGameMode::StartPreMatchTutorialGate` — 2173행 |
| E9-B | [DMRespawnCoordinator.cpp](../Source/DontMelt/Private/Framework/Match/DMRespawnCoordinator.cpp) | `void UDMRespawnCoordinator::ScheduleRespawnRetry` — 574행 |
| E9-C | [DMNPCPopulationDirector.cpp](../Source/DontMelt/Private/AI/DMNPCPopulationDirector.cpp) | `void UDMNPCPopulationDirector::HandleNearbyPlayerDeath` — 396행 |
| E9-C | [DMJellyAIController.cpp](../Source/DontMelt/Private/Characters/DMJellyAIController.cpp) | `bool ADMJellyAIController::TryStartDeathMockingReaction` — 443행 |
| E10 | [DMMatchPlayerState.cpp](../Source/DontMelt/Private/Framework/Match/DMMatchPlayerState.cpp) | `COND_OwnerOnly` — 22행 |
| E10 | [DMCharacterStateComponent.cpp](../Source/DontMelt/Private/Characters/Components/DMCharacterStateComponent.cpp) | `COND_OwnerOnly` — 33행 |
| E10 | [DMMatchPlayerController.h](../Source/DontMelt/Public/Framework/Match/DMMatchPlayerController.h) | `ClientSetFootstepAmplifyPresentation` — 433행 |
