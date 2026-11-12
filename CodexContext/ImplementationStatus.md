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
