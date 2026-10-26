# 패키징 테스트 전 맵 데이터·환경 체크리스트

코드에 문제가 없어도 **맵 데이터나 실행 환경이 코드의 가정과 다르면** 경기가 시작되지 않거나 Steam 접속이 실패한다. 이런 문제는 컴파일로 잡히지 않고, 패키지 테스트에서 "그냥 안 된다"로만 보인다. 그래서 초기화·검증 코드를 거꾸로 읽어 **"이 코드가 통과하려면 맵과 환경이 어때야 하는가"** 를 목록으로 만들었다.

## 만든 방법

1. `TryFinalizeStartingRoster`에서 시작해, 호출되는 초기화 함수(`InitializeNPCPopulation`, `InitializeRelicSystem` 등)를 따라간다.
2. 각 함수에서 `return false`나 `ensureMsgf`로 실패하는 조건을 모은다.
3. 조건을 "에디터에서 무엇을 확인하면 되는가"로 바꾼다.
4. 어긋났을 때 보이는 증상과 로그 키워드를 붙인다.

## 경기 맵 데이터

| 확인 항목 | 코드가 요구하는 조건 (근거 함수) | 어긋날 때 |
|---|---|---|
| NPC 스폰 지점 | `DM_NPCSpawn` 태그 TargetPoint ≥ 인원 × `DefaultNPCMultiplier` (`InitializePopulation`). 기본 배율 15라 4인이면 60개 | 경기 시작 안 됨 |
| 시작 위치 | `DM_Base_0~3` 태그 PlayerStart가 슬롯마다 정확히 1개 (`FindPlayerStartForBaseSlot`) | 초기 스폰 실패 |
| 리스폰 지점 | 참가 슬롯마다 활성·Weight>0 RespawnPoint ≥ 1 (`ValidateRespawnPointsForStartingRoster`) | 시작 판정 실패 |
| 상자·기지 | 슬롯마다 RelicChest 1개, 같은 BaseSlot의 BaseZone 연결 (`ValidateMatchSetup`) | 유물 초기화 실패 |
| 공용 유물 지점 | PublicStart 용도 RelicSpawnPoint **정확히 1개** (`FindUniquePublicStartPoint`) | 유물 초기화 실패 |
| 유물 개수 데이터 | `TotalRelicCount = 인원 + 1` (`SpawnInitialRelics` 개수 검증) | 유물 초기화 실패 |
| NavMesh | 경기 맵에 빌드된 NavMesh | 크래시는 없지만 **NPC가 움직이지 않음** → 움직이는 캐릭터가 곧 플레이어 |
| 바닥 콜리전 | 유물이 놓일 바닥이 WorldStatic (`IsRelicWorldTransformValid`) | 떨어진 유물이 1초마다 복구 지점으로 순간이동 |

앞의 여섯 항목 중 하나라도 어긋나면 경기가 Waiting에서 멈춘다. [Waiting 타임아웃](../Debugging/Network/10_Match_Waiting_Timeout.md) 이후에는 90초 뒤 중단되고 로비로 돌아간다. 정상이면 `Starting roster frozen` 로그가 찍히고, 찍히지 않으면 직전의 `Starting roster finalization failed:` 로그가 원인을 알려 준다.

**BP 콜리전 함정:** 블루프린트 안에 넣은 StaticMesh의 기본 프리셋은 BlockAllDynamic(WorldDynamic)이다. 레벨에 직접 놓은 StaticMeshActor(WorldStatic)와 다르므로, 바닥을 BP로 만들었다면 따로 확인한다.

## Steam·패키징 환경

| 항목 | 내용 | 어긋날 때 |
|---|---|---|
| NetDriver 설정 | `DefaultEngine.ini`에 `!NetDriverDefinitions=ClearArray` 후 SteamNetDriver·DemoNetDriver 정의 | 호스트가 IP 드라이버로 열려 `steam.` 주소 접속 실패 |
| Shipping | `steam_appid.txt`(480)를 exe 옆에 직접 배치. Development는 자동 생성 | Steam 초기화 실패 → `Steam 세션 인터페이스를 찾을 수 없습니다` |
| 계정 | 두 PC가 **서로 다른** Steam 계정 | 같은 UniqueId로 명단 검사 충돌 |
| 빌드 | 두 PC가 **같은 패키지** | BuildUniqueId가 달라 로비 검색에서 서로 안 보임 |
| 로그 | Development + `-log -LogCmds="LogOnline Verbose, LogNet Verbose"` | Shipping은 로그가 없어 원인 추적 불가 |
| 맵 목록 | 지정할 경우 Title·Lobby·Match 포함 (현재는 비워 두어 전체 쿡) | 이동 시 맵 없음 |

## 에셋

| 항목 | 내용 | 어긋날 때 |
|---|---|---|
| WBP 바인딩 | 모든 WBP Compile/Save 후 Cook 로그 확인 (`BindWidget` 이름·타입) | [Cook 실패](../Debugging/Build/02_Compile_Link_Errors.md) |
| 결과·로비 Row | `PlayerResultRowClass`, `PlayerRowWidgetClass` 지정 | 데이터는 있는데 목록이 빔 |
| 한글 폰트 | C++ `SetText`로 바뀌는 문자열까지 글리프 지원 | 런타임 텍스트만 깨짐 |
| 공격 몽타주 Notify | 판정 Notify를 Branching Point로 | 서버에서 원격 캐릭터의 판정 타이밍이 흔들림 |
| 패드 이동 입력 | 데드존 확인 | 상호작용 중 아주 작은 입력에도 취소됨 |
| PhysicsAsset | 공용 Mesh에 PhysicsAsset 지정 | 경고 후 사망 Ragdoll이 실행되지 않음 |
| `ABP_DMJelly` | Transform Bone 8개, 착지 Translation은 Ribcage에만 연결 | 몸통이 과하게 변형됨 |

## 정리

- 런타임 조건은 **검증 코드를 거꾸로 읽으면** 체크리스트가 된다. 테스트 전에 확인할 목록이 코드에서 나오므로 빠뜨림이 줄어든다.
- 항목마다 **로그 키워드**를 같이 적어, 테스트 중 문제가 생기면 바로 원인 항목을 찾을 수 있게 했다.

---

[Verification 목차](README.md)
