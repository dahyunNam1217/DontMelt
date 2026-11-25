# 15. 맵·BaseSlot·스폰 포인트·에셋 계약

## P3-9 최신 기준 — 2026-11-17

기준 HEAD: `001a08241ecd01dfa1ef565f11c7791eaf73eaeb`. [최신 구현 대조와 검증 한계](../ImplementationStatus.md#p3-9--최신-아키텍처-결정-및-구현-대조-2026-11-17)를 함께 읽는다. 아래 최신 절과 충돌하는 P2/기존 본문은 당시 설계·조사 이력으로 보존하며 현재 완료 근거로 사용하지 않는다.

- `Public/Characters/DMJellyCharacterBase.h` 기본값 WalkSpeed=200/SprintSpeed=500, `Private/Characters/DMJellyCharacterBase.cpp::PostInitializeComponents`의 MaxWalkSpeed 적용, `Private/Characters/Components/DMCharacterMovementComponent.cpp::GetMaxSpeed`의 Sprint 경로를 확인했다. Player/NPC는 이 공통 기반을 상속한다.
- `Public/Data/DMMatchRuleDataAsset.h::DefaultNPCMultiplier=15`, GameMode 시작 인원×배율 스냅샷, `Private/AI/DMNPCPopulationDirector.cpp::InitializePopulation`의 1~60 검증을 확인했다. 2/3/4인 목표는30/45/60명이다.
- `Private/Characters/Components/DMCharacterAudioComponent.cpp::CanPlayFootstepLocally`는 ADMPlayerCharacter만 허용한다. NPC 발소리 제거 정책은 기존 구현이며 이번 커밋이 새로 제거한 것으로 기록하지 않는다. BP 직접 재생·Sound Notify 우회는 미검증이다.
- P3 커밋에서 P3-5에 해당하는 직접 변경은 규칙 DataAsset 헤더 주석의 4×10=40 → 4×15=60 정정이다. 이동·배율·발소리 동작은 이전부터 존재하던 코드를 재확인한 것이다. 에셋 기본값·실제 속도·수량·음향·60NPC 성능은 미검증이다.
- 신규 Worktree에 Content/Food 폴더는 없다. 추적 파일의 경로 문자열 검사에서 참조는 발견하지 못했으나 에디터 참조 그래프·동적 로딩은 확인 불가다. 파일 복사 없이 중앙 P3-9의 후속 검수 항목으로 남긴다.

## P2 승인 동기화 — 2026-11-13 (당시 기록; 최신 기준은 상단 P3-9)

이번 사용자 P2 요청의 명시 범위만 **설계 승인**으로 채택한다. 과거 승인으로 소급하지 않는다. 기준 HEAD는 `914173a`이며 구현 사실·설계 승인·에셋 적용·실행 검증은 별개다. 상세 근거와 미결정 사항은 [ImplementationStatus 10절](../ImplementationStatus.md#10-p2-승인-설계-및-현재-구현--2026-11-13)을 따른다.

**아래 기존 본문과의 관계:** 이 절 뒤의 기존 본문은 삭제하지 않고 이전 설계·조사 기록으로 보존한다. E1~E10과 충돌하는 값·완료/승인/미확인 표현은 이 절과 중앙10절이 대체한다. 특히90초·Walk300·NPC×10/40·NPC 발소리·3요소 이벤트 HUD·타임아웃 즉시 재요청은 최신 목표가 아니다. E1~E10 밖의 항목을 이번 승인으로 일괄 확정하지 않는다.

### E5 — 공통 이동 속도와 NPC 목표 수

**설계 승인:** Player와 NPC 모두 공통 CharacterBase 기준 Walk200cm/s·Sprint500cm/s를 사용한다. 시작 참가자당 NPC15명, 최대60명이다. 2/3/4인 목표는30/45/60명이다.

**구현 확인:** `ADMJellyCharacterBase`의 WalkSpeed/SprintSpeed 기본값은200/500이며 `PostInitializeComponents`와 이동 컴포넌트 `GetMaxSpeed` 경로에서 사용한다. `UDMMatchRuleDataAsset::DefaultNPCMultiplier` 기본15, `ADMMatchGameMode::TryFinalizeStartingRoster`의 인원×배율 스냅샷, `UDMNPCPopulationDirector::InitializePopulation`의 최대60 검증을 확인했다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../ImplementationStatus.md)의 E5 절을 함께 읽는다.

### E9-B — 서버 리스폰 재시도 상한

**설계 승인:** 최초 실패 후 최대3회 재시도, 서버 관리, 무한 재시도 방지, 성공 시 Pawn·Controller·PlayerState 연결 복구, 실패 원인·소진 기록을 유지한다.

**구현 확인:** `UDMRespawnCoordinator::HandleRespawnTimerExpired` / `ScheduleRespawnRetry`가 재시도와 연결·보호 적용을 관리한다. 기본 재시도 간격은 GameMode의1초이며 최소0.05초 보정이 있다. 최대3회 소진 시 현재 코드는 전체 경기 내부 실패 중단을 요청한다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../ImplementationStatus.md)의 E9-B 절을 함께 읽는다.

