# 06. NPC 군중·AI·오인 공격 반응

## 목표

NPC와 플레이어는 같은 젤리 캐릭터 기반 메시·이동·애니메이션을 쓴다. 행동 단서를 완전히 동일하게 만드는 것이 아니라, 플레이어가 NPC로 위장할 수 있는 군중을 제공한다. NPC는 **유물이나 전투 결과를 실제로 바꾸지 않는다**.

## 책임

`UDMNPCPopulationDirector`: 인원 스냅샷 기반 스폰·등록/회수·지역별 분산·오인 공격 집단 반응·전역 패닉 배포. `ADMJellyAIController`: 개인 행동·NavMove·타이머·MoveComplete. `ADMNPCCharacter`: 공용 캐릭터 메시·이동·행동 표현. 서버 AI 전용 판단, 클라이언트는 이동/표현만 복제.

## 확정 수치

- `DefaultNPCMultiplier=15`, 2인=30/3인=45/4인=60명 (구 10배/40명보다 최신 목표).
- Director 허용 범위 1~60(기술 문서 기록). 맵은 4인용 최소 **60개** 이상의 `DM_NPCSpawn` TargetPoint 필요.
- 행동 가중치: Walk 40, RelicApproach 20, Sprint 15, Idle 10, Jump 5. 확률은 이 가중치로 정규화하되 DataTable `DT_NPCBehaviorProfiles`/RuleDataAsset 실존 값 확인.
- 첫 행동 지연 0.15~1.0초, 이동 후 0.15~0.4초, Move 실패 0.5초, 유물 접근 후 정지 2초(문서 기본).
- 주변 반응: 기본 반경 **1200cm**, 추가 NPC **최대 6명**, 도주 기본 **900cm**.

## 상태와 흐름

```text
서버 타이머 → 가중치 행동 선택 → Idle/Jump/Nav Move/RelicApproach
             └─MoveCompleted/타이머→다음 행동 스케줄
오인 공격 → 맞은 NPC React + 가장 가까운 주변 NPC(최대 6) Flee
랜덤 패닉 → Director가 분산 지연으로 React/Panic 이동 지시
```

NPC는 유물·상자에 접근한 뒤 머물 수 있지만 Pickup, Deposit, Steal은 금지. 가짜 공격·가짜 죽음은 NPC 일반 행동에 포함하지 않는다. 스폰 지점은 매치마다 셔플하여 2인 등 인원이 적을 때 동일 지역만 채워지는 편향을 피한다. 필수 스폰 실패 시 부분 생성 개체를 회수하고 초기화를 실패로 처리한다.

## 성능 제약

- Behavior Tree/StateTree/개체 Tick 대신 Timer·MoveCompleted 사용.
- NPC **40명** 실측: AI 평균 약 0.12~0.25ms, Move/CharacterMovement 비용은 실행 프로세스에 크게 영향. 60명 측정 결과는 아직 증거 없음.
- NPC NetUpdateFrequency 30/10, URO, NavWalking, Audio Concurrency는 **검토 후보**. 이미 적용했다고 단정 금지.
- Render 기반 Pose 갱신 중단은 발소리 AnimNotify를 없애 위장 게임 규칙을 훼손할 수 있음. 최적화 시 오디오 테스트 필수.

## 완료 기준

- 2/3/4인 NPC 정확 30/45/60 및 스폰 실패 정리.
- 행동 선택·경로 실패·오인 공격 주변 6명·패닉 도중 사망/경기 종료.
- 실시간 타이머 누수, 반복 MoveTo 폭주, RPC 폭주, 발소리 동작 누락 없음.
- PIE 한 프로세스 수치가 아닌 별도 프로세스/패키지 환경에서 NPC 60명 측정.

[AI 설계](../../Docs/Design/Stages/06_NPCAI.md) · [AI 기술](../../Docs/Technical/08_NPC_AI.md) · [성능 측정](../../Docs/Performance/01_NPC_Performance_Analysis.md)
