# 6단계 — NPC 인구 관리와 AI

## 45. NPC Population 구조와 서버 소유권

매치 시작 인원 Snapshot으로 NPC 수를 고정했다. 게임 중 접속 인원 변화마다 NPC를 다시 계산하지 않도록 했다.

**구현 및 판단**

- 2인 20명·3인 30명·4인 40명 기준
- NPCPopulationDirector가 Spawn과 등록 정보를 관리
- Player와 NPC는 공통 JellyCharacterBase 외형 공유

**결과**

이 단계는 배율 10(최대 40명)으로 시작했고, 11단계에서 배율 15(최대 60명)로 올렸다.

## 46. NPC 전용 Spawn Point와 레벨 배치

최대 40명의 NPC가 사용할 Spawn Point를 레벨에 배치하고 PlayerStart, BaseZone, RespawnPoint와 용도를 구분했다.

**구현 및 판단**

- 4명×10 기준 NPC 타겟 지점 40개 배치
- Collision 때문에 Spawn이 실패하는 상황 확인
- Server에서만 NPC 생성

**결과**

지점 수가 목표 NPC 수보다 적으면 `InitializePopulation`이 실패를 반환하도록 해, 레벨 배치 누락이 경기 시작 단계에서 바로 드러나게 했다.

## 47. JellyAIController 상태와 Timer 흐름

AI의 판단은 서버에 두고 이동은 기존 Navigation/CharacterMovement를 이용하도록 했다. 매 프레임 행동 목표를 다시 고르는 구조는 피했다.

**구현 및 판단**

- Idle·보행·달리기 등 행동 상태를 Timer 기반으로 전환
- NPC Runtime State의 중복 bool 추가 회피
- 필요할 때만 다음 행동 목표 선택

**결과**

40개 NPC의 상태 복제를 세부 행동마다 추가하지 않도록 검토했다.

## 48. NPC 위장 행동과 FakeDeath 처리

NPC가 실제 플레이어처럼 보이도록 유물·상자 방향 이동, 유휴 행동, FakeDeath 표현의 우선순위를 검토했다.

**구현 및 판단**

- NPC의 FakeDeath는 Player Death와 다른 상태
- NPC 오인 타격과 전역 Panic의 우선순위 별도 처리
- 아직 확정되지 않은 위장 연기는 보류

**결과**

겉으로 쓰러지는 행동 때문에 실제 LifeState나 서버 Respawn 경로에 진입하면 안 된다.

## 49. 오인 공격과 그룹 반응 연동

CombatComponent의 NPC 오인 타격을 AI 반응 트리거로 연결했다. 직접 맞은 NPC와 주변 그룹의 반응을 분리했다.

**구현 및 판단**

- 타격 이벤트의 서버 전달
- 개별 AI 반응·도주/패닉 선택
- 타격이 반복돼도 불필요한 전체 NPC 검색을 피하도록 함

**결과**

전투 파트가 NPC의 내부 AI 상태를 직접 수정하지 않는 경계를 유지했다.

## 50. Global Panic 분산 처리

랜덤 이벤트에서 NPC 전체가 반응할 수 있도록 PopulationDirector가 분산 호출을 담당하는 구조를 정리했다.

**구현 및 판단**

- 전역 이벤트와 개인 오인 공격 우선순위 구분
- NPC마다 불필요한 매 프레임 RPC를 만들지 않음
- 클라이언트에는 실제 이동 결과를 공유

**결과**

Panic 관련 행동은 Stage 7 Random Event와의 통합 경계로 남겼다.

## 51. NPC 행동 속도와 밸런스 보류

기본 이동 속도와 공격 거리 조정 위치를 검토했다. 행동 템포(걷기→Idle→달리기 전환 간격)는 구조 변경 없이 후반 밸런스 단계로 미뤘다.

**구현 및 판단**

- 이동 속도는 CharacterMovement의 실제 소유값 확인
- 공격 거리는 `UDMCombatRuleDataAsset` 인스턴스가 최종 원본

**결과**

임의의 기본값을 설정값으로 착각하지 않도록 소스와 DataAsset을 분리해서 적었다.

## 52. NPC 20~40명 환경의 네트워크 검수

NPC를 별도의 Client AI로 실행하지 않고 Server만 판단하도록 구성했다. Actor Movement와 Animation 표현의 비용은 후반 프로파일링 단계로 이관했다.

**구현 및 판단**

- AI Controller는 Server Only
- 관찰자별 AI 결정을 복제하지 않음
- Tick·Timer와 NPC 등록 컬렉션 범위 확인

**결과**

측정에서 AI 비용은 작았고, Movement·Animation 비용은 별도로 분석했다.

**관련 문서**

- [NPC가 프레임 저하의 원인인지 판단한 과정](../Docs/Performance/01_NPC_Performance_Analysis.md)

## 53. NPC 시스템 동결과 퀘스트 연결

NPC Population·개별 Controller·이벤트 반응 경계를 확정하고 7단계로 넘어갔다.

**구현 및 판단**

- 매치 시작 시 스폰 인원 결정
- Quest와 Random Event는 디렉터 이벤트로 AI에 영향
- NPC 행동 템포는 후속 밸런스로 이관

**결과**

이 단계에서 불필요한 상태나 Actor를 추가하지 않고 현재 구조를 유지한다.