**검증 상태:** BP/DataAsset/Animation 실제 적용은 에셋 미검증, 빌드·PIE·Steam은 실행 미검증이다. 아래 완료 기준은 향후 검증 시나리오이며 통과 기록이 아니다. C++·WBP·Config 변경은 이번 문서 승인에 포함되지 않는다. [공통 게임 규칙](../GameplayRules.md) · [네트워크 정책](../NetworkPolicy.md)를 상호 참조한다.

---

## 맵 콘셉트

마녀의 저택/젤리 마을을 중심으로 하는 `Horror Mansion Map`. 사탕·설탕 오브젝트, 진영별 유물 상자/시작점/리스폰 지점, 중앙 공용 유물 및 NPC 이동 동선이 필요하다. 정확한 `.umap` 파일명과 현재 배치는 첨부 파일에 없어 **확인 불가**.

## 필수 배치 계약

미니맵은 월드 좌표 보정값과 활성 자기 BaseZone을 사용한다. 실제 맵 범위와 WBP 아이콘 위치는 에디터에서 맞춰 본다.

| 배치 객체 | 조건 |
|---|---|
| 진영 BaseSlot 0~3 | 최대 4인용 서로 구분되는 진영. 맵과 일치하는 슬롯 ID 보유 |
| `ADMBaseZone` + `ADMRelicChest` | 각 활성 BaseSlot에 유효한 상자·상호작용 지점 |
| 시작/리스폰 지점 | 참가자마다 매핑 가능한 BaseSlot 및 Character 생성 가능한 Capsule 공간 |
| `ADMRelicSpawnPoint` | 활성 진영 유물 각 1 + 중앙 공용 1, Drop 실패 복구 지점 |
| `ATargetPoint` tagged `DM_NPCSpawn` | 4인 최대 60명용 충분한 지점(동선·충돌·NavMesh 검증) |
| NavMesh / NPC 동선 | Walking/Sprint/RelicApproach/Flee/Panic으로 이동 가능한 구간 |
| `ADMBlackoutController` 및 조명 | 정전 이벤트에서 로컬 조명 연출·원상복귀 가능 |
| 공용 Skeletal Mesh/AnimBP/PhysicsAsset | 플레이어·NPC 동일 외형·젤리 연출·사망 물리 지원 |
| UI/입력 자산 | IMC_Gameplay/Pose/Interaction/SkillTargeting/Radial/Spectator/Voice, 필수 WBP 바인딩 |

**미확정:** 정확한 에셋 경로, 물리 소재, NPC 스폰 간격, 최종 라이팅/환경음·VFX 에셋은 현재 코드/Content 없이는 판단 불가. 존재하는 Blueprint 오버라이드와 실제 Skeleton Bone 이름을 확인해야 함.

## 규칙 연계

- 경기 시작 때 참가 인원·BaseSlot·Relic/SpawnPoint·NPC 목표 수를 검증. 일부 실패했는데 매치를 강행하면 장기 Waiting/유물 누락 발생 위험.
- NPC는 스폰 포인트 셔플. 시작 인원이 2명이라고 첫 30개 위치를 매번 그대로 쓰지 않음.
- 유물 떨어진 위치가 월드 바깥/복구 불가하면 미리 지정된 SpawnPoint로 반환. 임의 좌표 생성 금지.
- 리스폰 Spawn/Possess 실패하면 1초 간격 재시도; 정상 Alive 확정/클라이언트 Handoff 전제 아님.
- 상대 식별을 허용하지 않는 외형·UI: 상자 진영 문양은 서로 다른 것과 플레이어 외형을 연결하지 않음.

## 성능·검증

커밋 `a3a6e07`의 `Maps/Test/Experiments/P3_11r/MatchMap_T_A_144.umap`와 `MatchMap_T_B_176.umap`은 비교용 맵이다. 실제 NPC 배치, NavMesh 경로와 성능 결과는 미검증이다. [측정 계획](../../Docs/Performance/03_Profiling_Plan.md)

- 60 NPC + 4 Player 기준 AI/NavMesh·CharacterMovement·Animation·Voice·Actor Replication 측정.
- 스폰 위치 겹침, 시작 시 스냅샷 잠금, Physics Asset·Collision/Trace 채널, Pawn이 상호작용 LOS를 차단하지 않는지.
- BlendSpace/AnimNotify/PhysicsAsset 정상, 한글/영문 Widget 글꼴, 필수 액터가 없을 때 명확한 시작 실패 로그.

[게임 기획](../../Docs/Design/GameDesign.md) · [NPC](../../Docs/Technical/08_NPC_AI.md) · [패키징 체크리스트](../../Docs/Verification/02_Packaging_Checklist.md)
