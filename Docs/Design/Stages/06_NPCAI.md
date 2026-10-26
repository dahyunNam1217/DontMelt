# 06 — NPC 인구 관리와 AI 행동

## 1. 서버 구조

```text
ADMMatchGameMode
└─ UDMNPCPopulationDirector      스폰·등록·집단 반응
   └─ ADMJellyAIController       NPC별 행동 선택·타이머·MoveTo
      └─ ADMNPCCharacter         이동 복제·애니메이션
```

NPC 수는 `StartingPlayerCountSnapshot × ResolvedNPCMultiplierSnapshot`이다. 이 단계에서는 배율 10(2인 20명, 3인 30명, 4인 40명)으로 시작했고, 이후 배율 15(최대 60명)로 늘렸다. 중도 이탈로 수를 다시 계산하지 않는다. 스폰 지점은 `DM_NPCSpawn` 태그를 단 `ATargetPoint`다.

## 2. AI 상태

| 상태 | 의미 |
|---|---|
| `Idle`, `Waiting` | 정지·지연 |
| `Moving` | 걷기와 달리기 공용 |
| `RelicApproachMoving` | 유물 또는 상자 접근 |
| `Reacting`, `FleeMoving` | 오인 공격 반응·도주 |
| `PanicWaiting`, `PanicMoving` | 전역 패닉 이벤트 |

Behavior Tree·Blackboard·StateTree와 AI Tick을 쓰지 않고 C++ 타이머 기반 상태 머신으로 만들었다.

```text
타이머 → 가중치 기반 선택 → Nav 이동 또는 동작 → 완료 타이머
```

## 3. 행동 데이터

| 행동 | 기본 가중치·시간 |
|---|---|
| Walk | 40 |
| RelicApproach | 20 |
| Sprint | 15 |
| Idle | 10 |
| Jump | 5 |
| Idle 기본 시간 | 1.5초, ×0.85~1.15 |
| 이동 후 대기 | 0.15~0.40초 |
| 유물 접근 후 정지 | 2초 |

가중치는 `DT_NPCBehaviorProfiles` 데이터에서 읽는다. 무작위 감정표현·가짜 공격은 데이터에 가중치를 두되 실제 행동 선택에서는 뺐고, 가짜 죽음도 일반 NPC 행동으로 넣지 않았다. NPC의 가짜 죽음은 플레이어의 실제 사망과 다른 상태로 다룬다.

## 4. 위장과 반응

NPC는 유물·상자까지 걸어가 잠시 머물 수 있지만 줍기·등록·탈취를 하지 않는다. 오인 공격에 직접 맞은 NPC는 반응하고, 주변 NPC는 거리순으로 지정 수까지만 비명·도주한다. 기본 반응 반경 1200cm, 추가 반응 6명, 도주 거리 900cm다. 패닉 이벤트는 Director가 등록된 NPC에 전파한다.

AI 판단은 서버 전용이다. 클라이언트에는 이동과 필요한 행동 표현만 보낸다. 개체별 RPC로 가중치를 보내지 않는다.

## 5. 성능 기록

4인·NPC 40명 환경에서 AI 평균 비용은 약 0.12~0.25ms로 작았다. 패닉 때는 동시 이동 때문에 Movement와 복제 비용이 늘었다. 측정 과정과 판단은 [NPC 성능 분석](../../Performance/01_NPC_Performance_Analysis.md)에 정리했다.

## 6. 이후 변경

- 공통 `WalkSpeed=300`은 NPC Blueprint가 덮어쓰지 않으면 NPC에도 적용된다.
- 배율을 15로 올리면서 Director 허용 범위를 1~60으로 넓혔다. 레벨에는 4인 기준 60개 이상의 스폰 지점이 필요하다.
- 스폰 지점 순서를 섞어 매 경기 같은 구역에 NPC가 몰리지 않게 했다([스폰 편향](../../Debugging/Gameplay/12_NPC_Spawn_Shuffle.md)).

관련 문서: [NPC 구현](../../Technical/08_NPC_AI.md)

[Stages 목차](README.md)
