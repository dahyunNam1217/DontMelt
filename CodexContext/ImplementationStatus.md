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
- P0 승인 범위는 모듈 AGENTS, 이 폴더 README, 이 파일의 근거 정정뿐이다. P1~P3, C++/BP/에셋/Config 변경, Git add/commit/push/merge, 빌드·테스트는 실행하지 않는다.
- P0 완료 후 즉시 중단하고 변경 diff와 남은 F/G 및 P1 결정 항목을 보고한다. 다음 작업은 별도 승인 후 진행한다.
